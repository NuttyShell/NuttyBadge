/*
 * NuttyBTPresenter
 *
 * A BLE HID-over-GATT slideshow presenter. The badge advertises itself as a
 * Bluetooth keyboard; pair to it from the host computer's Bluetooth settings
 * (it appears as "NuttyPresenter"), then use the badge buttons to drive a
 * slideshow:
 *
 *   LEFT       -> Previous slide   (Page Up)
 *   RIGHT / A  -> Next slide       (Page Down)
 *   UP         -> Volume Up        (Consumer Control)
 *   DOWN       -> Volume Down      (Consumer Control)
 *   B          -> Toggle black     (the "B" key in PowerPoint/Keynote)
 *   START hold -> Exit app
 *
 * Two HID reports are exposed:
 *   Report ID 1 : 8-byte boot-keyboard report (modifier, reserved, 6 keycodes)
 *   Report ID 2 : 2-byte consumer-control report (16-bit usage)
 */

#include "NuttyBTPresenter.h"
#include "services/NuttyInput/NuttyInput.h"
#include "services/NuttyDisplay/NuttyDisplay.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdio.h>

#ifdef CONFIG_BT_ENABLED
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#endif

static const char *TAG = "BTPresenter";

#define DEVICE_NAME_PREFIX "NuttyPresenter"
/* "NuttyPresenter" (14) + 6 hex chars + NUL */
static char g_device_name[14 + 6 + 1] = DEVICE_NAME_PREFIX;
/* The 6-digit passkey CURRENTLY DISPLAYED on the badge, sourced from the
   BLE Security Manager (either as Passkey Entry display or Numeric
   Comparison value). Zero means "no passkey to show right now". */
static uint32_t g_passkey = 0;

/* ---------- HID Usage IDs ---------- */
#define KEY_PAGE_UP        0x4B
#define KEY_PAGE_DOWN      0x4E
#define KEY_B              0x05
#define CC_VOLUME_UP       0x00E9
#define CC_VOLUME_DOWN     0x00EA
#define CC_PLAY_PAUSE      0x00CD

/* ---------- App state ---------- */
typedef enum {
    PRESENTER_ADVERTISING,
    PRESENTER_CONNECTED,
} PresenterState;

typedef struct {
    PresenterState state;
    uint16_t conn_handle;
    uint8_t  own_addr_type;
    /* HID notification handles */
    uint16_t kbd_input_handle;
    uint16_t cc_input_handle;
    /* Last action label for the UI */
    char     last_action[24];
} AppState;

static AppState appState = {0};

/* Set when nutty_main() is exiting. Tells the GAP event handler to stop
   restarting advertising on disconnect. */
static volatile bool g_exiting = false;

/* True after the very first successful ble_bringup(). NimBLE init, HCI
   bring-up, and GATT service registration are one-shot per OS lifetime -
   on subsequent app launches we just re-arm advertising without
   re-initializing the stack. */
static bool g_ble_initialized = false;

/* ---------- UI ---------- */
static lv_obj_t *mainContainer = NULL;
static lv_obj_t *titleLabel    = NULL;
static lv_obj_t *statusLabel   = NULL;
static lv_obj_t *hintLabel     = NULL;
static lv_obj_t *actionLabel   = NULL;

static SemaphoreHandle_t uiMutex = NULL;

static void ui_set_state(PresenterState s);
static void ui_set_action(const char *txt);
static void ui_set_device_name(const char *name);

#ifdef CONFIG_BT_ENABLED

/* ===========================================================
 * HID Report Map
 * Two top-level collections: Keyboard (Report ID 1) and
 * Consumer Control (Report ID 2).
 * =========================================================== */
static const uint8_t hid_report_map[] = {
    /* Keyboard */
    0x05, 0x01,        /* Usage Page (Generic Desktop) */
    0x09, 0x06,        /* Usage (Keyboard) */
    0xA1, 0x01,        /* Collection (Application) */
    0x85, 0x01,        /*   Report ID (1) */
    0x05, 0x07,        /*   Usage Page (Keyboard) */
    0x19, 0xE0,        /*   Usage Min (224) */
    0x29, 0xE7,        /*   Usage Max (231) */
    0x15, 0x00,        /*   Logical Min (0) */
    0x25, 0x01,        /*   Logical Max (1) */
    0x75, 0x01,        /*   Report Size (1) */
    0x95, 0x08,        /*   Report Count (8)  - modifier byte */
    0x81, 0x02,        /*   Input (Data,Var,Abs) */
    0x95, 0x01,        /*   Report Count (1) */
    0x75, 0x08,        /*   Report Size (8)   - reserved */
    0x81, 0x01,        /*   Input (Const) */
    0x95, 0x06,        /*   Report Count (6)  - 6 keycodes */
    0x75, 0x08,        /*   Report Size (8) */
    0x15, 0x00,        /*   Logical Min (0) */
    0x25, 0x65,        /*   Logical Max (101) */
    0x05, 0x07,        /*   Usage Page (Keyboard) */
    0x19, 0x00,        /*   Usage Min (0) */
    0x29, 0x65,        /*   Usage Max (101) */
    0x81, 0x00,        /*   Input (Data,Array) */
    0xC0,              /* End Collection */

    /* Consumer Control */
    0x05, 0x0C,        /* Usage Page (Consumer) */
    0x09, 0x01,        /* Usage (Consumer Control) */
    0xA1, 0x01,        /* Collection (Application) */
    0x85, 0x02,        /*   Report ID (2) */
    0x15, 0x00,        /*   Logical Min (0) */
    0x26, 0xFF, 0x03,  /*   Logical Max (1023) */
    0x19, 0x00,        /*   Usage Min (0) */
    0x2A, 0xFF, 0x03,  /*   Usage Max (1023) */
    0x75, 0x10,        /*   Report Size (16) */
    0x95, 0x01,        /*   Report Count (1) */
    0x81, 0x00,        /*   Input (Data,Array) */
    0xC0,              /* End Collection */
};

/* ---------- UUIDs ---------- */
static const ble_uuid16_t SVC_HID      = BLE_UUID16_INIT(0x1812);
static const ble_uuid16_t CHR_HID_INFO = BLE_UUID16_INIT(0x2A4A);
static const ble_uuid16_t CHR_REPORT_MAP = BLE_UUID16_INIT(0x2A4B);
static const ble_uuid16_t CHR_HID_CTRL = BLE_UUID16_INIT(0x2A4C);
static const ble_uuid16_t CHR_REPORT   = BLE_UUID16_INIT(0x2A4D);
static const ble_uuid16_t CHR_PROTO_MODE = BLE_UUID16_INIT(0x2A4E);
static const ble_uuid16_t DSC_REPORT_REF = BLE_UUID16_INIT(0x2908);

static const ble_uuid16_t SVC_BAS      = BLE_UUID16_INIT(0x180F);
static const ble_uuid16_t CHR_BAT_LVL  = BLE_UUID16_INIT(0x2A19);

static const ble_uuid16_t SVC_DIS      = BLE_UUID16_INIT(0x180A);
static const ble_uuid16_t CHR_PNP_ID   = BLE_UUID16_INIT(0x2A50);

/* Fixed values */
static const uint8_t hid_info[4]   = { 0x11, 0x01, 0x00, 0x02 }; /* bcdHID 1.11, no country, remote-wake */
static       uint8_t hid_ctrl_pt   = 0;
static       uint8_t proto_mode    = 0x01; /* 0x00=Boot, 0x01=Report. We only do Report. */
static const uint8_t batt_level    = 100;

/* PnP ID:
 *   [0]    Vendor ID Source: 0x02 = USB Implementers Forum
 *   [1-2]  Vendor ID (LE)   : 0x05AC = Apple (used widely by hobbyist HID
 *                             devices; not officially registered to us, but
 *                             BlueZ accepts it without question)
 *   [3-4]  Product ID (LE)  : 0x820A (arbitrary; "NuttyPresenter")
 *   [5-6]  Product Version  : 0x0001
 */
static const uint8_t pnp_id[7] = {
    0x02,
    0xAC, 0x05,
    0x0A, 0x82,
    0x01, 0x00,
};

/* Report Reference descriptor payloads: <ReportID, ReportType=1 (Input)> */
static const uint8_t rpt_ref_kbd[2] = { 0x01, 0x01 };
static const uint8_t rpt_ref_cc[2]  = { 0x02, 0x01 };

/* ---------- GATT access callbacks ---------- */
static int gatt_chr_read_static(uint16_t conn, uint16_t attr,
                                struct ble_gatt_access_ctxt *ctxt, void *arg) {
    const struct {
        const void *data;
        uint16_t len;
    } *blob = arg;
    return os_mbuf_append(ctxt->om, blob->data, blob->len) == 0
        ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

#define STATIC_BLOB(name, ptr, length) \
    static const struct { const void *data; uint16_t len; } name = { (ptr), (length) }

STATIC_BLOB(blob_report_map, hid_report_map, sizeof(hid_report_map));
STATIC_BLOB(blob_hid_info,   hid_info,       sizeof(hid_info));
STATIC_BLOB(blob_batt,       &batt_level,    sizeof(batt_level));
STATIC_BLOB(blob_rpt_ref_kbd, rpt_ref_kbd,   sizeof(rpt_ref_kbd));
STATIC_BLOB(blob_rpt_ref_cc,  rpt_ref_cc,    sizeof(rpt_ref_cc));
STATIC_BLOB(blob_pnp_id,     pnp_id,         sizeof(pnp_id));

static int gatt_chr_hid_ctrl(uint16_t conn, uint16_t attr,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len > 0) ble_hs_mbuf_to_flat(ctxt->om, &hid_ctrl_pt, 1, NULL);
    }
    return 0;
}

/* Protocol Mode: read returns current mode, write changes it. We only
   really support Report Protocol; if the host writes Boot we accept the
   write but keep emitting report-protocol packets. BlueZ doesn't actually
   exercise Boot mode for HoG, it just needs the characteristic to exist
   and be writable. */
static int gatt_chr_proto_mode(uint16_t conn, uint16_t attr,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return os_mbuf_append(ctxt->om, &proto_mode, 1) == 0
            ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t val = 0;
        ble_hs_mbuf_to_flat(ctxt->om, &val, 1, NULL);
        proto_mode = val;
        ESP_LOGI(TAG, "Protocol mode set to %d (informational only)", val);
    }
    return 0;
}

static int gatt_chr_report_input(uint16_t conn, uint16_t attr,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) {
    /* Host reads of the input characteristic return an empty report.
       Real input is delivered via notifications. */
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t zero[8] = {0};
        size_t  sz = (intptr_t)arg;
        return os_mbuf_append(ctxt->om, zero, sz) == 0
            ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return 0;
}

/* Forward declarations of attr handles */
static uint16_t hnd_kbd_input;
static uint16_t hnd_cc_input;

/* Common HID-attribute permission flags. iOS will refuse to engage as a
   HID host unless the protected attributes (Report Map, Reports, Report
   Reference descriptors) require both encryption and authentication.
   Marking them as such forces iOS to initiate pairing the moment it
   tries to read the Report Map or subscribe to a report. */
#define HID_CHR_READ_PERM   (BLE_GATT_CHR_F_READ | \
                             BLE_GATT_CHR_F_READ_ENC | \
                             BLE_GATT_CHR_F_READ_AUTHEN)
#define HID_CHR_RDNOT_PERM  (HID_CHR_READ_PERM | BLE_GATT_CHR_F_NOTIFY)
#define HID_DSC_READ_PERM   (BLE_ATT_F_READ | \
                             BLE_ATT_F_READ_ENC | \
                             BLE_ATT_F_READ_AUTHEN)

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        /* Device Information Service - mandatory for BlueZ to register us
           as a HID device with the kernel. PnP ID gives the host vendor
           and product IDs, which are what the kernel HID layer uses to
           pick a driver and publish /dev/input/eventN. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SVC_DIS.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &CHR_PNP_ID.u,
                .access_cb = gatt_chr_read_static,
                .arg = (void *)&blob_pnp_id,
                .flags = BLE_GATT_CHR_F_READ,
            },
            { 0 }
        },
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SVC_BAS.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &CHR_BAT_LVL.u,
                .access_cb = gatt_chr_read_static,
                .arg = (void *)&blob_batt,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 }
        },
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SVC_HID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {   /* HID Information */
                .uuid = &CHR_HID_INFO.u,
                .access_cb = gatt_chr_read_static,
                .arg = (void *)&blob_hid_info,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {   /* Report Map - encryption-required to make iOS pair */
                .uuid = &CHR_REPORT_MAP.u,
                .access_cb = gatt_chr_read_static,
                .arg = (void *)&blob_report_map,
                .flags = HID_CHR_READ_PERM,
            },
            {   /* HID Control Point */
                .uuid = &CHR_HID_CTRL.u,
                .access_cb = gatt_chr_hid_ctrl,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {   /* Protocol Mode - mandatory for BlueZ to attach as HID.
                   Spec requires it whenever the report map declares boot-
                   keyboard usages, which ours does (modifier byte etc.). */
                .uuid = &CHR_PROTO_MODE.u,
                .access_cb = gatt_chr_proto_mode,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {   /* Keyboard input report (ID 1) - encryption-required */
                .uuid = &CHR_REPORT.u,
                .access_cb = gatt_chr_report_input,
                .arg = (void *)8,
                .flags = HID_CHR_RDNOT_PERM,
                .val_handle = &hnd_kbd_input,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = &DSC_REPORT_REF.u,
                        .att_flags = HID_DSC_READ_PERM,
                        .access_cb = gatt_chr_read_static,
                        .arg = (void *)&blob_rpt_ref_kbd,
                    },
                    { 0 }
                },
            },
            {   /* Consumer control input report (ID 2) - encryption-required */
                .uuid = &CHR_REPORT.u,
                .access_cb = gatt_chr_report_input,
                .arg = (void *)2,
                .flags = HID_CHR_RDNOT_PERM,
                .val_handle = &hnd_cc_input,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = &DSC_REPORT_REF.u,
                        .att_flags = HID_DSC_READ_PERM,
                        .access_cb = gatt_chr_read_static,
                        .arg = (void *)&blob_rpt_ref_cc,
                    },
                    { 0 }
                },
            },
            { 0 }
        },
    },
    { 0 }
};

/* ---------- Advertising ---------- */
static void start_advertising(void);

static int gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            appState.conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "Link up, conn_handle=%d (waiting for pairing)",
                     appState.conn_handle);
            /* Don't flip the UI to "Connected!" yet - the link is up but
               un-encrypted. We wait for ENC_CHANGE with status=0 before
               telling the user we're really paired and able to send HID. */

            /* Proactively kick off pairing/encryption. iOS in particular
               often won't initiate security on its own when connecting to
               a HID peripheral - it waits for the peripheral to ask, or
               for an attribute access to fail with insufficient-encryption.
               Calling ble_gap_security_initiate() here triggers the
               Security Manager handshake immediately, which is when
               BLE_GAP_EVENT_PASSKEY_ACTION fires and our PIN appears. */
            int srx = ble_gap_security_initiate(event->connect.conn_handle);
            if (srx != 0 && srx != BLE_HS_EALREADY) {
                ESP_LOGW(TAG, "security_initiate rc=%d", srx);
            }
        } else {
            ESP_LOGW(TAG, "Connect failed (%d)", event->connect.status);
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected (reason=%d)", event->disconnect.reason);
        appState.state = PRESENTER_ADVERTISING;
        appState.conn_handle = 0;
        appState.kbd_input_handle = 0;
        appState.cc_input_handle = 0;
        g_passkey = 0;  /* clear stale PIN; new one issued on next pair */
        if (g_exiting) {
            /* App is shutting down - don't restart advertising. */
            ESP_LOGI(TAG, "Exit in progress, not re-advertising");
        } else {
            ui_set_state(PRESENTER_ADVERTISING);
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "Encryption change, status=%d", event->enc_change.status);
        if (event->enc_change.status == 0) {
            /* Link is now encrypted and authenticated - we're truly paired.
               Flip the UI and start accepting HID input. */
            appState.state = PRESENTER_CONNECTED;
            ui_set_state(PRESENTER_CONNECTED);
        } else {
            /* Pairing failed (wrong PIN, user canceled, etc.) - drop the link.
               The host won't be able to send HID reports without an encrypted
               connection anyway. */
            ESP_LOGW(TAG, "Pairing failed - terminating connection");
            ble_gap_terminate(event->enc_change.conn_handle,
                              BLE_ERR_AUTH_FAIL);
        }
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Subscribe: attr=%d notify=%d",
                 event->subscribe.attr_handle, event->subscribe.cur_notify);
        if (event->subscribe.attr_handle == hnd_kbd_input) {
            appState.kbd_input_handle = hnd_kbd_input;
        } else if (event->subscribe.attr_handle == hnd_cc_input) {
            appState.cc_input_handle = hnd_cc_input;
        }
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU=%d", event->mtu.value);
        break;

    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        struct ble_sm_io pkey = {0};
        ESP_LOGI(TAG, "PASSKEY_ACTION action=%d",
                 event->passkey.params.action);
        switch (event->passkey.params.action) {
        case BLE_SM_IOACT_DISP:
            /* Passkey Entry, peripheral displays. We pick the number; the
               user types it on the host. Generate it HERE (not at app
               start) so the value on the badge screen is guaranteed to be
               the value handed to SM. */
            g_passkey = 100000 + (esp_random() % 900000);
            pkey.action  = BLE_SM_IOACT_DISP;
            pkey.passkey = g_passkey;
            ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "Displaying passkey %06lu",
                     (unsigned long)g_passkey);
            ui_set_state(PRESENTER_ADVERTISING); /* repaint with new PIN */
            break;

        case BLE_SM_IOACT_NUMCMP:
            /* LE Secure Connections Numeric Comparison. The value comes
               from the SM (derived from the SC public-key exchange) - we
               MUST display exactly numcmp, not a number we made up, or it
               won't match what the host shows. Auto-accept; the user's
               confirmation on the host is the real check. */
            g_passkey = event->passkey.params.numcmp;
            pkey.action = BLE_SM_IOACT_NUMCMP;
            pkey.numcmp_accept = 1;
            ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "Numeric comparison value %06lu (auto-accepted)",
                     (unsigned long)g_passkey);
            ui_set_state(PRESENTER_ADVERTISING); /* repaint with SC value */
            break;

        case BLE_SM_IOACT_INPUT:
            /* Peer wants us to type a passkey - we have no keyboard. */
            ESP_LOGW(TAG, "Peer asked us to INPUT a passkey - cannot satisfy");
            ble_gap_terminate(event->passkey.conn_handle, BLE_ERR_AUTH_FAIL);
            break;

        case BLE_SM_IOACT_OOB:
        default:
            ESP_LOGW(TAG, "Unsupported passkey action %d",
                     event->passkey.params.action);
            ble_gap_terminate(event->passkey.conn_handle, BLE_ERR_AUTH_FAIL);
            break;
        }
        break;
    }

    default:
        break;
    }
    return 0;
}

static void start_advertising(void) {
    /* Primary advertising payload must fit in 31 bytes. With flags(3) +
       16-bit UUID list(4) + appearance(4) = 11 bytes, only 20 bytes remain
       for the device name TLV. "NuttyPresenterAABBCC" is 20 chars + 2-byte
       header = 22 bytes, which overflows. So we put the full name in the
       SCAN RESPONSE and leave the primary advertisement compact. */
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    ble_uuid16_t hid_svc = BLE_UUID16_INIT(0x1812);
    fields.uuids16 = &hid_svc;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    fields.appearance = 0x03C1; /* Keyboard */
    fields.appearance_is_present = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields rc=%d", rc);
        return;
    }

    /* Scan response carries the full, MAC-suffixed name. */
    struct ble_hs_adv_fields rsp = {0};
    rsp.name = (const uint8_t *)g_device_name;
    rsp.name_len = strlen(g_device_name);
    rsp.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&rsp);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_rsp_set_fields rc=%d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(appState.own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "adv_start rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising as %s", g_device_name);
    }
}

static void on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &appState.own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "id_infer rc=%d", rc);
        return;
    }
    start_advertising();
}

static void on_reset(int reason) {
    ESP_LOGW(TAG, "BLE reset, reason=%d", reason);
}

static void ble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ---------- HID report transmit helpers ---------- */
static void send_keyboard(uint8_t modifier, uint8_t keycode) {
    if (appState.state != PRESENTER_CONNECTED || appState.kbd_input_handle == 0) return;

    uint8_t press[8]   = { modifier, 0, keycode, 0, 0, 0, 0, 0 };
    uint8_t release[8] = { 0 };

    struct os_mbuf *om = ble_hs_mbuf_from_flat(press, sizeof(press));
    if (om) ble_gatts_notify_custom(appState.conn_handle, hnd_kbd_input, om);

    vTaskDelay(pdMS_TO_TICKS(15));

    om = ble_hs_mbuf_from_flat(release, sizeof(release));
    if (om) ble_gatts_notify_custom(appState.conn_handle, hnd_kbd_input, om);
}

static void send_consumer(uint16_t usage) {
    if (appState.state != PRESENTER_CONNECTED || appState.cc_input_handle == 0) return;

    uint8_t press[2]   = { (uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8) };
    uint8_t release[2] = { 0, 0 };

    struct os_mbuf *om = ble_hs_mbuf_from_flat(press, sizeof(press));
    if (om) ble_gatts_notify_custom(appState.conn_handle, hnd_cc_input, om);

    vTaskDelay(pdMS_TO_TICKS(15));

    om = ble_hs_mbuf_from_flat(release, sizeof(release));
    if (om) ble_gatts_notify_custom(appState.conn_handle, hnd_cc_input, om);
}

/* ---------- BLE bring-up ---------- */
static bool ble_bringup(void) {
    /* Second and later launches: stack is already up, GATT services already
       registered, host task already running. We only need to re-arm
       advertising. NimBLE controller/host can only be initialized once per
       OS lifetime - the previous app version was crashing here on relaunch
       with "controller init failed" because we were trying to init twice. */
    if (g_ble_initialized) {
        ESP_LOGI(TAG, "NimBLE already initialized, re-advertising only");
        ble_svc_gap_device_name_set(g_device_name);
        if (ble_hs_synced()) {
            start_advertising();
        }
        /* If not yet synced, on_sync() will fire and start advertising. */
        return true;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_err_t nrc = nimble_port_init();
    if (nrc != ESP_OK && nrc != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(nrc));
        return false;
    }

    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    /* Pairing config tuned to be Android-friendly:
       - sm_sc = 1 : Required. Android refuses BLE HID pairing without
                     LE Secure Connections (HID needs Security Mode 1 Lvl 4).
       - sm_mitm = 1 : Forces an authenticated pairing method - prevents
                     "Just Works" from being negotiated, which Android would
                     also refuse for HID.
       - io_cap = KeyboardDisplay : This is the key change. With DisplayOnly
                     some Android builds resolve the SC pairing matrix in a
                     way that makes the PHONE generate and display its own
                     passkey (and expects us to input it - which we can't,
                     so pairing fails with the phone showing a number that
                     doesn't match the badge). KeyboardDisplay × Android's
                     KeyboardDisplay resolves to Passkey Entry with the
                     RESPONDER (us) as displayer, which is what we want:
                     badge shows digits, phone asks user to type them. */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_KEYBOARD_DISP;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) { ESP_LOGE(TAG, "count_cfg rc=%d", rc); return false; }
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) { ESP_LOGE(TAG, "add_svcs rc=%d", rc); return false; }

    ble_svc_gap_device_name_set(g_device_name);
    ble_svc_gap_device_appearance_set(0x03C1);

    nimble_port_freertos_init(ble_host_task);
    g_ble_initialized = true;
    return true;
}

static void ble_teardown(void) {
    /* Tell the GAP event handler not to re-advertise once the disconnect
       lands. */
    g_exiting = true;

    /* Always stop advertising - safe whether we're connected or not. */
    ble_gap_adv_stop();

    /* If there's an active link (connected, OR connected-but-not-yet-paired),
       conn_handle is set. Drop it. The previous gating on
       state==PRESENTER_CONNECTED was wrong because that state isn't set
       until encryption completes - quitting mid-pairing left the link up. */
    if (appState.conn_handle != 0) {
        ESP_LOGI(TAG, "Terminating conn_handle=%d", appState.conn_handle);
        int rc = ble_gap_terminate(appState.conn_handle,
                                   BLE_ERR_REM_USER_CONN_TERM);
        if (rc != 0 && rc != BLE_HS_ENOTCONN) {
            ESP_LOGW(TAG, "ble_gap_terminate rc=%d", rc);
        }

        /* ble_gap_terminate is async - the disconnect event arrives later.
           Wait briefly so the host actually sees the link drop before we
           tear down the UI and return to the menu. Cap the wait at ~500ms;
           if NimBLE doesn't drop within that, the link will eventually
           time out on the host side anyway. */
        for (int i = 0; i < 50; i++) {
            if (appState.conn_handle == 0) break; /* cleared by DISCONNECT evt */
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    /* Leave NimBLE running (other BT apps share it) */
}

#else /* !CONFIG_BT_ENABLED */
static void send_keyboard(uint8_t m, uint8_t k) { (void)m; (void)k; }
static void send_consumer(uint16_t u)           { (void)u; }
static bool ble_bringup(void)                   { return false; }
static void ble_teardown(void)                  { }
#endif

/* =========================================================
 * UI
 *
 * The user app area is 128 x 59 px. Layout:
 *   y= 0..5   Title (tiny 4x5 font)                  "BT Presenter"
 *   y= 7..12  Device name line (tiny)                "NuttyPresenterAABBCC"
 *   y=14..23  PIN line (montserrat_10, prominent)    "PIN: 1234"
 *             OR connected status                    "Connected!"
 *   y=25..44  Hint block (tiny, multiline)           controls cheat sheet
 *   y=46..52  Last-action line (tiny)
 *   y=54..58  Exit hint (tiny)
 * ========================================================= */
static void createUI(void) {
    uiMutex = xSemaphoreCreateMutex();
    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();

    NuttyDisplay_lockLVGL();

    mainContainer = lv_obj_create(drawArea);
    lv_obj_set_size(mainContainer, 128, 59);
    lv_obj_set_pos(mainContainer, 0, 0);
    lv_obj_set_style_border_width(mainContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(mainContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mainContainer, LV_OPA_TRANSP, LV_PART_MAIN);

    /* Title */
    titleLabel = lv_label_create(mainContainer);
    lv_obj_set_style_text_font(titleLabel, &cg_pixel_4x5_mono, LV_PART_MAIN);
    lv_obj_set_pos(titleLabel, 1, 0);
    lv_label_set_text(titleLabel, "BT Presenter");

    /* Device name (dynamic; filled in by nutty_main after MAC is read) */
    statusLabel = lv_label_create(mainContainer);
    lv_obj_set_style_text_font(statusLabel, &cg_pixel_4x5_mono, LV_PART_MAIN);
    lv_obj_set_pos(statusLabel, 1, 7);
    lv_label_set_text(statusLabel, "Starting...");

    /* PIN / connected status (the prominent line) */
    hintLabel = lv_label_create(mainContainer);
    lv_obj_set_style_text_font(hintLabel, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_pos(hintLabel, 1, 14);
    lv_label_set_text(hintLabel, "PIN: ----");

    /* Controls cheat sheet - tiny font, multi-line */
    actionLabel = lv_label_create(mainContainer);
    lv_obj_set_style_text_font(actionLabel, &cg_pixel_4x5_mono, LV_PART_MAIN);
    lv_obj_set_pos(actionLabel, 1, 26);
    lv_obj_set_width(actionLabel, 126);
    lv_label_set_long_mode(actionLabel, LV_LABEL_LONG_WRAP);
    lv_label_set_text(actionLabel,
        "L/R: prev/next slide\n"
        "U/D: vol up/down\n"
        "A:next B:black SEL:play\n"
        "hold START to exit");

    NuttyDisplay_unlockLVGL();
}

static void destroyUI(void) {
    if (!mainContainer) return;
    NuttyDisplay_lockLVGL();
    lv_obj_del(mainContainer);
    mainContainer = titleLabel = statusLabel = hintLabel = actionLabel = NULL;
    NuttyDisplay_unlockLVGL();
    if (uiMutex) { vSemaphoreDelete(uiMutex); uiMutex = NULL; }
}

static void ui_set_state(PresenterState s) {
    if (!hintLabel) return;
    NuttyDisplay_lockLVGL();
    if (s == PRESENTER_CONNECTED) {
        lv_label_set_text(hintLabel, "Connected!");
    } else if (g_passkey == 0) {
        /* No pairing in progress yet - SM hasn't asked us for a passkey. */
        lv_label_set_text(hintLabel, "Ready to pair");
    } else {
        /* Always exactly 6 digits because the value is either generated by
           us in 100000..999999 (Passkey Entry) or supplied by the SM as a
           Numeric Comparison value, which is also 6 digits. */
        char buf[16];
        snprintf(buf, sizeof(buf), "PIN: %06lu", (unsigned long)g_passkey);
        lv_label_set_text(hintLabel, buf);
    }
    NuttyDisplay_unlockLVGL();
}

static void ui_set_device_name(const char *name) {
    if (!statusLabel) return;
    NuttyDisplay_lockLVGL();
    lv_label_set_text(statusLabel, name);
    NuttyDisplay_unlockLVGL();
}

static void ui_set_action(const char *txt) {
    /* Just log it - we don't have screen real-estate to spare on the badge,
       and the cheat-sheet is more useful kept on-screen. */
    (void)txt;
    ESP_LOGI(TAG, "%s", txt);
}

/* =========================================================
 * App entrypoint
 * ========================================================= */
static void nutty_main(void) {
    ESP_LOGI(TAG, "Starting BT Presenter");
    memset(&appState, 0, sizeof(appState));
    appState.state = PRESENTER_ADVERTISING;
    g_exiting = false;

    /* Build device name = "NuttyPresenter" + last 3 MAC bytes (hex). */
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(g_device_name, sizeof(g_device_name),
             "%s%02X%02X%02X",
             DEVICE_NAME_PREFIX, mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Device name: %s", g_device_name);

    /* The PIN is NOT generated here. It's generated (or received from the
       SM) in BLE_GAP_EVENT_PASSKEY_ACTION, when pairing actually starts.
       That guarantees the value on the badge screen is the same value the
       SM uses to derive the encryption keys - so it always matches what
       the host shows. */
    g_passkey = 0;

    createUI();
    ui_set_device_name(g_device_name);
    ui_set_state(PRESENTER_ADVERTISING);
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);

    bool bt_ok = ble_bringup();
    if (!bt_ok) {
        NuttyDisplay_lockLVGL();
        lv_label_set_text(hintLabel, "BT not available");
        NuttyDisplay_unlockLVGL();
    }

    while (true) {
        /* Exit: hold START */
        if (NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_START)) {
            ESP_LOGI(TAG, "Exit requested");
            break;
        }

        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_LEFT)) {
            send_keyboard(0, KEY_PAGE_UP);
            ui_set_action("< Prev slide");
        }
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_RIGHT)) {
            send_keyboard(0, KEY_PAGE_DOWN);
            ui_set_action("Next slide >");
        }
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
            send_keyboard(0, KEY_PAGE_DOWN);
            ui_set_action("Next slide >");
        }
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B)) {
            send_keyboard(0, KEY_B);
            ui_set_action("Toggle black");
        }
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
            send_consumer(CC_VOLUME_UP);
            ui_set_action("Vol +");
        }
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
            send_consumer(CC_VOLUME_DOWN);
            ui_set_action("Vol -");
        }
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_SELECT)) {
            send_consumer(CC_PLAY_PAUSE);
            ui_set_action("Play/Pause");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "Tearing down");
    ble_teardown();
    destroyUI();
    NuttyDisplay_clearUserAppArea();
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyBTPresenter = {
    .appName = "BT Presenter",
    .appMainEntry = nutty_main,
    .appHidden = false
};

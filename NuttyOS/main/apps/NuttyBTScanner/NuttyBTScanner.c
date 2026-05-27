#include "NuttyBTScanner.h"
#include "services/NuttyInput/NuttyInput.h"
#include "services/NuttyDisplay/NuttyDisplay.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>

// NimBLE headers
#ifdef CONFIG_BT_ENABLED
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#endif

static const char *TAG = "BTScanner";

#define MAX_BT_DEVICE_COUNT 20
#define BT_SCAN_DONE_BIT BIT0

typedef struct {
    char name[33];
    int8_t rssi;
    uint8_t addr[6];     // MAC address
    uint8_t addr_type;   // Address type
    bool is_connectable;
} BTDeviceInfo;

typedef enum {
    VIEW_LIST,
    VIEW_DETAIL
} ViewMode;

typedef struct {
    BTDeviceInfo device_list[MAX_BT_DEVICE_COUNT];
    uint8_t device_count;
    uint8_t selected_index;
    uint8_t page_start;
    ViewMode current_view;
    bool scan_in_progress;
} AppState;

static AppState appState = {0};
static EventGroupHandle_t bt_event_group;

// UI elements
static lv_obj_t *mainContainer = NULL;
static lv_obj_t *titleLabel = NULL;
static lv_obj_t *listContainer = NULL;
static lv_obj_t *detailContainer = NULL;

// Function declarations
static void initAppState(void);

#ifdef CONFIG_BT_ENABLED
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            ESP_LOGI(TAG, "BLE device found: %02X:%02X:%02X:%02X:%02X:%02X", 
                    event->disc.addr.val[0], event->disc.addr.val[1], event->disc.addr.val[2],
                    event->disc.addr.val[3], event->disc.addr.val[4], event->disc.addr.val[5]);
            
            if (appState.device_count < MAX_BT_DEVICE_COUNT) {
                BTDeviceInfo *device = &appState.device_list[appState.device_count];
                
                // Try to get device name from advertising data
                const uint8_t *name_data = NULL;
                uint8_t name_len = 0;
                
                // Parse advertising data manually to find device name
                const uint8_t *data = event->disc.data;
                uint8_t data_len = event->disc.length_data;
                uint8_t pos = 0;
                
                while (pos < data_len) {
                    uint8_t field_len = data[pos];
                    if (field_len == 0 || pos + field_len > data_len) {
                        break;
                    }
                    
                    uint8_t field_type = data[pos + 1];
                    
                    // Complete Local Name (0x09) or Shortened Local Name (0x08)
                    if (field_type == 0x09 || field_type == 0x08) {
                        name_data = &data[pos + 2];
                        name_len = field_len - 1;
                        break;
                    }
                    
                    pos += field_len + 1;
                }
                
                if (name_data && name_len > 0) {
                    // Copy name, ensuring null termination
                    uint8_t copy_len = (name_len < sizeof(device->name) - 1) ? name_len : sizeof(device->name) - 1;
                    memcpy(device->name, name_data, copy_len);
                    device->name[copy_len] = '\0';
                } else {
                    // Use MAC address as name
                    snprintf(device->name, sizeof(device->name), "BLE_%02X%02X%02X", 
                            event->disc.addr.val[3], event->disc.addr.val[4], event->disc.addr.val[5]);
                }
                
                // Copy other info
                device->rssi = event->disc.rssi;
                memcpy(device->addr, event->disc.addr.val, 6);
                device->addr_type = event->disc.addr.type;
                device->is_connectable = (event->disc.length_data > 0);
                
                appState.device_count++;
                ESP_LOGI(TAG, "Added device %d: %s (RSSI: %d)", 
                        appState.device_count, device->name, device->rssi);
            }
            break;
            
        case BLE_GAP_EVENT_DISC_COMPLETE:
            ESP_LOGI(TAG, "BLE scan completed, found %d devices", appState.device_count);
            appState.scan_in_progress = false;
            xEventGroupSetBits(bt_event_group, BT_SCAN_DONE_BIT);
            break;
            
        default:
            ESP_LOGI(TAG, "BLE GAP event: %d", event->type);
            break;
    }
    
    return 0;
}

static void start_ble_scan() {
    ESP_LOGI(TAG, "Starting BLE scan...");
    appState.scan_in_progress = true;
    appState.device_count = 0;
    
    struct ble_gap_disc_params scan_params = {
        .filter_duplicates = 1,
        .passive = 0,
        .itvl = BLE_GAP_SCAN_ITVL_MS(100),
        .window = BLE_GAP_SCAN_WIN_MS(50),
        .filter_policy = 0,
        .limited = 0,
    };
    
    int err = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 30000, &scan_params, ble_gap_event, NULL);
    if (err != 0) {
        ESP_LOGE(TAG, "Failed to start BLE scan: %d", err);
        appState.scan_in_progress = false;
    } else {
        ESP_LOGI(TAG, "BLE scan started successfully");
    }
}

static void ble_on_sync(void) {
    ESP_LOGI(TAG, "BLE stack synchronized");
    start_ble_scan();
}

static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}
#else
static void start_ble_scan() {
    ESP_LOGI(TAG, "Bluetooth not enabled in this build");
    appState.scan_in_progress = false;
    appState.device_count = 0;
    xEventGroupSetBits(bt_event_group, BT_SCAN_DONE_BIT);
}
#endif

// Initialize app state
static void initAppState(void) {
    memset(&appState, 0, sizeof(AppState));
    appState.scan_in_progress = true;  // Start with scanning state
    appState.device_count = 0;
    appState.selected_index = 0;
    appState.page_start = 0;
    appState.current_view = VIEW_LIST;
}

static void createUI() {
    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    
    NuttyDisplay_lockLVGL();
    
    // Create main container
    mainContainer = lv_obj_create(drawArea);
    lv_obj_set_size(mainContainer, 128, 64);
    lv_obj_set_pos(mainContainer, 0, 0);
    lv_obj_set_style_border_width(mainContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mainContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    
    // Title/Status label (first line)
    titleLabel = lv_label_create(mainContainer);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_pos(titleLabel, 2, 2);
    lv_label_set_text(titleLabel, "BT [Scanning]");
    
    // List container (lines 2-5)
    listContainer = lv_obj_create(mainContainer);
    lv_obj_set_size(listContainer, 124, 45);
    lv_obj_set_pos(listContainer, 2, 12);
    lv_obj_set_style_border_width(listContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(listContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    
    // Detail container (initially hidden)
    detailContainer = lv_obj_create(mainContainer);
    lv_obj_set_size(detailContainer, 124, 40);
    lv_obj_set_pos(detailContainer, 2, 12);
    lv_obj_set_style_border_width(detailContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(detailContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(detailContainer, LV_OBJ_FLAG_HIDDEN);
    
    NuttyDisplay_unlockLVGL();
}

static void updateListDisplay() {
    NuttyDisplay_lockLVGL();
    
    // Clear existing list items
    lv_obj_clean(listContainer);
    
    if (appState.device_count == 0) {
        lv_obj_t *noDeviceLabel = lv_label_create(listContainer);
        lv_obj_set_style_text_font(noDeviceLabel, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_pos(noDeviceLabel, 2, 2);
#ifdef CONFIG_BT_ENABLED
        lv_label_set_text(noDeviceLabel, "No devices found");
#else
        lv_label_set_text(noDeviceLabel, "BT not enabled");
#endif
    } else {
        // Calculate visible range (4 items per page)
        uint8_t items_per_page = 4;
        uint8_t start_idx = appState.page_start;
        uint8_t end_idx = start_idx + items_per_page;
        if (end_idx > appState.device_count) {
            end_idx = appState.device_count;
        }
        
        for (uint8_t i = start_idx; i < end_idx; i++) {
            lv_obj_t *itemLabel = lv_label_create(listContainer);
            lv_obj_set_style_text_font(itemLabel, &lv_font_montserrat_10, LV_PART_MAIN);
            lv_obj_set_pos(itemLabel, 2, (i - start_idx) * 10 + 2);
            
            // Create signal strength bar (5 characters)
            char signalBar[6] = "     ";
            int8_t rssi = appState.device_list[i].rssi;
            
            // Convert RSSI to signal strength (0-5)
            int signalStrength = 0;
            if (rssi >= -50) signalStrength = 5;      // Excellent
            else if (rssi >= -60) signalStrength = 4;  // Good
            else if (rssi >= -70) signalStrength = 3;  // Fair
            else if (rssi >= -80) signalStrength = 2;  // Poor
            else if (rssi >= -90) signalStrength = 1;  // Very Poor
            else signalStrength = 0;                   // No signal
            
            // Fill signal bar
            for (int j = 0; j < signalStrength; j++) {
                signalBar[j] = '|';
            }
            
            // Create device display line
            char line[48];
            snprintf(line, sizeof(line), "[%s] %s", signalBar, appState.device_list[i].name);
            
            // Truncate if too long
            if (strlen(line) > 20) {
                line[17] = '.';
                line[18] = '.';
                line[19] = '.';
                line[20] = '\0';
            }
            
            lv_label_set_text(itemLabel, line);
            
            // Highlight selected item (same as WiFi Scanner)
            if (i == appState.selected_index) {
                lv_obj_set_style_text_color(itemLabel, lv_color_white(), LV_PART_MAIN);
                lv_obj_set_style_bg_color(itemLabel, lv_color_black(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(itemLabel, LV_OPA_COVER, LV_PART_MAIN);
            } else {
                lv_obj_set_style_text_color(itemLabel, lv_color_black(), LV_PART_MAIN);
                lv_obj_set_style_bg_color(itemLabel, lv_color_white(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(itemLabel, LV_OPA_TRANSP, LV_PART_MAIN);
            }
        }
    }
    
    NuttyDisplay_unlockLVGL();
}

static void updateDetailDisplay() {
    NuttyDisplay_lockLVGL();
    
    // Clear existing detail items
    lv_obj_clean(detailContainer);
    
    if (appState.selected_index < appState.device_count) {
        BTDeviceInfo *device = &appState.device_list[appState.selected_index];
        
        // Device name with RSSI
        lv_obj_t *nameLabel = lv_label_create(detailContainer);
        lv_obj_set_style_text_font(nameLabel, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_decor(nameLabel, LV_TEXT_DECOR_UNDERLINE, LV_PART_MAIN);
        lv_obj_set_pos(nameLabel, 2, 2);
        char nameStr[48];  // Increased buffer size
        snprintf(nameStr, sizeof(nameStr), "%s (%ddBm)", device->name, device->rssi);
        lv_label_set_text(nameLabel, nameStr);
        
        // MAC address
        lv_obj_t *addrLabel = lv_label_create(detailContainer);
        lv_obj_set_style_text_font(addrLabel, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_pos(addrLabel, 2, 16);
        char addrStr[32];
        snprintf(addrStr, sizeof(addrStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                device->addr[0], device->addr[1], device->addr[2],
                device->addr[3], device->addr[4], device->addr[5]);
        lv_label_set_text(addrLabel, addrStr);
        
        // Address type
        lv_obj_t *typeLabel = lv_label_create(detailContainer);
        lv_obj_set_style_text_font(typeLabel, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_pos(typeLabel, 2, 26);
        char typeStr[16];
        snprintf(typeStr, sizeof(typeStr), "Type: %s", 
                device->addr_type == BLE_ADDR_PUBLIC ? "Public" : "Random");
        lv_label_set_text(typeLabel, typeStr);
    }
    
    NuttyDisplay_unlockLVGL();
}

static void updateDisplay() {
    if (appState.current_view == VIEW_LIST) {
        updateListDisplay();
        
        // Update title (same format as WiFi Scanner)
        NuttyDisplay_lockLVGL();
        if (appState.scan_in_progress) {
            lv_label_set_text(titleLabel, "BT [Scanning]");
        } else {
            char title[32];
            uint8_t current_page_end = appState.page_start + 4;
            if (current_page_end > appState.device_count) {
                current_page_end = appState.device_count;
            }
            snprintf(title, sizeof(title), "BT [%d-%d/%d]", 
                    appState.page_start + 1, 
                    current_page_end, 
                    appState.device_count);
            lv_label_set_text(titleLabel, title);
        }
        NuttyDisplay_unlockLVGL();
    } else {
        updateDetailDisplay();
        
        // Update title for detail view
        NuttyDisplay_lockLVGL();
        lv_label_set_text(titleLabel, "BT Detail");
        NuttyDisplay_unlockLVGL();
    }
}

static void nutty_main(void) {
    ESP_LOGI(TAG, "BT Scanner app starting...");
    
    // Initialize app state
    initAppState();
    
    // Create event group for scan completion
    bt_event_group = xEventGroupCreate();
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    createUI();
    
    // Clear button states at start
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    
    // Initial display update
    updateDisplay();
    
#ifdef CONFIG_BT_ENABLED
    // Initialize NimBLE (check if already initialized)
    esp_err_t nimble_ret = nimble_port_init();
    if (nimble_ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "NimBLE already initialized, continuing...");
    } else if (nimble_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NimBLE: %s", esp_err_to_name(nimble_ret));
        return;
    }
    
    // Initialize the NimBLE host configuration (these can be called multiple times safely)
    ble_svc_gap_init();
    ble_svc_gatt_init();
    
    // Set the default device name
    ble_svc_gap_device_name_set("NuttyBTScanner");
    
    // Set the callback for sync events
    ble_hs_cfg.sync_cb = ble_on_sync;
    
    // Create the host task (this will fail if already created, but that's OK)
    nimble_port_freertos_init(ble_host_task);
    
    // Start scan immediately if NimBLE was already initialized
    if (nimble_ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "NimBLE already initialized, starting scan directly");
        start_ble_scan();
    }
#else
    // Simulate scan completion immediately
    start_ble_scan();
#endif
    
    // Main app loop
    while (1) {
        // Check for exit first (same as WiFi Scanner)
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_START)) {
            ESP_LOGI(TAG, "Exit requested");
            break;
        }
        
        // Check for scan completion (non-blocking)
        EventBits_t bits = xEventGroupGetBits(bt_event_group);
        if (bits & BT_SCAN_DONE_BIT) {
            xEventGroupClearBits(bt_event_group, BT_SCAN_DONE_BIT);
            updateDisplay();
        }
        
        if (appState.current_view == VIEW_LIST) {
            // List view navigation (same as WiFi Scanner)
            if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
                if (appState.selected_index > 0) {
                    appState.selected_index--;
                    // Adjust page if needed
                    if (appState.selected_index < appState.page_start) {
                        appState.page_start = appState.selected_index;
                    }
                    updateDisplay();
                }
            }
            
            if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
                if (appState.selected_index < appState.device_count - 1) {
                    appState.selected_index++;
                    // Adjust page if needed
                    if (appState.selected_index >= appState.page_start + 4) {
                        appState.page_start = appState.selected_index - 3;
                    }
                    updateDisplay();
                }
            }
            
            // Enter detail view (same as WiFi Scanner)
            if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A) ||
                NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_RIGHT)) {
                if (appState.device_count > 0) {
                    appState.current_view = VIEW_DETAIL;
                    lv_obj_add_flag(listContainer, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(detailContainer, LV_OBJ_FLAG_HIDDEN);
                    updateDisplay();
                }
            }
            
        } else {
            // Detail view - return to list (same as WiFi Scanner)
            if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_LEFT) ||
                NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B)) {
                appState.current_view = VIEW_LIST;
                lv_obj_clear_flag(listContainer, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(detailContainer, LV_OBJ_FLAG_HIDDEN);
                updateDisplay();
            }
        }
        
        // Same delay as WiFi Scanner
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Cleanup before exit
    ESP_LOGI(TAG, "Cleaning up before exit");
    
#ifdef CONFIG_BT_ENABLED
    // Stop scanning if in progress
    if (appState.scan_in_progress) {
        ESP_LOGI(TAG, "Stopping BLE scan");
        ble_gap_disc_cancel();
        appState.scan_in_progress = false;
    }
    
    // Don't deinit NimBLE - let it stay active for other apps (same as WiFi Scanner)
    ESP_LOGI(TAG, "Keeping NimBLE active for other apps");
#endif
    
    // Clean up event group
    if (bt_event_group) {
        ESP_LOGI(TAG, "Deleting event group");
        vEventGroupDelete(bt_event_group);
        bt_event_group = NULL;
    }
    
    // Clear display area
    ESP_LOGI(TAG, "Clearing display area");
    NuttyDisplay_clearUserAppArea();
    
    // Reset app state
    memset(&appState, 0, sizeof(appState));
    
    ESP_LOGI(TAG, "BT Scanner app exit complete");
    
    // Return to main menu (same as WiFi Scanner)
    NuttyApps_launchAppByIndex(0);
}

void NuttyBTScanner_init(void) {
    // This function is called when the app is selected
    ESP_LOGI(TAG, "BT Scanner app initialized");
}

void NuttyBTScanner_deinit(void) {
    // This function is called when the app is deselected
    ESP_LOGI(TAG, "BT Scanner app deinitialized");
    
#ifdef CONFIG_BT_ENABLED
    // Stop scanning if in progress
    if (appState.scan_in_progress) {
        ble_gap_disc_cancel();
        appState.scan_in_progress = false;
    }
#endif
    
    // Clean up UI
    if (mainContainer) {
        NuttyDisplay_lockLVGL();
        lv_obj_del(mainContainer);
        mainContainer = NULL;
        titleLabel = NULL;
        listContainer = NULL;
        detailContainer = NULL;
        NuttyDisplay_unlockLVGL();
    }
    
    // Clean up event group
    if (bt_event_group) {
        vEventGroupDelete(bt_event_group);
        bt_event_group = NULL;
    }
    
    // Reset app state
    memset(&appState, 0, sizeof(appState));
}

NuttyAppDefinition NuttyBTScanner = {
    .appName = "BT Scanner",
    .appMainEntry = nutty_main,
    .appHidden = false
};

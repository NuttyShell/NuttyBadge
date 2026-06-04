#include "usb.h"

static const char *TAG = "USB";

static void device_event_handler(tinyusb_event_t *event, void *arg) {
    switch (event->id) {
        case TINYUSB_EVENT_DETACHED:
            ESP_LOGI(TAG, "USB device detached");
            break;
        case TINYUSB_EVENT_ATTACHED:
            ESP_LOGI(TAG, "USB device attached");
            break;
        default:
            ESP_LOGW(TAG, "Unknown USB event: %d", event->id);
            break;
    }
}

static tinyusb_msc_storage_handle_t storage_hdl = NULL;
static tinyusb_msc_mount_point_t mp;

static esp_err_t init_usb() {
    ESP_LOGI(TAG, "Initializing USB...");
    // TODO: Implement this
    tinyusb_msc_storage_config_t storage_cfg = {
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
        .fat_fs = {
            .base_path = NULL, 
            .config.max_files = 5, 
            .format_flags = 0, 
        },
    };

    //storage_cfg.medium.card = nuttyDriverSDCard.getCardObject();
    //if(storage_cfg.medium.card == NULL) return ESP_ERR_NOT_FOUND;
    //ESP_ERROR_CHECK(tinyusb_msc_new_storage_sdmmc(&storage_cfg, &storage_hdl));

    return ESP_OK;
}


NuttyDriverUSB nuttyDriverUSB = {
    .initUSB = init_usb
};

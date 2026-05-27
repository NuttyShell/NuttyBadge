#include "NuttyWifiScanner.h"
#include "services/NuttyInput/NuttyInput.h"
#include "services/NuttyDisplay/NuttyDisplay.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "WifiScanner";

#define MAX_WIFI_AP_COUNT 20
#define WIFI_SCAN_DONE_BIT BIT0

typedef struct {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t authmode;
    uint8_t channel;
    uint8_t bssid[6];  // MAC address
} WifiAPInfo;

typedef enum {
    VIEW_LIST,
    VIEW_DETAIL
} ViewMode;

typedef struct {
    WifiAPInfo ap_list[MAX_WIFI_AP_COUNT];
    uint8_t ap_count;
    uint8_t selected_index;
    uint8_t page_start;
    ViewMode current_view;
    bool scan_in_progress;
} AppState;

static AppState appState = {0};
static EventGroupHandle_t wifi_event_group;
static esp_event_handler_instance_t scan_handler_instance = NULL;

// UI elements
static lv_obj_t *mainContainer = NULL;
static lv_obj_t *titleLabel = NULL;
static lv_obj_t *listContainer = NULL;
static lv_obj_t *detailContainer = NULL;

static void scan_done_handler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data) {
    ESP_LOGI(TAG, "WiFi scan completed");
    
    uint16_t ap_count = 0;
    esp_err_t err = esp_wifi_scan_get_ap_num(&ap_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP count: %s", esp_err_to_name(err));
        appState.scan_in_progress = false;
        xEventGroupSetBits(wifi_event_group, WIFI_SCAN_DONE_BIT);
        return;
    }
    
    ESP_LOGI(TAG, "Found %d APs", ap_count);
    
    if (ap_count > MAX_WIFI_AP_COUNT) {
        ap_count = MAX_WIFI_AP_COUNT;
    }
    
    if (ap_count == 0) {
        ESP_LOGI(TAG, "No APs found");
        appState.ap_count = 0;
        appState.scan_in_progress = false;
        xEventGroupSetBits(wifi_event_group, WIFI_SCAN_DONE_BIT);
        return;
    }
    
    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_list == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for AP list");
        appState.scan_in_progress = false;
        xEventGroupSetBits(wifi_event_group, WIFI_SCAN_DONE_BIT);
        return;
    }
    
    err = esp_wifi_scan_get_ap_records(&ap_count, ap_list);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP records: %s", esp_err_to_name(err));
        free(ap_list);
        appState.scan_in_progress = false;
        xEventGroupSetBits(wifi_event_group, WIFI_SCAN_DONE_BIT);
        return;
    }
    
    appState.ap_count = 0;
    for (int i = 0; i < ap_count; i++) {
        if (appState.ap_count < MAX_WIFI_AP_COUNT) {
            strncpy(appState.ap_list[appState.ap_count].ssid, (char*)ap_list[i].ssid, 32);
            appState.ap_list[appState.ap_count].ssid[32] = '\0';
            appState.ap_list[appState.ap_count].rssi = ap_list[i].rssi;
            appState.ap_list[appState.ap_count].authmode = ap_list[i].authmode;
            appState.ap_list[appState.ap_count].channel = ap_list[i].primary;
            memcpy(appState.ap_list[appState.ap_count].bssid, ap_list[i].bssid, 6);
            ESP_LOGI(TAG, "AP %d: %s (RSSI: %d, Ch: %d)", 
                    appState.ap_count, 
                    appState.ap_list[appState.ap_count].ssid,
                    appState.ap_list[appState.ap_count].rssi,
                    appState.ap_list[appState.ap_count].channel);
            appState.ap_count++;
        }
    }
    
    free(ap_list);
    appState.scan_in_progress = false;
    ESP_LOGI(TAG, "Scan processing completed, found %d APs", appState.ap_count);
    xEventGroupSetBits(wifi_event_group, WIFI_SCAN_DONE_BIT);
}

static void start_wifi_scan() {
    ESP_LOGI(TAG, "Starting WiFi scan...");
    appState.scan_in_progress = true;
    appState.ap_count = 0;
    
    // Stop any ongoing scan first
    esp_wifi_scan_stop();
    
    // Small delay to ensure WiFi system is ready
    vTaskDelay(pdMS_TO_TICKS(100));
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 120,
        .scan_time.active.max = 150,
    };
    
    esp_err_t err = esp_wifi_scan_start(&scan_config, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(err));
        appState.scan_in_progress = false;
    } else {
        ESP_LOGI(TAG, "WiFi scan started successfully");
    }
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
    lv_label_set_text(titleLabel, "WiFi [Scanning]");
    
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
    
    if (appState.ap_count == 0) {
        lv_obj_t *noApLabel = lv_label_create(listContainer);
        lv_obj_set_style_text_font(noApLabel, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_pos(noApLabel, 2, 2);
        lv_label_set_text(noApLabel, "No networks found");
    } else {
        // Calculate visible range (4 items per page)
        uint8_t items_per_page = 4;
        uint8_t start_idx = appState.page_start;
        uint8_t end_idx = start_idx + items_per_page;
        if (end_idx > appState.ap_count) {
            end_idx = appState.ap_count;
        }
        
        for (uint8_t i = start_idx; i < end_idx; i++) {
            lv_obj_t *itemLabel = lv_label_create(listContainer);
            lv_obj_set_style_text_font(itemLabel, &lv_font_montserrat_10, LV_PART_MAIN);
            lv_obj_set_pos(itemLabel, 2, (i - start_idx) * 10 + 2);
            
            // Create signal strength bar (5 characters)
            char signalBar[6] = "     ";
            int8_t rssi = appState.ap_list[i].rssi;
            
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
            
            char itemText[64];
            const char* securitySuffix;
            if (appState.ap_list[i].authmode == WIFI_AUTH_OPEN) {
                securitySuffix = " (Open)";
            } else {
                securitySuffix = " *";
            }
            snprintf(itemText, sizeof(itemText), "[%s] %s%s", signalBar, appState.ap_list[i].ssid, securitySuffix);
            lv_label_set_text(itemLabel, itemText);
            
            // Highlight selected item
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
    
    lv_obj_clean(detailContainer);
    
    if (appState.selected_index < appState.ap_count) {
        WifiAPInfo *ap = &appState.ap_list[appState.selected_index];
        
        // SSID
        lv_obj_t *ssidLabel = lv_label_create(detailContainer);
        lv_obj_set_style_text_font(ssidLabel, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_decor(ssidLabel, LV_TEXT_DECOR_UNDERLINE, LV_PART_MAIN);
        lv_obj_set_pos(ssidLabel, 2, 2);
        char ssidText[64];
        snprintf(ssidText, sizeof(ssidText), "%s (%ddBm)", ap->ssid, ap->rssi);
        lv_label_set_text(ssidLabel, ssidText);
        
        // RSSI
        lv_obj_t *rssiLabel = lv_label_create(detailContainer);
        lv_obj_set_style_text_font(rssiLabel, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_pos(rssiLabel, 2, 14);
        char rssiText[64];
        char encryptText[32];
        const char* encryptType;
        switch(ap->authmode) {
            case WIFI_AUTH_OPEN:
                encryptType = "Open";
                break;
            case WIFI_AUTH_WEP:
                encryptType = "WEP";
                break;
            case WIFI_AUTH_WPA_PSK:
                encryptType = "WPA-PSK";
                break;
            case WIFI_AUTH_WPA2_PSK:
                encryptType = "WPA2-PSK";
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                encryptType = "WPA/WPA2";
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                encryptType = "WPA2-EAP";
                break;
            case WIFI_AUTH_WPA3_PSK:
                encryptType = "WPA3-PSK";
                break;
            case WIFI_AUTH_WPA2_WPA3_PSK:
                encryptType = "WPA2/WPA3";
                break;
            case WIFI_AUTH_WAPI_PSK:
                encryptType = "WAPI-PSK";
                break;
            default:
                encryptType = "Unknown";
                break;
        }
        snprintf(encryptText, sizeof(encryptText), "%s", encryptType);
        snprintf(rssiText, sizeof(rssiText), "Ch %d\t%s", ap->channel, encryptText);
        lv_label_set_text(rssiLabel, rssiText);
        
        // MAC Address (BSSID)
        lv_obj_t *macLabel = lv_label_create(detailContainer);
        lv_obj_set_style_text_font(macLabel, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_pos(macLabel, 2, 24);
        char macText[32];
        snprintf(macText, sizeof(macText), "MAC %02X:%02X:%02X:%02X:%02X:%02X",
                ap->bssid[0], ap->bssid[1], ap->bssid[2], 
                ap->bssid[3], ap->bssid[4], ap->bssid[5]);
        lv_label_set_text(macLabel, macText);
    }
    
    NuttyDisplay_unlockLVGL();
}

static void updateDisplay() {
    if (appState.current_view == VIEW_LIST) {
        lv_obj_clear_flag(listContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(detailContainer, LV_OBJ_FLAG_HIDDEN);
        updateListDisplay();
        
        // Update status line
        NuttyDisplay_lockLVGL();
        if (appState.scan_in_progress) {
            lv_label_set_text(titleLabel, "WiFi [Scanning]");
        } else {
            char statusText[32];
            uint8_t current_page_end = appState.page_start + 4;
            if (current_page_end > appState.ap_count) {
                current_page_end = appState.ap_count;
            }
            snprintf(statusText, sizeof(statusText), "WiFi [%d-%d/%d]", 
                    appState.page_start + 1, 
                    current_page_end, 
                    appState.ap_count);
            lv_label_set_text(titleLabel, statusText);
        }
        NuttyDisplay_unlockLVGL();
    } else {
        lv_obj_add_flag(listContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(detailContainer, LV_OBJ_FLAG_HIDDEN);
        updateDetailDisplay();
        
        // Update title for detail view
        NuttyDisplay_lockLVGL();
        lv_label_set_text(titleLabel, "WiFi Detail");
        NuttyDisplay_unlockLVGL();
    }
}

static void nutty_main(void) {
    ESP_LOGI(TAG, "Starting WiFi Scanner...");
    
    // Initialize NVS first
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return;
    }
    
    // Initialize WiFi event group
    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }
    
    // Initialize WiFi
    err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(err));
        return;
    }
    
    err = esp_event_loop_create_default();
    if (err == ESP_ERR_INVALID_STATE) {
        // Event loop already exists, this is fine
        ESP_LOGI(TAG, "Event loop already exists, continuing...");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(err));
        return;
    }
    
    // Check if default WiFi STA netif already exists
    esp_netif_t *default_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (default_sta == NULL) {
        esp_netif_create_default_wifi_sta();
    } else {
        ESP_LOGI(TAG, "Default WiFi STA netif already exists, continuing...");
    }
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err == ESP_ERR_INVALID_STATE) {
        // WiFi already initialized, this is fine
        ESP_LOGI(TAG, "WiFi already initialized, continuing...");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(err));
        return;
    }
    
    // Always register event handler (will replace if already exists)
    if (scan_handler_instance != NULL) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, scan_handler_instance);
        scan_handler_instance = NULL;
    }
    
    err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &scan_done_handler, NULL, &scan_handler_instance);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register scan done handler: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Event handler registered successfully");
    
    err = esp_wifi_start();
    if (err == ESP_ERR_INVALID_STATE) {
        // WiFi already started, this is fine
        ESP_LOGI(TAG, "WiFi already started, continuing...");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(err));
        return;
    }
    
    // Ensure WiFi is in STA mode and ready for scanning
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(err));
        return;
    }
    
    // Stop any existing scan
    esp_wifi_scan_stop();
    
    // Initialize application state
    memset(&appState, 0, sizeof(AppState));
    appState.selected_index = 0;
    appState.page_start = 0;
    appState.current_view = VIEW_LIST;
    appState.scan_in_progress = false;
    
    // Create UI
    createUI();
    
    // Start initial scan
    start_wifi_scan();
    updateDisplay();
    
    // Clear button states
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    
    while(true) {
        // Check for exit
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_START)) {
            break;
        }
        
        // Check scan completion
        if (xEventGroupGetBits(wifi_event_group) & WIFI_SCAN_DONE_BIT) {
            xEventGroupClearBits(wifi_event_group, WIFI_SCAN_DONE_BIT);
            updateDisplay();
        }
        
        if (appState.current_view == VIEW_LIST) {
            // List view navigation
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
                if (appState.selected_index > 0) {
                    appState.selected_index--;
                    // Adjust page if needed
                    if (appState.selected_index < appState.page_start) {
                        appState.page_start = appState.selected_index;
                    }
                    updateDisplay();
                }
            }
            
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
                if (appState.selected_index < appState.ap_count - 1) {
                    appState.selected_index++;
                    // Adjust page if needed
                    if (appState.selected_index >= appState.page_start + 4) {
                        appState.page_start = appState.selected_index - 3;
                    }
                    updateDisplay();
                }
            }
            
            // Enter detail view
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A) || 
               NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_RIGHT)) {
                if (appState.ap_count > 0) {
                    appState.current_view = VIEW_DETAIL;
                    updateDisplay();
                }
            }
            
        } else {
            // Detail view - return to list
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_LEFT)) {
                appState.current_view = VIEW_LIST;
                updateDisplay();
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Cleanup
    esp_wifi_scan_stop();  // Stop any ongoing scan
    
    // Unregister event handler
    if (scan_handler_instance != NULL) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, scan_handler_instance);
        scan_handler_instance = NULL;
        ESP_LOGI(TAG, "Event handler unregistered");
    }
    
    // Don't stop or deinit WiFi - let it stay active for other apps
    vEventGroupDelete(wifi_event_group);
    NuttyDisplay_clearUserAppArea();
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyWifiScanner = {
    .appName = "Wifi Scanner",
    .appMainEntry = nutty_main,
    .appHidden = false
};

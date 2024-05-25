#include "NuttySystemMonitor.h"

static const char* TAG = "NuttySystemMonitor";

static SemaphoreHandle_t sysmon_semaphore;
static int NuttySystemMonitor_BatteryVoltage=0;
static uint8_t NuttySystemMonitor_Status=0x00; // 0000 {SD_MNT} {SD_CD} {VBUS} {BATT_LOW}

static SemaphoreHandle_t systemtray_semaphore;
static bool showSystemTray = true;
static char *systemTrayTempText = NULL;
static uint8_t systemTrayTempTextDuration = 0;

int NuttySystemMonitor_getBatteryVoltage() {
    int v=0;
    while(xSemaphoreTake(sysmon_semaphore, (TickType_t)10) != pdTRUE);
    v=NuttySystemMonitor_BatteryVoltage;
    xSemaphoreGive(sysmon_semaphore);
    return v;
}

bool NuttySystemMonitor_isLowBattery() {
    uint8_t s=0;
    while(xSemaphoreTake(sysmon_semaphore, (TickType_t)10) != pdTRUE);
    s=NuttySystemMonitor_Status;
    xSemaphoreGive(sysmon_semaphore);
    return ((s & 0x01) == 0x01);
}

bool NuttySystemMonitor_isVBUSConnected() {
    uint8_t s=0;
    while(xSemaphoreTake(sysmon_semaphore, (TickType_t)10) != pdTRUE);
    s=NuttySystemMonitor_Status;
    xSemaphoreGive(sysmon_semaphore);
    return ((s & 0x02) == 0x02);
}

bool NuttySystemMonitor_isSDCardInserted() {
    uint8_t s=0;
    while(xSemaphoreTake(sysmon_semaphore, (TickType_t)10) != pdTRUE);
    s=NuttySystemMonitor_Status;
    xSemaphoreGive(sysmon_semaphore);
    return ((s & 0x04) == 0x04);
}

bool NuttySystemMonitor_isSDCardMounted() {
    uint8_t s=0;
    while(xSemaphoreTake(sysmon_semaphore, (TickType_t)10) != pdTRUE);
    s=NuttySystemMonitor_Status;
    xSemaphoreGive(sysmon_semaphore);
    return ((s & 0x08) == 0x08);
}

// Hide system tray
void NuttySystemMonitor_hideSystemTray() {
    showSystemTray=false;
}

// Show system tray
void NuttySystemMonitor_showSystemTray() {
    showSystemTray=true;
}


void NuttySystemMonitor_setSystemTrayTempText(char *text, uint8_t durationSecond) {
    while(xSemaphoreTake(systemtray_semaphore, portMAX_DELAY) != pdTRUE);
    systemTrayTempText = text;
    systemTrayTempTextDuration = durationSecond;
    xSemaphoreGive(systemtray_semaphore);
}

static const char *boolToYN(bool in) {
    if(in) {
        return "Y";
    }else{
        return "N";
    }
}

static const char *boolToLoOK(bool in) {
    if(in) {
        return "Lo";
    }else{
        return "OK";
    }
}


static void NuttySystemMonitor_Worker(void* arg) {
    uint8_t ioeReadout, i;
    uint32_t totalVolt=0;
    int adc_raw=0, batt_volt=0;

    lv_style_t system_tray_style;
    lv_style_init(&system_tray_style);
    lv_style_set_text_font(&system_tray_style, &cg_pixel_4x5_mono);

    while(true) {
        // Read IOE for VBUS Detect
        nuttyDriverIOE.lockIOE();
        nuttyDriverIOE.readIOE(&ioeReadout);
        nuttyDriverIOE.releaseIOE();

        // Read ADC 1+5times for battery voltage
        totalVolt = 0;
        nuttyPeripherals.readADC(&adc_raw, &batt_volt); // Discard first result
        for(i=0; i<5; i++) {
            nuttyPeripherals.readADC(&adc_raw, &batt_volt);
            totalVolt += (batt_volt + batt_volt); // *2 for voltage divider
        }
        totalVolt /= 5;
        
        // Update global values
        while(xSemaphoreTake(sysmon_semaphore, (TickType_t)10) != pdTRUE);
        NuttySystemMonitor_BatteryVoltage = totalVolt;
        if(nuttyDriverSDCard.isSDCardMounted()) {
            NuttySystemMonitor_Status |= 0b00001000;
        }else{
            NuttySystemMonitor_Status &= 0b11110111;
        }
        if(nuttyDriverSDCard.getCardInserted()) {
            NuttySystemMonitor_Status |= 0b00000100;
        }else{
            NuttySystemMonitor_Status &= 0b11111011;
        }
        if(ioeReadout & 0x40) {
            NuttySystemMonitor_Status &= 0b11111101;
        }else{
            NuttySystemMonitor_Status |= 0b00000010;
        }
        if(totalVolt > NUTTYSYSTEMMONITOR_LOW_BATTERY_THRESHOLD_MV) {
            NuttySystemMonitor_Status &= 0b11111110;
        }else{
            NuttySystemMonitor_Status |= 0b00000001;
        }
        xSemaphoreGive(sysmon_semaphore);

        // Draw System Info in Tray Area of LCD
        while(xSemaphoreTake(systemtray_semaphore, portMAX_DELAY) != pdTRUE);
        NuttyDisplay_clearSystemTrayArea();
        if(showSystemTray) {
            if(systemTrayTempTextDuration == 0 || systemTrayTempText == NULL) {
                systemTrayTempTextDuration = 0;
                systemTrayTempText = NULL;
                lv_obj_t *tray = NuttyDisplay_getSystemTrayArea();
                NuttyDisplay_lockLVGL();
                lv_obj_t *lbl = lv_label_create(tray);
                lv_obj_add_style(lbl, &system_tray_style, LV_PART_MAIN);
                //lv_obj_set_size(lbl, lv_obj_get_width(tray), lv_obj_get_height(tray));
                lv_label_set_text_fmt(lbl, "SD=%s M=%s BAT=%04d[%s]", 
                boolToYN(NuttySystemMonitor_isSDCardInserted()), 
                boolToYN(NuttySystemMonitor_isSDCardMounted()), 
                NuttySystemMonitor_getBatteryVoltage(),
                boolToLoOK(NuttySystemMonitor_isLowBattery()));
                lv_obj_center(lbl);
                NuttyDisplay_unlockLVGL();
            }else{
                lv_obj_t *tray = NuttyDisplay_getSystemTrayArea();
                NuttyDisplay_lockLVGL();
                lv_obj_t *lbl = lv_label_create(tray);
                lv_obj_add_style(lbl, &system_tray_style, LV_PART_MAIN);
                lv_label_set_text(lbl, systemTrayTempText);
                lv_obj_center(lbl);
                NuttyDisplay_unlockLVGL();
                systemTrayTempTextDuration -= 1;
            }
        }
        xSemaphoreGive(systemtray_semaphore);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    
}

esp_err_t NuttySystemMonitor_Deinit() {
    ESP_LOGI(TAG, "Deinit");
    abort(); // This NuttyService is not allowed to deinit
    return ESP_FAIL;
}

esp_err_t NuttySystemMonitor_Init() {
    ESP_LOGI(TAG, "Init");
	sysmon_semaphore = xSemaphoreCreateMutex();
	systemtray_semaphore = xSemaphoreCreateMutex();
	assert(sysmon_semaphore != NULL);
	assert(systemtray_semaphore != NULL);
    xTaskCreatePinnedToCore(NuttySystemMonitor_Worker, "sysmon_worker", 4096, NULL, 10, NULL, 1);
    return ESP_OK;
}
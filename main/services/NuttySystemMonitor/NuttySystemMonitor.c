#include "NuttySystemMonitor.h"

static const char* TAG = "NuttySystemMonitor";

static SemaphoreHandle_t sysmon_semaphore;
static int NuttySystemMonitor_BatteryVoltage=0;
static uint8_t NuttySystemMonitor_Status=0x00; // 0000 {SD_MNT} {SD_CD} {VBUS} {BATT_LOW}

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


static void NuttySystemMonitor_Worker(void* arg) {
    uint8_t ioeReadout, i;
    uint32_t totalVolt=0;
    int adc_raw=0, batt_volt=0;
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

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    
}

esp_err_t NuttySystemMonitor_Init() {
    ESP_LOGI(TAG, "Init");
	sysmon_semaphore = xSemaphoreCreateMutex();
	assert(sysmon_semaphore != NULL);
    xTaskCreate(NuttySystemMonitor_Worker, "sysmon_worker", 4096, NULL, 10, NULL);
    return ESP_OK;
}
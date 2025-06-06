#include "NuttyIR.h"

static const char* TAG = "NuttyIR";

static SemaphoreHandle_t irMutex=NULL;
static uint16_t addr, cmd;
static uint8_t status;
static bool inited=false;
static TaskHandle_t workerHandle = NULL;

static void NuttyIR_Worker(void* arg) {
    while(true) {
        while(xSemaphoreTake(irMutex, portMAX_DELAY) != pdTRUE);
        nuttyDriverIR.waitForIRNECRx(&addr, &cmd, &status, 100);
        if(status == 2 || status == 4) {
            ESP_LOGI(TAG, "[%d] IRRx: %04X:%04X", status, addr, cmd);
        }else if(status == 1 || status == 3) {
            ESP_LOGI(TAG, "IRRx: NULL");
        }
        xSemaphoreGive(irMutex);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void NuttyIR_TxNEC(uint16_t _addr, uint16_t _cmd) {
    ESP_LOGI(TAG, "IRTxNEC: %04X:%04X", _addr, _cmd);
    nuttyDriverIR.txIRNEC(_addr, _cmd);
}

void NuttyIR_TxNECext(uint16_t _addr, uint16_t _cmd) {
    ESP_LOGI(TAG, "IRTxNECext: %04X:%04X", _addr, _cmd);
    nuttyDriverIR.txIRNECext(_addr, _cmd);
}

void NuttyIR_getLatestRecvResult(uint16_t *_addr, uint16_t *_cmd, uint8_t *_status) {
    if(irMutex == NULL) return;
    while(xSemaphoreTake(irMutex, portMAX_DELAY) != pdTRUE);
    *_addr = addr;
    *_cmd = cmd;
    *_status = status;
    xSemaphoreGive(irMutex);
}

esp_err_t NuttyIR_Init() {
    ESP_LOGI(TAG, "Init");
    if(inited) return ESP_ERR_INVALID_STATE;
    if(irMutex == NULL) irMutex = xSemaphoreCreateMutex();
    xTaskCreate(NuttyIR_Worker, "ir_worker", 4096, NULL, 10, &workerHandle);
    inited = true;
    return ESP_OK;
}

esp_err_t NuttyIR_Deinit() {
    ESP_LOGI(TAG, "Deinit");
    if(!inited) return ESP_ERR_INVALID_STATE;
    if(workerHandle != NULL) vTaskDelete(workerHandle);
    if(irMutex != NULL) {
        vSemaphoreDelete(irMutex);
        irMutex = NULL;
    }
    inited = false;
    return ESP_OK;
}

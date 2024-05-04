#include "NuttyIR.h"

static const char* TAG = "NuttyIR";

static uint16_t addr, cmd;
static uint8_t status;
static void NuttyIR_Worker(void* arg) {
    while(true) {
        nuttyDriverIR.waitForIRNECRx(&addr, &cmd, &status, 1000);
        if(status == 2 || status == 4) {
            ESP_LOGI(TAG, "[%d] IRRx: %04X:%04X", status, addr, cmd);
        }else if(status == 1 || status == 3) {
                ESP_LOGI(TAG, "IRRx: NULL");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void NuttyIR_Replay() {
    ESP_LOGI(TAG, "IRTx: %04X:%04X", addr, cmd);
    nuttyDriverIR.txIRNEC(addr, cmd);
}

esp_err_t NuttyIR_Init() {
    ESP_LOGI(TAG, "Init");
    //xTaskCreate(NuttyIR_Worker, "ir_worker", 3072, NULL, 10, NULL);
    return ESP_OK;
}
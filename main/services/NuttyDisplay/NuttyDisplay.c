#include "NuttyDisplay.h"

static const char* TAG = "NuttyDisplay";


static void NuttyDisplay_Worker(void* arg) {
    
    while(true) {
        

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    
}

esp_err_t NuttyDisplay_Init() {
    ESP_LOGI(TAG, "Init");
    xTaskCreate(NuttyDisplay_Worker, "display_worker", 2048, NULL, 10, NULL);
    return ESP_OK;
}
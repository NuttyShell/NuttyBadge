#include "NuttyRGB.h"

static const char* TAG = "NuttyRGB";


static void NuttyRGB_Worker(void* arg) {
    
    while(true) {
        nuttyDriverRGB.setRGBWithoutDisplay(0, 255,255,255);
        nuttyDriverRGB.setRGBWithoutDisplay(1, 255,255,255);
        nuttyDriverRGB.setRGBWithoutDisplay(2, 255,255,255);
        nuttyDriverRGB.displayNow();
        vTaskDelay(pdMS_TO_TICKS(2000));
        nuttyDriverRGB.setRGBWithoutDisplay(0, 255,0,0);
        nuttyDriverRGB.setRGBWithoutDisplay(1, 0,255,0);
        nuttyDriverRGB.setRGBWithoutDisplay(2, 0,0,255);
        nuttyDriverRGB.displayNow();
        vTaskDelay(pdMS_TO_TICKS(2000));
        nuttyDriverRGB.setRGBWithoutDisplay(0, 0,0,0);
        nuttyDriverRGB.setRGBWithoutDisplay(1, 0,0,0);
        nuttyDriverRGB.setRGBWithoutDisplay(2, 0,0,0);
        nuttyDriverRGB.displayNow();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    
}

esp_err_t NuttyRGB_Init() {
    ESP_LOGI(TAG, "Init");
    xTaskCreate(NuttyRGB_Worker, "rgb_worker", 2048, NULL, 10, NULL);
    return ESP_OK;
}
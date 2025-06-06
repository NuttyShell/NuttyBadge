#include "NuttyRGB.h"

static const char* TAG = "NuttyRGB";


static SemaphoreHandle_t rgbAnimationMutex=NULL;
static bool inited=false;
static TaskHandle_t workerHandle = NULL;
static bool animationStart=false;
static NuttyRGBAnimationSequence *animationSequence;
static uint16_t animationSequenceSize;

void NuttyRGB_SetRGBWithoutDisplay(uint8_t bulb, uint8_t r, uint8_t g, uint8_t b) {
    nuttyDriverRGB.setRGBWithoutDisplay(bulb, r, g, b);
}

void NuttyRGB_SetRGBAndDisplay(uint8_t bulb, uint8_t r, uint8_t g, uint8_t b) {
    nuttyDriverRGB.setRGBAndDisplay(bulb, r, g, b);
}

void NuttyRGB_SetHSVWithoutDisplay(uint8_t bulb, uint8_t h, uint8_t s, uint8_t v) {
    nuttyDriverRGB.setHSVWithoutDisplay(bulb, h, s, v);
}

void NuttyRGB_SetHSVAndDisplay(uint8_t bulb, uint8_t h, uint8_t s, uint8_t v) {
    nuttyDriverRGB.setHSVAndDisplay(bulb, h, s, v);
}

void NuttyRGB_SetAllRGBWithoutDisplay(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t i;
    for(i=0; i<RGB_BULBS; i++) NuttyRGB_SetRGBWithoutDisplay(i, r, g, b);
}

void NuttyRGB_DisplayNow() {
    nuttyDriverRGB.displayNow();
}

void NuttyRGB_SetAllRGBAndDisplay(uint8_t r, uint8_t g, uint8_t b) {
    NuttyRGB_SetAllRGBWithoutDisplay(r, g, b);
    NuttyRGB_DisplayNow();
}

void NuttyRGB_SetAnimationSequences(NuttyRGBAnimationSequence *animSeq, uint16_t size) {
    while(xSemaphoreTake(rgbAnimationMutex, portMAX_DELAY) != pdTRUE);
    animationSequence=animSeq;
    animationSequenceSize=size;
    xSemaphoreGive(rgbAnimationMutex);
}

void NuttyRGB_StartAnimation() {
    while(xSemaphoreTake(rgbAnimationMutex, portMAX_DELAY) != pdTRUE);
    animationStart=true;
    xSemaphoreGive(rgbAnimationMutex);
}
void NuttyRGB_StopAnimation() {
    while(xSemaphoreTake(rgbAnimationMutex, portMAX_DELAY) != pdTRUE);
    animationStart=false;
    xSemaphoreGive(rgbAnimationMutex);
}

static void NuttyRGB_Worker(void* arg) {
    
    uint16_t i;
    uint8_t j;
    while(true) {
        //ESP_LOGI(TAG, "TICK");
        while(xSemaphoreTake(rgbAnimationMutex, portMAX_DELAY) != pdTRUE);
        if(animationStart) {
            for(i=0; i<animationSequenceSize; i++) {
                for(j=0; j<3; j++) NuttyRGB_SetRGBWithoutDisplay(j, animationSequence[i].r[j], animationSequence[i].g[j], animationSequence[i].b[j]);
                NuttyRGB_DisplayNow();
                vTaskDelay(pdMS_TO_TICKS(animationSequence[i].durationMs));
            }
            animationStart=false;
            xSemaphoreGive(rgbAnimationMutex);
        }else{
            xSemaphoreGive(rgbAnimationMutex);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

esp_err_t NuttyRGB_Init() {
    ESP_LOGI(TAG, "Init");
    if(inited) return ESP_ERR_INVALID_STATE;
    if(rgbAnimationMutex == NULL) rgbAnimationMutex = xSemaphoreCreateMutex();
    xTaskCreate(NuttyRGB_Worker, "rgb_worker", 4096, NULL, 10, &workerHandle);
    return ESP_OK;
}

esp_err_t NuttyRGB_Deinit() {
    ESP_LOGI(TAG, "Deinit");
    if(!inited) return ESP_ERR_INVALID_STATE;
    if(workerHandle != NULL) vTaskDelete(workerHandle);
    if(rgbAnimationMutex != NULL) {
        vSemaphoreDelete(rgbAnimationMutex);
        rgbAnimationMutex = NULL;
    }
    inited = false;
    return ESP_OK;
}
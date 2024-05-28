#include "NuttyStorage.h"

static const char* TAG = "NuttyStorage";

static bool sdPersistAndInited=false;
static bool mounted=false;
static uint64_t sdSzMb=0;
static char *sdType=NULL;
static SemaphoreHandle_t storage_semaphore;


bool NuttyStorage_isSDCardMounted() {
    bool result = false;
    while(xSemaphoreTake(storage_semaphore, portMAX_DELAY) != pdTRUE);
    result = mounted;
    xSemaphoreGive(storage_semaphore);
    return result;
}

bool NuttyStorage_isSDCardInserted() {
    return nuttyDriverSDCard.getCardInserted();
}

bool NuttyStorage_isSDCardPersist() {
    bool result = false;
    while(xSemaphoreTake(storage_semaphore, portMAX_DELAY) != pdTRUE);
    result = sdPersistAndInited;
    xSemaphoreGive(storage_semaphore);
    return result;
}

void NuttyStorage_getSDCardSizeMB(uint64_t *mb) {
    while(xSemaphoreTake(storage_semaphore, portMAX_DELAY) != pdTRUE);
    *mb = sdSzMb;
    xSemaphoreGive(storage_semaphore);
}

void NuttyStorage_getSDCardType(char **type) {
    while(xSemaphoreTake(storage_semaphore, portMAX_DELAY) != pdTRUE);
    *type = sdType;
    xSemaphoreGive(storage_semaphore);
}


static void NuttyStorage_Worker(void* arg) {
    while(true) {
        while(xSemaphoreTake(storage_semaphore, portMAX_DELAY) != pdTRUE);
        if(!sdPersistAndInited) {
            ESP_LOGI(TAG, "Probing SD: %d", nuttyDriverSDCard.getCardInserted());
            if(nuttyDriverSDCard.getCardInserted() && nuttyDriverSDCard.initSDCard() == ESP_OK) {
                sdPersistAndInited = true;
                if(nuttyDriverSDCard.getSDCardSizeMB(&sdSzMb) != ESP_OK) sdSzMb = 0;
                if(nuttyDriverSDCard.getSDCardType(&sdType) != ESP_OK) sdType = NULL;
                ESP_LOGI(TAG, "SDCard detected, trying to mount...");
                nuttyDriverSDCard.mountSDCard();
                mounted = nuttyDriverSDCard.isSDCardMounted();
                xSemaphoreGive(storage_semaphore);
            }else{
                xSemaphoreGive(storage_semaphore);
                vTaskDelay(pdMS_TO_TICKS(2000)); // Poll in longer period
            }
        }else{
            if(!nuttyDriverSDCard.isSDCardPersist()) {
                sdPersistAndInited = false;
                mounted = false;
                nuttyDriverSDCard.unmountSDCard();
                ESP_LOGI(TAG, "SDCard no longer persist");
            }else{
                mounted = nuttyDriverSDCard.isSDCardMounted();
            }
            xSemaphoreGive(storage_semaphore);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        
    }
    
    
}

esp_err_t NuttyStorage_Deinit() {
    ESP_LOGI(TAG, "Deinit");
    abort(); // This NuttyService is not allowed to deinit
    return ESP_FAIL;
}

esp_err_t NuttyStorage_Init() {
    ESP_LOGI(TAG, "Init");
	storage_semaphore = xSemaphoreCreateMutex();
	assert(storage_semaphore != NULL);
    xTaskCreate(NuttyStorage_Worker, "storage_worker", 4096, NULL, 10, NULL);
    return ESP_OK;
}
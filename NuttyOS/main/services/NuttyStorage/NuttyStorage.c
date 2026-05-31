#include "NuttyStorage.h"

static const char* TAG = "NuttyStorage";

// SD Card
static bool sdPersistAndInited=false;
static bool mounted=false;
static uint64_t sdSzMb=0;
static char *sdType=NULL;
static SemaphoreHandle_t storage_semaphore;

// SD Card
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

// NVS (KV)
esp_err_t NuttyStorage_setIntegerKV(const char* key, NuttyStorageKVIntegerValueType valueType, void *value) {
    switch(valueType) {
        case NVS_TYPE_INT64:
            return nuttyDriverKV.setInt64(key, *(int64_t*)value);
        case NVS_TYPE_UINT64:
            return nuttyDriverKV.setUInt64(key, *(uint64_t*)value);
        case NVS_TYPE_INT32:
            return nuttyDriverKV.setInt32(key, *(int32_t*)value);
        case NVS_TYPE_UINT32:
            return nuttyDriverKV.setUInt32(key, *(uint32_t*)value);
        case NVS_TYPE_INT16:
            return nuttyDriverKV.setInt16(key, *(int16_t*)value);
        case NVS_TYPE_UINT16:
            return nuttyDriverKV.setUInt16(key, *(uint16_t*)value);
        case NVS_TYPE_INT8:
            return nuttyDriverKV.setInt8(key, *(int8_t*)value);
        case NVS_TYPE_UINT8:
            return nuttyDriverKV.setUInt8(key, *(uint8_t*)value);
        default:
            return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t NuttyStorage_getIntegerKV(const char* key, NuttyStorageKVIntegerValueType valueType, void *value) {
    switch(valueType) {
        case NVS_TYPE_INT64:
            return nuttyDriverKV.getInt64(key, (int64_t*)value, 0);
        case NVS_TYPE_UINT64:
            return nuttyDriverKV.getUInt64(key, (uint64_t*)value, 0);
        case NVS_TYPE_INT32:
            return nuttyDriverKV.getInt32(key, (int32_t*)value, 0);
        case NVS_TYPE_UINT32:
            return nuttyDriverKV.getUInt32(key, (uint32_t*)value, 0);
        case NVS_TYPE_INT16:
            return nuttyDriverKV.getInt16(key, (int16_t*)value, 0);
        case NVS_TYPE_UINT16:
            return nuttyDriverKV.getUInt16(key, (uint16_t*)value, 0);
        case NVS_TYPE_INT8:
            return nuttyDriverKV.getInt8(key, (int8_t*)value, 0);
        case NVS_TYPE_UINT8:
            return nuttyDriverKV.getUInt8(key, (uint8_t*)value, 0);
        default:
            return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t NuttyStorage_setStringKV(const char* key, const char* value) {
    return nuttyDriverKV.setStr(key, value);
}

esp_err_t NuttyStorage_getStringKV(const char* key, char *out_value, size_t max_size, const char* default_value) {
    return nuttyDriverKV.getStr(key, out_value, max_size, default_value);
}

esp_err_t NuttyStorge_setBlobKV(const char* key, const void* value, size_t length) {
    return nuttyDriverKV.setBlob(key, value, length);
}

esp_err_t NuttyStorage_getBlobKV(const char* key, void* out_value, size_t max_size, const void* default_value, size_t default_length) {
    return nuttyDriverKV.getBlob(key, out_value, max_size, default_value, default_length);
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
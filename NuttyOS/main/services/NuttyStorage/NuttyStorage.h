#ifndef _NUTTYSTORAGE_H
#define _NUTTYSTORAGE_H

#include "NuttyPeripherals.h"
#include "drivers/ioe.h"
#include "drivers/sdcard.h"
#include "drivers/kv.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>


typedef enum _NuttyStorageKVIntegerValueType {
    NVS_TYPE_INT64,
    NVS_TYPE_UINT64,
    NVS_TYPE_INT32,
    NVS_TYPE_UINT32,
    NVS_TYPE_INT16,
    NVS_TYPE_UINT16,
    NVS_TYPE_INT8,
    NVS_TYPE_UINT8
} NuttyStorageKVIntegerValueType;

esp_err_t NuttyStorage_Init();
bool NuttyStorage_isSDCardMounted();
bool NuttyStorage_isSDCardInserted();
bool NuttyStorage_isSDCardPersist();
void NuttyStorage_getSDCardSizeMB(uint64_t *mb);
void NuttyStorage_getSDCardType(char **type);
esp_err_t NuttyStorage_setIntegerKV(const char* key, NuttyStorageKVIntegerValueType valueType, void *value);
esp_err_t NuttyStorage_getIntegerKV(const char* key, NuttyStorageKVIntegerValueType valueType, void *value);
esp_err_t NuttyStorage_setStringKV(const char* key, const char* value);
esp_err_t NuttyStorage_getStringKV(const char* key, char *out_value, size_t max_size, const char* default_value);
esp_err_t NuttyStorge_setBlobKV(const char* key, const void* value, size_t length);
esp_err_t NuttyStorage_getBlobKV(const char* key, void *out_value, size_t max_size, const void* default_value, size_t default_length);
#endif /* _NUTTYSTORAGE_H */
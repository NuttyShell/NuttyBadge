#ifndef _KV_H
#define _KV_H

#include "config.h"
#include <string.h>
#include <esp_err.h>
#include "NuttyPeripherals.h"
#include "drivers/ioe.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

typedef esp_err_t (*NuttyDriverKVInitFunc)(void);
typedef esp_err_t (*NuttyDriverKVSetInt64Func)(const char *key, int64_t value);
typedef esp_err_t (*NuttyDriverKVGetInt64Func)(const char *key, int64_t *out_value, int64_t default_value);
typedef esp_err_t (*NuttyDriverKVSetUInt64Func)(const char *key, uint64_t value);
typedef esp_err_t (*NuttyDriverKVGetUInt64Func)(const char *key, uint64_t *out_value, uint64_t default_value);
typedef esp_err_t (*NuttyDriverKVSetInt32Func)(const char *key, int32_t value);
typedef esp_err_t (*NuttyDriverKVGetInt32Func)(const char *key, int32_t *out_value, int32_t default_value);
typedef esp_err_t (*NuttyDriverKVSetUInt32Func)(const char *key, uint32_t value);
typedef esp_err_t (*NuttyDriverKVGetUInt32Func)(const char *key, uint32_t *out_value, uint32_t default_value);
typedef esp_err_t (*NuttyDriverKVSetInt16Func)(const char *key, int16_t value);
typedef esp_err_t (*NuttyDriverKVGetInt16Func)(const char *key, int16_t *out_value, int16_t default_value);
typedef esp_err_t (*NuttyDriverKVSetUInt16Func)(const char *key, uint16_t value);
typedef esp_err_t (*NuttyDriverKVGetUInt16Func)(const char *key, uint16_t *out_value, uint16_t default_value);
typedef esp_err_t (*NuttyDriverKVSetInt8Func)(const char *key, int8_t value);
typedef esp_err_t (*NuttyDriverKVGetInt8Func)(const char *key, int8_t *out_value, int8_t default_value);
typedef esp_err_t (*NuttyDriverKVSetUInt8Func)(const char *key, uint8_t value);
typedef esp_err_t (*NuttyDriverKVGetUInt8Func)(const char *key, uint8_t *out_value, uint8_t default_value);
typedef esp_err_t (*NuttyDriverKVSetStrFunc)(const char *key, const char *value);
typedef esp_err_t (*NuttyDriverKVGetStrFunc)(const char *key, char *out_value, size_t max_size, const char *default_value);
typedef esp_err_t (*NuttyDriverKVSetBlobFunc)(const char *key, const void *value, size_t length);
typedef esp_err_t (*NuttyDriverKVGetBlobFunc)(const char *key, void *out_value, size_t max_size, const void *default_value, size_t default_length);
typedef struct _NuttyDriverKV {
    NuttyDriverKVInitFunc initKV;
    NuttyDriverKVSetBlobFunc setBlob;
    NuttyDriverKVGetBlobFunc getBlob;
    NuttyDriverKVSetStrFunc setStr;
    NuttyDriverKVGetStrFunc getStr;
    NuttyDriverKVSetInt64Func setInt64;
    NuttyDriverKVGetInt64Func getInt64;
    NuttyDriverKVSetUInt64Func setUInt64;
    NuttyDriverKVGetUInt64Func getUInt64;
    NuttyDriverKVSetInt32Func setInt32;
    NuttyDriverKVGetInt32Func getInt32;
    NuttyDriverKVSetUInt32Func setUInt32;
    NuttyDriverKVGetUInt32Func getUInt32;
    NuttyDriverKVSetInt16Func setInt16;
    NuttyDriverKVGetInt16Func getInt16;
    NuttyDriverKVSetUInt16Func setUInt16;
    NuttyDriverKVGetUInt16Func getUInt16;
    NuttyDriverKVSetInt8Func setInt8;
    NuttyDriverKVGetInt8Func getInt8;
    NuttyDriverKVSetUInt8Func setUInt8;
    NuttyDriverKVGetUInt8Func getUInt8;

} NuttyDriverKV;
extern NuttyDriverKV nuttyDriverKV;

#endif /* _KV_H */
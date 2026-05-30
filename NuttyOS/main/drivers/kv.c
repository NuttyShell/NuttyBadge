#include "kv.h"

static nvs_handle_t kv_nvs_handle;
static const char* NVS_NAMESPACE = "NuttyKVStore";
esp_err_t kv_init() {
    return nvs_open(NVS_NAMESPACE, NVS_READWRITE, &kv_nvs_handle);
}

esp_err_t kv_set_int64(const char *key, int64_t value) {
    esp_err_t err = nvs_set_i64(kv_nvs_handle, key, value);
    if (err != ESP_OK) {
        return err;
    }
    return nvs_commit(kv_nvs_handle);
}

esp_err_t kv_get_int64(const char *key, int64_t *out_value, int64_t default_value) {
    esp_err_t err = nvs_get_i64(kv_nvs_handle, key, out_value);
    if (err != ESP_OK) {
        *out_value = default_value;
    }
    return err;
}

esp_err_t kv_set_uint64(const char *key, uint64_t value) {
    esp_err_t err = nvs_set_u64(kv_nvs_handle, key, (int64_t)value);
    if (err != ESP_OK) {
        return err;
    }
    return nvs_commit(kv_nvs_handle);
}

esp_err_t kv_get_uint64(const char *key, uint64_t *out_value, uint64_t default_value) {
    esp_err_t err = nvs_get_u64(kv_nvs_handle, key, out_value);
    if (err != ESP_OK) {
        *out_value = default_value;
    }
    return err;
}


esp_err_t kv_set_int32(const char *key, int32_t value) {
    esp_err_t err = nvs_set_i32(kv_nvs_handle, key, value);
    if (err != ESP_OK) {
        return err;
    }
    return nvs_commit(kv_nvs_handle);
}

esp_err_t kv_get_int32(const char *key, int32_t *out_value, int32_t default_value) {
    esp_err_t err = nvs_get_i32(kv_nvs_handle, key, out_value);
    if (err != ESP_OK) {
        *out_value = default_value;
    }
    return err;
}

esp_err_t kv_set_uint32(const char *key, uint32_t value) {
    esp_err_t err = nvs_set_u32(kv_nvs_handle, key, value);
    if (err != ESP_OK) {
        return err;
    }
    return nvs_commit(kv_nvs_handle);
}

esp_err_t kv_get_uint32(const char *key, uint32_t *out_value, uint32_t default_value) {
    esp_err_t err = nvs_get_u32(kv_nvs_handle, key, out_value);
    if (err != ESP_OK) {
        *out_value = default_value;
    }
    return err;
}


esp_err_t kv_set_int16(const char *key, int16_t value) {
    esp_err_t err = nvs_set_i16(kv_nvs_handle, key, value);
    if (err != ESP_OK) {
        return err;
    }
    return nvs_commit(kv_nvs_handle);
}

esp_err_t kv_get_int16(const char *key, int16_t *out_value, int16_t default_value) {
    esp_err_t err = nvs_get_i16(kv_nvs_handle, key, out_value);
    if (err != ESP_OK) {
        *out_value = default_value;
    }
    return err;
}

esp_err_t kv_set_uint16(const char *key, uint16_t value) {
    esp_err_t err = nvs_set_u16(kv_nvs_handle, key, value);
    if (err != ESP_OK) {
        return err;
    }
    return nvs_commit(kv_nvs_handle);
}

esp_err_t kv_get_uint16(const char *key, uint16_t *out_value, uint16_t default_value) {
    esp_err_t err = nvs_get_u16(kv_nvs_handle, key, out_value);
    if (err != ESP_OK) {
        *out_value = default_value;
    }
    return err;
}


esp_err_t kv_set_int8(const char *key, int8_t value) {
    esp_err_t err = nvs_set_i8(kv_nvs_handle, key, value);
    if (err != ESP_OK) {
        return err;
    }
    return nvs_commit(kv_nvs_handle);
}

esp_err_t kv_get_int8(const char *key, int8_t *out_value, int8_t default_value) {
    esp_err_t err = nvs_get_i8(kv_nvs_handle, key, out_value);
    if (err != ESP_OK) {
        *out_value = default_value;
    }
    return err;
}

esp_err_t kv_set_uint8(const char *key, uint8_t value) {
    esp_err_t err = nvs_set_u8(kv_nvs_handle, key, value);
    if (err != ESP_OK) {
        return err;
    }
    return nvs_commit(kv_nvs_handle);
}

esp_err_t kv_get_uint8(const char *key, uint8_t *out_value, uint8_t default_value) {
    esp_err_t err = nvs_get_u8(kv_nvs_handle, key, out_value);
    if (err != ESP_OK) {
        *out_value = default_value;
    }
    return err;
}

esp_err_t kv_set_str(const char *key, const char *value) {
    esp_err_t err = nvs_set_str(kv_nvs_handle, key, value);
    if (err != ESP_OK) {
        return err;
    }
    return nvs_commit(kv_nvs_handle);
}

esp_err_t kv_get_str(const char *key, char *out_value, size_t max_size, const char *default_value) {
    esp_err_t err = nvs_get_str(kv_nvs_handle, key, out_value, &max_size);
    if (err != ESP_OK) {
        if (default_value != NULL) {
            strncpy(out_value, default_value, max_size);
            out_value[max_size - 1] = '\0'; // Ensure null-termination
        } else {
            out_value[0] = '\0'; // Set to empty string if no default provided
        }
    }
    return err;
}

esp_err_t kv_set_blob(const char *key, const void *value, size_t length) {
    esp_err_t err = nvs_set_blob(kv_nvs_handle, key, value, length);
    if (err != ESP_OK) {
        return err;
    }
    return nvs_commit(kv_nvs_handle);
}

esp_err_t kv_get_blob(const char *key, void *out_value, size_t max_size, const void *default_value, size_t default_length) {
    esp_err_t err = nvs_get_blob(kv_nvs_handle, key, out_value, &max_size);
    if (err != ESP_OK) {
        if (default_value != NULL && default_length <= max_size) {
            memcpy(out_value, default_value, default_length);
        } else {
            memset(out_value, 0, max_size); // Set to zero if no default provided or default is too large
        }
    }
    return err;
}

NuttyDriverKV nuttyDriverKV = {
    .initKV = kv_init,
    .setBlob = kv_set_blob,
    .getBlob = kv_get_blob,
    .setStr = kv_set_str,
    .getStr = kv_get_str,
    .setInt64 = kv_set_int64,
    .getInt64 = kv_get_int64,
    .setUInt64 = kv_set_uint64,
    .getUInt64 = kv_get_uint64,
    .setInt32 = kv_set_int32,
    .getInt32 = kv_get_int32,
    .setUInt32 = kv_set_uint32,
    .getUInt32 = kv_get_uint32,
    .setInt16 = kv_set_int16,
    .getInt16 = kv_get_int16,
    .setUInt16 = kv_set_uint16,
    .getUInt16 = kv_get_uint16,
    .setInt8 = kv_set_int8,
    .getInt8 = kv_get_int8,
    .setUInt8 = kv_set_uint8,
    .getUInt8 = kv_get_uint8
};
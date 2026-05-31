#include "NuttyELFLoader.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_elf.h"
#include "esp_log.h"

static const char *TAG = "NuttyELFLoader";

static bool g_initialized = false;

static esp_err_t nutty_elf_load_file(const char *path, uint8_t **out_buffer, size_t *out_size)
{
    FILE *file = NULL;
    uint8_t *buffer = NULL;
    long file_size = 0;
    size_t bytes_read = 0;

    if (!path || !out_buffer || !out_size)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *out_buffer = NULL;
    *out_size = 0;

    file = fopen(path, "rb");
    if (!file)
    {
        ESP_LOGE(TAG, "Failed to open %s: errno=%d", path, errno);
        return ESP_FAIL;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        ESP_LOGE(TAG, "Failed to seek end for %s", path);
        fclose(file);
        return ESP_FAIL;
    }

    file_size = ftell(file);
    if (file_size <= 0)
    {
        ESP_LOGE(TAG, "Invalid file size for %s: %ld", path, file_size);
        fclose(file);
        return ESP_FAIL;
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        ESP_LOGE(TAG, "Failed to rewind %s", path);
        fclose(file);
        return ESP_FAIL;
    }

    buffer = (uint8_t *)malloc((size_t)file_size);
    if (!buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate %ld bytes for %s", file_size, path);
        fclose(file);
        return ESP_ERR_NO_MEM;
    }

    bytes_read = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size)
    {
        ESP_LOGE(TAG, "Failed to read %s: expected=%ld read=%u", path, file_size, (unsigned)bytes_read);
        free(buffer);
        return ESP_FAIL;
    }

    *out_buffer = buffer;
    *out_size = (size_t)file_size;
    return ESP_OK;
}

esp_err_t NuttyELFLoader_Init(void)
{
    if (g_initialized)
    {
        return ESP_OK;
    }

    g_initialized = true;
    ESP_LOGI(TAG, "Initialized");
    return ESP_OK;
}

esp_err_t NuttyELFLoader_RegisterHostSymbols(void)
{
    return ESP_OK;
}

esp_err_t NuttyELFLoader_RunFromPath(const char *elf_path, int argc, char **argv)
{
    esp_err_t ret;
    esp_elf_t elf;
    uint8_t *elf_buffer = NULL;
    size_t elf_size = 0;

    if (!g_initialized)
    {
        ret = NuttyELFLoader_Init();
        if (ret != ESP_OK)
        {
            return ret;
        }
    }

    if (!elf_path || elf_path[0] == '\0')
    {
        return ESP_ERR_INVALID_ARG;
    }

    ret = nutty_elf_load_file(elf_path, &elf_buffer, &elf_size);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = esp_elf_init(&elf);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "esp_elf_init failed: %d", ret);
        free(elf_buffer);
        return ESP_FAIL;
    }

    ret = esp_elf_relocate(&elf, elf_buffer);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "esp_elf_relocate failed for %s: %d", elf_path, ret);
        esp_elf_deinit(&elf);
        free(elf_buffer);
        return ESP_FAIL;
    }

    ret = esp_elf_request(&elf, 0, argc, argv);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "esp_elf_request failed for %s: %d", elf_path, ret);
        esp_elf_deinit(&elf);
        free(elf_buffer);
        return ESP_FAIL;
    }

    esp_elf_deinit(&elf);
    free(elf_buffer);

    ESP_LOGI(TAG, "Loaded ELF: %s (%u bytes)", elf_path, (unsigned)elf_size);
    return ESP_OK;
}

esp_err_t NuttyELFLoader_Run(const NuttyELFLoaderOptions *options)
{
    if (!options)
    {
        return ESP_ERR_INVALID_ARG;
    }

    return NuttyELFLoader_RunFromPath(options->path, options->argc, options->argv);
}

void NuttyELFLoader_Deinit(void)
{
    g_initialized = false;
    ESP_LOGI(TAG, "Deinitialized");
}
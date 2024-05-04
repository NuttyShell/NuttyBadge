#include "sdcard.h"


static const char *TAG = "SD";

#define CHECK_EXECUTE_RESULT(err, str) do { \
    if ((err) !=ESP_OK) { \
        ESP_LOGE("SD", str" (0x%x).", err); \
        goto cleanup; \
    } \
    } while(0)
   static BYTE pdrv;                                  //Drive number that is mounted
   static  esp_vfs_fat_mount_config_t mount_config;    //Mount configuration
   static  FATFS *fs;                                  //FAT structure pointer that is registered
   static  sdmmc_card_t *card;                         //Card info


static esp_err_t partition_card(const esp_vfs_fat_mount_config_t *mount_config,
                                const char *drv, sdmmc_card_t *card, BYTE pdrv)
{
    FRESULT res = FR_OK;
    esp_err_t err;
    const size_t workbuf_size = 4096;
    void* workbuf = NULL;
    ESP_LOGW("TAG", "partitioning card");

    workbuf = ff_memalloc(workbuf_size);
    if (workbuf == NULL) {
        return ESP_ERR_NO_MEM;
    }

    LBA_t plist[] = {100, 0, 0, 0};
    res = f_fdisk(pdrv, plist, workbuf);
    if (res != FR_OK) {
        err = ESP_FAIL;
        ESP_LOGD("SD", "f_fdisk failed (%d)", res);
        goto fail;
    }
    size_t alloc_unit_size = esp_vfs_fat_get_allocation_unit_size(
                card->csd.sector_size,
                mount_config->allocation_unit_size);
    ESP_LOGW("SD", "formatting card, allocation unit size=%d", alloc_unit_size);
    const MKFS_PARM opt = {(BYTE)FM_ANY, 0, 0, 0, alloc_unit_size};
    res = f_mkfs(drv, &opt, workbuf, workbuf_size);
    if (res != FR_OK) {
        err = ESP_FAIL;
        ESP_LOGD("SD", "f_mkfs failed (%d)", res);
        goto fail;
    }

    free(workbuf);
    return ESP_OK;
fail:
    free(workbuf);
    return err;
}

static esp_err_t s_f_mount(sdmmc_card_t *card, FATFS *fs, const char *drv, uint8_t pdrv, const esp_vfs_fat_mount_config_t *mount_config)
{
    esp_err_t err = ESP_OK;
    FRESULT res = f_mount(fs, drv, 1);
    if (res != FR_OK) {
        err = ESP_FAIL;
        ESP_LOGW("SD", "failed to mount card (%d)", res);

        bool need_mount_again = (res == FR_NO_FILESYSTEM || res == FR_INT_ERR) && mount_config->format_if_mount_failed;
        if (!need_mount_again) {
            return ESP_FAIL;
        }

        err = partition_card(mount_config, drv, card, pdrv);
        if (err != ESP_OK) {
            return err;
        }

        ESP_LOGW("SD", "mounting again");
        res = f_mount(fs, drv, 0);
        if (res != FR_OK) {
            err = ESP_FAIL;
            ESP_LOGD("SD", "f_mount failed after formatting (%d)", res);
            return err;
        }
    }

    return ESP_OK;
}


static sdmmc_card_t *card = NULL;
static esp_err_t init_sd_card() {
    if (card == NULL) {
        card = (sdmmc_card_t*)malloc(sizeof(sdmmc_card_t));
    }
    assert(card != NULL);
    nuttyPeripherals.initSDCard(card);
    return ESP_OK;
}

static void print_sd_card_info() {
    if(card == NULL) {
        ESP_LOGE(TAG, "Card not yet initialized.");
        return;
    }
    sdmmc_card_print_info(stdout, card);
}

static char *sd_path = NULL;
static FATFS *fs = NULL;
static BYTE pdrv = FF_DRV_NOT_USED;
static char drv[3] = {'X', ':', 0};

static esp_err_t unmount_sd_card() {
    if(sd_path == NULL) return ESP_ERR_INVALID_STATE; // Not mounted
    if(card == NULL) return ESP_ERR_INVALID_STATE; // Card not initialized
    if(fs) {
        f_mount(NULL, drv, 0);
    }
    ESP_ERROR_CHECK(esp_vfs_fat_unregister_path(sd_path));
    if(pdrv != FF_DRV_NOT_USED) {
        ff_diskio_unregister(pdrv);
    }
    free(sd_path);
    //free(card); // unmount shouldn't remove the "card"
    sd_path = NULL;
    //card = NULL;
    ESP_LOGI(TAG, "Unmounted");
    return ESP_OK;
}

static bool is_sd_card_mounted() {
    if (sd_path == NULL || card == NULL || fs == NULL || pdrv == FF_DRV_NOT_USED) return false; // Not even mounted with a drv
    // Test SD Card really still there
    if (sdmmc_get_status(card) != ESP_OK) {
        unmount_sd_card();
        return false;
    };
    return true;
}

static esp_err_t mount_sd_card() {
    esp_err_t err;
    if(sd_path == NULL) {
        sd_path = (char *)malloc(sizeof(SDCARD_MOUNT_POINT));
    }
    assert(sd_path != NULL);
    strcpy(sd_path, SDCARD_MOUNT_POINT);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 0
    };

    if (ff_diskio_get_drive(&pdrv) != ESP_OK || pdrv == FF_DRV_NOT_USED) {
        ESP_LOGD(TAG, "the maximum count of volumes is already mounted");
        return ESP_ERR_NO_MEM;
    }

    ff_diskio_register_sdmmc(pdrv, card);
    ff_sdmmc_set_disk_status_check(pdrv, mount_config.disk_status_check_enable);
    drv[0] = (char)('0' + pdrv);
    // connect FATFS to VFS
    err = esp_vfs_fat_register(sd_path, drv, mount_config.max_files, &fs);
    ESP_LOGI(TAG, "pDrv: %02X; drv=%s", pdrv, drv);
    if (err == ESP_ERR_INVALID_STATE) {
        // it's okay, already registered with VFS
    } else if (err != ESP_OK) {
        ESP_LOGD(TAG, "esp_vfs_fat_register failed 0x(%x)", err);
        unmount_sd_card();
        return ESP_FAIL;
    }

    // Try to mount partition
    //err = s_f_mount(card, fs, drv, pdrv, &mount_config);

    FRESULT res = f_mount(fs, drv, 1);

    if (res == FR_NO_FILESYSTEM || res == FR_INT_ERR) {
        ESP_LOGW(TAG, "failed to mount card (%d)", res);
        unmount_sd_card();
        return ESP_ERR_NOT_FOUND;
    }

    if (res != FR_OK) {
        ESP_LOGW(TAG, "failed to mount card (%d)", res);
        unmount_sd_card();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Mounted");
    return ESP_OK;
}


// true = inserted; false = not inserted
static bool get_card_detect_inserted() {
    int res;
    nuttyDriverIR.txIRWaitFinishAndReserveLine();
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_IR_TX_SDCD, GPIO_MODE_INPUT));
    res = gpio_get_level(GPIO_IR_TX_SDCD);
    nuttyDriverIR.txIRReleaseLine();
    return (res == 0);
}

NuttyDriverSDCard nuttyDriverSDCard = {
    .initSDCard = init_sd_card,
    .printSDCardInfo = print_sd_card_info,
    .mountSDCard = mount_sd_card,
    .unmountSDCard = unmount_sd_card,
    .isSDCardMounted = is_sd_card_mounted,
    .getCardInserted = get_card_detect_inserted
};
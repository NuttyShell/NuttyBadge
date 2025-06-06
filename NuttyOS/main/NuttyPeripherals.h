#ifndef _NUTTYPERIPHERALS_H
#define _NUTTYPERIPHERALS_H

#include <esp_err.h>
#include <esp_check.h>
#include <string.h>
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "hal/adc_types.h"
#include "esp_vfs_fat.h"
#include "esp_vfs.h"
#include "vfs_fat_internal.h"
#include "diskio_impl.h"
#include "diskio_sdmmc.h"

#include "config.h"

typedef esp_err_t (*NuttyPeripheralsInitFunc)(void);
typedef esp_err_t (*NuttyPeripheralsInitGPIOISRFunc)(void (*)(void *), void *);
typedef void (*NuttyPeripheralsReadADCFunc)(int *, int *);
typedef esp_err_t (*NuttyPeripheralsInitSPIDeviceFunc)(spi_device_handle_t *, int, uint8_t, int, uint32_t, int, void (*)(spi_transaction_t *));
typedef esp_err_t (*NuttyPeripheralsInitRMTTxDeviceFunc)(rmt_channel_handle_t *rmt_channel, uint8_t gpio, size_t mem_blk_syms, uint32_t res_hz, rmt_carrier_config_t *);
typedef esp_err_t (*NuttyPeripheralsInitRMTRxDeviceFunc)(rmt_channel_handle_t *rmt_channel, uint8_t gpio, size_t mem_blk_syms, uint32_t res_hz, QueueHandle_t *rx_queue, rmt_rx_event_callbacks_t *rx_cb);
typedef esp_err_t (*NuttyPeripheralsInitSDCardFunc)(sdmmc_card_t *card);
typedef struct _NuttyPeripherals {
    NuttyPeripheralsInitFunc initGPIO;
    NuttyPeripheralsInitFunc initI2C;
    NuttyPeripheralsInitFunc initFSPI;
    NuttyPeripheralsInitSPIDeviceFunc initFSPIDevice;
    NuttyPeripheralsInitFunc initLEDC;
    NuttyPeripheralsInitRMTTxDeviceFunc initRMTTxDevice;
    NuttyPeripheralsInitRMTRxDeviceFunc initRMTRxDevice;
    NuttyPeripheralsInitGPIOISRFunc initGPIOISR;
    NuttyPeripheralsInitFunc initADC;
    NuttyPeripheralsReadADCFunc readADC;
    NuttyPeripheralsInitFunc initSDHost;
    NuttyPeripheralsInitSDCardFunc initSDCard;
} NuttyPeripherals;
extern NuttyPeripherals nuttyPeripherals;

#endif /* _NUTTYPERIPHERALS_H */
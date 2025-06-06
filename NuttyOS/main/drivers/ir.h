#ifndef _IR_H
#define _IR_H

#include "config.h"
#include <string.h>
#include <esp_err.h>
#include <esp_rom_gpio.h>
#include <esp_private/rmt.h>
#include <soc/rmt_periph.h>
#include "hal/rmt_types.h"
#include "NuttyPeripherals.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

typedef esp_err_t (*NuttyDriverIRTxInitFunc)();
typedef esp_err_t (*NuttyDriverIRRxInitFunc)();
typedef esp_err_t (*NuttyDriverIRTxNECXFunc)(uint16_t address, uint16_t command);
typedef esp_err_t (*NuttyDriverIRTxNECFunc)(uint8_t address, uint8_t command);
typedef void (*NuttyDriverIRTxWaitFinishAndReserveLineFunc)();
typedef void (*NuttyDriverIRTxReleaseLineFunc)();
typedef void (*NuttyDriverIRRxNECWaitFunc)(uint16_t *address, uint16_t *command, uint8_t *status, uint16_t ms_to_wait);
typedef struct _NuttyDriverIR {
    NuttyDriverIRTxInitFunc initIRTx;
    NuttyDriverIRRxInitFunc initIRRx;
    NuttyDriverIRTxNECXFunc txIRNECext;
    NuttyDriverIRTxNECFunc txIRNEC;
    NuttyDriverIRTxWaitFinishAndReserveLineFunc txIRWaitFinishAndReserveLine;
    NuttyDriverIRTxReleaseLineFunc txIRReleaseLine;
    NuttyDriverIRRxNECWaitFunc waitForIRNECRx;
} NuttyDriverIR;
extern NuttyDriverIR nuttyDriverIR;

/**
 * @brief NEC timing spec
 */
#define NEC_LEADING_CODE_DURATION_0  9000
#define NEC_LEADING_CODE_DURATION_1  4500
#define NEC_PAYLOAD_ZERO_DURATION_0  560
#define NEC_PAYLOAD_ZERO_DURATION_1  560
#define NEC_PAYLOAD_ONE_DURATION_0   560
#define NEC_PAYLOAD_ONE_DURATION_1   1690
#define NEC_REPEAT_CODE_DURATION_0   9000
#define NEC_REPEAT_CODE_DURATION_1   2250

#endif /* _IR_H */
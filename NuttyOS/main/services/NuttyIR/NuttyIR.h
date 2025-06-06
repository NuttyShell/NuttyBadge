#ifndef _NUTTYIR_H
#define _NUTTYIR_H

#include "NuttyPeripherals.h"
#include "drivers/ir.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>

esp_err_t NuttyIR_Init();
esp_err_t NuttyIR_Deinit();
void NuttyIR_TxNEC(uint16_t _addr, uint16_t _cmd);
void NuttyIR_TxNECext(uint16_t _addr, uint16_t _cmd);
void NuttyIR_getLatestRecvResult(uint16_t *_addr, uint16_t *_cmd, uint8_t *_status);

#endif /* _NUTTYIR_H */
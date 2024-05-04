#ifndef _NUTTYIR_H
#define _NUTTYIR_H

#include "NuttyPeripherals.h"
#include "drivers/ir.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>

esp_err_t NuttyIR_Init();
void NuttyIR_Replay();
//void NuttyIR_waitForValidIRSignal();

#endif /* _NUTTYIR_H */
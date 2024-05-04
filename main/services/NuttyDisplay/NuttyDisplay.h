#ifndef _NUTTYDISPLAY_H
#define _NUTTYDISPLAY_H

#include "NuttyPeripherals.h"
#include "drivers/lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>

esp_err_t NuttyDisplay_Init();

#endif /* _NUTTYDISPLAY_H */
#ifndef _NUTTYRGB_H
#define _NUTTYRGB_H

#include "NuttyPeripherals.h"
#include "drivers/rgb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>

esp_err_t NuttyRGB_Init();

#endif /* _NUTTYRGB_H */
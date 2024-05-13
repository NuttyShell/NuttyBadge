#ifndef _NUTTYDISPLAY_H
#define _NUTTYDISPLAY_H

#include "NuttyPeripherals.h"
#include "drivers/lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include <esp_timer.h>

#include <lvgl.h>

esp_err_t NuttyDisplay_Init();
void NuttyDisplay_lockLVGL();
void NuttyDisplay_unlockLVGL();
lv_obj_t* NuttyDisplay_getUserAppArea();
void NuttyDisplay_clearUserAppArea();
void NuttyDisplay_setLCDBacklight(uint8_t percentage);
void NuttyDisplay_showPNG(uint8_t *pngData, size_t pngSz);

#endif /* _NUTTYDISPLAY_H */
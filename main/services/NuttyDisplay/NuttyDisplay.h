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
#include "lvgl_fonts/cg_pixel_4x5_mono.h"

esp_err_t NuttyDisplay_Init();
void NuttyDisplay_lockLVGL();
void NuttyDisplay_unlockLVGL();

uint16_t NuttyDisplay_getUserAppAreaWidth();
uint16_t NuttyDisplay_getUserAppAreaHeight();
lv_obj_t* NuttyDisplay_getUserAppArea();
lv_obj_t* NuttyDisplay_getSystemTrayArea();
lv_obj_t* NuttyDisplay_showPNGWithWHXY(lv_img_dsc_t* lvgl_png_img_wh, lv_obj_t* drawArea, uint8_t x, uint8_t y);
lv_img_dsc_t NuttyDisplay_getPNGDsc(uint8_t *pngData, size_t pngSz);
void NuttyDisplay_clearUserAppArea();
void NuttyDisplay_setLCDBacklight(uint8_t percentage);
void NuttyDisplay_showPNG(uint8_t *pngData, size_t pngSz);
void NuttyDisplay_clearWholeScreen();
void print_lcd_frame_buffer();
void NuttyDisplay_clearSystemTrayArea();

#endif /* _NUTTYDISPLAY_H */
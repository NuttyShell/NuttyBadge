#ifndef _NUTTYRGB_H
#define _NUTTYRGB_H

#include "NuttyPeripherals.h"
#include "drivers/rgb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>


typedef struct _NuttyRGBAnimationSequence {
    uint8_t r[RGB_BULBS];
    uint8_t g[RGB_BULBS];
    uint8_t b[RGB_BULBS];
    uint16_t durationMs;
} NuttyRGBAnimationSequence;

esp_err_t NuttyRGB_Init();
esp_err_t NuttyRGB_Deinit();
void NuttyRGB_SetRGBWithoutDisplay(uint8_t bulb, uint8_t r, uint8_t g, uint8_t b);
void NuttyRGB_SetRGBAndDisplay(uint8_t bulb, uint8_t r, uint8_t g, uint8_t b);
void NuttyRGB_SetHSVWithoutDisplay(uint8_t bulb, uint8_t h, uint8_t s, uint8_t v);
void NuttyRGB_SetHSVAndDisplay(uint8_t bulb, uint8_t h, uint8_t s, uint8_t v);
void NuttyRGB_SetAnimationSequences(NuttyRGBAnimationSequence *animSeq, uint16_t size);
void NuttyRGB_StartAnimation();
void NuttyRGB_StopAnimation();
void NuttyRGB_SetAllRGBWithoutDisplay(uint8_t r, uint8_t g, uint8_t b);
void NuttyRGB_SetAllRGBAndDisplay(uint8_t r, uint8_t g, uint8_t b);
void NuttyRGB_DisplayNow();

#endif /* _NUTTYRGB_H */
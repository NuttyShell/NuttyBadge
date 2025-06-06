#ifndef _RGB_H
#define _RGB_H

#include "config.h"
#include <string.h>
#include <esp_err.h>
#include "NuttyPeripherals.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

typedef esp_err_t (*NuttyDriverRGBInitFunc)(uint8_t);
typedef esp_err_t (*NuttyDriverRGBDisplayFunc)();
typedef esp_err_t (*NuttyDriverRGBSetWithoutDisplayFunc)(uint8_t, uint8_t, uint8_t, uint8_t);
typedef esp_err_t (*NuttyDriverRGBSetAndDisplayFunc)(uint8_t, uint8_t, uint8_t, uint8_t);
typedef struct _NuttyDriverRGB {
    NuttyDriverRGBInitFunc initRGB;
    NuttyDriverRGBSetWithoutDisplayFunc setRGBWithoutDisplay;
    NuttyDriverRGBSetWithoutDisplayFunc setHSVWithoutDisplay;
    NuttyDriverRGBSetAndDisplayFunc setRGBAndDisplay;
    NuttyDriverRGBSetAndDisplayFunc setHSVAndDisplay;
    NuttyDriverRGBDisplayFunc displayNow;
} NuttyDriverRGB;
extern NuttyDriverRGB nuttyDriverRGB;

#endif /* _RGB_H */
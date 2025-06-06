#ifndef _LCD_H
#define _LCD_H

#include "config.h"
#include <string.h>
#include <esp_err.h>
#include "NuttyPeripherals.h"
#include "drivers/ioe.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

typedef esp_err_t (*NuttyDriverLCDInitFunc)(void);
typedef void (*NuttyDriverLCDUpdatePageFunc)(uint8_t page, uint8_t *data, size_t datasz);
typedef void (*NuttyDriverLCDSetBacklightDutyCycleFunc)(uint32_t duty);
typedef struct _NuttyDriverLCD {
    NuttyDriverLCDInitFunc initLCD;
    NuttyDriverLCDUpdatePageFunc updatePage;
    NuttyDriverLCDUpdatePageFunc updatePageSeqOrder;
    NuttyDriverLCDSetBacklightDutyCycleFunc setBacklightDutyCycle;
} NuttyDriverLCD;
extern NuttyDriverLCD nuttyDriverLCD;

#endif /* _LCD_H */
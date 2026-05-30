#ifndef _NUTTYSNAKE_H
#define _NUTTYSNAKE_H

#include "NuttyPeripherals.h"
#include "drivers/lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include <esp_timer.h>

#include "services/NuttyApps/NuttyApps.h"
#include "services/NuttyDisplay/NuttyDisplay.h"
#include "services/NuttyInput/NuttyInput.h"
#include "services/NuttySystemMonitor/NuttySystemMonitor.h"

typedef struct _NuttySnakeConfig {
    uint8_t difficulty;
    uint16_t speed;
} _NuttySnakeConfig;

typedef struct _NuttySnakeQueue {
    struct _NuttySnakeQueue *next;
    lv_obj_t *current;
    uint8_t X;
    uint8_t Y;
} _NuttySnakeQueue;

enum _NuttySnakeDIRECTION{
    UP, DOWN, LEFT, RIGHT
};

extern NuttyAppDefinition NuttySnake;

#endif /* _NUTTYSNAKE_H */

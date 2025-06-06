#ifndef _IOE_H
#define _IOE_H

#include "config.h"
#include <esp_err.h>
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

extern EventGroupHandle_t ioe_int_evt_grp;

typedef esp_err_t (*NuttyDriverIOEInitFunc)(void);
typedef esp_err_t (*NuttyDriverIOEWriteFunc)(uint8_t);
typedef esp_err_t (*NuttyDriverIOEReadFunc)(uint8_t *);
typedef void (*NuttyDriverIOEAccquireFunc)(void);
typedef void (*NuttyDriverIOEReleaseFunc)(void);
typedef uint8_t (*NuttyDriverIOEGetOutputStateFunc)(void);
typedef struct _NuttyDriverIOE {
    NuttyDriverIOEInitFunc initIOE;
    NuttyDriverIOEWriteFunc writeIOE;
    NuttyDriverIOEReadFunc readIOE;
    NuttyDriverIOEAccquireFunc lockIOE;
    NuttyDriverIOEReleaseFunc releaseIOE;
    NuttyDriverIOEGetOutputStateFunc getIOEOutputState;
} NuttyDriverIOE;
extern NuttyDriverIOE nuttyDriverIOE;

#endif /* _IOE_H */
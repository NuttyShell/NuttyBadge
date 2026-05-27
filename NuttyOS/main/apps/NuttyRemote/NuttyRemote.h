#ifndef _NUTTYREMOTE_H
#define _NUTTYREMOTE_H

#include "NuttyPeripherals.h"
#include "drivers/lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <stdio.h>

#include "services/NuttyApps/NuttyApps.h"
#include "services/NuttyDisplay/NuttyDisplay.h"
#include "services/NuttyInput/NuttyInput.h"
#include "services/NuttyIR/NuttyIR.h"
#include "services/NuttySystemMonitor/NuttySystemMonitor.h"
#include "services/NuttyStorage/NuttyStorage.h"

typedef struct _IRDBParsedRemoteData {
    char protocolName[16];
    uint8_t addr[4];
    uint8_t cmd[4];
} IRDBParsedRemoteData;

typedef struct _IRDBRawRemoteData {
    uint32_t frequency;
    double duty;
    uint32_t *data;
    size_t dataSz;
} IRDBRawRemoteData;

extern NuttyAppDefinition NuttyRemote;

#endif /* _NUTTYREMOTE_H */

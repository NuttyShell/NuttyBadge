#ifndef _NUTTYAPPS_H
#define _NUTTYAPPS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_system.h>
#include <esp_log.h>
#include <stdlib.h>

typedef void (*NuttyAppEntryPoint)(void);
typedef struct _NuttyAppDefinition {
    char *appName;
    NuttyAppEntryPoint appMainEntry;
    bool appHidden;
} NuttyAppDefinition;


void NuttyApps_Init();

void NuttyApps_registerApp(NuttyAppDefinition app);
uint8_t NuttyApps_getTotalNumberOfApps();
NuttyAppDefinition NuttyApps_getAppByIndex(uint8_t i);
void NuttyApps_launchAppByDef(NuttyAppDefinition def);
void NuttyApps_launchAppByIndex(uint8_t i);
void NuttyApps_printApps();

#endif /* _NUTTYAPPS_H */
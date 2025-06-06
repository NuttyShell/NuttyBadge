#ifndef _NUTTYAPPS_H
#define _NUTTYAPPS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_system.h>
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

typedef void (*NuttyAppEntryPoint)(void);
typedef void (*NuttyAppEntryPointWithParams)(uint8_t params_count, void** params);
typedef struct _NuttyAppDefinition {
    char *appName;
    NuttyAppEntryPoint appMainEntry;
    NuttyAppEntryPointWithParams appMainEntryWithParams;
    bool appHidden;
} NuttyAppDefinition;

void NuttyApps_Init();

void NuttyApps_registerApp(NuttyAppDefinition app);
void NuttyApps_registerParamedApp(NuttyAppDefinition app);
uint8_t NuttyApps_getTotalNumberOfApps();
NuttyAppDefinition NuttyApps_getAppByIndex(uint8_t i);
void NuttyApps_launchAppByIndex(uint8_t i);
void NuttyApps_launchAppByEntry(NuttyAppEntryPoint entryPoint);
uint8_t NuttyApps_getAppIndexByName(char *name);
void NuttyApps_launchAppByName(char *name);
void NuttyApps_launchParamedAppByName(char *name, uint8_t param_count, void **params);
void NuttyApps_launchParamedAppByIndex(uint8_t i, uint8_t param_count, void **params);
void NuttyApps_launchParamedAppByEntry(NuttyAppEntryPointWithParams entryPoint, uint8_t param_count, void **params);

uint8_t NuttyApps_isParamedAppByIndex(uint8_t i);
void NuttyApps_printApps();

#endif /* _NUTTYAPPS_H */
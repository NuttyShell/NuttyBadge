#include "NuttyApps.h"

static const char *TAG = "NuttyApps";
static NuttyAppDefinition *nuttyApps = NULL;
static uint8_t nuttyAppsLength=5;
static uint8_t totalApps=0;

typedef struct _ParamedAppRunnerParam {
    NuttyAppEntryPointWithParams entrypoint;
    uint8_t param_count;
    void **params;
} ParamedAppRunnerParam;

void NuttyApps_Init() {
    ESP_LOGI(TAG, "Init");
    nuttyApps = malloc(sizeof(NuttyAppDefinition) * nuttyAppsLength);
    assert(nuttyApps != NULL);
}

void NuttyApps_registerApp(NuttyAppDefinition app) {
    app.appMainEntryWithParams=NULL;
    if(nuttyAppsLength <= totalApps) {
        nuttyAppsLength += 5;
        nuttyApps = realloc(nuttyApps, sizeof(NuttyAppDefinition) * nuttyAppsLength);
    }
    assert(nuttyApps != NULL);
    nuttyApps[totalApps] = app;
    totalApps++;
}

void NuttyApps_registerParamedApp(NuttyAppDefinition app) {
    app.appMainEntry=NULL;
    if(nuttyAppsLength <= totalApps) {
        nuttyAppsLength += 5;
        nuttyApps = realloc(nuttyApps, sizeof(NuttyAppDefinition) * nuttyAppsLength);
    }
    assert(nuttyApps != NULL);
    nuttyApps[totalApps] = app;
    totalApps++;
}

uint8_t NuttyApps_getTotalNumberOfApps() {
    return totalApps;
}

NuttyAppDefinition NuttyApps_getAppByIndex(uint8_t i) {
    return nuttyApps[i];
}

static void NuttyApps_AppRunner(void *entryPoint) {
    ((NuttyAppEntryPoint)entryPoint)();
    vTaskDelete(NULL); // This task ends.
}

static void NuttyApps_ParamedAppRunner(void *_appRunnerParam) {
    ParamedAppRunnerParam *appRunnerParam = (ParamedAppRunnerParam *)_appRunnerParam;
    ((NuttyAppEntryPointWithParams)appRunnerParam->entrypoint)(appRunnerParam->param_count, appRunnerParam->params);
    free(appRunnerParam);
    vTaskDelete(NULL); // This task ends.
}

void NuttyApps_launchAppByIndex(uint8_t i) {
    TaskHandle_t taskHandle;
    ESP_LOGI(TAG, "Starting App Id=%d", i);
    //xTaskCreate(NuttyApps_AppRunner, nuttyApps[i].appName, 10240, (void *)nuttyApps[i].appMainEntry, 10, &taskHandle);
    xTaskCreatePinnedToCore(NuttyApps_AppRunner, nuttyApps[i].appName, 10240, (void *)nuttyApps[i].appMainEntry, 10, &taskHandle, 1);
}

void NuttyApps_launchParamedAppByIndex(uint8_t i, uint8_t param_count, void **params) {
    TaskHandle_t taskHandle;
    ESP_LOGI(TAG, "Starting Paramed App Id=%d", i);
    //xTaskCreate(NuttyApps_AppRunner, nuttyApps[i].appName, 10240, (void *)nuttyApps[i].appMainEntry, 10, &taskHandle);
    ParamedAppRunnerParam *appRunnerParam = malloc(sizeof(ParamedAppRunnerParam));
    assert(appRunnerParam != NULL);
    appRunnerParam->entrypoint = nuttyApps[i].appMainEntryWithParams;
    appRunnerParam->param_count = param_count;
    appRunnerParam->params = params;
    xTaskCreatePinnedToCore(NuttyApps_ParamedAppRunner, nuttyApps[i].appName, 10240, (void *)appRunnerParam, 10, &taskHandle, 1);
}

void NuttyApps_launchAppByEntry(NuttyAppEntryPoint entryPoint) {
    for(uint8_t i=0; i<totalApps; i++) {
        if(nuttyApps[i].appMainEntry == entryPoint) {
            return NuttyApps_launchAppByIndex(i);
        }
    }
}

uint8_t NuttyApps_getAppIndexByName(char *name) {
    for(uint8_t i=0; i<totalApps; i++) {
        if(strcmp(nuttyApps[i].appName, name) == 0) {
            return i;
        }
    }
    return 0;
}

void NuttyApps_launchAppByName(char *name) {
    return NuttyApps_launchAppByIndex(NuttyApps_getAppIndexByName(name));
}

void NuttyApps_launchParamedAppByName(char *name, uint8_t param_count, void **params) {
    return NuttyApps_launchParamedAppByIndex(NuttyApps_getAppIndexByName(name), param_count, params);
}

void NuttyApps_launchParamedAppByEntry(NuttyAppEntryPointWithParams entryPoint, uint8_t param_count, void **params) {
    for(uint8_t i=0; i<totalApps; i++) {
        if(nuttyApps[i].appMainEntryWithParams == entryPoint) {
            return NuttyApps_launchParamedAppByIndex(i, param_count, params);
        }
    }
}

uint8_t NuttyApps_isParamedAppByIndex(uint8_t i) {
    int8_t isParamedApp = -1;
    if(nuttyApps[i].appMainEntry != NULL && nuttyApps[i].appMainEntryWithParams == NULL) isParamedApp = 0;
    if(nuttyApps[i].appMainEntry == NULL && nuttyApps[i].appMainEntryWithParams != NULL) isParamedApp = 1;
    assert(isParamedApp != -1);
    return isParamedApp;
}

void NuttyApps_printApps() {
    uint8_t i;
    assert(nuttyApps != NULL);
    for(i=0; i<totalApps; i++) {
        if(NuttyApps_isParamedAppByIndex(i)) {
            ESP_LOGI(TAG, "App[%d]: %s, paramed_app=%s, main_ptr=%p", i, nuttyApps[i].appName, "true", (void *)nuttyApps[i].appMainEntryWithParams);
        }else{
            ESP_LOGI(TAG, "App[%d]: %s, paramed_app=%s, main_ptr=%p", i, nuttyApps[i].appName, "false", (void *)nuttyApps[i].appMainEntry);
        }
        
    }
}
#include "NuttyApps.h"

static const char *TAG = "NuttyApps";
static NuttyAppDefinition *nuttyApps = NULL;
static uint8_t nuttyAppsLength=5;
static uint8_t totalApps=0;

void NuttyApps_Init() {
    ESP_LOGI(TAG, "Init");
    nuttyApps = malloc(sizeof(NuttyAppDefinition) * nuttyAppsLength);
    assert(nuttyApps != NULL);
}

void NuttyApps_registerApp(NuttyAppDefinition app) {
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


void NuttyApps_launchAppByIndex(uint8_t i) {
    TaskHandle_t taskHandle;
    xTaskCreate(NuttyApps_AppRunner, nuttyApps[i].appName, 10240, (void *)nuttyApps[i].appMainEntry, 10, &taskHandle);
}

void NuttyApps_printApps() {
    uint8_t i;
    assert(nuttyApps != NULL);
    for(i=0; i<totalApps; i++) {
        ESP_LOGI(TAG, "App[%d]: %s, main_ptr=%p", i, nuttyApps[i].appName, (void *)nuttyApps[i].appMainEntry);
    }
}
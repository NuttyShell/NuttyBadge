#ifndef _NUTTYSYSTEMMONITOR_H
#define _NUTTYSYSTEMMONITOR_H

#include "NuttyPeripherals.h"
#include "drivers/ioe.h"
#include "drivers/sdcard.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "services/NuttyDisplay/NuttyDisplay.h"
#include <esp_log.h>

esp_err_t NuttySystemMonitor_Init();
int NuttySystemMonitor_getBatteryVoltage();
bool NuttySystemMonitor_isLowBattery();
bool NuttySystemMonitor_isVBUSConnected();
bool NuttySystemMonitor_isSDCardInserted();
bool NuttySystemMonitor_isSDCardMounted();
void NuttySystemMonitor_hideSystemTray();
void NuttySystemMonitor_showSystemTray();
void NuttySystemMonitor_setSystemTrayTempText(char *text, uint8_t durationSecond);

#endif /* _NUTTYSYSTEMMONITOR_H */
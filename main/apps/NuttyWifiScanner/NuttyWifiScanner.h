#ifndef _NUTTYWIFISCANNER_H
#define _NUTTYWIFISCANNER_H

#include "NuttyPeripherals.h"
#include "drivers/lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include <esp_timer.h>

#include "services/NuttyApps/NuttyApps.h"


extern NuttyAppDefinition NuttyWifiScanner;

#endif /* _NUTTYWIFISCANNER_H */

#ifndef _NUTTYDOOM_H
#define _NUTTYDOOM_H

#include "NuttyPeripherals.h"
#include "drivers/lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include <esp_timer.h>

#include "services/NuttyApps/NuttyApps.h"
#include "services/NuttyInput/NuttyInput.h"
#include "services/NuttyDisplay/NuttyDisplay.h"
#include "services/NuttyStorage/NuttyStorage.h"
#include "services/NuttySystemMonitor/NuttySystemMonitor.h"

#include "doomgeneric/doomgeneric.h"


extern NuttyAppDefinition NuttyDOOM;

#endif /* _NUTTYDOOM_H */

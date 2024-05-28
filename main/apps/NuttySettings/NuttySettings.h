#ifndef _NUTTYSETTINGS_H
#define _NUTTYSETTINGS_H

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
#include "services/NuttyIR/NuttyIR.h"
#include "services/NuttySystemMonitor/NuttySystemMonitor.h"


extern NuttyAppDefinition NuttySettings;

#endif /* _NUTTYSETTINGS_H */

#ifndef _NUTTYBTREMOTE_H
#define _NUTTYBTREMOTE_H

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
#include "services/NuttySystemMonitor/NuttySystemMonitor.h"
#include "services/NuttyStorage/NuttyStorage.h"


extern NuttyAppDefinition NuttyBTRemote;

#endif /* _NUTTYBTREMOTE_H */

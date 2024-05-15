#ifndef _NUTTYREMOTE_H
#define _NUTTYREMOTE_H

#include "NuttyPeripherals.h"
#include "drivers/lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include <esp_timer.h>

#include "services/NuttyApps/NuttyApps.h"


extern NuttyAppDefinition NuttyRemote;

#endif /* _NUTTYREMOTE_H */

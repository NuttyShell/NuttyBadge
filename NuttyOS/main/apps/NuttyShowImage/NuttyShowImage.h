#ifndef _NUTTYSHOWIMAGE_H
#define _NUTTYSHOWIMAGE_H

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


extern NuttyAppDefinition NuttyShowImage;

#endif /* _NUTTYSHOWIMAGE_H */

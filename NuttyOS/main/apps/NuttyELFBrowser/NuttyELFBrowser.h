#ifndef _NUTTYELFBROWSER_H
#define _NUTTYELFBROWSER_H

#include "NuttyPeripherals.h"
#include "drivers/lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <stdio.h>

#include "services/NuttyApps/NuttyApps.h"
#include "services/NuttyDisplay/NuttyDisplay.h"
#include "services/NuttyELFLoader/NuttyELFLoader.h"
#include "services/NuttyInput/NuttyInput.h"
#include "services/NuttyStorage/NuttyStorage.h"
#include "services/NuttySystemMonitor/NuttySystemMonitor.h"

extern NuttyAppDefinition NuttyELFBrowser;

#endif /* _NUTTYELFBROWSER_H */
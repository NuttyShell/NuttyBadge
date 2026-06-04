#ifndef _NUTNUTREVOLUTION_H
#define _NUTNUTREVOLUTION_H

#include "NuttyPeripherals.h"
#include "drivers/lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <stdlib.h>
#include <string.h>

#include "services/NuttyApps/NuttyApps.h"
#include "services/NuttyDisplay/NuttyDisplay.h"
#include "services/NuttyInput/NuttyInput.h"
#include "services/NuttySystemMonitor/NuttySystemMonitor.h"
#include "lvgl_fonts/cg_pixel_4x5_mono.h"

extern NuttyAppDefinition NutNutRevolution;

#endif /* _NUTNUTREVOLUTION_H */

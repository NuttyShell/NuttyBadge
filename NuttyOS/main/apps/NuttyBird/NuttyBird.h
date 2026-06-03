#ifndef _NUTTYBIRD_H
#define _NUTTYBIRD_H

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
#include "lvgl_fonts/cg_pixel_4x5_mono.h"

extern NuttyAppDefinition NuttyBird;

#endif /* _NUTTYBIRD_H */

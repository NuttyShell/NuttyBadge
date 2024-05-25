#ifndef _NUTTYABOUT_H
#define _NUTTYABOUT_H

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

#include "BuildDefs.h"

extern NuttyAppDefinition NuttyAbout;

#endif /* _NUTTYABOUT_H */

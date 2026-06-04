#ifndef _NUTTYBTPRESENTER_H
#define _NUTTYBTPRESENTER_H

#include "NuttyPeripherals.h"
#include "drivers/lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include <esp_log.h>
#include <esp_timer.h>

#include "services/NuttyApps/NuttyApps.h"
#include "lvgl_fonts/cg_pixel_4x5_mono.h"

extern NuttyAppDefinition NuttyBTPresenter;

#endif /* _NUTTYBTPRESENTER_H */

#ifndef _NUTTYMENU_H
#define _NUTTYMENU_H

#include "NuttyPeripherals.h"
#include "drivers/lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include <esp_timer.h>

#include "services/NuttyApps/NuttyApps.h"
#include "services/NuttyDisplay/NuttyDisplay.h"

extern NuttyAppDefinition NuttyMenu;

#endif /* _NUTTYMENU_H */
#ifndef _NUTTYSHOOTER_H
#define _NUTTYSHOOTER_H

#include "NuttyPeripherals.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>

/* Include headers for all the services the application will use */
#include "services/NuttyApps/NuttyApps.h"
#include "services/NuttyDisplay/NuttyDisplay.h"
#include "services/NuttyInput/NuttyInput.h"
#include "services/NuttyIR/NuttyIR.h"
#include "services/NuttyRGB/NuttyRGB.h"
#include "services/NuttySystemMonitor/NuttySystemMonitor.h"


/*
 * Declare the NuttyAppDefinition for the Shooter Game.
 * This makes the app visible to the application manager.
 */
extern NuttyAppDefinition NuttyShooter;

#endif /* _NUTTYSHOOTER_H */
#ifndef _NUTTYMINESWEEPER_H
#define _NUTTYMINESWEEPER_H

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

// Cleaned: Removed unused 'seed' member
typedef struct _NuttyMinesweeperConfig {
    uint8_t width;
    uint8_t height;
    uint8_t mines;
    uint8_t difficulty;
} _NuttyMinesweeperConfig;

typedef struct _MineCell {
    bool is_mine;
    bool is_revealed;
    bool is_flagged;
    uint8_t adjacent_mines;
} _MineCell;

// Cleaned: Removed unused _NuttyMinesweeperState enum
// Cleaned: Removed all unused PNG data arrays

extern NuttyAppDefinition NuttyMinesweeper;

#endif /* _NUTTYMINESWEEPER_H */
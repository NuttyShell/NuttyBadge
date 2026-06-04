// doomgeneric for NuttyBadge (ESP32-S3)
// Uses NuttyInput, LCD driver (FSPI), and esp_timer instead of SDL

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"
#include "net_client.h"

// Stub definitions for networking symbols (not used in single-player mode)
boolean drone = false;
boolean net_client_connected = false;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include <esp_timer.h>

#include "NuttyPeripherals.h"
#include "drivers/lcd.h"
#include "services/NuttyInput/NuttyInput.h"
#include "services/NuttySystemMonitor/NuttySystemMonitor.h"

static const char *TAG = "NuttyDOOM";

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int   s_KeyQueueWriteIndex = 0;
static unsigned int   s_KeyQueueReadIndex  = 0;

// Flag set when DOOM should quit (via menu or force-quit combo)
volatile bool nuttydoom_quit_requested = false;

// ---------------------------------------------------------------------------
// Key mapping: NuttyBadge buttons -> DOOM keycodes
// ---------------------------------------------------------------------------
static unsigned char mapNuttyToDoomKey(uint16_t btn)
{
    switch (btn)
    {
    case NUTTYINPUT_BTN_UP:     return KEY_UPARROW;
    case NUTTYINPUT_BTN_DOWN:   return KEY_DOWNARROW;
    case NUTTYINPUT_BTN_LEFT:   return KEY_LEFTARROW;
    case NUTTYINPUT_BTN_RIGHT:  return KEY_RIGHTARROW;
    case NUTTYINPUT_BTN_A:      return KEY_FIRE;       // shoot
    case NUTTYINPUT_BTN_B:      return KEY_USE;         // open / use
    case NUTTYINPUT_BTN_START:  return KEY_ENTER;       // menu select
    case NUTTYINPUT_BTN_SELECT: return KEY_ESCAPE;      // back / menu
    case NUTTYINPUT_BTN_USRDEF: return KEY_RSHIFT;      // run
    default:                    return 0;
    }
}

// ---------------------------------------------------------------------------
// Key queue
// ---------------------------------------------------------------------------
static void addKeyToQueue(int pressed, unsigned char doomKey)
{
    unsigned short keyData = (pressed << 8) | doomKey;
    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

// Poll the IO expander directly and push button events into the key queue.
// Also detect START+SELECT held together as a force-quit signal.
static void pollNuttyInput(void)
{
    static uint16_t prev_buttons = 0;
    uint16_t current_buttons = 0;

    // Read raw IO expander state (active-low)
    uint8_t ioeVal = 0;
    nuttyDriverIOE.lockIOE();
    nuttyDriverIOE.readIOE(&ioeVal);
    nuttyDriverIOE.releaseIOE();

    // Map active-low bits to NuttyInput button constants
    if (!(ioeVal & 0x01)) current_buttons |= NUTTYINPUT_BTN_UP;
    if (!(ioeVal & 0x02)) current_buttons |= NUTTYINPUT_BTN_DOWN;
    if (!(ioeVal & 0x04)) current_buttons |= NUTTYINPUT_BTN_LEFT;
    if (!(ioeVal & 0x08)) current_buttons |= NUTTYINPUT_BTN_RIGHT;
    if (!(ioeVal & 0x10)) current_buttons |= NUTTYINPUT_BTN_A;
    if (!(ioeVal & 0x20)) current_buttons |= NUTTYINPUT_BTN_B;
    if (!(ioeVal & 0x40)) current_buttons |= NUTTYINPUT_BTN_SELECT;
    if (!(ioeVal & 0x80)) current_buttons |= NUTTYINPUT_BTN_START;

    // Edge detection
    uint16_t pressed  = current_buttons & ~prev_buttons;
    uint16_t released = prev_buttons & ~current_buttons;

    // Push pressed events
    uint16_t mask;
    for (mask = 1; mask <= NUTTYINPUT_BTN_USRDEF; mask <<= 1)
    {
        if (pressed & mask)
        {
            unsigned char k = mapNuttyToDoomKey(mask);
            if (k) addKeyToQueue(1, k);
        }
    }

    // Push released events
    for (mask = 1; mask <= NUTTYINPUT_BTN_USRDEF; mask <<= 1)
    {
        if (released & mask)
        {
            unsigned char k = mapNuttyToDoomKey(mask);
            if (k) addKeyToQueue(0, k);
        }
    }

    // Force-quit: START + SELECT held together
    if ((current_buttons & (NUTTYINPUT_BTN_START | NUTTYINPUT_BTN_SELECT))
         == (NUTTYINPUT_BTN_START | NUTTYINPUT_BTN_SELECT))
    {
        nuttydoom_quit_requested = true;
    }

    prev_buttons = current_buttons;
}

// ---------------------------------------------------------------------------
// DG_* platform callbacks required by doomgeneric
// ---------------------------------------------------------------------------

void DG_Init(void)
{
    ESP_LOGI(TAG, "DG_Init: DOOM screen %dx%d", DOOMGENERIC_RESX, DOOMGENERIC_RESY);
    NuttySystemMonitor_hideSystemTray();
    NuttyDisplay_clearWholeScreen();
}

void DG_DrawFrame(void)
{
    // ---- 1. Convert DG_ScreenBuffer (32bpp) to LCD monochrome pages ----
    // LCD: 128x64 = 8 pages x 128 bytes, each byte = 8 vertical pixels.
    // DG_ScreenBuffer: 128x64 uint32_t (0x00FFFFFF = white, 0x00000000 = black).

    static uint8_t lcd_fb[8][128];

    memset(lcd_fb, 0, sizeof(lcd_fb));

    pixel_t *src = DG_ScreenBuffer;

    for (int y = 0; y < DOOMGENERIC_RESY; y++)
    {
        int page     = y >> 3;       // y / 8
        int bit      = y & 7;        // y % 8
        uint8_t mask = 1 << bit;     // bit 0 = top of page

        for (int x = 0; x < DOOMGENERIC_RESX; x++)
        {
            pixel_t p = src[y * DOOMGENERIC_RESX + x];

            // Extract RGBA8888 components
            uint8_t r = (p >> 16) & 0xFF;
            uint8_t g = (p >>  8) & 0xFF;
            uint8_t b = (p >>  0) & 0xFF;

            // Luminance threshold (ITU-R BT.601)
            uint8_t lum = (77 * r + 150 * g + 29 * b) >> 8;

            if (lum > 128)
            {
                lcd_fb[page][x] |= mask;
            }
        }
    }

    // Push all 8 pages to the LCD (handles page reordering automatically)
    for (int page = 0; page < 8; page++)
    {
        nuttyDriverLCD.updatePageSeqOrder(page, lcd_fb[page], 128);
    }

    // ---- 2. Poll button input ----
    pollNuttyInput();
}

void DG_SleepMs(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

uint32_t DG_GetTicksMs(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
    {
        return 0; // queue empty
    }

    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex++;
    s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

    *pressed = (keyData >> 8) & 1;
    *doomKey = keyData & 0xFF;

    return 1;
}

void DG_SetWindowTitle(const char *title)
{
    (void)title; // no-op on the NuttyBadge
}
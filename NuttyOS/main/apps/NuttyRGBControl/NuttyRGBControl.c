/*
 * NuttyRGBControl Application Usage:
 *
 * This application allows you to control the RGB LEDs individually or use presets.
 *
 * General Controls:
 * - START: Exit the application.
 * - SELECT: Toggle between Preset Mode and Custom Mode.
 *
 * Preset Mode:
 * - D-Pad UP/DOWN: Cycle through color presets.
 * - A Button: Apply the selected preset to all LEDs.
 * - B Button: Start a rainbow animation.
 *
 * Custom Mode:
 * - D-Pad UP/DOWN: Select the LED to control (1-3).
 * - D-Pad LEFT: Switch between RGB and HSV color modes.
 * - A Button: Increase the first value (R for RGB, H for HSV).
 * - B Button: Increase the second value (G for RGB, S for HSV).
 * - D-Pad RIGHT: Increase the third value (B for RGB, V for HSV).
 */

#include "NuttyRGBControl.h"
#include "services/NuttyRGB/NuttyRGB.h"

static const char *TAG = "RGB Controller";

// Color presets
typedef struct {
    const char* name;
    uint8_t r, g, b;
} ColorPreset;

static const ColorPreset colorPresets[] = {
    {"Red", 255, 0, 0},
    {"Green", 0, 255, 0},
    {"Blue", 0, 0, 255},
    {"Yellow", 255, 255, 0},
    {"Purple", 255, 0, 255},
    {"Cyan", 0, 255, 255},
    {"White", 255, 255, 255},
    {"Off", 0, 0, 0}
};

#define PRESET_COUNT (sizeof(colorPresets) / sizeof(ColorPreset))

// Application state
typedef enum {
    MODE_RGB,
    MODE_HSV
} ColorMode;

typedef struct {
    uint8_t selectedBulb;  // Currently selected LED (0-2)
    ColorMode mode;        // RGB or HSV mode
    uint8_t r[RGB_BULBS], g[RGB_BULBS], b[RGB_BULBS];  // RGB values for each LED
    uint8_t h[RGB_BULBS], s[RGB_BULBS], v[RGB_BULBS];  // HSV values for each LED
    uint8_t selectedPreset; // Currently selected preset
    bool presetMode;       // Whether in preset mode
} AppState;

static AppState appState = {0};

// UI elements
static lv_obj_t *mainContainer = NULL;
static lv_obj_t *modeLabel = NULL;
static lv_obj_t *bulbLabel = NULL;
static lv_obj_t *colorLabel = NULL;
static lv_obj_t *presetLabel = NULL;
static lv_obj_t *helpLabel = NULL;

// Animation sequences
static NuttyRGBAnimationSequence rainbowAnim[] = {
    {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, 1000},    // Red, Green, Blue
    {{255, 255, 0}, {0, 255, 255}, {255, 0, 255}, 1000}, // Yellow, Cyan, Purple
    {{255, 255, 255}, {0, 0, 0}, {128, 128, 128}, 1000}  // White, Black, Gray
};

static void updateDisplay() {
    NuttyDisplay_lockLVGL();
    
    // Update first line: LED number, type, and values
    char firstLineText[64];
    if (appState.mode == MODE_RGB) {
        snprintf(firstLineText, sizeof(firstLineText), "LED%d [RGB] %d %d %d", 
                appState.selectedBulb + 1, appState.r[appState.selectedBulb], appState.g[appState.selectedBulb], appState.b[appState.selectedBulb]);
    } else {
        snprintf(firstLineText, sizeof(firstLineText), "LED%d [HSV] %d %d %d", 
                appState.selectedBulb + 1, appState.h[appState.selectedBulb], appState.s[appState.selectedBulb], appState.v[appState.selectedBulb]);
    }
    lv_label_set_text(modeLabel, firstLineText);
    
    // Update second line: Color preset
    if (appState.presetMode) {
        lv_label_set_text(presetLabel, colorPresets[appState.selectedPreset].name);
    } else {
        lv_label_set_text(presetLabel, "Custom");
    }
    
    NuttyDisplay_unlockLVGL();
}

static void applyColor() {
    if (appState.mode == MODE_RGB) {
        NuttyRGB_SetRGBAndDisplay(appState.selectedBulb, appState.r[appState.selectedBulb], appState.g[appState.selectedBulb], appState.b[appState.selectedBulb]);
    } else {
        NuttyRGB_SetHSVAndDisplay(appState.selectedBulb, appState.h[appState.selectedBulb], appState.s[appState.selectedBulb], appState.v[appState.selectedBulb]);
    }
}

static void readCurrentLEDColor() {
    // Read the actual color values from the RGB service for the selected LED
    // Note: This assumes NuttyRGB service provides functions to read current values
    // If not available, we'll keep using the stored values
    // TODO: Implement actual color reading if NuttyRGB service supports it
}

static void applyPreset() {
    const ColorPreset *preset = &colorPresets[appState.selectedPreset];
    appState.r[appState.selectedBulb] = preset->r;
    appState.g[appState.selectedBulb] = preset->g;
    appState.b[appState.selectedBulb] = preset->b;
    appState.mode = MODE_RGB;
    applyColor();
    updateDisplay();
}

static void createUI() {
    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    
    NuttyDisplay_lockLVGL();
    
    // Create main container
    mainContainer = lv_obj_create(drawArea);
    lv_obj_set_size(mainContainer, 128, 59);
    lv_obj_set_pos(mainContainer, 0, 0);
    lv_obj_set_style_border_width(mainContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mainContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    
    // Create a style for the labels
    static lv_style_t label_style;
    lv_style_init(&label_style);
    lv_style_set_text_font(&label_style, &lv_font_montserrat_10);

    // First line: LED number, type, and values
    modeLabel = lv_label_create(mainContainer);
    lv_obj_add_style(modeLabel, &label_style, 0);
    lv_obj_set_pos(modeLabel, 2, 2);
    lv_label_set_text(modeLabel, "LED1 [RGB] 255 255 255");
    
    // Second line: Color preset
    presetLabel = lv_label_create(mainContainer);
    lv_obj_add_style(presetLabel, &label_style, 0);
    lv_obj_set_pos(presetLabel, 2, 12);
    lv_label_set_text(presetLabel, "Custom");
    
    // Third line: Instructions
    helpLabel = lv_label_create(mainContainer);
    lv_obj_add_style(helpLabel, &label_style, 0);
    lv_obj_set_pos(helpLabel, 2, 22);
    lv_label_set_text(helpLabel, "SEL: Preset, START: Exit");

    // Fourth line: Instructions
    helpLabel = lv_label_create(mainContainer);
    lv_obj_add_style(helpLabel, &label_style, 0);
    lv_obj_set_pos(helpLabel, 2, 32);
    lv_label_set_text(helpLabel, "A: R, B: G, Rt: B");

    // Fifth line: Instructions
    helpLabel = lv_label_create(mainContainer);
    lv_obj_add_style(helpLabel, &label_style, 0);
    lv_obj_set_pos(helpLabel, 2, 42);
    lv_label_set_text(helpLabel, "Up/Dn: +/-, Lf:CMod");

    // Remove unused labels
    bulbLabel = NULL;
    colorLabel = NULL;
    
    NuttyDisplay_unlockLVGL();
}

static void nutty_main(void) {
    ESP_LOGI(TAG, "Starting RGB Controller...");
    
    // Initialize application state
    appState.selectedBulb = 0;
    appState.mode = MODE_RGB;
    for (int i = 0; i < RGB_BULBS; i++) {
        appState.selectedBulb = i;
        appState.r[i] = 0;
        appState.g[i] = 0;
        appState.b[i] = 0;
        appState.h[i] = 0;    // Hue: 0 = red (doesn't matter when V=0)
        appState.s[i] = 255;  // Saturation: full saturation (doesn't matter when V=0)
        appState.v[i] = 0;    // Value: 0 = off/black
        applyColor();
    }
    appState.selectedBulb = 0;
    appState.selectedPreset = 0;
    appState.presetMode = false;
    
    // Create UI
    createUI();
    
    // Apply initial color
    applyColor();
    updateDisplay();
    
    // Clear button states
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    
    while(true) {
        // Check for exit
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_START)) {
            break;
        }
        
        // Toggle preset mode
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_SELECT)) {
            appState.presetMode = !appState.presetMode;
            if (appState.presetMode) {
                applyPreset();
            }
            updateDisplay();
        }
        
        if (appState.presetMode) {
            // Preset mode: use D-pad to select presets
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
                if (appState.selectedPreset > 0) {
                    appState.selectedPreset--;
                } else {
                    appState.selectedPreset = PRESET_COUNT - 1;
                }
                applyPreset();
            }
            
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
                appState.selectedPreset = (appState.selectedPreset + 1) % PRESET_COUNT;
                applyPreset();
            }
            
            // Apply preset to all LEDs
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
                const ColorPreset *preset = &colorPresets[appState.selectedPreset];
                for (int i = 0; i < RGB_BULBS; i++) {
                    appState.r[i] = preset->r;
                    appState.g[i] = preset->g;
                    appState.b[i] = preset->b;
                }
            }
            
            // Rainbow animation
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B)) {
                NuttyRGB_SetAnimationSequences(rainbowAnim, sizeof(rainbowAnim) / sizeof(NuttyRGBAnimationSequence));
                NuttyRGB_StartAnimation();
                vTaskDelay(pdMS_TO_TICKS(3000)); // Play for 3 seconds
                NuttyRGB_StopAnimation();
            }
            
        } else {
            // Custom mode: use D-pad to select LED
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
                appState.selectedBulb = (appState.selectedBulb + 1) % RGB_BULBS;
                readCurrentLEDColor(); // Read the actual color values for the new LED
                updateDisplay();
            }
            
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
                appState.selectedBulb = (appState.selectedBulb + RGB_BULBS - 1) % RGB_BULBS;
                readCurrentLEDColor(); // Read the actual color values for the new LED
                updateDisplay();
            }
            
            // Mode switching
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_LEFT)) {
                appState.mode = (appState.mode == MODE_RGB) ? MODE_HSV : MODE_RGB;
                updateDisplay();
            }
            
            // Color adjustment
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
                if (appState.mode == MODE_RGB) {
                    appState.r[appState.selectedBulb] = (appState.r[appState.selectedBulb] + 15) % 256;
                } else {
                    appState.h[appState.selectedBulb] = (appState.h[appState.selectedBulb] + 15) % 256;
                }
                applyColor();
                updateDisplay();
            }
            
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B)) {
                if (appState.mode == MODE_RGB) {
                    appState.g[appState.selectedBulb] = (appState.g[appState.selectedBulb] + 15) % 256;
                } else {
                    appState.s[appState.selectedBulb] = (appState.s[appState.selectedBulb] + 15) % 256;
                }
                applyColor();
                updateDisplay();
            }
            
            // Brightness adjustment (right button)
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_RIGHT)) {
                if (appState.mode == MODE_RGB) {
                    appState.b[appState.selectedBulb] = (appState.b[appState.selectedBulb] + 15) % 256;
                } else {
                    appState.v[appState.selectedBulb] = (appState.v[appState.selectedBulb] + 15) % 256;
                }
                applyColor();
                updateDisplay();
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Cleanup
    NuttyDisplay_clearUserAppArea();
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyRGBControl = {
    .appName = "RGB Control",
    .appMainEntry = nutty_main,
    .appHidden = false
};

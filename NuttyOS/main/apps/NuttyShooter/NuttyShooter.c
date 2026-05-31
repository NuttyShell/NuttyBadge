#include "NuttyShooter.h"
#include "esp_mac.h"

// A tag for logging messages to the console, useful for debugging.
static const char *TAG = "NuttyShooter";

/* --- Game Configuration --- */
#define SHOOTER_GAME_IR_ADDR 0xA1B2
#define MAX_LIVES 9
#define REGEN_TIME_US (10 * 1000 * 1000)
#define HALF_BRIGHTNESS 128
#define INVINCIBILITY_TIME_US (1 * 1000 * 1000)

/* --- Game State and UI --- */
typedef struct {
    int lives;
    uint64_t last_hit_timestamp;
    uint64_t invincibility_end_timestamp;
    bool game_over;
    uint16_t player_ir_cmd;
} GameState;

static GameState gameState;

// UI elements
static lv_obj_t *livesLabel = NULL;
static lv_obj_t *statusLabel = NULL;
static lv_obj_t *playerCodeLabel = NULL;
static lv_obj_t *regenProgressLabel = NULL; // Using a text-based progress bar

// Menu choices
static const char *menuChoices[] = {
    "Tutorial",
    "Start",
    "Exit"
};

/**
 * @brief A structure to hold RGB color values.
 */
typedef struct {
    uint8_t r, g, b;
} Color;

/**
 * @brief Updates the RGB LEDs with the correct cascading health display.
 */
static void update_rgb_leds() {
    const Color GREEN = {0, HALF_BRIGHTNESS, 0};
    const Color YELLOW = {HALF_BRIGHTNESS, HALF_BRIGHTNESS, 0};
    const Color RED = {HALF_BRIGHTNESS, 0, 0};
    const Color OFF = {0, 0, 0};
    Color led_colors[RGB_BULBS];

    if (gameState.lives >= 9) { led_colors[0] = led_colors[1] = led_colors[2] = GREEN; }
    else if (gameState.lives == 8) { led_colors[0] = YELLOW; led_colors[1] = led_colors[2] = GREEN; }
    else if (gameState.lives == 7) { led_colors[0] = led_colors[1] = YELLOW; led_colors[2] = GREEN; }
    else if (gameState.lives == 6) { led_colors[0] = led_colors[1] = led_colors[2] = YELLOW; }
    else if (gameState.lives == 5) { led_colors[0] = RED; led_colors[1] = led_colors[2] = YELLOW; }
    else if (gameState.lives == 4) { led_colors[0] = led_colors[1] = RED; led_colors[2] = YELLOW; }
    else if (gameState.lives == 3) { led_colors[0] = led_colors[1] = led_colors[2] = RED; }
    else if (gameState.lives == 2) { led_colors[0] = OFF; led_colors[1] = led_colors[2] = RED; }
    else if (gameState.lives == 1) { led_colors[0] = led_colors[1] = OFF; led_colors[2] = RED; }
    else { led_colors[0] = led_colors[1] = led_colors[2] = OFF; }

    for (int i = 0; i < RGB_BULBS; i++) {
        NuttyRGB_SetRGBWithoutDisplay(i, led_colors[i].r, led_colors[i].g, led_colors[i].b);
    }
    NuttyRGB_DisplayNow();
}

/**
 * @brief Updates the display based on game state, including the regen progress.
 */
static void update_display() {
    NuttyDisplay_lockLVGL();
    if (gameState.game_over) {
        lv_label_set_text(livesLabel, "End Game");
        lv_label_set_text(statusLabel, "Press any key to exit");
        lv_obj_add_flag(playerCodeLabel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(regenProgressLabel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text_fmt(livesLabel, "Lives: %d / %d", gameState.lives, MAX_LIVES);
        lv_label_set_text(statusLabel, "A: Shoot | START: Quit");
        lv_label_set_text_fmt(playerCodeLabel, "Player ID: 0x%04X", gameState.player_ir_cmd);
        lv_obj_clear_flag(playerCodeLabel, LV_OBJ_FLAG_HIDDEN);

        // Show progress bar only when regenerating health
        if (gameState.lives < MAX_LIVES) {
            lv_obj_clear_flag(regenProgressLabel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(regenProgressLabel, LV_OBJ_FLAG_HIDDEN);
        }
    }
    NuttyDisplay_unlockLVGL();
}

/**
 * @brief Updates the regeneration progress bar with dashes.
 */
static void update_regen_progress(int percent) {
    if (gameState.lives >= MAX_LIVES) {
        return; // Don't update if at full health
    }

    // MODIFIED: Reduced character count to prevent screen overflow with pixel font.
    const int MAX_CHARS = 24;
    char progress_text[MAX_CHARS + 3]; // +3 for brackets and null terminator
    int filled_chars = (percent * MAX_CHARS) / 100;

    // Ensure at least one dash appears when regeneration starts
    if (percent > 0 && filled_chars == 0) {
        filled_chars = 1;
    }

    // Build the progress string: [----------          ]
    progress_text[0] = '[';
    int i;
    for (i = 0; i <= filled_chars && i < MAX_CHARS; i++) {
        progress_text[i + 1] = '-';
    }
    for (; i < MAX_CHARS; i++) {
        progress_text[i + 1] = ' ';
    }
    progress_text[MAX_CHARS + 1] = ']';
    progress_text[MAX_CHARS + 2] = '\0';

    NuttyDisplay_lockLVGL();
    lv_label_set_text(regenProgressLabel, progress_text);
    NuttyDisplay_unlockLVGL();
}

/**
 * @brief Creates the UI for the main game session.
 */
static void create_game_ui() {
    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    NuttyDisplay_lockLVGL();

    static lv_style_t lives_style, status_style, code_style, progress_style;
    lv_style_init(&lives_style);
    lv_style_set_text_font(&lives_style, &lv_font_montserrat_16);
    lv_style_init(&status_style);
    lv_style_set_text_font(&status_style, &lv_font_montserrat_10);
    lv_style_init(&code_style);
    lv_style_set_text_font(&code_style, &lv_font_montserrat_10);
    lv_style_init(&progress_style);
    lv_style_set_text_font(&progress_style, &cg_pixel_4x5_mono);

    // Create progress bar at the top, full width
    regenProgressLabel = lv_label_create(drawArea);
    lv_obj_add_style(regenProgressLabel, &progress_style, 0);
    lv_obj_align(regenProgressLabel, LV_ALIGN_TOP_LEFT, 0, 2);
    // MODIFIED: Initial empty bar string shortened to match new character count
    lv_label_set_text(regenProgressLabel, "[                       ]");

    // Position other elements below the progress bar
    livesLabel = lv_label_create(drawArea);
    lv_obj_add_style(livesLabel, &lives_style, 0);
    lv_obj_align(livesLabel, LV_ALIGN_TOP_MID, 0, 10);

    statusLabel = lv_label_create(drawArea);
    lv_obj_add_style(statusLabel, &status_style, 0);
    lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_MID, 0, -5);

    playerCodeLabel = lv_label_create(drawArea);
    lv_obj_add_style(playerCodeLabel, &code_style, 0);
    lv_obj_align(playerCodeLabel, LV_ALIGN_CENTER, 0, 5);

    NuttyDisplay_unlockLVGL();
}

/**
 * @brief Runs one full game session, from start to game over.
 */
static void run_game_session(void) {
    ESP_LOGI(TAG, "Starting new game session...");

    // 1. Initialization
    gameState.lives = MAX_LIVES;
    gameState.last_hit_timestamp = esp_timer_get_time();
    gameState.invincibility_end_timestamp = 0;
    gameState.game_over = false;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    gameState.player_ir_cmd = (mac[3] << 8) | (mac[4] ^ mac[5]);
    ESP_LOGI(TAG, "Player IR Command (MAC-derived): 0x%04X", gameState.player_ir_cmd);

    NuttyIR_Init();
    NuttyRGB_Init();
    create_game_ui();
    update_rgb_leds();
    update_display();

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);

    // 2. Main Game Loop
    while (true) {
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_START)) break;
        if (gameState.game_over && NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)) break;

        if (!gameState.game_over) {
            uint64_t current_time = esp_timer_get_time();

            if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
                ESP_LOGI(TAG, "Action: Shoot IR signal (Cmd: 0x%04X)", gameState.player_ir_cmd);
                NuttyIR_TxNECext(SHOOTER_GAME_IR_ADDR, gameState.player_ir_cmd);
                NuttySystemMonitor_setSystemTrayTempText("Pew Pew!", 5);
            }

            uint16_t irAddr, irCmd;
            uint8_t irStatus;
            NuttyIR_getLatestRecvResult(&irAddr, &irCmd, &irStatus);

            if (current_time >= gameState.invincibility_end_timestamp &&
                (irStatus == 2 || irStatus == 4) &&
                irAddr == SHOOTER_GAME_IR_ADDR &&
                irCmd != gameState.player_ir_cmd) {

                ESP_LOGI(TAG, "Event: Hit by IR signal! (Cmd: 0x%04X)", irCmd);
                NuttySystemMonitor_setSystemTrayTempText("Ouch! I'm hit!", 10);

                if (gameState.lives > 0) {
                    gameState.lives--;
                    gameState.last_hit_timestamp = current_time;
                    gameState.invincibility_end_timestamp = current_time + INVINCIBILITY_TIME_US;
                    update_rgb_leds();
                    update_display();
                    // Reset progress bar when hit
                    update_regen_progress(0);
                    if (gameState.lives == 0) {
                        ESP_LOGI(TAG, "Event: Game Over.");
                        gameState.game_over = true;
                        update_display();
                    }
                }
            }

            // Update regeneration progress
            if (gameState.lives < MAX_LIVES) {
                uint64_t time_since_hit = current_time - gameState.last_hit_timestamp;
                int32_t percent = (time_since_hit * 100) / REGEN_TIME_US;
                if (percent > 100) percent = 100;

                update_regen_progress(percent);
            }

            if (gameState.lives < MAX_LIVES && (current_time - gameState.last_hit_timestamp) > REGEN_TIME_US) {
                ESP_LOGI(TAG, "Event: Regenerating one life.");
                gameState.lives++;
                gameState.last_hit_timestamp = current_time;
                update_rgb_leds();
                update_display();
                NuttySystemMonitor_setSystemTrayTempText("Life +1", 10);
                // Reset progress bar after regeneration
                update_regen_progress(0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(3));
    }

    // 3. Cleanup for the session
    ESP_LOGI(TAG, "Ending game session...");
    NuttySystemMonitor_setSystemTrayTempText("Goodbye!", 2);
    NuttyRGB_SetAllRGBAndDisplay(HALF_BRIGHTNESS, HALF_BRIGHTNESS, HALF_BRIGHTNESS);
    vTaskDelay(pdMS_TO_TICKS(2000));
    NuttyRGB_SetAllRGBAndDisplay(0, 0, 0);
    NuttyIR_Deinit();
    NuttyRGB_Deinit();
    NuttyDisplay_clearUserAppArea();
}

/**
 * @brief Helper function to create a new label with a specific style and position.
 */
static lv_obj_t* new_pixel_label(const char* text, lv_obj_t* parent, lv_style_t* style, lv_align_t align, lv_coord_t y_offset) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_add_style(label, style, 0);
    lv_obj_align(label, align, 0, y_offset);
    return label;
}

/**
 * @brief Displays the tutorial screen with a new, structured layout.
 */
static void show_tutorial() {
    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    NuttyDisplay_lockLVGL();

    static lv_style_t pixel_style;
    lv_style_init(&pixel_style);
    lv_style_set_text_font(&pixel_style, &cg_pixel_4x5_mono);

    new_pixel_label("--- TUTORIAL ---", drawArea, &pixel_style, LV_ALIGN_TOP_MID, 4);
    new_pixel_label("A BUTTON: SHOOT", drawArea, &pixel_style, LV_ALIGN_TOP_MID, 12);
    new_pixel_label("START: QUIT GAME", drawArea, &pixel_style, LV_ALIGN_TOP_MID, 20);
    new_pixel_label("YOUR ID IS ON SCREEN", drawArea, &pixel_style, LV_ALIGN_TOP_MID, 28);
    new_pixel_label("AVOID USING SAME ID", drawArea, &pixel_style, LV_ALIGN_TOP_MID, 36);
    // MODIFIED: Changed "DOT BAR" to "TOP BAR" for accuracy
    new_pixel_label("TOP BAR SHOWS REGEN", drawArea, &pixel_style, LV_ALIGN_TOP_MID, 44);
    new_pixel_label("PRESS ANY KEY TO RETURN", drawArea, &pixel_style, LV_ALIGN_BOTTOM_MID, 52);

    NuttyDisplay_unlockLVGL();

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while(true) {
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    NuttyDisplay_clearUserAppArea();
}

// LVGL event handler for menu clicks
static void lvgl_menu_on_click_event_handler(lv_event_t * event) {
    char *item_text = lv_label_get_text(lv_obj_get_child(event->target, 0));
    for(int i = 0; i < sizeof(menuChoices) / sizeof(char *); i++) {
        if(strcmp(menuChoices[i], item_text) == 0) {
            *((char **)event->user_data) = (char *)menuChoices[i];
            return;
        }
    }
}

/**
 * @brief The main application entry point, now managing the menu.
 */
static void nutty_main(void) {
    ESP_LOGI(TAG, "Welcome to Nutty Shooter!");
    bool exitApp = false;

    while(!exitApp) {
        char *selectedChoice = NULL;
        NuttyInputLVGLInputMapping mapping = { .UP=LV_KEY_PREV, .DOWN=LV_KEY_NEXT, .A=LV_KEY_ENTER };
        lv_indev_t *indev = NuttyInput_UpdateLVGLInDev(mapping);
        static lv_group_t *g = NULL;
        if (g == NULL) g = lv_group_create();
        lv_group_set_default(g);
        lv_indev_set_group(indev, g);

        lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
        NuttyDisplay_lockLVGL();
        lv_obj_t *menu = lv_menu_create(drawArea);
        lv_obj_set_size(menu, lv_obj_get_width(drawArea), lv_obj_get_height(drawArea));
        lv_obj_center(menu);
        lv_obj_t *mainPage = lv_menu_page_create(menu, NULL);

        for(uint8_t i = 0; i < sizeof(menuChoices) / sizeof(char *); i++) {
            lv_obj_t* cont = lv_menu_cont_create(mainPage);
            lv_obj_t* label = lv_label_create(cont);
            lv_label_set_text(label, menuChoices[i]);
            lv_obj_add_event_cb(cont, lvgl_menu_on_click_event_handler, LV_EVENT_CLICKED, (void *)&selectedChoice);
            lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
            lv_group_add_obj(g, cont);
        }
        lv_menu_set_page(menu, mainPage);
        NuttyDisplay_unlockLVGL();

        while(selectedChoice == NULL) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        NuttyDisplay_lockLVGL();
        lv_group_remove_all_objs(g);
        NuttyDisplay_unlockLVGL();
        NuttyDisplay_clearUserAppArea();

        if(strcmp(selectedChoice, "Start") == 0) {
            run_game_session();
        } else if(strcmp(selectedChoice, "Tutorial") == 0) {
            show_tutorial();
        } else if(strcmp(selectedChoice, "Exit") == 0) {
            exitApp = true;
        }
    }

    ESP_LOGI(TAG, "Exiting application.");
    NuttyApps_launchAppByIndex(0);
}

// The application definition structure that the system uses to list and launch the game.
NuttyAppDefinition NuttyShooter = {
    .appName = "Nutty Shooter",
    .appMainEntry = nutty_main,
    .appHidden = false
};
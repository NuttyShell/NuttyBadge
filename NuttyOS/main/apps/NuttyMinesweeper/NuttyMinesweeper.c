#include "NuttyMinesweeper.h"

static const char *TAG = "Minesweeper";

static const char *menuChoices[] = {
    "Start",
    "Edit Difficulty",
    "Tutorial",
    "< Back"
};

static _NuttyMinesweeperConfig minesweeper_config = {
    .width = 10,
    .height = 8,
    .mines = 12,
    .difficulty = 1
};

static _MineCell minefield[16][32];
static lv_obj_t *cell_buttons[16][32];
static uint8_t cursor_x = 0, cursor_y = 0;
static bool game_over = false;
static bool game_won = false;
static uint8_t revealed_count = 0;
static uint8_t flag_count = 0;

// Display constants
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define TOP_OCCUPIED_HEIGHT 5
#define GAME_START_Y 0
#define USABLE_HEIGHT (DISPLAY_HEIGHT - TOP_OCCUPIED_HEIGHT)

static void lvgl_menu_on_click_event_handler(lv_event_t * event) {
    char *item_text = lv_label_get_text(lv_obj_get_child(event->target, 0));
    uint8_t i;
    for(i=0; i<sizeof(menuChoices)/sizeof(char *); i++) {
        if(strcmp(menuChoices[i], item_text) == 0) {
            *((char **)event->user_data) = (char *)menuChoices[i];
            return;
        }
    }
    *((char **)event->user_data) = NULL;
}

static lv_obj_t *new_label(char *text, lv_obj_t* drawArea, lv_style_t* style, lv_align_t aligned, lv_coord_t x, lv_coord_t y){
    lv_obj_t *lbl = lv_label_create(drawArea);
    lv_label_set_text(lbl, text);
    lv_obj_add_style(lbl, style, LV_PART_MAIN);
    lv_obj_align(lbl, aligned, x, y);
    return lbl;
}

static void set_difficulty(uint8_t difficulty){
    minesweeper_config.difficulty = difficulty;
    switch(difficulty) {
        case 0: // Easy
            minesweeper_config.width = 8;
            minesweeper_config.height = 6;
            minesweeper_config.mines = 6;
            break;
        case 1: // Medium
            minesweeper_config.width = 12;
            minesweeper_config.height = 8;
            minesweeper_config.mines = 14;
            break;
        case 2: // Hard
            minesweeper_config.width = 21;
            minesweeper_config.height = 9;
            minesweeper_config.mines = 40;
            break;
        default:
            // Default to Medium
            minesweeper_config.width = 12;
            minesweeper_config.height = 8;
            minesweeper_config.mines = 14;
            break;
    }
    ESP_LOGI(TAG, "Difficulty set to %d: %dx%d with %d mines",
             difficulty, minesweeper_config.width, minesweeper_config.height, minesweeper_config.mines);
}

static void init_minefield() {
    // 1. Initialize game state variables (this part is already optimal)
    revealed_count = 0;
    flag_count = 0;
    game_over = false;
    game_won = false;
    cursor_x = 0;
    cursor_y = 0;

    // 2. Clear the entire minefield data structure
    for (uint8_t y = 0; y < minesweeper_config.height; y++) {
        memset(minefield[y], 0, minesweeper_config.width * sizeof(_MineCell));
    }

    const uint16_t total_cells = minesweeper_config.width * minesweeper_config.height;
    const uint8_t num_mines = minesweeper_config.mines;

    // Defensive check: ensure we don't try to place more mines than there are cells.
    if (num_mines > total_cells) {
        ESP_LOGE(TAG, "Cannot place more mines than cells!");
        return;
    }

    // 3. IMPROVEMENT: Use the Fisher-Yates shuffle for efficient and fair mine placement.

    // Create a list of all possible cell locations (0 to total_cells-1)
    uint16_t* locations = malloc(total_cells * sizeof(uint16_t));
    if (!locations) {
        ESP_LOGE(TAG, "Failed to allocate memory for mine placement!");
        return;
    }
    for (uint16_t i = 0; i < total_cells; i++) {
        locations[i] = i;
    }

    // Shuffle the list
    srand(esp_timer_get_time());
    for (uint16_t i = total_cells - 1; i > 0; i--) {
        uint16_t j = rand() % (i + 1);
        // Swap locations[i] and locations[j]
        uint16_t temp = locations[i];
        locations[i] = locations[j];
        locations[j] = temp;
    }

    // 4. IMPROVEMENT: Place mines and calculate adjacent counts in a single pass.
    for (uint8_t i = 0; i < num_mines; i++) {
        uint16_t loc = locations[i];
        uint8_t x = loc % minesweeper_config.width;
        uint8_t y = loc / minesweeper_config.width;

        // Place the mine
        minefield[y][x].is_mine = true;

        // Increment the adjacent_mines count for all 8 neighbors
        for (int8_t dy = -1; dy <= 1; dy++) {
            for (int8_t dx = -1; dx <= 1; dx++) {
                // Skip the mine's own cell
                if (dx == 0 && dy == 0) continue;

                int16_t nx = x + dx;
                int16_t ny = y + dy;

                // Check if the neighbor is within the grid boundaries
                if (nx >= 0 && nx < minesweeper_config.width &&
                    ny >= 0 && ny < minesweeper_config.height) {
                    // No need to check if neighbor is a mine, just increment.
                    // This works because all counts start at 0.
                    minefield[ny][nx].adjacent_mines++;
                }
            }
        }
    }

    // 5. Clean up the temporary locations array
    free(locations);

    ESP_LOGI(TAG, "Minefield initialized with new high-performance algorithm.");
}

static void reveal_cell(uint8_t x, uint8_t y) {
    if(x >= minesweeper_config.width || y >= minesweeper_config.height ||
       minefield[y][x].is_revealed || minefield[y][x].is_flagged) {
        return;
    }

    minefield[y][x].is_revealed = true;
    revealed_count++;

    if(minefield[y][x].is_mine) {
        game_over = true;
        return;
    }

    // Auto-reveal adjacent cells if no adjacent mines
    if(minefield[y][x].adjacent_mines == 0) {
        for(int8_t dy = -1; dy <= 1; dy++) {
            for(int8_t dx = -1; dx <= 1; dx++) {
                int8_t nx = x + dx;
                int8_t ny = y + dy;
                if(nx >= 0 && nx < minesweeper_config.width &&
                   ny >= 0 && ny < minesweeper_config.height) {
                    reveal_cell(nx, ny);
                }
            }
        }
    }

    // Check win condition
    uint8_t safe_cells = minesweeper_config.width * minesweeper_config.height - minesweeper_config.mines;
    if(revealed_count >= safe_cells) {
        game_won = true;
    }
}

static void toggle_flag(uint8_t x, uint8_t y) {
    if(x >= minesweeper_config.width || y >= minesweeper_config.height ||
       minefield[y][x].is_revealed) {
        return;
    }

    if(minefield[y][x].is_flagged) {
        minefield[y][x].is_flagged = false;
        flag_count--;
    } else {
        minefield[y][x].is_flagged = true;
        flag_count++;
    }
}

static void reveal_all_mines() {
    for(uint8_t y = 0; y < minesweeper_config.height; y++) {
        for(uint8_t x = 0; x < minesweeper_config.width; x++) {
            if(minefield[y][x].is_mine) {
                minefield[y][x].is_revealed = true;
                minefield[y][x].is_flagged = false;
            }
        }
    }
}

static void update_display(lv_obj_t *drawArea) {
    NuttyDisplay_lockLVGL();

    lv_color_t black = lv_color_hex(0x000000);
    lv_color_t white = lv_color_hex(0xFFFFFF);

    for (uint8_t y = 0; y < minesweeper_config.height; y++) {
        for (uint8_t x = 0; x < minesweeper_config.width; x++) {
            lv_obj_t *btn = cell_buttons[y][x];
            lv_obj_t *label = lv_obj_get_child(btn, 0);

            lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN);

            if (minefield[y][x].is_revealed && minefield[y][x].is_mine) {
                // Cleaned: Updated comment to match code
                // Revealed Mine: Use an 'X' symbol.
                lv_obj_set_style_bg_color(btn, white, LV_PART_MAIN);
                lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
                lv_label_set_text(label, "X");
                lv_obj_set_style_text_color(label, black, LV_PART_MAIN);
            } else if (minefield[y][x].is_revealed) {
                // Revealed Safe Cell
                lv_obj_set_style_bg_color(btn, white, LV_PART_MAIN);
                lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
                lv_obj_set_style_text_color(label, black, LV_PART_MAIN);
                if (minefield[y][x].adjacent_mines > 0) {
                    char num_buffer[5];
                    sprintf(num_buffer, "%d", minefield[y][x].adjacent_mines);
                    lv_label_set_text(label, num_buffer);
                } else {
                    lv_label_set_text(label, "");
                }
            } else if (minefield[y][x].is_flagged) {
                // Unrevealed, Flagged Cell - now without a border
                lv_obj_set_style_bg_color(btn, white, LV_PART_MAIN);
                lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN); // CHANGE: Removed border for flagged cells
                lv_label_set_text(label, "F");
                lv_obj_set_style_text_color(label, black, LV_PART_MAIN);
            } else {
                // Unrevealed, Un-flagged Cell
                lv_obj_set_style_bg_color(btn, white, LV_PART_MAIN);
                lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
                lv_obj_set_style_border_color(btn, black, LV_PART_MAIN);
                lv_label_set_text(label, "");
            }

            // Cursor logic
            if (x == cursor_x && y == cursor_y) {
                lv_obj_set_style_outline_width(btn, 1, LV_PART_MAIN);
                lv_obj_set_style_outline_pad(btn, 0, LV_PART_MAIN);

                if (!minefield[y][x].is_revealed) {
                    lv_obj_set_style_outline_color(btn, white, LV_PART_MAIN);
                } else {
                    lv_obj_set_style_outline_color(btn, black, LV_PART_MAIN);
                }
            }
        }
    }
    NuttyDisplay_unlockLVGL();
}

static void minesweeper_start(){
    ESP_LOGI(TAG, "Minesweeper Game Start");

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    init_minefield();

    NuttyDisplay_lockLVGL();

    // Calculate cell size
    uint8_t cell_width = DISPLAY_WIDTH / minesweeper_config.width;
    uint8_t cell_height = USABLE_HEIGHT / minesweeper_config.height;
    uint8_t cell_size = (cell_width < cell_height) ? cell_width : cell_height;
    if(cell_size < 6) cell_size = 6; // Minimum size

    // Calculate grid positioning
    uint8_t grid_width = minesweeper_config.width * cell_size;
    uint8_t grid_height = minesweeper_config.height * cell_size;
    uint8_t start_x = (DISPLAY_WIDTH - grid_width) / 2;
    uint8_t start_y = GAME_START_Y + (USABLE_HEIGHT - grid_height) / 2;

    // Create buttons
    for(uint8_t y = 0; y < minesweeper_config.height; y++) {
        for(uint8_t x = 0; x < minesweeper_config.width; x++) {
            cell_buttons[y][x] = lv_btn_create(drawArea);
            lv_obj_set_size(cell_buttons[y][x], cell_size, cell_size);
            lv_obj_set_pos(cell_buttons[y][x], start_x + x * cell_size, start_y + y * cell_size);
            lv_obj_set_style_radius(cell_buttons[y][x], 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(cell_buttons[y][x], 0, LV_PART_MAIN);

            lv_obj_t *label = lv_label_create(cell_buttons[y][x]);
            lv_label_set_text(label, "");
            lv_obj_set_style_text_font(label, &cg_pixel_4x5_mono, LV_PART_MAIN);
            lv_obj_center(label);
        }
    }

    NuttyDisplay_unlockLVGL();

    // Initial display
    update_display(drawArea);
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);

    while(!game_over && !game_won) {
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
            if(cursor_y > 0) cursor_y--;
            update_display(drawArea);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
            if(cursor_y < minesweeper_config.height - 1) cursor_y++;
            update_display(drawArea);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_LEFT)) {
            if(cursor_x > 0) cursor_x--;
            update_display(drawArea);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_RIGHT)) {
            if(cursor_x < minesweeper_config.width - 1) cursor_x++;
            update_display(drawArea);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
            reveal_cell(cursor_x, cursor_y);
            update_display(drawArea);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B)) {
            toggle_flag(cursor_x, cursor_y);
            update_display(drawArea);
        }

        char status_text[64];
        sprintf(status_text, "M:%d F:%d", minesweeper_config.mines, flag_count);
        NuttySystemMonitor_setSystemTrayTempText(status_text, 10);

        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_SELECT)) break;

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // Game end
    if(game_over) {
        reveal_all_mines();
        update_display(drawArea);
        NuttySystemMonitor_setSystemTrayTempText("Game Over!", 30);
    } else if(game_won) {
        NuttySystemMonitor_setSystemTrayTempText("You Won!", 30);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
    NuttyDisplay_clearUserAppArea();
}

static void option_difficulty(){
    lv_style_t lbl_font_big, lbl_font_nano;
    lv_style_init(&lbl_font_big);
    lv_style_init(&lbl_font_nano);
    lv_style_set_text_font(&lbl_font_big, &lv_font_unscii_16);
    lv_style_set_text_font(&lbl_font_nano, &cg_pixel_4x5_mono);

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();

    NuttyDisplay_lockLVGL();
    new_label("Change Difficulty", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 4);

    char diff_names[][10] = {"Easy", "Medium", "Hard"};
    lv_obj_t *lbl_diff = new_label(diff_names[minesweeper_config.difficulty], drawArea, &lbl_font_big, LV_ALIGN_TOP_MID, 0, 15);

    new_label("Press A to Confirm", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 33);
    new_label("Press ARROWS to edit", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 39);
    new_label("Press SELECT back to menu", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 45);

    uint8_t current_diff = minesweeper_config.difficulty;
    NuttyDisplay_unlockLVGL();

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);

    while(1){
        vTaskDelay(pdMS_TO_TICKS(5));

        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
            set_difficulty(current_diff);
            NuttySystemMonitor_setSystemTrayTempText("Difficulty changed!", 20);
        }

        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP) ||
           NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_RIGHT)) {
            current_diff = (current_diff + 1) % 3;
            NuttyDisplay_lockLVGL();
            lv_label_set_text(lbl_diff, diff_names[current_diff]);
            NuttyDisplay_unlockLVGL();
        }

        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN) ||
           NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_LEFT)) {
            current_diff = (current_diff + 2) % 3;
            NuttyDisplay_lockLVGL();
            lv_label_set_text(lbl_diff, diff_names[current_diff]);
            NuttyDisplay_unlockLVGL();
        }

        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_SELECT) ||
           NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B)) {
            break;
        }
    }
    NuttyDisplay_clearUserAppArea();
}

static void tutorial(){
    lv_style_t lbl_font_nano;
    lv_style_init(&lbl_font_nano);
    lv_style_set_text_font(&lbl_font_nano, &cg_pixel_4x5_mono);

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();

    NuttyDisplay_lockLVGL();
    new_label("MINESWEEPER TUTORIAL", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 2);
    new_label("ARROWS: Move cursor", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 12);
    new_label("A: Reveal cell", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 20);
    new_label("B: Flag/Unflag cell", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 28);
    new_label("Number show adjacent mine", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 36);
    new_label("X represents bombs", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 44);
    new_label("Hold SELECT: back to menu", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 52);
    NuttyDisplay_unlockLVGL();

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while(1){
        vTaskDelay(pdMS_TO_TICKS(5));
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)){
            break;
        }
    }
    NuttyDisplay_clearUserAppArea();
}

static void nutty_main(void) {
    ESP_LOGI(TAG, "Starting NuttyMinesweeper...");
    set_difficulty(1);

    bool exitApp = false;
    while(!exitApp) {
        char *selectedChoice = NULL;
        NuttyInputLVGLInputMapping mapping = {
            .UP=LV_KEY_PREV,
            .DOWN=LV_KEY_NEXT,
            .A=LV_KEY_ENTER,
        };
        lv_indev_t *indev = NuttyInput_UpdateLVGLInDev(mapping);
        assert(indev != NULL);

        static lv_group_t *g;
        g = lv_group_create();
        assert(g != NULL);
        lv_group_set_default(g);
        lv_indev_set_group(indev, g);

        lv_style_t text_style;
        lv_style_init(&text_style);
        lv_style_set_text_font(&text_style, &lv_font_montserrat_12);
        lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
        NuttyDisplay_lockLVGL();
        lv_obj_t *menu = lv_menu_create(drawArea);

        lv_obj_set_size(menu, lv_obj_get_width(drawArea), lv_obj_get_height(drawArea));
        lv_obj_center(menu);

        lv_obj_t *mainPage = lv_menu_page_create(menu, NULL);
        lv_obj_t *cont, *label;
        for(uint8_t i=0; i<sizeof(menuChoices)/sizeof(char *); i++) {
            cont = lv_menu_cont_create(mainPage);
            label = lv_label_create(cont);
            lv_label_set_text(label, menuChoices[i]);
            lv_obj_add_style(label, &text_style, LV_PART_MAIN);
            lv_menu_set_load_page_event(menu, cont, NULL);
            lv_obj_add_event_cb(cont, lvgl_menu_on_click_event_handler, LV_EVENT_CLICKED, (void *)&selectedChoice);
            lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
            lv_group_add_obj(g, cont);
        }
        lv_menu_set_page(menu, mainPage);
        NuttyDisplay_unlockLVGL();

        while(1) {
            if(selectedChoice != NULL) {
                NuttyDisplay_lockLVGL();
                lv_group_remove_all_objs(g);
                lv_group_del(g);
                NuttyDisplay_unlockLVGL();
                NuttyDisplay_clearUserAppArea();

                if(selectedChoice == menuChoices[0]) {
                    minesweeper_start();
                }else if(selectedChoice == menuChoices[1]) {
                    option_difficulty();
                }else if(selectedChoice == menuChoices[2]) {
                    tutorial();
                }else if(selectedChoice == menuChoices[3]) {
                    exitApp = true;
                }
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    NuttyDisplay_clearUserAppArea();
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyMinesweeper = {
    .appName = "Nutty Minesweeper",
    .appMainEntry = nutty_main,
    .appHidden = false
};
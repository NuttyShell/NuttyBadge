#include "NuttySnake.h"

#include "services/NuttyStorage/NuttyStorage.h"

static const char *TAG = "Snake";

/* TRUE_COLOR pixel buffer: 1 byte/pixel, 0=visible, 1=hidden (active-low LCD) */
static uint8_t snake_px[128 * 64];
static lv_img_dsc_t snake_img_dsc;
static lv_obj_t *snake_img_obj = NULL;

#define GRID_X_MIN 1
#define GRID_X_MAX 62
#define GRID_Y_MIN 1
#define GRID_Y_MAX 27

static inline void px_set(uint8_t x, uint8_t y) { snake_px[y * 128 + x] = 0; }
static inline void px_clear(uint8_t x, uint8_t y) { snake_px[y * 128 + x] = 1; }

static inline void draw_block(uint8_t gx, uint8_t gy, bool on) {
    uint8_t px = gx * 2;
    uint8_t py = gy * 2;
    if (on) {
        px_set(px, py); px_set(px+1, py);
        px_set(px, py+1); px_set(px+1, py+1);
    } else {
        px_clear(px, py); px_clear(px+1, py);
        px_clear(px, py+1); px_clear(px+1, py+1);
    }
}

/* Body pattern: .X / X. (01\10) */
static inline void draw_body(uint8_t gx, uint8_t gy) {
    uint8_t px = gx * 2;
    uint8_t py = gy * 2;
    px_clear(px, py); px_set(px+1, py);
    px_set(px, py+1); px_clear(px+1, py+1);
}

/* Wall pattern: X. / .X (10\01) */
static inline void draw_wall_block(uint8_t bx, uint8_t by) {
    px_set(bx, by); px_clear(bx+1, by);
    px_clear(bx, by+1); px_set(bx+1, by+1);
}

/* Bitmap collision map: 224 bytes */
static uint8_t snake_map[28][8];
static inline bool map_get(uint8_t y, uint8_t x) { return (snake_map[y][x >> 3] >> (x & 7)) & 1; }
static inline void map_set(uint8_t y, uint8_t x) { snake_map[y][x >> 3] |= (1 << (x & 7)); }
static inline void map_clr(uint8_t y, uint8_t x) { snake_map[y][x >> 3] &= ~(1 << (x & 7)); }

static void draw_wall(void) {
    for (uint8_t x = 0; x < 128; x += 2) draw_wall_block(x, 0);
    for (uint8_t x = 0; x < 128; x += 2) draw_wall_block(x, 56);
    for (uint8_t y = 2; y < 56; y += 2) draw_wall_block(0, y);
    for (uint8_t y = 2; y < 56; y += 2) draw_wall_block(126, y);
}

/* ---- Scoreboard: top 5 scores persisted via NVS blob ---- */
#define SCOREBOARD_KEY "snake_scores"
#define SCOREBOARD_SIZE 5

static void scoreboard_load(uint16_t *scores) {
    uint16_t defaults[SCOREBOARD_SIZE] = {0};
    NuttyStorage_getBlobKV(SCOREBOARD_KEY, scores, SCOREBOARD_SIZE * sizeof(uint16_t), defaults, SCOREBOARD_SIZE * sizeof(uint16_t));
}

static void scoreboard_save(uint16_t *scores) {
    NuttyStorge_setBlobKV(SCOREBOARD_KEY, scores, SCOREBOARD_SIZE * sizeof(uint16_t));
}

/* Insert score into top 5 (descending). Returns rank 0-4 if qualified, -1 if not */
static int8_t scoreboard_insert(uint16_t *scores, uint16_t score) {
    for (uint8_t i = 0; i < SCOREBOARD_SIZE; i++) {
        if (score > scores[i]) {
            /* Shift lower scores down */
            for (int8_t j = SCOREBOARD_SIZE - 1; j > i; j--) {
                scores[j] = scores[j - 1];
            }
            scores[i] = score;
            scoreboard_save(scores);
            return (int8_t)i;
        }
    }
    return -1;
}

static const char *menuChoices[] = {
    "Start",
    "Scoreboard",
    "Edit Difficulty",
    "Tutorial",
    "< Back"
};

static _NuttySnakeConfig snake_config = {
    .difficulty = 5,
    .speed = 140
};

static _NuttySnakeQueue* snake;
static _NuttySnakeQueue* snake_tail;

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

static int update_difficulty(lv_obj_t* lbl_m_s, int difficulty){
    difficulty = ((difficulty % 10) + 10) % 10;
    ESP_LOGI(TAG, "Current: %d", difficulty);
    NuttyDisplay_lockLVGL();
    lv_label_set_text_fmt(lbl_m_s, "%d", difficulty);
    NuttyDisplay_unlockLVGL();
    return difficulty;
}


static int64_t min(int64_t a, int64_t b){
    if(a < b) return a; else return b;
}


static void set_difficulty(uint8_t difficulty){
    snake_config.difficulty = difficulty;
    snake_config.speed = 250 - difficulty * 22;
    ESP_LOGI(TAG, "Change difficulty->%d speed->%d", snake_config.difficulty, snake_config.speed);
}

static void show_scoreboard(void) {
    lv_style_t lbl_font_nano;
    lv_style_init(&lbl_font_nano);
    lv_style_set_text_font(&lbl_font_nano, &cg_pixel_4x5_mono);

    uint16_t scores[SCOREBOARD_SIZE];
    scoreboard_load(scores);

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    NuttyDisplay_lockLVGL();
    new_label("Best Scores", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 4);
    char line[24];
    for (uint8_t i = 0; i < SCOREBOARD_SIZE; i++) {
        snprintf(line, sizeof(line), "%d.  %d", i + 1, scores[i]);
        new_label(line, drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 14 + i * 9);
    }
    NuttyDisplay_unlockLVGL();

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)) break;
    }
    lv_style_reset(&lbl_font_nano);
    NuttyDisplay_clearUserAppArea();
}


static void snake_start(){
    ESP_LOGI(TAG, "Game Start speed:%d", snake_config.speed);
    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    NuttySystemMonitor_setSystemTrayTempText("!!Game Started!!", 30);

    /* Init pixel buffer: fill hidden (1), draw wall borders */
    memset(snake_px, 1, sizeof(snake_px));
    draw_wall();

    /* Setup image descriptor */
    snake_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    snake_img_dsc.header.always_zero = 0;
    snake_img_dsc.header.reserved = 0;
    snake_img_dsc.header.w = 128;
    snake_img_dsc.header.h = 64;
    snake_img_dsc.data_size = 128 * 64;
    snake_img_dsc.data = snake_px;

    NuttyDisplay_lockLVGL();
    snake_img_obj = lv_img_create(drawArea);
    lv_img_set_src(snake_img_obj, &snake_img_dsc);
    lv_obj_align(snake_img_obj, LV_ALIGN_TOP_LEFT, 0, 0);
    NuttyDisplay_unlockLVGL();

    /* Init collision map */
    memset(snake_map, 0, sizeof(snake_map));

    /* Initial snake: 1 body node at (31,16), head at (31,15) */
    snake = malloc(sizeof(_NuttySnakeQueue));
    if (!snake) {
        NuttySystemMonitor_setSystemTrayTempText("!!OOM!!", 30);
        vTaskDelay(pdMS_TO_TICKS(1000));
        NuttyDisplay_lockLVGL();
        if (snake_img_obj) { lv_obj_del(snake_img_obj); snake_img_obj = NULL; }
        NuttyDisplay_unlockLVGL();
        NuttyDisplay_clearUserAppArea();
        return;
    }
    snake->X = 30;
    snake->Y = 16;
    snake->next = NULL;
    snake_tail = snake;
    map_set(16, 30);

    uint8_t nowX = 30, nowY = 15;
    map_set(15, 30);

    draw_block(30, 15, true);  /* head */
    draw_body(30, 16);

    srand(esp_timer_get_time());
    uint8_t foodX, foodY;
    do {
        foodX = rand() % 62 + 1;
        foodY = rand() % 27 + 1;
    } while (map_get(foodY, foodX) || (foodX == nowX && foodY == nowY));
    draw_block(foodX, foodY, true);

    NuttyDisplay_lockLVGL();
    lv_obj_invalidate(snake_img_obj);
    NuttyDisplay_unlockLVGL();

    uint8_t counter = 0;
    uint16_t score = 0;
    enum _NuttySnakeDIRECTION nextStep = UP;
    enum _NuttySnakeDIRECTION lastDir = UP;
    bool ticking = true;

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while(true) {
        /* Responsive input: prevent reversal of both queued and last direction */
        if(ticking) {
            if(nextStep != DOWN && lastDir != DOWN && NuttyInput_isOneOfTheButtonsCurrentlyPressed(NUTTYINPUT_BTN_UP)){
                nextStep = UP;
            }else if(nextStep != UP && lastDir != UP && NuttyInput_isOneOfTheButtonsCurrentlyPressed(NUTTYINPUT_BTN_DOWN)){
                nextStep = DOWN;
            }else if(nextStep != RIGHT && lastDir != RIGHT && NuttyInput_isOneOfTheButtonsCurrentlyPressed(NUTTYINPUT_BTN_LEFT)){
                nextStep = LEFT;
            }else if(nextStep != LEFT && lastDir != LEFT && NuttyInput_isOneOfTheButtonsCurrentlyPressed(NUTTYINPUT_BTN_RIGHT)){
                nextStep = RIGHT;
            }
        }

        if(ticking) {
            counter++;
            if(counter >= snake_config.speed){
                counter = 0;

                uint8_t nextX = nowX, nextY = nowY;
                if(nextStep == UP){
                    nextY = nowY - 1;
                }else if(nextStep == DOWN){
                    nextY = nowY + 1;
                }else if(nextStep == LEFT){
                    nextX = nowX - 1;
                }else if(nextStep == RIGHT){
                    nextX = nowX + 1;
                }
                lastDir = nextStep;

                /* Collision check */
                if(nextX < GRID_X_MIN || nextX > GRID_X_MAX ||
                   nextY < GRID_Y_MIN || nextY > GRID_Y_MAX ||
                   map_get(nextY, nextX)){
                    NuttyDisplay_lockLVGL();
                    lv_obj_invalidate(snake_img_obj);
                    NuttyDisplay_unlockLVGL();
                    char lose[32];
                    snprintf(lose, sizeof(lose), "Score: %d", score);
                    NuttySystemMonitor_setSystemTrayTempText(lose, 30);

                    /* Check scoreboard */
                    uint16_t scores[SCOREBOARD_SIZE];
                    scoreboard_load(scores);
                    int8_t rank = scoreboard_insert(scores, score);
                    if (rank >= 0) {
                        char newhi[32];
                        snprintf(newhi, sizeof(newhi), "NEW #%d! Score: %d", rank + 1, score);
                        NuttySystemMonitor_setSystemTrayTempText(newhi, 60);
                    }

                    vTaskDelay(pdMS_TO_TICKS(1500));
                    break;
                }

                /* Save old tail coords before any changes */
                uint8_t clearX = snake->X;
                uint8_t clearY = snake->Y;
                bool ate = (nextX == foodX && nextY == foodY);

                if(ate) {
                    uint8_t pts = snake_config.difficulty + 1;
                    score += pts;
                    char eatMsg[24];
                    snprintf(eatMsg, sizeof(eatMsg), "+%d  Score: %d", pts, score);
                    NuttySystemMonitor_setSystemTrayTempText(eatMsg, 10);
                    /* Spawn new food */
                    do{
                        foodX = rand() % 62 + 1;
                        foodY = rand() % 27 + 1;
                    }while(map_get(foodY, foodX) || (foodX == nextX && foodY == nextY));
                }else{
                    /* Remove tail */
                    map_clr(clearY, clearX);
                    _NuttySnakeQueue *old = snake;
                    snake = snake->next;
                    free(old);
                    if (!snake) snake_tail = NULL;
                    draw_block(clearX, clearY, false);
                }

                /* Save old head position for body rendering */
                uint8_t oldHX = nowX, oldHY = nowY;

                /* Add new node at tail of linked list */
                _NuttySnakeQueue *node = malloc(sizeof(_NuttySnakeQueue));
                if (!node) {
                    ESP_LOGE(TAG, "OOM allocating snake node");
                    break;
                }
                node->X = nowX;
                node->Y = nowY;
                node->next = NULL;
                if (snake_tail) snake_tail->next = node;
                snake_tail = node;
                if (!snake) snake = node;
                map_set(nowY, nowX);

                /* Draw old head as body pattern */
                draw_body(oldHX, oldHY);

                nowX = nextX;
                nowY = nextY;
                map_set(nowY, nowX);

                /* Draw new head solid */
                draw_block(nowX, nowY, true);
                draw_block(foodX, foodY, true);

                NuttyDisplay_lockLVGL();
                lv_obj_invalidate(snake_img_obj);
                NuttyDisplay_unlockLVGL();
            }
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B)){
            ticking = !ticking;
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)){
            ticking = true;
        }
        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_SELECT)) break;
        vTaskDelay(1);
    }

    /* Cleanup linked list */
    _NuttySnakeQueue *now = snake;
    while(now != NULL){
        _NuttySnakeQueue *tmp = now;
        now = now->next;
        free(tmp);
    }
    snake = NULL;
    snake_tail = NULL;
    NuttyDisplay_lockLVGL();
    if (snake_img_obj) { lv_obj_del(snake_img_obj); snake_img_obj = NULL; }
    NuttyDisplay_unlockLVGL();
    NuttyDisplay_clearUserAppArea();
}


static void option_difficulty(){
    ESP_LOGI(TAG, "Changing Difficulty...");

    lv_style_t lbl_font_big;
    lv_style_t lbl_font_nano;
    lv_style_init(&lbl_font_big);
    lv_style_init(&lbl_font_nano);
    lv_style_set_text_font(&lbl_font_big, &lv_font_unscii_16);
    lv_style_set_text_font(&lbl_font_nano, &cg_pixel_4x5_mono);

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();

    NuttyDisplay_lockLVGL();
    new_label("Change Difficulty", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_t *lbl_m_s = new_label("1", drawArea, &lbl_font_big, LV_ALIGN_TOP_MID, 0, 10);
    new_label("_", drawArea, &lbl_font_big, LV_ALIGN_TOP_MID, 0, 12);
    new_label("Press A to Confirm", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 33);
    new_label("Hold START to RESET", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 39);
    new_label("Press ARROWS to edit", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 45);
    new_label("Press SELECT back to menu", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 51);
    uint8_t counter = snake_config.difficulty;
    NuttyDisplay_unlockLVGL();
    counter = update_difficulty(lbl_m_s, counter);

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    ESP_LOGI(TAG, "StartedDifficulty");
    while(1){
        vTaskDelay(pdMS_TO_TICKS(10));
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
            set_difficulty(counter);
            NuttyDisplay_lockLVGL();
            lv_label_set_text_fmt(lbl_m_s, "%d", counter);
            NuttyDisplay_unlockLVGL();
            NuttySystemMonitor_setSystemTrayTempText("!!Difficulty changed!!", 20);
            NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_UP | NUTTYINPUT_BTN_DOWN);
            continue;
        }
        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_START)) {
            counter = 0;
            set_difficulty(counter);
            update_difficulty(lbl_m_s, counter);
            NuttySystemMonitor_setSystemTrayTempText("!!Difficulty reset!!", 20);
            NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_UP | NUTTYINPUT_BTN_DOWN);
            continue;
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
            counter = update_difficulty(lbl_m_s, counter + 1);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
            counter = update_difficulty(lbl_m_s, counter - 1);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_SELECT) || NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B)) {
            break;
        }
    }
    lv_style_reset(&lbl_font_big);
    lv_style_reset(&lbl_font_nano);
    NuttyDisplay_clearUserAppArea();
}


static void tutorial(){
    lv_style_t lbl_font_big;
    lv_style_t lbl_font_nano;
    lv_style_init(&lbl_font_big);
    lv_style_init(&lbl_font_nano);
    lv_style_set_text_font(&lbl_font_big, &lv_font_unscii_16);
    lv_style_set_text_font(&lbl_font_nano, &cg_pixel_4x5_mono);

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();

    NuttyDisplay_lockLVGL();
    new_label("Press [A]: START", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 10);
    new_label("Press [B]: STOP/RESUME", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 20);
    new_label("Press [ARROWS]: CONTROL", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 30);
    new_label("Hold [SELECT]: menu page", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 40);
    NuttyDisplay_unlockLVGL();
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while(1){
        vTaskDelay(pdMS_TO_TICKS(10));
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)){
            break;
        }
    }
    lv_style_reset(&lbl_font_big);
    lv_style_reset(&lbl_font_nano);
    NuttyDisplay_clearUserAppArea();
}


static void nutty_main(void) {
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Starting NuttySnake...");

    
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
                lv_style_reset(&text_style);
                NuttyDisplay_unlockLVGL();
                NuttyDisplay_clearUserAppArea();

                if(selectedChoice == menuChoices[0]) {
                    snake_start();
                }else if(selectedChoice == menuChoices[1]) {
                    show_scoreboard();
                }else if(selectedChoice == menuChoices[2]) {
                    option_difficulty();
                }else if(selectedChoice == menuChoices[3]) {
                    tutorial();
                }else if(selectedChoice == menuChoices[4]) {
                    exitApp = true;
                }
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    NuttyDisplay_clearUserAppArea();
    // After the app exits, we go back to menu
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttySnake = {
    .appName = "Nutty Snake",
    .appMainEntry = nutty_main,
    .appHidden = false
};

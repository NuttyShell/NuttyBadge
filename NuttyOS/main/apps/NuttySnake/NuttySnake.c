#include "NuttySnake.h"

static const char *TAG = "Snake";

static uint8_t background_png[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49,
    0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x3a, 0x08, 0x02,
    0x00, 0x00, 0x00, 0xf7, 0x0d, 0xc6, 0x8f, 0x00, 0x00, 0x00, 0xbc, 0x49, 0x44,
    0x41, 0x54, 0x78, 0x9c, 0xed, 0xdc, 0x31, 0x0e, 0x03, 0x21, 0x10, 0x04, 0x41,
    0xf6, 0xfe, 0xff, 0xe7, 0x75, 0x60, 0xe9, 0xfc, 0x03, 0x57, 0x40, 0x57, 0x30,
    0x22, 0x44, 0x48, 0x9d, 0x72, 0xce, 0x39, 0xbb, 0xdb, 0xb2, 0xf5, 0x37, 0xb8,
    0x7b, 0xe7, 0x3d, 0xe5, 0xcf, 0x66, 0x66, 0x77, 0x9f, 0xdd, 0x9d, 0x19, 0x7d,
    0x99, 0x1b, 0x7d, 0x5f, 0xbe, 0x02, 0x98, 0x0a, 0xc0, 0x2a, 0x00, 0xab, 0x00,
    0xac, 0x02, 0xb0, 0x0a, 0xc0, 0x2a, 0x00, 0xab, 0x00, 0xac, 0x02, 0xb0, 0x0a,
    0xc0, 0x2a, 0x00, 0xab, 0x00, 0xac, 0x02, 0xb0, 0x0a, 0xc0, 0x2a, 0x00, 0xab,
    0x00, 0xac, 0x02, 0xb0, 0x0a, 0xc0, 0x2a, 0x00, 0xab, 0x00, 0xac, 0x02, 0xb0,
    0x0a, 0xc0, 0x2a, 0x00, 0xab, 0x00, 0xac, 0x02, 0xb0, 0x0a, 0xc0, 0x2a, 0x00,
    0xab, 0x00, 0xac, 0x02, 0xb0, 0x0a, 0xc0, 0x2a, 0x00, 0xab, 0x00, 0xac, 0x02,
    0xb0, 0x0a, 0xc0, 0x2a, 0x00, 0xab, 0x00, 0xac, 0x02, 0xb0, 0x0a, 0xc0, 0x2a,
    0x00, 0xab, 0x00, 0xac, 0x02, 0xb0, 0x0a, 0xc0, 0x2a, 0x00, 0xab, 0x00, 0xac,
    0x02, 0xb0, 0x0a, 0xc0, 0x2a, 0x00, 0xab, 0x00, 0xac, 0x02, 0xb0, 0x0a, 0xc0,
    0x2a, 0x00, 0xab, 0x00, 0xec, 0xf7, 0xf2, 0xea, 0xc7, 0xc6, 0x76, 0xb7, 0x7f,
    0x43, 0xf5, 0x7e, 0x00, 0xc3, 0x25, 0x41, 0x3d, 0xef, 0x39, 0xf8, 0xc1, 0x00,
    0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

static uint8_t head_png[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49,
    0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02,
    0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a, 0x73, 0x00, 0x00, 0x00, 0x0b, 0x49, 0x44,
    0x41, 0x54, 0x78, 0x9c, 0x63, 0x60, 0x40, 0x06, 0x00, 0x00, 0x0e, 0x00, 0x01,
    0xa9, 0x91, 0x73, 0xb1, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
    0x42, 0x60, 0x82
};

static uint8_t tail_png[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49,
    0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02,
    0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a, 0x73, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44,
    0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xff, 0xff, 0x3f, 0x03, 0x04, 0xfc, 0xff,
    0xff, 0x1f, 0x00, 0x29, 0xe4, 0x05, 0xfb, 0x5a, 0xaa, 0x87, 0xae, 0x00, 0x00,
    0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

static const char *menuChoices[] = {
    "Start",
    "Edit Difficulty",
    "Tutorial",
    "< Back"
};

static _NuttySnakeConfig snake_config = {
    .difficulty = 5,
    .speed = 250
};

static _NuttySnakeQueue* snake;

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
    snake_config.speed = 500 - min(difficulty * 50, 450);
    ESP_LOGI(TAG, "Change difficulty->%d speed->%d", snake_config.difficulty, snake_config.speed);
}


static void snake_start(){
    ESP_LOGI(TAG, "Game Start with speed: %d with count %d", snake_config.speed, snake_config.speed/5);
    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    NuttySystemMonitor_setSystemTrayTempText("!!Game Started!!", 30);
    lv_img_dsc_t background_img = NuttyDisplay_getPNGDsc(background_png, 248);
    lv_img_dsc_t head_img = NuttyDisplay_getPNGDsc(head_png, 68);
    lv_img_dsc_t tail_img = NuttyDisplay_getPNGDsc(tail_png, 75);

    NuttyDisplay_lockLVGL();

    NuttyDisplay_showPNGWithWHXY(&background_img, drawArea, 0, 1);
    lv_obj_t *tail = NuttyDisplay_showPNGWithWHXY(&tail_img, drawArea, 62, 33);
    lv_obj_t *head = NuttyDisplay_showPNGWithWHXY(&head_img, drawArea, 62, 31);
    srand(esp_timer_get_time());
    uint8_t foodX = rand() % 61 + 1, foodY = rand() % 27 + 1;
    lv_obj_t *food = NuttyDisplay_showPNGWithWHXY(&head_img, drawArea, foodX * 2, foodY * 2 + 1);
    NuttyDisplay_unlockLVGL();

    snake = malloc(sizeof(_NuttySnakeQueue));
    if (!snake) {
        NuttyDisplay_unlockLVGL();
        NuttySystemMonitor_setSystemTrayTempText("!!Out of Memory!!", 30);
        NuttyDisplay_clearUserAppArea();
        return;
    }
    snake->current = tail;
    snake->next = NULL;
    snake->X = 31;
    snake->Y = 15;

    uint8_t counter = 0, nowX = 31, nowY = 15, score = 0;
    enum _NuttySnakeDIRECTION nextStep = UP;
    bool ticking = true, snake_map[28][63] = {0,};
    snake_map[nowY][nowX] = true;
    snake_map[nowY + 1][nowX] = true;
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while(true) {
        if(ticking){
            counter++;
            if(counter == snake_config.speed / 5){
                counter = 0;
                _NuttySnakeQueue *tmp, *now = snake;
                while(now->next != NULL){
                    now = now->next;
                }
                now->next = malloc(sizeof(_NuttySnakeQueue));
                if (!now->next) {
                    NuttyDisplay_unlockLVGL();
                    NuttySystemMonitor_setSystemTrayTempText("!!Out of Memory!!", 30);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    break;
                }

                NuttyDisplay_lockLVGL();
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

                lv_obj_align(head, LV_ALIGN_TOP_LEFT, nextX * 2, nextY * 2 + 1);
                now->next->current = NuttyDisplay_showPNGWithWHXY(&tail_img, drawArea, nowX * 2, nowY * 2 + 1);
                now->next->X = nowX;
                now->next->Y = nowY;
                now->next->next = NULL;
                if(nextX < 1 || nextX > 62 || nextY < 1 || nextY > 27 || snake_map[nextY][nextX]){
                    free(now->next);
                    now->next = NULL;
                    NuttyDisplay_unlockLVGL();
                    char lose[32];
                    memset(lose, 0x00, sizeof(lose));
                    sprintf(lose, "!!You Lose!!Score: %d!!", score);
                    NuttySystemMonitor_setSystemTrayTempText(lose, 20);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    break;
                }
                nowX = nextX;
                nowY = nextY;
                snake_map[nowY][nowX] = true;
                if(nowX == foodX && nowY == foodY){
                    do{
                        foodX = rand() % 61 + 1, foodY = rand() % 27 + 1;
                    }while(snake_map[foodY][foodX]);
                    lv_obj_align(food, LV_ALIGN_TOP_LEFT, foodX * 2, foodY * 2 + 1);
                    score++;
                }else{
                    lv_obj_del(snake->current);
                    snake_map[snake->Y][snake->X] = false;
                    tmp = snake;
                    snake = snake->next;
                    free(tmp);
                }

                ESP_LOGI(TAG, "xy %d %d", nowX, nowY);

                NuttyDisplay_unlockLVGL();
            }
            if(nextStep != DOWN && NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)){
                nextStep = UP;
            }
            if(nextStep != UP && NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)){
                nextStep = DOWN;
            }
            if(nextStep != RIGHT && NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_LEFT)){
                nextStep = LEFT;
            }
            if(nextStep != LEFT && NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_RIGHT)){
                nextStep = RIGHT;
            }
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B)){
            ticking = !ticking;
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)){
            ticking = true;
        }
        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_SELECT)) break;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    _NuttySnakeQueue *tmp, *now = snake;
    while(now != NULL){
        tmp = now;
        now = now->next;
        free(tmp);
    }
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
        }
        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_START)) {
            counter = 0;
            set_difficulty(counter);
            update_difficulty(lbl_m_s, counter);
            NuttySystemMonitor_setSystemTrayTempText("!!Difficulty reset!!", 20);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
            counter = update_difficulty(lbl_m_s, counter + 1);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
            update_difficulty(lbl_m_s, counter - 1);
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
    // After the app exits, we go back to menu
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttySnake = {
    .appName = "Nutty Snake",
    .appMainEntry = nutty_main,
    .appHidden = false
};

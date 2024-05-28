#include "NuttySnake.h"

static const char *TAG = "Snake";

static const char *menuChoices[] = {
    "Start",
    "Edit Difficulty",
    "Tutorial",
    "< Back"
};

static _NuttySnakeConfig snake_config = {
    .difficulty = 5,
    .seed = 20240527,
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

static void update_difficulty(lv_obj_t* lbl_m_s, uint8_t difficulty){
    ESP_LOGI(TAG, "Current: %d", difficulty);
    NuttyDisplay_lockLVGL();
    lv_label_set_text_fmt(lbl_m_s, "%d", difficulty % 10);
    NuttyDisplay_unlockLVGL();
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
    NuttySystemMonitor_setSystemTrayTempText("!!Game Started!!", 2);
    lv_img_dsc_t background_img = NuttyDisplay_getPNGDsc(background_png, 248);
    lv_img_dsc_t head_img = NuttyDisplay_getPNGDsc(head_png, 68);
    lv_img_dsc_t tail_img = NuttyDisplay_getPNGDsc(tail_png, 75);

    NuttyDisplay_lockLVGL();

    NuttyDisplay_showPNGWithWHXY(&background_img, drawArea, 0, 1);
    lv_obj_t *head = NuttyDisplay_showPNGWithWHXY(&head_img, drawArea, 62, 31);
    lv_obj_t *tail = NuttyDisplay_showPNGWithWHXY(&tail_img, drawArea, 62, 33);
    srand(snake_config.seed);
    uint8_t foodX = rand() % 61 + 1, foodY = rand() % 27 + 1;
    lv_obj_t *food = NuttyDisplay_showPNGWithWHXY(&head_img, drawArea, foodX * 2, foodY * 2 + 1);
    NuttyDisplay_unlockLVGL();

    snake = malloc(sizeof(_NuttySnakeQueue));
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

                NuttyDisplay_lockLVGL();

                now->next->current = NuttyDisplay_showPNGWithWHXY(&tail_img, drawArea, nowX * 2, nowY * 2 + 1);
                now->next->X = nowX;
                now->next->Y = nowY;
                now->next->next = NULL;
                
                if(nextStep == UP){
                    nowY--;
                }else if(nextStep == DOWN){
                    nowY++;
                }else if(nextStep == LEFT){
                    nowX--;
                }else if(nextStep == RIGHT){
                    nowX++;
                }
                if(nowX < 1 || nowX > 62 || nowY < 1 || nowY > 27 || snake_map[nowY][nowX]){
                    NuttyDisplay_unlockLVGL();
                    char lose[26];
                    sprintf(lose, "!!You Lose!!Score: %d!!", score);
                    NuttySystemMonitor_setSystemTrayTempText(lose, 2);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    break;
                }
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

                lv_obj_align(head, LV_ALIGN_TOP_LEFT, nowX * 2, nowY * 2 + 1);
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
    NuttyDisplay_clearUserAppArea();
}


static void option_difficulty(){
    ESP_LOGI(TAG, "ChangingDifficulty...");

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
    new_label("Hold B to RESET", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 39);
    new_label("Press ARROWS to edit", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 45);
    new_label("Press SELECT back to menu", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 51);
    uint8_t counter = snake_config.difficulty;
    NuttyDisplay_unlockLVGL();
    update_difficulty(lbl_m_s, counter);

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    ESP_LOGI(TAG, "StartedDifficulty");
    while(1){
        vTaskDelay(pdMS_TO_TICKS(10));
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
            set_difficulty(counter);
            update_difficulty(lbl_m_s, counter);
            NuttySystemMonitor_setSystemTrayTempText("!!Difficulty changed!!", 2);
        }
        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_B)) {
            counter = 0;
            set_difficulty(counter);
            update_difficulty(lbl_m_s, counter);
            NuttySystemMonitor_setSystemTrayTempText("!!Difficulty reset!!", 2);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
            counter = (counter + 1 + 10) % 10;
            update_difficulty(lbl_m_s, counter);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
            counter = (counter - 1 + 10) % 10;
            update_difficulty(lbl_m_s, counter);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_SELECT)) {
            break;
        }
    }
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
    lv_obj_t * lbl1 = new_label("Press A to START", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_t * lbl2 = new_label("Press B to STOP/RESUME", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_t * lbl3 = new_label("Press ARROWS to CONTROL", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_t * lbl4 = new_label("Hold SELECT back to menu", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 40);
    NuttyDisplay_unlockLVGL();
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while(1){
        vTaskDelay(pdMS_TO_TICKS(10));
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)){
            break;
        }
    }
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

#include "NuttyStopwatch.h"


static const char *TAG = "Timer";

static const char *menuChoices[] = {
    "Stopwatch",
    "Countdown",
    "< Back"
};


static const char *underline[] = {
    "_    ",
    " _   ",
    "   _ ",
    "    _",
    "     "
};


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


static void update_time(uint32_t current, lv_obj_t* lbl_m_s, lv_obj_t* lbl_ms){
    ESP_LOGI(TAG, "Current: %ld", current);
    NuttyDisplay_lockLVGL();
    lv_label_set_text_fmt(lbl_m_s, "%02ld:%02ld", (current / 600) % 100, (current / 10) % 60);
    if(lbl_ms != NULL){
        lv_label_set_text_fmt(lbl_ms, ".%ld", current % 10);
    }
    NuttyDisplay_unlockLVGL();
}


static void select_time(lv_obj_t *lbl_underline, int8_t position){
    NuttyDisplay_lockLVGL();
    lv_label_set_text(lbl_underline, underline[position]);
    NuttyDisplay_unlockLVGL();
}


static void stopwatch(){
    ESP_LOGI(TAG, "Starting StopWatch...");

    lv_style_t lbl_font_big;
    lv_style_t lbl_font_nano;
    lv_style_init(&lbl_font_big);
    lv_style_init(&lbl_font_nano);
    lv_style_set_text_font(&lbl_font_big, &lv_font_unscii_16);
    lv_style_set_text_font(&lbl_font_nano, &cg_pixel_4x5_mono);

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();

    NuttyDisplay_lockLVGL();
    new_label("Stopwatch", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_t *lbl_m_s = new_label("00:00", drawArea, &lbl_font_big, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_t *lbl_ms = new_label(".0", drawArea, &lbl_font_nano, LV_ALIGN_TOP_RIGHT, -15, 22);
    new_label("Press A to START/STOP", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 34);
    new_label("Hold B to RESET", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 40);
    new_label("Hold SELECT back to menu", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 46);
    NuttyDisplay_unlockLVGL();

    uint32_t start_time = 0, cnt_time = 0;
    bool timer_enable = false;
    uint8_t counter = 0;
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    ESP_LOGI(TAG, "Started NuttyStopWatch");
    while(1){
        vTaskDelay(pdMS_TO_TICKS(10));
        if(timer_enable){
            if(counter == 10){
                counter = 0;
                uint32_t current = (cnt_time + (uint32_t)esp_timer_get_time() - start_time) / 100000;
                update_time(current, lbl_m_s, lbl_ms);
            }
            else{
                counter++;
            }
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
            if(!timer_enable){
                start_time = (uint32_t)esp_timer_get_time();
                counter = 10;
            }else{
                cnt_time += (uint32_t)esp_timer_get_time() - start_time;
            }
            timer_enable = !timer_enable;
        }
        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_B)) {
            timer_enable = false;
            cnt_time = 0;
            update_time(0, lbl_m_s, lbl_ms);
        }
        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_SELECT)) {
            NuttySystemMonitor_setSystemTrayTempText("!!Timer Break!!", 2);
            break;
        }
    }
    NuttyDisplay_clearUserAppArea();
}


static void countdown(){
    ESP_LOGI(TAG, "Starting Countdown...");

    lv_style_t lbl_font_big;
    lv_style_t lbl_font_nano;
    lv_style_init(&lbl_font_big);
    lv_style_init(&lbl_font_nano);
    lv_style_set_text_font(&lbl_font_big, &lv_font_unscii_16);
    lv_style_set_text_font(&lbl_font_nano, &cg_pixel_4x5_mono);

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();

    NuttyDisplay_lockLVGL();
    new_label("CountDown", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_t *lbl_m_s = new_label("00:00", drawArea, &lbl_font_big, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_t *lbl_underline = new_label("_    ", drawArea, &lbl_font_big, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_t *lbl_ms = new_label(".0", drawArea, &lbl_font_nano, LV_ALIGN_TOP_RIGHT, -15, 22);
    new_label("Press A to START/STOP", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 33);
    new_label("Hold B to RESET", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 39);
    new_label("Press ARROWS to edit time", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 45);
    new_label("Hold SELECT back to menu", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 51);
    NuttyDisplay_unlockLVGL();

    uint32_t start_time = 0, cnt_time = 0;
    bool timer_enable = false, end = false, flash = false;
    uint16_t counter = 0;
    int8_t underline_location = 0;
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    ESP_LOGI(TAG, "Started Countdown");
    while(1){
        vTaskDelay(pdMS_TO_TICKS(10));
        if(timer_enable){
            if(counter == 10){
                counter = 0;
                uint32_t current = cnt_time - ((uint32_t)esp_timer_get_time() - start_time) / 100000;
                if(current <= 0){
                    timer_enable = false;
                    end = true;
                    counter = 0;
                }
                update_time(current, lbl_m_s, lbl_ms);
            }
            else{
                counter++;
            }
        }
        if(end){
            counter++;
            if(counter > 1000){
                counter = 10;
                end = false;
                flash = false;
                select_time(lbl_underline, 0);
            }if(counter % 50 == 0){
                flash = !flash;
            }
            NuttyDisplay_setLCDBacklight(30 * flash);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
            if(!timer_enable){
                start_time = (uint32_t)esp_timer_get_time();
                counter = 10;
                select_time(lbl_underline, 4);
            }else{
                cnt_time -= ((uint32_t)esp_timer_get_time() - start_time) / 100000;
            }
            timer_enable = !timer_enable;
        }
        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_B)) {
            timer_enable = false;
            end = false;
            cnt_time = 0;
            NuttyDisplay_setLCDBacklight(30);
            update_time(0, lbl_m_s, lbl_ms);
            select_time(lbl_underline, 0);
        }
        if(!timer_enable && !end){
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
                if(underline_location == 0){
                    cnt_time += 6000;
                }else if(underline_location == 1){
                    cnt_time += 600;
                }else if(underline_location == 2){
                    cnt_time += 100;
                }else if(underline_location == 3){
                    cnt_time += 10;
                }
                cnt_time %= 60000;
                update_time(cnt_time, lbl_m_s, lbl_ms);
            }
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
                if(underline_location == 0){
                    cnt_time -= 6000;
                }else if(underline_location == 1){
                    cnt_time -= 600;
                }else if(underline_location == 2){
                    cnt_time -= 100;
                }else if(underline_location == 3){
                    cnt_time -= 10;
                }
                cnt_time %= 60000;
                update_time(cnt_time, lbl_m_s, lbl_ms);
            }
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_LEFT)) {
                underline_location--;
                underline_location+=4;
                underline_location %=4;
                select_time(lbl_underline, underline_location);
            }
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_RIGHT)) {
                underline_location++;
                underline_location %=4;
                select_time(lbl_underline, underline_location);
            }
        }
        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_SELECT)) {
            NuttySystemMonitor_setSystemTrayTempText("!!Timer Break!!", 2);
            break;
        }
    }
    NuttyDisplay_clearUserAppArea();
}


static void nutty_main(void) {
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Starting NuttyStopWatch...");

    
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
        lv_style_set_text_font(&text_style, &lv_font_montserrat_18);
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
                    stopwatch();
                }else if(selectedChoice == menuChoices[1]) {
                    countdown();
                }else if(selectedChoice == menuChoices[2]) {
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

NuttyAppDefinition NuttyStopwatch = {
    .appName = "Stopwatch",
    .appMainEntry = nutty_main,
    .appHidden = false
};

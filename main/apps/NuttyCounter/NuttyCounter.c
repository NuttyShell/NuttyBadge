#include "NuttyCounter.h"

static const char *TAG = "Counter";


static lv_obj_t *new_label(char *text, lv_obj_t* drawArea, lv_style_t* style, lv_align_t aligned, lv_coord_t x, lv_coord_t y){
    lv_obj_t *lbl = lv_label_create(drawArea);
    lv_label_set_text(lbl, text);
    lv_obj_add_style(lbl, style, LV_PART_MAIN);
    lv_obj_align(lbl, aligned, x, y);
    return lbl;
}


static void nutty_main(void) {
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

    ESP_LOGI(TAG, "Starting NuttyCounter...");


    lv_style_t lbl_font_big, lbl_font_nano;
    lv_style_init(&lbl_font_big);
    lv_style_init(&lbl_font_nano);
    lv_style_set_text_font(&lbl_font_big, &lv_font_montserrat_18);
    lv_style_set_text_font(&lbl_font_nano, &cg_pixel_4x5_mono);

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    
    NuttyDisplay_lockLVGL();
    lv_obj_t *lblCount = lv_label_create(drawArea);
    lv_label_set_text(lblCount, "Count: 0");
    lv_obj_add_style(lblCount, &lbl_font_big, LV_PART_MAIN);
    lv_obj_align(lblCount, LV_ALIGN_TOP_MID, 0, 8);
    new_label("Press [A]: Increase Count", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 34);
    new_label("Hold [B]: RESET", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 40);
    new_label("HOLD [SELECT]: Exit", drawArea, &lbl_font_nano, LV_ALIGN_TOP_MID, 0, 46);
    NuttyDisplay_unlockLVGL();

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    uint32_t count=0;
    while(true) {
        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_SELECT)) break;
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
            count += 1;
            lv_label_set_text_fmt(lblCount, "Count: %ld", count);
        }
        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_B)) {
            count = 0;
            lv_label_set_text_fmt(lblCount, "Count: %ld", count);
            NuttySystemMonitor_setSystemTrayTempText("!!Count Resetted!!", 2);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    NuttyDisplay_clearUserAppArea();

    // After the app exits, we go back to menu
    NuttyApps_launchAppByIndex(0);


}

NuttyAppDefinition NuttyCounter = {
    .appName = "Counter",
    .appMainEntry = nutty_main,
    .appHidden = false
};

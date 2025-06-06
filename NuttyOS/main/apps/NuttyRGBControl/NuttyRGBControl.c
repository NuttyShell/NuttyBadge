#include "NuttyRGBControl.h"

static const char *TAG = "RGB Controller";

static void nutty_main(void) {

    ESP_LOGI(TAG, "Starting RGB Controller...");

    lv_style_t text_style;
    lv_style_init(&text_style);
    lv_style_set_text_font(&text_style, &lv_font_montserrat_10);
    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    NuttyDisplay_lockLVGL();
    lv_obj_t *lbl = lv_label_create(drawArea);
    lv_label_set_text(lbl, "This app is yet to be implemented...\nStay tuned on our GitHub!");
    lv_obj_add_style(lbl, &text_style, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_anim_speed(lbl, 4, LV_PART_MAIN);
    lv_obj_set_width(lbl, 150);
    NuttyDisplay_unlockLVGL();

    // Not using any LVGL input device here, as we are not using LVGL widgets for navigation
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while(true) {
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)) break;
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

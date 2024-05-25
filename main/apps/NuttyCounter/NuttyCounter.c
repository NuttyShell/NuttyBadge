#include "NuttyCounter.h"

static const char *TAG = "Counter";

static void nutty_main(void) {
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

    ESP_LOGI(TAG, "Starting NuttyCounter...");

    lv_style_t lbl_counter_style, lbl_instruction_style;
    lv_style_init(&lbl_counter_style);
    lv_style_init(&lbl_instruction_style);
    lv_style_set_text_font(&lbl_counter_style, &lv_font_montserrat_16);
    lv_style_set_text_font(&lbl_instruction_style, &lv_font_montserrat_8);
    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    
    NuttyDisplay_lockLVGL();
    lv_obj_t *lblCount = lv_label_create(drawArea);
    lv_obj_t *lblInstruction = lv_label_create(drawArea);
    lv_label_set_text(lblCount, "Count: 0");
    lv_label_set_text(lblInstruction, "            Press [A]: Increase Count\nLong Press [START]: Reset\n            Press [L]: Exit");
    lv_obj_add_style(lblCount, &lbl_counter_style, LV_PART_MAIN);
    lv_obj_add_style(lblInstruction, &lbl_instruction_style, LV_PART_MAIN);
    lv_obj_align(lblCount, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_align(lblInstruction, LV_ALIGN_TOP_LEFT, 0, 28);
    NuttyDisplay_unlockLVGL();

    NuttyInput_clearButtonHoldState(0x1ff);
    uint32_t count=0;
    while(true) {
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_LEFT)) break;
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
            count += 1;
            lv_label_set_text_fmt(lblCount, "Count: %ld", count);
        }
        if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_START)) {
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

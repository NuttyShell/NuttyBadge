#include "NuttyTest.h"


static const char *TAG = "Test";


extern uint8_t chipi_48k_start[] asm("_binary_chipi_mono8b48k_bin_start");
extern uint8_t chipi_48k_end[]   asm("_binary_chipi_mono8b48k_bin_end");


extern uint8_t ussr_48k_start[] asm("_binary_ussr_48k_bin_start");
extern uint8_t ussr_48k_end[]   asm("_binary_ussr_48k_bin_end");

static void nutty_main(void) {
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

    ESP_LOGI(TAG, "Starting NuttyTest...");

    NuttySystemMonitor_hideSystemTray();
    NuttyDisplay_clearWholeScreen();

    lv_style_t lbl_text_style, btn_pressed_style, btn_lbl_text_style;
    lv_style_init(&lbl_text_style);
    lv_style_set_text_font(&lbl_text_style, &lv_font_montserrat_10);

    NuttyDisplay_lockLVGL();
    lv_obj_t *lblTestCategory = lv_label_create(lv_scr_act());
    lv_obj_t *lblTestItem = lv_label_create(lv_scr_act());
    lv_obj_t *lblTestResult = lv_label_create(lv_scr_act());
    
    lv_label_set_text(lblTestCategory, "Testing: ??");
    lv_obj_add_style(lblTestCategory, &lbl_text_style, LV_PART_MAIN);
    lv_obj_align(lblTestCategory, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(lblTestItem, "Item: ??");
    lv_obj_add_style(lblTestItem, &lbl_text_style, LV_PART_MAIN);
    lv_obj_align(lblTestItem, LV_ALIGN_TOP_LEFT, 0, 10);
    lv_label_set_text(lblTestResult, "Result: ??");
    lv_obj_add_style(lblTestResult, &lbl_text_style, LV_PART_MAIN);
    lv_obj_align(lblTestResult, LV_ALIGN_TOP_LEFT, 0, 20);
    NuttyDisplay_unlockLVGL();

    bool exit=false;
    while(!exit) {
        // Test Buttons
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestCategory, "Testing: Buttons");
        lv_label_set_text(lblTestResult, "Result: WAITING USER...");
        lv_label_set_text(lblTestItem, "Item: PRESS [UP]");
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) vTaskDelay(pdMS_TO_TICKS(10));
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: PRESS [DOWN]");
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) vTaskDelay(pdMS_TO_TICKS(10));
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: PRESS [LEFT]");
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_LEFT)) vTaskDelay(pdMS_TO_TICKS(10));
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: PRESS [RIGHT]");
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_RIGHT)) vTaskDelay(pdMS_TO_TICKS(10));
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: PRESS [USRDEF]");
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_USRDEF)) vTaskDelay(pdMS_TO_TICKS(10));
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: PRESS [SELECT]");
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_SELECT)) vTaskDelay(pdMS_TO_TICKS(10));
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: PRESS [START]");
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_START)) vTaskDelay(pdMS_TO_TICKS(10));
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: PRESS [A]");
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) vTaskDelay(pdMS_TO_TICKS(10));
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: PRESS [B]");
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B)) vTaskDelay(pdMS_TO_TICKS(10));
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: -");
        lv_label_set_text(lblTestResult, "Result: PASSED.\nPress [A] for next item.");
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) vTaskDelay(pdMS_TO_TICKS(10));

        // Test IR
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestCategory, "Testing: IR Tx&Rx");
        lv_label_set_text(lblTestResult, "Result: TESTING...");
        lv_label_set_text(lblTestItem, "Item: Tx&Rx");
        NuttyDisplay_unlockLVGL();
        NuttyIR_Init();
        uint16_t addr, cmd;
        uint8_t status;
        bool irTestPass = false;
        for(uint i=0; i<10; i++) {
            NuttyIR_TxNECext(0x1234, 0x4321);
            vTaskDelay(pdMS_TO_TICKS(20));
            NuttyIR_getLatestRecvResult(&addr, &cmd, &status);
            if(addr == 0x1234 && cmd == 0x4321 && (status == 2 || status == 4)) {
                irTestPass = true;
                break;
            }
        }
        NuttyIR_Deinit();
        NuttyDisplay_lockLVGL();
        if (irTestPass) {
            lv_label_set_text(lblTestResult, "Result: PASSED.\nPress [A] for next item.");
        }else{
            lv_label_set_text(lblTestResult, "Result: FAILURE!!!\nPress [A] for next item.");
        }
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) vTaskDelay(pdMS_TO_TICKS(10));

        // Test RGB
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestCategory, "Testing: RGB");
        lv_label_set_text(lblTestResult, "Result: TESTING...");
        lv_label_set_text(lblTestItem, "Item: RGB");
        NuttyDisplay_unlockLVGL();
        NuttyRGB_Init();
        uint8_t bulb;
        NuttyRGB_SetAllRGBAndDisplay(0, 0, 0);
        for(bulb=0; bulb<RGB_BULBS; bulb++) {
            NuttyRGB_SetRGBAndDisplay(bulb, 0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(350));
            NuttyDisplay_lockLVGL();
            lv_label_set_text_fmt(lblTestItem, "Item: RGB[%d] - Red", bulb);
            NuttyDisplay_unlockLVGL();
            NuttyRGB_SetRGBAndDisplay(bulb, 255, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(350));
            NuttyDisplay_lockLVGL();
            lv_label_set_text_fmt(lblTestItem, "Item: RGB[%d] - Green", bulb);
            NuttyDisplay_unlockLVGL();
            NuttyRGB_SetRGBAndDisplay(bulb, 0, 255, 0);
            vTaskDelay(pdMS_TO_TICKS(350));
            NuttyDisplay_lockLVGL();
            lv_label_set_text_fmt(lblTestItem, "Item: RGB[%d] - Blue", bulb);
            NuttyDisplay_unlockLVGL();
            NuttyRGB_SetRGBAndDisplay(bulb, 0, 0, 255);
            vTaskDelay(pdMS_TO_TICKS(350));
        }
        NuttyRGB_SetAllRGBAndDisplay(0, 0, 0);
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: RGB Animation");
        NuttyDisplay_unlockLVGL();
        vTaskDelay(pdMS_TO_TICKS(350));
        ESP_LOGI(TAG, "-");

        NuttyRGBAnimationSequence rgbSeq[6];
        memset(rgbSeq, 0x00, sizeof(NuttyRGBAnimationSequence)*6);
        rgbSeq[0].g[0]=255; rgbSeq[0].b[0]=255; rgbSeq[0].r[0]=0; rgbSeq[0].durationMs=500;
        rgbSeq[1].g[1]=255; rgbSeq[1].b[1]=255; rgbSeq[1].r[1]=0; rgbSeq[1].durationMs=500;
        rgbSeq[2].g[2]=255; rgbSeq[2].b[2]=255; rgbSeq[2].r[2]=0; rgbSeq[2].durationMs=500;
        rgbSeq[3].g[0]=255; rgbSeq[3].b[0]=255; rgbSeq[3].r[0]=255; rgbSeq[3].durationMs=500;
        rgbSeq[4].g[1]=255; rgbSeq[4].b[1]=255; rgbSeq[4].r[1]=255; rgbSeq[4].durationMs=500;
        rgbSeq[5].g[2]=255; rgbSeq[5].b[2]=255; rgbSeq[5].r[2]=255; rgbSeq[5].durationMs=500;
        NuttyRGB_SetAnimationSequences(rgbSeq, 6);
        NuttyRGB_StartAnimation();
        vTaskDelay(pdMS_TO_TICKS(3000));
        NuttyRGB_SetAllRGBAndDisplay(0, 0, 0);
        NuttyRGB_StopAnimation();
        NuttyRGB_Deinit();
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: -");
        lv_label_set_text(lblTestResult, "Result: User Observe?\nPress [A] for next item.");
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) vTaskDelay(pdMS_TO_TICKS(10));

        // Test LCD Backlight
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestCategory, "Testing: LCD Backlight");
        lv_label_set_text(lblTestResult, "Result: TESTING...");
        lv_label_set_text(lblTestItem, "Item: BL");
        NuttyDisplay_unlockLVGL();
        for(uint8_t i=0; i<100; i++) {
            NuttyDisplay_lockLVGL();
            lv_label_set_text_fmt(lblTestItem, "Item: Backlight %d%%...", i);
            NuttyDisplay_unlockLVGL();
            NuttyDisplay_setLCDBacklight(i);
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        NuttyDisplay_setLCDBacklight(100);
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: -");
        lv_label_set_text(lblTestResult, "Result: User Observe?\nPress [A] for next item.");
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) vTaskDelay(pdMS_TO_TICKS(10));

        // Test SD Card
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestCategory, "Testing: SD Card");
        lv_label_set_text(lblTestResult, "Result: WAITING USER...");
        lv_label_set_text(lblTestItem, "Item: Pls unplug SD");
        NuttyDisplay_unlockLVGL();
    
        while(NuttyStorage_isSDCardInserted()) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestItem, "Item: Pls plug SD");
        NuttyDisplay_unlockLVGL();
        while(!NuttyStorage_isSDCardInserted()) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestResult, "Result: TESTING...");
        lv_label_set_text(lblTestItem, "Item: Waiting SD...");
        NuttyDisplay_unlockLVGL();
        while(!NuttyStorage_isSDCardPersist()) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        uint64_t sdMB;
        NuttyStorage_getSDCardSizeMB(&sdMB);
        lv_label_set_text(lblTestItem, "Item: -");
        if(sdMB == 0) {
            lv_label_set_text(lblTestResult, "Result: FAILURE!!!\nPress [A] for next item.");
        }else{
            lv_label_set_text_fmt(lblTestResult, "Result: PASSED\nSD Sz=%lluMB.\nPress [A] for next item.", sdMB);
        }
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) vTaskDelay(pdMS_TO_TICKS(10));

        vTaskDelay(pdMS_TO_TICKS(500));
        // Test Battery ADC
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestCategory, "Testing: Battery ADC");
        lv_label_set_text(lblTestResult, "Result: User Observe?\nPress [A] for next item.");
        NuttyDisplay_unlockLVGL();
        while(true) {
            NuttyDisplay_lockLVGL();
            lv_label_set_text_fmt(lblTestItem, "Item: ADC=%d", NuttySystemMonitor_getBatteryVoltage());
            NuttyDisplay_unlockLVGL();
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Audio
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestCategory, "Testing: Audio");
        lv_label_set_text(lblTestResult, "Result: User Observe?\nPress [A] for next item.");
        lv_label_set_text(lblTestItem, "Item: Listen");
        NuttyDisplay_unlockLVGL();
        bool playFinished;
        NuttyAudio_FinshedPlayedBuffer(&playFinished);
        if(playFinished) {
            NuttyAudio_PlayBuffer(chipi_48k_start, 634468);
        }
        NuttyAudio_SetVolume(2);
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) vTaskDelay(pdMS_TO_TICKS(10));
        NuttyAudio_StopPlaying();
        ESP_LOGI(TAG, "Stopped playing audio...");

        // Screen Flush
        NuttyDisplay_lockLVGL();
        lv_label_set_text(lblTestCategory, "Testing: Screen Dead Px");
        lv_label_set_text(lblTestResult, "Result: -");
        lv_label_set_text(lblTestItem, "Item: -");
        NuttyDisplay_unlockLVGL();
        vTaskDelay(pdMS_TO_TICKS(2000));
        lv_style_t line_style;
        lv_style_init(&line_style);
        lv_style_set_line_width(&line_style, 1);
        lv_style_set_line_color(&line_style, lv_color_black());

        for(uint8_t i=0; i<64; i++) {
            NuttyDisplay_lockLVGL();
            lv_obj_t *line = lv_line_create(lv_scr_act());
            lv_point_t line_points[2];
            line_points[0].x=0; line_points[0].y=32;
            line_points[1].x=128; line_points[1].y=32;
            lv_line_set_points(line, line_points, 2);
            lv_style_set_line_width(&line_style, i);
            lv_obj_add_style(line, &line_style, LV_PART_MAIN);
            NuttyDisplay_unlockLVGL();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) vTaskDelay(pdMS_TO_TICKS(10));

        // End
        NuttyDisplay_clearWholeScreen();
        NuttyDisplay_lockLVGL();
        lv_obj_t *lblEnd = lv_label_create(lv_scr_act());
        lv_label_set_text(lblEnd, "Finished.\nPress [A] to exit.");
        lv_obj_add_style(lblEnd, &lbl_text_style, LV_PART_MAIN);
        lv_obj_align(lblEnd, LV_ALIGN_TOP_LEFT, 0, 0);
        NuttyDisplay_unlockLVGL();
        while(!NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) vTaskDelay(pdMS_TO_TICKS(10));
        exit = true;
    }

    NuttyDisplay_clearWholeScreen();
    NuttySystemMonitor_showSystemTray();
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyTest = {
    .appName = "Function Test",
    .appMainEntry = nutty_main,
    .appHidden = false
};

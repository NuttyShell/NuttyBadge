#include "NuttyAbout.h"

static const char *TAG = "About";

extern const uint8_t about_start[] asm("_binary_about_png_start");
extern const uint8_t about_end[]   asm("_binary_about_png_end");

static void nutty_main(void) {
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Starting NuttyAbout...");

    
    NuttySystemMonitor_hideSystemTray();
    NuttyDisplay_clearWholeScreen();
    NuttyDisplay_showPNG((uint8_t *)about_start, about_end - about_start);

    
    lv_style_t text_style;
    lv_style_init(&text_style);
    lv_style_set_text_font(&text_style, &cg_pixel_4x5_mono);

    NuttyDisplay_lockLVGL();
    lv_obj_t *lbl = lv_label_create(lv_scr_act());
    lv_obj_t *lbl2 = lv_label_create(lv_scr_act());
    lv_obj_t *lbl3 = lv_label_create(lv_scr_act());
    lv_label_set_text(lbl, "Build Date:");
    lv_obj_add_style(lbl, &text_style, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_LEFT, 58, -9);

    lv_label_set_text_fmt(lbl2, "%04d-%02d-%02d %02d:%02d:%02d", BUILD_YEAR, BUILD_MONTH, BUILD_DAY, BUILD_HOUR, BUILD_MIN, BUILD_SEC);
    lv_obj_add_style(lbl2, &text_style, LV_PART_MAIN);
    lv_obj_align(lbl2, LV_ALIGN_BOTTOM_LEFT, 58, -3);
    lv_label_set_long_mode(lbl2, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_anim_speed(lbl2, 2, LV_PART_MAIN);
    lv_obj_set_width(lbl2, 58);

    lv_label_set_text(lbl3, NUTTYOS_VERSION);
    lv_obj_add_style(lbl3, &text_style, LV_PART_MAIN);
    lv_obj_align(lbl3, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_width(lbl3, 25);
    NuttyDisplay_unlockLVGL();

    // Not using any LVGL input device here, as we are not using LVGL widgets for navigation
    NuttyInput_clearButtonHoldState(0x1ff);
    while(true) {
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(0x1ff)) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Cleanup
    NuttyDisplay_clearWholeScreen();
    NuttySystemMonitor_showSystemTray();
    NuttyApps_launchAppByIndex(0);


}

NuttyAppDefinition NuttyAbout = {
    .appName = "About",
    .appMainEntry = nutty_main,
    .appHidden = false
};

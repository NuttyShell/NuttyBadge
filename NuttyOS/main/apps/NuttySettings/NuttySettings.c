#include "NuttySettings.h"

static char *TAG = "Settings";

static const char *menuChoices[] = {
    "Display", // Backlight, Backlight Timeout
    "Storage", // SD Info, Format SD, Benchmark SD
    "System"   // Function Test, "UDEF Button"
};

enum {
    LV_MENU_ITEM_BUILDER_VARIANT_1,
    LV_MENU_ITEM_BUILDER_VARIANT_2
};
typedef uint8_t lv_menu_builder_variant_t;

static lv_obj_t * create_text(lv_obj_t * parent, const char * icon, const char * txt,
                              lv_menu_builder_variant_t builder_variant)
{
    lv_obj_t * obj = lv_menu_cont_create(parent);

    lv_obj_t * img = NULL;
    lv_obj_t * label = NULL;

    if(icon) {
        img = lv_img_create(obj);
        lv_img_set_src(img, icon);
    }

    if(txt) {
        label = lv_label_create(obj);
        lv_label_set_text(label, txt);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_flex_grow(label, 1);
    }

    if(builder_variant == LV_MENU_ITEM_BUILDER_VARIANT_2 && icon && txt) {
        lv_obj_add_flag(img, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        lv_obj_swap(img, label);
    }

    return obj;
}

static lv_obj_t * create_slider(lv_obj_t * parent, const char * icon, const char * txt, int32_t min, int32_t max,
                                int32_t val)
{
    lv_obj_t * obj = create_text(parent, icon, txt, LV_MENU_ITEM_BUILDER_VARIANT_2);

    lv_obj_t * slider = lv_slider_create(obj);
    lv_obj_set_flex_grow(slider, 1);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, val, LV_ANIM_OFF);

    if(icon == NULL) {
        lv_obj_add_flag(slider, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    }

    return obj;
}

static void nutty_main(void) {
    /*heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

    ESP_LOGI(TAG, "Starting NuttySettings...");

    
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

        // Sub-items in "Display"
        lv_obj_t * displaySubPage = lv_menu_page_create(menu, NULL);
        cont = lv_menu_cont_create(displaySubPage);
        create_slider(cont, LV_SYMBOL_SETTINGS, "Velocity", 0, 150, 120);
        
        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
        
        cont = lv_menu_cont_create(displaySubPage);
        label = lv_label_create(cont);
        lv_label_set_text(label, "Backlight Lvl");
        lv_obj_add_style(label, &text_style, LV_PART_MAIN);
        lv_menu_set_load_page_event(menu, cont, NULL);
        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
        lv_group_add_obj(g, cont);

        cont = lv_menu_cont_create(displaySubPage);
        label = lv_label_create(cont);
        lv_label_set_text(label, "Backlight Timeout");
        lv_obj_add_style(label, &text_style, LV_PART_MAIN);
        lv_menu_set_load_page_event(menu, cont, NULL);
        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
        lv_group_add_obj(g, cont);

        // Parent "Display"
        cont = lv_menu_cont_create(mainPage);
        label = lv_label_create(cont);
        lv_label_set_text(label, "Display");
        lv_obj_add_style(label, &text_style, LV_PART_MAIN);
        lv_menu_set_load_page_event(menu, cont, displaySubPage);
        //lv_obj_add_event_cb(cont, lvgl_menu_on_click_event_handler, LV_EVENT_CLICKED, (void *)&selectedChoice); 
        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
        lv_group_add_obj(g, cont);

        
        cont = lv_menu_cont_create(mainPage);
        label = lv_label_create(cont);
        lv_label_set_text(label, "Storage");
        lv_obj_add_style(label, &text_style, LV_PART_MAIN);
        lv_menu_set_load_page_event(menu, cont, NULL);
        //lv_obj_add_event_cb(cont, lvgl_menu_on_click_event_handler, LV_EVENT_CLICKED, (void *)&selectedChoice); 
        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
        lv_group_add_obj(g, cont);

        
        cont = lv_menu_cont_create(mainPage);
        label = lv_label_create(cont);
        lv_label_set_text(label, "System");
        lv_obj_add_style(label, &text_style, LV_PART_MAIN);
        lv_menu_set_load_page_event(menu, cont, NULL);
        //lv_obj_add_event_cb(cont, lvgl_menu_on_click_event_handler, LV_EVENT_CLICKED, (void *)&selectedChoice); 
        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
        lv_group_add_obj(g, cont);


        lv_menu_set_page(menu, mainPage);
        NuttyDisplay_unlockLVGL();

        while(1) {
            if(selectedChoice != NULL) {
                NuttyDisplay_lockLVGL();
                lv_group_remove_all_objs(g);
                lv_group_del(g);
                NuttyDisplay_unlockLVGL();
                NuttyDisplay_clearUserAppArea();

                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // After the app exits, we go back to menu
    NuttyApps_launchAppByIndex(0);*/


    ESP_LOGI(TAG, "Starting Settings...");

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

NuttyAppDefinition NuttySettings = {
    .appName = "Settings",
    .appMainEntry = nutty_main,
    .appHidden = false
};

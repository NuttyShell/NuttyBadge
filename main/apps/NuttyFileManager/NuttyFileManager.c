#include "NuttyFileManager.h"

static const char *TAG = "FileManager";

static void lvgl_on_click_set_path(lv_event_t * event) {
    char *appName = lv_label_get_text(lv_obj_get_child(event->target, 0));
    *((char **)event->user_data) = appName;
}

static void generate_file_menu(char *filePath) {
    char *selectedFileOrDir = NULL;

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

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    NuttyDisplay_lockLVGL();
    lv_obj_t *menu = lv_menu_create(drawArea);
    
    lv_obj_set_size(menu, lv_obj_get_width(drawArea), lv_obj_get_height(drawArea));
    lv_obj_center(menu);

    lv_obj_t *mainPage = lv_menu_page_create(menu, NULL);
    lv_obj_t *cont, *label;
    for(uint8_t i=0; i<0; i++) {
        cont = lv_menu_cont_create(mainPage);
        label = lv_label_create(cont);
        lv_label_set_text(label, "");
        lv_menu_set_load_page_event(menu, cont, NULL);
        lv_obj_add_event_cb(cont, lvgl_on_click_set_path, LV_EVENT_CLICKED, (void *)&selectedFileOrDir); 
        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
        lv_group_add_obj(g, cont);
    }
    lv_menu_set_page(menu, mainPage);
    NuttyDisplay_unlockLVGL();

    NuttyDisplay_lockLVGL();
    lv_group_remove_all_objs(g);
    lv_group_del(g);
    NuttyDisplay_unlockLVGL();
    NuttyDisplay_clearUserAppArea();
}

static void nutty_main(uint8_t argc, void **argv) {
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

    ESP_LOGI(TAG, "Started with argc=%d", argc);

    if(argc !=1 || argc !=0) {
        // Invalid argument count
        ESP_LOGE(TAG, "Invalid argument counts");
        return;
    }


    
    
}

NuttyAppDefinition NuttyFileManager = {
    .appName = "File Manager",
    .appMainEntryWithParams = nutty_main,
    .appHidden = false
};

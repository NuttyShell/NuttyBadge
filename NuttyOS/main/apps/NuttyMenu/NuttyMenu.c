#include "NuttyMenu.h"
#include "services/NuttyInput/NuttyInput.h"

static const char *TAG = "Menu";

static void lvgl_on_click_event_handler(lv_event_t * event) {
    char *appName = lv_label_get_text(lv_obj_get_child(event->target, 0));
    *((char **)event->user_data) = appName;
}


static void lvgl_on_focus_event_handler(lv_event_t * event) {
    //printf("Focused=%s\n", (char *)event->user_data);
}

static void nutty_main(void) {
    char *appToStart = NULL;

    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

    ESP_LOGI(TAG, "Starting NuttyMenu...");

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
    lv_style_t text_style;
    lv_style_init(&text_style);
    lv_style_set_text_font(&text_style, &lv_font_montserrat_12);
    for(uint8_t i=0; i<NuttyApps_getTotalNumberOfApps(); i++) {
        NuttyAppDefinition app = NuttyApps_getAppByIndex(i);
        if(!app.appHidden) {
            cont = lv_menu_cont_create(mainPage);
            label = lv_label_create(cont);
            lv_label_set_text(label, app.appName);
            lv_obj_add_style(label, &text_style, LV_PART_MAIN);
            lv_menu_set_load_page_event(menu, cont, NULL);
            lv_obj_add_event_cb(cont, lvgl_on_click_event_handler, LV_EVENT_CLICKED, (void *)&appToStart); 
            lv_obj_add_event_cb(cont, lvgl_on_focus_event_handler, LV_EVENT_FOCUSED, (void *)app.appName);
            lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
            lv_group_add_obj(g, cont);
        }
    }
    lv_menu_set_page(menu, mainPage);
    NuttyDisplay_unlockLVGL();

    while(1) {
        if(appToStart != NULL) {
            uint8_t appIndex = NuttyApps_getAppIndexByName(appToStart); // Must get index first to start the app, as we will cleanup LVGL later, the string will be gone
            NuttyDisplay_lockLVGL();
            lv_group_remove_all_objs(g);
            lv_group_del(g);
            NuttyDisplay_unlockLVGL();
            NuttyDisplay_clearUserAppArea();
            
            printf("Cleaned up LVGL.\n");
            if(NuttyApps_isParamedAppByIndex(appIndex) == 1) {
                NuttyApps_launchParamedAppByIndex(appIndex, 0, NULL); // spawns new task
            }else{
                NuttyApps_launchAppByIndex(appIndex); // spawns new task
            }
            return; // ends nutty_main
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }        
}

NuttyAppDefinition NuttyMenu = {
    .appName = "Main Menu",
    .appMainEntry = nutty_main,
    .appHidden = true // Not showing in main menu
};
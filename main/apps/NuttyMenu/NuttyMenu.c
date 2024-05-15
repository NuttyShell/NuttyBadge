#include "NuttyMenu.h"

static const char *TAG = "Menu";

static void nutty_main(void) {
    ESP_LOGI(TAG, "Starting NuttyMenu...");
    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    lv_obj_t *menu = lv_menu_create(drawArea);
    lv_obj_set_size(menu, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_center(menu);

    lv_obj_t *mainPage = lv_menu_page_create(menu, NULL);
    lv_obj_t *cont, *label;

    cont = lv_menu_cont_create(mainPage);
    label = lv_label_create(cont);
    lv_label_set_text(label, "FIRST");
    lv_menu_set_load_page_event(menu, cont, NULL);

    cont = lv_menu_cont_create(mainPage);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Item 2");
    lv_menu_set_load_page_event(menu, cont, NULL);

    cont = lv_menu_cont_create(mainPage);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Item 3");
    lv_menu_set_load_page_event(menu, cont, NULL);

    cont = lv_menu_cont_create(mainPage);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Item 4");
    lv_menu_set_load_page_event(menu, cont, NULL);

    cont = lv_menu_cont_create(mainPage);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Item 5");
    lv_menu_set_load_page_event(menu, cont, NULL);

    cont = lv_menu_cont_create(mainPage);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Item 6");
    lv_menu_set_load_page_event(menu, cont, NULL);



    lv_menu_set_page(menu, mainPage);

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while(1) {
        lv_obj_t *appArea = NuttyDisplay_getUserAppArea();
        NuttyDisplay_lockLVGL();
        lv_obj_t *label2 = lv_label_create(appArea);
        lv_label_set_long_mode(label2, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_anim_speed(label2, 4, LV_PART_MAIN);
        lv_obj_set_width(label2, 150);
        lv_label_set_text(label2, "It is a circularly scrolling text. ");
        lv_obj_align(label2, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        NuttyDisplay_unlockLVGL();

        vTaskDelay(pdMS_TO_TICKS(100000));
    }
    ESP_LOGI(TAG, "Exiting, cleaning up...");
    NuttyDisplay_lockLVGL();
    NuttyDisplay_clearUserAppArea();
    NuttyDisplay_unlockLVGL();
}

NuttyAppDefinition NuttyMenu = {
    .appName = "Main Menu",
    .appMainEntry = nutty_main,
    .appHidden = false
};
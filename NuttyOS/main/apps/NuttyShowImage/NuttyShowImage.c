#include "NuttyShowImage.h"

static const char *TAG = "ShowImage";

static void nutty_main(void) {
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

    ESP_LOGI(TAG, "Starting NuttyShowImage...");

    NuttyInputLVGLInputMapping mapping = {
            .LEFT=LV_KEY_PREV,
            .RIGHT=LV_KEY_NEXT,
            .A=LV_KEY_ENTER,
    };
    lv_indev_t *indev = NuttyInput_UpdateLVGLInDev(mapping);
    assert(indev != NULL);

    static lv_group_t *g;
    g = lv_group_create();
    assert(g != NULL);
    lv_group_set_default(g);
    lv_indev_set_group(indev, g);

    lv_style_t lbl_text_style, lbl_text_style2;
    lv_style_init(&lbl_text_style);
    lv_style_set_text_font(&lbl_text_style, &lv_font_montserrat_10);
    lv_style_init(&lbl_text_style2);
    lv_style_set_text_font(&lbl_text_style2, &lv_font_montserrat_8);
    
    if(!NuttyStorage_isSDCardMounted()) {
        // We need SD Card mounted before able to read any IRDB files
        lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
        NuttyDisplay_lockLVGL();
        lv_obj_t *lblText = lv_label_create(drawArea);
        lv_obj_t *lblText2 = lv_label_create(drawArea);
        
        lv_label_set_text(lblText, "Error: SD Card\nNot Inserted/Mounted.");
        lv_obj_add_style(lblText, &lbl_text_style, LV_PART_MAIN);
        lv_obj_align(lblText, LV_ALIGN_TOP_LEFT, 0, 0);

        
        lv_label_set_text(lblText2, "Pls check your card/partitions.");
        lv_obj_add_style(lblText2, &lbl_text_style2, LV_PART_MAIN);
        lv_obj_align(lblText2, LV_ALIGN_TOP_LEFT, 0, 35);
        NuttyDisplay_unlockLVGL();

        NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
        while(true) {
            if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        NuttyDisplay_lockLVGL();
        lv_group_remove_all_objs(g);
        lv_group_del(g);
        NuttyDisplay_unlockLVGL();
        NuttyDisplay_clearUserAppArea();
    }else{
        // Launch File Manager to select IRDB
        NuttySystemMonitor_setSystemTrayTempText("!Please select a file!", 3);
        char *ImageFilePath = malloc(1024);
        assert(ImageFilePath != NULL);
        memset(ImageFilePath, 0x00, 1024);
        strcat(ImageFilePath, SDCARD_MOUNT_POINT"/");
        void *arg_list[2];
        arg_list[0] = xTaskGetCurrentTaskHandle();
        arg_list[1] = ImageFilePath;
        NuttyApps_launchParamedAppByName("File Manager", 2, arg_list);

        xTaskNotifyWait(0x00, ULONG_MAX, NULL, portMAX_DELAY);
        ESP_LOGI(TAG, "Selection Completed: %s", ImageFilePath);

        NuttySystemMonitor_hideSystemTray();
        NuttyDisplay_clearWholeScreen();

        FILE *imageFile = fopen(ImageFilePath, "rb");
        assert(imageFile != NULL);
        fseek(imageFile, 0, SEEK_END); // seek to end of file
        long imageFileSize = ftell(imageFile); // get current file pointer
        fseek(imageFile, 0, SEEK_SET); // seek back to beginning of file
        uint8_t *pngData = (uint8_t *)malloc(imageFileSize);
        assert(pngData != NULL);
        fread(pngData, imageFileSize, 1, imageFile);
        fclose(imageFile);
        free(ImageFilePath);


        NuttyDisplay_showPNG(pngData, imageFileSize);



        // Not using any LVGL input device here, as we are not using LVGL widgets for navigation
        NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
        while(true) {
            if(NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_START)) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        NuttyDisplay_clearWholeScreen();
        free(pngData);
    }

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    
    NuttySystemMonitor_showSystemTray();
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyShowImage = {
    .appName = "Show Image",
    .appMainEntry = nutty_main,
    .appHidden = false
};

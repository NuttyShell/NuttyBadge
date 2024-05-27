#include "NuttyRemote.h"

static const char *TAG="Remote";

static const char *menuChoices[] = {
    "Debug & Replay",
    "IRDB from SD",
    "New Remote to SD",
    "< Back"
};

static void set_user_data_to_true(lv_event_t * event) {
    *(bool *)event->user_data = true;
}

static void DebugAndReplay() {
    ESP_LOGI(TAG, "Running: %s", menuChoices[0]);

    bool replayOnce = false;
    bool exit=false;
    uint16_t irAddr;
    uint16_t irCmd;
    uint8_t irType=0; // 0 = error, 1 = nec, 2=necext

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
    
    lv_style_t lbl_text_style, btn_pressed_style, btn_lbl_text_style;
    lv_style_init(&lbl_text_style);
    lv_style_init(&btn_pressed_style);
    lv_style_init(&btn_lbl_text_style);
    lv_style_set_text_font(&lbl_text_style, &lv_font_montserrat_12);
    lv_style_set_outline_width(&btn_pressed_style, 2); // TODO
    lv_style_set_text_font(&btn_pressed_style, &lv_font_montserrat_10); // TODO
    lv_style_set_text_font(&btn_lbl_text_style, &lv_font_montserrat_16);
    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    NuttyDisplay_lockLVGL();
    lv_obj_t *lblMode = lv_label_create(drawArea);
    lv_obj_t *lblAddr = lv_label_create(drawArea);
    lv_obj_t *lblCmd = lv_label_create(drawArea);
    lv_obj_t *btnReplay = lv_btn_create(drawArea);
    lv_obj_t *lblReplay = lv_label_create(btnReplay);
    lv_obj_t *btnExit = lv_btn_create(drawArea);
    lv_obj_t *lblExit = lv_label_create(btnExit);
    
    lv_label_set_text(lblMode, "Mode: ??");
    lv_obj_add_style(lblMode, &lbl_text_style, LV_PART_MAIN);
    lv_obj_align(lblMode, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(lblAddr, "Addr: ??");
    lv_obj_add_style(lblAddr, &lbl_text_style, LV_PART_MAIN);
    lv_obj_align(lblAddr, LV_ALIGN_TOP_LEFT, 0, 12);
    lv_label_set_text(lblCmd, "Cmd: ??");
    lv_obj_add_style(lblCmd, &lbl_text_style, LV_PART_MAIN);
    lv_obj_align(lblCmd, LV_ALIGN_TOP_LEFT, 0, 24);
    lv_obj_add_event_cb(btnReplay, set_user_data_to_true, LV_EVENT_CLICKED, (void *)&replayOnce);
    lv_obj_align(btnReplay, LV_ALIGN_TOP_MID, -23, 40);
    lv_obj_set_size(btnReplay, 80, 18);
    lv_obj_set_style_outline_width(btnReplay, 5, LV_STATE_PRESSED | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(btnReplay, 1, LV_STATE_FOCUS_KEY);
    lv_label_set_text(lblReplay, "REPLAY");
    lv_obj_add_style(lblReplay, &btn_lbl_text_style, LV_PART_MAIN);
    lv_obj_center(lblReplay);
    lv_obj_add_event_cb(btnExit, set_user_data_to_true, LV_EVENT_CLICKED, (void *)&exit);
    lv_obj_align(btnExit, LV_ALIGN_TOP_MID, 40, 40);
    lv_obj_set_size(btnExit, 39, 18);
    lv_obj_set_style_outline_width(btnExit, 1, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(btnReplay, 2, LV_STATE_PRESSED);
    lv_label_set_text(lblExit, "EXIT");
    lv_obj_add_style(lblExit, &btn_lbl_text_style, LV_PART_MAIN);
    lv_obj_center(lblExit);
    NuttyDisplay_unlockLVGL();

    NuttyIR_Init();
    vTaskDelay(pdMS_TO_TICKS(100));
        
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while(!exit) {
        uint8_t irStatus;
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B)) break; // Press "B" to exit

        NuttyIR_getLatestRecvResult(&irAddr, &irCmd, &irStatus);
        if(irStatus == 2 || irStatus == 4) {
            // Determine Type
            if(((irAddr & 0x00FF) & (irAddr >> 8)) == 0x0000 && ((irCmd & 0x00FF) & (irCmd >> 8)) == 0x0000) {
                irType = 1;
                NuttyDisplay_lockLVGL();
                lv_label_set_text(lblMode, "Mode: NEC");
                lv_label_set_text_fmt(lblAddr, "Addr: 0x%02X", irAddr & 0xff);
                lv_label_set_text_fmt(lblCmd, "Cmd: 0x%02X", irCmd & 0xff);
                NuttyDisplay_unlockLVGL();
            }else{
                irType = 2;
                NuttyDisplay_lockLVGL();
                lv_label_set_text(lblMode, "Mode: NECext");
                lv_label_set_text_fmt(lblAddr, "Addr: 0x%04X", irAddr);
                lv_label_set_text_fmt(lblCmd, "Cmd: 0x%04X", irCmd);
                NuttyDisplay_unlockLVGL();
            }
        }else if(irStatus == 1 || irStatus == 3) {
            irType = 0; // Error when recv IR
            NuttyDisplay_lockLVGL();
            lv_label_set_text(lblMode, "Mode: ERR");
            lv_label_set_text(lblAddr, "Addr: ERR");
            lv_label_set_text(lblCmd, "Cmd: ERR");
            NuttyDisplay_unlockLVGL();
        }

        if(replayOnce) {
            replayOnce = false;
            if(irType == 0) {
                NuttySystemMonitor_setSystemTrayTempText("!!Nothing to Tx!!", 2);
            }else if(irType == 1) {
                NuttyIR_TxNEC(irAddr, irCmd);
                NuttySystemMonitor_setSystemTrayTempText("IR Tx'ed :)", 1);
            }else if(irType == 2) {
                NuttyIR_TxNECext(irAddr, irCmd);
                NuttySystemMonitor_setSystemTrayTempText("IR Tx'ed :)", 1);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    NuttyDisplay_lockLVGL();
    lv_group_remove_all_objs(g);
    lv_group_del(g);
    NuttyDisplay_unlockLVGL();
    NuttyDisplay_clearUserAppArea();

    NuttyIR_Deinit();
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void IRDBFromSD() {
    ESP_LOGI(TAG, "Running: %s", menuChoices[1]);
}

static void NewRemoteToSD() {
    ESP_LOGI(TAG, "Running: %s", menuChoices[2]);
}

static void lvgl_menu_on_click_event_handler(lv_event_t * event) {
    char *item_text = lv_label_get_text(lv_obj_get_child(event->target, 0));
    uint8_t i;
    for(i=0; i<sizeof(menuChoices)/sizeof(char *); i++) {
        if(strcmp(menuChoices[i], item_text) == 0) {
            *((char **)event->user_data) = (char *)menuChoices[i];
            return;
        }
    }
    *((char **)event->user_data) = NULL;
}

static void nutty_main(void) {
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

    ESP_LOGI(TAG, "Starting NuttyRemote...");

    
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
        for(uint8_t i=0; i<sizeof(menuChoices)/sizeof(char *); i++) {
            cont = lv_menu_cont_create(mainPage);
            label = lv_label_create(cont);
            lv_label_set_text(label, menuChoices[i]);
            lv_obj_add_style(label, &text_style, LV_PART_MAIN);
            lv_menu_set_load_page_event(menu, cont, NULL);
            lv_obj_add_event_cb(cont, lvgl_menu_on_click_event_handler, LV_EVENT_CLICKED, (void *)&selectedChoice); 
            lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
            lv_group_add_obj(g, cont);
        }
        lv_menu_set_page(menu, mainPage);
        NuttyDisplay_unlockLVGL();

        while(1) {
            if(selectedChoice != NULL) {
                NuttyDisplay_lockLVGL();
                lv_group_remove_all_objs(g);
                lv_group_del(g);
                NuttyDisplay_unlockLVGL();
                NuttyDisplay_clearUserAppArea();

                if(selectedChoice == menuChoices[0]) {
                    DebugAndReplay();
                }else if(selectedChoice == menuChoices[1]) {
                    IRDBFromSD();
                }else if(selectedChoice == menuChoices[2]) {
                    NewRemoteToSD();
                }else if(selectedChoice == menuChoices[3]) {
                    exitApp = true;
                }
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // After the app exits, we go back to menu
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyRemote = {
    .appName = "Nutty Remote",
    .appMainEntry = nutty_main,
    .appHidden = false
};

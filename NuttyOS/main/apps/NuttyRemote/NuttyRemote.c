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
    lv_label_set_text(lblCmd, "Cmd: ??    (...Rx Wait)");
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
                NuttySystemMonitor_setSystemTrayTempText("!!Nothing to Tx!!", 10);
            }else if(irType == 1) {
                NuttyIR_TxNEC(irAddr, irCmd);
                NuttySystemMonitor_setSystemTrayTempText("IR Tx'ed :)", 10);
            }else if(irType == 2) {
                NuttyIR_TxNECext(irAddr, irCmd);
                NuttySystemMonitor_setSystemTrayTempText("IR Tx'ed :)", 10);
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

static 
ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    if (!lineptr || !n || !stream) return -1;

    size_t pos = 0;
    int c;

    if (*lineptr == NULL || *n == 0) {
        *n = 128;
        *lineptr = malloc(*n);
        if (*lineptr == NULL) return -1;
    }

    while ((c = fgetc(stream)) != EOF) {
        if (pos + 1 >= *n) {
            size_t new_size = *n * 2;
            char *new_ptr = realloc(*lineptr, new_size);
            if (!new_ptr) return -1;
            *lineptr = new_ptr;
            *n = new_size;
        }
        (*lineptr)[pos++] = c;
        if (c == '\n') break;
    }

    if (pos == 0 && c == EOF) return -1;

    (*lineptr)[pos] = '\0';
    return pos;
}

static esp_err_t parseIRDBHeader(FILE *f) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    read = getline(&line, &len, f);
    if(read != -1 && len >= 24) {
        if(strncmp(line, "Filetype: IR signals file", 25) == 0) {
            // Filetype OK
            read = getline(&line, &len, f);
            if(read != -1 && len >= 10) {
                if(strncmp(line, "Version: 1", 10) == 0) {
                    // Version OK
                    free(line);
                    return ESP_OK;
                }else{
                    free(line);
                    return ESP_ERR_INVALID_VERSION;
                }
            }
        }
    }
    if(line != NULL) free(line);
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t parseNextIRDBButton(FILE* f, char *btnName, size_t btnNameSz, char *btnType, size_t btnTypeSz, void **result) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    // Parse each button
    // Parsed Buttons
    IRDBParsedRemoteData *btnParsedData = (IRDBParsedRemoteData *)malloc(sizeof(IRDBParsedRemoteData));
    memset(btnParsedData->protocolName, 0x00, 16);
    memset(btnParsedData->addr, 0x00, 4);
    memset(btnParsedData->cmd, 0x00, 4);
    bool btnParsedAddressExtracted=false;
    bool btnParsedCommandExtracted=false;
    // Raw buttons
    IRDBRawRemoteData *btnRawData = (IRDBRawRemoteData *)malloc(sizeof(IRDBRawRemoteData));
    btnRawData->duty=-1;
    btnRawData->frequency=0;
    btnRawData->dataSz=0;
    btnRawData->data=NULL;
    uint32_t btnRawDataCap=0;
    while ((read = getline(&line, &len, f)) != -1) {
        line[strcspn(line, "\r\n")] = 0;
        if (strncmp(line, "name: ", 6) == 0) {
            strncpy(btnName, line+6, btnNameSz);
            btnName[btnNameSz - 1] = '\0';
            //sscanf(line + 5, " %15s", btnName);
        }else if (strncmp(line, "type: ", 6) == 0) {
            strncpy(btnType, line+6, btnTypeSz);
            btnType[btnTypeSz - 1] = '\0';
            //sscanf(line + 5, " %15s", btnType);
        }
        if(btnName[0] && btnType[0]) {
            if (strcmp(btnType, "parsed") == 0) {
                if (strncmp(line, "protocol:", 9) == 0) {
                    sscanf(line + 9, " %15s", btnParsedData->protocolName);
                }else if (strncmp(line, "address:", 8) == 0) {
                    sscanf(line + 8, " %02hhx %02hhx %02hhx %02hhx", &btnParsedData->addr[0], &btnParsedData->addr[1], &btnParsedData->addr[2], &btnParsedData->addr[3]);
                    btnParsedAddressExtracted=true;
                }else if (strncmp(line, "command:", 8) == 0) {
                    sscanf(line + 8, " %02hhx %02hhx %02hhx %02hhx", &btnParsedData->cmd[0], &btnParsedData->cmd[1], &btnParsedData->cmd[2], &btnParsedData->cmd[3]);
                    btnParsedCommandExtracted=true;
                }
                if(btnParsedData->protocolName[0] && btnParsedAddressExtracted && btnParsedCommandExtracted) {
                    //ESP_LOGI(TAG, "Found button: %s (type: %s, %s) [0x%02X, 0x%02X, 0x%02X, 0x%02X] = [0x%02X, 0x%02X, 0x%02X, 0x%02X]", btnName, btnType, btnParsedData->protocolName, btnParsedData->addr[0], btnParsedData->addr[1], btnParsedData->addr[2], btnParsedData->addr[3], btnParsedData->cmd[0], btnParsedData->cmd[1], btnParsedData->cmd[2], btnParsedData->cmd[3]);
                    free(line);
                    free(btnRawData);
                    *result = (void *)btnParsedData;
                    return ESP_OK;
                }
            }else if(strcmp(btnType, "raw") == 0) {
                if (strncmp(line, "frequency:", 10) == 0) {
                    sscanf(line + 10, " %lu", &btnRawData->frequency);
                }else if (strncmp(line, "duty_cycle:", 11) == 0) {
                    sscanf(line + 11, " %lf", &btnRawData->duty);
                    btnParsedAddressExtracted=true;
                }else if (strncmp(line, "data:", 5) == 0) {
                    char *dataStartPtr = line+5;
                    uint32_t data_value;
                    if(!btnRawData->data) { 
                        btnRawData->data = malloc(32*sizeof(uint32_t));
                        btnRawDataCap = 32; // in uint32_t's
                        assert(btnRawData->data != NULL);
                    }
                    
                    while (*dataStartPtr != '\0') {
                        // Skip any leading whitespace
                        while (*dataStartPtr == ' ' || *dataStartPtr == '\t') dataStartPtr++;
                        // Break if end of line
                        if (*dataStartPtr == '\0' || *dataStartPtr == '\n') break;
                        // Parse number
                        if (sscanf(dataStartPtr, "%lu", &data_value) == 1) {
                            if(btnRawData->dataSz >= btnRawDataCap) {
                                btnRawDataCap = btnRawDataCap ? btnRawDataCap * 2 : 32;
                                btnRawData->data = realloc(btnRawData->data, btnRawDataCap * sizeof(uint32_t));
                                assert(btnRawData->data != NULL);
                            }
                            btnRawData->data[btnRawData->dataSz++] = data_value;
                            
                        }
                        while (*dataStartPtr && *dataStartPtr != ' ' && *dataStartPtr != '\t' && *dataStartPtr != '\n') dataStartPtr++;
                    }
                }
                if(btnRawData->frequency > 0 && btnRawData->duty > 0 && btnRawData->dataSz > 0) {
                    //ESP_LOGI(TAG, "Found button: %s (type: %s), freq=%ld, duty=%lf, data_sz=%u", btnName, btnType, btnRawData->frequency, btnRawData->duty, btnRawData->dataSz);
                    free(line);
                    free(btnParsedData);
                    *result = (void *)btnRawData;
                    return ESP_OK;
                }
            }
        }
    }
    if(line != NULL) free(line);
    free(btnParsedData);
    free(btnRawData);
    return ESP_ERR_NOT_FOUND;
}

static void lvgl_on_click_tx_irdb_btn(lv_event_t * event) {
    char *entry = lv_label_get_text(lv_obj_get_child(event->target, 0));
    *((char **)event->user_data) = entry;
}

static void IRDBFromSD() {
    ESP_LOGI(TAG, "Running: %s", menuChoices[1]);

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
    }else{
        // Launch File Manager to select IRDB
        NuttySystemMonitor_setSystemTrayTempText("!Please select a file!", 30);
        char *IRDBFilePath = malloc(1024);
        assert(IRDBFilePath != NULL);
        memset(IRDBFilePath, 0x00, 1024);
        strcat(IRDBFilePath, SDCARD_MOUNT_POINT"/");
        void *arg_list[2];
        arg_list[0] = xTaskGetCurrentTaskHandle();
        arg_list[1] = IRDBFilePath;
        NuttyApps_launchParamedAppByName("File Manager", 2, arg_list);

        xTaskNotifyWait(0x00, ULONG_MAX, NULL, portMAX_DELAY);
        ESP_LOGI(TAG, "Selection Completed: %s", IRDBFilePath);
        if(strcmp(IRDBFilePath, SDCARD_MOUNT_POINT"/") != 0) { // Selected a file from File Manager, not exited directly
            // Read the file and parse it
            FILE *f = fopen(IRDBFilePath, "r");
            char *line = NULL;
            size_t len = 0;
            ssize_t read;
            if(f) {
                esp_err_t hdrCheck = parseIRDBHeader(f);
                if(hdrCheck == ESP_OK) {
                    void *result;
                    char btnName[16];
                    char btnType[16];
                    char *selectedBtn = NULL;

                    lv_group_remove_all_objs(g);
                    lv_group_del(g);
                    lv_group_set_default(NULL); // FIXME: Not sure needed or not
                    NuttyInputLVGLInputMapping mapping = {
                        .UP=LV_KEY_PREV,
                        .DOWN=LV_KEY_NEXT,
                        .A=LV_KEY_ENTER,
                    };
                    lv_indev_t *indev = NuttyInput_UpdateLVGLInDev(mapping);
                    assert(indev != NULL);

                    g = lv_group_create();
                    assert(g != NULL);
                    lv_group_set_default(g);
                    lv_indev_set_group(indev, g);

                    lv_style_t current_path_text_style;
                    lv_style_init(&current_path_text_style);
                    lv_style_set_text_font(&current_path_text_style, &lv_font_montserrat_8);

                    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();

                    NuttyDisplay_lockLVGL();
                    lv_obj_t *menu = lv_menu_create(drawArea);
                    lv_obj_align(menu, LV_ALIGN_BOTTOM_MID, 0, 0);
                    lv_obj_set_size(menu, lv_obj_get_width(drawArea), lv_obj_get_height(drawArea));
                    lv_obj_align(menu, LV_ALIGN_BOTTOM_MID, 0, 0);
                    lv_obj_t *mainPage = lv_menu_page_create(menu, NULL);
                    lv_obj_t *cont, *label;
                    lv_style_t text_style;
                    lv_style_init(&text_style);
                    lv_style_set_text_font(&text_style, &lv_font_montserrat_10);

                    while(parseNextIRDBButton(f, btnName, 16, btnType, 16, &result) == ESP_OK) {
                        cont = lv_menu_cont_create(mainPage);
                        label = lv_label_create(cont);
                        lv_label_set_text(label, btnName);
                        lv_obj_add_style(label, &text_style, LV_PART_MAIN);
                        lv_menu_set_load_page_event(menu, cont, NULL);
                        lv_obj_add_event_cb(cont, lvgl_on_click_tx_irdb_btn, LV_EVENT_CLICKED, (void *)&selectedBtn); 
                        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
                        lv_group_add_obj(g, cont);

                        if(strcmp(btnType, "parsed") == 0) {
                            free(result);
                        }else if(strcmp(btnType, "raw") == 0) {
                            free(((IRDBRawRemoteData *)result)->data);
                            free(result);
                        }
                    
                    }

                    // Create exit selection
                    cont = lv_menu_cont_create(mainPage);
                    label = lv_label_create(cont);
                    lv_label_set_text(label, "<QUIT TO MENU>");
                    lv_obj_add_style(label, &text_style, LV_PART_MAIN);
                    lv_menu_set_load_page_event(menu, cont, NULL);
                    lv_obj_add_event_cb(cont, lvgl_on_click_tx_irdb_btn, LV_EVENT_CLICKED, (void *)&selectedBtn); 
                    lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
                    lv_group_add_obj(g, cont);

                    lv_menu_set_page(menu, mainPage);
                    NuttyDisplay_unlockLVGL();

                    while(true) {
                        while(selectedBtn == NULL) vTaskDelay(pdTICKS_TO_MS(5));
                        if(strcmp(selectedBtn, "<QUIT TO MENU>") != 0) {
                            heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
                            char tmp[24];
                            size_t sTmp=snprintf(tmp, sizeof(tmp), "Tx: %s", selectedBtn);
                            tmp[sTmp]=0x00;
                            NuttySystemMonitor_setSystemTrayTempText(tmp, 10);

                            // Tx this button
                            rewind(f);
                            parseIRDBHeader(f);
                            while(parseNextIRDBButton(f, btnName, 16, btnType, 16, &result) == ESP_OK) {
                                if(strcmp(btnName, selectedBtn) == 0) break;
                                if(strcmp(btnType, "parsed") == 0) {
                                    free(result);
                                }else if(strcmp(btnType, "raw") == 0) {
                                    free(((IRDBRawRemoteData *)result)->data);
                                    free(result);
                                }
                            }
                            if(strcmp(btnName, selectedBtn) == 0) { // Confrim above loop really broke only because of match
                                if(strcmp(btnType, "parsed") == 0) {
                                    ESP_LOGI(TAG, "Tx %s (%s)", btnName, ((IRDBParsedRemoteData *)result)->protocolName);
                                    if(strcmp(((IRDBParsedRemoteData *)result)->protocolName, "NEC") == 0) {
                                        NuttyIR_TxNEC(((IRDBParsedRemoteData *)result)->addr[0], ((IRDBParsedRemoteData *)result)->cmd[0]);
                                    }else if(strcmp(((IRDBParsedRemoteData *)result)->protocolName, "NECext") == 0) {
                                        NuttyIR_TxNECext(((((IRDBParsedRemoteData *)result)->addr[0] << 8) | (((IRDBParsedRemoteData *)result)->addr[1])), ((((IRDBParsedRemoteData *)result)->cmd[0] << 8) | (((IRDBParsedRemoteData *)result)->cmd[1])));
                                    //}else if(...) { // TODO: Implement other obsecure parsed protocol, like RC5, RC6, Samsung32, SIRC, SIRC15, SIRC20, Kaseikyo
                                    }else{
                                        NuttySystemMonitor_setSystemTrayTempText("Unimplemented Proto", 10);
                                    }
                                    free(result);
                                }else if(strcmp(btnType, "raw") == 0) {
                                    ESP_LOGI(TAG, "Tx %s (raw sz=%u)", btnName, ((IRDBRawRemoteData *)result)->dataSz);
                                    NuttyIR_TxRaw(((IRDBRawRemoteData *)result)->frequency, ((IRDBRawRemoteData *)result)->duty, ((IRDBRawRemoteData *)result)->data, ((IRDBRawRemoteData *)result)->dataSz);
                                    NuttySystemMonitor_setSystemTrayTempText("Unimplemented Proto", 10);
                                    free(((IRDBRawRemoteData *)result)->data);
                                    free(result);
                                }
                            }else{
                                NuttySystemMonitor_setSystemTrayTempText("Sth went wrong", 10); // This name not found in IRDB file
                            }

                            // Go back to the button menu (as onclick will go into a blank submenu)
                            ESP_LOGI(TAG, "Reshow IR button menu");
                            lv_menu_set_page(menu, mainPage);
                            selectedBtn = NULL;
                        }else{
                            break;
                        }
                    }
                }else if(hdrCheck == ESP_ERR_INVALID_VERSION) {
                    NuttySystemMonitor_setSystemTrayTempText("!! Invalid Version !!", 20);
                }else if(hdrCheck == ESP_ERR_NOT_SUPPORTED) {
                    NuttySystemMonitor_setSystemTrayTempText("!! Invalid File !!", 20);
                }
                fclose(f);
            }else{
                NuttySystemMonitor_setSystemTrayTempText("!! Cant open the file !!", 20);
            }

        }else{
            NuttySystemMonitor_setSystemTrayTempText("!! No file selected !!", 10);
        }
        free(IRDBFilePath);
    }

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    NuttyDisplay_lockLVGL();
    lv_group_remove_all_objs(g);
    lv_group_del(g);
    lv_group_set_default(NULL); // FIXME: Not sure needed or not
    NuttyDisplay_unlockLVGL();
    NuttyDisplay_clearUserAppArea();
    ESP_LOGI(TAG, "Ending: %s", menuChoices[1]);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void NewRemoteToSD() {
    ESP_LOGI(TAG, "Running: %s", menuChoices[2]);

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
    ESP_LOGI(TAG, "Ending: %s", menuChoices[2]);
    vTaskDelay(pdMS_TO_TICKS(1000));
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
                    selectedChoice=NULL;
                }else if(selectedChoice == menuChoices[1]) {
                    IRDBFromSD();
                    selectedChoice=NULL;
                }else if(selectedChoice == menuChoices[2]) {
                    NewRemoteToSD();
                    selectedChoice=NULL;
                }else if(selectedChoice == menuChoices[3]) {
                    exitApp = true;
                }
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
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

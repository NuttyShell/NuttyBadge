#include "NuttyFileManager.h"

static const char *TAG = "FileManager";

static void lvgl_on_click_set_path(lv_event_t * event) {
    char *entry = lv_label_get_text(lv_obj_get_child(event->target, 0));
    *((char **)event->user_data) = entry;
}

static void generate_file_menu(char *path, size_t path_buf_size, bool ret_when_sel_file) {
    char *selectedFileOrDir = NULL;

    if(path[0] == 0x00) return;

    DIR *dir = opendir(path);
    if(dir == NULL) {
        ESP_LOGE(TAG, "%s is NULL.", path);
        path[0] = 0x00; // Error when opening path
        return;
    }
    
    char entrypath[256];
    const size_t absPath_len = strlen(path);
    strlcpy(entrypath, path, sizeof(entrypath));

    struct dirent *entry;
    struct stat entry_stat;
    const char *entrytype;
    char entrysize[16];

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

    lv_style_t current_path_text_style;
    lv_style_init(&current_path_text_style);
    lv_style_set_text_font(&current_path_text_style, &lv_font_montserrat_8);

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();

    NuttyDisplay_lockLVGL();
    lv_obj_t *menu = lv_menu_create(drawArea);
    lv_obj_t *lblCurrentPath = lv_label_create(drawArea);
    char currentPath[300];
    snprintf(currentPath, sizeof(currentPath), "PATH:%s", path);
    lv_label_set_text(lblCurrentPath, currentPath);
    lv_obj_add_style(lblCurrentPath, &current_path_text_style, LV_PART_MAIN);
    lv_obj_align(lblCurrentPath, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_long_mode(lblCurrentPath, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_anim_speed(lblCurrentPath, 4, LV_PART_MAIN);
    lv_obj_set_size(lblCurrentPath, lv_obj_get_width(drawArea), 10);
    
    lv_obj_set_size(menu, lv_obj_get_width(drawArea), lv_obj_get_height(drawArea) - 10);
    lv_obj_align(menu, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *mainPage = lv_menu_page_create(menu, NULL);
    lv_obj_t *cont, *label;
    lv_style_t text_style;
    lv_style_init(&text_style);
    lv_style_set_text_font(&text_style, &lv_font_montserrat_10);

    
    // Back to last DIR
    if(strlen(path) > strlen(SDCARD_MOUNT_POINT"/")) {
        cont = lv_menu_cont_create(mainPage);
        label = lv_label_create(cont);
        lv_label_set_text(label, "..");
        lv_obj_add_style(label, &text_style, LV_PART_MAIN);
        lv_menu_set_load_page_event(menu, cont, NULL);
        lv_obj_add_event_cb(cont, lvgl_on_click_set_path, LV_EVENT_CLICKED, (void *)&selectedFileOrDir); 
        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
        lv_group_add_obj(g, cont);
    }

    char lblEntry[300];
    while ((entry = readdir(dir)) != NULL) {
        entrytype = (entry->d_type == DT_DIR ? "DIR" : "FIL");

        strlcpy(entrypath + absPath_len, entry->d_name, sizeof(entrypath) - absPath_len);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
            continue;
        }
        if (strncmp(entrytype, "FIL", 3) == 0 && strncmp(entry->d_name, "._", 2) == 0 && entry_stat.st_size == 4096) {
            ESP_LOGI(TAG, "Skipping Apple Double file: %s", entry->d_name);
            continue;
        }
        if (strncmp(entrytype, "DIR", 3) == 0 && (
            strcmp(entry->d_name, ".fseventsd") == 0 ||
            strncmp(entry->d_name, ".Spotlight-V100", 15) == 0 ||
            strncmp(entry->d_name, "System Volume", 13) == 0)) {
            ESP_LOGI(TAG, "Skipping macOS system folder: %s", entry->d_name);
            continue;
        }
        ESP_LOGI(TAG, "Found %s : %s (%ld bytes)", entrytype, entry->d_name, entry_stat.st_size);
        snprintf(lblEntry, sizeof(lblEntry), "[%s]\t[%ldB]\t%s", entrytype, entry_stat.st_size, entry->d_name);

        if(entry->d_name[0] == '.') continue; // Skip files/folders starting with "." (hidden things, especially Mac OS :))

        cont = lv_menu_cont_create(mainPage);
        label = lv_label_create(cont);
        lv_label_set_text(label, lblEntry);
        lv_obj_add_style(label, &text_style, LV_PART_MAIN);
        lv_menu_set_load_page_event(menu, cont, NULL);
        lv_obj_add_event_cb(cont, lvgl_on_click_set_path, LV_EVENT_CLICKED, (void *)&selectedFileOrDir); 
        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
        lv_group_add_obj(g, cont);
    }
    closedir(dir);

    // Create exit selection
    if(strcmp(path, SDCARD_MOUNT_POINT"/") == 0) {
        cont = lv_menu_cont_create(mainPage);
        label = lv_label_create(cont);
        lv_label_set_text(label, "<EXIT>");
        lv_obj_add_style(label, &text_style, LV_PART_MAIN);
        lv_menu_set_load_page_event(menu, cont, NULL);
        lv_obj_add_event_cb(cont, lvgl_on_click_set_path, LV_EVENT_CLICKED, (void *)&selectedFileOrDir); 
        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
        lv_group_add_obj(g, cont);
    }

    lv_menu_set_page(menu, mainPage);
    NuttyDisplay_unlockLVGL();

    while(selectedFileOrDir == NULL) {
        vTaskDelay(pdTICKS_TO_MS(10));
    }

    bool isExit = (strcmp(selectedFileOrDir, "<EXIT>") == 0);

    char *selectedFileOrDirName = selectedFileOrDir;
    while(*selectedFileOrDirName != '\t') selectedFileOrDirName++;
    selectedFileOrDirName++;
    while(*selectedFileOrDirName != '\t') selectedFileOrDirName++;
    selectedFileOrDirName++;

    bool isDir=false;
    if(selectedFileOrDir[1] == 'D' && selectedFileOrDir[2] == 'I' && selectedFileOrDir[3] == 'R') { // [DIR]
        isDir = true;
        size_t cur_len = strlen(path);
        size_t name_len = strlen(selectedFileOrDirName);
        if (cur_len + name_len + 2 > path_buf_size) {
            ESP_LOGE(TAG, "Path too long, rejecting: %s + %s", path, selectedFileOrDirName);
            path[0] = 0x00;
        } else {
            strlcat(path, selectedFileOrDirName, path_buf_size);
            strlcat(path, "/", path_buf_size);
        }
    }

    if(selectedFileOrDir[0] == '.' && selectedFileOrDir[1] == '.') { // ..
        isDir = true; // Back to last DIR
        size_t len = strlen(path);
        // Walk backward from the end
        for (int i = len - 2; i >= 0; i--) {
            if (path[i] == '/') {
                path[i + 1] = '\0';
                break;
            }
        }
    }
    
    if(!isDir && !isExit && ret_when_sel_file) {
        size_t cur_len = strlen(path);
        size_t name_len = strlen(selectedFileOrDirName);
        if (cur_len + name_len + 1 > path_buf_size) {
            ESP_LOGE(TAG, "Path too long, rejecting: %s + %s", path, selectedFileOrDirName);
            path[0] = 0x00;
        } else {
            strlcat(path, selectedFileOrDirName, path_buf_size);
        }
    }
   
    NuttyDisplay_lockLVGL();
    lv_group_remove_all_objs(g);
    lv_group_del(g);
    lv_group_set_default(NULL); // FIXME: Not sure needed or not
    NuttyDisplay_unlockLVGL();
    NuttyDisplay_clearUserAppArea();

    if(isExit) return;
    
    ESP_LOGI(TAG, "Selected: %s\n", path);
    if(!isDir && ret_when_sel_file) return;
    generate_file_menu(path, path_buf_size, ret_when_sel_file);
}

static void nutty_main(uint8_t argc, void **argv) {
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

    ESP_LOGI(TAG, "Started with argc=%d", argc);

    if(argc == 0) {
        printf("File_menu start\n");
        heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
        char tmpFilePath[512] = {0};
        strlcpy(tmpFilePath, SDCARD_MOUNT_POINT"/", sizeof(tmpFilePath));
        generate_file_menu(tmpFilePath, sizeof(tmpFilePath), false);
        printf("File_menu done\n");
        heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
        NuttyApps_launchAppByIndex(0);
    }else if(argc == 3) {
        size_t path_buf_size = *((size_t *)argv[2]);
        generate_file_menu((char *)argv[1], path_buf_size, true);
        xTaskNotify((TaskHandle_t)argv[0], 0, eNoAction);
    }else{
        // Invalid argument count
        ESP_LOGE(TAG, "Invalid argument counts");
    }
}

NuttyAppDefinition NuttyFileManager = {
    .appName = "File Manager",
    .appMainEntryWithParams = nutty_main,
    .appHidden = false
};

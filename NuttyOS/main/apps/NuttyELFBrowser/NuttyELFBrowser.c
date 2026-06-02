#include "NuttyELFBrowser.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "NuttyELFBrowser";

static bool has_extension(const char *filename, const char *ext)
{
    const char *dot = strrchr(filename, '.');

    if (!dot || dot == filename)
    {
        return false;
    }

    return strcmp(dot, ext) == 0;
}

static void nutty_main(void)
{
    ESP_LOGI(TAG, "Starting NuttyELFBrowser...");

    if (!NuttyStorage_isSDCardMounted())
    {
        lv_style_t text_style;
        lv_style_init(&text_style);
        lv_style_set_text_font(&text_style, &lv_font_montserrat_10);

        lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
        NuttyDisplay_lockLVGL();
        lv_obj_t *lbl = lv_label_create(drawArea);
        lv_label_set_text(lbl, "Error: SD Card\nNot Inserted/Mounted.");
        lv_obj_add_style(lbl, &text_style, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_anim_speed(lbl, 4, LV_PART_MAIN);
        lv_obj_set_width(lbl, 150);
        NuttyDisplay_unlockLVGL();

        NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
        while (true)
        {
            if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL))
            {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        NuttyDisplay_clearUserAppArea();
        NuttyApps_launchAppByIndex(0);
        return;
    }

    NuttySystemMonitor_setSystemTrayTempText("!Please select an ELF file!", 30);

    char *elfFilePath = malloc(1024);
    assert(elfFilePath != NULL);
    memset(elfFilePath, 0x00, 1024);
    strcat(elfFilePath, SDCARD_MOUNT_POINT "/");

    void *arg_list[2];
    arg_list[0] = xTaskGetCurrentTaskHandle();
    arg_list[1] = elfFilePath;
    NuttyApps_launchParamedAppByName("File Manager", 2, arg_list);

    xTaskNotifyWait(0x00, ULONG_MAX, NULL, portMAX_DELAY);

    ESP_LOGI(TAG, "Selection Completed: %s", elfFilePath);

    if (strcmp(elfFilePath, SDCARD_MOUNT_POINT "/") == 0)
    {
        NuttySystemMonitor_setSystemTrayTempText("!! No file selected !!", 10);
        free(elfFilePath);
        NuttyApps_launchAppByIndex(0);
        return;
    }

    if (!has_extension(elfFilePath, ".elf"))
    {
        NuttySystemMonitor_setSystemTrayTempText("Invalid ext, [START]=exit", 20);
        NuttyDisplay_clearUserAppArea();

        NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
        while (true)
        {
            if (NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_START))
            {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        free(elfFilePath);
        NuttyDisplay_clearUserAppArea();
        NuttyApps_launchAppByIndex(0);
        return;
    }

    NuttySystemMonitor_hideSystemTray();
    NuttyDisplay_clearWholeScreen();

    esp_err_t run_result = NuttyELFLoader_RunFromPath(elfFilePath, 0, NULL);
    if (run_result != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to run ELF %s: %s", elfFilePath, esp_err_to_name(run_result));
        NuttySystemMonitor_setSystemTrayTempText("ELF run failed", 20);
    }

    NuttySystemMonitor_showSystemTray();
    free(elfFilePath);

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while (true)
    {
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL))
        {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    NuttyDisplay_clearWholeScreen();
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyELFBrowser = {
    .appName = "NuttyELFBrowser",
    .appMainEntry = nutty_main,
    .appHidden = false};
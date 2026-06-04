#include "NuttyDOOM.h"
#include "esp_heap_caps.h"

// The quit flag exposed by the doomgeneric nuttybadge platform layer
extern volatile bool nuttydoom_quit_requested;

static const char *TAG = "NuttyDOOM";

// FreeRTOS task that runs the DOOM engine
static void nutty_doom_task(void *arg)
{
    char *wad_path = (char *)arg;

    // Build argv for doomgeneric: { "doom", "-iwad", "/sdcard/.../file.wad" }
    char *argv[4];
    int argc = 0;
    argv[argc++] = "doom";
    argv[argc++] = "-iwad";
    argv[argc++] = wad_path;
    argv[argc]   = NULL;

    ESP_LOGI(TAG, "Launching DOOM with IWAD: %s", wad_path);

    // doomgeneric_Create calls D_DoomMain -> D_DoomLoop (one tic), then returns.
    // We loop calling doomgeneric_Tick() ourselves.
    doomgeneric_Create(argc, argv);

    // Main game loop
    while (!nuttydoom_quit_requested)
    {
        doomgeneric_Tick();
    }

    ESP_LOGI(TAG, "DOOM quit requested, cleaning up...");

    // Cleanup
    NuttySystemMonitor_showSystemTray();
    NuttyDisplay_clearWholeScreen();

    free(wad_path);

    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// NuttyApp entry point
// ---------------------------------------------------------------------------
static void nutty_main(void)
{
    ESP_LOGI(TAG, "Starting NuttyDOOM...");

    if (!NuttyStorage_isSDCardMounted())
    {
        // Show error message via LVGL (same pattern as other apps)
        lv_style_t lbl_style;
        lv_style_init(&lbl_style);
        lv_style_set_text_font(&lbl_style, &lv_font_montserrat_10);

        NuttyInputLVGLInputMapping mapping = {
            .LEFT  = LV_KEY_PREV,
            .RIGHT = LV_KEY_NEXT,
            .A     = LV_KEY_ENTER,
        };
        lv_indev_t *indev = NuttyInput_UpdateLVGLInDev(mapping);
        assert(indev != NULL);

        lv_group_t *g = lv_group_create();
        assert(g != NULL);
        lv_group_set_default(g);
        lv_indev_set_group(indev, g);

        lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
        NuttyDisplay_lockLVGL();

        lv_obj_t *lbl = lv_label_create(drawArea);
        lv_label_set_text(lbl, "Error: SD Card\nNot Inserted/Mounted.");
        lv_obj_add_style(lbl, &lbl_style, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

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

        NuttyDisplay_lockLVGL();
        lv_group_remove_all_objs(g);
        lv_group_del(g);
        NuttyDisplay_unlockLVGL();
        NuttyDisplay_clearUserAppArea();
    }
    else
    {
        // SD card is present – launch File Manager to let the user pick a .wad
        NuttySystemMonitor_setSystemTrayTempText("!Select a .wad file!", 30);

        char *wadFilePath = (char *)malloc(1024);
        assert(wadFilePath != NULL);
        memset(wadFilePath, 0, 1024);
        strcat(wadFilePath, SDCARD_MOUNT_POINT "/");

        void *arg_list[2];
        arg_list[0] = xTaskGetCurrentTaskHandle();
        arg_list[1] = wadFilePath;

        NuttyApps_launchParamedAppByName("File Manager", 2, arg_list);

        // Wait for File Manager to signal completion
        xTaskNotifyWait(0x00, ULONG_MAX, NULL, portMAX_DELAY);

        ESP_LOGI(TAG, "Selection result: %s", wadFilePath);

        if (strcmp(wadFilePath, SDCARD_MOUNT_POINT "/") != 0)
        {
            // User selected a file – launch DOOM in a dedicated task
            nuttydoom_quit_requested = false;

            // Allocate task stack from PSRAM (FreeRTOS dynamic task creation
            // uses internal DRAM for the stack, which is exhausted).
            // Note: the TCB (StaticTask_t) must remain in internal DRAM.
            StackType_t *doom_stack = heap_caps_malloc(
                32768 * sizeof(StackType_t), MALLOC_CAP_SPIRAM);
            StaticTask_t *doom_task_buf = heap_caps_malloc(
                sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);

            if (doom_stack == NULL || doom_task_buf == NULL)
            {
                ESP_LOGE(TAG, "Failed to allocate PSRAM stack for DOOM task");
                free(doom_stack);
                free(doom_task_buf);
                free(wadFilePath);
            }
            else
            {
                TaskHandle_t doom_handle = xTaskCreateStaticPinnedToCore(
                    nutty_doom_task,
                    "doom",
                    32768 / sizeof(StackType_t),   // stack depth in words
                    (void *)wadFilePath,
                    5,                              // priority
                    doom_stack,
                    doom_task_buf,
                    1                               // run on core 1 (APP CPU)
                );

                if (doom_handle == NULL)
                {
                    ESP_LOGE(TAG, "Failed to create DOOM task");
                    free(doom_stack);
                    free(doom_task_buf);
                    free(wadFilePath);
                }
                else
                {
                    // Wait for the DOOM task to finish (it will delete itself)
                    while (!nuttydoom_quit_requested)
                    {
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                    // Small delay to let DOOM task clean up
                    vTaskDelay(pdMS_TO_TICKS(50));

                    // Free the PSRAM stack buffers now that the task is dead
                    free(doom_stack);
                    free(doom_task_buf);
                }
            }
        }
        else
        {
            NuttySystemMonitor_setSystemTrayTempText("!! No file selected !!", 10);
            free(wadFilePath);
        }
    }

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    NuttySystemMonitor_showSystemTray();
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyDOOM = {
    .appName = "NuttyDOOM",
    .appMainEntry = nutty_main,
    .appHidden = false
};

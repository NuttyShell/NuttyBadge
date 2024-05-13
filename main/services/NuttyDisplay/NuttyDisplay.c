#include "NuttyDisplay.h"

static const char* TAG = "NuttyDisplay";

// LVGL Stuff
static uint8_t lvglFramebuffer[128*64]; // 128 * 64, LVGL needs 1 byte per pixel
static lv_disp_drv_t lvglDisplay;
static lv_disp_draw_buf_t lvglDisplayBuffer;
static SemaphoreHandle_t lvglLock;

// LCD Stuff
static DMA_ATTR uint8_t lcdFramebuffer[128*64/8]; // 128 * 64, 1 bit per pixel
static DMA_ATTR uint8_t lcdPageTransponse[128];
static EventGroupHandle_t lcdUpdateFlag;
static SemaphoreHandle_t lcdLock;

// User Application Area
lv_obj_t *userAppArea = NULL;

#define LCD_UPDATED_BIT BIT0

// Inform the LVGL Library to do work
static void NuttyDisplay_Worker(void* arg) {
    uint8_t lcdPage;
    while(true) {
        if(xEventGroupGetBits(lcdUpdateFlag) == BIT0) {
            // LCD Update needed
            while(xSemaphoreTake(lcdLock, portMAX_DELAY) != pdTRUE);
            // Copy LCD FB (using DMA) to LCD
            for(lcdPage=0; lcdPage<8; lcdPage++) {
                for(uint16_t i=0; i<128; i++) {
                    lcdPageTransponse[i]  = (((lcdFramebuffer[128*lcdPage + 112 + (i/8)] & (1 << (7-(i%8)))) >> (7-(i%8))) << 7); // b7
                    lcdPageTransponse[i] |= (((lcdFramebuffer[128*lcdPage + 96 + (i/8)]  & (1 << (7-(i%8)))) >> (7-(i%8))) << 6); // b6
                    lcdPageTransponse[i] |= (((lcdFramebuffer[128*lcdPage + 80 + (i/8)]  & (1 << (7-(i%8)))) >> (7-(i%8))) << 5); // b5
                    lcdPageTransponse[i] |= (((lcdFramebuffer[128*lcdPage + 64 + (i/8)]  & (1 << (7-(i%8)))) >> (7-(i%8))) << 4); // b4
                    lcdPageTransponse[i] |= (((lcdFramebuffer[128*lcdPage + 48 + (i/8)]  & (1 << (7-(i%8)))) >> (7-(i%8))) << 3); // b3
                    lcdPageTransponse[i] |= (((lcdFramebuffer[128*lcdPage + 32 + (i/8)]  & (1 << (7-(i%8)))) >> (7-(i%8))) << 2); // b2
                    lcdPageTransponse[i] |= (((lcdFramebuffer[128*lcdPage + 16 + (i/8)]  & (1 << (7-(i%8)))) >> (7-(i%8))) << 1); // b1
                    lcdPageTransponse[i] |= (((lcdFramebuffer[128*lcdPage + 0 + (i/8)]   & (1 << (7-(i%8)))) >> (7-(i%8))) << 0); // b0
                }
                nuttyDriverLCD.updatePageSeqOrder(lcdPage, lcdPageTransponse, 128);
            }
            xSemaphoreGive(lcdLock);
            xEventGroupClearBits(lcdUpdateFlag, LCD_UPDATED_BIT);
        }
        if (xSemaphoreTake(lvglLock, portMAX_DELAY) == pdTRUE) {
            lv_timer_handler_run_in_period(5);
            xSemaphoreGive(lvglLock);
       }
       vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void lvgl_flush_to_lcd_fb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *px) {
    ESP_LOGD(TAG, "FLUSH: x=%d-%d; y=%d-%d\n", area->x1, area->x2, area->y1, area->y2);
    int16_t x, y;
    uint8_t lcd8Px;
    while(xSemaphoreTake(lcdLock, portMAX_DELAY) != pdTRUE);
    for(y = area->y1; y <= area->y2; y++) {
        for(x = area->x1; x <= area->x2; x+=8) { // TODO: What if x is out of bound
            lcd8Px = 0;
            lcd8Px |= ((px->full & 0x01) << 7); /*printf("%d", px->full);*/ px++;
            lcd8Px |= ((px->full & 0x01) << 6); /*printf("%d", px->full);*/ px++;
            lcd8Px |= ((px->full & 0x01) << 5); /*printf("%d", px->full);*/ px++;
            lcd8Px |= ((px->full & 0x01) << 4); /*printf("%d", px->full);*/ px++;
            lcd8Px |= ((px->full & 0x01) << 3); /*printf("%d", px->full);*/ px++;
            lcd8Px |= ((px->full & 0x01) << 2); /*printf("%d", px->full);*/ px++;
            lcd8Px |= ((px->full & 0x01) << 1); /*printf("%d", px->full);*/ px++;
            lcd8Px |= ((px->full & 0x01) << 0); /*printf("%d", px->full);*/ px++;
            #if NUTTYDISPLAY_INVERT_COLOR == 1
            lcdFramebuffer[16*y + x/8] = ~lcd8Px;
            #elif NUTTYDISPLAY_INVERT_COLOR == 0
            lcdFramebuffer[16*y + x/8] = lcd8Px;
            #endif
        }
        //printf("\n");
    }
    xSemaphoreGive(lcdLock);

    if(lv_disp_flush_is_last(disp_drv)) {
        xEventGroupSetBits(lcdUpdateFlag, LCD_UPDATED_BIT);
    }
    lv_disp_flush_ready(disp_drv);
}

static void lvgl_tick(void *arg) {
    (void) arg;
    lv_tick_inc(1);
}

void NuttyDisplay_lockLVGL() {
    while(xSemaphoreTake(lvglLock, portMAX_DELAY) != pdTRUE);
}

void NuttyDisplay_unlockLVGL() {
    xSemaphoreGive(lvglLock);
}

static lv_img_dsc_t lvgl_png_img;
void NuttyDisplay_showPNG(uint8_t *pngData, size_t pngSz) {
    lvgl_png_img.header.always_zero = 0;
    lvgl_png_img.header.w = 128;
    lvgl_png_img.header.h = 64;
    lvgl_png_img.data_size = pngSz;
    lvgl_png_img.header.cf = LV_IMG_CF_RAW_ALPHA;
    lvgl_png_img.data = pngData;
    NuttyDisplay_lockLVGL();
    lv_png_init();
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &lvgl_png_img);
    lv_obj_align(img, LV_ALIGN_TOP_LEFT, 0, 0);
    NuttyDisplay_unlockLVGL();
}


lv_obj_t* NuttyDisplay_getUserAppArea() {
    NuttyDisplay_lockLVGL();
    if(userAppArea == NULL) { 
        userAppArea = lv_obj_create(lv_scr_act());
        lv_obj_set_size(userAppArea, 128, 64);
    }
    NuttyDisplay_unlockLVGL();
    return userAppArea;
}

void NuttyDisplay_clearUserAppArea() {
    NuttyDisplay_lockLVGL();
    if(userAppArea != NULL) { 
        lv_obj_clean(userAppArea);
    }
    NuttyDisplay_unlockLVGL();
}

void NuttyDisplay_setLCDBacklight(uint8_t percentage) {
    nuttyDriverLCD.setBacklightDutyCycle(percentage << 2);
}

esp_err_t NuttyDisplay_Init() {
    ESP_LOGI(TAG, "Init");

    // LCD Stuff
    memset(lcdFramebuffer, 0x00, sizeof(lcdFramebuffer));
    lcdUpdateFlag = xEventGroupCreate();
    assert(lcdUpdateFlag != NULL);
    lcdLock = xSemaphoreCreateMutex();
    assert(lcdLock != NULL);

    // LVGL Stuff
    lvglLock = xSemaphoreCreateMutex();
    assert(lvglLock != NULL);

    memset(lvglFramebuffer, 0x00, sizeof(lvglFramebuffer));
    lv_init();
    lv_disp_draw_buf_init(&lvglDisplayBuffer, lvglFramebuffer, NULL, sizeof(lvglFramebuffer));
    lv_disp_drv_init(&lvglDisplay);
    lvglDisplay.draw_buf = &lvglDisplayBuffer;
    lvglDisplay.hor_res = 128;
    lvglDisplay.ver_res = 64;
    lvglDisplay.flush_cb = lvgl_flush_to_lcd_fb;
    lvglDisplay.antialiasing = 0;
    lvglDisplay.direct_mode = 1;
    lv_disp_drv_register(&lvglDisplay);

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1 * 1000));

    // LVGL needs to pin on single core
    xTaskCreatePinnedToCore(NuttyDisplay_Worker, "display_worker", 4096, NULL, 0, NULL, 1);
    return ESP_OK;
}
#include "NuttyBird.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "NuttyBird";

/* ── framebuffer ────────────────────────────────────────────────────
   128×64 bytes, 1 byte/pixel: 0 = lit, 1 = dark.
   Only rows 0-58 are visible (user app area is 59 px tall).
   ─────────────────────────────────────────────────────────────────*/
#define FB_W        128
#define FB_H        64
#define GAME_H      59          /* visible rows                     */
#define GROUND_Y    (GAME_H - 1)/* ground row = 58                  */

/* Bird */
#define BIRD_X      20
#define BIRD_W      4
#define BIRD_H      3

/* Pipes */
#define PIPE_W          6
#define PIPE_GAP        18
#define PIPE_SPEED      2       /* px per tick                      */
#define PIPE_SPACING    72      /* horizontal gap between pipe centres */
#define PIPE_COUNT      2
#define PIPE_GAP_MIN_Y  2
#define PIPE_GAP_MAX_Y  (GAME_H - PIPE_GAP - 2)   /* = 39          */
#define PIPE_GAP_RANGE  (PIPE_GAP_MAX_Y - PIPE_GAP_MIN_Y + 1)

/* Physics — per 50-ms tick */
#define GRAVITY     0.4f
#define FLAP_VEL   -3.2f
#define TICK_MS     50

typedef struct {
    int  x;
    int  gap_y;
    bool passed;
} Pipe;

static uint8_t      bird_px[FB_W * FB_H];
static lv_img_dsc_t bird_img_dsc;
static lv_obj_t    *bird_img_obj = NULL;

static const char *menuChoices[] = { "Start", "< Back" };

/* ── pixel helpers ──────────────────────────────────────────────── */
static inline void px_set(int x, int y) {
    if (x >= 0 && x < FB_W && y >= 0 && y < GAME_H)
        bird_px[y * FB_W + x] = 0;
}

static void draw_rect(int x, int y, int w, int h) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            px_set(x + dx, y + dy);
}

/* Bird sprite: draws a small 4×3 shape with a visible eye pixel */
static void draw_bird(int x, int y) {
    /* body */
    draw_rect(x, y, BIRD_W, BIRD_H);
    /* hollow centre to create outline effect */
    if (BIRD_W >= 4 && BIRD_H >= 3) {
        bird_px[(y + 1) * FB_W + (x + 1)] = 1;  /* inner dark pixel */
    }
}

/* ── game-over screen ───────────────────────────────────────────── */
static void show_game_over(uint16_t score) {
    lv_style_t s_mid, s_nano;
    lv_style_init(&s_mid);
    lv_style_init(&s_nano);
    lv_style_set_text_font(&s_mid,  &lv_font_montserrat_12);
    lv_style_set_text_font(&s_nano, &cg_pixel_4x5_mono);

    char buf[24];
    snprintf(buf, sizeof(buf), "Score: %d", score);

    lv_obj_t *da = NuttyDisplay_getUserAppArea();
    NuttyDisplay_lockLVGL();

    lv_obj_t *l1 = lv_label_create(da);
    lv_label_set_text(l1, "GAME OVER");
    lv_obj_add_style(l1, &s_mid, LV_PART_MAIN);
    lv_obj_align(l1, LV_ALIGN_CENTER, 0, -16);

    lv_obj_t *l2 = lv_label_create(da);
    lv_label_set_text(l2, buf);
    lv_obj_add_style(l2, &s_nano, LV_PART_MAIN);
    lv_obj_align(l2, LV_ALIGN_CENTER, 0, 2);

    lv_obj_t *l3 = lv_label_create(da);
    lv_label_set_text(l3, "Press any key");
    lv_obj_add_style(l3, &s_nano, LV_PART_MAIN);
    lv_obj_align(l3, LV_ALIGN_CENTER, 0, 14);

    NuttyDisplay_unlockLVGL();

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)) break;
    }

    lv_style_reset(&s_mid);
    lv_style_reset(&s_nano);
    NuttyDisplay_clearUserAppArea();
}

/* ── main game loop ─────────────────────────────────────────────── */
static void game_start(void) {
    lv_obj_t *da = NuttyDisplay_getUserAppArea();

    memset(bird_px, 1, sizeof(bird_px));

    bird_img_dsc.header.cf          = LV_IMG_CF_TRUE_COLOR;
    bird_img_dsc.header.always_zero = 0;
    bird_img_dsc.header.reserved    = 0;
    bird_img_dsc.header.w           = FB_W;
    bird_img_dsc.header.h           = FB_H;
    bird_img_dsc.data_size          = FB_W * FB_H;
    bird_img_dsc.data               = bird_px;

    NuttyDisplay_lockLVGL();
    bird_img_obj = lv_img_create(da);
    lv_img_set_src(bird_img_obj, &bird_img_dsc);
    lv_obj_align(bird_img_obj, LV_ALIGN_TOP_LEFT, 0, 0);
    NuttyDisplay_unlockLVGL();

    float bird_y  = (float)(GAME_H / 2 - BIRD_H / 2);
    float bird_vy = 0.0f;

    srand((unsigned int)esp_timer_get_time());

    Pipe pipes[PIPE_COUNT];
    pipes[0].x      = 96;
    pipes[0].gap_y  = PIPE_GAP_MIN_Y + rand() % PIPE_GAP_RANGE;
    pipes[0].passed = false;
    pipes[1].x      = 96 + PIPE_SPACING;
    pipes[1].gap_y  = PIPE_GAP_MIN_Y + rand() % PIPE_GAP_RANGE;
    pipes[1].passed = false;

    NuttySystemMonitor_setSystemTrayTempText("A:Flap  SEL:Quit", 40);

    uint16_t score = 0;
    bool     dead  = false;
    bool     quit  = false;

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);

    while (!dead && !quit) {
        /* ── input ── */
        if (NuttyInput_popPressedEvent(NUTTYINPUT_BTN_A | NUTTYINPUT_BTN_B)) {
            bird_vy = FLAP_VEL;
        }
        if (NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_SELECT)) {
            quit = true;
            break;
        }

        /* ── physics ── */
        bird_vy += GRAVITY;
        bird_y  += bird_vy;
        int iy = (int)bird_y;

        /* ── boundary collision ── */
        if (iy < 0 || iy + BIRD_H > GROUND_Y) dead = true;

        /* ── clear frame ── */
        memset(bird_px, 1, sizeof(bird_px));

        /* ground line */
        for (int x = 0; x < FB_W; x++) px_set(x, GROUND_Y);

        /* ── pipes ── */
        for (int i = 0; i < PIPE_COUNT; i++) {
            pipes[i].x -= PIPE_SPEED;

            /* recycle pipe that scrolled off-screen */
            if (pipes[i].x + PIPE_W < 0) {
                int max_x = 0;
                for (int j = 0; j < PIPE_COUNT; j++)
                    if (j != i && pipes[j].x > max_x) max_x = pipes[j].x;
                pipes[i].x      = max_x + PIPE_SPACING;
                pipes[i].gap_y  = PIPE_GAP_MIN_Y + rand() % PIPE_GAP_RANGE;
                pipes[i].passed = false;
            }

            /* score point when bird clears the pipe */
            if (!pipes[i].passed && pipes[i].x + PIPE_W < BIRD_X) {
                pipes[i].passed = true;
                score++;
                char msg[16];
                snprintf(msg, sizeof(msg), "Score: %d", score);
                NuttySystemMonitor_setSystemTrayTempText(msg, 10);
            }

            /* pipe collision */
            bool h_overlap = (BIRD_X + BIRD_W > pipes[i].x) &&
                             (BIRD_X < pipes[i].x + PIPE_W);
            if (!dead && h_overlap) {
                bool in_gap = (iy >= pipes[i].gap_y) &&
                              (iy + BIRD_H <= pipes[i].gap_y + PIPE_GAP);
                if (!in_gap) dead = true;
            }

            /* draw top pipe */
            if (pipes[i].gap_y > 0)
                draw_rect(pipes[i].x, 0, PIPE_W, pipes[i].gap_y);

            /* draw bottom pipe */
            int bot_y = pipes[i].gap_y + PIPE_GAP;
            int bot_h = GROUND_Y - bot_y;
            if (bot_h > 0)
                draw_rect(pipes[i].x, bot_y, PIPE_W, bot_h);
        }

        /* ── draw bird ── */
        draw_bird(BIRD_X, iy);

        /* ── push frame ── */
        NuttyDisplay_lockLVGL();
        lv_obj_invalidate(bird_img_obj);
        NuttyDisplay_unlockLVGL();

        if (dead) {
            vTaskDelay(pdMS_TO_TICKS(600)); /* brief death pause */
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }

    /* ── cleanup LVGL image ── */
    NuttyDisplay_lockLVGL();
    if (bird_img_obj) { lv_obj_del(bird_img_obj); bird_img_obj = NULL; }
    NuttyDisplay_unlockLVGL();
    NuttyDisplay_clearUserAppArea();

    if (!quit) show_game_over(score);
}

/* ── menu click handler ─────────────────────────────────────────── */
static void on_menu_click(lv_event_t *e) {
    char *text = lv_label_get_text(lv_obj_get_child(e->target, 0));
    for (uint8_t i = 0; i < sizeof(menuChoices) / sizeof(char *); i++) {
        if (strcmp(text, menuChoices[i]) == 0) {
            *((char **)e->user_data) = (char *)menuChoices[i];
            return;
        }
    }
}

/* ── app entry ──────────────────────────────────────────────────── */
static void nutty_main(void) {
    ESP_LOGI(TAG, "Starting NuttyBird...");

    bool exitApp = false;
    while (!exitApp) {
        char *selected = NULL;

        NuttyInputLVGLInputMapping mapping = {
            .UP   = LV_KEY_PREV,
            .DOWN = LV_KEY_NEXT,
            .A    = LV_KEY_ENTER,
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

        lv_obj_t *da = NuttyDisplay_getUserAppArea();
        NuttyDisplay_lockLVGL();
        lv_obj_t *menu = lv_menu_create(da);
        lv_obj_set_size(menu, lv_obj_get_width(da), lv_obj_get_height(da));
        lv_obj_center(menu);

        lv_obj_t *page = lv_menu_page_create(menu, NULL);
        for (uint8_t i = 0; i < sizeof(menuChoices) / sizeof(char *); i++) {
            lv_obj_t *cont  = lv_menu_cont_create(page);
            lv_obj_t *label = lv_label_create(cont);
            lv_label_set_text(label, menuChoices[i]);
            lv_obj_add_style(label, &text_style, LV_PART_MAIN);
            lv_menu_set_load_page_event(menu, cont, NULL);
            lv_obj_add_event_cb(cont, on_menu_click, LV_EVENT_CLICKED, (void *)&selected);
            lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
            lv_group_add_obj(g, cont);
        }
        lv_menu_set_page(menu, page);
        NuttyDisplay_unlockLVGL();

        while (1) {
            if (selected != NULL) {
                NuttyDisplay_lockLVGL();
                lv_group_remove_all_objs(g);
                lv_group_del(g);
                lv_style_reset(&text_style);
                NuttyDisplay_unlockLVGL();
                NuttyDisplay_clearUserAppArea();

                if (selected == menuChoices[0]) {
                    game_start();
                } else {
                    exitApp = true;
                }
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    NuttyDisplay_clearUserAppArea();
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyBird = {
    .appName      = "Nutty Bird",
    .appMainEntry = nutty_main,
    .appHidden    = false
};

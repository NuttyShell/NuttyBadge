#include "NuttyTetris.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "Tetris";

/* ============================================================
 * Display rotation: image is drawn rotated 90° CCW.
 * The user tilts the badge clockwise (physical right edge up)
 * to view a portrait Tetris board.
 *
 * Image buffer: 128(w) x 64(h), aligned top-left of user-app area.
 * Visible pixel rows: py 0..58 (the 59-px-tall user area).
 * User-view ("uv") space: x 0..58, y 0..127  (59 wide x 128 tall).
 *
 * Mapping (CCW: top of user view -> right of physical screen):
 *   px = 127 - uvy
 *   py = uvx
 * ============================================================ */

#define UV_W  59
#define UV_H  128

#define BS         5                       /* block size in uv pixels */
#define BOARD_W    10
#define BOARD_H    20
#define BOARD_PXW  (BOARD_W * BS)          /* 50 */
#define BOARD_PXH  (BOARD_H * BS)          /* 100 */
#define BOARD_OX   4                       /* uvx of first board cell (centered: (59-50)/2 = 4) */
#define BOARD_OY   1                       /* uvy of first board cell (border at uvy=0) */

/* Bottom HUD strip: drawn inside its own border, separate from board.
 * HUD_OY = 103, height 25 (uvy 103..127). Inner region uvy 104..126, uvx 1..57.
 */
#define HUD_OY     103
#define HUD_H      25

/* NEXT preview: 4x4 cells at smaller block size, on left of bottom strip */
#define BS_NEXT    4
#define NEXT_OX    2
#define NEXT_OY    (HUD_OY + 3)            /* 106 */

/* Score text: right of NEXT box */
#define TXT_OX     22

/* Pixel buffer covers the whole 128x64 image */
static uint8_t pxbuf[128 * 64];
static lv_img_dsc_t img_dsc;
static lv_obj_t   *img_obj = NULL;

/* ---------- pixel helpers ---------- */
static inline void px_phys(int px, int py, bool on) {
    if (px < 0 || px > 127 || py < 0 || py > 63) return;
    pxbuf[py * 128 + px] = on ? 0 : 1; /* active-low: 0 = visible */
}

static inline void px_uv(int uvx, int uvy, bool on) {
    if (uvx < 0 || uvx >= UV_W || uvy < 0 || uvy >= UV_H) return;
    px_phys(127 - uvy, uvx, on);
}

static void fill_uv_rect(int uvx, int uvy, int w, int h, bool on) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            px_uv(uvx + dx, uvy + dy, on);
}

/* ---------- 3x5 mini-font ---------- */
typedef struct { char c; uint16_t bits; } Glyph;
static const Glyph FONT[] = {
    {'0', 0b111101101101111},
    {'1', 0b010110010010111},
    {'2', 0b111001111100111},
    {'3', 0b111001111001111},
    {'4', 0b101101111001001},
    {'5', 0b111100111001111},
    {'6', 0b111100111101111},
    {'7', 0b111001010010010},
    {'8', 0b111101111101111},
    {'9', 0b111101111001111},
    {'A', 0b111101111101101},
    {'B', 0b110101111101110},
    {'C', 0b111100100100111},
    {'D', 0b110101101101110},
    {'E', 0b111100111100111},
    {'F', 0b111100111100100},
    {'G', 0b111100101101111},
    {'H', 0b101101111101101},
    {'I', 0b111010010010111},
    {'L', 0b100100100100111},
    {'M', 0b101111111101101},
    {'N', 0b101111111111101},
    {'O', 0b111101101101111},
    {'P', 0b111101111100100},
    {'R', 0b111101110101101},
    {'S', 0b111100111001111},
    {'T', 0b111010010010010},
    {'U', 0b101101101101111},
    {'V', 0b101101101101010},
    {'X', 0b101101010101101},
    {'Y', 0b101101010010010},
    {' ', 0},
    {':', 0b000010000010000},
    {'!', 0b010010010000010},
};

static const Glyph* find_glyph(char c) {
    for (size_t i = 0; i < sizeof(FONT)/sizeof(FONT[0]); i++)
        if (FONT[i].c == c) return &FONT[i];
    return NULL;
}

static void draw_char(int uvx, int uvy, char c) {
    const Glyph *g = find_glyph(c);
    if (!g) return;
    for (int row = 0; row < 5; row++)
        for (int col = 0; col < 3; col++) {
            int bit = (4 - row) * 3 + (2 - col);
            if ((g->bits >> bit) & 1) px_uv(uvx + col, uvy + row, true);
        }
}

static void draw_text(int uvx, int uvy, const char *s) {
    while (*s) {
        draw_char(uvx, uvy, *s);
        uvx += 4; /* 3 + 1 spacing */
        s++;
    }
}

/* ---------- tetrominoes ---------- */
typedef struct { uint16_t rot[4]; } Tetromino;

static const Tetromino PIECES[7] = {
    /* I */ {{ 0x0F00, 0x2222, 0x00F0, 0x4444 }},
    /* O */ {{ 0x0660, 0x0660, 0x0660, 0x0660 }},
    /* T */ {{ 0x0E40, 0x4C40, 0x4E00, 0x4640 }},
    /* S */ {{ 0x06C0, 0x4620, 0x06C0, 0x4620 }},
    /* Z */ {{ 0x0C60, 0x2640, 0x0C60, 0x2640 }},
    /* J */ {{ 0x44C0, 0x8E00, 0x6440, 0x0E20 }},
    /* L */ {{ 0x4460, 0x0E80, 0xC440, 0x2E00 }},
};

static inline bool piece_cell(int kind, int rot, int dx, int dy) {
    uint16_t s = PIECES[kind].rot[rot & 3];
    int bit = (3 - dy) * 4 + (3 - dx);
    return (s >> bit) & 1;
}

/* ---------- game state ---------- */
static int8_t   board[BOARD_H][BOARD_W];
static int      cur_kind, cur_rot, cur_x, cur_y;
static int      next_kind;
static uint32_t score;
static uint16_t lines_total;
static uint8_t  level;
static bool     game_over;
static bool     paused;

/* ---------- drawing ---------- */
static void draw_block(int col, int row, bool on) {
    int uvx = BOARD_OX + col * BS;
    int uvy = BOARD_OY + row * BS;
    fill_uv_rect(uvx, uvy, BS, BS, on);
    if (on) {
        /* hollow center for brick texture */
        px_uv(uvx + 1, uvy + 1, false);
        px_uv(uvx + 2, uvy + 2, false);
    }
}

static void draw_border(void) {
    /* Border around board */
    int x0 = BOARD_OX - 1, y0 = BOARD_OY - 1;
    int w  = BOARD_PXW + 2, h = BOARD_PXH + 2;
    for (int i = 0; i < w; i++) { px_uv(x0 + i, y0, true); px_uv(x0 + i, y0 + h - 1, true); }
    for (int i = 0; i < h; i++) { px_uv(x0, y0 + i, true); px_uv(x0 + w - 1, y0 + i, true); }
}

static void draw_hud_border(void) {
    /* Border around HUD strip: uvx 0..58, uvy HUD_OY..HUD_OY+HUD_H-1 */
    int x0 = 0, y0 = HUD_OY;
    int w  = UV_W, h = HUD_H;
    for (int i = 0; i < w; i++) { px_uv(x0 + i, y0, true); px_uv(x0 + i, y0 + h - 1, true); }
    for (int i = 0; i < h; i++) { px_uv(x0, y0 + i, true); px_uv(x0 + w - 1, y0 + i, true); }
}

static void draw_next_box(void) {
    int x0 = NEXT_OX, y0 = NEXT_OY;
    int w  = 4 * BS_NEXT + 2, h = 4 * BS_NEXT + 2;
    /* clear */
    fill_uv_rect(x0, y0, w, h, false);
    /* border */
    for (int i = 0; i < w; i++) { px_uv(x0 + i, y0, true); px_uv(x0 + i, y0 + h - 1, true); }
    for (int i = 0; i < h; i++) { px_uv(x0, y0 + i, true); px_uv(x0 + w - 1, y0 + i, true); }
    /* piece */
    for (int dy = 0; dy < 4; dy++)
        for (int dx = 0; dx < 4; dx++)
            if (piece_cell(next_kind, 0, dx, dy)) {
                int uvx = x0 + 1 + dx * BS_NEXT;
                int uvy = y0 + 1 + dy * BS_NEXT;
                fill_uv_rect(uvx, uvy, BS_NEXT, BS_NEXT, true);
            }
}

static void draw_hud(void) {
    /* Clear HUD text region (right of NEXT box, inside HUD border) */
    fill_uv_rect(TXT_OX, HUD_OY + 1, UV_W - TXT_OX - 1, HUD_H - 2, false);

    char buf[16];

    /* Row 1: SCORE */
    draw_text(TXT_OX, HUD_OY + 3,  "SCR");
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)score);
    draw_text(TXT_OX + 14, HUD_OY + 3, buf);

    /* Row 2: LINES */
    draw_text(TXT_OX, HUD_OY + 11, "LN");
    snprintf(buf, sizeof(buf), "%u", (unsigned)lines_total);
    draw_text(TXT_OX + 10, HUD_OY + 11, buf);

    /* Row 3: LEVEL */
    draw_text(TXT_OX, HUD_OY + 19, "LV");
    snprintf(buf, sizeof(buf), "%u", (unsigned)level);
    draw_text(TXT_OX + 10, HUD_OY + 19, buf);
}

static void draw_static_labels(void) {
    /* HUD frame is permanent */
    draw_hud_border();
}

static void draw_board_and_piece(void) {
    /* Clear board interior */
    fill_uv_rect(BOARD_OX, BOARD_OY, BOARD_PXW, BOARD_PXH, false);
    /* Locked cells */
    for (int y = 0; y < BOARD_H; y++)
        for (int x = 0; x < BOARD_W; x++)
            if (board[y][x]) draw_block(x, y, true);
    /* Falling piece */
    for (int dy = 0; dy < 4; dy++)
        for (int dx = 0; dx < 4; dx++)
            if (piece_cell(cur_kind, cur_rot, dx, dy)) {
                int x = cur_x + dx, y = cur_y + dy;
                if (y >= 0 && y < BOARD_H && x >= 0 && x < BOARD_W)
                    draw_block(x, y, true);
            }
}

static void redraw_all(void) {
    NuttyDisplay_lockLVGL();
    draw_board_and_piece();
    draw_next_box();
    draw_hud();
    if (img_obj) lv_obj_invalidate(img_obj);
    NuttyDisplay_unlockLVGL();
}

/* ---------- game logic ---------- */
static bool collides(int kind, int rot, int cx, int cy) {
    for (int dy = 0; dy < 4; dy++)
        for (int dx = 0; dx < 4; dx++) {
            if (!piece_cell(kind, rot, dx, dy)) continue;
            int x = cx + dx, y = cy + dy;
            if (x < 0 || x >= BOARD_W) return true;
            if (y >= BOARD_H) return true;
            if (y < 0) continue;
            if (board[y][x]) return true;
        }
    return false;
}

static void lock_piece(void) {
    for (int dy = 0; dy < 4; dy++)
        for (int dx = 0; dx < 4; dx++)
            if (piece_cell(cur_kind, cur_rot, dx, dy)) {
                int x = cur_x + dx, y = cur_y + dy;
                if (y >= 0 && y < BOARD_H && x >= 0 && x < BOARD_W)
                    board[y][x] = 1;
            }
}

static int clear_full_lines(void) {
    int cleared = 0;
    int y = BOARD_H - 1;
    while (y >= 0) {
        bool full = true;
        for (int x = 0; x < BOARD_W; x++) if (!board[y][x]) { full = false; break; }
        if (full) {
            for (int yy = y; yy > 0; yy--)
                memcpy(board[yy], board[yy - 1], sizeof(board[0]));
            memset(board[0], 0, sizeof(board[0]));
            cleared++;
        } else {
            y--;
        }
    }
    return cleared;
}

static void spawn_piece(void) {
    cur_kind = next_kind;
    next_kind = rand() % 7;
    cur_rot = 0;
    cur_x = (BOARD_W - 4) / 2;
    cur_y = -1;
    if (collides(cur_kind, cur_rot, cur_x, cur_y)) {
        game_over = true;
    }
}

static void try_move(int dx, int dy) {
    if (collides(cur_kind, cur_rot, cur_x + dx, cur_y + dy)) return;
    cur_x += dx; cur_y += dy;
}

static void try_rotate(void) {
    int nr = (cur_rot + 1) & 3;
    int kicks[] = { 0, -1, 1, -2, 2 };
    for (size_t i = 0; i < sizeof(kicks)/sizeof(kicks[0]); i++) {
        if (!collides(cur_kind, nr, cur_x + kicks[i], cur_y)) {
            cur_rot = nr;
            cur_x  += kicks[i];
            return;
        }
    }
}

static void hard_drop(void) {
    while (!collides(cur_kind, cur_rot, cur_x, cur_y + 1)) {
        cur_y++;
        score += 2;
    }
}

static uint32_t drop_interval_ms(void) {
    int lv = level;
    if (lv > 15) lv = 15;
    static const uint16_t tbl[] = { 700, 620, 540, 470, 400, 340, 280, 230, 180, 140, 110, 90, 75, 65, 55, 45 };
    return tbl[lv];
}

/* ---------- screens ---------- */
static void draw_centered_message(const char *line1, const char *line2) {
    int len1 = strlen(line1) * 4 - 1;
    int len2 = line2 ? (int)strlen(line2) * 4 - 1 : 0;
    int x1 = BOARD_OX + (BOARD_PXW - len1) / 2;
    int x2 = BOARD_OX + (BOARD_PXW - len2) / 2;
    int y  = BOARD_OY + BOARD_PXH / 2 - (line2 ? 6 : 3);
    fill_uv_rect(BOARD_OX, y - 1, BOARD_PXW, line2 ? 13 : 6, false);
    draw_text(x1, y, line1);
    if (line2) draw_text(x2, y + 7, line2);
}

static void show_splash(void) {
    memset(pxbuf, 1, sizeof(pxbuf));

    /* All text strictly inside the board border (uvy 1..100, uvx 4..53). */
    /* Title */
    draw_text(BOARD_OX + 13,  3, "TETRIS");

    /* "TILT BADGE RIGHT" centered */
    draw_text(BOARD_OX + 13, 14, "TILT");
    draw_text(BOARD_OX + 9,  22, "BADGE");
    draw_text(BOARD_OX + 9,  30, "RIGHT");

    /* Key map */
    draw_text(BOARD_OX + 1,  44, "U:LEFT");
    draw_text(BOARD_OX + 1,  52, "D:RGHT");
    draw_text(BOARD_OX + 1,  60, "L:DROP");
    draw_text(BOARD_OX + 1,  68, "R:ROT");
    draw_text(BOARD_OX + 1,  76, "B:HARD");

    /* Prompt */
    draw_text(BOARD_OX + 9,  88, "PRESS");
    draw_text(BOARD_OX + 5,  96, "ANY KEY");

    draw_border();

    NuttyDisplay_lockLVGL();
    if (img_obj) lv_obj_invalidate(img_obj);
    NuttyDisplay_unlockLVGL();

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while (1) {
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)) break;
        if (NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_SELECT)) {
            game_over = true;
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ---------- main game loop ---------- */
static void run_game(void) {
    memset(board, 0, sizeof(board));
    score = 0;
    lines_total = 0;
    level = 0;
    game_over = false;
    paused = false;

    srand((unsigned)esp_timer_get_time());
    next_kind = rand() % 7;
    spawn_piece();

    /* Initial frame */
    memset(pxbuf, 1, sizeof(pxbuf));
    draw_border();
    draw_static_labels();
    redraw_all();

    int64_t last_drop = esp_timer_get_time();

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);

    while (!game_over) {
        bool dirty = false;

        if (NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_SELECT)) break;

        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_START)) {
            paused = !paused;
            if (paused) {
                NuttyDisplay_lockLVGL();
                draw_centered_message("PAUSE", "STRT GO");
                if (img_obj) lv_obj_invalidate(img_obj);
                NuttyDisplay_unlockLVGL();
            } else {
                last_drop = esp_timer_get_time();
                dirty = true;
            }
        }

        if (paused) { vTaskDelay(pdMS_TO_TICKS(30)); continue; }

        /* Controls (rotated for portrait view):
         *   UP    -> piece LEFT
         *   DOWN  -> piece RIGHT
         *   LEFT  -> soft drop
         *   RIGHT -> rotate (CW)
         */
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
            try_move(-1, 0); dirty = true;
        }
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
            try_move(+1, 0); dirty = true;
        }
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_LEFT)) {
            if (!collides(cur_kind, cur_rot, cur_x, cur_y + 1)) {
                cur_y++; score++; dirty = true;
                last_drop = esp_timer_get_time();
            }
        }
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_RIGHT)) {
            try_rotate(); dirty = true;
        }
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
            try_rotate(); dirty = true;
        }
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B)) {
            hard_drop();
            lock_piece();
            int n = clear_full_lines();
            if (n > 0) {
                static const uint16_t LSCORE[5] = { 0, 100, 300, 500, 800 };
                score       += LSCORE[n] * (level + 1);
                lines_total += n;
                level        = lines_total / 10;
            }
            spawn_piece();
            last_drop = esp_timer_get_time();
            dirty = true;
        }

        /* Gravity */
        int64_t now = esp_timer_get_time();
        if ((now - last_drop) / 1000 >= (int64_t)drop_interval_ms()) {
            last_drop = now;
            if (!collides(cur_kind, cur_rot, cur_x, cur_y + 1)) {
                cur_y++;
            } else {
                lock_piece();
                int n = clear_full_lines();
                if (n > 0) {
                    static const uint16_t LSCORE[5] = { 0, 100, 300, 500, 800 };
                    score       += LSCORE[n] * (level + 1);
                    lines_total += n;
                    level        = lines_total / 10;
                }
                spawn_piece();
            }
            dirty = true;
        }

        if (dirty) redraw_all();
        vTaskDelay(pdMS_TO_TICKS(15));
    }

    if (game_over) {
        NuttyDisplay_lockLVGL();
        draw_centered_message("GAME", "OVER");
        if (img_obj) lv_obj_invalidate(img_obj);
        NuttyDisplay_unlockLVGL();
        char msg[40];
        snprintf(msg, sizeof(msg), "Game Over! Score:%lu", (unsigned long)score);
        NuttySystemMonitor_setSystemTrayTempText(msg, 40);
        NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
        int64_t t0 = esp_timer_get_time();
        while ((esp_timer_get_time() - t0) < 4000000LL) {
            if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)) break;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

/* ---------- app entry ---------- */
static void nutty_main(void) {
    ESP_LOGI(TAG, "Starting NuttyTetris...");

    memset(pxbuf, 1, sizeof(pxbuf));
    img_dsc.header.cf          = LV_IMG_CF_TRUE_COLOR;
    img_dsc.header.always_zero = 0;
    img_dsc.header.reserved    = 0;
    img_dsc.header.w           = 128;
    img_dsc.header.h           = 64;
    img_dsc.data_size          = 128 * 64;
    img_dsc.data               = pxbuf;

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();
    NuttyDisplay_lockLVGL();
    img_obj = lv_img_create(drawArea);
    lv_img_set_src(img_obj, &img_dsc);
    lv_obj_align(img_obj, LV_ALIGN_TOP_LEFT, 0, 0);
    NuttyDisplay_unlockLVGL();

    show_splash();
    if (!game_over) run_game();

    NuttyDisplay_lockLVGL();
    if (img_obj) { lv_obj_del(img_obj); img_obj = NULL; }
    NuttyDisplay_unlockLVGL();
    NuttyDisplay_clearUserAppArea();

    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyTetris = {
    .appName = "Nutty Tetris",
    .appMainEntry = nutty_main,
    .appHidden = false
};

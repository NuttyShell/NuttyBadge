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
 *
 * Layout inside HUD (left to right):
 *   HOLD box (3px cells, 14x14 incl border) | NEXT box (3x14x14) | text column
 */
#define HUD_OY     103
#define HUD_H      25

#define BS_PV      3                       /* preview block size */
#define PV_BOX_W   (4 * BS_PV + 2)         /* 14 */
#define PV_BOX_H   (4 * BS_PV + 2)         /* 14 */

/* HOLD box */
#define HOLD_OX    2
#define HOLD_OY    (HUD_OY + 6)            /* 109; label sits above at y=104 */

/* NEXT box */
#define NEXT_OX    18
#define NEXT_OY    (HUD_OY + 6)            /* 109 */

/* Score text column */
#define TXT_OX     34

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
static int      hold_kind;     /* -1 = empty */
static bool     hold_used;     /* true once you've held this piece */
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

static void draw_preview_box(int x0, int y0, int piece_kind) {
    int w = PV_BOX_W, h = PV_BOX_H;
    /* clear */
    fill_uv_rect(x0, y0, w, h, false);
    /* border */
    for (int i = 0; i < w; i++) { px_uv(x0 + i, y0, true); px_uv(x0 + i, y0 + h - 1, true); }
    for (int i = 0; i < h; i++) { px_uv(x0, y0 + i, true); px_uv(x0 + w - 1, y0 + i, true); }
    /* piece (skip if -1 for empty hold) */
    if (piece_kind < 0) return;
    for (int dy = 0; dy < 4; dy++)
        for (int dx = 0; dx < 4; dx++)
            if (piece_cell(piece_kind, 0, dx, dy)) {
                int uvx = x0 + 1 + dx * BS_PV;
                int uvy = y0 + 1 + dy * BS_PV;
                fill_uv_rect(uvx, uvy, BS_PV, BS_PV, true);
            }
}

static void draw_next_box(void) {
    draw_preview_box(NEXT_OX, NEXT_OY, next_kind);
}

static void draw_hold_box(void) {
    draw_preview_box(HOLD_OX, HOLD_OY, hold_kind);
}

/* Forward decl: defined alongside drop_interval_ms() further down. */
static uint32_t score_speed_tier(uint32_t s);

static void draw_hud(void) {
    /* Clear text region (right of NEXT box, inside HUD border). */
    fill_uv_rect(TXT_OX, HUD_OY + 1, UV_W - TXT_OX - 1, HUD_H - 2, false);

    char buf[16];

    /* Stat column (TXT_OX..UV_W-2 = ~24 px wide).
     * 5-px-tall glyphs; we stack four rows on 6-px pitch:
     *   y=HUD_OY+1  : "SCR" caption
     *   y=HUD_OY+7  : score value (full width, up to 6 digits)
     *   y=HUD_OY+13 : "LN" + lines_total (label and number on same row)
     *   y=HUD_OY+19 : "LV" + speed tier (label and number on same row)
     *
     * The displayed LV reflects the score-based speed tier (0..6) from the
     * 30%-faster-per-500-points table, NOT the legacy line-based level
     * (which is still tracked internally for the line-clear scoring bonus).
     */
    int y_scr_lbl  = HUD_OY + 1;
    int y_scr_val  = HUD_OY + 7;
    int y_lines    = HUD_OY + 13;
    int y_level    = HUD_OY + 19;

    /* Score: clamp display to 6 digits (max 999999) so the value never
     * overruns the HUD column. The score variable itself keeps counting. */
    draw_text(TXT_OX, y_scr_lbl, "SCR");
    uint32_t shown_score = score > 999999u ? 999999u : score;
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)shown_score);
    draw_text(TXT_OX, y_scr_val, buf);

    /* Lines: "LN" + number on the same row. Up to 4 digits fit (LN9999). */
    draw_text(TXT_OX, y_lines, "LN");
    unsigned shown_lines = lines_total > 9999u ? 9999u : lines_total;
    snprintf(buf, sizeof(buf), "%u", shown_lines);
    draw_text(TXT_OX + 9, y_lines, buf);

    /* Level: speed tier from the score-based table, capped at the table max. */
    draw_text(TXT_OX, y_level, "LV");
    unsigned shown_level = (unsigned)score_speed_tier(score);
    snprintf(buf, sizeof(buf), "%u", shown_level);
    draw_text(TXT_OX + 9, y_level, buf);
}

static void draw_static_labels(void) {
    /* HUD frame */
    draw_hud_border();
    /* Captions above the two preview boxes */
    draw_text(HOLD_OX + 1, HUD_OY + 1, "HLD");
    draw_text(NEXT_OX + 1, HUD_OY + 1, "NXT");
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
    draw_hold_box();
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
    hold_used = false;
    if (collides(cur_kind, cur_rot, cur_x, cur_y)) {
        game_over = true;
    }
}

/* Spawn a specific piece (used after swapping out of hold). Sets game_over on collision. */
static void spawn_specific(int kind) {
    cur_kind = kind;
    cur_rot = 0;
    cur_x = (BOARD_W - 4) / 2;
    cur_y = -1;
    if (collides(cur_kind, cur_rot, cur_x, cur_y)) {
        game_over = true;
    }
}

static bool do_hold(void) {
    if (hold_used) return false;       /* one swap per piece */
    int incoming = hold_kind;
    hold_kind = cur_kind;
    if (incoming < 0) {
        /* Hold was empty: stash current, draw from NEXT */
        spawn_piece();
    } else {
        /* Swap with previously held piece */
        spawn_specific(incoming);
    }
    hold_used = true;                  /* re-set false on next natural spawn */
    return true;
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

/* Score-based speed-up tiers.
 * Every 500 points the gravity interval is multiplied by 0.7 (i.e. the block
 * falls 30% faster). Tiers saturate after score >= 3000 (6 steps), keeping
 * the game playable rather than turning into a slideshow at extreme scores.
 *
 *   score     tier   speed factor (relative to base drop interval at this level)
 *   <500       0     1.000
 *   500..999   1     0.700
 *   1000..1499 2     0.490
 *   1500..1999 3     0.343
 *   2000..2499 4     0.240
 *   2500..2999 5     0.168
 *   >=3000     6     0.118  (capped)
 */
#define TETRIS_SCORE_SPEED_STEP        500u
#define TETRIS_SCORE_SPEED_MAX_TIER    6u
#define TETRIS_DROP_INTERVAL_FLOOR_MS  20u

static uint32_t score_speed_tier(uint32_t s) {
    uint32_t tier = s / TETRIS_SCORE_SPEED_STEP;
    if (tier > TETRIS_SCORE_SPEED_MAX_TIER) tier = TETRIS_SCORE_SPEED_MAX_TIER;
    return tier;
}

static uint32_t drop_interval_ms(void) {
    int lv = level;
    if (lv > 15) lv = 15;
    static const uint16_t tbl[] = { 700, 620, 540, 470, 400, 340, 280, 230, 180, 140, 110, 90, 75, 65, 55, 45 };
    uint32_t interval = tbl[lv];

    /* Apply score-based 30%-faster-per-500-points multiplier (integer-only:
     * each tier multiplies by 7/10). */
    uint32_t tier = score_speed_tier(score);
    for (uint32_t i = 0; i < tier; i++) {
        interval = (interval * 7u) / 10u;
    }
    if (interval < TETRIS_DROP_INTERVAL_FLOOR_MS) {
        interval = TETRIS_DROP_INTERVAL_FLOOR_MS;
    }
    return interval;
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

/* Splash result */
typedef enum { SPLASH_START, SPLASH_EXIT } splash_result_t;

static splash_result_t show_splash(void) {
    memset(pxbuf, 1, sizeof(pxbuf));

    /* All text strictly inside the board border (uvy 1..100, uvx 4..53). */
    /* Title */
    draw_text(BOARD_OX + 13,  2, "TETRIS");

    /* "TILT BADGE RIGHT" centered */
    draw_text(BOARD_OX + 13, 11, "TILT");
    draw_text(BOARD_OX + 9,  19, "BADGE");
    draw_text(BOARD_OX + 9,  27, "RIGHT");

    /* Key map */
    draw_text(BOARD_OX + 1,  39, "U:LEFT");
    draw_text(BOARD_OX + 1,  47, "D:RGHT");
    draw_text(BOARD_OX + 1,  55, "L:DROP");
    draw_text(BOARD_OX + 1,  63, "R:ROT");
    draw_text(BOARD_OX + 1,  71, "B:HARD");
    draw_text(BOARD_OX + 1,  79, "X:HOLD");

    /* Prompt */
    draw_text(BOARD_OX + 9,  88, "PRESS");
    draw_text(BOARD_OX + 5,  95, "ANY KEY");

    draw_border();

    NuttyDisplay_lockLVGL();
    if (img_obj) lv_obj_invalidate(img_obj);
    NuttyDisplay_unlockLVGL();

    /* Drain stale button events: wait until every button is released, then clear. */
    int64_t drain_deadline = esp_timer_get_time() + 400000LL; /* up to 400ms */
    while (esp_timer_get_time() < drain_deadline) {
        if (!NuttyInput_isOneOfTheButtonsCurrentlyPressed(NUTTYINPUT_BTN_ALL)) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);

    while (1) {
        /* Long-hold SELECT exits to main launcher. */
        if (NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_SELECT)) {
            return SPLASH_EXIT;
        }
        /* Any other tap-and-release starts the game. We exclude SELECT here so a
         * tap of SELECT doesn't start the game (it should only long-hold to exit). */
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(
                NUTTYINPUT_BTN_ALL & ~NUTTYINPUT_BTN_SELECT)) {
            return SPLASH_START;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* run_game return values */
typedef enum { GAME_END_OVER, GAME_END_USER_QUIT } game_end_t;

/* ---------- main game loop ---------- */
static game_end_t run_game(void) {
    memset(board, 0, sizeof(board));
    score = 0;
    lines_total = 0;
    level = 0;
    game_over = false;
    paused = false;
    hold_kind = -1;
    hold_used = false;

    srand((unsigned)esp_timer_get_time());
    next_kind = rand() % 7;
    spawn_piece();

    /* Initial frame */
    memset(pxbuf, 1, sizeof(pxbuf));
    draw_border();
    draw_static_labels();
    redraw_all();

    int64_t last_drop = esp_timer_get_time();

    /* Drain any held buttons from the splash before starting input loop. */
    int64_t drain_deadline = esp_timer_get_time() + 400000LL;
    while (esp_timer_get_time() < drain_deadline) {
        if (!NuttyInput_isOneOfTheButtonsCurrentlyPressed(NUTTYINPUT_BTN_ALL)) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);

    bool user_quit = false;

    while (!game_over) {
        bool dirty = false;

        if (NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_SELECT)) {
            user_quit = true;
            break;
        }

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
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_USRDEF)) {
            if (do_hold()) { dirty = true; last_drop = esp_timer_get_time(); }
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
        /* Drain held buttons so the next input is a real new press. */
        int64_t d = esp_timer_get_time() + 400000LL;
        while (esp_timer_get_time() < d) {
            if (!NuttyInput_isOneOfTheButtonsCurrentlyPressed(NUTTYINPUT_BTN_ALL)) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
        int64_t t0 = esp_timer_get_time();
        while ((esp_timer_get_time() - t0) < 4000000LL) {
            if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)) break;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        return GAME_END_OVER;
    }
    return user_quit ? GAME_END_USER_QUIT : GAME_END_OVER;
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

    /* Loop:
     *   splash --(any key)--> game --(game-over)--> splash
     *                              \--(long-hold SELECT)--> exit
     *   splash --(long-hold SELECT)--> exit
     */
    while (1) {
        splash_result_t s = show_splash();
        if (s == SPLASH_EXIT) break;

        game_end_t g = run_game();
        if (g == GAME_END_USER_QUIT) break;
        /* GAME_END_OVER: fall through and re-show splash */
    }

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

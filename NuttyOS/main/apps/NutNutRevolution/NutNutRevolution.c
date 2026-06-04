#include "NutNutRevolution.h"
#include "songs/butterfly_nnr.h"
#include "songs/night_of_fire_nnr.h"
#include "services/NuttyStorage/NuttyStorage.h"
#include "audio_player.h"
#include "drivers/pwm_audio.h"
#include "drivers/rgb.h"
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <strings.h>

static const char *TAG = "NutNutRevolution";

/* ── NNR binary format ─────────────────────────────────────────────
   Header 76 B : "NNR1"(4) title(32) artist(32) bpm(4f)
                 offset_ms(2h) num_diffs(1B) reserved(1B)
   Diff   16 B : name(8) meter(1B) reserved(1B) note_count(2H) data_offset(4I)
   Note    3 B : delta_ms(2H) columns(1B)  -- delta-encoded from song start
   columns: bit0=L  bit1=D  bit2=U  bit3=R
   ─────────────────────────────────────────────────────────────────*/
#define NNR_HEADER_SZ    76
#define NNR_DIFF_SZ      16
#define NNR_MAX_DIFFS     8
#define NNR_MAX_NOTES   700

/* ── display / game constants ──────────────────────────────────────*/
#define FB_W              128
#define FB_H               64
#define NNR_APPROACH_MS   800
#define NNR_PERFECT_MS     45
#define NNR_GOOD_MS        90
#define NNR_TICK_MS        33   /* ~30 fps */

#define NNR_PLAYFIELD_TOP   5   /* y below lane labels     */
#define NNR_HITZONE_Y      44   /* y of hit line           */
#define NNR_RECEPTOR_Y     44   /* receptor top (= hit line) */
#define NNR_RECEPTOR_H     12
#define NNR_PLAYFIELD_H    39   /* HITZONE_Y - PLAYFIELD_TOP = 44-5 */
#define NNR_NOTE_H         12   /* arrow height (timing + visual)   */
#define NNR_ARROW_W        12   /* visual arrow width               */
#define NNR_ARROW_XPAD     10   /* (32-12)/2 — centers arrow in lane */
#define NNR_SCORE_PERFECT 300
#define NNR_SCORE_GOOD    100
#define NNR_FLASH_TICKS     6   /* ~200 ms receptor flash  */

/* SD card base path.  Each sub-folder is one song:
     /sdcard/NutNutRevolution/songs/<name>/<NAME>.nnr
     /sdcard/NutNutRevolution/songs/<name>/<NAME>.mp3   */
#define NNR_SONGS_BASE  SDCARD_MOUNT_POINT "/NutNutRevolution/songs"
#define NNR_MAX_SONGS   16
#define NNR_MENU_MAX    (NNR_MAX_SONGS + 2)

/* PWM ring buffer is 4096 bytes; at 48 kHz 8-bit that is 85 ms of audio.
   game_start_us is shifted forward by this amount so that chart time 0
   aligns with when the first sample actually reaches the speaker.
   Increase this value if notes appear too early; decrease if too late. */
#define NNR_AUDIO_LATENCY_MS  85

#define NNR_AUDIO_CHUNK  256

/* ── types ─────────────────────────────────────────────────────────*/
typedef struct {
    char title[33]; char artist[33];
    float bpm; int16_t offset_ms; uint8_t num_diffs;
} NNRSongInfo;

typedef struct {
    char name[9]; uint8_t meter; uint16_t note_count; uint32_t data_offset;
} NNRDiffInfo;

typedef enum { NOTE_PENDING=0, NOTE_PERFECT, NOTE_GOOD, NOTE_MISSED } NoteState;

typedef struct {
    int32_t time_ms; uint8_t col; uint8_t state;
} GameNote;

/* ── song descriptor ─────────────────────────────────────────────────
   flash_data != NULL  → chart lives in flash  (built-in)
   flash_data == NULL  → chart is loaded from chart_path on SD card
   audio_path empty    → play silently                                */
typedef struct {
    char  title[33];
    char  artist[33];
    char  chart_path[256]; /* SD card .nnr; ignored when flash_data set */
    char  audio_path[256]; /* SD card .mp3; empty = silent              */
    const uint8_t *flash_data;
    size_t         flash_size;
} NNRSong;

static NNRSong g_songs[NNR_MAX_SONGS];
static int     g_song_count = 0;

/* ── static game buffers ────────────────────────────────────────────*/
static uint8_t      game_fb[FB_W * FB_H];
static lv_img_dsc_t game_img_dsc;
static lv_obj_t    *game_img_obj = NULL;
static GameNote     g_notes[NNR_MAX_NOTES];

/* ── audio state ────────────────────────────────────────────────────*/
static struct {
    bool     ready;
    uint32_t channels;
    uint32_t sample_rate;
    int32_t  dc_x1, dc_y1;
    uint32_t fade_left;
} g_audio = {0};

/* ── audio callbacks (same pipeline as NuttyAudioPlayer) ───────────*/
static esp_err_t nnr_audio_reconfig(uint32_t rate, uint32_t bits,
                                     i2s_slot_mode_t ch)
{
    if (bits != 16) return ESP_ERR_NOT_SUPPORTED;
    g_audio.channels = (ch == I2S_SLOT_MODE_MONO) ? 1u : 2u;
    if (g_audio.sample_rate == rate) {
        pwm_audio_status_t st = PWM_AUDIO_STATUS_UN_INIT;
        pwm_audio_get_status(&st);
        if (st == PWM_AUDIO_STATUS_BUSY) return ESP_OK;
    }
    g_audio.sample_rate = rate;
    pwm_audio_stop();
    pwm_audio_set_param((int)rate, LEDC_TIMER_8_BIT, 1);
    pwm_audio_start();
    pwm_audio_set_volume(24);
    g_audio.dc_x1 = 0; g_audio.dc_y1 = 0; g_audio.fade_left = 240;
    return ESP_OK;
}

static esp_err_t nnr_audio_write(void *buf, size_t len,
                                  size_t *written, uint32_t timeout_ms)
{
    *written = 0;
    if (!buf || !g_audio.channels) return ESP_ERR_INVALID_STATE;
    size_t frame_bytes = sizeof(int16_t) * g_audio.channels;
    size_t frames = len / frame_bytes;
    const int16_t *src = (const int16_t *)buf;
    uint8_t out[NNR_AUDIO_CHUNK];
    size_t done = 0;
    while (done < frames) {
        size_t chunk = frames - done;
        if (chunk > NNR_AUDIO_CHUNK) chunk = NNR_AUDIO_CHUNK;
        for (size_t i = 0; i < chunk; i++) {
            int32_t mono;
            if (g_audio.channels == 2u) {
                size_t b = (done + i) * 2u;
                mono = ((int32_t)src[b] + (int32_t)src[b+1]) / 2;
            } else {
                mono = src[done + i];
            }
            /* DC blocker */
            int32_t in = mono;
            mono = in - g_audio.dc_x1 + g_audio.dc_y1 - (g_audio.dc_y1 >> 8);
            g_audio.dc_x1 = in; g_audio.dc_y1 = mono;
            /* Fade-in on start to kill pop */
            if (g_audio.fade_left > 0) {
                mono = (int32_t)((int64_t)mono * (240 - g_audio.fade_left) / 240);
                g_audio.fade_left--;
            }
            /* Soft limiter */
            if (mono >  28000) mono =  28000 + ((mono - 28000) >> 2);
            if (mono < -28000) mono = -28000 + ((mono + 28000) >> 2);
            out[i] = (uint8_t)(((mono + 32768) >> 8) & 0xFF);
        }
        size_t nw = 0;
        pwm_audio_write(out, chunk, &nw, pdMS_TO_TICKS(timeout_ms));
        done += nw;
        if (nw < chunk) break;
    }
    *written = done * frame_bytes;
    return ESP_OK;
}

static void nnr_audio_init(void)
{
    if (g_audio.ready) return;
    g_audio.channels    = 1;
    g_audio.sample_rate = 48000;
    g_audio.fade_left   = 240;
    pwm_audio_status_t st = PWM_AUDIO_STATUS_UN_INIT;
    pwm_audio_get_status(&st);
    if (st != PWM_AUDIO_STATUS_BUSY) {
        pwm_audio_set_param(48000, LEDC_TIMER_8_BIT, 1);
        pwm_audio_start();
    }
    pwm_audio_set_volume(24);
    audio_player_config_t cfg = {
        .mute_fn      = NULL,
        .clk_set_fn   = nnr_audio_reconfig,
        .write_fn     = nnr_audio_write,
        .priority     = tskIDLE_PRIORITY + 4,
        .coreID       = 0,
        .force_stereo = false,
        .write_fn2    = NULL,
        .write_ctx    = NULL,
    };
    audio_player_new(cfg); /* no-op if already initialized */
    g_audio.ready = true;
}

/* Returns true if playback started. fp ownership transferred on success. */
static bool nnr_audio_play(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) { ESP_LOGW(TAG, "Cannot open: %s", path); return false; }
    g_audio.dc_x1 = 0; g_audio.dc_y1 = 0; g_audio.fade_left = 240;
    if (audio_player_play(fp) != ESP_OK) { fclose(fp); return false; }
    return true;
}

static void nnr_audio_stop(void) { audio_player_stop(); }

/* ── NNR parser ─────────────────────────────────────────────────────*/
static bool nnr_parse_header(const uint8_t *d, size_t sz,
                              NNRSongInfo *si, NNRDiffInfo *di, uint8_t max_di)
{
    if (sz < (size_t)NNR_HEADER_SZ || memcmp(d,"NNR1",4)) return false;
    memcpy(si->title,  d+4,  32); si->title[32]  = '\0';
    memcpy(si->artist, d+36, 32); si->artist[32] = '\0';
    memcpy(&si->bpm,       d+68, 4);
    memcpy(&si->offset_ms, d+72, 2);
    si->num_diffs = d[74];
    uint8_t nd = si->num_diffs < max_di ? si->num_diffs : max_di;
    for (uint8_t i = 0; i < nd; i++) {
        int base = NNR_HEADER_SZ + i*NNR_DIFF_SZ;
        if ((size_t)(base+NNR_DIFF_SZ) > sz) break;
        memcpy(di[i].name, d+base, 8); di[i].name[8] = '\0';
        di[i].meter = d[base+8];
        memcpy(&di[i].note_count,  d+base+10, 2);
        memcpy(&di[i].data_offset, d+base+12, 4);
    }
    return true;
}

static int nnr_load_notes(const uint8_t *d, size_t sz,
                           const NNRDiffInfo *di, GameNote *out, int max)
{
    int cnt = 0; int32_t ms = 0;
    for (uint16_t i = 0; i < di->note_count; i++) {
        uint32_t off = di->data_offset + (uint32_t)i*3;
        if (off+3 > sz) break;
        uint16_t delta; memcpy(&delta, d+off, 2);
        uint8_t  cols = d[off+2];
        ms += (int32_t)delta;
        for (int col = 0; col < 4 && cnt < max; col++) {
            if (cols & (1<<col)) {
                out[cnt].time_ms = ms;
                out[cnt].col     = (uint8_t)col;
                out[cnt].state   = NOTE_PENDING;
                cnt++;
            }
        }
    }
    return cnt;
}

/* ── SD card song management ────────────────────────────────────────*/

/* Load an entire file from SD into a heap buffer.  Caller must free(). */
static uint8_t *nnr_load_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0 || sz > 64*1024) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *out_size = (size_t)sz;
    return buf;
}

/* Scan NNR_SONGS_BASE and append found songs to g_songs[]. */
static void scan_sd_songs(void)
{
    if (!NuttyStorage_isSDCardMounted()) return;

    DIR *base = opendir(NNR_SONGS_BASE);
    if (!base) return;

    struct dirent *entry;
    while (g_song_count < NNR_MAX_SONGS && (entry = readdir(base)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        /* Check the entry is a directory */
        char folder[256];
        snprintf(folder, sizeof(folder), "%s/%.64s", NNR_SONGS_BASE, entry->d_name);
        struct stat st;
        if (stat(folder, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        /* Scan sub-folder for .nnr and .mp3 */
        char nnr_path[256] = {0};
        char mp3_path[256] = {0};
        DIR *sdir = opendir(folder);
        if (!sdir) continue;
        struct dirent *se;
        while ((se = readdir(sdir)) != NULL) {
            size_t nl = strlen(se->d_name);
            if (nl > 4) {
                if (strcasecmp(se->d_name + nl - 4, ".nnr") == 0)
                    snprintf(nnr_path, sizeof(nnr_path), "%.190s/%.64s", folder, se->d_name);
                else if (strcasecmp(se->d_name + nl - 4, ".mp3") == 0)
                    snprintf(mp3_path, sizeof(mp3_path), "%.190s/%.64s", folder, se->d_name);
            }
        }
        closedir(sdir);
        if (!nnr_path[0]) continue; /* no chart — skip */

        NNRSong *s = &g_songs[g_song_count];
        memset(s, 0, sizeof(*s));

        /* Read NNR header to get title / artist */
        uint8_t hdr[NNR_HEADER_SZ];
        FILE *f = fopen(nnr_path, "rb");
        if (f && fread(hdr, 1, NNR_HEADER_SZ, f) == NNR_HEADER_SZ
               && memcmp(hdr, "NNR1", 4) == 0) {
            memcpy(s->title,  hdr+4,  32); s->title[32]  = '\0';
            memcpy(s->artist, hdr+36, 32); s->artist[32] = '\0';
        } else {
            strncpy(s->title, entry->d_name, 32); /* fallback: folder name */
        }
        if (f) fclose(f);

        strncpy(s->chart_path, nnr_path, sizeof(s->chart_path)-1);
        strncpy(s->audio_path, mp3_path, sizeof(s->audio_path)-1);
        s->flash_data = NULL;
        s->flash_size = 0;
        g_song_count++;
    }
    closedir(base);
}

/* Build the full song list: built-in first, then SD card songs. */
static void populate_songs(void)
{
    g_song_count = 0;

    /* ── Built-in songs (chart from flash, audio from SD) ── */
    NNRSong *b;

    b = &g_songs[g_song_count++];
    memset(b, 0, sizeof(*b));
    strncpy(b->title,  "BUTTERFLY", 32);
    strncpy(b->artist, "SMILE.dk",  32);
    snprintf(b->audio_path, sizeof(b->audio_path),
             "%s/butterfly/BUTTERFLY.mp3", NNR_SONGS_BASE);
    b->flash_data = butterfly_nnr_data;
    b->flash_size = BUTTERFLY_NNR_SIZE;

    b = &g_songs[g_song_count++];
    memset(b, 0, sizeof(*b));
    strncpy(b->title,  "NIGHT OF FIRE", 32);
    strncpy(b->artist, "MIKO",         32);
    snprintf(b->audio_path, sizeof(b->audio_path),
             "%s/night of fire/Night Of Fire.mp3", NNR_SONGS_BASE);
    b->flash_data = night_of_fire_nnr_data;
    b->flash_size = NIGHT_OF_FIRE_NNR_SIZE;

    /* ── SD card songs ── */
    scan_sd_songs();
}

/* ── pixel helpers ──────────────────────────────────────────────────*/
static inline void fb_px(int x, int y) {
    if (x>=0 && x<FB_W && y>=0 && y<FB_H) game_fb[y*FB_W+x] = 0;
}
static void fb_hline(int x, int y, int w) {
    for (int i=0;i<w;i++) fb_px(x+i,y);
}
static void fb_vline(int x, int y, int h) {
    for (int i=0;i<h;i++) fb_px(x,y+i);
}
static void fb_rect(int x, int y, int w, int h) {
    for (int dy=0;dy<h;dy++) fb_hline(x,y+dy,w);
}
static void fb_hollow(int x, int y, int w, int h) {
    fb_hline(x,y,w); fb_hline(x,y+h-1,w);
    fb_vline(x,y,h); fb_vline(x+w-1,y,h);
}

/* ── Arrow drawing — 12 × 12 px ⇦ ⇩ ⇧ ⇨  ──────────────────────────
   ax = lane_x + NNR_ARROW_XPAD   ay = note top y

   ⇦  shaft connector at col 5 (full height), body cols 5-11 rows 4-7,
      head diagonal col 4→0, tip at col 0 rows 5-6
   ⇨  horizontal mirror of ⇦  (shaft at col 6, tip at col 11)
   ⇩  shaft cols 3-8 rows 0-5, wing-caps at row 6, diagonal to 2px tip
   ⇧  vertical mirror of ⇩
   ─────────────────────────────────────────────────────────────────*/
static void draw_arrow(int ax, int ay, int col)
{
    switch (col) {
    case 0: /* ⇦ */
        fb_px(ax+5, ay+0);
        fb_px(ax+4,ay+1);  fb_px(ax+5, ay+1);
        fb_px(ax+3,ay+2);  fb_px(ax+5, ay+2);
        fb_px(ax+2,ay+3);  fb_px(ax+5, ay+3);
        fb_px(ax+1,ay+4);  fb_hline(ax+5, ay+4, 7);   /* body top   */
        fb_px(ax,  ay+5);  fb_px(ax+11, ay+5);        /* tip + right*/
        fb_px(ax,  ay+6);  fb_px(ax+11, ay+6);
        fb_px(ax+1,ay+7);  fb_hline(ax+5, ay+7, 7);   /* body bottom*/
        fb_px(ax+2,ay+8);  fb_px(ax+5, ay+8);
        fb_px(ax+3,ay+9);  fb_px(ax+5, ay+9);
        fb_px(ax+4,ay+10); fb_px(ax+5, ay+10);
        fb_px(ax+5, ay+11);
        break;
    case 1: /* ⇩ */
        fb_hline(ax+4, ay+0, 4);                           /* shaft top  */
        fb_px(ax+4,ay+1);  fb_px(ax+7, ay+1);
        fb_px(ax+4,ay+2);  fb_px(ax+7, ay+2);
        fb_px(ax+4,ay+3);  fb_px(ax+7, ay+3);
        fb_px(ax+4,ay+4);  fb_px(ax+7, ay+4);
        fb_px(ax+4,ay+5);  fb_px(ax+7, ay+5);
        fb_hline(ax,   ay+6, 5); fb_hline(ax+7, ay+6, 5); /* wing caps  */
        fb_px(ax+1,ay+7);  fb_px(ax+10,ay+7);
        fb_px(ax+2,ay+8);  fb_px(ax+9, ay+8);
        fb_px(ax+3,ay+9);  fb_px(ax+8, ay+9);
        fb_px(ax+4,ay+10); fb_px(ax+7, ay+10);
        fb_px(ax+5,ay+11); fb_px(ax+6, ay+11);            /* 2px tip    */
        break;
    case 2: /* ⇧  vertical mirror of ⇩ */
        fb_px(ax+5,ay+0);  fb_px(ax+6, ay+0);             /* 2px tip    */
        fb_px(ax+4,ay+1);  fb_px(ax+7, ay+1);
        fb_px(ax+3,ay+2);  fb_px(ax+8, ay+2);
        fb_px(ax+2,ay+3);  fb_px(ax+9, ay+3);
        fb_px(ax+1,ay+4);  fb_px(ax+10,ay+4);
        fb_hline(ax,   ay+5, 5); fb_hline(ax+7, ay+5, 5); /* wing caps  */
        fb_px(ax+4,ay+6);  fb_px(ax+7, ay+6);
        fb_px(ax+4,ay+7);  fb_px(ax+7, ay+7);
        fb_px(ax+4,ay+8);  fb_px(ax+7, ay+8);
        fb_px(ax+4,ay+9);  fb_px(ax+7, ay+9);
        fb_px(ax+4,ay+10); fb_px(ax+7, ay+10);
        fb_hline(ax+4, ay+11, 4);                          /* shaft bottom*/
        break;
    case 3: /* ⇨  horizontal mirror of ⇦ */
        fb_px(ax+6, ay+0);
        fb_px(ax+6,ay+1);  fb_px(ax+7, ay+1);
        fb_px(ax+6,ay+2);  fb_px(ax+8, ay+2);
        fb_px(ax+6,ay+3);  fb_px(ax+9, ay+3);
        fb_hline(ax,   ay+4, 7); fb_px(ax+10, ay+4);  /* body top   */
        fb_px(ax,  ay+5);  fb_px(ax+11, ay+5);        /* left + tip */
        fb_px(ax,  ay+6);  fb_px(ax+11, ay+6);
        fb_hline(ax,   ay+7, 7); fb_px(ax+10, ay+7);  /* body bottom*/
        fb_px(ax+6,ay+8);  fb_px(ax+9, ay+8);
        fb_px(ax+6,ay+9);  fb_px(ax+8, ay+9);
        fb_px(ax+6,ay+10); fb_px(ax+7, ay+10);
        fb_px(ax+6, ay+11);
        break;
    }
}

/* Solid-filled versions — receptor flash only */
static void draw_arrow_flash(int ax, int ay, int col)
{
    switch (col) {
    case 0: /* ⇦ filled */
        fb_px        (ax+5,            ay+0);
        fb_hline(ax+4, ay+1,  2);
        fb_hline(ax+3, ay+2,  3);
        fb_hline(ax+2, ay+3,  4);
        fb_hline(ax+1, ay+4, 11);
        fb_hline(ax,   ay+5, 12);
        fb_hline(ax,   ay+6, 12);
        fb_hline(ax+1, ay+7, 11);
        fb_hline(ax+2, ay+8,  4);
        fb_hline(ax+3, ay+9,  3);
        fb_hline(ax+4, ay+10, 2);
        fb_px        (ax+5,            ay+11);
        break;
    case 1: /* ⇩ filled */
        fb_hline(ax+4, ay+0,  4);
        fb_hline(ax+4, ay+1,  4); fb_hline(ax+4, ay+2,  4);
        fb_hline(ax+4, ay+3,  4); fb_hline(ax+4, ay+4,  4);
        fb_hline(ax+4, ay+5,  4);
        fb_hline(ax,   ay+6, 12);
        fb_hline(ax+1, ay+7, 10);
        fb_hline(ax+2, ay+8,  8);
        fb_hline(ax+3, ay+9,  6);
        fb_hline(ax+4, ay+10, 4);
        fb_hline(ax+5, ay+11, 2);
        break;
    case 2: /* ⇧ filled */
        fb_hline(ax+5, ay+0,  2);
        fb_hline(ax+4, ay+1,  4);
        fb_hline(ax+3, ay+2,  6);
        fb_hline(ax+2, ay+3,  8);
        fb_hline(ax+1, ay+4, 10);
        fb_hline(ax,   ay+5, 12);
        fb_hline(ax+4, ay+6,  4); fb_hline(ax+4, ay+7,  4);
        fb_hline(ax+4, ay+8,  4); fb_hline(ax+4, ay+9,  4);
        fb_hline(ax+4, ay+10, 4);
        fb_hline(ax+4, ay+11, 4);
        break;
    case 3: /* ⇨ filled */
        fb_px        (ax+6,            ay+0);
        fb_hline(ax+6, ay+1,  2);
        fb_hline(ax+6, ay+2,  3);
        fb_hline(ax+6, ay+3,  4);
        fb_hline(ax,   ay+4, 11);
        fb_hline(ax,   ay+5, 12);
        fb_hline(ax,   ay+6, 12);
        fb_hline(ax,   ay+7, 11);
        fb_hline(ax+6, ay+8,  4);
        fb_hline(ax+6, ay+9,  3);
        fb_hline(ax+6, ay+10, 2);
        fb_px        (ax+6,            ay+11);
        break;
    }
}

/* ── renderer ────────────────────────────────────────────────────────*/
static void render_frame(int total, int32_t cur_ms, const uint8_t *flash)
{
    memset(game_fb, 1, sizeof(game_fb));

    /* Receptors — ⇦⇩⇧⇨ outline; briefly filled solid on hit */
    for (int lane = 0; lane < 4; lane++) {
        int ax = lane*32 + NNR_ARROW_XPAD;
        if (flash[lane])
            draw_arrow_flash(ax, NNR_RECEPTOR_Y, lane);
        else
            draw_arrow(ax, NNR_RECEPTOR_Y, lane);
    }

    /* Notes — ⇦⇩⇧⇨ outline arrows falling downward */
    for (int i = 0; i < total; i++) {
        if (g_notes[i].state != NOTE_PENDING) continue;
        int32_t td = g_notes[i].time_ms - cur_ms;
        if (td > NNR_APPROACH_MS || td < -NNR_GOOD_MS) continue;

        int ny = NNR_PLAYFIELD_TOP +
            (int)((int64_t)NNR_PLAYFIELD_H * (NNR_APPROACH_MS - td) / NNR_APPROACH_MS);

        /* Only skip if completely above or past the receptor */
        if (ny + NNR_NOTE_H < NNR_PLAYFIELD_TOP) continue;
        if (ny > NNR_RECEPTOR_Y + NNR_RECEPTOR_H) continue;

        draw_arrow(g_notes[i].col*32 + NNR_ARROW_XPAD, ny, g_notes[i].col);
    }

    NuttyDisplay_lockLVGL();
    lv_obj_invalidate(game_img_obj);
    NuttyDisplay_unlockLVGL();
}

/* ── shared LVGL click callback ─────────────────────────────────────
   LV_LABEL_LONG_SCROLL_CIRCULAR causes lv_label_get_text() to return a
   modified (duplicated) internal string, so text comparison is unreliable.
   Use an index stored per-item instead.                               */
typedef struct { volatile int *out; int idx; } NNRItemCtx;

static void nnr_click_cb(lv_event_t *e)
{
    NNRItemCtx *ctx = (NNRItemCtx *)e->user_data;
    *ctx->out = ctx->idx;
}

/* ── helper: build + run a LVGL menu, returns selected index (0-based)
   Returns -1 on error.                                                */
static int nnr_menu(lv_obj_t *da, const char **items, uint8_t count)
{
    if (count == 0 || count > NNR_MENU_MAX) return -1;
    volatile int result = -2;          /* -2 = nothing clicked yet */
    NNRItemCtx ctxs[NNR_MENU_MAX];

    NuttyInputLVGLInputMapping map = {
        .UP=LV_KEY_PREV, .DOWN=LV_KEY_NEXT, .A=LV_KEY_ENTER
    };
    lv_indev_t *indev = NuttyInput_UpdateLVGLInDev(map);
    assert(indev != NULL);
    lv_group_t *g = lv_group_create(); assert(g != NULL);
    lv_group_set_default(g);
    lv_indev_set_group(indev, g);

    lv_style_t ts; lv_style_init(&ts);
    lv_style_set_text_font(&ts, &lv_font_montserrat_12);

    NuttyDisplay_lockLVGL();
    lv_obj_t *menu = lv_menu_create(da);
    lv_obj_set_size(menu, lv_obj_get_width(da), lv_obj_get_height(da));
    lv_obj_center(menu);
    lv_obj_t *page = lv_menu_page_create(menu, NULL);
    for (uint8_t i = 0; i < count; i++) {
        ctxs[i].out = &result;
        ctxs[i].idx = (int)i;
        lv_obj_t *cont = lv_menu_cont_create(page);
        lv_obj_t *lbl  = lv_label_create(cont);
        lv_label_set_text(lbl, items[i]);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(lbl, lv_pct(100));
        lv_obj_add_style(lbl, &ts, LV_PART_MAIN);
        lv_menu_set_load_page_event(menu, cont, NULL);
        lv_obj_add_event_cb(cont, nnr_click_cb, LV_EVENT_CLICKED, (void*)&ctxs[i]);
        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
        lv_group_add_obj(g, cont);
    }
    lv_menu_set_page(menu, page);
    NuttyDisplay_unlockLVGL();

    while (result == -2) vTaskDelay(pdMS_TO_TICKS(50));

    NuttyDisplay_lockLVGL();
    lv_group_remove_all_objs(g); lv_group_del(g);
    lv_style_reset(&ts);
    NuttyDisplay_unlockLVGL();
    NuttyDisplay_clearUserAppArea();
    return (int)result;
}

/* ── results screen ──────────────────────────────────────────────────*/
static void show_results(const char *title, const char *diff,
                          int perf, int good, int miss,
                          int max_combo, int score, int total)
{
    char rank = 'D';
    if (total > 0) {
        int pct = (perf + good) * 100 / total;
        if (miss == 0 && good == 0) rank = 'S';
        else if (miss == 0)         rank = 'A';
        else if (pct >= 90)         rank = 'B';
        else if (pct >= 70)         rank = 'C';
    }
    lv_style_t s; lv_style_init(&s);
    lv_style_set_text_font(&s, &cg_pixel_4x5_mono);

    char buf[8][32];
    snprintf(buf[0], 32, "%s",            title);
    snprintf(buf[1], 32, "Diff: %s",      diff);
    snprintf(buf[2], 32, "PERFECT  : %d", perf);
    snprintf(buf[3], 32, "GOOD     : %d", good);
    snprintf(buf[4], 32, "MISS     : %d", miss);
    snprintf(buf[5], 32, "MAX COMBO: %d", max_combo);
    snprintf(buf[6], 32, "SCORE    : %d", score);
    snprintf(buf[7], 32, "RANK     : %c   (any=back)", rank);

    lv_obj_t *da = NuttyDisplay_getUserAppArea();
    NuttyDisplay_lockLVGL();
    for (int i = 0; i < 8; i++) {
        lv_obj_t *l = lv_label_create(da);
        lv_label_set_text(l, buf[i]);
        lv_obj_add_style(l, &s, LV_PART_MAIN);
        lv_obj_set_pos(l, 2, (int16_t)(i*7+1));
    }
    NuttyDisplay_unlockLVGL();
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)) break;
    }
    lv_style_reset(&s);
    NuttyDisplay_clearUserAppArea();
}

/* ── key mapping screen ──────────────────────────────────────────────*/
static void show_key_mapping(void)
{
    lv_style_t s; lv_style_init(&s);
    lv_style_set_text_font(&s, &cg_pixel_4x5_mono);
    lv_obj_t *da = NuttyDisplay_getUserAppArea();
    static const char *lines[] = {
        "  KEY MAPPING",
        " [LEFT]  = L lane",
        " [DOWN]  = D lane",
        " [UP]    = U lane",
        " [RIGHT] = R lane",
        " ----------------",
        " [SEL] hold = quit",
        " any key = back",
    };
    NuttyDisplay_lockLVGL();
    for (uint8_t i = 0; i < 8; i++) {
        lv_obj_t *l = lv_label_create(da);
        lv_label_set_text(l, lines[i]);
        lv_obj_add_style(l, &s, LV_PART_MAIN);
        lv_obj_set_pos(l, 0, (int16_t)(i*7+1));
    }
    NuttyDisplay_unlockLVGL();
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_ALL)) break;
    }
    lv_style_reset(&s);
    NuttyDisplay_clearUserAppArea();
}

/* ── game loop ───────────────────────────────────────────────────────*/
static void game_play(const NNRSong *song, int diff_idx)
{
    /* Obtain chart data — flash or heap-loaded from SD */
    const uint8_t *cd;
    size_t         csz;
    uint8_t       *heap_chart = NULL;
    if (song->flash_data) {
        cd  = song->flash_data;
        csz = song->flash_size;
    } else {
        heap_chart = nnr_load_file(song->chart_path, &csz);
        if (!heap_chart) {
            NuttySystemMonitor_setSystemTrayTempText("Chart load fail", 20);
            return;
        }
        cd = heap_chart;
    }

    NNRSongInfo si;
    NNRDiffInfo di[NNR_MAX_DIFFS];
    if (!nnr_parse_header(cd, csz, &si, di, NNR_MAX_DIFFS)) { free(heap_chart); return; }
    if (diff_idx < 0 || diff_idx >= si.num_diffs)           { free(heap_chart); return; }

    int total = nnr_load_notes(cd, csz, &di[diff_idx], g_notes, NNR_MAX_NOTES);
    if (total == 0) { free(heap_chart); return; }

    int32_t song_end_ms = g_notes[total-1].time_ms + 3000;

    /* ── init pixel buffer + LVGL image ── */
    memset(game_fb, 1, sizeof(game_fb));
    game_img_dsc.header.cf          = LV_IMG_CF_TRUE_COLOR;
    game_img_dsc.header.always_zero = 0;
    game_img_dsc.header.reserved    = 0;
    game_img_dsc.header.w           = FB_W;
    game_img_dsc.header.h           = FB_H;
    game_img_dsc.data_size          = FB_W * FB_H;
    game_img_dsc.data               = game_fb;

    lv_style_t s_lbl; lv_style_init(&s_lbl);
    lv_style_set_text_font(&s_lbl, &cg_pixel_4x5_mono);

    lv_obj_t *da = NuttyDisplay_getUserAppArea();
    NuttyDisplay_lockLVGL();
    game_img_obj = lv_img_create(da);
    lv_img_set_src(game_img_obj, &game_img_dsc);
    lv_obj_align(game_img_obj, LV_ALIGN_TOP_LEFT, 0, 0);
    /* Lane labels rendered on top of the pixel buffer image */
    static const char *lnames[] = {"L", "D", "U", "R"};
    static const int16_t lpos[]  = {12, 44, 76, 108};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *l = lv_label_create(da);
        lv_label_set_text(l, lnames[i]);
        lv_obj_add_style(l, &s_lbl, LV_PART_MAIN);
        lv_obj_set_pos(l, lpos[i], 0);
    }
    NuttyDisplay_unlockLVGL();

    /* ── start audio from SD card if available ── */
    bool    audio_on      = false;
    int64_t t_audio_start = 0;

    if (NuttyStorage_isSDCardMounted() && song->audio_path[0]) {
        nnr_audio_init();
        if (nnr_audio_play(song->audio_path)) {
            /* Record the moment play() was called — this is t=0 for the
               audio pipeline.  The first sample reaches the speaker after
               the ring buffer fills (≈ NNR_AUDIO_LATENCY_MS). */
            t_audio_start = esp_timer_get_time();
            int64_t tw = t_audio_start;
            while (audio_player_get_state() != AUDIO_PLAYER_STATE_PLAYING) {
                if (esp_timer_get_time() - tw > 2000000LL) break;
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            audio_on = (audio_player_get_state() == AUDIO_PLAYER_STATE_PLAYING);
        }
    }
    if (!audio_on) {
        NuttySystemMonitor_setSystemTrayTempText("No audio (SD?)", 15);
    }

    /* game_start_us: wall-clock moment that corresponds to chart time 0.
       With audio: shift forward by ring-buffer latency so the note visual
       reaches the receptor exactly when the speaker plays that beat.
       Without audio: start immediately. */
    int64_t game_start_us = audio_on
        ? (t_audio_start + (int64_t)NNR_AUDIO_LATENCY_MS * 1000LL)
        : esp_timer_get_time();

    /* ── game state ── */
    static const uint16_t lane_btn[4] = {
        NUTTYINPUT_BTN_LEFT, NUTTYINPUT_BTN_DOWN,
        NUTTYINPUT_BTN_UP,   NUTTYINPUT_BTN_RIGHT
    };
    int     score=0, combo=0, max_combo=0;
    int     perf_cnt=0, good_cnt=0, miss_cnt=0;
    uint8_t flash[4] = {0,0,0,0};
    bool    quit = false;
    /* RGB LED flash: 1=miss(red) 2=good(green) 3=perfect(blue); higher wins */
    int     rgb_event  = 0;   /* highest priority event this tick            */
    int     rgb_ticks  = 0;   /* countdown until LEDs turn off               */

    NuttySystemMonitor_setSystemTrayTempText("SEL hold = quit", 15);
    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);

    /* Fixed-timestep scheduler: next_tick_us advances by exactly TICK_US
       each iteration so cumulative drift is zero even under variable load. */
    const int64_t TICK_US    = (int64_t)NNR_TICK_MS * 1000LL;
    int64_t next_tick_us = esp_timer_get_time();

    while (!quit) {
        int32_t cur = (int32_t)((esp_timer_get_time() - game_start_us) / 1000LL);
        if (cur >= song_end_ms) break;

        /* ── input ── */
        uint16_t pressed = NuttyInput_popPressedEvent(
            NUTTYINPUT_BTN_LEFT | NUTTYINPUT_BTN_DOWN |
            NUTTYINPUT_BTN_UP   | NUTTYINPUT_BTN_RIGHT);

        if (pressed) {
            for (int col = 0; col < 4; col++) {
                if (!(pressed & lane_btn[col])) continue;
                int     best = -1;
                int32_t best_ad = NNR_GOOD_MS + 1;
                for (int j = 0; j < total; j++) {
                    if (g_notes[j].state != NOTE_PENDING) continue;
                    if (g_notes[j].col   != (uint8_t)col) continue;
                    int32_t td = g_notes[j].time_ms - cur;
                    if (td >  NNR_GOOD_MS) break;   /* notes sorted; rest too far */
                    if (td < -NNR_GOOD_MS) continue;
                    int32_t ad = td < 0 ? -td : td;
                    if (ad < best_ad) { best_ad = ad; best = j; }
                }
                if (best >= 0) {
                    if (best_ad <= NNR_PERFECT_MS) {
                        g_notes[best].state = NOTE_PERFECT;
                        score += NNR_SCORE_PERFECT; perf_cnt++;
                        if (rgb_event < 3) rgb_event = 3; /* blue */
                    } else {
                        g_notes[best].state = NOTE_GOOD;
                        score += NNR_SCORE_GOOD; good_cnt++;
                        if (rgb_event < 2) rgb_event = 2; /* green */
                    }
                    combo++; if (combo > max_combo) max_combo = combo;
                    flash[col] = NNR_FLASH_TICKS;
                }
            }
        }

        /* ── miss detection ── */
        for (int j = 0; j < total; j++) {
            if (g_notes[j].state == NOTE_PENDING &&
                g_notes[j].time_ms + NNR_GOOD_MS < cur) {
                g_notes[j].state = NOTE_MISSED;
                combo = 0; miss_cnt++;
                if (rgb_event < 1) rgb_event = 1; /* red */
            }
        }

        if (NuttyInput_waitSingleButtonHoldLongNonBlock(NUTTYINPUT_BTN_SELECT))
            quit = true;

        render_frame(total, cur, flash);

        char hud[32];
        if (combo >= 2) snprintf(hud, 32, "x%d  %d pts", combo, score);
        else            snprintf(hud, 32, "%d pts", score);
        NuttySystemMonitor_setSystemTrayTempText(hud, 1);

        for (int i = 0; i < 4; i++) if (flash[i]) flash[i]--;

        /* ── RGB LED flash ──
           perfect=blue  good=green  miss=red
           Higher priority wins within a tick; LEDs auto-off after countdown. */
        if (rgb_event > 0) {
            uint8_t r = 0, g = 0, b = 0;
            if      (rgb_event == 3) b = 255;   /* perfect → blue  */
            else if (rgb_event == 2) g = 255;   /* good    → green */
            else                     r = 255;   /* miss    → red   */
            for (uint8_t i = 0; i < RGB_BULBS; i++)
                nuttyDriverRGB.setRGBWithoutDisplay(i, r, g, b);
            nuttyDriverRGB.displayNow();
            rgb_ticks = NNR_FLASH_TICKS;
            rgb_event = 0;
        } else if (rgb_ticks > 0 && --rgb_ticks == 0) {
            for (uint8_t i = 0; i < RGB_BULBS; i++)
                nuttyDriverRGB.setRGBWithoutDisplay(i, 0, 0, 0);
            nuttyDriverRGB.displayNow();
        }

        /* Fixed-timestep sleep: wait until next_tick_us, then advance the
           deadline.  If we overran by more than one full tick re-sync to now
           to prevent a burst of zero-sleep frames trying to "catch up". */
        next_tick_us += TICK_US;
        int64_t sleep_us = next_tick_us - esp_timer_get_time();
        if (sleep_us > 1000LL) {
            vTaskDelay(pdMS_TO_TICKS(sleep_us / 1000LL));
        } else {
            if (sleep_us < -TICK_US) next_tick_us = esp_timer_get_time();
            taskYIELD();
        }
    }

    /* ── cleanup ── */
    for (uint8_t i = 0; i < RGB_BULBS; i++)
        nuttyDriverRGB.setRGBWithoutDisplay(i, 0, 0, 0);
    nuttyDriverRGB.displayNow();
    if (audio_on) nnr_audio_stop();
    lv_style_reset(&s_lbl);
    NuttyDisplay_lockLVGL();
    if (game_img_obj) { lv_obj_del(game_img_obj); game_img_obj = NULL; }
    NuttyDisplay_unlockLVGL();
    NuttyDisplay_clearUserAppArea();

    if (heap_chart) { free(heap_chart); heap_chart = NULL; }

    if (!quit)
        show_results(song->title, di[diff_idx].name,
                     perf_cnt, good_cnt, miss_cnt,
                     max_combo, score, total);
}

/* ── difficulty select ───────────────────────────────────────────────*/
static int show_diff_select(const NNRSong *song)
{
    const uint8_t *cd; size_t csz; uint8_t *heap = NULL;
    if (song->flash_data) { cd = song->flash_data; csz = song->flash_size; }
    else {
        ESP_LOGI(TAG, "show_diff_select: loading '%s'", song->chart_path);
        heap = nnr_load_file(song->chart_path, &csz);
        if (!heap) {
            ESP_LOGE(TAG, "show_diff_select: file load failed");
            NuttySystemMonitor_setSystemTrayTempText("NNR: file not found", 30);
            return -1;
        }
        cd = heap;
    }

    NNRSongInfo si; NNRDiffInfo di[NNR_MAX_DIFFS];
    if (!nnr_parse_header(cd, csz, &si, di, NNR_MAX_DIFFS)) {
        ESP_LOGE(TAG, "show_diff_select: bad header (sz=%u magic=%02x%02x%02x%02x)",
                 (unsigned)csz, cd[0], cd[1], cd[2], cd[3]);
        NuttySystemMonitor_setSystemTrayTempText("NNR: bad format", 30);
        free(heap);
        return -1;
    }

    static char dlabels[NNR_MAX_DIFFS+1][24];
    const char *ptrs[NNR_MAX_DIFFS+1];
    uint8_t nd = si.num_diffs;
    for (uint8_t i = 0; i < nd; i++) {
        snprintf(dlabels[i], 24, "%-10s [%d]", di[i].name, di[i].meter);
        ptrs[i] = dlabels[i];
    }
    snprintf(dlabels[nd], 24, "< Back");
    ptrs[nd] = dlabels[nd];

    int idx = nnr_menu(NuttyDisplay_getUserAppArea(), ptrs, nd+1);
    free(heap);
    if (idx < 0 || idx == (int)nd) return -1;  /* error or Back */
    return idx;
}

/* ── song select ─────────────────────────────────────────────────────*/
static int show_song_select(void)
{
    /* Refresh list each time to pick up newly inserted SD cards */
    populate_songs();

    static char slabels[NNR_MAX_SONGS + 1][72];
    const char *ptrs  [NNR_MAX_SONGS + 1];

    for (int i = 0; i < g_song_count; i++) {
        if (g_songs[i].artist[0])
            snprintf(slabels[i], 72, "%.32s - %.32s", g_songs[i].title, g_songs[i].artist);
        else
            snprintf(slabels[i], 72, "%.32s", g_songs[i].title);
        ptrs[i] = slabels[i];
    }
    snprintf(slabels[g_song_count], 72, "< Back");
    ptrs[g_song_count] = slabels[g_song_count];

    int idx = nnr_menu(NuttyDisplay_getUserAppArea(), ptrs,
                       (uint8_t)(g_song_count + 1));
    if (idx < 0 || idx == g_song_count) return -1;  /* error or Back */
    return idx;
}

/* ── app entry ───────────────────────────────────────────────────────*/
static const char *main_items[] = { "Play", "Key Mapping", "Exit" };

static void nutty_main(void)
{
    ESP_LOGI(TAG, "Starting NutNutRevolution...");
    bool exit_app = false;

    while (!exit_app) {
        int sel = nnr_menu(NuttyDisplay_getUserAppArea(), main_items, 3);
        if (sel < 0) continue;

        if (sel == 2) {         /* Exit */
            exit_app = true;
        } else if (sel == 1) {  /* Key Mapping */
            show_key_mapping();
        } else if (sel == 0) {  /* Play */
            int si = show_song_select();
            ESP_LOGI(TAG, "song select returned %d (count=%d)", si, g_song_count);
            if (si >= 0 && si < g_song_count) {
                int di = show_diff_select(&g_songs[si]);
                if (di >= 0) game_play(&g_songs[si], di);
            }
        }
    }

    NuttyDisplay_clearUserAppArea();
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NutNutRevolution = {
    .appName      = "NutNut Revolution",
    .appMainEntry = nutty_main,
    .appHidden    = false
};

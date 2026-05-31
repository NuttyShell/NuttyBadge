#include "NuttyAudioPlayer.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_player.h"
#include "drivers/rgb.h"
#include "drivers/pwm_audio.h"

static const char *TAG = "AudioPlayer";

#define AUDIO_PLAYER_MAX_PATH 1024
#define AUDIO_PLAYER_ACTION_COUNT 6

typedef enum {
    AUDIO_PLAYER_ACTION_SELECT_FILE = 0,
    AUDIO_PLAYER_ACTION_PLAY_PAUSE,
    AUDIO_PLAYER_ACTION_TOGGLE_LOOP,
    AUDIO_PLAYER_ACTION_TOGGLE_BG,
    AUDIO_PLAYER_ACTION_TOGGLE_CRAZY,
    AUDIO_PLAYER_ACTION_EXIT,
} audio_player_action_t;

typedef struct {
    bool backend_ready;
    bool has_file;
    bool loop_enabled;
    bool bg_enabled;
    bool crazy_enabled;
    bool crazy_leds_active;
    bool play_request_in_flight;
    bool auto_loop_armed;
    uint8_t crazy_level;
    uint32_t play_request_tick;
    uint32_t crazy_tick;
    uint32_t input_channels;
    uint32_t sample_rate;
    uint32_t fade_frames_remaining;
    int32_t dc_x1;
    int32_t dc_y1;
    char current_path[AUDIO_PLAYER_MAX_PATH];
} nutty_audio_player_state_t;

static nutty_audio_player_state_t g_player = {
    .backend_ready = false,
    .has_file = false,
    .loop_enabled = false,
    .bg_enabled = false,
    .crazy_enabled = false,
    .crazy_leds_active = false,
    .play_request_in_flight = false,
    .auto_loop_armed = false,
    .crazy_level = 0,
    .play_request_tick = 0,
    .crazy_tick = 0,
    .input_channels = 1,
    .sample_rate = 48000,
    .current_path = {0},
};

static const char *g_action_icons[AUDIO_PLAYER_ACTION_COUNT] = {
    [AUDIO_PLAYER_ACTION_SELECT_FILE] = LV_SYMBOL_DIRECTORY,
    [AUDIO_PLAYER_ACTION_PLAY_PAUSE] = LV_SYMBOL_PLAY,
    [AUDIO_PLAYER_ACTION_TOGGLE_LOOP] = LV_SYMBOL_REFRESH,
    [AUDIO_PLAYER_ACTION_TOGGLE_BG] = LV_SYMBOL_AUDIO,
    [AUDIO_PLAYER_ACTION_TOGGLE_CRAZY] = LV_SYMBOL_BELL,
    [AUDIO_PLAYER_ACTION_EXIT] = LV_SYMBOL_CLOSE,
};

typedef struct {
    audio_player_action_t action;
    audio_player_action_t *pending_action;
} audio_player_action_ctx_t;

typedef struct {
    bool *exit_requested;
} audio_player_menu_key_ctx_t;

static void audio_player_menu_action_cb(lv_event_t *event) {
    audio_player_action_ctx_t *ctx = (audio_player_action_ctx_t *)lv_event_get_user_data(event);
    if(ctx != NULL && ctx->pending_action != NULL) {
        *ctx->pending_action = ctx->action;
    }
}

static void audio_player_menu_key_cb(lv_event_t *event) {
    audio_player_menu_key_ctx_t *ctx = (audio_player_menu_key_ctx_t *)lv_event_get_user_data(event);
    if(ctx == NULL || ctx->exit_requested == NULL) {
        return;
    }

    if(lv_event_get_key(event) == LV_KEY_ESC) {
        *ctx->exit_requested = true;
    }
}

static bool audio_player_is_mp3_path(const char *path) {
    const char *ext = strrchr(path, '.');
    if(ext == NULL || ext[1] == '\0') {
        return false;
    }

    const char *target = "mp3";
    size_t i = 0;
    while(target[i] != '\0' && ext[i + 1] != '\0') {
        if((char)tolower((unsigned char)ext[i + 1]) != target[i]) {
            return false;
        }
        i++;
    }

    return (target[i] == '\0' && ext[i + 1] == '\0');
}

static bool audio_player_validate_selected_path(void) {
    if(!g_player.has_file) {
        return false;
    }

    if(!audio_player_is_mp3_path(g_player.current_path)) {
        g_player.has_file = false;
        g_player.auto_loop_armed = false;
        g_player.current_path[0] = '\0';
        NuttySystemMonitor_setSystemTrayTempText("!! MP3 only !!", 20);
        return false;
    }

    return true;
}

static esp_err_t audio_player_output_reconfig(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch) {
    if(bits_cfg != 16) {
        ESP_LOGW(TAG, "Unsupported PCM bit width: %lu", (unsigned long)bits_cfg);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if(ch != I2S_SLOT_MODE_MONO && ch != I2S_SLOT_MODE_STEREO) {
        ESP_LOGW(TAG, "Unsupported channel mode: %d", (int)ch);
        return ESP_ERR_NOT_SUPPORTED;
    }

    g_player.sample_rate = rate;
    g_player.input_channels = (ch == I2S_SLOT_MODE_MONO) ? 1U : 2U;

    esp_err_t err = pwm_audio_stop();
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "pwm_audio_stop failed: %s", esp_err_to_name(err));
    }

    err = pwm_audio_set_param((int)rate, LEDC_TIMER_8_BIT, 1);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "pwm_audio_set_param failed: %s", esp_err_to_name(err));
        return err;
    }

    err = pwm_audio_start();
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "pwm_audio_start failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

static esp_err_t audio_player_write_pcm(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms) {
    if(audio_buffer == NULL || bytes_written == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *bytes_written = 0;

    if(g_player.input_channels != 1U && g_player.input_channels != 2U) {
        return ESP_ERR_INVALID_STATE;
    }

    const size_t input_frame_bytes = sizeof(int16_t) * g_player.input_channels;
    if(input_frame_bytes == 0 || len < input_frame_bytes) {
        return ESP_OK;
    }

    const size_t input_frames = len / input_frame_bytes;
    const int16_t *src = (const int16_t *)audio_buffer;
    size_t frames_processed = 0;

    while(frames_processed < input_frames) {
        size_t chunk_frames = input_frames - frames_processed;
        if(chunk_frames > 256U) {
            chunk_frames = 256U;
        }

        size_t chunk_output_len = chunk_frames;
        if((chunk_output_len & 0x03U) != 0U) {
            chunk_output_len += (4U - (chunk_output_len & 0x03U));
        }

        uint8_t *converted = malloc(chunk_output_len);
        if(converted == NULL) {
            return ESP_ERR_NO_MEM;
        }

        uint32_t level_sum = 0;
        for(size_t i = 0; i < chunk_frames; i++) {
            int32_t mono = 0;

            if(g_player.input_channels == 1U) {
                mono = src[frames_processed + i];
            } else {
                const size_t base = (frames_processed + i) * 2U;
                mono = ((int32_t)src[base] + (int32_t)src[base + 1U]) / 2;
            }

            /* --- Audio clean-up: DC blocker + soft limiter + fade-in --- */

            /* 1) Single-pole DC-blocking HPF (alpha = 255/256, fc ≈ 30 Hz @ 48 kHz) */
            int32_t dc_in = mono;
            mono = dc_in - g_player.dc_x1 + g_player.dc_y1 - (g_player.dc_y1 >> 8);
            g_player.dc_x1 = dc_in;
            g_player.dc_y1 = mono;

            /* 2) Soft limiter — gentle knee at ±28000 to prevent volume-boost clipping */
            if(mono > 28000) {
                mono = 28000 + ((mono - 28000) >> 2);
            } else if(mono < -28000) {
                mono = -28000 + ((mono + 28000) >> 2);
            }

            /* 3) Fade-in on first frames to kill startup pop */
            if(g_player.fade_frames_remaining > 0) {
                uint32_t fade = g_player.fade_frames_remaining;
                mono = (int32_t)(((int64_t)mono * (int64_t)(240U - fade)) / 240U);
                g_player.fade_frames_remaining--;
            }

            /* --- End audio clean-up --- */

            if(g_player.crazy_enabled) {
                int32_t abs_mono = mono < 0 ? -mono : mono;
                level_sum += (uint32_t)abs_mono;
            }

            converted[i] = (uint8_t)(((mono + 32768) >> 8) & 0xFF);
        }

        if(g_player.crazy_enabled && chunk_frames > 0) {
            uint32_t avg = level_sum / chunk_frames;
            uint32_t scaled = avg >> 7;
            if(scaled > 255U) {
                scaled = 255U;
            }

            /* Peak detector: fast attack, slow decay for beat-flash effect */
            if(scaled > g_player.crazy_level) {
                g_player.crazy_level = (uint8_t)((g_player.crazy_level + (uint8_t)scaled) / 2U);
            } else {
                g_player.crazy_level = (uint8_t)(((uint16_t)g_player.crazy_level * 7U + (uint8_t)scaled) / 8U);
            }

            /* Drive LEDs from audio amplitude — brightness follows the beat */
            const uint32_t now = xTaskGetTickCount();
            if((now - g_player.crazy_tick) >= pdMS_TO_TICKS(30)) {
                g_player.crazy_tick = now;
                uint8_t level = g_player.crazy_level;
                if(level < 3) { level = 0; }
                /* Fixed warm-red hue, brightness follows the beat amplitude */
                for(uint8_t led = 0; led < RGB_BULBS; led++) {
                    nuttyDriverRGB.setHSVWithoutDisplay(led, 0, 255, level);
                }
                nuttyDriverRGB.displayNow();
                g_player.crazy_leds_active = true;
            }
        }

        if(chunk_output_len > chunk_frames) {
            memset(converted + chunk_frames, converted[chunk_frames - 1], chunk_output_len - chunk_frames);
        }

        size_t native_written = 0;
        esp_err_t err = pwm_audio_write(converted, chunk_output_len, &native_written, pdMS_TO_TICKS(timeout_ms));
        free(converted);

        if(err != ESP_OK) {
            return err;
        }

        size_t frames_written = native_written;
        if(frames_written > chunk_frames) {
            frames_written = chunk_frames;
        }

        frames_processed += frames_written;
        if(frames_written < chunk_frames) {
            break;
        }
    }

    *bytes_written = frames_processed * input_frame_bytes;
    return ESP_OK;
}

static esp_err_t audio_player_backend_init(void) {
    if(g_player.backend_ready) {
        return ESP_OK;
    }

    esp_err_t err = pwm_audio_set_param(48000, LEDC_TIMER_8_BIT, 1);
    if(err != ESP_OK) {
        ESP_LOGW(TAG, "Initial PWM audio parameter setup failed: %s", esp_err_to_name(err));
    }

    audio_player_config_t config = {
        .mute_fn = NULL,
        .clk_set_fn = audio_player_output_reconfig,
        .write_fn = audio_player_write_pcm,
        .priority = tskIDLE_PRIORITY + 4,
        .coreID = 0,
        .force_stereo = false,
        .write_fn2 = NULL,
        .write_ctx = NULL,
    };

    err = audio_player_new(config);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "audio_player_new failed: %s", esp_err_to_name(err));
        return err;
    }

    g_player.backend_ready = true;

    /* Crank the volume to max (+16 = double output) */
    esp_err_t vol_err = pwm_audio_set_volume(16);
    if(vol_err != ESP_OK) {
        ESP_LOGW(TAG, "pwm_audio_set_volume failed: %s", esp_err_to_name(vol_err));
    }

    return ESP_OK;
}

typedef struct {
    lv_group_t *group;
    lv_obj_t *menu;
    lv_obj_t *mainPage;
    lv_obj_t *action_labels[AUDIO_PLAYER_ACTION_COUNT];
    lv_style_t menu_style;
    audio_player_action_ctx_t action_ctx[AUDIO_PLAYER_ACTION_COUNT];
    audio_player_menu_key_ctx_t key_ctx;
    bool exit_requested;
    audio_player_state_t last_state;
} audio_player_ui_t;

static bool audio_player_ui_is_valid(const audio_player_ui_t *ui) {
    if(ui == NULL || ui->menu == NULL || ui->mainPage == NULL) {
        return false;
    }

    if(!lv_obj_is_valid(ui->menu) || !lv_obj_is_valid(ui->mainPage)) {
        return false;
    }

    for(uint8_t i = 0; i < AUDIO_PLAYER_ACTION_COUNT; i++) {
        if(ui->action_labels[i] == NULL || !lv_obj_is_valid(ui->action_labels[i])) {
            return false;
        }
    }

    return true;
}

static void audio_player_ensure_main_page(const audio_player_ui_t *ui) {
    if(ui == NULL || ui->menu == NULL || ui->mainPage == NULL) {
        return;
    }
    if(!lv_obj_is_valid(ui->menu) || !lv_obj_is_valid(ui->mainPage)) {
        return;
    }
    lv_menu_set_page(ui->menu, ui->mainPage);
}

static const char *audio_player_get_action_icon(audio_player_action_t action, audio_player_state_t state) {
    if(action == AUDIO_PLAYER_ACTION_PLAY_PAUSE) {
        return (state == AUDIO_PLAYER_STATE_PLAYING) ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
    }

    if(action < AUDIO_PLAYER_ACTION_COUNT) {
        return g_action_icons[action];
    }

    return LV_SYMBOL_SETTINGS;
}

static void audio_player_update_action_icons(audio_player_ui_t *ui, bool force) {
    if(ui == NULL) {
        return;
    }

    audio_player_state_t state = audio_player_get_state();
    if(force || ui->last_state != state) {
        lv_obj_t *label = ui->action_labels[AUDIO_PLAYER_ACTION_PLAY_PAUSE];
        if(label != NULL) {
            lv_label_set_text(label, audio_player_get_action_icon(AUDIO_PLAYER_ACTION_PLAY_PAUSE, state));
        }
        ui->last_state = state;
    }
}

static void audio_player_set_crazy_enabled(bool enabled) {
    g_player.crazy_enabled = enabled;
    if(!enabled) {
        g_player.crazy_leds_active = false;
        for(uint8_t i = 0; i < RGB_BULBS; i++) {
            nuttyDriverRGB.setRGBWithoutDisplay(i, 0, 0, 0);
        }
        nuttyDriverRGB.displayNow();
    }
}

static void audio_player_update_crazy_leds(void) {
    if(!g_player.crazy_enabled) {
        return;
    }

    audio_player_state_t state = audio_player_get_state();
    if(state != AUDIO_PLAYER_STATE_PLAYING && !g_player.bg_enabled) {
        /* Turn off LEDs only if not in BG mode — BG keeps music alive so LEDs run from PCM callback */
        if(g_player.crazy_leds_active) {
            for(uint8_t i = 0; i < RGB_BULBS; i++) {
                nuttyDriverRGB.setRGBWithoutDisplay(i, 0, 0, 0);
            }
            nuttyDriverRGB.displayNow();
            g_player.crazy_leds_active = false;
        }
    }
}

static void audio_player_destroy_ui(audio_player_ui_t *ui) {
    if(ui == NULL) {
        return;
    }

    NuttyDisplay_lockLVGL();
    if(ui->group != NULL) {
        lv_group_remove_all_objs(ui->group);
        lv_group_del(ui->group);
        lv_group_set_default(NULL);
        ui->group = NULL;
    }
    NuttyDisplay_unlockLVGL();

    NuttyDisplay_clearUserAppArea();
    memset(ui, 0, sizeof(*ui));
}

static void audio_player_init_ui(audio_player_ui_t *ui, audio_player_action_t *pending_action) {
    if(ui == NULL || pending_action == NULL) {
        return;
    }

    NuttyInputLVGLInputMapping mapping = {
        .UP=LV_KEY_PREV,
        .DOWN=LV_KEY_NEXT,
        .A=LV_KEY_ENTER,
        .B=LV_KEY_ESC,
        .START=LV_KEY_ESC,
    };
    lv_indev_t *indev = NuttyInput_UpdateLVGLInDev(mapping);
    assert(indev != NULL);

    memset(ui, 0, sizeof(*ui));
    ui->group = lv_group_create();
    assert(ui->group != NULL);
    lv_group_set_default(ui->group);
    lv_indev_set_group(indev, ui->group);

    lv_style_init(&ui->menu_style);
    lv_style_set_text_font(&ui->menu_style, &lv_font_montserrat_16);

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();

    NuttyDisplay_lockLVGL();
    ui->menu = lv_menu_create(drawArea);
    lv_obj_set_size(ui->menu, lv_obj_get_width(drawArea), lv_obj_get_height(drawArea));
    lv_obj_align(ui->menu, LV_ALIGN_CENTER, 0, 0);

    ui->exit_requested = false;
    ui->key_ctx.exit_requested = &ui->exit_requested;
    lv_obj_add_event_cb(ui->menu, audio_player_menu_key_cb, LV_EVENT_KEY, &ui->key_ctx);

    lv_obj_t *mainPage = lv_menu_page_create(ui->menu, NULL);
    ui->mainPage = mainPage;
    audio_player_state_t state = audio_player_get_state();
    for(uint8_t i = 0; i < AUDIO_PLAYER_ACTION_COUNT; i++) {
        lv_obj_t *cont = lv_menu_cont_create(mainPage);
        lv_obj_t *label = lv_label_create(cont);
        lv_obj_add_style(label, &ui->menu_style, LV_PART_MAIN);
        lv_label_set_text(label, audio_player_get_action_icon((audio_player_action_t)i, state));
        lv_obj_set_width(label, lv_pct(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        ui->action_labels[i] = label;
        lv_menu_set_load_page_event(ui->menu, cont, NULL);
        ui->action_ctx[i].action = (audio_player_action_t)i;
        ui->action_ctx[i].pending_action = pending_action;
        lv_obj_add_event_cb(cont, audio_player_menu_action_cb, LV_EVENT_CLICKED, &ui->action_ctx[i]);
        lv_obj_set_style_outline_width(cont, 1, LV_STATE_FOCUS_KEY);
        lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_group_add_obj(ui->group, cont);
    }

    lv_menu_set_page(ui->menu, mainPage);
    ui->last_state = state;
    audio_player_update_action_icons(ui, true);
    NuttyDisplay_unlockLVGL();
}

static bool audio_player_pick_file(char *selected_path, size_t selected_path_len) {
    if(!NuttyStorage_isSDCardMounted()) {
        NuttySystemMonitor_setSystemTrayTempText("!! SD Card Missing !!", 20);
        return false;
    }

    if(selected_path == NULL || selected_path_len == 0) {
        return false;
    }

    NuttySystemMonitor_setSystemTrayTempText("!Please select an MP3 file!", 30);
    snprintf(selected_path, selected_path_len, "%s/", SDCARD_MOUNT_POINT);

    void *args[2];
    args[0] = xTaskGetCurrentTaskHandle();
    args[1] = selected_path;
    NuttyApps_launchParamedAppByName("File Manager", 2, args);

    xTaskNotifyWait(0x00, ULONG_MAX, NULL, portMAX_DELAY);

    if(strcmp(selected_path, SDCARD_MOUNT_POINT"/") == 0) {
        return false;
    }

    if(!audio_player_is_mp3_path(selected_path)) {
        NuttySystemMonitor_setSystemTrayTempText("!! MP3 only !!", 20);
        return false;
    }

    return true;
}

static esp_err_t audio_player_start_selected_file(void) {
    if(!g_player.has_file) {
        NuttySystemMonitor_setSystemTrayTempText("!! No file selected !!", 20);
        return ESP_ERR_INVALID_STATE;
    }

    if(!audio_player_validate_selected_path()) {
        return ESP_ERR_INVALID_STATE;
    }

    FILE *fp = fopen(g_player.current_path, "rb");
    if(fp == NULL) {
        ESP_LOGE(TAG, "Failed to open %s", g_player.current_path);
        NuttySystemMonitor_setSystemTrayTempText("!! Open failed !!", 20);
        return ESP_FAIL;
    }

    esp_err_t err = audio_player_play(fp);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "audio_player_play failed: %s", esp_err_to_name(err));
        fclose(fp);
        NuttySystemMonitor_setSystemTrayTempText("!! Play failed !!", 20);
        return err;
    }

    g_player.play_request_in_flight = true;
    g_player.play_request_tick = xTaskGetTickCount();
    g_player.auto_loop_armed = true;
    g_player.dc_x1 = 0;
    g_player.dc_y1 = 0;
    g_player.fade_frames_remaining = 240; /* ~5ms fade-in at 48kHz */

    NuttySystemMonitor_setSystemTrayTempText("!Playing...", 12);
    return ESP_OK;
}

static void audio_player_stop_current(bool disarm_loop) {
    esp_err_t err = audio_player_stop();
    if(err != ESP_OK) {
        ESP_LOGW(TAG, "audio_player_stop failed: %s", esp_err_to_name(err));
    }

    g_player.play_request_in_flight = false;
    if(disarm_loop) {
        g_player.auto_loop_armed = false;
    }
}

static void audio_player_handle_auto_loop(void) {
    audio_player_state_t state = audio_player_get_state();

    if(state == AUDIO_PLAYER_STATE_PLAYING) {
        g_player.play_request_in_flight = false;
        return;
    }

    if(g_player.play_request_in_flight) {
        const uint32_t elapsed_ms = (uint32_t)((xTaskGetTickCount() - g_player.play_request_tick) * portTICK_PERIOD_MS);
        if(elapsed_ms > 2000U) {
            ESP_LOGW(TAG, "Playback did not start in time; disarming loop");
            g_player.play_request_in_flight = false;
            g_player.auto_loop_armed = false;
            NuttySystemMonitor_setSystemTrayTempText("!! Playback error !!", 20);
        }
        return;
    }

    if(state == AUDIO_PLAYER_STATE_IDLE) {
        if(g_player.loop_enabled && g_player.auto_loop_armed && g_player.has_file) {
            if(audio_player_start_selected_file() != ESP_OK) {
                g_player.auto_loop_armed = false;
            }
        } else if(!g_player.loop_enabled) {
            g_player.auto_loop_armed = false;
        }
    }
}

static void nutty_main(void) {
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Starting NuttyAudioPlayer...");

    if(audio_player_backend_init() != ESP_OK) {
        NuttySystemMonitor_setSystemTrayTempText("!! Audio backend init failed !!", 30);
    }

    audio_player_ui_t ui = {0};
    audio_player_action_t pending_action = AUDIO_PLAYER_ACTION_COUNT;
    audio_player_init_ui(&ui, &pending_action);
    bool exit_app = false;
    char selected_path[AUDIO_PLAYER_MAX_PATH] = {0};

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);

    while(!exit_app) {
        if(ui.exit_requested) {
            if(!g_player.bg_enabled) {
                audio_player_stop_current(true);
            }
            exit_app = true;
            ui.exit_requested = false;
        }

        if(!audio_player_ui_is_valid(&ui)) {
            audio_player_destroy_ui(&ui);
            audio_player_init_ui(&ui, &pending_action);
        }

        if(pending_action != AUDIO_PLAYER_ACTION_COUNT) {
            switch(pending_action) {
                case AUDIO_PLAYER_ACTION_SELECT_FILE: {
                    memset(selected_path, 0x00, sizeof(selected_path));
                    audio_player_destroy_ui(&ui);
                    if(audio_player_pick_file(selected_path, sizeof(selected_path))) {
                        snprintf(g_player.current_path, sizeof(g_player.current_path), "%s", selected_path);
                        g_player.has_file = true;
                        audio_player_init_ui(&ui, &pending_action);
                        if(audio_player_start_selected_file() == ESP_OK) {
                            NuttySystemMonitor_setSystemTrayTempText("!File selected!", 12);
                        }
                    } else {
                        audio_player_init_ui(&ui, &pending_action);
                    }
                    break;
                }
                case AUDIO_PLAYER_ACTION_PLAY_PAUSE: {
                    audio_player_state_t state = audio_player_get_state();
                    if(state == AUDIO_PLAYER_STATE_PLAYING) {
                        esp_err_t err = audio_player_pause();
                        if(err == ESP_OK) {
                            NuttySystemMonitor_setSystemTrayTempText("!Paused!", 12);
                        } else {
                            ESP_LOGW(TAG, "audio_player_pause failed: %s", esp_err_to_name(err));
                        }
                    } else if(state == AUDIO_PLAYER_STATE_PAUSE) {
                        esp_err_t err = audio_player_resume();
                        if(err == ESP_OK) {
                            NuttySystemMonitor_setSystemTrayTempText("!Resuming!", 12);
                        } else {
                            ESP_LOGW(TAG, "audio_player_resume failed: %s", esp_err_to_name(err));
                        }
                    } else {
                        if(audio_player_start_selected_file() != ESP_OK) {
                            g_player.auto_loop_armed = false;
                        }
                    }
                    break;
                }
                case AUDIO_PLAYER_ACTION_TOGGLE_LOOP: {
                    g_player.loop_enabled = !g_player.loop_enabled;
                    if(g_player.loop_enabled) {
                        g_player.auto_loop_armed = true;
                        NuttySystemMonitor_setSystemTrayTempText("!Loop ON!", 12);
                    } else {
                        NuttySystemMonitor_setSystemTrayTempText("!Loop OFF!", 12);
                    }
                    break;
                }
                case AUDIO_PLAYER_ACTION_TOGGLE_BG: {
                    g_player.bg_enabled = !g_player.bg_enabled;
                    if(g_player.bg_enabled) {
                        NuttySystemMonitor_setSystemTrayTempText("!BG ON!", 12);
                    } else {
                        NuttySystemMonitor_setSystemTrayTempText("!BG OFF!", 12);
                    }
                    break;
                }
                case AUDIO_PLAYER_ACTION_TOGGLE_CRAZY: {
                    audio_player_set_crazy_enabled(!g_player.crazy_enabled);
                    if(g_player.crazy_enabled) {
                        NuttySystemMonitor_setSystemTrayTempText("!Crazy ON!", 12);
                    } else {
                        NuttySystemMonitor_setSystemTrayTempText("!Crazy OFF!", 12);
                    }
                    break;
                }
                case AUDIO_PLAYER_ACTION_EXIT: {
                    if(!g_player.bg_enabled) {
                        audio_player_stop_current(true);
                        audio_player_set_crazy_enabled(false);
                    }
                    exit_app = true;
                    break;
                }
            }
            pending_action = AUDIO_PLAYER_ACTION_COUNT;
            NuttyDisplay_lockLVGL();
            audio_player_ensure_main_page(&ui);
            NuttyDisplay_unlockLVGL();
        }

        audio_player_handle_auto_loop();
        audio_player_update_crazy_leds();

        NuttyDisplay_lockLVGL();
        audio_player_ensure_main_page(&ui);
        audio_player_update_action_icons(&ui, false);
        NuttyDisplay_unlockLVGL();

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    audio_player_destroy_ui(&ui);
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyAudioPlayer = {
    .appName = "Audio Player",
    .appMainEntry = nutty_main,
    .appHidden = false
};

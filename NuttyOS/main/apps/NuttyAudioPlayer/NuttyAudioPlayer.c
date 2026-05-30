#include "NuttyAudioPlayer.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_player.h"
#include "drivers/pwm_audio.h"

static const char *TAG = "AudioPlayer";

#define AUDIO_PLAYER_MAX_PATH 1024
#define AUDIO_PLAYER_ACTION_COUNT 7

typedef enum {
    AUDIO_PLAYER_ACTION_SELECT_FILE = 0,
    AUDIO_PLAYER_ACTION_PLAY_RESUME,
    AUDIO_PLAYER_ACTION_PAUSE,
    AUDIO_PLAYER_ACTION_STOP,
    AUDIO_PLAYER_ACTION_TOGGLE_LOOP,
    AUDIO_PLAYER_ACTION_TOGGLE_BG,
    AUDIO_PLAYER_ACTION_EXIT,
} audio_player_action_t;

typedef struct {
    bool backend_ready;
    bool has_file;
    bool loop_enabled;
    bool bg_enabled;
    bool play_request_in_flight;
    bool auto_loop_armed;
    uint32_t play_request_tick;
    uint32_t input_channels;
    uint32_t sample_rate;
    char current_path[AUDIO_PLAYER_MAX_PATH];
} nutty_audio_player_state_t;

static nutty_audio_player_state_t g_player = {
    .backend_ready = false,
    .has_file = false,
    .loop_enabled = false,
    .bg_enabled = false,
    .play_request_in_flight = false,
    .auto_loop_armed = false,
    .play_request_tick = 0,
    .input_channels = 1,
    .sample_rate = 48000,
    .current_path = {0},
};

static const char *g_actions[AUDIO_PLAYER_ACTION_COUNT] = {
    "Select File",
    "Play / Resume",
    "Pause",
    "Stop",
    "Loop",
    "BG",
    "Exit",
};

static const char *audio_player_state_to_text(audio_player_state_t state) {
    switch(state) {
        case AUDIO_PLAYER_STATE_IDLE:
            return "IDLE";
        case AUDIO_PLAYER_STATE_PLAYING:
            return "PLAYING";
        case AUDIO_PLAYER_STATE_PAUSE:
            return "PAUSE";
        case AUDIO_PLAYER_STATE_SHUTDOWN:
            return "SHUTDOWN";
        default:
            return "UNKNOWN";
    }
}

static const char *audio_player_basename(const char *path) {
    const char *base = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');

    if(backslash != NULL && (base == NULL || backslash > base)) {
        base = backslash;
    }

    return (base == NULL) ? path : base + 1;
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

        for(size_t i = 0; i < chunk_frames; i++) {
            int32_t mono = 0;

            if(g_player.input_channels == 1U) {
                mono = src[frames_processed + i];
            } else {
                const size_t base = (frames_processed + i) * 2U;
                mono = ((int32_t)src[base] + (int32_t)src[base + 1U]) / 2;
            }

            converted[i] = (uint8_t)(((mono + 32768) >> 8) & 0xFF);
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
    return ESP_OK;
}

static void audio_player_apply_action_label(lv_obj_t *label, uint8_t selected_action) {
    lv_label_set_text_fmt(label, "Action %u/%u: %s", (unsigned)(selected_action + 1U), (unsigned)AUDIO_PLAYER_ACTION_COUNT, g_actions[selected_action]);
}

static void audio_player_refresh_ui(lv_obj_t *fileLabel, lv_obj_t *stateLabel, lv_obj_t *actionLabel, lv_obj_t *modeLabel, uint8_t selected_action) {
    audio_player_state_t state = audio_player_get_state();

    lv_label_set_text_fmt(
        fileLabel,
        "File: %s",
        g_player.has_file ? audio_player_basename(g_player.current_path) : "<none>"
    );

    lv_label_set_text_fmt(stateLabel, "State: %s%s", audio_player_state_to_text(state), g_player.play_request_in_flight ? " *" : "");
    audio_player_apply_action_label(actionLabel, selected_action);
    lv_label_set_text_fmt(modeLabel, "Loop:%s  BG:%s  A=OK", g_player.loop_enabled ? "ON" : "OFF", g_player.bg_enabled ? "ON" : "OFF");
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

    lv_style_t title_style;
    lv_style_t text_style;
    lv_style_init(&title_style);
    lv_style_init(&text_style);
    lv_style_set_text_font(&title_style, &lv_font_montserrat_10);
    lv_style_set_text_font(&text_style, &lv_font_montserrat_8);

    lv_obj_t *drawArea = NuttyDisplay_getUserAppArea();

    NuttyDisplay_lockLVGL();
    lv_obj_t *titleLabel = lv_label_create(drawArea);
    lv_obj_add_style(titleLabel, &title_style, LV_PART_MAIN);
    lv_label_set_text(titleLabel, "Audio Player");
    lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *fileLabel = lv_label_create(drawArea);
    lv_obj_add_style(fileLabel, &text_style, LV_PART_MAIN);
    lv_label_set_long_mode(fileLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(fileLabel, lv_obj_get_width(drawArea));
    lv_obj_align(fileLabel, LV_ALIGN_TOP_LEFT, 0, 10);

    lv_obj_t *stateLabel = lv_label_create(drawArea);
    lv_obj_add_style(stateLabel, &text_style, LV_PART_MAIN);
    lv_obj_set_width(stateLabel, lv_obj_get_width(drawArea));
    lv_obj_align(stateLabel, LV_ALIGN_TOP_LEFT, 0, 20);

    lv_obj_t *actionLabel = lv_label_create(drawArea);
    lv_obj_add_style(actionLabel, &text_style, LV_PART_MAIN);
    lv_obj_set_width(actionLabel, lv_obj_get_width(drawArea));
    lv_obj_align(actionLabel, LV_ALIGN_TOP_LEFT, 0, 30);

    lv_obj_t *modeLabel = lv_label_create(drawArea);
    lv_obj_add_style(modeLabel, &text_style, LV_PART_MAIN);
    lv_label_set_long_mode(modeLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(modeLabel, lv_obj_get_width(drawArea));
    lv_obj_align(modeLabel, LV_ALIGN_TOP_LEFT, 0, 40);

    lv_obj_t *helpLabel = lv_label_create(drawArea);
    lv_obj_add_style(helpLabel, &text_style, LV_PART_MAIN);
    lv_obj_set_width(helpLabel, lv_obj_get_width(drawArea));
    lv_label_set_text(helpLabel, "UP/DN  A=OK  B/START=Exit");
    lv_obj_align(helpLabel, LV_ALIGN_TOP_LEFT, 0, 50);
    NuttyDisplay_unlockLVGL();

    uint8_t selected_action = AUDIO_PLAYER_ACTION_SELECT_FILE;
    bool exit_app = false;
    char selected_path[AUDIO_PLAYER_MAX_PATH] = {0};

    NuttyInput_clearButtonHoldState(NUTTYINPUT_BTN_ALL);

    while(!exit_app) {
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
            if(selected_action == 0U) {
                selected_action = AUDIO_PLAYER_ACTION_COUNT - 1U;
            } else {
                selected_action--;
            }
        }

        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
            selected_action = (uint8_t)((selected_action + 1U) % AUDIO_PLAYER_ACTION_COUNT);
        }

        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_A)) {
            switch(selected_action) {
                case AUDIO_PLAYER_ACTION_SELECT_FILE: {
                    memset(selected_path, 0x00, sizeof(selected_path));
                    if(audio_player_pick_file(selected_path, sizeof(selected_path))) {
                        snprintf(g_player.current_path, sizeof(g_player.current_path), "%s", selected_path);
                        g_player.has_file = true;
                        if(audio_player_start_selected_file() == ESP_OK) {
                            NuttySystemMonitor_setSystemTrayTempText("!File selected!", 12);
                        }
                    }
                    break;
                }
                case AUDIO_PLAYER_ACTION_PLAY_RESUME: {
                    audio_player_state_t state = audio_player_get_state();
                    if(state == AUDIO_PLAYER_STATE_PAUSE) {
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
                case AUDIO_PLAYER_ACTION_PAUSE: {
                    esp_err_t err = audio_player_pause();
                    if(err == ESP_OK) {
                        NuttySystemMonitor_setSystemTrayTempText("!Paused!", 12);
                    } else {
                        ESP_LOGW(TAG, "audio_player_pause failed: %s", esp_err_to_name(err));
                    }
                    break;
                }
                case AUDIO_PLAYER_ACTION_STOP: {
                    audio_player_stop_current(true);
                    NuttySystemMonitor_setSystemTrayTempText("!Stopped!", 12);
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
                case AUDIO_PLAYER_ACTION_EXIT: {
                    if(!g_player.bg_enabled) {
                        audio_player_stop_current(true);
                    }
                    exit_app = true;
                    break;
                }
            }
        }

        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_B) ||
           NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_START)) {
            if(!g_player.bg_enabled) {
                audio_player_stop_current(true);
            }
            exit_app = true;
        }

        audio_player_handle_auto_loop();

        NuttyDisplay_lockLVGL();
        audio_player_refresh_ui(fileLabel, stateLabel, actionLabel, modeLabel, selected_action);
        NuttyDisplay_unlockLVGL();

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    NuttyDisplay_clearUserAppArea();
    NuttyApps_launchAppByIndex(0);
}

NuttyAppDefinition NuttyAudioPlayer = {
    .appName = "Audio Player",
    .appMainEntry = nutty_main,
    .appHidden = false
};

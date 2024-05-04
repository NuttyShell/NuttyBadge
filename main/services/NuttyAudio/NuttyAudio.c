#include "NuttyAudio.h"

static const char* TAG = "NuttyAudio";
static bool NuttyAudioInitialized=false;
static TaskHandle_t NuttyAudio_WorkerHandle = NULL;

static uint8_t *playBuf = NULL;
static size_t playBufSz = 0;
static size_t playBufLoc = 0;
static uint16_t playHz = 0;
static uint16_t playHzDuration=0;
static uint16_t playHzTotalDuration=0;
static uint8_t toneVolume=0;

static void NuttyAudio_Worker(void *pvParameters) {
    size_t bytesWritten=0;
    while(true) {
        if(playHz == 0 && playBuf != NULL && playBufLoc != playBufSz) {
            // Play Buffer
            pwm_audio_reset_freq();
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
            while(playBufLoc != playBufSz) {
                pwm_audio_write(playBuf + playBufLoc, playBufSz - playBufLoc, &bytesWritten, 100 / portTICK_PERIOD_MS);
                playBufLoc += bytesWritten;
                //ESP_LOGI(TAG, "Audio: Written = %u; Left=%u", bytesWritten, (playBufSz - playBufLoc));
            }
        }else if(playHz > 0) {
            // Play Tone
            ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_2, playHz);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 512-((32 << toneVolume)));
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
            while(playHzDuration != playHzTotalDuration) {
                playHzDuration++;
                vTaskDelay(100/portTICK_PERIOD_MS);
            }
            pwm_audio_reset_freq();
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
        }
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

esp_err_t NuttyAudio_Init() {
    esp_err_t err = ESP_OK;
    ESP_LOGI(TAG, "Init...");
    pwm_audio_config_t pac;
    pac.duty_resolution    = LEDC_TIMER_8_BIT;
    pac.gpio_num_left      = GPIO_AUDIO_OUT;
    pac.gpio_num_right      = -1;
    pac.ledc_channel_left  = LEDC_CHANNEL_1;
    pac.ledc_timer_sel     = LEDC_TIMER_2;
    pac.ringbuf_len        = 1024 * 8;

    err = pwm_audio_init(&pac);             /**< Initialize pwm audio */
    ESP_ERROR_CHECK(err);
    //err = pwm_audio_set_param(32000, 8, 1);  /**< Set sample rate, bits and channel numner */
    err = pwm_audio_set_param(48000, 8, 1);  /**< Set sample rate, bits and channel numner */
    ESP_ERROR_CHECK(err);
    err = pwm_audio_start();                 /**< Start to run */
    ESP_ERROR_CHECK(err);

    NuttyAudioInitialized=true;

    ESP_LOGI(TAG, "Init done.");
    
    return err;
}

esp_err_t NuttyAudio_Deinit() {
    esp_err_t err = ESP_OK;
    ESP_LOGI(TAG, "Deinit...");
    if(!NuttyAudioInitialized) return err;

    if(NuttyAudio_WorkerHandle != NULL) vTaskDelete(NuttyAudio_WorkerHandle);

    err = pwm_audio_stop();
    ESP_ERROR_CHECK(err);
    err = pwm_audio_deinit();
    ESP_ERROR_CHECK(err);

    NuttyAudioInitialized=false;

    ESP_LOGI(TAG, "Deinit done.");

    return err;
}

esp_err_t NuttyAudio_PlayBuffer(uint8_t *buf, size_t sz) {
    if(!NuttyAudioInitialized) return ESP_ERR_INVALID_STATE;

    if(NuttyAudio_WorkerHandle != NULL) vTaskDelete(NuttyAudio_WorkerHandle);

    playBuf = buf;
    playBufLoc=0;
    playBufSz=sz;
    playHz=0;
    playHzDuration=0;
    playHzTotalDuration=0;

    xTaskCreate(&NuttyAudio_Worker, "audio_worker", 4096, NULL, 4, &NuttyAudio_WorkerHandle);

    return ESP_OK;
}

esp_err_t NuttyAudio_PlayTone(uint16_t hz, uint16_t duration_100ms) {
    if(!NuttyAudioInitialized) return ESP_ERR_INVALID_STATE;
    if(NuttyAudio_WorkerHandle != NULL) vTaskDelete(NuttyAudio_WorkerHandle);

    playBuf=NULL;
    playBufLoc=0;
    playBufSz=0;
    playHz=hz;
    playHzDuration=0;
    playHzTotalDuration=duration_100ms;

    xTaskCreate(&NuttyAudio_Worker, "audio_worker", 4096, NULL, 4, &NuttyAudio_WorkerHandle);

    return ESP_OK;
}

esp_err_t NuttyAudio_FinshedPlayedBuffer(bool *status) {
    if(!NuttyAudioInitialized) return ESP_ERR_INVALID_STATE;
    *status = (playHz > 0 || (playBufLoc == playBufSz));
    return ESP_OK;
}

esp_err_t NuttyAudio_FinshedPlayedTone(bool *status) {
    if(!NuttyAudioInitialized) return ESP_ERR_INVALID_STATE;
    *status = (playHz == 0 || (playHzDuration == playHzTotalDuration));
    return ESP_OK;
}

esp_err_t NuttyAudio_FinishedPlayed(bool *status) {
    bool _statusBuf = true;
    bool _statusTone = true;
    esp_err_t err = NuttyAudio_FinshedPlayedBuffer(&_statusBuf);
    ESP_ERROR_CHECK(err);
    err = NuttyAudio_FinshedPlayedTone(&_statusTone);
    ESP_ERROR_CHECK(err);
    *status = (_statusBuf && _statusTone);
    return ESP_OK;
}

esp_err_t NuttyAudio_SetVolume(int8_t volume) {
    if(!NuttyAudioInitialized) return ESP_ERR_INVALID_STATE;
    toneVolume=volume;
    return pwm_audio_set_volume(volume);
}
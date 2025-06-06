#ifndef _NUTTYAUDIO_H_
#define _NUTTYAUDIO_H_


#include "config.h"
#include "drivers/pwm_audio.h"
#include <esp_log.h>

esp_err_t NuttyAudio_Init();
esp_err_t NuttyAudio_Deinit();
esp_err_t NuttyAudio_PlayBuffer(uint8_t *buf, size_t sz);
esp_err_t NuttyAudio_PlayTone(uint16_t hz, uint16_t duration_100ms);
esp_err_t NuttyAudio_FinshedPlayedBuffer(bool *status);
esp_err_t NuttyAudio_FinshedPlayedTone(bool *status);
esp_err_t NuttyAudio_FinishedPlayed(bool *status);
esp_err_t NuttyAudio_SetVolume(int8_t volume);
esp_err_t NuttyAudio_StopPlaying();

#endif /* _NUTTYAUDIO_H_ */
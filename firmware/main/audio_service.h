#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef void (*wake_word_cb_t)(void);
typedef void (*vad_cb_t)(void);
typedef void (*audio_data_cb_t)(const uint8_t *data, size_t len);

typedef enum {
    AUDIO_TONE_PROV_START,
    AUDIO_TONE_PROV_SUCCESS,
    AUDIO_TONE_PROV_FAILED,
    AUDIO_TONE_PROV_CONNECTING,
} audio_tone_t;

void audio_service_init(void);
void audio_service_start(void);
void audio_service_stop(void);
bool audio_service_is_running(void);

void audio_service_set_wake_word_callback(wake_word_cb_t cb);
void audio_service_set_vad_callback(vad_cb_t cb);

void audio_service_play(const uint8_t *data, size_t len);
void audio_service_play_stop(void);
void audio_service_play_tone(audio_tone_t tone);
void audio_service_beep(int freq, int duration_ms);

void audio_service_notify_wifi_connecting(void);
void audio_service_notify_wifi_connected(void);
void audio_service_notify_wifi_failed(void);

#endif
#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*wake_word_cb_t)(void);
typedef void (*vad_cb_t)(void);
typedef void (*audio_data_cb_t)(const uint8_t *data, size_t len);

void audio_service_init(void);
void audio_service_start(void);
void audio_service_stop(void);
bool audio_service_is_running(void);

void audio_service_set_wake_word_callback(wake_word_cb_t cb);
void audio_service_set_vad_callback(vad_cb_t cb);
void audio_service_set_audio_callback(audio_data_cb_t cb);

void audio_service_play(const uint8_t *data, size_t len);
void audio_service_play_stop(void);

#endif

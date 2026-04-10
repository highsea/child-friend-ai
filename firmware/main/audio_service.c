#include "audio_service.h"
#include "esp_log.h"
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "audio_service";

static bool is_running = false;
static TaskHandle_t beep_task_handle = NULL;
static bool provision_wifi_done = false;

static wake_word_cb_t on_wake_word = NULL;
static vad_cb_t on_vad_speech_end = NULL;

void audio_service_init(void)
{
    ESP_LOGI(TAG, "Audio service initialized");
}

void audio_service_start(void)
{
    if (is_running) {
        return;
    }
    is_running = true;
    ESP_LOGI(TAG, "Audio service started");
}

void audio_service_stop(void)
{
    if (!is_running) {
        return;
    }
    is_running = false;

    if (beep_task_handle) {
        vTaskDelete(beep_task_handle);
        beep_task_handle = NULL;
    }
    ESP_LOGI(TAG, "Audio service stopped");
}

bool audio_service_is_running(void)
{
    return is_running;
}

void audio_service_set_wake_word_callback(wake_word_cb_t cb)
{
    on_wake_word = cb;
}

void audio_service_set_vad_callback(vad_cb_t cb)
{
    on_vad_speech_end = cb;
}

void audio_service_play(const uint8_t *data, size_t len)
{
    if (!data || len == 0) {
        ESP_LOGW(TAG, "Play called with null data or zero length");
        return;
    }
    ESP_LOGI(TAG, "Playing audio: %d bytes", len);

    size_t bytes_written = 0;
    esp_err_t err = i2s_write(I2S_NUM_0, data, len, &bytes_written, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S write error: %d", err);
    }
}

void audio_service_play_stop(void)
{
    ESP_LOGI(TAG, "Stop playing");
}

void audio_service_beep(int freq, int duration_ms)
{
    const int SAMPLE_RATE = 16000;
    int num_samples = SAMPLE_RATE * duration_ms / 1000;

    if (num_samples == 0) {
        num_samples = SAMPLE_RATE / 10;
    }

    uint8_t *samples = malloc(num_samples * 4);
    if (!samples) {
        ESP_LOGE(TAG, "Failed to allocate beep buffer");
        return;
    }

    ESP_LOGI(TAG, "Beep: %d Hz, %d ms", freq, duration_ms);

    for (int i = 0; i < num_samples; i++) {
        float t = (float)i / SAMPLE_RATE;
        int16_t sample = (int16_t)(20000 * sin(2 * M_PI * freq * t));
        samples[i * 4 + 0] = 0;
        samples[i * 4 + 1] = 0;
        samples[i * 4 + 2] = sample & 0xFF;
        samples[i * 4 + 3] = (sample >> 8) & 0xFF;
    }

    size_t bytes_written = 0;
    esp_err_t err = i2s_write(I2S_NUM_0, samples, num_samples * 4, &bytes_written, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S write error: %d", err);
    }

    free(samples);
}

void audio_service_play_tone(audio_tone_t tone)
{
    switch (tone) {
        case AUDIO_TONE_PROV_START:
            audio_service_beep(880, 100);
            break;
        case AUDIO_TONE_PROV_SUCCESS:
            audio_service_beep(523, 200);
            break;
        case AUDIO_TONE_PROV_FAILED:
            audio_service_beep(330, 300);
            break;
        case AUDIO_TONE_PROV_CONNECTING:
            audio_service_beep(660, 150);
            break;
        default:
            audio_service_beep(440, 200);
            break;
    }
}

typedef struct {
    int interval_ms;
    int beep_count;
    int freq;
    int duration_ms;
} beep_pattern_t;

void audio_service_beep_task(void *arg)
{
    beep_pattern_t *pattern = (beep_pattern_t *)arg;

    ESP_LOGI(TAG, "Beep task: freq=%d, interval=%dms, count=%d",
             pattern->freq, pattern->interval_ms, pattern->beep_count);

    for (int i = 0; i < pattern->beep_count && is_running; i++) {
        audio_service_beep(pattern->freq, pattern->duration_ms);
        if (i < pattern->beep_count - 1) {
            vTaskDelay(pdMS_TO_TICKS(pattern->interval_ms));
        }
    }

    free(pattern);
    beep_task_handle = NULL;
    vTaskDelete(NULL);
}

void audio_service_set_beep_pattern(int interval_ms, int beep_count, int freq, int duration_ms)
{
    if (beep_task_handle) {
        vTaskDelete(beep_task_handle);
        beep_task_handle = NULL;
    }

    beep_pattern_t *pattern = malloc(sizeof(beep_pattern_t));
    pattern->interval_ms = interval_ms;
    pattern->beep_count = beep_count;
    pattern->freq = freq;
    pattern->duration_ms = duration_ms;

    xTaskCreate(audio_service_beep_task, "beep", 4096, pattern, 5, &beep_task_handle);

    ESP_LOGI(TAG, "Beep pattern set: %dHz, %dms, every %dms, %d times",
             freq, duration_ms, interval_ms, beep_count);
}

void audio_service_notify_wifi_connecting(void)
{
    ESP_LOGI(TAG, "WiFi connecting, beep pattern: short 3s x 5");
    audio_service_set_beep_pattern(3000, 5, 880, 100);
}

void audio_service_notify_wifi_connected(void)
{
    ESP_LOGI(TAG, "WiFi connected, beep pattern: long 5s x 5");
    audio_service_set_beep_pattern(5000, 5, 523, 200);
}

void audio_service_notify_wifi_failed(void)
{
    ESP_LOGI(TAG, "WiFi failed, beep pattern: short 1s x 10");
    audio_service_set_beep_pattern(1000, 10, 330, 100);
}
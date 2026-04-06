#include "audio_service.h"
#include "esp_log.h"
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sr_iface.h"
#include "esp_sr_models.h"
#include "model_path.h"
#include "esp_afe_vad.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"

static const char *TAG = "audio_service";

static bool is_running = false;
static bool wake_word_enabled = true;
static TaskHandle_t record_task_handle = NULL;
static TaskHandle_t play_task_handle = NULL;

static wake_word_cb_t on_wake_word = NULL;
static vad_cb_t on_vad_speech_end = NULL;
static audio_data_cb_t on_audio_data = NULL;

static const esp_sr_iface_t *wakenet = NULL;
static model_iface_t *wakenet_model = NULL;
static const esp_afe_vad_iface_t *vad_handle = NULL;
static esp_afe_vad_handle_t vad_inst = NULL;

static bool vad_active = false;
static uint32_t silence_frame_count = 0;
static uint32_t speech_frame_count = 0;

#define VAD_SILENCE_THRESHOLD 30
#define VAD_SPEECH_MIN_FRAMES 10
#define I2S_SAMPLE_RATE 16000
#define I2S_READ_SIZE 1600

static void audio_record_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Audio record task started");

    int16_t *i2s_buffer = malloc(I2S_READ_SIZE);
    if (!i2s_buffer) {
        ESP_LOGE(TAG, "Failed to allocate I2S buffer");
        vTaskDelete(NULL);
        return;
    }

    size_t bytes_read = 0;

    while (is_running) {
        if (i2s_read(I2S_NUM_0, i2s_buffer, I2S_READ_SIZE, &bytes_read, pdMS_TO_TICKS(100)) == ESP_OK) {
            int samples = bytes_read / sizeof(int16_t);
            
            if (wake_word_enabled && wakenet_model) {
                wakenet_state_t wn_state = wakenet->detect(wakenet_model, i2s_buffer);
                if (wn_state == WAKENET_DETECTED) {
                    ESP_LOGI(TAG, "Wake word DETECTED!");
                    vad_active = true;
                    silence_frame_count = 0;
                    speech_frame_count = 0;
                    
                    if (on_wake_word) {
                        on_wake_word();
                    }
                }
            }
            
            if (vad_active && vad_inst) {
                int vad_state = vad_handle->get_state(vad_inst, i2s_buffer, samples);
                
                if (vad_state == ESP_AFE_VAD_SPEECH) {
                    speech_frame_count++;
                    silence_frame_count = 0;
                    
                    if (on_audio_data) {
                        on_audio_data((const uint8_t *)i2s_buffer, bytes_read);
                    }
                } else {
                    silence_frame_count++;
                    
                    if (speech_frame_count > VAD_SPEECH_MIN_FRAMES && silence_frame_count > VAD_SILENCE_THRESHOLD) {
                        ESP_LOGI(TAG, "Speech ended");
                        vad_active = false;
                        
                        if (on_vad_speech_end) {
                            on_vad_speech_end();
                        }
                    }
                }
            }
        }
    }

    free(i2s_buffer);
    vTaskDelete(NULL);
}

static void audio_play_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Audio play task started");

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        if (!is_running) {
            break;
        }
    }
    
    vTaskDelete(NULL);
}

esp_err_t audio_service_init(void)
{
    ESP_LOGI(TAG, "Initializing audio service with ESP-SR...");
    
    esp_err_t ret = ESP_OK;

    char *wn_model_name = NULL;
    asr_manager_get_wakeup_word(&wn_model_name);
    ESP_LOGI(TAG, "Wake word model: %s", wn_model_name ? wn_model_name : "default");
    
    wakenet = (const esp_sr_iface_t *)&esp_sr_wakenet_model;
    wakenet_model = wakenet->create(wn_model_name, DET_MODE_ULP);
    
    if (!wakenet_model) {
        ESP_LOGW(TAG, "Failed to create wakenet model, running in passive mode");
        wake_word_enabled = false;
    } else {
        wake_word_enabled = true;
    }
    
    vad_handle = (const esp_afe_vad_iface_t *)&esp_afe_vad;
    vad_inst = vad_handle->create(4096, 16);
    
    if (!vad_inst) {
        ESP_LOGE(TAG, "Failed to create VAD instance");
        ret = ESP_FAIL;
    }
    
    vad_active = false;
    silence_frame_count = 0;
    speech_frame_count = 0;
    
    ESP_LOGI(TAG, "Audio service initialized (WakeNet: %s, VAD: %s)",
             wake_word_enabled ? "enabled" : "disabled",
             vad_inst ? "enabled" : "disabled");
    
    return ret;
}

void audio_service_start(void)
{
    if (is_running) {
        return;
    }

    is_running = true;
    vad_active = wake_word_enabled;
    xTaskCreate(audio_record_task, "audio_record", 4096, NULL, 10, &record_task_handle);

    ESP_LOGI(TAG, "Audio service started");
}

void audio_service_stop(void)
{
    if (!is_running) {
        return;
    }

    is_running = false;
    vad_active = false;
    
    if (record_task_handle) {
        vTaskDelete(record_task_handle);
        record_task_handle = NULL;
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

void audio_service_set_audio_callback(audio_data_cb_t cb)
{
    on_audio_data = cb;
}

void audio_service_play(const uint8_t *data, size_t len)
{
    size_t bytes_written = 0;
    i2s_write(I2S_NUM_0, data, len, &bytes_written, portMAX_DELAY);
}

void audio_service_play_stop(void)
{
    i2s_zero_dma_buffer(I2S_NUM_0);
}

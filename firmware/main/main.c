#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "board.h"
#include "state_machine.h"
#include "audio_service.h"
#include "websocket_client.h"
#include "display.h"
#include "wifi_init.h"
#include "wifi_provisioning.h"

static const char *TAG = "child-friend-ai";

static void on_wake_word_detected(void)
{
    ESP_LOGI(TAG, "Wake word detected!");
    state_machine_set_state(STATE_LISTENING);
    display_show_state(DISPLAY_STATE_LISTENING);
    websocket_send_status("listening");
    websocket_send_wakeword(true);
}

static void on_vad_speech_end(void)
{
    ESP_LOGI(TAG, "Speech ended, processing...");
    state_machine_set_state(STATE_PROCESSING);
    display_show_state(DISPLAY_STATE_PROCESSING);
    websocket_send_status("processing");
    websocket_send_audio_end();
}

static void on_state_changed(state_t state)
{
    ESP_LOGI(TAG, "State changed to: %d", state);
}

static void on_ws_connected(void)
{
    ESP_LOGI(TAG, "WebSocket connected");
    display_show_state(DISPLAY_STATE_IDLE);
}

static void on_ws_disconnected(void)
{
    ESP_LOGI(TAG, "WebSocket disconnected");
    display_show_state(DISPLAY_STATE_ERROR);
}

static void on_ws_text(const char *text)
{
    ESP_LOGI(TAG, "Received text: %s", text);
}

static void on_ws_binary(const uint8_t *data, size_t len)
{
    ESP_LOGI(TAG, "Received binary: %d bytes", len);
    audio_service_play(data, len);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Child Friend AI Starting...");

    ESP_ERROR_CHECK(nvs_flash_init());
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    state_machine_init();
    state_machine_set_callback(on_state_changed);

    board_init(NULL);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    display_init();
    display_show_state(DISPLAY_STATE_STARTING);

    if (!wifi_provisioning_is_completed()) {
        ESP_LOGI(TAG, "WiFi not configured, starting provisioning...");
        display_show_text("配网中...\n连接WiFi:\n小星配网\n密码:12345678");
        wifi_provisioning_start();
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    char ssid[32] = {0};
    char password[64] = {0};
    wifi_provisioning_get_credentials(ssid, password);
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    wifi_init_sta();

    while (!wifi_is_connected) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    wifi_provisioning_stop();

    ret = audio_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio service init failed!");
    }
    audio_service_set_wake_word_callback(on_wake_word_detected);
    audio_service_set_vad_callback(on_vad_speech_end);

    websocket_init(CONFIG_SERVER_HOST, CONFIG_SERVER_PORT);
    websocket_set_connected_callback(on_ws_connected);
    websocket_set_disconnected_callback(on_ws_disconnected);
    websocket_set_text_callback(on_ws_text);
    websocket_set_binary_callback(on_ws_binary);

    audio_service_start();

    state_machine_set_state(STATE_IDLE);
    display_show_state(DISPLAY_STATE_IDLE);

    ESP_LOGI(TAG, "System initialized!");
}

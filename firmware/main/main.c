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
#include "button.h"

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

static void on_button_event(button_event_t event)
{
    if (event == BUTTON_EVENT_LONG_PRESS) {
        ESP_LOGI(TAG, "Button long press - resetting WiFi provisioning");
        wifi_provisioning_clear();
        esp_restart();
    }
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

    audio_service_init();
    audio_service_start();

    button_init();
    button_set_callback(on_button_event);

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

    audio_service_notify_wifi_connecting();

    wifi_init_sta(ssid, password);

    int retry_count = 0;
    while (!wifi_is_connected && retry_count < 30) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry_count++;
        ESP_LOGI(TAG, "Waiting for WiFi connection... (%d)", retry_count);
    }

    if (!wifi_is_connected) {
        ESP_LOGE(TAG, "WiFi connection failed!");
        audio_service_notify_wifi_failed();
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    audio_service_notify_wifi_connected();
    wifi_provisioning_stop();

    char ws_host[64];
    uint16_t ws_port;
    wifi_provisioning_get_websocket_credentials(ws_host, &ws_port);
    ESP_LOGI(TAG, "WebSocket server: %s:%d", ws_host, ws_port);

    websocket_init(ws_host, ws_port);
    websocket_set_connected_callback(on_ws_connected);
    websocket_set_disconnected_callback(on_ws_disconnected);
    websocket_set_text_callback(on_ws_text);
    websocket_set_binary_callback(on_ws_binary);

    state_machine_set_state(STATE_IDLE);
    display_show_state(DISPLAY_STATE_IDLE);

    ESP_LOGI(TAG, "System initialized!");
}
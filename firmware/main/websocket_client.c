#include "websocket_client.h"
#include <string.h>
#include <sys/socket.h>
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_event.h"
#include "nvs_flash.h"

static const char *TAG = "websocket";

static esp_websocket_client_handle_t client = NULL;
static bool connected = false;
static char ws_uri[128];

static ws_connected_cb_t on_connected = NULL;
static ws_disconnected_cb_t on_disconnected = NULL;
static ws_text_cb_t on_text = NULL;
static ws_binary_cb_t on_binary = NULL;

static void websocket_task(void *pvParameters);

static void log_error_if_nonzero(const char *message, int errorno)
{
    if (errorno != 0) {
        ESP_LOGE(TAG, "Last error %s: %d", message, errorno);
    }
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        connected = true;
        if (on_connected) {
            on_connected();
        }
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        connected = false;
        log_error_if_nonzero("HTTP status code", data->error_handle->http_errno);
        if (on_disconnected) {
            on_disconnected();
        }
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA");
        ESP_LOGI(TAG, "Received opcode: %d", data->op_code);
        if (data->op_code == 0x1) {
            ESP_LOGI(TAG, "Received text: %.*s", data->data_len, (char *)data->data_ptr);
            if (on_text) {
                on_text((const char *)data->data_ptr);
            }
        } else if (data->op_code == 0x2) {
            ESP_LOGI(TAG, "Received binary data: %d bytes", data->data_len);
            if (on_binary) {
                on_binary((const uint8_t *)data->data_ptr, data->data_len);
            }
        } else if (data->op_code == 0x8) {
            ESP_LOGI(TAG, "Received close frame");
            connected = false;
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        log_error_if_nonzero("WebSocket", data->error_handle->error_type);
        log_error_if_nonzero("HTTP status code", data->error_handle->http_errno);
        log_error_if_nonzero("HTTP failure", data->error_handle->http_handle);
        break;
    default:
        break;
    }
}

void websocket_init(const char *host, uint16_t port)
{
    sprintf(ws_uri, "ws://%s:%d/", host, port);

    esp_websocket_client_config_t websocket_cfg = {
        .uri = ws_uri,
        .keepalive = 60,
        .disable_auto_reconnect = false,
        .ping_interval_sec = 30,
        .ping_pong_interval_sec = 10,
    };

    client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);

    ESP_LOGI(TAG, "Connecting to %s...", ws_uri);
    esp_websocket_client_start(client);
}

void websocket_deinit(void)
{
    if (client) {
        esp_websocket_client_stop(client);
        esp_websocket_client_destroy(client);
        client = NULL;
    }
}

bool websocket_is_connected(void)
{
    return connected;
}

int websocket_send_text(const char *text)
{
    if (!client || !connected) {
        return -1;
    }
    return esp_websocket_client_send_text(client, text, strlen(text), portMAX_DELAY);
}

int websocket_send_binary(const uint8_t *data, size_t len)
{
    if (!client || !connected) {
        return -1;
    }
    return esp_websocket_client_send_bin(client, (const char *)data, len, portMAX_DELAY);
}

int websocket_send_status(const char *status)
{
    char buf[128];
    int len = sprintf(buf, "{\"type\":\"status\",\"content\":\"%s\"}", status);
    return websocket_send_text(buf);
}

int websocket_send_wakeword(bool detected)
{
    char buf[128];
    int len = sprintf(buf, "{\"type\":\"wakeword\",\"detected\":%s}", detected ? "true" : "false");
    return websocket_send_text(buf);
}

int websocket_send_audio_end(void)
{
    return websocket_send_text("{\"type\":\"audio_end\"}");
}

void websocket_set_connected_callback(ws_connected_cb_t cb)
{
    on_connected = cb;
}

void websocket_set_disconnected_callback(ws_disconnected_cb_t cb)
{
    on_disconnected = cb;
}

void websocket_set_text_callback(ws_text_cb_t cb)
{
    on_text = cb;
}

void websocket_set_binary_callback(ws_binary_cb_t cb)
{
    on_binary = cb;
}

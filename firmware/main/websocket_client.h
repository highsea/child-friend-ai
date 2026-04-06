#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*ws_connected_cb_t)(void);
typedef void (*ws_disconnected_cb_t)(void);
typedef void (*ws_text_cb_t)(const char *text);
typedef void (*ws_binary_cb_t)(const uint8_t *data, size_t len);

void websocket_init(const char *host, uint16_t port);
void websocket_deinit(void);
bool websocket_is_connected(void);
int websocket_send_text(const char *text);
int websocket_send_binary(const uint8_t *data, size_t len);
int websocket_send_status(const char *status);
int websocket_send_wakeword(bool detected);
int websocket_send_audio_end(void);

void websocket_set_connected_callback(ws_connected_cb_t cb);
void websocket_set_disconnected_callback(ws_disconnected_cb_t cb);
void websocket_set_text_callback(ws_text_cb_t cb);
void websocket_set_binary_callback(ws_binary_cb_t cb);

#endif

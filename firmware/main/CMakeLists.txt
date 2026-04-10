idf_component_register(
    SRCS "main.c" "wifi_init.c" "wifi_provisioning.c" "audio_service.c" "websocket_client.c" "display.c" "board.c" "state_machine.c" "button.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_event nvs_flash driver esp_http_server
)

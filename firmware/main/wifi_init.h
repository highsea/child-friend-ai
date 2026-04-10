#ifndef WIFI_INIT_H
#define WIFI_INIT_H

#include "esp_err.h"
#include <stdbool.h>

extern volatile bool wifi_is_connected;

void wifi_init_sta(const char *ssid, const char *password);

#endif

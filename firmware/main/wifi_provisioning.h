#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

void wifi_provisioning_start(void);
bool wifi_provisioning_is_completed(void);
void wifi_provisioning_get_credentials(char *ssid, char *password);
void wifi_provisioning_stop(void);
esp_err_t wifi_provisioning_clear(void);

#endif

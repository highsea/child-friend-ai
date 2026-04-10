#include "wifi_provisioning.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "esp_http_server.h"
#include "string.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_mac.h"

static const char *TAG = "wifi_prov";

#define PROVISIONING_AP_CHANNEL 1
#define PROVISIONING_MAX_CONNECTIONS 4

static bool provisioning_completed = false;
static char saved_ssid[32] = {0};
static char saved_password[64] = {0};
static char ap_ssid[32] = {0};

static void url_decode(const char *src, char *dest);
static esp_err_t reset_handler(httpd_req_t *req);

static esp_err_t root_handler(httpd_req_t *req)
{
    const char *resp =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
        "<title>Child Friend AI 设备配网</title>"
        "<style>"
        "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
        "       max-width: 400px; margin: 50px auto; padding: 20px; }"
        "h1 { text-align: center; color: #333; }"
        ".form-group { margin-bottom: 20px; }"
        "label { display: block; margin-bottom: 5px; font-weight: bold; }"
        "input { width: 100%; padding: 12px; border: 1px solid #ddd; border-radius: 8px; "
        "        box-sizing: border-box; font-size: 16px; }"
        "button { width: 100%; padding: 15px; background: #4CAF50; color: white; "
        "         border: none; border-radius: 8px; font-size: 16px; cursor: pointer; }"
        "button:hover { background: #45a049; }"
        ".tips { text-align: center; color: #666; margin-top: 20px; font-size: 14px; }"
        "</style>"
        "</head>"
        "<body>"
        "<h1>🤖 设备配网</h1>"
        "<form method=\"POST\" action=\"/save\">"
        "<div class=\"form-group\">"
        "<label>WiFi 名称 (SSID)</label>"
        "<input type=\"text\" name=\"ssid\" required placeholder=\"请输入WiFi名称\">"
        "</div>"
        "<div class=\"form-group\">"
        "<label>WiFi 密码</label>"
        "<input type=\"password\" name=\"password\" placeholder=\"请输入WiFi密码\">"
        "</div>"
        "<div class=\"form-group\">"
        "<label>服务器地址</label>"
        "<input type=\"text\" name=\"ws_host\" value=\"192.168.31.181\" required placeholder=\"如: 192.168.31.181\">"
        "</div>"
        "<div class=\"form-group\">"
        "<label>服务器端口</label>"
        "<input type=\"number\" name=\"ws_port\" value=\"12020\" required placeholder=\"如: 12020\">"
        "</div>"
        "<button type=\"submit\">保存并连接</button>"
        "</form>"
        "<form method=\"POST\" action=\"/reset\" style=\"margin-top:30px;\">"
        "<button type=\"submit\" style=\"background:#f44336;\">恢复出厂设置</button>"
        "</form>"
        "<p class=\"tips\">请连接成功后，设备将自动重启</p>"
        "</body>"
        "</html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char *ssid_start = strstr(buf, "ssid=");
    char *pass_start = strstr(buf, "password=");
    char *ws_host_start = strstr(buf, "ws_host=");
    char *ws_port_start = strstr(buf, "ws_port=");

    char ws_host[64] = {0};
    char ws_port[8] = {0};

    if (ssid_start) {
        ssid_start += 5;
        char *ssid_end = strchr(ssid_start, '&');
        if (ssid_end) {
            *ssid_end = '\0';
        }
        url_decode(ssid_start, saved_ssid);
    }

    if (pass_start) {
        pass_start += 9;
        char *pass_end = strchr(pass_start, '&');
        if (pass_end) {
            *pass_end = '\0';
        }
        url_decode(pass_start, saved_password);
    }

    if (ws_host_start) {
        ws_host_start += 8;
        char *ws_host_end = strchr(ws_host_start, '&');
        if (ws_host_end) {
            *ws_host_end = '\0';
        }
        url_decode(ws_host_start, ws_host);
    }

    if (ws_port_start) {
        ws_port_start += 8;
        char *ws_port_end = strchr(ws_port_start, '&');
        if (ws_port_end) {
            *ws_port_end = '\0';
        }
        url_decode(ws_port_start, ws_port);
    }

    ESP_LOGI(TAG, "=== User Configuration ===");
    ESP_LOGI(TAG, "WiFi SSID: %s", saved_ssid);
    ESP_LOGI(TAG, "WiFi Password: %s", saved_password);
    ESP_LOGI(TAG, "WebSocket Host: %s, Port: %s", ws_host, ws_port);
    ESP_LOGI(TAG, "========================");

    nvs_handle_t nvs;
    if (nvs_open("wifi_config", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", saved_ssid);
        nvs_set_str(nvs, "password", saved_password);
        nvs_set_str(nvs, "ws_host", ws_host);
        nvs_set_str(nvs, "ws_port", ws_port);
        nvs_commit(nvs);
        nvs_close(nvs);
        ESP_LOGI(TAG, "Credentials saved to NVS");
    }

    provisioning_completed = true;

    const char *resp =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<meta http-equiv=\"refresh\" content=\"3;url=/\">"
        "<title>连接中...</title>"
        "<style>"
        "body { font-family: -apple-system, sans-serif; text-align: center; padding: 50px; }"
        ".spinner { border: 4px solid #f3f3f3; border-top: 4px solid #4CAF50; "
        "           border-radius: 50%; width: 40px; height: 40px; "
        "           animation: spin 1s linear infinite; margin: 20px auto; }"
        "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }"
        "</style>"
        "</head>"
        "<body>"
        "<h2>✅ 配置成功！</h2>"
        "<p>正在连接 WiFi...</p>"
        "<div class=\"spinner\"></div>"
        "<p>设备将自动重启并连接网络</p>"
        "</body>"
        "</html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, resp, strlen(resp));

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

static void url_decode(const char *src, char *dest)
{
    while (*src) {
        if (*src == '+') {
            *dest++ = ' ';
        } else if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            *dest++ = (char)strtol(hex, NULL, 16);
            src += 2;
        } else {
            *dest++ = *src;
        }
        src++;
    }
    *dest = '\0';
}

static void wifi_ap_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Client connected to AP");
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "Client disconnected from AP");
    }
}

static void start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler
        };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t save_uri = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = save_handler
        };
        httpd_register_uri_handler(server, &save_uri);

        httpd_uri_t reset_uri = {
            .uri = "/reset",
            .method = HTTP_POST,
            .handler = reset_handler
        };
        httpd_register_uri_handler(server, &reset_uri);

        ESP_LOGI(TAG, "Web server started on port 80");
    }
}

static esp_err_t reset_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Reset requested via web");

    wifi_provisioning_clear();

    const char *resp =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<meta http-equiv=\"refresh\" content=\"3;url=/\">"
        "<title>恢复出厂设置...</title>"
        "<style>"
        "body { font-family: -apple-system, sans-serif; text-align: center; padding: 50px; }"
        "</style>"
        "</head>"
        "<body>"
        "<h2>✅ 已恢复出厂设置！</h2>"
        "<p>设备将自动重启进入配网模式</p>"
        "</body>"
        "</html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, resp, strlen(resp));

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

void wifi_provisioning_start(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(ap_ssid, "hi-friend-%02x%02x", mac[4], mac[5]);

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_ap_event_handler, NULL));

    wifi_config_t ap_config = {
        .ap = {
            .ssid_len = strlen(ap_ssid),
            .channel = PROVISIONING_AP_CHANNEL,
            .max_connection = PROVISIONING_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .required = false
            }
        }
    };

    strncpy((char *)ap_config.ap.ssid, ap_ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.password[0] = '\0';

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi Provisioning AP started");
    ESP_LOGI(TAG, "SSID: %s (open)", ap_ssid);
    ESP_LOGI(TAG, "Connect to http://192.168.4.1 to configure");

    start_web_server();
}

bool wifi_provisioning_is_completed(void)
{
    if (provisioning_completed) {
        return true;
    }

    nvs_handle_t nvs;
    if (nvs_open("wifi_config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t ssid_len = sizeof(saved_ssid);
        if (nvs_get_str(nvs, "ssid", saved_ssid, &ssid_len) == ESP_OK) {
            nvs_get_str(nvs, "password", saved_password, NULL);
            nvs_close(nvs);
            return (saved_ssid[0] != '\0');
        }
        nvs_close(nvs);
    }
    return false;
}

void wifi_provisioning_get_credentials(char *ssid, char *password)
{
    nvs_handle_t nvs;
    if (nvs_open("wifi_config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t ssid_len = sizeof(saved_ssid);
        if (nvs_get_str(nvs, "ssid", saved_ssid, &ssid_len) == ESP_OK) {
            ESP_LOGI(TAG, "NVS read SSID: %s", saved_ssid);
            size_t pass_len = sizeof(saved_password);
            esp_err_t err = nvs_get_str(nvs, "password", saved_password, &pass_len);
            ESP_LOGI(TAG, "NVS read password result: %d, len: %d, password: %s", err, pass_len, saved_password);
            nvs_close(nvs);
            strncpy(ssid, saved_ssid, 32);
            strncpy(password, saved_password, 64);
            return;
        }
        nvs_close(nvs);
    }
    ESP_LOGW(TAG, "Failed to read credentials from NVS!");
}

void wifi_provisioning_get_websocket_credentials(char *host, uint16_t *port)
{
    nvs_handle_t nvs;
    if (nvs_open("wifi_config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t host_len = 64;
        nvs_get_str(nvs, "ws_host", host, &host_len);

        char port_str[8] = {0};
        size_t port_len = sizeof(port_str);
        if (nvs_get_str(nvs, "ws_port", port_str, &port_len) == ESP_OK) {
            *port = atoi(port_str);
        } else {
            *port = 12020;
        }
        nvs_close(nvs);
    } else {
        strcpy(host, "192.168.31.181");
        *port = 12020;
    }
}

void wifi_provisioning_stop(void)
{
    ESP_LOGI(TAG, "Stopping WiFi provisioning AP...");

    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK) {
        if (mode & WIFI_MODE_AP) {
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        }
    }

    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif) {
        esp_netif_destroy(ap_netif);
    }

    ESP_LOGI(TAG, "WiFi provisioning AP stopped");
}

esp_err_t wifi_provisioning_clear(void)
{
    ESP_LOGI(TAG, "Clearing WiFi credentials...");

    nvs_handle_t nvs;
    esp_err_t ret = nvs_open("wifi_config", NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS");
        return ret;
    }

    ret = nvs_erase_key(nvs, "ssid");
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to erase SSID");
        nvs_close(nvs);
        return ret;
    }

    ret = nvs_erase_key(nvs, "password");
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to erase password");
        nvs_close(nvs);
        return ret;
    }

    ret = nvs_erase_key(nvs, "ws_host");
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to erase ws_host");
        nvs_close(nvs);
        return ret;
    }

    ret = nvs_erase_key(nvs, "ws_port");
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to erase ws_port");
        nvs_close(nvs);
        return ret;
    }

    ret = nvs_commit(nvs);
    nvs_close(nvs);

    if (ret == ESP_OK) {
        memset(saved_ssid, 0, sizeof(saved_ssid));
        memset(saved_password, 0, sizeof(saved_password));
        provisioning_completed = false;
        ESP_LOGI(TAG, "WiFi credentials cleared");
    }

    return ret;
}

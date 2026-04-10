#include "button.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "button";

static button_callback_t s_callback = NULL;
static TaskHandle_t s_button_task_handle = NULL;
static bool s_running = false;

static void button_task(void *arg)
{
    bool last_level = true;
    uint32_t press_start_time = 0;
    bool long_press_reported = false;

    while (s_running) {
        bool level = gpio_get_level(BUTTON_GPIO_BOOT);

        if (level == 0 && last_level == true) {
            press_start_time = xTaskGetTickCount();
            long_press_reported = false;
            if (s_callback) {
                s_callback(BUTTON_EVENT_PRESSED);
            }
        } else if (level == 1 && last_level == false) {
            if (s_callback) {
                s_callback(BUTTON_EVENT_RELEASED);
            }
        }

        if (level == 0) {
            uint32_t press_duration = (xTaskGetTickCount() - press_start_time) * portTICK_PERIOD_MS;
            if (press_duration >= BUTTON_PRESS_THRESHOLD_MS && !long_press_reported) {
                long_press_reported = true;
                if (s_callback) {
                    s_callback(BUTTON_EVENT_LONG_PRESS);
                }
            }
        }

        last_level = level;
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    vTaskDelete(NULL);
}

void button_init(void)
{
    ESP_LOGI(TAG, "Button initialization");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO_BOOT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    s_running = true;
    xTaskCreate(&button_task, "button", 2048, NULL, 5, &s_button_task_handle);

    ESP_LOGI(TAG, "Button initialized on GPIO%" PRIu32, (uint32_t)BUTTON_GPIO_BOOT);
}

void button_set_callback(button_callback_t cb)
{
    s_callback = cb;
}

bool button_is_pressed(void)
{
    return gpio_get_level(BUTTON_GPIO_BOOT) == 0;
}

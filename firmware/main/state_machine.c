#include "state_machine.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "state_machine";
static state_t current_state = STATE_IDLE;
static state_callback_t callback = NULL;

static const char *state_names[] = {
    "IDLE",
    "LISTENING",
    "PROCESSING",
    "SPEAKING",
    "ERROR"
};

void state_machine_init(void)
{
    current_state = STATE_IDLE;
    ESP_LOGI(TAG, "State machine initialized");
}

void state_machine_set_state(state_t state)
{
    if (current_state != state) {
        current_state = state;
        ESP_LOGI(TAG, "State changed: %s", state_names[state]);
        if (callback) {
            callback(state);
        }
    }
}

state_t state_machine_get_state(void)
{
    return current_state;
}

void state_machine_set_callback(state_callback_t cb)
{
    callback = cb;
}

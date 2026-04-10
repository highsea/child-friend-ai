#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"

#define BUTTON_GPIO_BOOT GPIO_NUM_0
#define BUTTON_PRESS_THRESHOLD_MS 3000

typedef enum {
    BUTTON_EVENT_PRESSED,
    BUTTON_EVENT_RELEASED,
    BUTTON_EVENT_LONG_PRESS,
} button_event_t;

typedef void (*button_callback_t)(button_event_t event);

void button_init(void);
void button_set_callback(button_callback_t cb);
bool button_is_pressed(void);

#endif

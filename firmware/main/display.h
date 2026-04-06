#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

typedef enum {
    DISPLAY_STATE_STARTING = 0,
    DISPLAY_STATE_IDLE,
    DISPLAY_STATE_LISTENING,
    DISPLAY_STATE_PROCESSING,
    DISPLAY_STATE_SPEAKING,
    DISPLAY_STATE_ERROR
} display_state_t;

void display_init(void);
void display_show_state(display_state_t state);
void display_show_text(const char *text);
void display_clear(void);

#endif

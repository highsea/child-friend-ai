#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    DISPLAY_STATE_STARTING,
    DISPLAY_STATE_PROV_AP,
    DISPLAY_STATE_PROV_SCANNING,
    DISPLAY_STATE_PROV_CONNECTING,
    DISPLAY_STATE_PROV_SUCCESS,
    DISPLAY_STATE_PROV_FAILED,
    DISPLAY_STATE_IDLE,
    DISPLAY_STATE_LISTENING,
    DISPLAY_STATE_PROCESSING,
    DISPLAY_STATE_SPEAKING,
    DISPLAY_STATE_ERROR
} display_state_t;

void display_init(void);
void display_show_state(display_state_t state);
void display_show_text(const char *text);
void display_show_text_with_bg(const char *text, uint16_t fg_color, uint16_t bg_color);
void display_show_emoji(const char *emoji);
void display_clear(void);

#endif

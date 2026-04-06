#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STATE_IDLE = 0,
    STATE_LISTENING,
    STATE_PROCESSING,
    STATE_SPEAKING,
    STATE_ERROR
} state_t;

typedef void (*state_callback_t)(state_t state);

void state_machine_init(void);
void state_machine_set_state(state_t state);
state_t state_machine_get_state(void);
void state_machine_set_callback(state_callback_t callback);

#endif

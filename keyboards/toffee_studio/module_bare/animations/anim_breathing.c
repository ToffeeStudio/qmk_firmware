#include "anim_breathing.h"
#include <lib/lib8tion/lib8tion.h>   // sin8, scale8, etc.

// No state needed for this simple animation
typedef struct {} breathing_state_t;
static breathing_state_t state_instance;

static HSV breathing_task(animation_params_t* params) {
    HSV hsv = params->config->color;

    uint8_t time = (params->time * params->config->speed) / 2048;

    uint8_t wave = sin8(time);

    hsv.v = scale8(hsv.v, wave);
    return hsv;
}

animation_t anim_breathing = {
    .init       = NULL, // No init function needed
    .task       = breathing_task,
    .state      = &state_instance,
    .state_size = sizeof(breathing_state_t),
};

#include "anim_cycle_left_right.h"
#include <lib/lib8tion/lib8tion.h>

// No state needed for this simple animation
typedef struct {} cycle_left_right_state_t;
static cycle_left_right_state_t state_instance;

/**
 * @brief Task function for a horizontal rainbow cycle animation.
 */
static HSV cycle_left_right_task(animation_params_t* params) {
    HSV hsv;
    hsv.s = 255; // Full saturation for a vibrant rainbow
    hsv.v = params->config->brightness;

    uint8_t time = (params->time * params->config->speed) / 2048;
    uint8_t pos_hue = (params->pos.x * 4);
    hsv.h = time + pos_hue;

    return hsv;
}

// The public animation struct that the manager will use
animation_t anim_cycle_left_right = {
    .init       = NULL,
    .task       = cycle_left_right_task,
    .state      = &state_instance,
    .state_size = sizeof(cycle_left_right_state_t),
};
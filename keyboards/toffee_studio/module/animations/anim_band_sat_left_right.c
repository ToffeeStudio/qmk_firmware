#include "anim_band_sat_left_right.h"
#include <lib/lib8tion/lib8tion.h>

// Define a center point for the radial animation. These are default QMK values.
// X = 255 / 2 = 127
// Y = 127 / 2 = 63



static HSV band_sat_left_right_task(animation_params_t* params) {
    HSV hsv = params->config->color; // Start with the base color

    // Use the x-coordinate and time to create a moving wave from left to right
    uint32_t time = (params->time * params->config->speed) / 2048;
    uint8_t wave = triwave8(params->pos.x + time);

    // Scale the saturation based on the wave
    hsv.s = scale8(hsv.s, wave);

    return hsv;
}

// The public animation struct that the manager will use
animation_t anim_band_sat_left_right = {
    .name = "Band Sat L-R",
    .init = NULL,
    .task = band_sat_left_right_task,
    .state = NULL,
    .state_size = 0,
};

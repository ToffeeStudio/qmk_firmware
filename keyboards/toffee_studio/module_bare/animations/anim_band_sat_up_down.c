#include "anim_band_sat_up_down.h"
#include <lib/lib8tion/lib8tion.h>

static HSV band_sat_up_down_task(animation_params_t* params) {
    HSV hsv = params->config->color; // Start with the base color

    // Use the y-coordinate and time to create a moving wave from top to bottom
    uint32_t time = (params->time * params->config->speed) / 2048;
    uint8_t wave = triwave8(params->pos.y + time); // Changed to pos.y

    // Scale the saturation based on the wave
    hsv.s = scale8(hsv.s, wave);

    return hsv;
}

animation_t anim_band_sat_up_down = {
    .name = "Band Sat U-D",
    .init = NULL,
    .task = band_sat_up_down_task,
    .state = NULL,
    .state_size = 0,
};

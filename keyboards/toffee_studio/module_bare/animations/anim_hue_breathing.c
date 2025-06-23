#include "anim_hue_breathing.h"
#include <lib/lib8tion/lib8tion.h>

// No state needed for this animation
typedef struct {} hue_breathing_state_t;
static hue_breathing_state_t state_instance;

static HSV hue_breathing_task(animation_params_t* params) {
    HSV hsv = params->config->color;

    // Create a time variable that respects the animation speed
    uint8_t time = (params->time * params->config->speed) / 2048;

    // A sine wave for the breathing effect, 0 -> 255 -> 0
    uint8_t wave = sin8(time);

    // Define the maximum amount of hue shift (e.g., 32 out of 255)
    uint8_t huedelta = 32;
    uint8_t hue_shift = scale8(wave, huedelta);

    // Add the breathing shift to the base hue
    hsv.h += hue_shift;

    // Unlike normal breathing, we don't scale the brightness (value)
    // hsv.v remains at the user's configured level.

    return hsv;
}

animation_t anim_hue_breathing = {
    .init       = NULL,
    .task       = hue_breathing_task,
    .state      = &state_instance,
    .state_size = sizeof(hue_breathing_state_t),
};

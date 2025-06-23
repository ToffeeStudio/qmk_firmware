#include "anim_rainbow_vortex.h"
#include <lib/lib8tion/lib8tion.h>
#include <math.h> // for atan2f

// No state needed
typedef struct {} rainbow_vortex_state_t;
static rainbow_vortex_state_t state_instance;

// Center of the keyboard
#define CENTER_X 130
#define CENTER_Y 104

static HSV rainbow_vortex_task(animation_params_t* params) {
    HSV hsv;
    hsv.s = 255;
    hsv.v = params->config->brightness;

    // Calculate angle of the LED relative to the center
    float angle = atan2f(params->pos.y - CENTER_Y, params->pos.x - CENTER_X);

    // Convert angle from radians (-PI to PI) to 0-255 range for hue
    uint8_t angle_hue = (uint8_t)((angle + M_PI) * 255.0f / (2.0f * M_PI));

    // Get a smoothly shifting time value
    uint8_t time = (params->time * params->config->speed) / 2048;

    hsv.h = time + angle_hue;
    return hsv;
}

animation_t anim_rainbow_vortex = {
    .name       = "Rainbow Vortex",
    .init       = NULL,
    .task       = rainbow_vortex_task,
    .state      = &state_instance,
    .state_size = sizeof(rainbow_vortex_state_t),
};

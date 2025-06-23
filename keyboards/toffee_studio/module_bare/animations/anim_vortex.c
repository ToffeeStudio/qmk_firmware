#include "anim_vortex.h"
#include <lib/lib8tion/lib8tion.h>
#include <math.h> // for atan2f

// No state needed
typedef struct {} vortex_state_t;
static vortex_state_t state_instance;

// Center of the keyboard
#define CENTER_X 130
#define CENTER_Y 104

static HSV vortex_task(animation_params_t* params) {
    HSV hsv = params->config->color; // Use the configured color
    hsv.v = params->config->brightness;

    // Calculate angle of the LED relative to the center
    float angle = atan2f(params->pos.y - CENTER_Y, params->pos.x - CENTER_X);

    // Convert angle from radians (-PI to PI) to 0-255 range
    uint8_t angle_val = (uint8_t)((angle + M_PI) * 255.0f / (2.0f * M_PI));

    // Get a smoothly shifting time value
    uint8_t time = (params->time * params->config->speed) / 2048;

    // Use a sine wave to modulate saturation between a low value (e.g., 64) and full (255)
    // This creates the two-toned swirl effect
    uint8_t sin_wave = sin8(time + angle_val);
    hsv.s = scale8(sin_wave, 223) + 32; // Scale sin output (0-255) to 32-255 range for a more extreme effect

    return hsv;
}

animation_t anim_vortex = {
    .name       = "Vortex",
    .init       = NULL,
    .task       = vortex_task,
    .state      = &state_instance,
    .state_size = sizeof(vortex_state_t),
};

#include "anim_comet_tail.h"
#include <lib/lib8tion/lib8tion.h>
#include <math.h>

typedef struct {} comet_tail_state_t;
static comet_tail_state_t state_instance;

#define COMET_LENGTH 100 // How "long" the tail is
#define MAX_X 255 // Max coordinate for the keyboard layout

static HSV comet_tail_task(animation_params_t* params) {
    // --- Comet Head Position ---
    // Simple horizontal movement from left to right, then wraps around.
    uint32_t time_scaled = (params->time * params->config->speed) / 50;
    int16_t head_x = time_scaled % (MAX_X + COMET_LENGTH) - COMET_LENGTH;

    // --- LED Calculation ---
    // Calculate the distance of the LED from the comet's head *horizontally*.
    int16_t dist_x = params->pos.x - head_x;

    // Check if the LED is within the comet's tail.
    if (dist_x > 0 && dist_x < COMET_LENGTH) {
        // This LED is part of the comet or tail.
        HSV hsv = params->config->color;

        // The head of the comet is the brightest.
        // The tail fades linearly behind it.
        uint8_t brightness_factor = 255 - (dist_x * 255) / COMET_LENGTH;

        // Apply a curve to the fade to make it look more like a tail
        brightness_factor = ease8InOutCubic(brightness_factor);

        hsv.v = scale8(params->config->brightness, brightness_factor);
        return hsv;
    }

    // If not part of the comet, the LED is off.
    return (HSV){0, 0, 0};
}

animation_t anim_comet_tail = {
    .name       = "Comet Tail",
    .init       = NULL,
    .task       = comet_tail_task,
    .state      = &state_instance,
    .state_size = sizeof(comet_tail_state_t),
};

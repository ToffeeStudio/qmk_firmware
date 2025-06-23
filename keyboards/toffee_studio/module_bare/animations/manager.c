#include "manager.h"
#include "quantum.h"
#include <lib/lib8tion/lib8tion.h>   // for scale8(), sin8(), etc.
#include "color.h"

// --- Include all your animation modules here ---
// As you create new animations, you must include their header files.
#include "anim_breathing.h"
#include "anim_cycle_left_right.h"
#include "anim_cycle_up_down.h"
#include "anim_band_sat_left_right.h"
#include "anim_band_sat_up_down.h"
#include "anim_hue_breathing.h"
#include "anim_solid.h"
#include "anim_rainbow_vortex.h"
// #include "anim_rainbow_wave.h"


// The global configuration for the entire underglow system.
animation_config_t g_underglow_config;


// --- Master List of Animations ---
// Add a pointer to each new animation's public struct here.
// The order in this array determines the cycle order.
animation_t* underglow_animations[] = {
    &anim_solid,
    &anim_breathing,
    &anim_cycle_left_right,
    &anim_cycle_up_down,
    &anim_band_sat_left_right,
    &anim_band_sat_up_down,
    &anim_hue_breathing,
    &anim_rainbow_vortex,
    // &anim_rainbow_wave,
};

// This calculates the total number of animations automatically.
const uint8_t UNDERGLOW_ANIMATION_COUNT = sizeof(underglow_animations) / sizeof(animation_t*);


/**
 * @brief Initializes the underglow animation manager.
 */
void underglow_manager_init(void) {
    g_underglow_config.brightness = 200;
    g_underglow_config.speed      = 100;
    g_underglow_config.color      = (HSV){170, 255, 255}; // A bright cyan
    g_underglow_config.current_animation_id = 0;
    g_underglow_config.active_animation = underglow_animations[0];

    // If the selected animation has an initialization function, run it.
    if (g_underglow_config.active_animation->init) {
        g_underglow_config.active_animation->init(g_underglow_config.active_animation->state);
    }
}

/**
 * @brief The main update task for the underglow system.
 */
void underglow_manager_task(void) {
    // Prepare the parameter struct that will be passed to the animation functions.
    animation_params_t params;
    params.time   = timer_read32();
    params.config = &g_underglow_config;
    params.state  = g_underglow_config.active_animation->state;

    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        // Check the flag of each LED. If it's not underglow, we skip it.
        if (HAS_FLAGS(g_led_config.flags[i], LED_FLAG_UNDERGLOW)) {
            params.led_index = i;
            params.pos       = g_led_config.point[i];

            // 1. Get the calculated color from the current animation's task function.
            HSV hsv_out = g_underglow_config.active_animation->task(&params);

            // 2. Apply the global brightness setting from our config.
            hsv_out.v = scale8(hsv_out.v, g_underglow_config.brightness);

            // 3. Convert the final HSV color to RGB.
            RGB rgb_out = hsv_to_rgb(hsv_out);

            // 4. Set the LED color, overwriting the default QMK effect for this LED.
            rgb_matrix_set_color(i, rgb_out.r, rgb_out.g, rgb_out.b);
        }
    }
}

/**
 * @brief Cycles to the next available underglow animation.
 */
void underglow_manager_next_anim(void) {
    // Increment the animation ID, wrapping around to the beginning if we reach the end.
    g_underglow_config.current_animation_id = (g_underglow_config.current_animation_id + 1) % UNDERGLOW_ANIMATION_COUNT;

    // Update the pointer to the now-active animation.
    g_underglow_config.active_animation = underglow_animations[g_underglow_config.current_animation_id];

    // If the new animation has an initialization function, run it to reset its state.
    if (g_underglow_config.active_animation->init) {
        g_underglow_config.active_animation->init(g_underglow_config.active_animation->state);
    }
}

/**
 * @brief Sets the underglow animation to a specific ID.
 * @param anim_id The index of the animation in the underglow_animations array.
 */
void underglow_manager_set_anim(uint8_t anim_id) {
    if (anim_id >= UNDERGLOW_ANIMATION_COUNT) {
        return;
    }

    // Set the new animation
    g_underglow_config.current_animation_id = anim_id;
    g_underglow_config.active_animation = underglow_animations[anim_id];

    // Initialize the new animation if it has an init function.
    if (g_underglow_config.active_animation->init) {
        g_underglow_config.active_animation->init(g_underglow_config.active_animation->state);
    }
}

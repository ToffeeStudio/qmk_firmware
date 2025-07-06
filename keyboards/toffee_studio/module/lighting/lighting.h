#pragma once

#include "quantum_keycodes.h"
#include "rgb_matrix_types.h"   // for led_point_t and led_config_t

extern led_config_t g_led_config;

// Custom keycodes for the keymap
enum custom_keycodes {
    UG_ANIM = SAFE_RANGE, // Underglow Next Animation
};

void lighting_init(void);
void lighting_task(void);

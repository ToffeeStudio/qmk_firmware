#pragma once

#include "quantum_keycodes.h"
#include "rgb_matrix_types.h"   // for led_point_t and led_config_t

extern led_config_t g_led_config;

// Keep a command-ready rescue window before any LED output, then fade in.
#define MODULE_LIGHTING_STARTUP_DELAY_MS 1000U
#define MODULE_LIGHTING_STARTUP_FADE_MS 1000U

// Custom keycodes for the keymap
enum custom_keycodes {
    UG_ANIM = SAFE_RANGE, // Underglow Next Animation
};

void lighting_init(void);
void lighting_task(void);
void lighting_enable_underglow(void);
void keylight_set_brightness(uint16_t led_index, uint8_t brightness);

#ifdef MODULE_LED_DISCOVERY
#define LIGHTING_DISCOVERY_LED_COUNT 5
void lighting_discovery_set_leds(const uint8_t *led_indices);
uint8_t lighting_discovery_show_front(void);
uint8_t lighting_discovery_show_all(void);
uint8_t lighting_discovery_show_back(bool cycle, uint8_t phase);
#endif

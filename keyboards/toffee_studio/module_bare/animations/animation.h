#pragma once

#include "rgb_matrix.h"

struct animation_t;
struct animation_config_t;

typedef struct {
    const struct animation_config_t* config; // Pointer to the zone's config (brightness, speed, color)
    uint16_t led_index;                      // The index of the LED (0 to RGB_MATRIX_LED_COUNT-1)
    led_point_t pos;                         // The (x, y) coordinate of the LED
    uint32_t time;                           // The current system time (in ms)
    void* state;                             // Pointer to the animation's private, persistent state
} animation_params_t;


typedef struct animation_t {
    const char* name;
    void (*init)(void* state_arg);
    HSV (*task)(animation_params_t* params);
    void* state;
    size_t state_size;
} animation_t;


typedef struct animation_config_t {
    uint8_t     brightness; // 0-255
    uint8_t     speed;      // 0-255 (animations will interpret this)
    HSV         color;      // The base color for the effect
    uint8_t     current_animation_id; // Index in the animation list
    animation_t* active_animation;    // Pointer to the currently running animation
} animation_config_t;

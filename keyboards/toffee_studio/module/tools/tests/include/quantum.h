#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define RGB_MATRIX_LED_COUNT 201
#ifdef MODULE_MANUAL_MEASUREMENT
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 255
#else
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 160
#endif
#define MODULE_LED_DISCOVERY_BRIGHTNESS 18
#define MATRIX_ROWS 9
#define MATRIX_COLS 8
#define BOOTMAGIC_LITE
#define BOOTMAGIC_LITE_ROW 0
#define BOOTMAGIC_LITE_COLUMN 0
#define NO_LED 255
#define SAFE_RANGE 0x5F00
#define LED_FLAG_KEYLIGHT 4
#define LED_FLAG_MODIFIER 1
#define LED_FLAG_UNDERGLOW 2
#define LED_FLAG_INDICATOR 8
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define HAS_FLAGS(value, flags) (((value) & (flags)) == (flags))
#define uprintf(...) ((void)0)
enum {RGB_MATRIX_SOLID_COLOR = 1, RGB_MATRIX_BREATHING, RGB_MATRIX_CYCLE_ALL, RGB_MATRIX_CYCLE_LEFT_RIGHT, RGB_MATRIX_CYCLE_UP_DOWN, RGB_MATRIX_GRADIENT_LEFT_RIGHT, RGB_MATRIX_GRADIENT_UP_DOWN, RGB_MATRIX_RAINBOW_BEACON, RGB_MATRIX_ALPHAS_MODS, RGB_MATRIX_BAND_SAT, RGB_MATRIX_BAND_VAL, RGB_MATRIX_BAND_PINWHEEL_SAT, RGB_MATRIX_BAND_PINWHEEL_VAL, RGB_MATRIX_BAND_SPIRAL_SAT, RGB_MATRIX_BAND_SPIRAL_VAL, RGB_MATRIX_RAINBOW_MOVING_CHEVRON, RGB_MATRIX_CYCLE_OUT_IN, RGB_MATRIX_CYCLE_OUT_IN_DUAL, RGB_MATRIX_CYCLE_PINWHEEL, RGB_MATRIX_CYCLE_SPIRAL, RGB_MATRIX_DUAL_BEACON, RGB_MATRIX_RAINBOW_PINWHEELS, RGB_MATRIX_RAINDROPS, RGB_MATRIX_JELLYBEAN_RAINDROPS, RGB_MATRIX_HUE_BREATHING, RGB_MATRIX_HUE_PENDULUM, RGB_MATRIX_HUE_WAVE, RGB_MATRIX_PIXEL_RAIN, RGB_MATRIX_PIXEL_FLOW, RGB_MATRIX_PIXEL_FRACTAL, RGB_MATRIX_TYPING_HEATMAP, RGB_MATRIX_DIGITAL_RAIN, RGB_MATRIX_SOLID_REACTIVE_SIMPLE, RGB_MATRIX_SOLID_REACTIVE, RGB_MATRIX_SOLID_REACTIVE_WIDE, RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE, RGB_MATRIX_SOLID_REACTIVE_CROSS, RGB_MATRIX_SOLID_REACTIVE_MULTICROSS, RGB_MATRIX_SOLID_REACTIVE_NEXUS, RGB_MATRIX_SOLID_REACTIVE_MULTINEXUS, RGB_MATRIX_SPLASH, RGB_MATRIX_MULTISPLASH, RGB_MATRIX_SOLID_SPLASH, RGB_MATRIX_SOLID_MULTISPLASH};
// Use QMK's real GRB/RGB memory layout; never invent a different host RGB ABI.
#include "../../../../../../quantum/color.h"
typedef struct {uint8_t x, y;} led_point_t;
typedef struct {
    uint8_t matrix_co[MATRIX_ROWS][MATRIX_COLS];
    led_point_t point[RGB_MATRIX_LED_COUNT];
    uint8_t flags[RGB_MATRIX_LED_COUNT];
} led_config_t;
typedef struct {struct {bool pressed;} event;} keyrecord_t;
extern led_config_t g_led_config;
uint8_t matrix_scan(void);
bool matrix_is_on(uint8_t row, uint8_t col);
void wait_ms(uint16_t milliseconds);
void eeconfig_disable(void);
void bootloader_jump(void);
void bootmagic_lite(void);
uint32_t timer_read32(void);
uint32_t timer_elapsed32(uint32_t last);
void rgb_matrix_enable_noeeprom(void);
void rgb_matrix_set_flags_noeeprom(uint8_t flags);
void rgb_matrix_mode_noeeprom(uint8_t mode);
void rgb_matrix_set_speed_noeeprom(uint8_t speed);
void rgb_matrix_sethsv_noeeprom(uint8_t hue, uint8_t saturation, uint8_t value);
void rgb_matrix_set_color(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void rgb_matrix_set_color_all(uint8_t r, uint8_t g, uint8_t b);
RGB hsv_to_rgb_nocie(HSV hsv);

#include "quantum.h"
#include "rgb_matrix.h"
#include "rgb_matrix_types.h"   // for led_point_t and led_config_t
#include "animations/manager.h"
#include "lighting.h"
#include <lib/lib8tion/lib8tion.h>   // for scale8()

// The driver's working color buffer, holding the colors the active effect
// rendered this frame. We read it back to dim individual front LEDs.
extern rgb_led_t rgb_matrix_ws2812_array[RGB_MATRIX_LED_COUNT];

// Per-front-LED brightness override (0-255), applied on top of the active
// effect. 255 = unchanged, 0 = off. Lets a single key's LED be dimmed/off.
static uint8_t g_keylight_brightness[RGB_MATRIX_LED_COUNT];


// NOT QMK-specific
#define KEYLIGHT_COUNT 68

led_config_t g_led_config = {
    {
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED }
    },
    {
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}
    },
    {
        LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,
        LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,
        LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,
        LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,
        LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT
    }
};

const uint8_t led_physical_map[RGB_MATRIX_LED_COUNT] = {
    75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,   88,
    51,  52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,  64, 65,
    49, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36,   35,   34,
    18,    19, 20, 21, 22, 23, 24, 25, 26, 27, 28,  29,   30, 31,
    17,  15,  13,                10,              7,  6,   4, 2, 0,
    74, 73, 72, 71, 70, 69, 68, 67, 66, 50, 33, 48, 32, 16, 14, 12, 11, 9, 8, 5, 3, 1
};

const led_point_t led_physical_pos[RGB_MATRIX_LED_COUNT] = {
    {   8,   0}, {  24,   0}, {  40,   0}, {  56,   0}, {  72,   0}, {  88,   0}, { 104,   0}, { 120,   0}, { 136,   0}, { 152,   0}, { 168,   0}, { 184,   0}, { 200,   0}, { 224,   0},
    {  12,  50}, {  32,  50}, {  48,  50}, {  64,  50}, {  80,  50}, {  96,  50}, { 112,  50}, { 128,  50}, { 144,  50}, { 160,  50}, { 176,  50}, { 192,  50}, { 208,  50}, { 228,  50}, { 244,  50},
    {   8, 100}, {  24, 100}, {  40, 100}, {  56, 100}, {  72, 100}, {  88, 100}, { 104, 100}, { 120, 100}, { 136, 100}, { 152, 100}, { 168, 100}, { 184, 100}, { 200, 100}, { 224, 100}, { 244, 100},
    {  24, 150}, {  52, 150}, {  68, 150}, {  84, 150}, { 100, 150}, { 116, 150}, { 132, 150}, { 148, 150}, { 164, 150}, { 180, 150}, { 196, 150}, { 216, 150}, { 236, 150}, { 252, 150},
    {   8, 200}, {  28, 200}, {  52, 200}, { 124, 200}, { 192, 200}, { 208, 200}, { 228, 200}, { 240, 200}, { 252, 200},
    {10, 8},   {25, 8},   {50, 8},   {80, 8},   {115, 8},   {150, 8},   {180, 8},   {225, 8},   {240, 8},
    {10, 80},  {240, 80}, {10, 156}, {240, 156}, {10, 190}, {25, 190}, {50, 190}, {80, 190}, {115, 190}, {150, 190}, {180, 190}, {225, 190}, {240, 190}
};

typedef struct { uint8_t row; uint8_t col; uint8_t led_index; } logical_pos_t;
const logical_pos_t key_logical_map[KEYLIGHT_COUNT] = {
    {0,0,75},{1,0,76},{0,5,77},{1,5,78},{0,4,79},{1,4,80},{0,3,81},{1,3,82},{0,2,83},{1,2,84},{0,1,85},{1,1,86},{0,6,87},{1,6,88},{0,7,88},
    {2,0,51},{3,0,52},{2,1,53},{3,1,54},{2,2,55},{3,2,56},{2,3,57},{3,3,58},{2,4,59},{3,4,60},{2,5,61},{3,5,62},{2,6,63},{3,6,64},{2,7,65},
    {4,0,49},{5,0,46},{4,1,45},{5,1,44},{4,2,43},{5,2,42},{4,3,41},{5,3,40},{4,4,39},{5,4,38},{4,5,37},{5,5,36},{4,6,35},{4,7,34},
    {7,0,18}, {6,0,18},{6,1,19},{7,1,20},{6,2,21},{7,2,22},{6,3,23},{7,3,24},{7,4,25},{6,4,26},{7,5,27},{6,5,28},{6,6,29},{7,6,30},{6,7,31},
    {8,0,17},  {8,1,15},  {8,2,13},      {8,3,10},      {8,4,7},   {8,5,6},    {8,6,4},{5,6,2},{7,7,0}
};

static void generate_led_positions(void) {
    for(int i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        g_led_config.point[led_physical_map[i]] = led_physical_pos[i];
    }
}

static void generate_matrix_to_led_map(void) {
    for(int i = 0; i < KEYLIGHT_COUNT; i++) {
        logical_pos_t key = key_logical_map[i];
        g_led_config.matrix_co[key.row][key.col] = key.led_index;
    }
}

void keylight_set_brightness(uint16_t led_index, uint8_t brightness) {
    if (led_index >= RGB_MATRIX_LED_COUNT) {
        return;
    }
    g_keylight_brightness[led_index] = brightness;
}

// Scale down any front LED that has a brightness override set. Runs inside the
// rgb_matrix indicators callback, after the active effect has rendered.
static void keylight_apply_overrides(void) {
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        if (g_keylight_brightness[i] == 255) {
            continue; // full brightness, leave the effect's color untouched
        }
        rgb_led_t c = rgb_matrix_ws2812_array[i];
        rgb_matrix_set_color(i,
            scale8(c.r, g_keylight_brightness[i]),
            scale8(c.g, g_keylight_brightness[i]),
            scale8(c.b, g_keylight_brightness[i]));
    }
}

void lighting_init(void) {
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        g_keylight_brightness[i] = 255; // full brightness by default
    }
    generate_led_positions();
    generate_matrix_to_led_map();
    underglow_manager_init();
}

void lighting_task(void) {
    underglow_manager_task();
    keylight_apply_overrides();
}

bool lighting_process_user_command(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        switch (keycode) {
            case UG_ANIM:
                underglow_manager_next_anim();
                return false; // We handled the keycode
        }
    }
    return true; // Keycode not handled, continue processing
}

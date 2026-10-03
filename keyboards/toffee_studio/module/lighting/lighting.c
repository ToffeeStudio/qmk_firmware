#include "quantum.h"
#include "rgb_matrix.h"
#include "rgb_matrix_types.h"   // for led_point_t and led_config_t
#include "animations/manager.h"
#include "lighting.h"
#include "persistence.h"
#include "groups.h"
#include "screenless-map.h"
#include "usb_device_state.h"
#ifdef MODULE_EXTENSIVE_TEST
#include "test_mode.h"
#endif
#include <lib/lib8tion/lib8tion.h>   // for scale8()

// The driver's working color buffer, holding the colors the active effect
// rendered this frame. We read it back to dim individual front LEDs.
extern rgb_led_t rgb_matrix_ws2812_array[RGB_MATRIX_LED_COUNT];

static bool startup_waiting = true;
static bool startup_done;
static uint32_t startup_started;

void notify_usb_device_state_change_kb(enum usb_device_state state) {
    if (state != USB_DEVICE_STATE_CONFIGURED) {
        startup_waiting = true;
        startup_done = false;
    }
    notify_usb_device_state_change_user(state);
}

static void lighting_startup_apply(void) {
    // Run on completed frames, not a blocking sleep during keyboard/USB init.
    if (usb_device_state != USB_DEVICE_STATE_CONFIGURED) {
        rgb_matrix_set_color_all(0, 0, 0);
        return;
    }
    if (startup_done) return;
    if (startup_waiting) {
        startup_waiting = false;
        startup_started = timer_read32();
        uprintf("Lighting: USB ready; 1000ms off, then 1000ms fade\n");
    }
    uint32_t elapsed = timer_elapsed32(startup_started);
    if (elapsed <= MODULE_LIGHTING_STARTUP_DELAY_MS) {
        rgb_matrix_set_color_all(0, 0, 0);
        return;
    }
    elapsed -= MODULE_LIGHTING_STARTUP_DELAY_MS;
    if (elapsed >= MODULE_LIGHTING_STARTUP_FADE_MS) {
        startup_done = true; // Do not restart the fade when the 32-bit timer wraps.
        uprintf("Lighting: startup fade complete\n");
        return;
    }
    // Scale the freshly rendered frame, never the saved/requested settings.
    // All groups, per-key overrides and diagnostic patterns pass through here.
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        rgb_led_t c = rgb_matrix_ws2812_array[i];
        rgb_matrix_set_color(i,
            (uint32_t)c.r * elapsed / MODULE_LIGHTING_STARTUP_FADE_MS,
            (uint32_t)c.g * elapsed / MODULE_LIGHTING_STARTUP_FADE_MS,
            (uint32_t)c.b * elapsed / MODULE_LIGHTING_STARTUP_FADE_MS);
    }
}

// Per-front-LED brightness override (0-255), applied on top of the active
// effect. 255 = unchanged, 0 = off. Lets a single key's LED be dimmed/off.
static uint8_t g_keylight_brightness[RGB_MATRIX_LED_COUNT];

#ifdef MODULE_LED_DISCOVERY
// Red, blue, green, yellow, pink. NO_LED leaves a colour unused.
static uint8_t discovery_leds[LIGHTING_DISCOVERY_LED_COUNT];
static bool discovery_front_check;
static bool discovery_all_check;
static bool discovery_startup_check;
static bool discovery_back_check;
static bool discovery_underglow_active;
static bool discovery_back_cycle;
static uint8_t discovery_back_phase;
static uint32_t discovery_back_started;

// Original typing-view coordinates, with rear indices verified by camera.
// Rear camera view mirrors x; do not mirror the stored animation coordinates.
// Evidence: toffee-accounting/discord_capture/led_discovery/back_map_2026-09-26.json
static const struct { uint8_t index, x, y; } discovery_back_leds[] = {
    {90, 10, 8}, {89, 25, 8}, {88, 50, 8}, {87, 80, 8},
    {86, 115, 8}, {85, 150, 8}, {84, 180, 8}, {82, 225, 8}, {81, 240, 8},
    {49, 10, 80}, {33, 240, 80}, {48, 10, 156}, {32, 240, 156},
    {16, 10, 190}, {14, 25, 190}, {12, 50, 190}, {11, 80, 190},
    {9, 115, 190}, {8, 150, 190}, {5, 180, 190}, {3, 225, 190}, {1, 240, 190},
};
// Verified physical front packages, including the badge and repaired R1C13 (103).
// Evidence: toffee-accounting/discord_capture/led_discovery/front_map_2026-09-25.json
static const uint8_t discovery_front_leds[] = {
    0, 2, 4, 6, 7, 10, 13, 15, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 34, 35, 36, 37, 38, 39, 40, 41,
    42, 43, 44, 45, 46, 47, 50, 51, 52, 53, 54, 55, 56, 57, 58,
    59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
    74, 75, 76, 77, 78, 79, 80, 83, 91, 92, 93, 94, 95, 96, 97,
    98, 99, 100, 101, 102, 103,
};
static const RGB discovery_colors[LIGHTING_DISCOVERY_LED_COUNT] = {
    {.r = MODULE_LED_DISCOVERY_BRIGHTNESS, .g = 0, .b = 0},
    {.r = 0, .g = 0, .b = MODULE_LED_DISCOVERY_BRIGHTNESS},
    {.r = 0, .g = MODULE_LED_DISCOVERY_BRIGHTNESS, .b = 0},
    {.r = MODULE_LED_DISCOVERY_BRIGHTNESS, .g = MODULE_LED_DISCOVERY_BRIGHTNESS, .b = 0},
    {.r = MODULE_LED_DISCOVERY_BRIGHTNESS, .g = MODULE_LED_DISCOVERY_BRIGHTNESS / 2, .b = MODULE_LED_DISCOVERY_BRIGHTNESS * 3 / 4},
};

// Raw HID validates indices once before calling this function.
void lighting_discovery_set_leds(const uint8_t *led_indices) {
    discovery_underglow_active = false;
    discovery_back_check = false;
    discovery_startup_check = false;
    discovery_front_check = false;
    discovery_all_check = false;
    memcpy(discovery_leds, led_indices, sizeof(discovery_leds));
#ifdef MODULE_EXTENSIVE_TEST
    uint8_t groups = 0;
    bool selected = false;
    for (uint8_t i = 0; i < LIGHTING_DISCOVERY_LED_COUNT; i++) {
        if (led_indices[i] == NO_LED) continue;
        selected = true;
        uint8_t flags = g_led_config.flags[led_indices[i]];
        if (flags == LED_FLAG_KEYLIGHT) groups |= 1;
        if (flags == LED_FLAG_UNDERGLOW) groups |= 2;
        if (flags == LED_FLAG_INDICATOR) groups |= 4;
    }
    lighting_test_pattern(selected ? MODULE_TEST_SELECTED : MODULE_TEST_OFF, groups);
#endif
    uprintf("LED discovery: red=%u blue=%u green=%u yellow=%u pink=%u\n",
            discovery_leds[0], discovery_leds[1], discovery_leds[2],
            discovery_leds[3], discovery_leds[4]);
}

uint8_t lighting_discovery_show_front(void) {
    discovery_underglow_active = false;
    discovery_back_check = false;
    discovery_startup_check = false;
    discovery_all_check = false;
    discovery_front_check = true;
#ifdef MODULE_EXTENSIVE_TEST
    lighting_test_pattern(MODULE_TEST_FRONT_WHITE, 5);
#endif
    uprintf("LED discovery: front check, %u LEDs\n", (unsigned)sizeof(discovery_front_leds));
    return sizeof(discovery_front_leds);
}

uint8_t lighting_discovery_show_all(void) {
    discovery_underglow_active = false;
    discovery_back_check = false;
    discovery_startup_check = false;
    discovery_front_check = false;
    discovery_all_check = true;
#ifdef MODULE_EXTENSIVE_TEST
    lighting_test_pattern(MODULE_TEST_ALL_WHITE, 7);
#endif
    uprintf("LED discovery: all %u addresses\n", RGB_MATRIX_LED_COUNT);
    return RGB_MATRIX_LED_COUNT;
}

uint8_t lighting_discovery_show_back(bool cycle, uint8_t phase) {
    discovery_underglow_active = false;
    discovery_startup_check = false;
    discovery_front_check = false;
    discovery_all_check = false;
    discovery_back_check = true;
    discovery_back_cycle = cycle;
    discovery_back_phase = phase;
    discovery_back_started = timer_read32();
#ifdef MODULE_EXTENSIVE_TEST
    lighting_test_pattern(MODULE_TEST_REAR_RAINBOW, 2);
#endif
    uprintf("LED discovery: rear XY rainbow, cycle=%u phase=%u\n", cycle, phase);
    return ARRAY_SIZE(discovery_back_leds);
}

static void lighting_discovery_task(void) {
    if (discovery_all_check) {
        rgb_matrix_set_color_all(MODULE_LED_DISCOVERY_BRIGHTNESS,
                                 MODULE_LED_DISCOVERY_BRIGHTNESS, MODULE_LED_DISCOVERY_BRIGHTNESS);
        return;
    }
    rgb_matrix_set_color_all(0, 0, 0);
    if (discovery_back_check) {
        uint8_t phase = discovery_back_phase;
        if (discovery_back_cycle) {
            phase += timer_elapsed32(discovery_back_started) / 40;
        }
        for (uint8_t i = 0; i < ARRAY_SIZE(discovery_back_leds); i++) {
            uint8_t hue = (discovery_back_leds[i].x + discovery_back_leds[i].y) / 2 + phase;
            RGB color = hsv_to_rgb_nocie((HSV){hue, 255, MODULE_LED_DISCOVERY_BRIGHTNESS});
            rgb_matrix_set_color(discovery_back_leds[i].index, color.r, color.g, color.b);
        }
        return;
    }
    if (discovery_startup_check) {
        for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
            rgb_matrix_set_color(i, MODULE_LED_DISCOVERY_BRIGHTNESS,
                                 MODULE_LED_DISCOVERY_BRIGHTNESS, MODULE_LED_DISCOVERY_BRIGHTNESS);
        }
        return;
    }
    if (discovery_front_check) {
        for (uint8_t i = 0; i < sizeof(discovery_front_leds); i++) {
            rgb_matrix_set_color(discovery_front_leds[i], MODULE_LED_DISCOVERY_BRIGHTNESS,
                                 MODULE_LED_DISCOVERY_BRIGHTNESS, MODULE_LED_DISCOVERY_BRIGHTNESS);
        }
        return;
    }
    for (uint8_t i = 0; i < LIGHTING_DISCOVERY_LED_COUNT; i++) {
        if (discovery_leds[i] == NO_LED) {
            continue;
        }
        RGB color = discovery_colors[i];
        rgb_matrix_set_color(discovery_leds[i], color.r, color.g, color.b);
    }
}
#endif


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
#ifdef MODULE_LED_DISCOVERY
    module_lighting_state.overrides[led_index] = brightness;
    lighting_groups_schedule_save();
    lighting_enable_underglow();
    return;
#endif
    if (led_index >= RGB_MATRIX_LED_COUNT) {
        return;
    }
    g_keylight_brightness[led_index] = brightness;
}

// Scale down any front LED that has a brightness override set. Runs inside the
// rgb_matrix indicators callback, after the active effect has rendered.
static void keylight_apply_overrides(void) {
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        uint8_t brightness = g_keylight_brightness[i];
#ifdef MODULE_LED_DISCOVERY
        if (!lighting_is_front_led(i)) continue;
        brightness = module_lighting_state.overrides[i];
#endif
        if (brightness == 255) {
            continue; // full brightness, leave the effect's color untouched
        }
        rgb_led_t c = rgb_matrix_ws2812_array[i];
        rgb_matrix_set_color(i,
            scale8(c.r, brightness),
            scale8(c.g, brightness),
            scale8(c.b, brightness));
    }
}

void lighting_init(void) {
    startup_waiting = true;
    startup_done = false;
    rgb_matrix_set_color_all(0, 0, 0);
#ifdef MODULE_LED_DISCOVERY
    discovery_back_check = false;
    // Use QMK's normal render/flush cycle, with a black solid base effect.
    rgb_matrix_enable_noeeprom();
    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
    rgb_matrix_sethsv_noeeprom(0, 0, 0);
    discovery_front_check = false;
    discovery_all_check = false;
    discovery_startup_check = false;
    memset(g_led_config.flags, 0, sizeof(g_led_config.flags));
    memset(g_led_config.point, 0, sizeof(g_led_config.point));
    memset(g_led_config.matrix_co, NO_LED, sizeof(g_led_config.matrix_co));
    for (uint8_t i = 0; i < ARRAY_SIZE(module_front_leds); i++) {
        uint8_t index = module_front_leds[i].index;
        g_led_config.flags[index] = LED_FLAG_KEYLIGHT;
        g_led_config.point[index] = (led_point_t){module_front_leds[i].x, module_front_leds[i].y};
    }
    for (uint8_t i = 0; i < ARRAY_SIZE(module_badge_leds); i++)
        g_led_config.flags[module_badge_leds[i]] = LED_FLAG_INDICATOR;
    for (uint8_t i = 0; i < ARRAY_SIZE(module_key_map); i++)
        g_led_config.matrix_co[module_key_map[i].row][module_key_map[i].col] = module_key_map[i].index;
    for (uint8_t i = 0; i < ARRAY_SIZE(discovery_back_leds); i++) {
        uint8_t index = discovery_back_leds[i].index;
        g_led_config.flags[index] = LED_FLAG_UNDERGLOW;
        g_led_config.point[index] = (led_point_t){discovery_back_leds[i].x, discovery_back_leds[i].y};
    }
    underglow_manager_init();
    underglow_manager_set_brightness(MODULE_LED_DISCOVERY_BRIGHTNESS);
    load_underglow_config();
    lighting_groups_init();
    lighting_enable_underglow();
#ifdef MODULE_EXTENSIVE_TEST
    lighting_test_init();
#endif
    uprintf("Screenless lighting: %u verified rear LEDs ready\n", (unsigned)ARRAY_SIZE(discovery_back_leds));
    return;
#endif
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        g_keylight_brightness[i] = 255; // full brightness by default
    }
    generate_led_positions();
    generate_matrix_to_led_map();
    underglow_manager_init();
}

void lighting_enable_underglow(void) {
#ifdef MODULE_LED_DISCOVERY
    discovery_underglow_active = true;
#ifdef MODULE_EXTENSIVE_TEST
    lighting_test_pattern(MODULE_TEST_GROUPS, 0);
#endif
#endif
}

void lighting_task(void) {
    underglow_persistence_task();
    lighting_groups_persistence_task();
#ifdef MODULE_LED_DISCOVERY
    if (discovery_underglow_active) {
        lighting_groups_task();
        underglow_manager_task();
        keylight_apply_overrides();
    } else {
        lighting_discovery_task();
    }
#ifdef MODULE_EXTENSIVE_TEST
    lighting_test_apply_limits();
#endif
    lighting_startup_apply();
    return;
#endif
    underglow_manager_task();
    keylight_apply_overrides();
    lighting_startup_apply();
}

bool lighting_process_user_command(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        switch (keycode) {
            case UG_ANIM:
                underglow_manager_next_anim();
                lighting_enable_underglow();
                return false; // We handled the keycode
        }
    }
    return true; // Keycode not handled, continue processing
}

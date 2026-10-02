#include "test_mode.h"
#include "groups.h"
#include "animations/manager.h"

#ifndef MODULE_TEST_BUILD_ID
#error "An explicit MODULE_TEST_BUILD_ID is required for temporary test firmware"
#endif

extern rgb_led_t rgb_matrix_ws2812_array[RGB_MATRIX_LED_COUNT];
static bool blank, expired;
static uint8_t pattern, commanded_groups;
static uint32_t last_keepalive;

static void expire_deadline(void) {
    if (!blank && timer_elapsed32(last_keepalive) >= MODULE_TEST_TIMEOUT_MS) {
        blank = true;
        expired = true;
        uprintf("TEST: controller timeout; LEDs latched off\n");
    }
}

void lighting_test_init(void) {
    blank = true;
    expired = false;
    pattern = MODULE_TEST_OFF;
    commanded_groups = 0;
    last_keepalive = timer_read32();
    rgb_matrix_sethsv_noeeprom(0, 0, 0);
    rgb_matrix_set_color_all(0, 0, 0);
    uprintf("TEST: startup off, channel cap 18, keepalive timeout 5000ms; lighting saves disabled\n");
}
void lighting_test_pattern(uint8_t value, uint8_t groups) {
    pattern = value;
    commanded_groups = groups;
    last_keepalive = timer_read32();
    blank = value == MODULE_TEST_OFF;
    expired = false;
}
void lighting_test_keepalive(void) {
    // Check the old deadline before refreshing it, including between frames.
    expire_deadline();
    last_keepalive = timer_read32();
    // A late keepalive does not resume a timed-out pattern; a new setter must.
}
void lighting_test_apply_limits(void) {
    expire_deadline();
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        RGB c = rgb_matrix_ws2812_array[i];
        if (blank) c = (RGB){0, 0, 0};
        else {
            if (c.r > MODULE_TEST_CHANNEL_CAP) c.r = MODULE_TEST_CHANNEL_CAP;
            if (c.g > MODULE_TEST_CHANNEL_CAP) c.g = MODULE_TEST_CHANNEL_CAP;
            if (c.b > MODULE_TEST_CHANNEL_CAP) c.b = MODULE_TEST_CHANNEL_CAP;
        }
        rgb_matrix_set_color(i, c.r, c.g, c.b);
    }
}
void lighting_test_status(uint8_t *payload) {
    uint32_t uptime = timer_read32(), age = timer_elapsed32(last_keepalive);
    uint32_t build = MODULE_TEST_BUILD_ID;
    payload[0] = 1;
    payload[1] = MODULE_TEST_CHANNEL_CAP;
    payload[2] = !blank;
    payload[3] = expired;
    memcpy(payload + 4, &uptime, 4);
    memcpy(payload + 8, &build, 4);
    payload[12] = pattern;
    payload[13] = module_lighting_state.front.effect;
    payload[14] = module_lighting_state.front.brightness;
    payload[15] = g_underglow_config.current_animation_id;
    payload[16] = g_underglow_config.brightness;
    payload[17] = module_lighting_state.badge.enabled;
    payload[18] = module_lighting_state.badge.brightness;
    uint8_t groups = commanded_groups;
    if (pattern == MODULE_TEST_GROUPS) {
        groups = (module_lighting_state.front.effect && module_lighting_state.front.brightness ? 1 : 0) |
                 (g_underglow_config.brightness ? 2 : 0) |
                 (module_lighting_state.badge.enabled && module_lighting_state.badge.brightness ? 4 : 0);
    }
    payload[19] = blank ? 0 : groups;
    memcpy(payload + 20, &age, 4);
}

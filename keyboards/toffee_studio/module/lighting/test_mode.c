#include "test_mode.h"
#include "groups.h"
#include "lighting.h"
#include "animations/manager.h"

#ifndef MODULE_TEST_BUILD_ID
#error "An explicit MODULE_TEST_BUILD_ID is required for temporary test firmware"
#endif

extern rgb_led_t rgb_matrix_ws2812_array[RGB_MATRIX_LED_COUNT];
static bool blank, expired;
static uint8_t pattern, commanded_groups;
static uint32_t last_keepalive;
#ifdef MODULE_POWER_CALIBRATION
static RGB raw_color;
static uint8_t raw_mask[13];

bool lighting_test_raw(const uint8_t *payload) {
    unsigned count = 0;
    uint8_t groups = 0;
    for (unsigned i = 0; i < MODULE_MAPPED_LED_COUNT; i++) {
        if (!(payload[3 + i / 8] & (1u << (i % 8)))) continue;
        count++;
        uint8_t flags = g_led_config.flags[i];
        if (flags == LED_FLAG_KEYLIGHT) groups |= 1;
        if (flags == LED_FLAG_UNDERGLOW) groups |= 2;
        if (flags == LED_FLAG_INDICATOR) groups |= 4;
    }
    // User's experiment endpoints: up to 50 packages at 255, otherwise <=150.
    // These are requests, still subject to the shared current-model limiter.
    if (!count || (count > 50 && (payload[0] > 150 || payload[1] > 150 || payload[2] > 150))) return false;
    raw_color = (RGB){payload[0], payload[1], payload[2]};
    memcpy(raw_mask, payload + 3, sizeof(raw_mask));
    lighting_test_pattern(MODULE_TEST_RAW, groups);
    return true;
}
#endif

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
    uprintf("TEST: startup off, channel cap %u, keepalive timeout 5000ms; lighting saves disabled\n", MODULE_TEST_CHANNEL_CAP);
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
#ifdef MODULE_POWER_CALIBRATION
        if (pattern == MODULE_TEST_RAW)
            c = i < MODULE_MAPPED_LED_COUNT && (raw_mask[i / 8] & (1u << (i % 8))) ? raw_color : (RGB){0, 0, 0};
#endif
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

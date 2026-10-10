#include "test_mode.h"
#include "groups.h"
#include "lighting.h"
#include "power.h"
#include "animations/manager.h"

#ifndef MODULE_TEST_BUILD_ID
#error "An explicit MODULE_TEST_BUILD_ID is required for temporary test firmware"
#endif

extern rgb_led_t rgb_matrix_ws2812_array[RGB_MATRIX_LED_COUNT];
static bool blank, expired;
static uint8_t pattern, commanded_groups;
static uint32_t last_keepalive;
#ifdef MODULE_MANUAL_MEASUREMENT
static RGB mixed_colors[3];
void lighting_test_mixed(const uint8_t *payload) {
    for (unsigned group = 0; group < 3; group++)
        mixed_colors[group] = (RGB){.r = payload[group * 3], .g = payload[group * 3 + 1], .b = payload[group * 3 + 2]};
    lighting_test_pattern(MODULE_TEST_MIXED, 7);
}
#endif
#ifdef MODULE_POWER_CALIBRATION
static RGB raw_color;
static uint8_t raw_mask[13];

bool lighting_test_raw(const uint8_t *payload) {
    unsigned count = 0;
    uint8_t groups = 0;
#ifndef MODULE_MANUAL_MEASUREMENT
    uint64_t weighted_request = 0;
#endif
    for (unsigned i = 0; i < MODULE_MAPPED_LED_COUNT; i++) {
        if (!(payload[3 + i / 8] & (1u << (i % 8)))) continue;
        count++;
        uint8_t flags = g_led_config.flags[i];
        if (flags == LED_FLAG_KEYLIGHT) groups |= 1;
        if (flags == LED_FLAG_UNDERGLOW) groups |= 2;
        if (flags == LED_FLAG_INDICATOR) groups |= 4;
#ifndef MODULE_MANUAL_MEASUREMENT
        unsigned group = flags == LED_FLAG_KEYLIGHT ? 0 : flags == LED_FLAG_UNDERGLOW ? 1 : 2;
        for (unsigned channel = 0; channel < 3; channel++)
            weighted_request += (uint64_t)payload[channel] * module_power_profile.channel_ua[group][channel];
#endif
    }
    // The earlier automated calibration retains population/current admission.
    // Manual measurement accepts any nonempty mapped selection at full byte range.
    if (!count) return false;
#ifndef MODULE_MANUAL_MEASUREMENT
    if (count > 50 && (payload[0] > 150 || payload[1] > 150 || payload[2] > 150)) return false;
    if (weighted_request > (uint64_t)module_power_led_budget(&module_power_profile) * 1000 * 255) return false;
#endif
    raw_color = (RGB){.r = payload[0], .g = payload[1], .b = payload[2]};
    memcpy(raw_mask, payload + 3, sizeof(raw_mask));
    lighting_test_pattern(MODULE_TEST_RAW, groups);
    return true;
}
#endif

static void expire_deadline(void) {
#ifndef MODULE_MANUAL_MEASUREMENT
    if (!blank && timer_elapsed32(last_keepalive) >= MODULE_TEST_TIMEOUT_MS) {
        blank = true;
        expired = true;
        uprintf("TEST: controller timeout; LEDs latched off\n");
    }
#endif
}

void lighting_test_init(void) {
    blank = true;
    expired = false;
    pattern = MODULE_TEST_OFF;
    commanded_groups = 0;
    last_keepalive = timer_read32();
#ifdef MODULE_MANUAL_MEASUREMENT
    // Do not restore saved high groups when the first ordinary UI setter arrives.
    // Colours/effects/overrides remain in RAM; no storage writes here.
    module_lighting_state.front.brightness = 0;
    module_lighting_state.badge.brightness = 0;
    g_underglow_config.brightness = 0;
#endif
    rgb_matrix_sethsv_noeeprom(0, 0, 0);
    rgb_matrix_set_color_all(0, 0, 0);
#ifdef MODULE_MANUAL_MEASUREMENT
    uprintf("MANUAL TEST: no current/brightness limiter, no timeout; startup off, lighting saves disabled\n");
#else
    uprintf("TEST: startup off, channel cap %u, keepalive timeout 5000ms; lighting saves disabled\n", MODULE_TEST_CHANNEL_CAP);
#endif
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
#ifdef MODULE_MANUAL_MEASUREMENT
        if (pattern == MODULE_TEST_MIXED) {
            uint8_t flags = g_led_config.flags[i];
            c = (RGB){0, 0, 0};
            if (i < MODULE_MAPPED_LED_COUNT) {
                if (flags == LED_FLAG_KEYLIGHT) c = mixed_colors[0];
                else if (flags == LED_FLAG_UNDERGLOW) c = mixed_colors[1];
                else if (flags == LED_FLAG_INDICATOR) c = mixed_colors[2];
            }
        }
#endif
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

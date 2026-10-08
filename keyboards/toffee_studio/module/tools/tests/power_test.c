// No USB. Production limiter + custom driver, synthetic profiles and TX/time adapters.
#include <assert.h>
#include "../../lighting/power.h"
#include "../../lighting/screenless-map.h"
#include "usb_device_state.h"

extern rgb_led_t rgb_matrix_ws2812_array[RGB_MATRIX_LED_COUNT];
led_config_t g_led_config;
enum usb_device_state usb_device_state = USB_DEVICE_STATE_CONFIGURED;
static uint32_t now;
static uint16_t startup = 1000;
static rgb_led_t wire[RGB_MATRIX_LED_COUNT];
static unsigned transmissions;
#ifdef MODULE_EXTENSIVE_TEST
// This suite isolates the final power boundary; real watchdog/cap behavior is
// covered with the production test_mode.c in groups_test/test_mode_test.
void lighting_test_apply_limits(void) {}
#endif

// Explicitly synthetic, never a board calibration or deployable profile.
#ifdef POWER_TEST_PROFILE
const module_power_profile_t module_power_profile = {
    .revision = 42, .configured = true, .board_limit_ma = 350, .base_ma = 90, .margin_ma = 35,
    .channel_ua = {{13000, 14000, 15000}, {11000, 12000, 13000}, {9000, 10000, 11000}},
    .channel_ceiling = {160, 255, 255},
};
#endif

uint32_t timer_read32(void) {return now;}
uint32_t timer_elapsed32(uint32_t previous) {return now - previous;}
uint16_t lighting_startup_scale(void) {return usb_device_state == USB_DEVICE_STATE_CONFIGURED ? startup : 0;}
void ws2812_setleds(rgb_led_t *leds, uint16_t count) {
    assert(count == RGB_MATRIX_LED_COUNT);
    memcpy(wire, leds, sizeof(wire));
    transmissions++;
}

static int group(unsigned i) {
    if (i >= 104) return -1;
    switch (g_led_config.flags[i]) {
        case LED_FLAG_KEYLIGHT: return 0;
        case LED_FLAG_UNDERGLOW: return 1;
        case LED_FLAG_INDICATOR: return 2;
        default: return -1;
    }
}
static uint64_t demand(const rgb_led_t *frame, const module_power_profile_t *profile) {
    uint64_t total = 0;
    for (unsigned i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        int g = group(i);
        if (g < 0) continue;
        total += (uint64_t)frame[i].r * profile->channel_ua[g][0];
        total += (uint64_t)frame[i].g * profile->channel_ua[g][1];
        total += (uint64_t)frame[i].b * profile->channel_ua[g][2];
    }
    return total;
}
static uint16_t u16(const uint8_t *p) {return p[0] | (uint16_t)p[1] << 8;}
static uint32_t rng = 0x26100401;
static uint32_t random32(void) {rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng;}
#ifdef MODULE_MANUAL_MEASUREMENT
#include "manual_power_test.c"
#endif

int main(void) {
    for (unsigned i = 0; i < 104; i++) g_led_config.flags[i] = LED_FLAG_UNDERGLOW;
    for (unsigned i = 0; i < ARRAY_SIZE(module_front_leds); i++) g_led_config.flags[module_front_leds[i].index] = LED_FLAG_KEYLIGHT;
    for (unsigned i = 0; i < ARRAY_SIZE(module_badge_leds); i++) g_led_config.flags[module_badge_leds[i]] = LED_FLAG_INDICATOR;
    assert(group(103) == 0); // Broken physical repair cannot reduce the accounting map.
#ifdef MODULE_MANUAL_MEASUREMENT
    test_manual_power();
    return 0;
#endif
    rgb_led_t input[201], original[201], output[201], repeated[201];
    module_power_frame_t result, repeat;
#if !defined(MODULE_POWER_CALIBRATION) && !defined(POWER_TEST_PROFILE)
    // Fitted normal candidate for PCB-B; 900 mA includes measured idle current.
    assert(module_power_profile.revision == 5 && module_power_profile.configured);
    assert(module_power_profile.board_limit_ma == 900);
    assert(module_power_profile.base_ma == 110 && module_power_profile.margin_ma == 0);
    module_power_profile_t candidate = module_power_profile;
    assert(module_power_led_budget(&candidate) == 790);
    // These unscaled predictions reproduce the recorded model-fit.json values.
    // Requested demand remains visible even when final output must be limited.
    const struct {int group; uint8_t r, g, b; uint16_t estimate_ma;} samples[] = {
        {0, 26, 0, 0, 185},    // PCB-B front red: measured 220 mA.
        {0, 255, 0, 0, 843},   // PCB-B front red: measured 830 mA.
        {0, 9, 9, 9, 188},     // PCB-B front white: measured 200 mA.
        {1, 220, 220, 220, 816}, // PCB-B rear white: measured 800 mA.
        {-1, 110, 55, 0, 883}, // PCB-A all orange: 853 mA with A's 80 mA base.
    };
    for (unsigned s = 0; s < ARRAY_SIZE(samples); s++) {
        memset(input, 0, sizeof(input));
        for (unsigned i = 0; i < 104; i++)
            if (samples[s].group == -1 || group(i) == samples[s].group)
                input[i] = (RGB){.r=samples[s].r, .g=samples[s].g, .b=samples[s].b};
        module_power_limit(&candidate, input, g_led_config.flags, 1000, output, &result);
        assert(candidate.base_ma + (result.requested_ua + 999) / 1000 == samples[s].estimate_ma);
        assert(memcmp(input, output, sizeof(input)) == 0 && !result.limited);
    }
    // Exact integer boundary with a synthetic 100 uA per byte: 789.9/790/790.1
    // mA LED demand. Below/at the cap passes unchanged; one byte over scales.
    candidate.channel_ua[0][0] = 25500;
    memset(input, 0, sizeof(input));
    for (unsigned i = 0; i < 7899; i++) input[module_front_leds[i / 255].index].r++;
    for (unsigned step = 0; step < 3; step++) {
        if (step) input[module_front_leds[31].index].r++;
        module_power_limit(&candidate, input, g_led_config.flags, 1000, output, &result);
        assert(result.limited == (step == 2));
        assert(candidate.base_ma + (result.effective_ua + 999) / 1000 <= 900);
        if (step < 2) assert(memcmp(input, output, sizeof(input)) == 0);
    }
    candidate = module_power_profile;
    for (unsigned level = 0; level <= 255; level++) {
        memset(input, 0, sizeof(input));
        for (unsigned i = 0; i < 104; i++) input[i] = (RGB){.r=level, .g=level, .b=level};
        memcpy(original, input, sizeof(input));
        module_power_limit(&candidate, input, g_led_config.flags, 1000, output, &result);
        assert(candidate.base_ma + (result.effective_ua + 999) / 1000 <= 900);
        assert(result.effective_ua == (demand(output, &candidate) + 254) / 255);
        assert(memcmp(input, original, sizeof(input)) == 0);
        if (level == 255) assert(result.limited);
    }
#endif
#ifdef MODULE_POWER_CALIBRATION
    assert(module_power_profile.revision == 0x8005);
    assert(module_power_profile.base_ma == 80 && module_power_profile.margin_ma == 100);
    assert(module_power_profile.board_limit_ma == 1100 && module_power_led_budget(&module_power_profile) == 920);
    // Full RGB sparse requests 1/2/3 pass unchanged; larger requests are scaled
    // at the same final boundary, never treated as successfully delivered 255.
    for (unsigned count = 1; count <= 104; count++) {
        memset(input, 0, sizeof(input));
        for (unsigned i = 0; i < count; i++) input[i] = (RGB){255, 255, 255};
        module_power_limit(&module_power_profile, input, g_led_config.flags, 1000, output, &result);
        assert(result.requested_ua == (demand(input, &module_power_profile) + 254) / 255);
        assert(result.effective_ua <= 920000);
        assert(result.limited == (result.requested_ua > 920000));
        if (!result.limited) assert(memcmp(input, output, sizeof(input)) == 0);
    }
#endif
    module_power_profile_t profile = {
        .configured = true, .board_limit_ma = 500, .base_ma = 100, .margin_ma = 50,
        .channel_ua = {{12000, 15000, 18000}, {10000, 9000, 8000}, {20000, 19000, 18000}},
        .channel_ceiling = {160, 255, 255},
    };
    memset(input, 0, sizeof(input));
    module_power_limit(&profile, input, g_led_config.flags, 1000, output, &result);
    assert(!result.requested_ua && !result.effective_ua && !result.limited);
    assert(result.limit_scale == UINT16_MAX);
    input[103] = (RGB){.r=100, .g=50, .b=25};
    module_power_limit(&profile, input, g_led_config.flags, 1000, output, &result);
    assert(memcmp(input, output, sizeof(input)) == 0 && !result.limited);
    assert(result.requested_ua == (100 * 12000 + 50 * 15000 + 25 * 18000 + 254) / 255);
    // RGB coefficients are separate; equal numeric red and blue need not draw alike.
    input[103] = (RGB){.b=100};
    module_power_limit(&profile, input, g_led_config.flags, 1000, output, &result);
    assert(result.requested_ua == (100 * 18000 + 254) / 255);
    // Even a falsely flagged padding address stays black and is not counted.
    g_led_config.flags[200] = LED_FLAG_KEYLIGHT;
    input[200] = (RGB){255, 255, 255};
    module_power_limit(&profile, input, g_led_config.flags, 1000, output, &result);
    assert(!output[200].r && result.requested_ua == (100 * 18000 + 254) / 255);
    g_led_config.flags[200] = 0;
    // 20,000 frames: channel extremes, mixed colours, huge coefficients, tiny
    // headroom, zero budget, unknown profile, every startup phase and integer bounds.
    for (unsigned trial = 0; trial < 20000; trial++) {
        for (unsigned g = 0; g < 3; g++) {
            for (unsigned c = 0; c < 3; c++) profile.channel_ua[g][c] = 1 + random32() % 65535;
            profile.channel_ceiling[g] = random32() % 256;
        }
        profile.board_limit_ma = random32() % 2000;
        profile.base_ma = random32() % 200;
        profile.margin_ma = random32() % 200;
        profile.configured = trial % 7 != 0;
        uint16_t fade = trial % 1001;
        for (unsigned i = 0; i < 201; i++) input[i] = (RGB){random32(), random32(), random32()};
        memcpy(original, input, sizeof(input));
        module_power_limit(&profile, input, g_led_config.flags, fade, output, &result);
        module_power_limit(&profile, input, g_led_config.flags, fade, repeated, &repeat);
        assert(memcmp(input, original, sizeof(input)) == 0);
        assert(memcmp(output, repeated, sizeof(output)) == 0);
        uint64_t measured = demand(output, &profile);
        uint64_t requested = demand(input, &profile);
        assert(measured <= (uint64_t)module_power_led_budget(&profile) * 1000 * 255);
        assert(result.effective_ua == (measured + 254) / 255);
        assert(result.requested_ua == (requested + 254) / 255);
        for (unsigned i = 0; i < 201; i++) {
            int g = group(i);
            RGB o = output[i], c = input[i];
            if (g < 0) {assert(!o.r && !o.g && !o.b); continue;}
            uint64_t numerator = (uint64_t)result.limit_scale * fade;
            assert(o.r == c.r * numerator / (65535U * 1000U));
            assert(o.g == c.g * numerator / (65535U * 1000U));
            assert(o.b == c.b * numerator / (65535U * 1000U));
            assert(o.r <= profile.channel_ceiling[g] && o.g <= profile.channel_ceiling[g] && o.b <= profile.channel_ceiling[g]);
        }
    }
    // The REAL driver is the only TX adapter: write directly after indicators,
    // flush repeatedly, inspect transmitted pixels and read-only telemetry.
    uint8_t status[27]; memset(status, 0xCC, sizeof(status));
    lighting_power_status(status + 1);
    assert(status[0] == 0xCC && status[26] == 0xCC && status[1] == 1);
    assert(!(status[2] & 16));
    rgb_matrix_driver.init();
    rgb_matrix_driver.set_color_all(255, 255, 255);
    rgb_matrix_driver.set_color(103, 245, 210, 90);
    memcpy(original, rgb_matrix_ws2812_array, sizeof(original));
    for (unsigned elapsed = 0; elapsed <= 1000; elapsed++) {
        now = 1000 + elapsed;
        startup = elapsed;
        rgb_matrix_driver.flush();
        assert(memcmp(original, rgb_matrix_ws2812_array, sizeof(original)) == 0);
        assert(demand(wire, &module_power_profile) <= (uint64_t)module_power_led_budget(&module_power_profile) * 255 * 1000);
        lighting_power_status(status + 1);
        assert(status[0] == 0xCC && status[26] == 0xCC && (status[2] & 16));
        assert(u16(status + 5) == elapsed + 1);
        assert(u16(status + 17) == (demand(wire, &module_power_profile) + 254999) / 255000);
        assert(u16(status + 21) == elapsed);
        assert(u16(status + 23) == module_power_led_budget(&module_power_profile));
#ifdef MODULE_EXTENSIVE_TEST
        uint8_t sums[27]; memset(sums, 0xCC, sizeof(sums));
        lighting_power_rgb_sums(sums + 1);
        assert(sums[0] == 0xCC && sums[26] == 0xCC && sums[1] == 1 && sums[2] == 1 && sums[25] == 0);
        assert(u16(sums + 3) == elapsed + 1 && u16(sums + 5) == 0);
        uint16_t expected[3][3] = {{0}};
        for (unsigned i = 0; i < 201; i++) {
            int g = group(i); if (g < 0) continue;
            expected[g][0] += wire[i].r; expected[g][1] += wire[i].g; expected[g][2] += wire[i].b;
        }
        for (unsigned g = 0; g < 3; g++) for (unsigned c = 0; c < 3; c++)
            assert(u16(sums + 7 + (g * 3 + c) * 2) == expected[g][c]);
#endif
        for (unsigned i = 104; i < 201; i++) assert(!wire[i].r && !wire[i].g && !wire[i].b);
    }
    assert(transmissions == 1001);
    assert(wire[103].r > 0 && (status[2] & 1));
    unsigned before = transmissions;
    now += 100000;
    lighting_power_status(status + 1);
    assert(u16(status + 7) == UINT16_MAX && transmissions == before);
    usb_device_state = USB_DEVICE_STATE_SUSPEND;
    rgb_matrix_driver.flush();
    for (unsigned i = 0; i < 201; i++) assert(!wire[i].r && !wire[i].g && !wire[i].b);
    puts("Power PASS: 20,000 bounded frames, immutable render buffer, colour coefficients, package limits, 103/padding, final TX, 1001 fade phases, read-only telemetry; no hardware.");
}

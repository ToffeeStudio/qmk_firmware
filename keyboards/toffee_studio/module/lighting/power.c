#include "power.h"
#include "lighting.h"
#include "groups.h"
#include "usb_device_state.h"

static module_power_frame_t last_frame;
static uint16_t sequence;
static uint32_t captured_at;
static bool frame_valid;
#ifdef MODULE_EXTENSIVE_TEST
static uint16_t transmitted_sums[3][3];
#endif

static int group_for(uint16_t index, uint8_t flags) {
    if (index >= MODULE_MAPPED_LED_COUNT) return -1;
    if (flags == LED_FLAG_KEYLIGHT) return 0;
    if (flags == LED_FLAG_UNDERGLOW) return 1;
    if (flags == LED_FLAG_INDICATOR) return 2;
    return -1;
}

static uint64_t weighted(rgb_led_t c, const uint16_t *coefficients) {
    return (uint64_t)c.r * coefficients[0] + (uint64_t)c.g * coefficients[1] +
           (uint64_t)c.b * coefficients[2];
}

static rgb_led_t rescale(rgb_led_t c, int group, const module_power_profile_t *profile) {
#ifndef MODULE_MANUAL_MEASUREMENT
    if (group == 0 || group == 2) {
        c.r = (uint16_t)c.r * profile->front_badge_max / 255;
        c.g = (uint16_t)c.g * profile->front_badge_max / 255;
        c.b = (uint16_t)c.b * profile->front_badge_max / 255;
    }
#else
    (void)group; (void)profile;
#endif
    return c;
}

uint16_t module_power_led_budget(const module_power_profile_t *profile) {
    uint32_t reserve = (uint32_t)profile->base_ma + profile->margin_ma;
    if (!profile->configured || profile->board_limit_ma <= reserve) return 0;
    return profile->board_limit_ma - reserve;
}

void module_power_limit(const module_power_profile_t *profile, const rgb_led_t *input,
                        const uint8_t *flags, uint16_t startup_permille,
                        rgb_led_t *output, module_power_frame_t *result) {
    uint64_t requested = 0, capped = 0, effective = 0;
    uint32_t scale = UINT16_MAX;
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        int group = group_for(i, flags[i]);
        if (group < 0) continue;
        rgb_led_t c = input[i];
        requested += weighted(c, profile->channel_ua[group]);
        c = rescale(c, group, profile);
        capped += weighted(c, profile->channel_ua[group]);
#ifndef MODULE_MANUAL_MEASUREMENT
        uint8_t maximum = c.r > c.g ? c.r : c.g;
        if (c.b > maximum) maximum = c.b;
        if (maximum > profile->channel_ceiling[group]) {
            uint32_t package_scale = (uint32_t)profile->channel_ceiling[group] * UINT16_MAX / maximum;
            if (package_scale < scale) scale = package_scale;
        }
#endif
    }
#ifndef MODULE_MANUAL_MEASUREMENT
    uint64_t available = (uint64_t)module_power_led_budget(profile) * 1000 * 255;
    if (!profile->configured || !available) scale = 0;
    else if (capped > available) {
        uint32_t budget_scale = available * UINT16_MAX / capped;
        if (budget_scale < scale) scale = budget_scale;
    }
#else
    (void)capped;
#endif
    // Rescale front/badge first, then one common current/startup multiplier.
    // Round drive down, current estimates up. The raw
    // render buffer is immutable here, so repeated flushes never compound dimming.
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        int group = group_for(i, flags[i]);
        rgb_led_t c = {0};
        if (group >= 0) {
            c = rescale(input[i], group, profile);
            const uint64_t numerator = (uint64_t)scale * startup_permille;
            const uint32_t denominator = (uint32_t)UINT16_MAX * 1000;
            c.r = c.r * numerator / denominator;
            c.g = c.g * numerator / denominator;
            c.b = c.b * numerator / denominator;
            effective += weighted(c, profile->channel_ua[group]);
        }
        output[i] = c;
    }
    *result = (module_power_frame_t){
        .requested_ua = (requested + 254) / 255,
        .effective_ua = (effective + 254) / 255,
        .limit_scale = scale,
        .startup_permille = startup_permille,
        .limited = requested && scale < UINT16_MAX,
    };
}

void lighting_power_prepare(const rgb_led_t *input, rgb_led_t *output) {
    module_power_limit(&module_power_profile, input, g_led_config.flags,
                       lighting_startup_scale(), output, &last_frame);
#ifdef MODULE_EXTENSIVE_TEST
    memset(transmitted_sums, 0, sizeof(transmitted_sums));
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        int group = group_for(i, g_led_config.flags[i]);
        if (group < 0) continue;
        transmitted_sums[group][0] += output[i].r;
        transmitted_sums[group][1] += output[i].g;
        transmitted_sums[group][2] += output[i].b;
    }
#endif
    sequence++;
    captured_at = timer_read32();
    frame_valid = true;
}

static void put16(uint8_t *data, uint16_t value) {
    data[0] = value;
    data[1] = value >> 8;
}

#ifdef MODULE_EXTENSIVE_TEST
void lighting_power_rgb_sums(uint8_t *payload) {
    uint32_t age = timer_elapsed32(captured_at);
    memset(payload, 0, 25);
    payload[0] = 1;
    payload[1] = frame_valid;
    put16(payload + 2, sequence);
    put16(payload + 4, age > UINT16_MAX ? UINT16_MAX : age);
    for (unsigned group = 0; group < 3; group++)
        for (unsigned color = 0; color < 3; color++)
            put16(payload + 6 + (group * 3 + color) * 2, transmitted_sums[group][color]);
}
#endif

void lighting_power_status(uint8_t *payload) {
    uint32_t age = timer_elapsed32(captured_at);
    memset(payload, 0, 25);
    payload[0] = 1;
#ifndef MODULE_EXTENSIVE_TEST
    payload[0] = 2; // v2: unchanged fields, byte24 = front/badge rescaling maximum.
    payload[24] = module_power_profile.front_badge_max;
#endif
    payload[1] = (module_power_profile.configured ? 1 : 0) |
                 (last_frame.limited ? 2 : 0) |
                 (usb_device_state == USB_DEVICE_STATE_CONFIGURED ? 4 : 0) |
                 (last_frame.startup_permille < 1000 ? 8 : 0) |
                 (frame_valid ? 16 : 0);
#ifdef MODULE_MANUAL_MEASUREMENT
    payload[1] |= 32; // Manual measurement: reference budget ONLY, no limiter/timeout.
#endif
    put16(payload + 2, module_power_profile.revision);
    put16(payload + 4, sequence);
    put16(payload + 6, age > UINT16_MAX ? UINT16_MAX : age);
    put16(payload + 8, module_power_profile.board_limit_ma);
    put16(payload + 10, module_power_profile.base_ma);
    put16(payload + 12, module_power_profile.margin_ma);
    put16(payload + 14, (last_frame.requested_ua + 999) / 1000);
    put16(payload + 16, (last_frame.effective_ua + 999) / 1000);
    put16(payload + 18, (uint32_t)last_frame.limit_scale * 1000 / UINT16_MAX);
    put16(payload + 20, last_frame.startup_permille);
    put16(payload + 22, module_power_led_budget(&module_power_profile));
}

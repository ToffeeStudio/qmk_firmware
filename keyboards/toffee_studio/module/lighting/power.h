#pragma once
#include "rgb_matrix.h"

#if defined(MODULE_MANUAL_MEASUREMENT) && (!defined(MODULE_EXTENSIVE_TEST) || !defined(MODULE_POWER_CALIBRATION))
#error "Manual measurement requires explicit temporary calibration firmware"
#endif

// Source-controlled profile, never writable from a lighting slider/HID command.
// Group order: regular front, rear, badge. Coefficients are incremental uA at 255.
typedef struct {
    uint16_t revision, board_limit_ma, base_ma, margin_ma;
    uint16_t channel_ua[3][3];
    uint8_t channel_ceiling[3];
    uint8_t front_badge_max; // Full-range rescaling: RGB * max /255; rear unchanged.
    bool configured;
} module_power_profile_t;

typedef struct {
    uint32_t requested_ua, effective_ua;
    uint16_t limit_scale, startup_permille;
    bool limited;
} module_power_frame_t;

extern const module_power_profile_t module_power_profile;
// Required, distinct input/output buffers each contain RGB_MATRIX_LED_COUNT LEDs.
// flags is the verified map. Unmapped transport slots never reach the LED chain.
void module_power_limit(const module_power_profile_t *profile, const rgb_led_t *input,
                        const uint8_t *flags, uint16_t startup_permille,
                        rgb_led_t *output, module_power_frame_t *result);
uint16_t module_power_led_budget(const module_power_profile_t *profile);
void lighting_power_prepare(const rgb_led_t *input, rgb_led_t *output);
void lighting_power_status(uint8_t *payload); // 25 bytes; normal v2 adds front/badge max at byte24.
#ifdef MODULE_EXTENSIVE_TEST
void lighting_power_rgb_sums(uint8_t *payload); // Test-only final-output sums, 25 bytes.
#endif

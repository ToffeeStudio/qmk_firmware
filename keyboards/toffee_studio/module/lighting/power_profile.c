#include "power.h"

#ifdef MODULE_POWER_CALIBRATION
#ifndef MODULE_EXTENSIVE_TEST
#error "Calibration profile requires startup-off, watchdog, RAM-only test firmware"
#endif
// Measurement fixture only, NOT fitted/calibrated product coefficients.
// Start below the currently recorded 500 mA USB allocation. The 450 mA test
// ceiling is separate from the user's provisional 900 mA product budget.
// Allow sparse full-drive measurements; final limiting/watchdog still apply.
const module_power_profile_t module_power_profile = {
    .revision = 0x8002,
    .configured = true,
    .board_limit_ma = 450,
    .base_ma = 120,
    .margin_ma = 100,
    .channel_ua = {{20000, 20000, 20000}, {20000, 20000, 20000}, {20000, 20000, 20000}},
    .channel_ceiling = {255, 255, 255},
};
#else
// Ethan's provisional whole-board budget, agreed 4 October 2026: 900 mA.
// Calibration remains incomplete, so the profile stays disabled and LEDs off.
// 90 mA is the preliminary meter baseline, NOT a worst-case reserve.
// 12 mA/channel is a datasheet reference, NOT calibrated fitted-part coefficients.
// Calibrate baseline, channel coefficients and margin before enabling the profile.
const module_power_profile_t module_power_profile = {
    .revision = 2,
    .configured = false,
    .board_limit_ma = 900,
    .base_ma = 90,
    .margin_ma = 0,
    .channel_ua = {{12000, 12000, 12000}, {12000, 12000, 12000}, {12000, 12000, 12000}},
    .channel_ceiling = {160, 255, 255},
};
#endif

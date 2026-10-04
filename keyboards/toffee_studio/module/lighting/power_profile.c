#include "power.h"

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

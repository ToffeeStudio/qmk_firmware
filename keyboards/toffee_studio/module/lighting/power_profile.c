#include "power.h"

// RESEARCH CANDIDATE: no qualified operating ceiling yet. LEDs stay off.
// 90 mA is the preliminary meter baseline, NOT a worst-case reserve.
// 12 mA/channel is a datasheet reference, NOT calibrated fitted-part coefficients.
// Replace this complete profile only after the documented electrical gates pass.
const module_power_profile_t module_power_profile = {
    .revision = 1,
    .configured = false,
    .board_limit_ma = 0,
    .base_ma = 90,
    .margin_ma = 0,
    .channel_ua = {{12000, 12000, 12000}, {12000, 12000, 12000}, {12000, 12000, 12000}},
    .channel_ceiling = {160, 255, 255},
};

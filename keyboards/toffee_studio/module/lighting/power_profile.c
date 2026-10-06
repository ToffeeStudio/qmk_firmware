#include "power.h"

#ifdef MODULE_MANUAL_MEASUREMENT
// Informational proposal only: explicitly NOT enforced in this manual build.
// Retain estimates for comparison, not as evidence of a measured safe ceiling.
const module_power_profile_t module_power_profile = {
    .revision = 0x8006,
    .configured = true,
    .board_limit_ma = 1000,
    .base_ma = 120,
    .margin_ma = 50,
    .channel_ua = {{15000, 15000, 15000}, {18000, 19000, 18000}, {18000, 18000, 18000}},
    .channel_ceiling = {255, 255, 255},
};
#elif defined(MODULE_POWER_CALIBRATION)
#ifndef MODULE_EXTENSIVE_TEST
#error "Calibration profile requires startup-off, watchdog, RAM-only test firmware"
#endif
// Provisional measurement fixture: no claim of source/thermal qualification.
// Exact-output campaign: 83 visually recorded readings, variable 0–120 mA idle.
// Rounded nominal coefficients below over/under-predicted by -170/+40 mA.
// The separate 100 mA margin covers observed variation, not a certified bound.
// Ethan explicitly authorised a separate staged measurement toward 1 A.
// 1100 mA modeled envelope minus baseline/margin admits up to 1000 mA
// nominal board demand. Host observations stop at 1000 mA; not a sensor/cutoff.
// This test-only envelope does not change the 900 mA product budget below.
// Raw calibration requests must fit this budget unchanged or are rejected.
// Other render paths retain the final limiter; startup-off/watchdog still apply.
const module_power_profile_t module_power_profile = {
    .revision = 0x8005,
    .configured = true,
    .board_limit_ma = 1100,
    .base_ma = 80,
    .margin_ma = 100,
    .channel_ua = {{12000, 12000, 12000}, {15000, 15000, 15000}, {13000, 13000, 13000}},
    .channel_ceiling = {255, 255, 255},
};
#else
// Ethan's provisional whole-board budget, agreed 4 October 2026: 900 mA.
// Enabled for Ethan's explicit normal-firmware deployment request, 5 October.
// Remains provisional, NOT source/thermal certification. Retain the conservative
// coefficients and both reserves. See AUTORESEARCH/04-09-2026 PCB Lacking Current/
// visual-calibration-20261004-lL59DO/model-fit.json (visually read meter, no OCR).
// Nominal baseline 80 mA; coefficients front 11/11/11, rear 13/14/13,
// badge 13/13/13 mA at full channel. Below: ceil(1.25 * nominal + 1),
// with 120 mA electronics reserve and a separate 100 mA model margin.
// Observed validation range is NOT a qualification for the full 900 mA budget.
const module_power_profile_t module_power_profile = {
    .revision = 4,
    .configured = true,
    .board_limit_ma = 900,
    .base_ma = 120,
    .margin_ma = 100,
    .channel_ua = {{15000, 15000, 15000}, {18000, 19000, 18000}, {18000, 18000, 18000}},
    .channel_ceiling = {160, 255, 255},
};
#endif

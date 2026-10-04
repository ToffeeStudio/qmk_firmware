#include "power.h"

#ifdef MODULE_POWER_CALIBRATION
#ifndef MODULE_EXTENSIVE_TEST
#error "Calibration profile requires startup-off, watchdog, RAM-only test firmware"
#endif
// Provisional measurement fixture: no claim of source/thermal qualification.
// Exact-output campaign: 54 visually recorded readings, 60–120 mA idle.
// Rounded nominal coefficients below over/under-predicted by -132/+40 mA.
// The separate 100 mA margin covers observed variation, not a certified bound.
// Ethan authorised staged measurements targeting 800 mA actual current.
// Raw calibration requests must fit this budget unchanged or are rejected.
// Other render paths retain the final limiter; startup-off/watchdog still apply.
const module_power_profile_t module_power_profile = {
    .revision = 0x8004,
    .configured = true,
    .board_limit_ma = 900,
    .base_ma = 80,
    .margin_ma = 100,
    .channel_ua = {{12000, 12000, 12000}, {15000, 15000, 15000}, {13000, 13000, 13000}},
    .channel_ceiling = {255, 255, 255},
};
#else
// Ethan's provisional whole-board budget, agreed 4 October 2026: 900 mA.
// Provisional measured model, still disabled pending intended-range source and
// thermal qualification. See AUTORESEARCH/04-09-2026 PCB Lacking Current/
// visual-calibration-20261004-lL59DO/model-fit.json (visually read meter, no OCR).
// Nominal baseline 80 mA; coefficients front 11/11/11, rear 13/14/13,
// badge 13/13/13 mA at full channel. Below: ceil(1.25 * nominal + 1),
// with 120 mA electronics reserve and a separate 100 mA model margin.
// Observed validation range is NOT a qualification for the full 900 mA budget.
const module_power_profile_t module_power_profile = {
    .revision = 3,
    .configured = false,
    .board_limit_ma = 900,
    .base_ma = 120,
    .margin_ma = 100,
    .channel_ua = {{15000, 15000, 15000}, {18000, 19000, 18000}, {18000, 18000, 18000}},
    .channel_ceiling = {160, 255, 255},
};
#endif

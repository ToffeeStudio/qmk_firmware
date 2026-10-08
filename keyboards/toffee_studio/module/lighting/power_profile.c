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
// 8 October 2026: fit to 174 manually entered lit observations, using actual
// final RGB sums and measured idle baselines (A:80 mA, B:110 mA). Shared RGB
// coefficients; this normal candidate explicitly uses PCB-B's 110 mA baseline.
// Hold out levels 3/5: mean absolute error 14.6 mA, error -37..+66 mA.
// See AUTORESEARCH/04-09-2026 PCB Lacking Current/
// fitted-model-20261008-IuYfWL/model-fit.json for data/provenance/limitations.
// Ethan requests estimated BOARD current <=900 mA, including idle consumption.
// No coefficient padding, extra margin or separate front160 cap: one common
// final-boundary scale limits LED demand to 790 mA. Saved requests stay intact.
// This caps an ESTIMATE, not sensed current or a certified hardware-safe load.
const module_power_profile_t module_power_profile = {
    .revision = 5,
    .configured = true,
    .board_limit_ma = 900,
    .base_ma = 110,
    .margin_ma = 0,
    .channel_ua = {{11106, 11049, 11300}, {12506, 12279, 12360}, {11931, 11610, 10486}},
    .channel_ceiling = {255, 255, 255},
};
#endif

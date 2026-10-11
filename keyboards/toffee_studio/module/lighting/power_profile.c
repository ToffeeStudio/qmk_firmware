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
// This test-only envelope does not change the product budget below.
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
// 11 October 2026: balanced shared fit to 302 manual observations from A/B,
// using actual final RGB sums. One fitted intercept and nine RGB weights;
// no PCB-specific correction. Held-out MAE A25.68/B29.36 mA (100 readings).
// Normal6 was better on B (17.52 mA) but worse on A (39.84 mA); user accepted
// this balance. The 103 mA intercept is fitted, not a measured idle guarantee.
// B has a reported broken front LED; retain all104 mapped positions.
// See AUTORESEARCH/04-09-2026 PCB Lacking Current/
// shared-fit-20261011-tSgJXg/fit.json for data/provenance/limitations.
// Ethan requests estimated BOARD current <=940 mA, including idle consumption.
// No coefficient padding, extra margin or separate front160 cap: one common
// final-boundary scale limits LED demand to 837 mA. Saved requests stay intact.
// This caps an ESTIMATE, not sensed current or a certified hardware-safe load.
const module_power_profile_t module_power_profile = {
    .revision = 8,
    .configured = true,
    .board_limit_ma = 940,
    .base_ma = 103,
    .margin_ma = 0,
    .channel_ua = {{10992, 11016, 11185}, {12294, 12044, 12203}, {12447, 11937, 11456}},
    .channel_ceiling = {255, 255, 255},
};
#endif

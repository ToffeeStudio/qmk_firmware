#include "power.h"

#ifdef MODULE_MANUAL_MEASUREMENT
// Informational proposal only: explicitly NOT enforced in this manual build.
// Retain estimates for comparison, not as evidence of a measured safe ceiling.
const module_power_profile_t module_power_profile = {
    .revision = 0x8006,
    .front_badge_max = 255,
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
    .front_badge_max = 255,
    .configured = true,
    .board_limit_ma = 1100,
    .base_ma = 80,
    .margin_ma = 100,
    .channel_ua = {{12000, 12000, 12000}, {15000, 15000, 15000}, {13000, 13000, 13000}},
    .channel_ceiling = {255, 255, 255},
};
#else
// 21 October 2026: user-selected fit for repaired PCB-B, using all 157 held
// attempts/baselines (785 personally transcribed photos) from 20 October.
// One median per attempt; repeated final outputs stay together in five-fold
// validation. MAE 12.56 mA versus 30.55 mA for normal9 on the same observations.
// The retained over-limit attempt is included; its 1.13 A sample is not erased
// by using a median for fitting. The 75 mA intercept is fitted, not sensed idle.
// All104 mapped positions remain. This is not a universal A/B calibration:
// older, unrepaired B records have greater underprediction with this model.
// See AUTORESEARCH/04-09-2026 PCB Lacking Current/
// repaired-b-fit-20261021/fit.json for data/provenance/limitations.
// Ethan requests estimated BOARD current <=940 mA, including idle consumption.
// No coefficient padding or extra margin: one common final-boundary scale
// limits LED demand to 865 mA. Existing render/output caps and saves stay intact.
// This caps an ESTIMATE, not sensed current or a certified hardware-safe load.
const module_power_profile_t module_power_profile = {
#ifdef MODULE_CURRENT_UNLIMITED
    .revision = 12, // Normal saves/startup/output caps; current estimate only.
#else
    .revision = 11,
#endif
    .front_badge_max = 200,
    .configured = true,
    .board_limit_ma = 940,
    .base_ma = 75,
    .margin_ma = 0,
    .channel_ua = {{11262, 10049, 11020}, {12994, 12869, 13131}, {11021, 11398, 11652}},
    .channel_ceiling = {255, 255, 255},
};
#endif

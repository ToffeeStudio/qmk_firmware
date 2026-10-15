#include "groups.h"
#include "lighting.h"
#include "persistence.h"
#include <string.h>
#include <lib/lib8tion/lib8tion.h>

module_lighting_state_t module_lighting_state;
static bool save_pending;
static uint32_t last_change;
static uint32_t badge_epoch, badge_last_press;
static bool badge_pulse_active;
static const uint8_t front_effects[MODULE_FRONT_EFFECT_COUNT] = {
    RGB_MATRIX_SOLID_COLOR,
    RGB_MATRIX_SOLID_COLOR,
    RGB_MATRIX_BREATHING,
    RGB_MATRIX_CYCLE_ALL,
    RGB_MATRIX_CYCLE_LEFT_RIGHT,
    RGB_MATRIX_CYCLE_UP_DOWN,
    RGB_MATRIX_GRADIENT_LEFT_RIGHT,
    RGB_MATRIX_GRADIENT_UP_DOWN,
    RGB_MATRIX_RAINBOW_BEACON,
    RGB_MATRIX_ALPHAS_MODS,
    RGB_MATRIX_BAND_SAT,
    RGB_MATRIX_BAND_VAL,
    RGB_MATRIX_BAND_PINWHEEL_SAT,
    RGB_MATRIX_BAND_PINWHEEL_VAL,
    RGB_MATRIX_BAND_SPIRAL_SAT,
    RGB_MATRIX_BAND_SPIRAL_VAL,
    RGB_MATRIX_RAINBOW_MOVING_CHEVRON,
    RGB_MATRIX_CYCLE_OUT_IN,
    RGB_MATRIX_CYCLE_OUT_IN_DUAL,
    RGB_MATRIX_CYCLE_PINWHEEL,
    RGB_MATRIX_CYCLE_SPIRAL,
    RGB_MATRIX_DUAL_BEACON,
    RGB_MATRIX_RAINBOW_PINWHEELS,
    RGB_MATRIX_RAINDROPS,
    RGB_MATRIX_JELLYBEAN_RAINDROPS,
    RGB_MATRIX_HUE_BREATHING,
    RGB_MATRIX_HUE_PENDULUM,
    RGB_MATRIX_HUE_WAVE,
    RGB_MATRIX_PIXEL_RAIN,
    RGB_MATRIX_PIXEL_FLOW,
    RGB_MATRIX_PIXEL_FRACTAL,
    RGB_MATRIX_TYPING_HEATMAP,
    RGB_MATRIX_DIGITAL_RAIN,
    RGB_MATRIX_SOLID_REACTIVE_SIMPLE,
    RGB_MATRIX_SOLID_REACTIVE,
    RGB_MATRIX_SOLID_REACTIVE_WIDE,
    RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE,
    RGB_MATRIX_SOLID_REACTIVE_CROSS,
    RGB_MATRIX_SOLID_REACTIVE_MULTICROSS,
    RGB_MATRIX_SOLID_REACTIVE_NEXUS,
    RGB_MATRIX_SOLID_REACTIVE_MULTINEXUS,
    RGB_MATRIX_SPLASH,
    RGB_MATRIX_MULTISPLASH,
    RGB_MATRIX_SOLID_SPLASH,
    RGB_MATRIX_SOLID_MULTISPLASH,
};

static void apply_front(void) {
    front_config_t *front = &module_lighting_state.front;
    // Keep QMK rendering/flush active even when front is off, for independent rear/badge.
    rgb_matrix_enable_noeeprom();
    rgb_matrix_set_flags_noeeprom(LED_FLAG_KEYLIGHT);
    rgb_matrix_mode_noeeprom(front_effects[front->effect]);
    rgb_matrix_set_speed_noeeprom(front->speed);
    uint8_t value = front->effect ? scale8(front->brightness, RGB_MATRIX_MAXIMUM_BRIGHTNESS) : 0;
    rgb_matrix_sethsv_noeeprom(front->hue, front->saturation, value);
}

void lighting_groups_init(void) {
    memset(&module_lighting_state, 0, sizeof(module_lighting_state));
    memset(module_lighting_state.overrides, 255, sizeof(module_lighting_state.overrides));
    module_lighting_state.front = (front_config_t){1, 100, 18, 0, 0};
    module_lighting_state.badge = (badge_config_t){1, 18, 0, 0};
    module_lighting_state.badge_effect = 1;
    module_lighting_state.badge_speed = 100;
    bool loaded = lighting_storage_load(&module_lighting_state);
    save_pending = false;
    badge_epoch = timer_read32();
    badge_pulse_active = false;
    apply_front();
    uprintf("Lighting groups: %s saved front/badge/per-key state\n", loaded ? "restored" : "no");
}

void lighting_groups_schedule_save(void) {
    save_pending = true;
    last_change = timer_read32();
}
void lighting_groups_persistence_task(void) {
#ifdef MODULE_EXTENSIVE_TEST
    return; // Temporary patterns must not overwrite the user's saved settings.
#endif
    if (!save_pending || timer_elapsed32(last_change) < 1000) return;
    if (!lighting_storage_save(&module_lighting_state)) {
        save_pending = false;
        uprintf("Lighting groups: ERROR saving state; stopped automatic retries\n");
        return;
    }
    save_pending = false;
    uprintf("Lighting groups: saved\n");
}
bool lighting_set_front(const front_config_t *config) {
    if (config->effect >= MODULE_FRONT_EFFECT_COUNT) return false;
    module_lighting_state.front = *config;
    apply_front();
    lighting_enable_underglow();
    lighting_groups_schedule_save();
    return true;
}
// Explicit recovery only: unlike ordinary test setters, this may persist zeros.
// Prefer stored preferences to temporary test colours/effects in RAM.
bool lighting_groups_rescue_off(void) {
    module_lighting_state_t target = module_lighting_state;
    lighting_storage_load(&target);
    target.front.brightness = 0;
    target.badge.brightness = 0;
    module_lighting_state = target;
    apply_front();
    save_pending = false;
    if (!lighting_storage_save(&target)) return false;
    module_lighting_state_t stored;
    return lighting_storage_load(&stored) && memcmp(&target, &stored, sizeof(target)) == 0;
}
bool lighting_set_badge(const badge_config_t *config) {
    // Existing solid-only protocol remains explicit and unchanged.
    return lighting_set_badge_animation(config, 1, 100);
}
bool lighting_set_badge_animation(const badge_config_t *config, uint8_t effect, uint8_t speed) {
    if (config->enabled > 1 || effect >= MODULE_BADGE_EFFECT_COUNT) return false;
    module_lighting_state.badge = *config;
    module_lighting_state.badge_effect = effect;
    module_lighting_state.badge_speed = speed;
    badge_epoch = timer_read32();
    badge_pulse_active = false;
    lighting_enable_underglow();
    lighting_groups_schedule_save();
    return true;
}
bool lighting_is_front_led(uint8_t index) {
    return index < MODULE_MAPPED_LED_COUNT && HAS_FLAGS(g_led_config.flags[index], LED_FLAG_KEYLIGHT);
}
uint8_t lighting_get_led_brightness(uint8_t index) {
    return module_lighting_state.overrides[index];
}
void lighting_groups_task(void) {
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        if (!HAS_FLAGS(g_led_config.flags[i], LED_FLAG_KEYLIGHT)) rgb_matrix_set_color(i, 0, 0, 0);
    }
    const badge_config_t *badge = &module_lighting_state.badge;
    uint8_t effect = module_lighting_state.badge_effect;
    uint16_t period = 4000 - (uint16_t)module_lighting_state.badge_speed * 3500 / 255;
    uint8_t phase = (timer_elapsed32(badge_epoch) % period) * 256 / period;
    uint8_t position = 0;
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        if (g_led_config.flags[i] != LED_FLAG_INDICATOR) continue;
        HSV hsv = {badge->hue, badge->saturation, badge->enabled && effect ? badge->brightness : 0};
        if (effect == 2) { // Breathing, independent of QMK front timing.
            uint8_t wave = phase < 128 ? phase * 2 : (255 - phase) * 2;
            hsv.v = (uint16_t)hsv.v * wave / 254;
        } else if (effect == 3) { // Rainbow cycle.
            hsv.h += phase;
            hsv.s = 255;
        } else if (effect == 4) { // Chase in the badge's electrical address order.
            uint8_t distance = (position + 16 - phase / 16) % 16;
            hsv.v = distance < 4 ? (uint16_t)hsv.v * (4 - distance) / 4 : 0;
        } else if (effect == 5) { // Typing pulse; releases do not retrigger it.
            uint16_t duration = 1200 - (uint16_t)module_lighting_state.badge_speed * 1000 / 255;
            uint32_t age = timer_elapsed32(badge_last_press);
            if (badge_pulse_active && age < duration) hsv.v = (uint32_t)hsv.v * (duration - age) / duration;
            else { hsv.v = 0; badge_pulse_active = false; }
        }
        RGB color = hsv_to_rgb_nocie(hsv);
        rgb_matrix_set_color(i, color.r, color.g, color.b);
        position++;
    }
}
void lighting_badge_keypress(void) {
    badge_last_press = timer_read32();
    badge_pulse_active = true;
}

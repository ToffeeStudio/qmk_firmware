#include "groups.h"
#include "lighting.h"
#include "persistence.h"
#include <string.h>
#include <lib/lib8tion/lib8tion.h>

module_lighting_state_t module_lighting_state;
static bool save_pending;
static uint32_t last_change;
static const uint8_t front_effects[MODULE_FRONT_EFFECT_COUNT] = {
    RGB_MATRIX_SOLID_COLOR, RGB_MATRIX_SOLID_COLOR, RGB_MATRIX_BREATHING,
    RGB_MATRIX_CYCLE_ALL, RGB_MATRIX_CYCLE_LEFT_RIGHT, RGB_MATRIX_CYCLE_UP_DOWN,
    RGB_MATRIX_GRADIENT_LEFT_RIGHT, RGB_MATRIX_GRADIENT_UP_DOWN, RGB_MATRIX_RAINBOW_BEACON,
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
    bool loaded = lighting_storage_load(&module_lighting_state);
    save_pending = false;
    apply_front();
    uprintf("Lighting groups: %s saved front/badge/per-key state\n", loaded ? "restored" : "no");
}

void lighting_groups_schedule_save(void) {
    save_pending = true;
    last_change = timer_read32();
}
void lighting_groups_persistence_task(void) {
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
bool lighting_set_badge(const badge_config_t *config) {
    if (config->enabled > 1) return false;
    module_lighting_state.badge = *config;
    lighting_enable_underglow();
    lighting_groups_schedule_save();
    return true;
}
bool lighting_is_front_led(uint8_t index) {
    return index < MODULE_MAPPED_LED_COUNT && g_led_config.flags[index] == LED_FLAG_KEYLIGHT;
}
uint8_t lighting_get_led_brightness(uint8_t index) {
    return module_lighting_state.overrides[index];
}
void lighting_groups_task(void) {
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        if (g_led_config.flags[i] != LED_FLAG_KEYLIGHT) rgb_matrix_set_color(i, 0, 0, 0);
    }
    const badge_config_t *badge = &module_lighting_state.badge;
    RGB color = hsv_to_rgb_nocie((HSV){badge->hue, badge->saturation, badge->enabled ? badge->brightness : 0});
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        if (g_led_config.flags[i] == LED_FLAG_INDICATOR) rgb_matrix_set_color(i, color.r, color.g, color.b);
    }
}

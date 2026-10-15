#pragma once
#include "quantum.h"

#define MODULE_MAPPED_LED_COUNT 104
#define MODULE_FRONT_EFFECT_COUNT 45
#define MODULE_BADGE_EFFECT_COUNT 6

typedef struct __attribute__((packed)) {
    uint8_t effect, speed, brightness, hue, saturation;
} front_config_t;
typedef struct __attribute__((packed)) {
    uint8_t enabled, brightness, hue, saturation;
} badge_config_t;
typedef struct __attribute__((packed)) {
    front_config_t front;
    badge_config_t badge;
    uint8_t overrides[MODULE_MAPPED_LED_COUNT];
    // Appended: v1's existing front/badge/override bytes retain their offsets.
    uint8_t badge_effect, badge_speed;
} module_lighting_state_t;

extern module_lighting_state_t module_lighting_state;
void lighting_groups_init(void);
void lighting_groups_task(void);
bool lighting_set_front(const front_config_t *config);
bool lighting_set_badge(const badge_config_t *config);
bool lighting_set_badge_animation(const badge_config_t *config, uint8_t effect, uint8_t speed);
void lighting_badge_keypress(void);
bool lighting_is_front_led(uint8_t index);
uint8_t lighting_get_led_brightness(uint8_t index);
void lighting_groups_schedule_save(void);
void lighting_groups_persistence_task(void);
bool lighting_groups_rescue_off(void);
bool lighting_storage_load(module_lighting_state_t *state);
bool lighting_storage_save(const module_lighting_state_t *state);

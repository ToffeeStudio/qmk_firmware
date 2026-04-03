#pragma once
#include "animation.h"

typedef enum {
    ANIM_ID_SOLID = 0,
    ANIM_ID_BREATHING,
    ANIM_ID_CYCLE_LEFT_RIGHT,
    ANIM_ID_CYCLE_UP_DOWN,
    ANIM_ID_BAND_SAT_LEFT_RIGHT,
    ANIM_ID_BAND_SAT_UP_DOWN,
    ANIM_ID_HUE_BREATHING,
    ANIM_ID_RAINBOW_VORTEX,
    ANIM_ID_VORTEX,
    ANIM_ID_COMET_TAIL,
} underglow_animation_ids;

extern animation_config_t g_underglow_config;

void underglow_manager_init(void);
void underglow_manager_task(void);
void underglow_manager_next_anim(void);
void underglow_manager_set_anim(uint8_t anim_id);
void underglow_manager_set_speed(uint8_t speed);
void underglow_manager_set_color_hsv(uint8_t h, uint8_t s, uint8_t v);
void underglow_manager_set_brightness(uint8_t brightness);
void underglow_manager_set_color_hs(uint8_t h, uint8_t s);
void underglow_manager_save(void);

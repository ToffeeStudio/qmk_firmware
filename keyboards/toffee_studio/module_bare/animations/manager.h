#pragma once
#include "animation.h"

typedef enum {
    ANIM_ID_BREATHING = 0,
    ANIM_ID_CYCLE_LEFT_RIGHT,
    ANIM_ID_BAND_SAT_LEFT_RIGHT,
    ANIM_ID_HUE_BREATHING,
} underglow_animation_ids;

// The single, global configuration for our underglow system
extern animation_config_t g_underglow_config;

// Public functions
void underglow_manager_init(void);
void underglow_manager_task(void);
void underglow_manager_next_anim(void);
void underglow_manager_set_anim(uint8_t anim_id);

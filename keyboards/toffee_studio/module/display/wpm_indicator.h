#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    WPM_MODE_STATIC_FRAME = 0,
    WPM_MODE_PLAYBACK_SPEED = 1,
} wpm_mode_t;

void wpm_indicator_init(void);
void wpm_indicator_task(void);
void wpm_indicator_set_anim(const char* path, wpm_mode_t mode);
void wpm_indicator_set_config(uint8_t min_wpm, uint8_t max_wpm, uint8_t max_fps);
void wpm_indicator_deactivate(void);

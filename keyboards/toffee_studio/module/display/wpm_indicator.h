#pragma once
#include <stdint.h>

void wpm_indicator_init(void);
void wpm_indicator_task(void);
void wpm_indicator_set_anim(const char* path);
void wpm_indicator_set_config(uint8_t min_wpm, uint8_t max_wpm);
void wpm_indicator_deactivate(void);

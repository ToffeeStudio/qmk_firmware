#ifndef PERSISTENCE_H
#define PERSISTENCE_H
#include <stdbool.h>

void save_display_state(const char* path);
void load_display_state(void);
void save_underglow_config(void);
void load_underglow_config(void);
void schedule_underglow_save(void);
void underglow_persistence_task(void);
#ifdef MODULE_LED_DISCOVERY
bool underglow_rescue_off(void);
#endif

#endif // PERSISTENCE_H

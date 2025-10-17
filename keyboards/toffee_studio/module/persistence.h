#ifndef PERSISTENCE_H
#define PERSISTENCE_H

void save_display_state(const char* path);
void load_display_state(void);
void save_underglow_config(void);
void load_underglow_config(void);

#endif // PERSISTENCE_H


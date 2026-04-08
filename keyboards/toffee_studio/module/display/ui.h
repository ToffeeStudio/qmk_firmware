#ifndef UI_H
#define UI_H

#include <stdint.h>

void ui_init(void);
void ui_reinit_display(void);
void ui_retry_wake_tail(void);
void ui_display_gradient(void);
int ui_display_static_image(const char *path);

#endif // UI_H

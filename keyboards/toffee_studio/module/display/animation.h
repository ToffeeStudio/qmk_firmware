// display/animation.h

#ifndef ANIMATION_H
#define ANIMATION_H

#include "lvgl.h" // For lv_obj_t, lv_timer_t

// --- Public Definitions ---
// Other files need to know the frame size.
#define FRAME_WIDTH 128
#define FRAME_HEIGHT 128
#define BYTES_PER_PIXEL 2
#define FRAME_SIZE ((FRAME_WIDTH * FRAME_HEIGHT) * BYTES_PER_PIXEL)
#define SINGLE_FRAME_SIZE (FRAME_WIDTH * FRAME_HEIGHT * BYTES_PER_PIXEL)

// --- Public (Extern) Variables ---
// Declare these buffers as "external", meaning they are defined elsewhere (in animation.c)
// but other files that include this header can use them.
extern uint8_t frame_buffers[2][FRAME_SIZE];
extern lv_img_dsc_t images[2];

// --- Public Functions ---
void animation_init(void);
void animation_cleanup(void);
int animation_start(const char *path);

#endif // ANIMATION_H

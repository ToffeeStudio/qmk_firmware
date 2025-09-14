#include "quantum.h"
#include "lfs.h"
#include "lvgl.h"
#include "module.h"
#include "display/animation.h"
#include "display/wpm_indicator.h"

#define WPM_SLOW_PERIOD 30000 // 30 seconds, effectively "paused"

typedef struct {
    char       filename[64];
    wpm_mode_t mode;
    uint8_t    min_wpm;
    uint8_t    max_wpm;
    uint8_t    max_fps;
    bool       is_active;
    uint32_t   frame_count;
    int32_t    last_frame_idx;
    uint32_t   last_period_ms;
    lv_obj_t*  img_widget;
} wpm_state_t;

static wpm_state_t wpm_state;

void wpm_indicator_init(void) {
    memset(&wpm_state, 0, sizeof(wpm_state_t));
    wpm_state.mode = WPM_MODE_PLAYBACK_SPEED; // Default to new mode
    wpm_state.min_wpm = 10;
    wpm_state.max_wpm = 35;
    wpm_state.max_fps = 20;
    wpm_state.is_active = false;
    wpm_state.last_frame_idx = -1;
    wpm_state.last_period_ms = 0;
    wpm_state.img_widget = NULL;
}

void wpm_indicator_deactivate(void) {
    if (wpm_state.is_active) {
        wpm_state.is_active = false;
        wpm_state.last_frame_idx = -1;
        wpm_state.last_period_ms = 0;
        if (wpm_state.mode == WPM_MODE_PLAYBACK_SPEED) {
            animation_cleanup(); // Stop the running animation
        } else if (wpm_state.img_widget) {
            lv_obj_add_flag(wpm_state.img_widget, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void wpm_indicator_set_anim(const char* path, wpm_mode_t mode) {
    animation_cleanup(); // Stop any other animation (WPM or regular)
    strncpy(wpm_state.filename, path, sizeof(wpm_state.filename) - 1);
    wpm_state.is_active = true;
    wpm_state.mode = mode;
    wpm_state.last_frame_idx = -1;
    wpm_state.last_period_ms = 0;

    struct lfs_info info;
    if (lfs_stat(&lfs, wpm_state.filename, &info) == 0) {
        wpm_state.frame_count = info.size / FRAME_SIZE;
        if (wpm_state.mode == WPM_MODE_PLAYBACK_SPEED) {
            animation_start(wpm_state.filename); // Start the animation loop
        }
    } else {
        wpm_state.frame_count = 0;
        wpm_state.is_active = false; // File not found, disable
    }
}

void wpm_indicator_set_config(uint8_t min_wpm, uint8_t max_wpm, uint8_t max_fps) {
    wpm_state.min_wpm = min_wpm;
    wpm_state.max_wpm = max_wpm;
    wpm_state.max_fps = max_fps > 0 ? max_fps : 1; // Prevent divide by zero
}

static void display_static_frame(uint32_t frame_index) {
    lfs_file_t file;
    int err = lfs_file_open(&lfs, &file, wpm_state.filename, LFS_O_RDONLY);
    if (err < 0) return;

    lfs_file_seek(&lfs, &file, frame_index * FRAME_SIZE, LFS_SEEK_SET);
    lfs_file_read(&lfs, &file, frame_buffers[0], FRAME_SIZE);
    lfs_file_close(&lfs, &file);

    if (!wpm_state.img_widget) {
        wpm_state.img_widget = lv_img_create(lv_scr_act());
        lv_obj_align(wpm_state.img_widget, LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_clear_flag(wpm_state.img_widget, LV_OBJ_FLAG_HIDDEN);
    lv_img_set_src(wpm_state.img_widget, &images[0]);
    lv_obj_move_foreground(wpm_state.img_widget);
}

void wpm_indicator_task(void) {
    if (!wpm_state.is_active || wpm_state.frame_count == 0) {
        return;
    }

    uint16_t current_wpm = get_current_wpm();
    uprintf("Current WPM: %u\r\n", current_wpm);
    float    progress = 0.0f;

    if (current_wpm > wpm_state.min_wpm) {
        if (current_wpm >= wpm_state.max_wpm) {
            progress = 1.0f;
        } else {
            progress = (float)(current_wpm - wpm_state.min_wpm) / (wpm_state.max_wpm - wpm_state.min_wpm);
        }
    }

    switch (wpm_state.mode) {
        case WPM_MODE_STATIC_FRAME: {
            uint32_t frame_idx = (uint32_t)(progress * (wpm_state.frame_count - 1));
            if (frame_idx >= wpm_state.frame_count) frame_idx = wpm_state.frame_count - 1;

            if ((int32_t)frame_idx != wpm_state.last_frame_idx) {
                display_static_frame(frame_idx);
                wpm_state.last_frame_idx = frame_idx;
            }
            break;
        }
        case WPM_MODE_PLAYBACK_SPEED: {
            uint32_t period_ms;
            if (progress > 0.0f) {
                float target_fps = progress * wpm_state.max_fps;
                if (target_fps < 1.0f) target_fps = 1.0f; // Minimum 1 FPS if typing at all
                period_ms = (uint32_t)(1000.0f / target_fps);
            } else {
                period_ms = WPM_SLOW_PERIOD; // Effectively paused
            }

            if (period_ms != wpm_state.last_period_ms) {
                animation_timer_set_period(period_ms);
                wpm_state.last_period_ms = period_ms;
            }
            break;
        }
    }
}

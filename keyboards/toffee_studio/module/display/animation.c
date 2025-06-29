// display/animation.c

#include "quantum.h"
#include "ch.h"
#include "lfs.h"
#include "lvgl.h"
#include "module.h" // For the global lfs object
#include "display/animation.h" // Include our own header

// --- Module-Private Definitions ---
#define FPS 12
#define FRAME_INTERVAL_MS (1000 / FPS)

// --- Variable Definitions ---
// These are the actual definitions for the variables declared "extern" in the .h file.
uint8_t frame_buffers[2][FRAME_SIZE];
lv_img_dsc_t images[2] = {
    {
        .header.always_zero = 0, .header.w = FRAME_WIDTH, .header.h = FRAME_HEIGHT,
        .data_size = FRAME_SIZE, .header.cf = LV_IMG_CF_TRUE_COLOR, .data = frame_buffers[0],
    },
    {
        .header.always_zero = 0, .header.w = FRAME_WIDTH, .header.h = FRAME_HEIGHT,
        .data_size = FRAME_SIZE, .header.cf = LV_IMG_CF_TRUE_COLOR, .data = frame_buffers[1],
    }
};

// Animation state with double buffering
typedef struct {
    lfs_file_t file;
    lv_obj_t *img;
    uint32_t frame_count;
    uint32_t current_frame;
    uint8_t current_buffer;
    uint8_t next_buffer;
    bool buffer_ready;
    lv_timer_t *lv_timer;
    thread_t *loader_thread;
    bool is_playing;
    bool should_stop;
    mutex_t state_mutex;
} animation_state_t;

static animation_state_t anim_state = {0};

// Forward declarations for functions within this file
static THD_FUNCTION(FrameLoader, arg);
static void frame_timer_callback(lv_timer_t *timer);

// (The rest of the file is identical to what I sent before, this is just for confirmation)
void animation_init(void) {
    chMtxObjectInit(&anim_state.state_mutex);
    anim_state.should_stop = false;
    anim_state.is_playing = false;
    anim_state.img = NULL;
    anim_state.loader_thread = NULL;
}

void animation_cleanup(void) {
    chMtxLock(&anim_state.state_mutex);
    if (anim_state.is_playing) {
        anim_state.should_stop = true;
        if (anim_state.lv_timer) {
            lv_timer_del(anim_state.lv_timer);
            anim_state.lv_timer = NULL;
        }
        if (anim_state.loader_thread) {
            while (1) {
                chSysLock();
                if (anim_state.loader_thread == NULL) {
                    chSysUnlock();
                    break;
                }
                chSysUnlock();
                chThdSleepMilliseconds(10);
            }
        }
        lfs_file_close(&lfs, &anim_state.file);
        if (anim_state.img) {
            lv_obj_del(anim_state.img);
            anim_state.img = NULL;
        }
        anim_state.is_playing = false;
    }
    chMtxUnlock(&anim_state.state_mutex);
}

static THD_WORKING_AREA(waFrameLoader, 1024);
static THD_FUNCTION(FrameLoader, arg) {
    (void)arg;
    while (!anim_state.should_stop) {
        chMtxLock(&anim_state.state_mutex);
        if (!anim_state.is_playing) {
            chMtxUnlock(&anim_state.state_mutex);
            chThdSleepMilliseconds(10);
            continue;
        }
        if (!anim_state.buffer_ready) {
            lfs_off_t frame_pos = anim_state.current_frame * (lfs_off_t)FRAME_SIZE;
            lfs_file_seek(&lfs, &anim_state.file, frame_pos, LFS_SEEK_SET);
            lfs_ssize_t bytes_read = lfs_file_read(&lfs, &anim_state.file, frame_buffers[anim_state.next_buffer], FRAME_SIZE);
            if (bytes_read < 0) {
                uprintf("Error reading frame %ld: %ld\n", (long)anim_state.current_frame, (long)bytes_read);
                anim_state.should_stop = true;
            } else if (bytes_read < FRAME_SIZE) {
                memset(frame_buffers[anim_state.next_buffer] + bytes_read, 0, FRAME_SIZE - bytes_read);
                anim_state.buffer_ready = true;
            } else {
                anim_state.buffer_ready = true;
            }
        }
        chMtxUnlock(&anim_state.state_mutex);
        chThdSleepMilliseconds(FRAME_INTERVAL_MS / 4);
    }
    chSysLock();
    anim_state.loader_thread = NULL;
    chSysUnlock();
}

static void frame_timer_callback(lv_timer_t *timer) {
    (void) timer;
    chMtxLock(&anim_state.state_mutex);
    if (!anim_state.is_playing || !anim_state.buffer_ready) {
        chMtxUnlock(&anim_state.state_mutex);
        return;
    }
    lv_img_set_src(anim_state.img, &images[anim_state.next_buffer]);
    lv_obj_invalidate(anim_state.img);
    uint8_t temp = anim_state.current_buffer;
    anim_state.current_buffer = anim_state.next_buffer;
    anim_state.next_buffer = temp;
    anim_state.buffer_ready = false;
    anim_state.current_frame = (anim_state.current_frame + 1) % anim_state.frame_count;
    chMtxUnlock(&anim_state.state_mutex);
}

int animation_start(const char *path) {
    uprintf("start_animation: Received path: '%s'\n", path);
    chThdSleepMilliseconds(50);
    struct lfs_info info;
    int err = lfs_stat(&lfs, path, &info);
    if (err < 0) {
        uprintf("start_animation: lfs_stat failed for '%s' with error %d\n", path, err);
        return err;
    }
    anim_state.frame_count = info.size / FRAME_SIZE;
    if (anim_state.frame_count == 0 && info.size > 0) {
        uprintf("start_animation: Warning - file size %lu is less than one frame\n", (unsigned long)info.size);
    } else if (info.size % FRAME_SIZE != 0) {
         uprintf("start_animation: Warning - file size %lu is not multiple of frame size.\n", (unsigned long)info.size);
    }
    anim_state.current_frame = 0;
    anim_state.current_buffer = 0;
    anim_state.next_buffer = 1;
    anim_state.buffer_ready = false;
    anim_state.should_stop = false;
    err = lfs_file_open(&lfs, &anim_state.file, path, LFS_O_RDONLY);
    if (err < 0) {
        uprintf("start_animation: lfs_file_open failed for '%s' with error %d\n", path, err);
        return err;
    }
    if (!anim_state.img) {
        anim_state.img = lv_img_create(lv_scr_act());
        if (!anim_state.img) {
             uprintf("start_animation: ERROR - Failed to create lv_img object!\n");
             lfs_file_close(&lfs, &anim_state.file);
             return -1;
        }
         lv_obj_align(anim_state.img, LV_ALIGN_CENTER, 0, 0);
    } else {
         lv_obj_clear_flag(anim_state.img, LV_OBJ_FLAG_HIDDEN);
         lv_obj_move_foreground(anim_state.img);
    }
    lfs_file_seek(&lfs, &anim_state.file, 0, LFS_SEEK_SET);

    // THIS IS THE CORRECTED LINE: uses anim_state.file instead of file
    lfs_ssize_t bytes_read = lfs_file_read(&lfs, &anim_state.file, frame_buffers[0], FRAME_SIZE);

    if (bytes_read < FRAME_SIZE) {
         uprintf("start_animation: Warning - read only %ld bytes for first frame.\n", bytes_read);
         if (bytes_read > 0) {
            memset(frame_buffers[0] + bytes_read, 0, FRAME_SIZE - bytes_read);
         } else if (bytes_read < 0) {
             uprintf("start_animation: ERROR reading first frame: %ld\n", bytes_read);
             lfs_file_close(&lfs, &anim_state.file);
             return bytes_read;
         }
    }
    lv_img_set_src(anim_state.img, &images[0]);
    lv_obj_invalidate(anim_state.img);
    anim_state.is_playing = true;
    if (!anim_state.loader_thread) {
        anim_state.loader_thread = chThdCreateStatic(waFrameLoader, sizeof(waFrameLoader), NORMALPRIO + 1, FrameLoader, NULL);
        if(!anim_state.loader_thread) {
             anim_state.is_playing = false;
             lfs_file_close(&lfs, &anim_state.file);
             return -1;
        }
    }
    if (!anim_state.lv_timer) {
         anim_state.lv_timer = lv_timer_create(frame_timer_callback, FRAME_INTERVAL_MS, NULL);
         if (!anim_state.lv_timer) {
              anim_state.is_playing = false;
              lfs_file_close(&lfs, &anim_state.file);
              anim_state.should_stop = true;
              return -1;
         }
    } else {
         lv_timer_reset(anim_state.lv_timer);
         lv_timer_resume(anim_state.lv_timer);
    }
    return 0;
}

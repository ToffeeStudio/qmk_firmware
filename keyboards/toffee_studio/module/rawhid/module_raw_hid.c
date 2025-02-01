#include "print.h"
#include "ch.h"
#include "usb_descriptor.h"
#include "raw_hid.h"
#include "file_system.h"
#include "lfs.h"
#include "module.h"
#include "module_raw_hid.h"
#include "lvgl.h"

#define CHUNK_SIZE 256
static uint8_t file_buffer[CHUNK_SIZE];
static size_t current_write_pointer = 0;

#define DIRECTORY_MAX 64
#define MAX_PATH_LENGTH 256
#define FRAME_WIDTH 128
#define FRAME_HEIGHT 128
#define FRAME_SIZE ((FRAME_WIDTH * FRAME_HEIGHT) * LV_COLOR_DEPTH / 8)
#define FPS 12
#define FRAME_INTERVAL_MS (1000 / FPS)

// Double buffered image data
static uint8_t frame_buffers[2][FRAME_SIZE];

// Double buffered LVGL image descriptors
static lv_img_dsc_t images[2] = {
    {
        .header.always_zero = 0,
        .header.w = FRAME_WIDTH,
        .header.h = FRAME_HEIGHT,
        .data_size = FRAME_SIZE,
        .header.cf = LV_IMG_CF_TRUE_COLOR,
        .data = frame_buffers[0],
    },
    {
        .header.always_zero = 0,
        .header.w = FRAME_WIDTH,
        .header.h = FRAME_HEIGHT,
        .data_size = FRAME_SIZE,
        .header.cf = LV_IMG_CF_TRUE_COLOR,
        .data = frame_buffers[1],
    }
};

// Animation state with double buffering
typedef struct {
    lfs_file_t file;
    lv_obj_t *img;              // Single LVGL image object we'll update
    uint32_t frame_count;
    uint32_t current_frame;
    uint8_t current_buffer;     // Index of buffer currently being displayed
    uint8_t next_buffer;        // Index of buffer being loaded
    bool buffer_ready;          // Indicates if next buffer is ready
    lv_timer_t *lv_timer;
    thread_t *loader_thread;    // Store thread reference for cleanup
    bool is_playing;
    bool should_stop;           // Flag to signal thread termination
    mutex_t state_mutex;        // Protect shared state
} animation_state_t;

static animation_state_t anim_state = {0};

lfs_file_t current_file;
char path[MAX_PATH_LENGTH];
char current_directory[DIRECTORY_MAX];

// Helper function to open a file
static int open_file(lfs_t *lfs, lfs_file_t *file, const char *path, int flags) {
    int err = lfs_file_open(lfs, file, path, flags);
    if (err < 0) {
        uprintf("Error opening file %s: %d\n", path, err);
    }
    return err;
}
//
// Helper function to close a file
static int close_file(lfs_t *lfs, lfs_file_t *file) {
    int err = lfs_file_sync(lfs, file);
    if (err < 0) {
        uprintf("Error syncing file: %d\n", err);
    }

    err = lfs_file_close(lfs, file);
    if (err < 0) {
        uprintf("Error closing file: %d\n", err);
    }
    return err;
}

static uint8_t *return_buf;

static int parse_ls(uint8_t *data, uint8_t length) {
    uprintf("List files\n");

    lfs_dir_t dir;
    int err = lfs_dir_open(&lfs, &dir, ".");
    if (err < 0) {
        uprintf("Error opening directory: %d\n", err);
        return module_ret_invalid_command;
    }

    struct lfs_info info;
    int offset = 1;  // Start at index 1 to leave room for the return code

    while (true) {
        int res = lfs_dir_read(&lfs, &dir, &info);
        if (res < 0) {
            uprintf("Error reading directory: %d\n", res);
            lfs_dir_close(&lfs, &dir);
            return module_ret_invalid_command;
        }
        if (res == 0) {
            break;
        }

        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) {
            continue;
        }

        int name_len = strlen(info.name);

        // Ensure name_len does not exceed buffer size
        if (name_len > RAW_EPSIZE - offset - 2) {
            name_len = RAW_EPSIZE - offset - 2;
        }

        if (offset + name_len + 2 > RAW_EPSIZE) {
            memset(return_buf + 1, 0, RAW_EPSIZE - 1);  // Clear all but the first byte
            offset = 1;
        }

        strncpy((char *)return_buf + offset, info.name, name_len);
        offset += name_len;
        return_buf[offset++] = (info.type == LFS_TYPE_DIR) ? '/' : ' ';
        return_buf[offset++] = '\n';
    }

    lfs_dir_close(&lfs, &dir);

    return module_ret_success;
}

static int parse_cd(uint8_t *data, uint8_t length) {
    uprintf("Change directory\n");

    if (length <= sizeof(struct packet_header)) {
        uprintf("Insufficient data length\n");
        return module_ret_invalid_command;
    }

    uint8_t *path_data = data + sizeof(struct packet_header);
    size_t path_length = length - sizeof(struct packet_header);

    char new_directory[DIRECTORY_MAX];
    if (path_length >= DIRECTORY_MAX) {
        uprintf("Directory name too long\n");
        return module_ret_invalid_command;
    }
    memcpy(new_directory, path_data, path_length);
    new_directory[path_length] = '\0';

    lfs_dir_t dir;
    int err = lfs_dir_open(&lfs, &dir, new_directory);
    if (err < 0) {
        uprintf("Error opening directory: %d\n", err);
        return module_ret_invalid_command;
    }

    lfs_dir_close(&lfs, &dir);

    // Update current directory
    strncpy(current_directory, new_directory, DIRECTORY_MAX - 1);
    current_directory[DIRECTORY_MAX - 1] = '\0';

    uprintf("Changed to directory: %s\n", new_directory);
    return module_ret_success;
}

static int parse_pwd(uint8_t *data, uint8_t length) {
    uprintf("Print working directory\n");

    // Ensure current_directory is null-terminated
    current_directory[DIRECTORY_MAX - 1] = '\0';
    size_t dir_length = strlen(current_directory);

    if (dir_length > RAW_EPSIZE - 1) {
        dir_length = RAW_EPSIZE - 1;
    }

    memcpy(return_buf + 1, current_directory, dir_length);

    return module_ret_success;
}

static int parse_rm(uint8_t *data, uint8_t length) {
    uprintf("Remove file/directory\n");

    if (length <= sizeof(struct packet_header)) {
        uprintf("Insufficient data length\n");
        return module_ret_invalid_command;
    }

    uint8_t *path_data = data + sizeof(struct packet_header);
    size_t path_length = length - sizeof(struct packet_header);

    char path[MAX_PATH_LENGTH];
    if (path_length >= MAX_PATH_LENGTH) {
        uprintf("Path too long\n");
        return module_ret_invalid_command;
    }
    memcpy(path, path_data, path_length);
    path[path_length] = '\0';

    int err = lfs_remove(&lfs, path);
    if (err < 0) {
        uprintf("Error removing file/directory: %d\n", err);
        return err;
    }

    return module_ret_success;
}

static int parse_mkdir(uint8_t *data, uint8_t length) {
    uprintf("Make directory\n");

    if (length <= sizeof(struct packet_header)) {
        uprintf("Insufficient data length\n");
        return module_ret_invalid_command;
    }

    uint8_t *path_data = data + sizeof(struct packet_header);
    size_t path_length = length - sizeof(struct packet_header);

    char path[MAX_PATH_LENGTH];
    if (path_length >= MAX_PATH_LENGTH) {
        uprintf("Path too long\n");
        return module_ret_invalid_command;
    }
    memcpy(path, path_data, path_length);
    path[path_length] = '\0';

    int err = lfs_mkdir(&lfs, path);
    if (err < 0) {
        uprintf("Error creating directory: %d\n", err);
        return err;
    }

    return module_ret_success;
}

static int parse_touch(uint8_t *data, uint8_t length) {
    uprintf("Create empty file\n");

    if (length <= sizeof(struct packet_header)) {
        uprintf("Insufficient data length\n");
        return module_ret_invalid_command;
    }

    uint8_t *path_data = data + sizeof(struct packet_header);
    size_t path_length = length - sizeof(struct packet_header);

    char path[MAX_PATH_LENGTH];
    if (path_length >= MAX_PATH_LENGTH) {
        uprintf("Path too long\n");
        return module_ret_invalid_command;
    }
    memcpy(path, path_data, path_length);
    path[path_length] = '\0';

    lfs_file_t file;
    int err = lfs_file_open(&lfs, &file, path, LFS_O_WRONLY | LFS_O_CREAT);
    if (err < 0) {
        uprintf("Error creating file: %d\n", err);
        return err;
    }

    err = lfs_file_close(&lfs, &file);
    if (err < 0) {
        uprintf("Error closing file: %d\n", err);
        return err;
    }

    return module_ret_success;
}

static int parse_cat(uint8_t *data, uint8_t length) {
    uprintf("Read file contents\n");

    if (length <= sizeof(struct packet_header)) {
        uprintf("Insufficient data length\n");
        return module_ret_invalid_command;
    }

    uint8_t *path_data = data + sizeof(struct packet_header);
    size_t path_length = length - sizeof(struct packet_header);

    char path[MAX_PATH_LENGTH];
    if (path_length >= MAX_PATH_LENGTH) {
        uprintf("Path too long\n");
        return module_ret_invalid_command;
    }
    memcpy(path, path_data, path_length);
    path[path_length] = '\0';

    lfs_file_t file;
    int err = open_file(&lfs, &file, path, LFS_O_RDONLY);
    if (err < 0) {
        return err;
    }

    lfs_ssize_t bytes_read;
    while ((bytes_read = lfs_file_read(&lfs, &file, return_buf + 1, RAW_EPSIZE - 1)) > 0) {
        uprintf("Read %ld bytes from file %s\n", bytes_read, path);
    }

    if (bytes_read < 0) {
        uprintf("Error reading file: %ld\n", bytes_read);
        close_file(&lfs, &file);
        return bytes_read;
    }

    err = close_file(&lfs, &file);
    if (err < 0) {
        return err;
    }

    return module_ret_success;
}

static int parse_open(uint8_t *data, uint8_t length) {
    uprintf("Open file\n");

    if (length <= sizeof(struct packet_header)) {
        uprintf("Insufficient data length\n");
        return module_ret_invalid_command;
    }

    uint8_t *path_data = data + sizeof(struct packet_header);
    size_t path_length = length - sizeof(struct packet_header);

    if (path_length >= MAX_PATH_LENGTH) {
        uprintf("Path too long\n");
        return module_ret_invalid_command;
    }
    memcpy(path, path_data, path_length);
    path[path_length] = '\0';

    int err = open_file(&lfs, &current_file, path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND);
    if (err < 0) {
        return err;
    }

    current_write_pointer = 0;
    memset(file_buffer, 0, CHUNK_SIZE);

    return module_ret_success;
}

static int parse_write(uint8_t *data, uint8_t length) {
    if (length <= sizeof(struct packet_header)) {
        uprintf("Invalid length: %d\n", length);
        return module_ret_invalid_command;
    }

    uint8_t *write_data = data + sizeof(struct packet_header);
    size_t data_length   = length - sizeof(struct packet_header);

    // 1) Query how many blocks are used so far
    lfs_ssize_t used_blocks = lfs_fs_size(&lfs);
    if (used_blocks < 0) {
        uprintf("Error reading used blocks: %ld\n", used_blocks);
        return module_ret_invalid_command;
    }

    // 2) total_blocks (16MB / 4096)
    uint32_t total_blocks = 4096;
    uint32_t free_blocks  = (used_blocks < total_blocks)
                            ? (total_blocks - (uint32_t)used_blocks)
                            : 0;
    uint32_t remaining_bytes = free_blocks * 4096;

    if (data_length > remaining_bytes) {
        uprintf("Not enough space, refusing to write.\n");
        return module_ret_image_flash_full; // Or any error code you prefer
    }

    // 3) Continue with the existing logic that buffers data and writes in 256-byte chunks
    uprintf("Got data len: %d, current buf: %d\n", data_length, current_write_pointer);

    // Sanity check
    if (data_length == 0 || data_length > 4096) { // Max reasonable packet size
        uprintf("Bad data length: %d\n", data_length);
        return module_ret_invalid_command;
    }

    // If this data would overflow our current buffer
    if (current_write_pointer + data_length >= CHUNK_SIZE) {
        size_t bytes_to_fill = CHUNK_SIZE - current_write_pointer;

        uprintf("Will fill %d bytes to complete chunk\n", bytes_to_fill);

        // Fill up the current buffer
        memcpy(file_buffer + current_write_pointer, write_data, bytes_to_fill);

        uprintf("Writing full chunk of %d bytes\n", CHUNK_SIZE);

        // Write out the full chunk with interrupt protection
        lfs_ssize_t written = lfs_file_write(&lfs, &current_file, file_buffer, CHUNK_SIZE);

        if (written < 0) {
            uprintf("Write failed with %ld\n", (long)written);            current_write_pointer = 0;  // Reset on error
            return written;
        }

        if (written != CHUNK_SIZE) {
            uprintf("Incomplete write: %ld of %d\n", (long)written, CHUNK_SIZE);
            current_write_pointer = 0;  // Reset on error
            return -1;
        }

        // Move remaining bytes to start of buffer
        size_t remaining = data_length - bytes_to_fill;
        uprintf("Moving %d remaining bytes to start\n", remaining);

        if (remaining > 0 && remaining < CHUNK_SIZE) {
            memcpy(file_buffer, write_data + bytes_to_fill, remaining);
            current_write_pointer = remaining;
        } else {
            uprintf("Invalid remaining bytes: %d\n", remaining);
            current_write_pointer = 0;
            return -1;
        }

    } else {
        // Just add to current buffer
        uprintf("Adding %d bytes to buffer at %d\n", data_length, current_write_pointer);
        memcpy(file_buffer + current_write_pointer, write_data, data_length);
        current_write_pointer += data_length;
    }

    uprintf("Buffer now at %d/256\n", current_write_pointer);
    return module_ret_success;
}

// Add this helper function to flush any remaining data
static int flush_write_buffer(void) {
    if (current_write_pointer == 0) {
        return module_ret_success;
    }

    uprintf("Final flush of %zu bytes\n", current_write_pointer);
    lfs_ssize_t written = lfs_file_write(&lfs, &current_file, file_buffer, current_write_pointer);
    if (written < 0) {
        uprintf("Error on final flush: %ld\n", written);
        current_write_pointer = 0;
        return written;
    }

    current_write_pointer = 0;
    return module_ret_success;
}

static int parse_close(uint8_t *data, uint8_t length) {
    uprintf("Close current file\n");

    // 1) Flush leftover data in file_buffer[]
    int err = flush_write_buffer();
    if (err < 0) {
        uprintf("Error flushing leftover data: %d\n", err);
        // Decide whether to close anyway or return the error
    }

    // 2) Now do the usual close
    err = close_file(&lfs, &current_file);
    if (err < 0) {
        return err;
    }

    return module_ret_success;
}

static int parse_format_filesystem(uint8_t *data, uint8_t length) {
    uprintf("Format filesystem\n");
    int err = rp2040_format_lfs(&lfs);
    if (err < 0) {
        uprintf("Error formatting filesystem: %d\n", err);
        return err;
    }
    err = rp2040_mount_lfs(&lfs);
    if (err < 0) {
        uprintf("Error mounting filesystem: %d\n", err);
        return err;
    }
    return module_ret_success;
}

static int parse_flash_remaining(uint8_t *data, uint8_t length) {
    uprintf(">>> Flash remaining NEW PRINT\n");

    // How many blocks are actually used in the current LFS partition
    lfs_ssize_t used_blocks = lfs_fs_size(&lfs);
    if (used_blocks < 0) {
        uprintf("Error reading used blocks: %ld\n", used_blocks);
        return module_ret_invalid_command;
    }

    // If your block size is 4096 (typical), and the entire partition is 16 MB:
    // 16 MB / 4 KB = 4096 total blocks
    uint32_t total_blocks = 4096;  // 16MB / 4KB

    // Compute how many free blocks remain
    uint32_t free_blocks = (used_blocks < total_blocks)
                           ? (total_blocks - (uint32_t)used_blocks)
                           : 0;

    // Each block is 4096 bytes
    uint32_t remaining_bytes = free_blocks * 4096;

    // Return that to the host
    memcpy(return_buf + 1, &remaining_bytes, sizeof(remaining_bytes));

    uprintf("Remaining bytes: %lu\n", remaining_bytes);
    return module_ret_success;
}

// Forward declarations
static void cleanup_animation(void);
static THD_FUNCTION(FrameLoader, arg);
static void frame_timer_callback(lv_timer_t *timer);

static void init_animation_state(void) {
    chMtxObjectInit(&anim_state.state_mutex);
    anim_state.should_stop = false;
    anim_state.is_playing = false;
    anim_state.img = NULL;
    anim_state.loader_thread = NULL;
}

static void cleanup_animation(void) {
    chMtxLock(&anim_state.state_mutex);

    if (anim_state.is_playing) {
        // Signal thread to stop
        anim_state.should_stop = true;

        // Delete LVGL timer if exists
        if (anim_state.lv_timer) {
            lv_timer_del(anim_state.lv_timer);
            anim_state.lv_timer = NULL;
        }

        // Ensure the loader thread terminates cleanly
        if (anim_state.loader_thread) {
            while (chThdGetState(anim_state.loader_thread) != THD_STATE_TERMINATED) {
                chThdSleepMilliseconds(10);
            }
            anim_state.loader_thread = NULL;
        }

        // Close file
        lfs_file_close(&lfs, &anim_state.file);

        // Cleanup LVGL object
        if (anim_state.img) {
            lv_obj_del(anim_state.img);
            anim_state.img = NULL;
        }

        anim_state.is_playing = false;
    }

    chMtxUnlock(&anim_state.state_mutex);
}

// Background frame loading thread
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
            // Calculate next frame's file offset
            lfs_off_t frame_pos = anim_state.current_frame * (lfs_off_t)FRAME_SIZE;

            // Seek & read
            lfs_file_seek(&lfs, &anim_state.file, frame_pos, LFS_SEEK_SET);
            lfs_ssize_t bytes_read = lfs_file_read(&lfs, &anim_state.file,
                                                  frame_buffers[anim_state.next_buffer],
                                                  FRAME_SIZE);

            if (bytes_read < 0) {
                // Serious LFS error
                uprintf("Error reading frame %ld: %ld\n",
                        (long)anim_state.current_frame, (long)bytes_read);
                // Stop or handle the error
                anim_state.should_stop = true; 
            } else if (bytes_read < FRAME_SIZE) {
                // We got a partial frame
                memset(frame_buffers[anim_state.next_buffer] + bytes_read,
                       0,
                       FRAME_SIZE - bytes_read);
                anim_state.buffer_ready = true;
                // We can keep going, or decide to stop if we do not want partial frames
            } else {
                // Normal full read
                anim_state.buffer_ready = true;
            }
        }

        chMtxUnlock(&anim_state.state_mutex);
        // Sleep for some fraction of the frame interval
        chThdSleepMilliseconds(FRAME_INTERVAL_MS / 4);
    }
}

static void frame_timer_callback(lv_timer_t *timer) {
    (void) timer;
    chMtxLock(&anim_state.state_mutex);

    if (!anim_state.is_playing || !anim_state.buffer_ready) {
        chMtxUnlock(&anim_state.state_mutex);
        return;
    }

    // Update LVGL image source to next buffer
    lv_img_set_src(anim_state.img, &images[anim_state.next_buffer]);

    // Force a screen update
    lv_obj_invalidate(anim_state.img);

    // Swap buffer indices
    uint8_t temp = anim_state.current_buffer;
    anim_state.current_buffer = anim_state.next_buffer;
    anim_state.next_buffer = temp;

    anim_state.buffer_ready = false;
    anim_state.current_frame = (anim_state.current_frame + 1) % anim_state.frame_count;

    chMtxUnlock(&anim_state.state_mutex);
}

static int start_animation(const char *path) {
    // Get file size for frame count
    struct lfs_info info;
    int err = lfs_stat(&lfs, path, &info);
    if (err < 0) return err;

    // Initialize animation state
    anim_state.frame_count = info.size / FRAME_SIZE;
    anim_state.current_frame = 0;
    anim_state.current_buffer = 0;
    anim_state.next_buffer = 1;
    anim_state.buffer_ready = false;
    anim_state.should_stop = false;

    // Open file for animation
    err = lfs_file_open(&lfs, &anim_state.file, path, LFS_O_RDONLY);
    if (err < 0) return err;

    // Create LVGL image object if needed
    if (!anim_state.img) {
        anim_state.img = lv_img_create(lv_scr_act());
    }

    // Set initial image (first buffer)
    lv_img_set_src(anim_state.img, &images[0]);

    anim_state.is_playing = true;

    // Start background loader thread
    anim_state.loader_thread = chThdCreateStatic(waFrameLoader, sizeof(waFrameLoader),
                                                NORMALPRIO + 1, FrameLoader, NULL);

    // Start frame timer
    anim_state.lv_timer = lv_timer_create(frame_timer_callback, FRAME_INTERVAL_MS, NULL);

    return module_ret_success;
}

static int parse_choose_image(uint8_t *data, uint8_t length) {
    uprintf("Choose image\n");

    if (length <= sizeof(struct packet_header)) {
        uprintf("Insufficient data length\n");
        return module_ret_invalid_command;
    }

    // Clean up any existing animation first
    cleanup_animation();

    uint8_t *path_data = data + sizeof(struct packet_header);
    size_t max_path_length = length - sizeof(struct packet_header);
    size_t path_length = 0;

    // Find actual string length
    for(path_length = 0; path_length < max_path_length; path_length++) {
        if(path_data[path_length] == 0) break;
    }

    char path[MAX_PATH_LENGTH];
    if (path_length >= MAX_PATH_LENGTH - 2) {
        uprintf("Path too long\n");
        return module_ret_invalid_command;
    }
    memcpy(path, path_data, path_length);
    path[path_length] = '\0';

    bool is_anim = path_length > 5 && strncmp(path + (path_length - 5), ".araw", 5) == 0;

    if (is_anim) {
        uprintf("Animated image\n");
        return start_animation(path);
    }

    // Handle static images
    lfs_file_t file;
    int err = lfs_file_open(&lfs, &file, path, LFS_O_RDONLY);
    if (err < 0) {
        uprintf("Error opening image file: %d\n", err);
        return err;
    }

    // Read into first buffer
    lfs_ssize_t bytes_read = lfs_file_read(&lfs, &file, frame_buffers[0], FRAME_SIZE);
    if (bytes_read < 0) {
        uprintf("Error reading image file: %ld\n", bytes_read);
        lfs_file_close(&lfs, &file);
        return bytes_read;
    }

    lfs_file_close(&lfs, &file);

    // Create and display static image
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &images[0]);

    return module_ret_success;
}

static int parse_write_display(uint8_t *data, uint8_t length) {
    uprintf("Write to display\n");

    if (length <= sizeof(struct packet_header)) {
        uprintf("No data to write to display\n");
        return module_ret_invalid_command;
    }

    uint8_t *write_data = data + sizeof(struct packet_header);
    uint32_t bytes_to_write = length - sizeof(struct packet_header);

    static uint32_t write_pointer = 0;

    // Ensure we don't exceed the buffer size
    if (write_pointer + bytes_to_write > FRAME_SIZE) {
        bytes_to_write = FRAME_SIZE - write_pointer;
    }

    // Write to first buffer
    memcpy(frame_buffers[0] + write_pointer, write_data, bytes_to_write);
    write_pointer += bytes_to_write;

    // Update display if buffer is full
    if (write_pointer >= FRAME_SIZE) {
        write_pointer = 0;

        lv_obj_t *img = lv_img_create(lv_scr_act());
        lv_img_set_src(img, &images[0]);
    }

    uprintf("Wrote %lu bytes to display buffer\n", bytes_to_write);

    return module_ret_success;
}

static int parse_set_time(uint8_t *data, uint8_t length) {
    uprintf("Set time\n");

    if (length < sizeof(struct packet_header) + 3) {
        uprintf("Insufficient data length for time\n");
        return module_ret_invalid_command;
    }

    uint8_t *time_data = data + sizeof(struct packet_header);

    uint8_t hour = time_data[0];
    uint8_t minute = time_data[1];
    uint8_t second = time_data[2];

    // Implement logic to set the system time
    // This might involve updating an RTC or internal time counter

    uprintf("Time set to: %02d:%02d:%02d\n", hour, minute, second);
    return module_ret_success;
}

static module_raw_hid_parse_t* parse_packet_funcs[] = {
    parse_ls,
    parse_cd,
    parse_pwd,
    parse_rm,
    parse_mkdir,
    parse_touch,
    parse_cat,
    parse_open,
    parse_write,
    parse_close,
    parse_format_filesystem,
    parse_flash_remaining,
    parse_choose_image,
    parse_write_display,
    parse_set_time,
};

static bool anim_init = false;
int module_raw_hid_parse_packet(uint8_t *data, uint8_t length) {
    int err;
    return_buf = data;

		if (!anim_init) {
				init_animation_state();
				anim_init = true;
		}

    uprintf("Received packet. Parsing command.\r\n");

    if (length < 6 || length > RAW_EPSIZE) {  // Assuming header is 6 bytes
        uprintf("Invalid packet length\n");
        return -1;
    }

    // Manually parse header
    uint8_t magic_number = data[0];
    uint8_t command_id = data[1];
//    uint32_t packet_id = *(uint32_t *)(data + 2);

    uprintf("Buffer contents: ");
    for (int i = 0; i < length; i++) {
        uprintf("%02X ", data[i]);
    }
    uprintf("\n");

    if (magic_number != 0x09) {
        uprintf("Invalid magic number: %02X\n", magic_number);
        return -1;
    }

    command_id -= id_module_cmd_base;
    uprintf("Command ID: %d\n", command_id);

    if (command_id >= (sizeof(parse_packet_funcs) / sizeof(parse_packet_funcs[0]))) {
        uprintf("Invalid command ID\n");
        return -1;
    }

    // Call the appropriate parsing function
    err = parse_packet_funcs[command_id](data, length);
    if (err < 0) {
        uprintf("Error parsing packet: %d\n", err);
        return_buf[0] = err;
    } else {
        return_buf[0] = module_ret_success;
    }

    return err;
}


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

// Static variables for paged directory listings
static lfs_dir_t paged_ls_dir;
static bool paged_ls_dir_open = false;

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
    uprintf("List files (First Page)\n");

    //chThdSleepMilliseconds(1000);
    const char *message_to_send = "LS PARSED\r\n";
    const char *ptr = message_to_send; // Create a pointer to the start of the string
    while (*ptr != '\0') {
        virtser_send((uint8_t)(*ptr)); // Send the byte pointed to by ptr
        ptr++;                        // Move the pointer to the next character
    }

    // Close any previously open directory listing
    if (paged_ls_dir_open) {
        uprintf("Closing previously open paged directory handle.\n");
        lfs_dir_close(&lfs, &paged_ls_dir);
        paged_ls_dir_open = false;
    }

    // 'return_buf' points to the same memory as 'data'.
    // We overwrite 'data' starting from index 1 for the response.
    return_buf = data; // Ensure return_buf is set correctly
    uint8_t *response_payload = return_buf + 1; // Payload starts after the return code byte
    const uint8_t max_payload_size = RAW_EPSIZE - 1; // Max bytes for filenames + separators
    uint8_t current_offset = 0;

    // Clear the response payload area first
    memset(response_payload, 0, max_payload_size);

    // Open the current directory for listing
    int err = lfs_dir_open(&lfs, &paged_ls_dir, "."); // Open "."
    if (err < 0) {
        uprintf("Error opening directory '.': %d\n", err);
        return_buf[0] = module_ret_invalid_command; // Use enum for clarity
        return module_ret_invalid_command; // Return error code
    }
    paged_ls_dir_open = true; // Mark directory as open for paging
    uprintf("Opened directory '.' for paged listing.\n");

    struct lfs_info info;
    bool has_more = false;

    // Read directory entries until buffer is full or end of directory
    while (true) {
        // Store position *before* reading the next entry
        lfs_off_t current_pos = lfs_dir_tell(&lfs, &paged_ls_dir);
        if (current_pos < 0) {
            uprintf("Error getting directory position: %ld\n", (long)current_pos);
            lfs_dir_close(&lfs, &paged_ls_dir);
            paged_ls_dir_open = false;
            return_buf[0] = module_ret_invalid_command;
            return module_ret_invalid_command;
        }

        int res = lfs_dir_read(&lfs, &paged_ls_dir, &info);
        if (res < 0) {
            uprintf("Error reading directory entry: %d\n", res);
            lfs_dir_close(&lfs, &paged_ls_dir);
            paged_ls_dir_open = false;
            return_buf[0] = module_ret_invalid_command;
            return module_ret_invalid_command;
        }

        if (res == 0) {
            // End of directory reached
            uprintf("End of directory reached in parse_ls.\n");
            lfs_dir_close(&lfs, &paged_ls_dir); // Close the directory
            paged_ls_dir_open = false;         // Mark as closed
            has_more = false; // Ensure has_more is false
            break;            // Exit the loop
        }

        // Skip "." and ".." entries if they appear (should only be first two)
        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) {
            uprintf("Skipping '.' or '..'\n");
            continue;
        }

        // Filter out potentially invalid names or empty strings
        if (info.name[0] == '\0') {
            uprintf("Skipping empty filename entry.\n");
            continue;
        }

        int name_len = strlen(info.name);
        // Calculate needed space: name + type_char + null_separator_char
        int needed_space = name_len + 2;

        // Check if adding this entry *would* exceed the buffer
        if (current_offset + needed_space > max_payload_size) {
            uprintf("Buffer would be full with '%s' (%d bytes needed, %d available), more entries remain.\n", info.name, needed_space, max_payload_size - current_offset);
            has_more = true; // Mark that there are more entries

            // *** THE FIX: Rewind the directory iterator ***
            // Seek back to the position *before* we read the entry that didn't fit.
            int seek_err = lfs_dir_seek(&lfs, &paged_ls_dir, current_pos);
            if (seek_err < 0) {
                 uprintf("Error rewinding directory iterator: %d\n", seek_err);
                 lfs_dir_close(&lfs, &paged_ls_dir); // Close on error
                 paged_ls_dir_open = false;
                 return_buf[0] = module_ret_invalid_command; // Indicate error
                 return module_ret_invalid_command;
            }
             uprintf("Rewound directory iterator to position %ld for next read.\n", (long)current_pos);
            // ******************************************

            break; // Exit the loop, leaving the directory open
        }

        // If we reach here, the entry fits. Copy filename
        memcpy(response_payload + current_offset, info.name, name_len);
        current_offset += name_len;

        // Add type indicator ('/' for dir, ' ' for file)
        response_payload[current_offset++] = (info.type == LFS_TYPE_DIR) ? '/' : ' ';

        // Add NULL separator
        response_payload[current_offset++] = '\0';

        uprintf("Added entry: %s%c\n", info.name, (info.type == LFS_TYPE_DIR) ? '/' : ' ');
    }

    // Set appropriate return code based on whether more entries are available
    if (has_more) {
        uprintf("More entries available, returning MORE_ENTRIES code\n");
        return_buf[0] = module_ret_more_entries;
        return module_ret_more_entries; // Return "more entries" code
    } else {
        // If we finished without filling the buffer, it means no more entries *or*
        // the directory was empty after skipping '.' and '..'.
        // The directory should have been closed in the (res == 0) block if end was reached.
        uprintf("No more entries, returning SUCCESS code (dir_open=%d)\n", paged_ls_dir_open);
        // Ensure directory is closed if loop finished naturally
        if (paged_ls_dir_open) {
             uprintf("Closing directory handle at the end of parse_ls.\n");
             lfs_dir_close(&lfs, &paged_ls_dir);
             paged_ls_dir_open = false;
        }
        return_buf[0] = module_ret_success;
        return module_ret_success;
    }
}

static int parse_ls_next(uint8_t *data, uint8_t length) {
    uprintf("List files (Next Page)\n");

    // Check if we have an active directory listing
    if (!paged_ls_dir_open) {
        uprintf("No active directory listing for paging\n");
        return_buf[0] = module_ret_invalid_command;
        return module_ret_invalid_command;
    }

    // 'return_buf' points to the same memory as 'data'.
    // We overwrite 'data' starting from index 1 for the response.
    uint8_t *response_payload = return_buf + 1;
    const uint8_t max_payload_size = RAW_EPSIZE - 1;
    uint8_t current_offset = 0;

    // Clear the response payload area first
    memset(response_payload, 0, max_payload_size);

    struct lfs_info info;
    bool has_more = false;

    // Read directory entries until we fill the buffer or reach the end
    while (true) {
        int res = lfs_dir_read(&lfs, &paged_ls_dir, &info);
        if (res < 0) {
            uprintf("Error reading directory: %d\n", res);
            lfs_dir_close(&lfs, &paged_ls_dir);
            paged_ls_dir_open = false;
            return_buf[0] = module_ret_invalid_command;
            return module_ret_invalid_command;
        }
        if (res == 0) {
            // End of directory - close and reset state
            uprintf("End of directory reached in ls_next\n");
            lfs_dir_close(&lfs, &paged_ls_dir);
            paged_ls_dir_open = false;
            break;
        }

        // Skip "." and ".." entries
        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) {
            continue;
        }

        // Filter out potentially invalid names
        if (info.name[0] == '\0') {
            uprintf("Skipping empty filename entry.\n");
            continue;
        }

        int name_len = strlen(info.name);
        // Calculate needed space: name + type_char + null_separator_char
        int needed_space = name_len + 2;

        // Check if adding this entry would exceed the buffer
        if (current_offset + needed_space > max_payload_size) {
            // Buffer would be full with this entry, so we have more entries
            // for a subsequent page
            uprintf("Buffer would be full with this entry in ls_next, more entries remain.\n");
            has_more = true;
            break;
        }

        // Copy filename
        memcpy(response_payload + current_offset, info.name, name_len);
        current_offset += name_len;

        // Add type indicator ('/' for dir, ' ' for file)
        response_payload[current_offset++] = (info.type == LFS_TYPE_DIR) ? '/' : ' ';

        // Add NULL separator
        response_payload[current_offset++] = '\0';

        uprintf("Added entry in ls_next: %s%c\n", info.name, (info.type == LFS_TYPE_DIR) ? '/' : ' ');
    }

    // Set appropriate return code based on whether more entries are available
    if (has_more) {
        uprintf("More entries available in ls_next, returning MORE_ENTRIES code\n");
        return_buf[0] = module_ret_more_entries;
        return module_ret_more_entries; // Return "more entries" code
    } else {
        uprintf("No more entries in ls_next, returning SUCCESS code\n");
        return_buf[0] = module_ret_success;
        return module_ret_success;
    }
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
            uprintf("Write failed with %ld\n", (long)written);
            current_write_pointer = 0;  // Reset on error
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

// New ping handler
static int parse_ping(uint8_t *data, uint8_t length) {
    uprintf("Ping command received.\n");
    const char response[] = "TS_Module_v1";
    virtser_send((const uint8_t *)response, sizeof(response) - 1);
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
    // Thread is exiting; mark it as terminated
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
    // *** ADD DEBUG PRINT AND DELAY HERE ***
    uprintf("start_animation: Received path: '%s'\n", path);
    uprintf("start_animation: Adding short delay before lfs_stat...\n");
    chThdSleepMilliseconds(50);

    // Get file size for frame count
    struct lfs_info info;
    uprintf("start_animation: Calling lfs_stat for '%s'...\n", path); // Print right before call
    int err = lfs_stat(&lfs, path, &info);
    if (err < 0) {
        uprintf("start_animation: lfs_stat failed for '%s' with error %d (LFS_ERR_NOENT = -2)\n", path, err);
        return err; // Return the LFS error code
    }
    uprintf("start_animation: lfs_stat successful. Size: %lu\n", (unsigned long)info.size);


    // Initialize animation state (Ensure this doesn't re-open the file if already open, but cleanup should handle it)
    anim_state.frame_count = info.size / FRAME_SIZE;
    if (anim_state.frame_count == 0 && info.size > 0) {
        uprintf("start_animation: Warning - file size %lu is less than one frame (%d)?\n", (unsigned long)info.size, FRAME_SIZE);
        // Decide how to handle: maybe treat as 1 frame? or error out?
        // For now, let it proceed, might just display garbage or nothing.
    } else if (info.size % FRAME_SIZE != 0) {
         uprintf("start_animation: Warning - file size %lu is not an exact multiple of frame size %d.\n", (unsigned long)info.size, FRAME_SIZE);
         // Playback might be truncated or behave unexpectedly at the end.
    }
     uprintf("start_animation: Calculated frame count: %lu\n", anim_state.frame_count);

    anim_state.current_frame = 0;
    anim_state.current_buffer = 0;
    anim_state.next_buffer = 1;
    anim_state.buffer_ready = false;
    anim_state.should_stop = false; // Ensure stop flag is clear

    // Open file for animation
    uprintf("start_animation: Attempting to open file '%s' for reading...\n", path);
    err = lfs_file_open(&lfs, &anim_state.file, path, LFS_O_RDONLY);
    if (err < 0) {
        uprintf("start_animation: lfs_file_open failed for '%s' with error %d\n", path, err);
        // Clean up any partially set state? (Might not be necessary if cleanup_animation handles it)
        return err; // Return the LFS error code
    }
    uprintf("start_animation: File '%s' opened successfully for reading.\n", path);

    // Create LVGL image object if needed (should be done only once ideally)
    // Consider moving img creation outside start_animation if possible,
    // or ensure cleanup_animation reliably deletes it.
    if (!anim_state.img) {
        uprintf("start_animation: Creating lv_img object.\n");
        anim_state.img = lv_img_create(lv_scr_act());
        if (!anim_state.img) {
             uprintf("start_animation: ERROR - Failed to create lv_img object!\n");
             lfs_file_close(&lfs, &anim_state.file); // Close the file we just opened
             return -1; // Indicate generic error
        }
         lv_obj_align(anim_state.img, LV_ALIGN_CENTER, 0, 0); // Center it once
    } else {
         uprintf("start_animation: Reusing existing lv_img object.\n");
         // Ensure it's visible/on top if other things were drawn
         lv_obj_clear_flag(anim_state.img, LV_OBJ_FLAG_HIDDEN);
         lv_obj_move_foreground(anim_state.img);
    }


    // Set initial image (first buffer) - Load the first frame immediately?
    // The current logic relies on the loader thread and timer, let's stick with that for now.
    // Need to ensure the first frame gets loaded promptly.
    // Maybe pre-load the *first* frame here synchronously?
    uprintf("start_animation: Pre-loading first frame into buffer 0...\n");
    lfs_file_seek(&lfs, &anim_state.file, 0, LFS_SEEK_SET); // Go to start
    lfs_ssize_t bytes_read = lfs_file_read(&lfs, &anim_state.file, frame_buffers[0], FRAME_SIZE);
    if (bytes_read < FRAME_SIZE) {
         uprintf("start_animation: Warning - read only %ld bytes for first frame.\n", bytes_read);
         // Zero out the rest if needed
         if (bytes_read > 0) {
            memset(frame_buffers[0] + bytes_read, 0, FRAME_SIZE - bytes_read);
         } else if (bytes_read < 0) {
             uprintf("start_animation: ERROR reading first frame: %ld\n", bytes_read);
             lfs_file_close(&lfs, &anim_state.file);
             // Optionally delete lv_img if created here? cleanup_animation should handle it.
             return bytes_read;
         }
    }
    lv_img_set_src(anim_state.img, &images[0]); // Set src to buffer 0
    lv_obj_invalidate(anim_state.img); // Force redraw
    uprintf("start_animation: First frame loaded and displayed.\n");


    anim_state.is_playing = true; // Mark as playing *after* essential setup

    // Start background loader thread (if not already running from a previous attempt?)
    // cleanup_animation should ensure the old one is stopped.
    if (!anim_state.loader_thread) {
        uprintf("start_animation: Creating FrameLoader thread...\n");
        anim_state.loader_thread = chThdCreateStatic(waFrameLoader, sizeof(waFrameLoader),
                                                    NORMALPRIO + 1, FrameLoader, NULL);
        if(!anim_state.loader_thread) {
             uprintf("start_animation: ERROR - Failed to create FrameLoader thread!\n");
             anim_state.is_playing = false;
             lfs_file_close(&lfs, &anim_state.file);
             // cleanup?
             return -1;
        }
    } else {
         uprintf("start_animation: FrameLoader thread might already exist?\n"); // Should not happen if cleanup works
    }


    // Start frame timer (if not already running?)
    // cleanup_animation should ensure the old one is stopped.
    if (!anim_state.lv_timer) {
         uprintf("start_animation: Creating LVGL timer...\n");
         anim_state.lv_timer = lv_timer_create(frame_timer_callback, FRAME_INTERVAL_MS, NULL);
         if (!anim_state.lv_timer) {
              uprintf("start_animation: ERROR - Failed to create LVGL timer!\n");
              anim_state.is_playing = false;
              lfs_file_close(&lfs, &anim_state.file);
              // Need to potentially stop thread if created
              anim_state.should_stop = true; // Signal thread
              // cleanup?
              return -1;
         }
    } else {
         uprintf("start_animation: LVGL timer might already exist?\n"); // Should not happen if cleanup works
         lv_timer_reset(anim_state.lv_timer); // Reset existing timer?
         lv_timer_resume(anim_state.lv_timer);
    }

    uprintf("start_animation: Animation setup complete.\n");
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

static int parse_placeholder(uint8_t *data, uint8_t length) {
    uprintf("Unimplemented command received.\n");
    return module_ret_invalid_command; // Or another appropriate error
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
    parse_placeholder,
    parse_ls_next, // Add the new function to handle "next page" requests
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
        // DON'T override the return code if the function already set it!
        // Only set the success code if no other code was set
        if (return_buf[0] != module_ret_more_entries) {
            return_buf[0] = module_ret_success;
        }
    }

    return err;
}

#include "print.h"
#include "ch.h"
#include "usb_descriptor.h"
#include "raw_hid.h"
#include "file_system.h"
#include "lfs.h"
#include "module.h"
#include "module_raw_hid.h"
#include "display/animation.h"
#include "display/ui.h"
#include "animations/manager.h"
#include "lvgl.h"

#define CHUNK_SIZE 256
static uint8_t file_buffer[CHUNK_SIZE];
static size_t current_write_pointer = 0;

#define DIRECTORY_MAX 64
#define MAX_PATH_LENGTH 256

// Static variables for paged directory listings
static lfs_dir_t paged_ls_dir;
static bool paged_ls_dir_open = false;

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

#define CDC_READ_BUFFER_SIZE 256 // Size of buffer to read from flash at a time
static uint8_t cdc_read_buffer[CDC_READ_BUFFER_SIZE];

static void virtser_send_u32_le(uint32_t value) {
    uint8_t bytes[4];
    bytes[0] = (value >> 0) & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
    bytes[3] = (value >> 24) & 0xFF;
    for (int i = 0; i < 4; ++i) {
        virtser_send(bytes[i]);
    }
}

static void virtser_send_string(const char *str) {
    const char *ptr = str;
    while (*ptr != '\0') {
        virtser_send((uint8_t)(*ptr));
        ptr++;
    }
    virtser_send('\0'); // Send the null terminator
}

static void virtser_send_block(const uint8_t *buffer, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        virtser_send(buffer[i]);
    }
}

static int parse_ls_all(uint8_t *data, uint8_t length) {
    (void)data; // Unused
    (void)length; // Unused

    uprintf("CMD: parse_ls_all received. Starting CDC file dump...\n");
    // Give the host OS a moment to potentially enumerate/prepare the CDC port
    chThdSleepMilliseconds(1000);

    lfs_dir_t dir;
    struct lfs_info info;
    int err;
    int files_sent = 0;

    // Buffer for potentially modified filename (.araw -> .raw)
    // LFS_NAME_MAX is usually defined in lfs.h or platform includes
    #ifndef LFS_NAME_MAX
    #define LFS_NAME_MAX 64 // Provide a sensible default if not defined
    // #warning "LFS_NAME_MAX not defined, using default 64" // Optional warning
    #endif
    char filename_buffer[LFS_NAME_MAX + 1]; // +1 for null terminator

    uprintf("LFS_ALL: Opening root directory '/'\n");
    err = lfs_dir_open(&lfs, &dir, "/"); // Open root directory explicitly
    if (err < 0) {
        uprintf("LFS_ALL: Error opening root directory: %d\n", err);
        uprintf("LFS_ALL: Sending termination signal (error case).\n");
        virtser_send('\0'); // Send termination signal (empty filename)
        return module_ret_invalid_command; // Return HID error
    }

    uprintf("LFS_ALL: Reading directory entries...\n");
    while (true) {
        int res = lfs_dir_read(&lfs, &dir, &info);
        if (res < 0) {
            uprintf("LFS_ALL: Error reading directory entry: %d\n", res);
            break; // Exit loop on read error
        }

        if (res == 0) {
            uprintf("LFS_ALL: End of directory reached.\n");
            break; // End of directory
        }

        // Skip directories
        if (info.type == LFS_TYPE_DIR) {
            continue;
        }

        // Process regular files
        if (info.type == LFS_TYPE_REG) {
            const char *dot_raw = strstr(info.name, ".raw");
            const char *dot_araw = strstr(info.name, ".araw");
            size_t name_len = strlen(info.name);

            bool ends_with_raw = dot_raw != NULL && dot_raw == info.name + name_len - 4;
            bool ends_with_araw = dot_araw != NULL && dot_araw == info.name + name_len - 5;

            // Check if it's a target file type
            if (ends_with_raw || ends_with_araw) {

                bool is_araw = ends_with_araw;
                const char *filename_to_send = info.name; // Default to original name
                uint32_t size_to_send = (uint32_t)info.size; // Default to original size

                // --- Logic specific to .araw files ---
                if (is_araw) {
                    uprintf("LFS_ALL: Processing .araw file: %s\n", info.name);
                    // Check if file is large enough for at least one frame
                    if (info.size < SINGLE_FRAME_SIZE) {
                        uprintf("LFS_ALL: WARN: '%s' is smaller (%lu bytes) than one frame (%d bytes). Skipping.\n",
                                info.name, (unsigned long)info.size, SINGLE_FRAME_SIZE);
                        continue; // Skip this file entirely
                    }

                    // Create the modified filename (.raw) in the buffer
                    // Ensure buffer is large enough (checked by LFS_NAME_MAX implicitly)
                    strcpy(filename_buffer, info.name);
                    // Find the last dot safely
                    char *last_dot = strrchr(filename_buffer, '.');
                    if (last_dot != NULL && strcmp(last_dot, ".araw") == 0) {
                        // Overwrite extension safely
                        *(last_dot + 1) = 'r';
                        *(last_dot + 2) = 'a';
                        *(last_dot + 3) = 'w';
                        *(last_dot + 4) = '\0'; // Null-terminate after .raw
                        filename_to_send = filename_buffer; // Point to the modified name in the buffer
                        uprintf("LFS_ALL: Reporting as: %s\n", filename_to_send);
                    } else {
                        // This case should ideally not happen if ends_with_araw was true
                        uprintf("LFS_ALL: WARN: Could not modify filename extension for %s. Using original.\n", info.name);
                        filename_to_send = info.name; // Fallback to original name
                    }

                    // Set size to report/send as a single frame
                    size_to_send = SINGLE_FRAME_SIZE;
                    uprintf("LFS_ALL: Reporting size as single frame: %lu\n", (unsigned long)size_to_send);
                } else {
                     // Standard .raw file processing
                     uprintf("LFS_ALL: Processing .raw file: %s, Size: %lu\n", info.name, (unsigned long)info.size);
                     // filename_to_send and size_to_send are already correct from defaults
                }
                // --- End .araw specific logic ---

                // --- Send Header ---
                chThdSleepMilliseconds(20); // Short delay before header
                uprintf("LFS_ALL: Sending header: Name='%s', Size=%lu\n", filename_to_send, (unsigned long)size_to_send);
                virtser_send_string(filename_to_send); // Send original or modified name
                virtser_send_u32_le(size_to_send);     // Send original or single frame size

                // --- Send Data ---
                lfs_file_t file;
                uprintf("LFS_ALL: Opening original file '%s' for reading...\n", info.name);
                err = lfs_file_open(&lfs, &file, info.name, LFS_O_RDONLY); // Always open the original file

                if (err < 0) {
                    uprintf("LFS_ALL: ERROR opening file '%s': %d. Skipping data send.\n", info.name, err);
                } else {
                    uprintf("LFS_ALL: File opened. Sending %lu bytes...\n", (unsigned long)size_to_send);
                    lfs_ssize_t bytes_read;
                    lfs_size_t total_bytes_sent = 0;
                    uint32_t loop_counter = 0; // Counter for progress print

                    // Read loop - stops when EOF is reached OR when enough bytes for size_to_send are sent
                    while (total_bytes_sent < size_to_send && (bytes_read = lfs_file_read(&lfs, &file, cdc_read_buffer, CDC_READ_BUFFER_SIZE)) > 0) {

                        // Determine how many bytes from this read chunk to actually send
                        lfs_size_t bytes_in_chunk_to_send = bytes_read;
                        // If sending the full chunk would exceed the target size (relevant for .araw)
                        if (total_bytes_sent + bytes_read > size_to_send) {
                            bytes_in_chunk_to_send = size_to_send - total_bytes_sent;
                            // uprintf("LFS_ALL: Adjusting last chunk size to %lu bytes\n", (unsigned long)bytes_in_chunk_to_send); // Optional debug
                        }

                        // Send the required portion of the chunk
                        virtser_send_block(cdc_read_buffer, bytes_in_chunk_to_send);
                        total_bytes_sent += bytes_in_chunk_to_send;

                        // Optional: Print progress every N chunks to avoid spamming console
                        loop_counter++;
                        if (loop_counter % 100 == 0 && size_to_send > 0) { // Print approx every 100 * 256 bytes
                             uprintf("... sent %lu / %lu bytes (%lu%%)\n",
                                     (unsigned long)total_bytes_sent, (unsigned long)size_to_send,
                                     (unsigned long)(total_bytes_sent * 100 / size_to_send));
                        }

                        // If we've sent the target number of bytes, exit the read loop
                        if (total_bytes_sent >= size_to_send) {
                            break; // Exit while loop explicitly
                        }
                    } // End while lfs_file_read

                    // Check if lfs_file_read ended with an error
                    if (bytes_read < 0) {
                        uprintf("LFS_ALL: ERROR reading from file '%s' during transfer: %ld\n", info.name, bytes_read);
                    }
                    uprintf("LFS_ALL: Finished sending data for '%s'. Total %lu bytes sent.\n", filename_to_send, (unsigned long)total_bytes_sent);

                    // Close the file handle
                    err = lfs_file_close(&lfs, &file);
                    if (err < 0) {
                        uprintf("LFS_ALL: ERROR closing file '%s': %d\n", info.name, err);
                    } // else { uprintf("LFS_ALL: File closed.\n"); } // Can uncomment for verbose logs
                }
                // --- End Send Data ---

                files_sent++;
                chThdSleepMilliseconds(50); // Delay between files seems helpful
            } // End if (file is .raw or .araw)
        } // End if (file is LFS_TYPE_REG)
    } // end while true (directory read loop)

    // Close the directory handle
    err = lfs_dir_close(&lfs, &dir);
    if (err < 0) {
         uprintf("LFS_ALL: Error closing directory: %d\n", err);
    } else {
         uprintf("LFS_ALL: Directory closed.\n");
    }

    // Send Termination Signal (empty filename: just a single null byte)
    uprintf("LFS_ALL: Sending termination signal (end of list).\n");
    virtser_send('\0');

    uprintf("LFS_ALL: Finished. Sent %d files (headers + data).\n", files_sent);
    return module_ret_success; // Indicate success to HID host
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

static int parse_choose_image(uint8_t *data, uint8_t length) {
    uprintf("Choose image\n");

    if (length <= sizeof(struct packet_header)) {
        uprintf("Insufficient data length\n");
        return module_ret_invalid_command;
    }

    // Clean up any existing animation first
    animation_cleanup();

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
        return animation_start(path);
    }

    // Handle static images by calling the new UI function
    return ui_display_static_image(path);
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

static int parse_set_animation(uint8_t *data, uint8_t length) {
    if (length < 7) {
        return module_ret_invalid_command;
    }
    uint8_t anim_id = data[6];
    uprintf("RAW HID: Setting animation to ID %u\n", anim_id);
    underglow_manager_set_anim(anim_id);
    return module_ret_success;
}

static int parse_set_speed(uint8_t *data, uint8_t length) {
    if (length < 7) {
        return module_ret_invalid_command;
    }
    uint8_t speed = data[6];
    uprintf("RAW HID: Setting speed to %u\n", speed);
    underglow_manager_set_speed(speed);
    return module_ret_success;
}

static int parse_set_color_hsv(uint8_t *data, uint8_t length) {
    if (length < 9) { // HSV needs 3 bytes
        return module_ret_invalid_command;
    }
    uint8_t h = data[6];
    uint8_t s = data[7];
    uint8_t v = data[8];
    uprintf("RAW HID: Setting color to HSV(%u, %u, %u)\n", h, s, v);
    underglow_manager_set_color_hsv(h, s, v);
    return module_ret_success;
}

static int parse_placeholder(uint8_t *data, uint8_t length) {
    uprintf("Unimplemented command received.\n");
    return module_ret_invalid_command; // Or another appropriate error
}

int module_raw_hid_parse_packet(uint8_t *data, uint8_t length) {
    int err = module_ret_invalid_command; // Default to error
    return_buf = data;

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

    uprintf("Command ID: 0x%02X\n", command_id);

    switch (command_id) {
        case id_module_cmd_ls:
            err = parse_ls(data, length);
            break;
        case id_module_cmd_cd:
            err = parse_cd(data, length);
            break;
        case id_module_cmd_pwd:
            err = parse_pwd(data, length);
            break;
        case id_module_cmd_rm:
            err = parse_rm(data, length);
            break;
        case id_module_cmd_mkdir:
            err = parse_mkdir(data, length);
            break;
        case id_module_cmd_touch:
            err = parse_touch(data, length);
            break;
        case id_module_cmd_cat:
            err = parse_cat(data, length);
            break;
        case id_module_cmd_open:
            err = parse_open(data, length);
            break;
        case id_module_cmd_write:
            err = parse_write(data, length);
            break;
        case id_module_cmd_close:
            err = parse_close(data, length);
            break;
        case id_module_cmd_format_filesystem:
            err = parse_format_filesystem(data, length);
            break;
        case id_module_cmd_flash_remaining:
            err = parse_flash_remaining(data, length);
            break;
        case id_module_cmd_choose_image:
            err = parse_choose_image(data, length);
            break;
        case id_module_cmd_write_display:
            err = parse_write_display(data, length);
            break;
        case id_module_cmd_set_time:
            err = parse_set_time(data, length);
            break;
        case id_module_cmd_ls_next:
            err = parse_ls_next(data, length);
            break;
        case id_module_cmd_lsall:
            err = parse_ls_all(data, length);
            break;
        case id_lighting_set_animation:
            err = parse_set_animation(data, length);
            break;
        case id_lighting_set_speed:
            err = parse_set_speed(data, length);
            break;
        case id_lighting_set_color_hsv:
            err = parse_set_color_hsv(data, length);
            break;
        default:
            uprintf("Invalid command ID\n");
            err = -1;
            break;
    }

    if (err < 0) {
        uprintf("Error parsing packet: %d\n", err);
        return_buf[0] = err;
    } else {
        // DON'T override the return code if the function already set it!
        // Only set the success code if no other code was set
        if (err != module_ret_more_entries) {
            return_buf[0] = err;
        }
    }

    return err;
}

// module/persistence.c

#include "quantum.h"
#include "lfs.h"
#include "module.h"
#include "string.h"
#include "persistence.h"
#include "display/animation.h"
#include "display/ui.h"
#include "animations/manager.h"
#include "animations/animation.h"

// The dedicated file for storing the last displayed image path.
#define DISPLAY_STATE_FILE "/display_state.bin"
#define UNDERGLOW_CONFIG_FILE "/underglow_config.bin"
// Max path length we can save/load.
#define MAX_PATH_LEN 64

void save_display_state(const char* path) {
    uprintf("PERSIST: Saving display state. Path: '%s'\n", path);

    lfs_file_t file;
    // LFS_O_WRONLY: Write-only. LFS_O_CREAT: Create if doesn't exist. LFS_O_TRUNC: Clear file before writing.
    int err = lfs_file_open(&lfs, &file, DISPLAY_STATE_FILE, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (err < 0) {
        uprintf("PERSIST: ERROR! Failed to open display state file for writing: %d\n", err);
        return;
    }

    // Write the path string, including the null terminator.
    lfs_ssize_t written = lfs_file_write(&lfs, &file, path, strlen(path) + 1);
    lfs_file_close(&lfs, &file); // Always close the file.

    if (written < 0) {
        uprintf("PERSIST: ERROR! Failed to write to display state file: %ld\n", written);
    } else {
        uprintf("PERSIST: Display state saved successfully.\n");
    }
}

void load_display_state(void) {
    uprintf("PERSIST: Attempting to load last display state...\n");

    lfs_file_t file;
    int err = lfs_file_open(&lfs, &file, DISPLAY_STATE_FILE, LFS_O_RDONLY);

    if (err < 0) {
        uprintf("PERSIST: No saved display state found. Skipping.\n");
        return; // This is normal on first boot or if never set.
    }

    char path_buffer[MAX_PATH_LEN] = {0}; // Initialize buffer to all zeros.
    lfs_ssize_t bytes_read = lfs_file_read(&lfs, &file, path_buffer, sizeof(path_buffer) - 1);
    lfs_file_close(&lfs, &file);

    if (bytes_read <= 0) {
        uprintf("PERSIST: ERROR! Failed to read display state file or file is empty: %ld\n", bytes_read);
        return;
    }

    // The buffer is already null-terminated due to the initialization.
    uprintf("PERSIST: Loaded saved path: '%s'\n", path_buffer);

    // Check if the path is for an animated file or a static one.
    if (strstr(path_buffer, ".araw") != NULL) {
        uprintf("PERSIST: Restoring animated image.\n");
        animation_start(path_buffer);
    } else if (strstr(path_buffer, ".raw") != NULL) {
        uprintf("PERSIST: Restoring static image.\n");
        ui_display_static_image(path_buffer);
    } else {
        uprintf("PERSIST: WARN! Loaded path is not a .raw or .araw file. Ignoring.\n");
    }
}


extern animation_t* underglow_animations[];
extern const uint8_t UNDERGLOW_ANIMATION_COUNT;

typedef struct {
    uint8_t brightness;
    uint8_t speed;
    HSV     color;
    uint8_t current_animation_id;
} underglow_storage_t;

void save_underglow_config(void) {
    uprintf("PERSIST: Saving underglow config...\n");

    // 1. Prepare the data to be saved.
    underglow_storage_t config_to_save;
    config_to_save.brightness = g_underglow_config.brightness;
    config_to_save.speed = g_underglow_config.speed;
    config_to_save.color = g_underglow_config.color;
    config_to_save.current_animation_id = g_underglow_config.current_animation_id;

    // 2. Open the file for writing.
    lfs_file_t file;
    int err = lfs_file_open(&lfs, &file, UNDERGLOW_CONFIG_FILE, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (err < 0) {
        uprintf("PERSIST: ERROR! Failed to open underglow config for writing: %d\n", err);
        return;
    }

    // 3. Write the struct to the file.
    lfs_ssize_t written = lfs_file_write(&lfs, &file, &config_to_save, sizeof(underglow_storage_t));
    lfs_file_close(&lfs, &file);

    if (written != sizeof(underglow_storage_t)) {
        uprintf("PERSIST: ERROR! Failed to write all underglow data. Wrote %ld bytes.\n", written);
    } else {
        uprintf("PERSIST: Underglow config saved successfully.\n");
    }
}

void load_underglow_config(void) {
    uprintf("PERSIST: Attempting to load underglow config...\n");

    underglow_storage_t loaded_config;
    lfs_file_t file;

    // 1. Attempt to open and read the file.
    int err = lfs_file_open(&lfs, &file, UNDERGLOW_CONFIG_FILE, LFS_O_RDONLY);
    if (err < 0) {
        uprintf("PERSIST: No saved underglow config found. Using defaults.\n");
        return;
    }

    lfs_ssize_t bytes_read = lfs_file_read(&lfs, &file, &loaded_config, sizeof(underglow_storage_t));
    lfs_file_close(&lfs, &file);

    if (bytes_read != sizeof(underglow_storage_t)) {
        uprintf("PERSIST: ERROR! Underglow config file is wrong size. Read %ld bytes. Using defaults.\n", bytes_read);
        return;
    }

    // 2. Apply the loaded settings to the live config struct.
    uprintf("PERSIST: Found saved config. Applying settings.\n");
    g_underglow_config.brightness = loaded_config.brightness;
    g_underglow_config.speed = loaded_config.speed;
    g_underglow_config.color = loaded_config.color;

    // 3. IMPORTANT: Validate and set the animation ID and pointer.
    if (loaded_config.current_animation_id < UNDERGLOW_ANIMATION_COUNT) {
        g_underglow_config.current_animation_id = loaded_config.current_animation_id;
        g_underglow_config.active_animation = underglow_animations[g_underglow_config.current_animation_id];
        uprintf("PERSIST: Restored animation ID: %u\n", g_underglow_config.current_animation_id);
    } else {
        uprintf("PERSIST: WARN! Saved animation ID %u is invalid. Reverting to default.\n", loaded_config.current_animation_id);
        // The default (ID 0) is already set by underglow_manager_init(), so we do nothing.
    }
}

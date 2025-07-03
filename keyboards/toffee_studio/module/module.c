// --- Core QMK Includes ---
#include "display/animation.h"
#include "display/ui.h"
#include "display/cdc_handler.h"
#include "lighting/lighting.h"
#include "quantum.h" // Includes core QMK functionality, ChibiOS, config files etc.
#include "rgb_matrix.h"
#include "gpio.h"
#include "print.h"   // For uprintf
#include "ch.h"

// --- Feature Includes ---
#include <stdint.h>
#include <stdbool.h>
#include <string.h> // For memset, strlen etc. <--- Added/Ensured this include

#ifdef QUANTUM_PAINTER_ENABLE
#include "qp.h"               // Quantum Painter core
#include "qp_lvgl.h"          // Quantum Painter LVGL integration
#include "lvgl.h"             // LVGL library itself
#include "qp_gc9107.h"        // Specific driver for GC9107
// Include pre-compiled graphics resources if you have/use them elsewhere
// #include "graphics/thintel15.qff.c"
// #include "graphics/Crimson_Light.c"
#endif // QUANTUM_PAINTER_ENABLE

#ifdef LITTLEFS_ENABLE
#include "lfs.h"          // LittleFS core
#include "file_system.h"  // QMK helpers for LFS (rp2040_mount_lfs etc.)
#endif // LITTLEFS_ENABLE

#ifdef VIA_ENABLE // Include Raw HID header only if VIA is enabled (as it's called from via command)
#include "rawhid/module_raw_hid.h" // Your custom Raw HID parser header
#endif // VIA_ENABLE

#include "virtser.h"                // For virtser_recv, virtser_send

// --- Include the header defining the global `lfs` object ---
#include "module.h" // Assumes `lfs_t lfs;` is declared here or in a common header included by module.h


// =========================================================================
// Initialization and Other Callbacks
// =========================================================================

void board_init(void) {
    // Keep this minimal if keyboard_post_init_kb handles major init
    uprintf("board_init() called.\n");
}

bool rgb_matrix_indicators_user(void) {
    lighting_task();
    return false; // Return false to prevent QMK from running its own animations.
}

void keyboard_post_init_kb(void) {
    chThdSleepMilliseconds(3000);
    uprintf("keyboard_post_init_kb called.\n");

    lighting_init();

#ifdef LITTLEFS_ENABLE
    // 1) Mount LFS here:
    uprintf("Mounting LFS...\n");
    int err = rp2040_mount_lfs(&lfs);
    if (err < 0) {
         uprintf("LFS mount failed: %d. Trying to format...\n", err);
         err = rp2040_format_lfs(&lfs); // Try formatting
         if (err < 0) {
              uprintf("LFS format failed: %d\n", err);
         } else {
              err = rp2040_mount_lfs(&lfs); // Try mount again
              if (err < 0) {
                   uprintf("LFS mount failed AFTER format: %d\n", err);
              } else {
                   uprintf("LFS mounted successfully after format.\n");
              }
         }
    } else {
         uprintf("LFS mounted successfully.\n");
    }

    // 2) Optional debug prints for space:
    lfs_ssize_t used_blocks = lfs_fs_size(&lfs);
    if(used_blocks >= 0) {
        uprintf("LFS used blocks at boot: %ld\n", used_blocks);
        // Use literal value for reservation KB from rules.mk if PICO_FLASH_SIZE_BYTES is standard
        #ifndef FLASH_RESERVATION_KB
        #define FLASH_RESERVATION_KB 1024 // Default if not in rules.mk
        #warning "FLASH_RESERVATION_KB not defined in rules.mk, using default 1024"
        #endif
        #ifndef PICO_FLASH_SIZE_BYTES
        #define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024) // Default Pico size if not defined
        #warning "PICO_FLASH_SIZE_BYTES not defined, using default 2MB"
        #endif
        // Calculate total LFS size: Total Flash - Bootloader (assume standard size?) - Code/Firmware (hard to know exactly) - Reservation
        // Simplification: Assume LFS partition starts after reservation
        // WARNING: This calculation is a rough estimate!
        uint32_t lfs_partition_bytes = PICO_FLASH_SIZE_BYTES - (FLASH_RESERVATION_KB * 1024);
        uint32_t lfs_block_size = 4096; // Common LFS block size for RP2040 SPI flash
        if(lfs.cfg) { // Use configured block size if available
             lfs_block_size = lfs.cfg->block_size;
        }
        uint32_t total_blocks = lfs_partition_bytes / lfs_block_size;

        uprintf("Estimated total LFS blocks: %lu (based on %lu KB reservation and %lu byte blocks)\n", total_blocks, (unsigned long)FLASH_RESERVATION_KB, (unsigned long)lfs_block_size);
        uint32_t free_blocks  = (used_blocks < total_blocks) ? (total_blocks - (uint32_t)used_blocks) : 0;
        uprintf("Estimated free space: %lu blocks => %lu bytes\n", free_blocks, free_blocks * lfs_block_size);
    } else {
         uprintf("Error getting LFS size: %ld\n", used_blocks);
    }
#endif // LITTLEFS_ENABLE check

#ifdef QUANTUM_PAINTER_ENABLE
    // 3) Initialize your display hardware and QP/LVGL
    uprintf("Initializing display hardware...\n");
    setPinOutputPushPull(0); // GP0
    writePinHigh(0);         // Turn backlight on
    ui_init();                         // Initialize QP/LVGL etc. which calls draw_gradient
    uprintf("Display initialized.\n");
#endif // QUANTUM_PAINTER_ENABLE

    // --- Initialize CDC Receive State ---
    #if defined(VIRTSER_ENABLE) && defined(LITTLEFS_ENABLE) // <--- Use the new CDC logic condition
    uprintf("Initializing CDC Receive State...\n");
    cdc_handler_init(); // <--- Use the reset function to ensure clean start
    #endif

    animation_init();

    // 4) Call the default post-init user function if it exists
    keyboard_post_init_user(); // Weakly defined, safe to call
    uprintf("keyboard_post_init_kb finished.\n");
}

// This function handles Raw HID commands coming *from Via*
void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    uprintf("via_custom_value_command_kb called, length %d\n", length);

    // LIGHTING COMMANDS
    lighting_handle_hid_command(data, length);

    // FILE SYSTEM AND OTHER COMMANDS
    int hid_err = module_raw_hid_parse_packet(data, length);
    if (hid_err < 0) {
        uprintf("Error parsing Raw HID packet via VIA: %d\n", hid_err);
    }
    // uprintf("via_custom_value_command_kb: LFS disabled, skipping Raw HID parse.\n");
}

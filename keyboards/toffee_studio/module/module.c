// --- Core QMK Includes ---
#include "quantum.h" // Includes core QMK functionality, ChibiOS, config files etc.
#include "gpio.h"
#include "print.h"   // For uprintf

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
// RGB Matrix Description
// =========================================================================

led_config_t g_led_config = {
    // key_led_map initialiser - Map key matrix to LED index
    {
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED,      0, NO_LED, NO_LED, NO_LED, NO_LED},
    },
    // LED Index to physical position (x: [0, 224], y: [0, 64]) -> (0, 0) is top left
    {
        {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
    },
    // Assign purpose flags to indices
    {
        LED_FLAG_UNDERGLOW,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT
    },
};


// =========================================================================
// CDC Receive Logic (Filename + Size Header + Direct LFS Write)
// =========================================================================

#if defined(VIRTSER_ENABLE) && defined(LITTLEFS_ENABLE) // Ensure Virtser (CDC) and LFS are enabled

// --- Configuration ---
#define MAX_FILENAME_LEN 64 // Maximum allowed filename length (including null terminator)

// --- State Machine ---
typedef enum {
    CDC_STATE_WAITING_FOR_FILENAME,
    CDC_STATE_RECEIVING_FILENAME,
    CDC_STATE_WAITING_FOR_SIZE,
    CDC_STATE_RECEIVING_SIZE,
    CDC_STATE_RECEIVING_DATA
} CdcReceiveState;

// --- Static Variables ---
static CdcReceiveState cdc_state = CDC_STATE_WAITING_FOR_FILENAME; // Start waiting for filename

// Filename reception
static char     cdc_target_filename[MAX_FILENAME_LEN];
static uint8_t  filename_index = 0;

// Size reception
static uint8_t  size_buffer[4];
static uint8_t  size_buffer_index = 0;
static uint32_t expected_data_size = 0;

// Data reception / LFS writing
static uint32_t   received_data_count = 0;
static lfs_file_t cdc_current_file;
static bool       cdc_file_is_open = false; // Track if the file is open

// Function to reset the state machine completely
static void reset_cdc_state(void) {
    uprintf("CDC: Resetting state machine.\n");
    if (cdc_file_is_open) {
        uprintf("CDC: Closing potentially open file during reset.\n");
        int close_err = lfs_file_close(&lfs, &cdc_current_file);
        if (close_err < 0) {
            uprintf("CDC: Error closing file during reset: %d\n", close_err);
        }
        cdc_file_is_open = false;
    }
    cdc_state = CDC_STATE_WAITING_FOR_FILENAME;
    filename_index = 0;
    size_buffer_index = 0;
    expected_data_size = 0;
    received_data_count = 0;
    memset(cdc_target_filename, 0, MAX_FILENAME_LEN);
    memset(size_buffer, 0, sizeof(size_buffer));
}

// --- virtser_recv Implementation ---
// This is called for EACH byte received over the CDC serial port.
void virtser_recv(const uint8_t ch) {
    int lfs_err;

    switch (cdc_state) {
        case CDC_STATE_WAITING_FOR_FILENAME:
            // First byte received marks the start of the filename
            // uprintf("CDC: S_WAIT_FN: Got first byte 0x%02X, starting filename receive.\n", ch); // Debug
            memset(cdc_target_filename, 0, MAX_FILENAME_LEN); // Clear buffer for new name
            filename_index = 0;
            cdc_state = CDC_STATE_RECEIVING_FILENAME;
            // Fall through to process this first byte in the new state immediately
            __attribute__((fallthrough)); // Explicit fallthrough annotation

        case CDC_STATE_RECEIVING_FILENAME:
            if (ch == '\0') { // Null terminator marks end of filename
                cdc_target_filename[filename_index] = '\0'; // Ensure null termination
                uprintf("CDC: S_RECV_FN: Received Filename: '%s'\n", cdc_target_filename);

                // Basic filename validation (optional, but good practice)
                if (filename_index == 0) {
                    uprintf("CDC: ERROR - Received empty filename. Resetting.\n");
                    reset_cdc_state();
                    return;
                }
                // Add more checks? (e.g., invalid characters '/')

                // Transition to waiting for size
                cdc_state = CDC_STATE_WAITING_FOR_SIZE;
                size_buffer_index = 0; // Reset size buffer index
                memset(size_buffer, 0, sizeof(size_buffer));
                uprintf("CDC: S_RECV_FN: Transitioning to S_WAIT_SIZE.\n");

            } else if (filename_index < MAX_FILENAME_LEN - 1) {
                // Store the character if space allows (leave room for null terminator)
                cdc_target_filename[filename_index++] = (char)ch;
                // uprintf("CDC: S_RECV_FN[%d]: Got char '%c' (0x%02X)\n", filename_index - 1, ch, ch); // Debug

            } else {
                // Filename buffer overflow
                uprintf("CDC: ERROR - Filename received exceeds buffer size (%d). Resetting.\n", MAX_FILENAME_LEN);
                reset_cdc_state();
                // Don't process this character further
            }
            break;

        case CDC_STATE_WAITING_FOR_SIZE:
            // Start collecting the 4 size bytes
            // uprintf("CDC: S_WAIT_SIZE: Starting size receive.\n"); // Debug
            size_buffer_index = 0;
            memset(size_buffer, 0, sizeof(size_buffer));
            cdc_state = CDC_STATE_RECEIVING_SIZE;
            // Fall through to process this first byte
             __attribute__((fallthrough)); // Explicit fallthrough annotation

        case CDC_STATE_RECEIVING_SIZE:
            if (size_buffer_index < 4) {
                // uprintf("CDC: S_RECV_SIZE[%d]: Got byte 0x%02X\n", size_buffer_index, ch); // Debug
                size_buffer[size_buffer_index++] = ch;

                if (size_buffer_index == 4) {
                    // Reconstruct size (Little Endian)
                    expected_data_size = (uint32_t)size_buffer[0] |
                                         ((uint32_t)size_buffer[1] << 8) |
                                         ((uint32_t)size_buffer[2] << 16) |
                                         ((uint32_t)size_buffer[3] << 24);

                    uprintf("CDC: S_RECV_SIZE: Reconstructed size: %lu bytes\n", expected_data_size);

                    // --- Validate Size ---
                    if (expected_data_size == 0) {
                        // Handle 0-byte file: create/truncate and finish
                        uprintf("CDC: Received size 0. Creating/truncating file '%s'.\n", cdc_target_filename);
                        lfs_err = lfs_file_open(&lfs, &cdc_current_file, cdc_target_filename, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
                        if (lfs_err < 0) {
                             uprintf("CDC: ERROR - Failed to open/truncate 0-byte file! LFS Error: %d. Resetting.\n", lfs_err);
                        } else {
                             lfs_err = lfs_file_close(&lfs, &cdc_current_file); // Close immediately
                             if (lfs_err < 0) {
                                 uprintf("CDC: ERROR - Failed to close 0-byte file! LFS Error: %d.\n", lfs_err);
                             } else {
                                 uprintf("CDC: 0-byte file '%s' processed.\n", cdc_target_filename);
                             }
                        }
                        reset_cdc_state(); // Reset for next transfer regardless of close error
                        return;

                    } else {
                        // --- Size > 0: Try to Open File for Writing ---
                        uprintf("CDC: Attempting to open file '%s' for writing...\n", cdc_target_filename);
                        lfs_err = lfs_file_open(&lfs, &cdc_current_file, cdc_target_filename, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);

                        if (lfs_err < 0) {
                            uprintf("CDC: ERROR - Failed to open file '%s'! LFS Error: %d. Resetting.\n", cdc_target_filename, lfs_err);
                            reset_cdc_state();
                        } else {
                            uprintf("CDC: File '%s' opened successfully. Switching to S_RECV_DATA state.\n", cdc_target_filename);
                            cdc_file_is_open = true;
                            received_data_count = 0; // Reset data counter
                            cdc_state = CDC_STATE_RECEIVING_DATA; // *** Change State ***
                        }
                    }
                }
                // else: Size header not yet complete, continue collecting bytes...
            }
            // else: Should not happen if index logic is correct
            break;

        case CDC_STATE_RECEIVING_DATA:
            if (!cdc_file_is_open) {
                // Safety check: Should not be in this state if file isn't tracked as open
                uprintf("CDC: ERROR - State mismatch (S_RECV_DATA but file not tracked as open)! Resetting state.\n");
                reset_cdc_state();
                return; // Exit function for this byte
            }

            // Write received byte directly to the opened file
            lfs_ssize_t written = lfs_file_write(&lfs, &cdc_current_file, &ch, 1); // Write single byte

            if (written < 0) {
                uprintf("CDC: ERROR - Failed to write byte to file! LFS Error: %ld. Resetting.\n", written);
                reset_cdc_state(); // Includes closing the file
                return;
            } else if (written != 1) {
                 uprintf("CDC: ERROR - Failed to write byte (wrote %ld instead of 1). Resetting.\n", written);
                 reset_cdc_state(); // Includes closing the file
                 return;
            }

            // Increment counter AFTER successful write
            received_data_count++;

            // Optional: Progress indicator (can slow things down if too frequent)
            // if (received_data_count % 4096 == 0) {
            //     uprintf("CDC: Received %lu / %lu bytes\n", received_data_count, expected_data_size);
            // }

            // Check if this was the *last* byte
            if (received_data_count == expected_data_size) {
                uprintf("CDC: OK - Received final byte. Total %lu bytes written to '%s'.\n", received_data_count, cdc_target_filename);

                // --- Sync and Close the File ---
                uprintf("CDC: Syncing file...\n");
                lfs_err = lfs_file_sync(&lfs, &cdc_current_file);
                if (lfs_err < 0) {
                     uprintf("CDC: ERROR - Failed to sync file! LFS Error: %d\n", lfs_err);
                     // Continue to close attempt anyway
                }

                uprintf("CDC: Closing file '%s'.\n", cdc_target_filename);
                lfs_err = lfs_file_close(&lfs, &cdc_current_file);
                // Mark file closed *before* resetting state, even if close fails
                cdc_file_is_open = false;
                if (lfs_err < 0) {
                    uprintf("CDC: ERROR - Failed to close file! LFS Error: %d\n", lfs_err);
                }

                // --- Reset state for the next transfer ---
                uprintf("CDC: Transfer complete. Resetting to S_WAIT_FN state.\n");
                cdc_state = CDC_STATE_WAITING_FOR_FILENAME; // Ready for next filename
                // Reset other variables just in case
                filename_index = 0;
                size_buffer_index = 0;
                expected_data_size = 0;
                received_data_count = 0;
                memset(cdc_target_filename, 0, MAX_FILENAME_LEN);
                memset(size_buffer, 0, sizeof(size_buffer));

            } else if (received_data_count > expected_data_size) {
                 // This should theoretically not happen if expected_data_size was correct
                 uprintf("CDC: ERROR - Received MORE data than expected (%lu > %lu)! Resetting.\n", received_data_count, expected_data_size);
                 reset_cdc_state(); // Includes closing file
            }
            // else: More data bytes needed for this block... keep receiving
            break;

        default:
            // Invalid state - Should never happen
            uprintf("CDC: FATAL - Invalid state (%d)! Resetting state.\n", cdc_state);
            reset_cdc_state(); // Includes closing file if open
            break;
    }
}

#endif // VIRTSER_ENABLE && LITTLEFS_ENABLE check for CDC receive logic


// =========================================================================
// Dynamic Gradient Drawing
// =========================================================================

#ifdef QUANTUM_PAINTER_ENABLE // Only define if painter is enabled

//------------------------------------------------------------------------------
// Helper function to generate a colored gradient in RGB565 format.
// This function creates a 128x128 gradient and displays it via LVGL.
//------------------------------------------------------------------------------
static void draw_gradient(void) {
    uprintf("Drawing dynamic gradient...\n");
    const int width = 128;
    const int height = 128;
    // Allocate buffer in static RAM (32KB) - too big for stack
    static uint16_t gradient_buffer[128 * 128];

    // Populate buffer with gradient data
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t r5 = (x * 31) / (width - 1);
            uint8_t g6 = ((x + y) * 63) / (width + height - 2);
            uint8_t b5 = (y * 31) / (height - 1);
            // Store as native uint16_t (LVGL/driver handles byte order if needed)
            gradient_buffer[y * width + x] = (r5 << 11) | (g6 << 5) | b5;
        }
    }
    uprintf("Gradient buffer populated.\n");

    // --- Setup LVGL Image Descriptor (must also be static) ---
    static lv_img_dsc_t gradient_img_dsc;
    gradient_img_dsc.header.always_zero = 0;
    gradient_img_dsc.header.w = width;
    gradient_img_dsc.header.h = height;
    gradient_img_dsc.data_size = width * height * sizeof(uint16_t);
    // Assuming LV_COLOR_DEPTH is 16. Check lv_conf.h if issues.
    gradient_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    gradient_img_dsc.data = (const uint8_t *)gradient_buffer;

    // --- Create LVGL Image Widget ---
    lv_obj_t *img_widget = lv_img_create(lv_scr_act()); // Get the active screen
    if (img_widget) {
        uprintf("Setting gradient image source...\n");
        lv_img_set_src(img_widget, &gradient_img_dsc); // Point widget to static descriptor
        lv_obj_align(img_widget, LV_ALIGN_CENTER, 0, 0); // Center it
        uprintf("Gradient image displayed.\n");
    } else {
        uprintf("ERROR: Failed to create LVGL image widget for gradient!\n");
    }
}
//------------------------------------------------------------------------------

#endif // QUANTUM_PAINTER_ENABLE check for gradient drawing

// =========================================================================
// Initialization and Other Callbacks
// =========================================================================

#ifdef VIA_ENABLE // Only define board_init etc. if VIA (or maybe just base QMK) needs them

void board_init(void) {
    // Keep this minimal if keyboard_post_init_kb handles major init
    uprintf("board_init() called.\n");
}

#ifdef QUANTUM_PAINTER_ENABLE // ui_init only needed if Painter enabled
// Define QP/LVGL related static variables only if painter is enabled
static painter_device_t oled;

__attribute__((weak)) void ui_init(void) {
    uprintf("ui_init() called.\n"); // Added print
    // Ensure GPIO pins are defined (GP0, GP1 should be available via platform headers)
    oled = qp_gc9107_make_spi_device(128, 128, 0xFF, OLED_DC_PIN, 0xFF, 8, 0);
    qp_init(oled, QP_ROTATION_180);
    qp_power(oled, true); // Turn on display

    // --- Attach LVGL first ---
    bool lvgl_attached = false;
#ifdef LITTLEFS_ENABLE
    volatile lv_fs_drv_t *result = NULL; // Initialize to NULL
    if (qp_lvgl_attach(oled)) {
        lvgl_attached = true; // Mark as attached
        uprintf("Attempting to attach LFS to LVGL...\n");
        result = lv_fs_littlefs_set_driver(LV_FS_LITTLEFS_LETTER, &lfs); // Assign LFS driver to LVGL
        if (result == NULL) {
            uprintf("Error attaching LFS to LVGL\n");
        } else {
             uprintf("LVGL attached to LFS driver successfully (Drive %c:).\n", LV_FS_LITTLEFS_LETTER);
        }
    } else {
        uprintf("Failed to attach LVGL to painter.\n");
    }
#else
    // Attach LVGL even if LFS is disabled
    if (qp_lvgl_attach(oled)) {
        lvgl_attached = true;
         uprintf("LVGL attached (no LFS).\n");
    } else {
        uprintf("Failed to attach LVGL to painter.\n");
    }
#endif // LITTLEFS_ENABLE check for LVGL FS

    // --- Call draw_gradient AFTER LVGL is attached ---
    // if (lvgl_attached) {
    //     draw_gradient(); // Call the function to generate and display the gradient
    // } else {
    //     uprintf("Skipping gradient draw because LVGL failed to attach.\n");
    // }
}
#endif // QUANTUM_PAINTER_ENABLE check for ui_init

// keyboard_post_init_kb is a good place for LFS mount and final setup
void keyboard_post_init_kb(void) {
    uprintf("keyboard_post_init_kb called.\n");

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
    reset_cdc_state(); // <--- Use the reset function to ensure clean start
    #endif

    // 4) Call the default post-init user function if it exists
    keyboard_post_init_user(); // Weakly defined, safe to call
    uprintf("keyboard_post_init_kb finished.\n");
}

// This function handles Raw HID commands coming *from Via*
void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    // uprintf("via_custom_value_command_kb called, length %d\n", length);
    #ifdef LITTLEFS_ENABLE // Raw HID parser likely needs LFS too
        int err = module_raw_hid_parse_packet(data, length);
        if (err < 0) {
            // uprintf("Error parsing Raw HID packet via VIA: %d\n", err);
        }
    #else
        // uprintf("via_custom_value_command_kb: LFS disabled, skipping Raw HID parse.\n");
    #endif
}
#endif //VIA_ENABLE

// =========================================================================
// Includes
// =========================================================================

// --- Core QMK Includes ---
#include "quantum.h" // Includes core QMK functionality, ChibiOS, config files etc.
#include "print.h"   // For uprintf

// --- Feature Includes ---
#include <stdint.h>
#include <stdbool.h>
#include <string.h> // For memset if used

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

#include "platforms/chibios/gpio.h" // For direct GPIO manipulation (setPinOutputPushPull, writePinHigh, GPx defines)
#include "virtser.h"                // For virtser_recv, virtser_send

// --- Include the header defining the global `lfs` object ---
#include "module.h" // Assumes `lfs_t lfs;` is declared here or in a common header included by module.h

// =========================================================================
// CDC Receive Logic (with LFS writing)
// =========================================================================

#if defined(CONSOLE_ENABLE) && defined(LITTLEFS_ENABLE) // Only compile CDC receive logic if CONSOLE and LFS are enabled

// --- Configuration ---
#define CDC_DATA_BUFFER_SIZE 32768 // Buffer sized for 128x128x2 bytes
#define CDC_TARGET_FILENAME "cdc_image.raw" // Hardcoded filename for CDC uploads

// --- State Machine ---
typedef enum {
    CDC_STATE_WAITING_FOR_SIZE,
    CDC_STATE_RECEIVING_DATA
} CdcReceiveState;

// --- Static Variables ---
static CdcReceiveState cdc_state = CDC_STATE_WAITING_FOR_SIZE; // Default state

static uint8_t size_buffer[4];
static uint8_t size_buffer_index = 0;

static uint32_t expected_data_size = 0;
static uint32_t received_data_count = 0;

// Buffer to hold the *entire* image data for now
static uint8_t cdc_data_buffer[CDC_DATA_BUFFER_SIZE];

// LFS file handle for the duration of the transfer
static lfs_file_t cdc_current_file;
static bool cdc_file_is_open = false; // Track if the file is open

// --- virtser_recv Implementation ---
void virtser_recv(const uint8_t ch) {
    int lfs_err; // Variable for LFS error codes

    switch (cdc_state) {
        case CDC_STATE_WAITING_FOR_SIZE:
            if (cdc_file_is_open) {
                // Safety check: Should not be waiting for size if file is open. Reset.
                uprintf("CDC: ERROR - State mismatch (S_WAIT but file open)! Closing file and resetting.\n");
                lfs_file_close(&lfs, &cdc_current_file); // Attempt close
                cdc_file_is_open = false;
                // Reset other state vars
                size_buffer_index = 0;
                expected_data_size = 0;
                received_data_count = 0;
                // Stay in WAITING state
                return; // Exit function for this byte
            }

            // Collect the 4 bytes for the size header
            if (size_buffer_index < 4) {
                 // uprintf("CDC: S_WAIT[%d]: Got byte 0x%02X\n", size_buffer_index, ch); // Reduce verbosity
                 size_buffer[size_buffer_index++] = ch;

                 if (size_buffer_index == 4) {
                    // uprintf("CDC: S_WAIT: Reconstructing size from bytes: %02X %02X %02X %02X\n",
                    //         size_buffer[0], size_buffer[1], size_buffer[2], size_buffer[3]);

                    expected_data_size = (uint32_t)size_buffer[0] |
                                         ((uint32_t)size_buffer[1] << 8) |
                                         ((uint32_t)size_buffer[2] << 16) |
                                         ((uint32_t)size_buffer[3] << 24);

                    size_buffer_index = 0; // Reset index for next potential header
                    uprintf("CDC: Reconstructed size value: %lu bytes\n", expected_data_size);

                    // --- Validate Size ---
                    if (expected_data_size == 0) {
                        uprintf("CDC: WARN - Received size 0. Ignoring header.\n");
                    } else if (expected_data_size > CDC_DATA_BUFFER_SIZE) {
                        uprintf("CDC: ERROR - Expected size (%lu) > buffer size (%d). Ignoring header.\n",
                                expected_data_size, CDC_DATA_BUFFER_SIZE);
                        expected_data_size = 0; // Clear the invalid size
                    } else {
                        // --- Size is Valid: Try to Open File ---
                        uprintf("CDC: Size OK. Attempting to open file '%s' for writing...\n", CDC_TARGET_FILENAME);
                        // Open for writing, create if doesn't exist, truncate if it does
                        lfs_err = lfs_file_open(&lfs, &cdc_current_file, CDC_TARGET_FILENAME, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);

                        if (lfs_err < 0) {
                            uprintf("CDC: ERROR - Failed to open file '%s'! LFS Error: %d. Aborting transfer.\n", CDC_TARGET_FILENAME, lfs_err);
                            expected_data_size = 0; // Invalidate size, stay in WAITING state
                        } else {
                            uprintf("CDC: File '%s' opened successfully. Switching to S_RECV state.\n", CDC_TARGET_FILENAME);
                            cdc_file_is_open = true; // Mark file as open
                            received_data_count = 0; // Reset data counter
                            cdc_state = CDC_STATE_RECEIVING_DATA; // *** Change State ***
                        }
                    }
                }
                 // else: Size header not yet complete, continue collecting bytes...
            }
            // else: Should not happen if index logic correct, defensive reset removed for simplicity here
            break; // End WAITING_FOR_SIZE case

        case CDC_STATE_RECEIVING_DATA:
            if (!cdc_file_is_open) {
                // Safety check: Should not be in this state if file isn't tracked as open
                uprintf("CDC: ERROR - State mismatch (S_RECV but file not tracked as open)! Resetting state.\n");
                cdc_state = CDC_STATE_WAITING_FOR_SIZE;
                size_buffer_index = 0;
                expected_data_size = 0;
                received_data_count = 0;
                return; // Exit function for this byte
            }

            // Collect data bytes until expected size is reached
            if (received_data_count < expected_data_size) {
                 // Store the byte in the RAM buffer (bounds check already done)
                 cdc_data_buffer[received_data_count] = ch;
                 received_data_count++;

                // Check if this was the *last* byte
                if (received_data_count == expected_data_size) {
                    uprintf("CDC: OK - Received expected %lu data bytes into RAM buffer.\n", received_data_count);

                    // --- Write Buffered Data to Flash ---
                    uprintf("CDC: Attempting to write %lu bytes from buffer to file '%s'...\n", expected_data_size, CDC_TARGET_FILENAME);
                    lfs_ssize_t written = lfs_file_write(&lfs, &cdc_current_file, cdc_data_buffer, expected_data_size);

                    if (written < 0) {
                        uprintf("CDC: ERROR - Failed to write to file! LFS Error: %ld\n", written);
                    } else if ((uint32_t)written != expected_data_size) {
                        uprintf("CDC: ERROR - Partial write! Wrote %ld bytes, expected %lu.\n", written, expected_data_size);
                    } else {
                        uprintf("CDC: Successfully wrote %ld bytes to file.\n", written);
                        // Optional: Sync data to flash before closing (safer but slower)
                        // uprintf("CDC: Syncing file...\n");
                        // lfs_err = lfs_file_sync(&lfs, &cdc_current_file);
                        // if (lfs_err < 0) {
                        //     uprintf("CDC: ERROR - Failed to sync file! LFS Error: %d\n", lfs_err);
                        // }
                    }

                    // --- Close the File ---
                    uprintf("CDC: Closing file '%s'.\n", CDC_TARGET_FILENAME);
                    lfs_err = lfs_file_close(&lfs, &cdc_current_file);
                    cdc_file_is_open = false; // Mark file as closed *before* changing state
                    if (lfs_err < 0) {
                        uprintf("CDC: ERROR - Failed to close file! LFS Error: %d\n", lfs_err);
                    }

                    // --- Reset state for the next transfer ---
                    uprintf("CDC: Block complete. Resetting to S_WAIT state.\n");
                    cdc_state = CDC_STATE_WAITING_FOR_SIZE;
                    size_buffer_index = 0;
                    expected_data_size = 0;
                    received_data_count = 0;
                }
                // else: More data bytes needed for this block... keep receiving
            } else {
                // Received data when we shouldn't have (already got expected_data_size)
                uprintf("CDC: WARN - Received unexpected extra data byte (0x%02X) in S_RECV state! Closing file & Resetting.\n", ch);

                // Attempt to close the file cleanly before resetting
                if (cdc_file_is_open) {
                    lfs_file_close(&lfs, &cdc_current_file);
                    cdc_file_is_open = false;
                }

                // Reset state machine
                cdc_state = CDC_STATE_WAITING_FOR_SIZE;
                expected_data_size = 0;
                received_data_count = 0;

                // Assume this byte might be the start of the next header
                size_buffer[0] = ch;
                size_buffer_index = 1;
            }
            break; // End RECEIVING_DATA case

        default:
            // Invalid state - Should never happen
            uprintf("CDC: FATAL - Invalid state (%d)! Resetting state.\n", cdc_state);
            if (cdc_file_is_open) { // Attempt close if file was somehow open
                lfs_file_close(&lfs, &cdc_current_file);
                cdc_file_is_open = false;
            }
            cdc_state = CDC_STATE_WAITING_FOR_SIZE;
            size_buffer_index = 0;
            expected_data_size = 0;
            received_data_count = 0;
            break;
    }
}

#endif // CONSOLE_ENABLE && LITTLEFS_ENABLE check for CDC receive logic

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
    if (lvgl_attached) {
        draw_gradient(); // Call the function to generate and display the gradient
    } else {
        uprintf("Skipping gradient draw because LVGL failed to attach.\n");
    }
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
        // Use literal value for reservation KB from rules.mk
        uint32_t total_blocks = (PICO_FLASH_SIZE_BYTES / 1024 - 1024) / 4; // Use literal 1024 KB reservation
        uprintf("Estimated total LFS blocks: %lu\n", total_blocks);
        uint32_t free_blocks  = (used_blocks < total_blocks) ? (total_blocks - (uint32_t)used_blocks) : 0;
        uprintf("Estimated free space: %lu blocks => %lu bytes\n", free_blocks, free_blocks * 4096);
    } else {
         uprintf("Error getting LFS size: %ld\n", used_blocks);
    }
#endif // LITTLEFS_ENABLE check

#ifdef QUANTUM_PAINTER_ENABLE
    // 3) Initialize your display hardware and QP/LVGL
    uprintf("Initializing display hardware...\n");
    setPinOutputPushPull(OLED_BL_PIN); // GP0
    writePinHigh(OLED_BL_PIN);         // Turn backlight on
    ui_init();                         // Initialize QP/LVGL etc. which calls draw_gradient
    uprintf("Display initialized.\n");
#endif // QUANTUM_PAINTER_ENABLE

    // --- Initialize CDC Receive State ---
    #if defined(CONSOLE_ENABLE) && defined(LITTLEFS_ENABLE)
    uprintf("Initializing CDC Receive State...\n");
    cdc_state = CDC_STATE_WAITING_FOR_SIZE; // Explicitly set initial state
    size_buffer_index = 0;
    expected_data_size = 0;
    received_data_count = 0;
    cdc_file_is_open = false; // Ensure file starts as closed
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

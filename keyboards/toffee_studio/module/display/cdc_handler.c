// display/cdc_handler.c
#include "quantum.h"
#include "lfs.h"
#include "module.h" // For the global lfs object
#include "display/cdc_handler.h"

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
void cdc_handler_init(void) {
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
                    cdc_handler_init();
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
                cdc_handler_init();
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
                        cdc_handler_init(); // Reset for next transfer regardless of close error
                        return;

                    } else {
                        // --- Size > 0: Try to Open File for Writing ---
                        uprintf("CDC: Attempting to open file '%s' for writing...\n", cdc_target_filename);
                        lfs_err = lfs_file_open(&lfs, &cdc_current_file, cdc_target_filename, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);

                        if (lfs_err < 0) {
                            uprintf("CDC: ERROR - Failed to open file '%s'! LFS Error: %d. Resetting.\n", cdc_target_filename, lfs_err);
                            cdc_handler_init();
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
                cdc_handler_init();
                return; // Exit function for this byte
            }

            // Write received byte directly to the opened file
            lfs_ssize_t written = lfs_file_write(&lfs, &cdc_current_file, &ch, 1); // Write single byte

            if (written < 0) {
                uprintf("CDC: ERROR - Failed to write byte to file! LFS Error: %ld. Resetting.\n", written);
                cdc_handler_init(); // Includes closing the file
                return;
            } else if (written != 1) {
                 uprintf("CDC: ERROR - Failed to write byte (wrote %ld instead of 1). Resetting.\n", written);
                 cdc_handler_init(); // Includes closing the file
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
                 cdc_handler_init(); // Includes closing file
            }
            // else: More data bytes needed for this block... keep receiving
            break;

        default:
            // Invalid state - Should never happen
            uprintf("CDC: FATAL - Invalid state (%d)! Resetting state.\n", cdc_state);
            cdc_handler_init(); // Includes closing file if open
            break;
    }
}

#endif // VIRTSER_ENABLE && LITTLEFS_ENABLE

/* module_virtser.c
 *
 * Implements CDC (virtual serial port) data handling for high-speed image transfers.
 * When data is received on the CDC interface, QMK calls virtser_recv(), which then
 * uses the existing raw HID packet parser (module_raw_hid_parse_packet) to handle commands.
 *
 * This implementation assumes that the packet format (with header magic, command_id, etc.)
 * is the same as used for raw HID.
 */

#include "quantum.h"
#include "print.h"
#include "module.h"
#include "module_raw_hid.h"

// This function is called automatically by QMK when data arrives on the CDC interface.
void virtser_recv(uint8_t *data, uint32_t length) {
    uprintf("CDC Received packet (length: %lu bytes)\n", length);

    // Call the existing parser (from your module_raw_hid.c) to process the command.
    int result = module_raw_hid_parse_packet(data, (uint8_t)length);

    // Send back an acknowledgment (or error) over the CDC interface.
    if (result < 0) {
        const char error_response[] = "ERROR";
        uprintf("Sending error response: %s\n", error_response);
        virtser_send((const uint8_t *)error_response, sizeof(error_response) - 1);
    } else {
        const char ok_response[] = "OK";
        uprintf("Sending OK response: %s\n", ok_response);
        virtser_send((const uint8_t *)ok_response, sizeof(ok_response) - 1);
    }
}


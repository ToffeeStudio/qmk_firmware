#include "quantum.h"
#include "print.h"   // For uprintf
#include "ch.h"
#include "i2c_master.h"

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    // We are looking for a packet with our custom command.
    // data[0]: Magic (0x09)
    // data[1]: Command ID
    // data[2-5]: Packet ID (unused)
    // data[6...]: Payload
    if (length >= 7 && data[0] == 0x09) {
        switch (data[1]) {
            default:
                uprintf("DEBUG: Unknown Command ID received: 0x%02X\n", data[1]);
                break;
        }
    }
}

void keyboard_post_init_user(void) {
    chThdSleepMilliseconds(1000);
    uprint("INITIALISATION: TEST");
}

void keyboard_post_init_kb(void) {
    chThdSleepMilliseconds(1000);
    i2c_init();
    uprintf("[INITIATION]: keyboard_post_init_kb\r\n");
    keyboard_post_init_user();
}

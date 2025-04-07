/* module_virtser.c */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "ch.h"
#include "hal.h"
#include "quantum.h"
#include "print.h"
#include "virtser.h"
#include "module_virtser.h"
#include "module_raw_hid.h"
#include "module.h"
#include "usb_descriptor.h"

void virtser_recv(uint8_t *data, uint32_t length) {
    uprintf("CDC Received packet (length: %lu bytes)\n", (unsigned long)length);
    virtser_send(data, length);
}


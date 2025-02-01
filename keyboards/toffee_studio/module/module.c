#include <stdio.h>
#include <string.h>
#include "print.h"
#include "quantum.h"
#include "file_system.h"
#include "lfs.h"
#include "qp.h"
#include "qp_gc9107.h"
#include "graphics/thintel15.qff.c"
#include "platforms/chibios/gpio.h"
#include "qp_lvgl.h"
#include "lvgl.h"

#include "module.h"
#include "module_raw_hid.h"

#ifdef VIA_ENABLE

void board_init(void) {
		// DON'T USE THIS FUNCTION FOR NOW
		return;
		uprintf("CALLED HERE CALLED HERE\n");
    rp2040_mount_lfs(&lfs);

		// Print how many blocks are used at mount
    lfs_ssize_t used_blocks = lfs_fs_size(&lfs);
    uprintf("At boot: LFS used blocks = %ld\n", used_blocks);

    // Let's also do a quick check with parse_flash_remaining logic:
    uint32_t total_blocks = 4096; // For 16MB at 4KB block
    uint32_t free_blocks = (used_blocks < total_blocks)
                           ? (total_blocks - (uint32_t)used_blocks)
                           : 0;
    uprintf("At boot: free blocks = %lu => %lu bytes\n",
            free_blocks, free_blocks * 4096);

    debug_enable   = true;
    debug_matrix   = true;
    debug_keyboard = true;
    debug_mouse    = true;
}

static painter_device_t      oled;
static painter_font_handle_t font;

#include "qp_comms.h"
#include "qp_gc9xxx_opcodes.h"

__attribute__((weak)) void ui_init(void) {
    oled = qp_gc9107_make_spi_device(128, 128, 0xFF, OLED_DC_PIN, 0xFF, 8, 0);

    qp_init(oled, QP_ROTATION_180);

    qp_power(oled, true);

    volatile lv_fs_drv_t *result;

    if (qp_lvgl_attach(oled)) {
        result = lv_fs_littlefs_set_driver(LV_FS_LITTLEFS_LETTER, &lfs);
        if (result == NULL) {
            uprintf("Error mounting LFS\n");
        }
    }
}

#ifdef QUANTUM_PAINTER_ENABLE
void keyboard_post_init_kb(void) {
    uprintf("keyboard_post_init_kb called\n");

    // 1) Mount LFS here:
    rp2040_mount_lfs(&lfs);

    // 2) Optional debug prints:
    lfs_ssize_t used_blocks = lfs_fs_size(&lfs);
    uprintf("At boot: LFS used blocks = %ld\n", used_blocks);

    uint32_t total_blocks = 4096; // For a 16MB partition with 4KB blocks
    uint32_t free_blocks  = (used_blocks < total_blocks)
                            ? (total_blocks - (uint32_t)used_blocks)
                            : 0;
    uprintf("At boot: free blocks = %lu => %lu bytes\n",
            free_blocks, free_blocks * 4096);

    // 3) Initialize your display
    setPinOutputPushPull(OLED_BL_PIN);
    writePinHigh(OLED_BL_PIN);
    ui_init();

    // 4) Call the default post-init
    keyboard_post_init_user();
}
#endif //QUANTUM_PAINTER_ENABLE

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    int err;

    err = module_raw_hid_parse_packet(data, length);
    if (err < 0) {
        uprintf("Error parsing packet: %d\n", err);
    }
}
#endif

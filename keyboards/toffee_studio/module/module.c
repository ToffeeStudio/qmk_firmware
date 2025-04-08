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


// --- VIRTSER RECEIVE FUNCTIONALITY -------

#define RECEIVE_BUFFER_SIZE 64
static uint8_t cdc_receive_buffer[RECEIVE_BUFFER_SIZE];
static uint16_t cdc_buffer_index = 0;

void virtser_recv(const uint8_t ch) {
    if (cdc_buffer_index < RECEIVE_BUFFER_SIZE) {
        cdc_receive_buffer[cdc_buffer_index] = ch;
        cdc_buffer_index++;
        if (cdc_buffer_index == RECEIVE_BUFFER_SIZE || ch == '\n') {
            uprintf("[CDC]: Buffer filled (%u bytes):\n", cdc_buffer_index);
            chThdSleepMilliseconds(5);
            for (uint16_t i = 0; i < cdc_buffer_index; i++) {
                uprintf("%02X ", cdc_receive_buffer[i]);
                chThdSleepMilliseconds(1);
            }
            uprintf("\n");
            chThdSleepMilliseconds(5);
        }
    }
}

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

//------------------------------------------------------------------------------
// New helper function to generate a colored gradient in RGB565 format.
// This function creates a 128x128 gradient and displays it via LVGL.
static void draw_gradient(void) {
    const int width = 128;
    const int height = 128;
    static uint16_t gradient_buffer[128 * 128];
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Create a diagonal gradient:
            // red increases with x, blue increases with y, green is a mix.
            uint8_t red   = (x * 31) / (width - 1);             // 5-bit red
            uint8_t green = ((x + y) * 63) / (width + height - 2); // 6-bit green
            uint8_t blue  = (y * 31) / (height - 1);              // 5-bit blue
            uint16_t color = (red << 11) | (green << 5) | blue;    // Pack into RGB565
            gradient_buffer[y * width + x] = color;
        }
    }

    static lv_img_dsc_t gradient_img;
    gradient_img.header.always_zero = 0;
    gradient_img.header.w = width;
    gradient_img.header.h = height;
    gradient_img.data_size = width * height * 2;  // 2 bytes per pixel
    gradient_img.header.cf = LV_IMG_CF_TRUE_COLOR;
    gradient_img.data = (const uint8_t *)gradient_buffer;

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &gradient_img);
}

#include "qp_comms.h"
#include "qp_gc9xxx_opcodes.h"

//------------------------------------------------------------------------------
// Override ui_init() to initialize the display and show the gradient.
__attribute__((weak)) void ui_init(void) {
    // Initialize the OLED display.
    oled = qp_gc9107_make_spi_device(128, 128, 0xFF, OLED_DC_PIN, 0xFF, 8, 0);
    qp_init(oled, QP_ROTATION_180);
    qp_power(oled, true);

    // Attach LVGL to the OLED.
    qp_lvgl_attach(oled);

    // Draw and display the colored gradient.
    draw_gradient();
}

#ifdef QUANTUM_PAINTER_ENABLE
void keyboard_post_init_kb(void) {
    virtser_send((const uint8_t*)"Hello CDC!\n", 11);
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


#include "quantum.h"
#include "print.h"   // For uprintf
#include "ch.h"

#include "lvgl.h"
#include "qp.h"
#include "qp_gc9107.h"
#include "qp_lvgl.h"


void ui_display_gradient(void) {
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

static painter_device_t oled;

void keyboard_post_init_kb(void) {
    chThdSleepMilliseconds(3000); uprintf("PRINT TESTING\n");


    setPinOutputPushPull(0);
    writePinHigh(0);

    oled = qp_gc9107_make_spi_device(128, 128, 0xFF, OLED_DC_PIN, 0xFF, 8, 0);

    chThdSleepMilliseconds(1000); uprintf("qp_init(oled, QP_ROTATION_180);\n");
    // --------------------
    qp_init(oled, QP_ROTATION_180);
    // --------------------

    chThdSleepMilliseconds(1000); uprintf("qp_power(oled, true);\n");
    // --------------------
    qp_power(oled, true); // Turn on display
    // --------------------

    if (qp_lvgl_attach(oled)) {
        uprintf("LVGL attached to the screen\n");
        chThdSleepMilliseconds(1000);
        uprintf("ui_display_gradient();\n");
        // --------------------
        ui_display_gradient(); // Call the function to generate and display the gradient
        // --------------------
    }
}


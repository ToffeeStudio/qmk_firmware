#include "quantum.h"
#include "lfs.h"
#include "lvgl.h"
#include "qp.h"
#include "qp_comms.h"
#include "qp_gc9107.h"
#include "qp_gc9107_opcodes.h"
#include "qp_gc9xxx_opcodes.h"
#include "qp_lvgl.h"
#include "module.h" // For the global lfs object
#include "display/ui.h"
#include "display/animation.h" // For frame_buffers and images

// Define QP/LVGL related static variables
static painter_device_t oled;

#define GC9107_CMD_NORMAL_DISPLAY_ON 0x13
#define GC9107_SLEEP_OUT_DELAY_MS 250
#define GC9107_DISPLAY_ON_DELAY_MS 80

bool qp_gc9107_init(painter_device_t device, painter_rotation_t rotation) {
    const uint8_t gc9107_init_sequence[] = {
        GC9XXX_SET_INTER_REG_ENABLE1, 5, 0,
        GC9XXX_SET_INTER_REG_ENABLE2, 5, 0,
        GC9107_SET_FUNCTION_CTL1, 0, 1, GC9107_ALLOW_SET_VGH_VGL_CLK,
        GC9107_SET_FUNCTION_CTL2, 0, 1, GC9107_ALLOW_SET_VGH | GC9107_ALLOW_SET_VGL,
        GC9107_SET_FUNCTION_CTL3, 0, 1, GC9107_ALLOW_SET_GAMMA1 | GC9107_ALLOW_SET_GAMMA2,
        GC9107_SET_FUNCTION_CTL6, 0, 1, GC9107_ALLOW_SET_COMPLEMENT_RGB | 0x08 | GC9107_ALLOW_SET_FRAMERATE,
        GC9107_SET_COMPLEMENT_RGB, 0, 1, GC9107_COMPLEMENT_WITH_LSB,
        GC9107_SET_VGH, 0, 1, 0x23,
        GC9107_SET_VGL, 0, 1, 0x47,
        GC9107_SET_VGH_VGL_CLK, 0, 1, 0x99,
        0xAB, 0, 1, 0x0E,
        GC9107_SET_FRAME_RATE, 0, 1, 0x19,
        GC9XXX_SET_PIXEL_FORMAT, 0, 1, GC9107_PIXEL_FORMAT_16_BPP_IFPF,
        GC9XXX_SET_GAMMA1, 0, 14, 0x05, 0x1D, 0x51, 0x2F, 0x85, 0x2A, 0x11, 0x62, 0x00, 0x07, 0x07, 0x0F, 0x08, 0x1F,
        GC9XXX_SET_GAMMA2, 0, 14, 0x2E, 0x41, 0x62, 0x56, 0xA5, 0x3A, 0x3F, 0x60, 0x0F, 0x07, 0x0A, 0x18, 0x18, 0x1D,
        GC9XXX_CMD_SLEEP_OFF, GC9107_SLEEP_OUT_DELAY_MS, 0,
        GC9107_CMD_NORMAL_DISPLAY_ON, 0, 0,
        GC9XXX_CMD_DISPLAY_ON, GC9107_DISPLAY_ON_DELAY_MS, 0,
    };

    qp_comms_bulk_command_sequence(device, gc9107_init_sequence, sizeof(gc9107_init_sequence));

    const uint8_t madctl[] = {
        [QP_ROTATION_0]   = GC9XXX_MADCTL_BGR,
        [QP_ROTATION_90]  = GC9XXX_MADCTL_BGR | GC9XXX_MADCTL_MX | GC9XXX_MADCTL_MV,
        [QP_ROTATION_180] = GC9XXX_MADCTL_BGR | GC9XXX_MADCTL_MX | GC9XXX_MADCTL_MY,
        [QP_ROTATION_270] = GC9XXX_MADCTL_BGR | GC9XXX_MADCTL_MV | GC9XXX_MADCTL_MY,
    };
    qp_comms_command_databyte(device, GC9XXX_SET_MEM_ACS_CTL, madctl[rotation]);

    return true;
}

void ui_retry_wake_tail(void) {
    uprintf("[DISPLAY WAKE RETRY]: sending 11h -> 13h -> 29h tail.\n");
    if (!qp_comms_start(oled)) {
        uprintf("[DISPLAY WAKE RETRY]: failed to start comms.\n");
        return;
    }

    qp_comms_command(oled, GC9XXX_CMD_SLEEP_OFF);
    wait_ms(GC9107_SLEEP_OUT_DELAY_MS);
    qp_comms_command(oled, GC9107_CMD_NORMAL_DISPLAY_ON);
    qp_comms_command(oled, GC9XXX_CMD_DISPLAY_ON);
    wait_ms(GC9107_DISPLAY_ON_DELAY_MS);
    qp_comms_stop(oled);

    lv_obj_invalidate(lv_scr_act());
}

void ui_init(void) {
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
        ui_display_gradient(); // Call the function to generate and display the gradient
    } else {
        uprintf("Skipping gradient draw because LVGL failed to attach.\n");
    }
}

void ui_reinit_display(void) {
    uprintf("[DISPLAY RE-INIT]: ui_reinit_display() called.\n");
    qp_init(oled, QP_ROTATION_180);
    qp_power(oled, true);
    lv_obj_invalidate(lv_scr_act());
}

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

int ui_display_static_image(const char *path) {
    lfs_file_t file;
    int err = lfs_file_open(&lfs, &file, path, LFS_O_RDONLY);
    if (err < 0) {
        uprintf("Error opening image file: %d\n", err);
        return err;
    }

    // Read into the first buffer, which is declared in animation.c
    lfs_ssize_t bytes_read = lfs_file_read(&lfs, &file, frame_buffers[0], FRAME_SIZE);
    if (bytes_read < 0) {
        uprintf("Error reading image file: %ld\n", bytes_read);
        lfs_file_close(&lfs, &file);
        return bytes_read;
    }

    lfs_file_close(&lfs, &file);

    // Create and display static image
    // Note: This creates a new widget each time. For performance, you might
    // reuse a single image widget and just change its source.
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &images[0]); // Uses the image descriptor from animation.c

    return 0; // Success
}

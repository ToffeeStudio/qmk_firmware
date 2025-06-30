#include "quantum.h"
#include "lfs.h"
#include "lvgl.h"
#include "qp.h"
#include "qp_gc9107.h"
#include "qp_lvgl.h"
#include "module.h" // For the global lfs object
#include "display/ui.h"
#include "display/animation.h" // For frame_buffers and images

// Define QP/LVGL related static variables
static painter_device_t oled;

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
    // if (lvgl_attached) {
    //     ui_display_gradient(); // Call the function to generate and display the gradient
    // } else {
    //     uprintf("Skipping gradient draw because LVGL failed to attach.\n");
    // }
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

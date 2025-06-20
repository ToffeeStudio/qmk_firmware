#include "quantum.h"
#include "print.h"   // For uprintf
#include "ch.h"
#include "rgb_matrix.h"

led_config_t g_led_config = {
    /* Key-matrix → LED index
     * 9 rows × 8 cols  (COL2ROW diode dir, see info.json)
     * Placeholder mapping for 68 keys, assuming a dense layout.
     * Key-light LEDs are indexed from 22 to 89.
     */
    {
        // This is a generic placeholder and will need to be adjusted for the real layout.
        { 22, 23, 24, 25, 26, 27, 28, 29 }, // row 0 (8 keys) -> LEDs 22-29
        { 30, 31, 32, 33, 34, 35, 36, 37 }, // row 1 (8 keys) -> LEDs 30-37
        { 38, 39, 40, 41, 42, 43, 44, 45 }, // row 2 (8 keys) -> LEDs 38-45
        { 46, 47, 48, 49, 50, 51, 52, 53 }, // row 3 (8 keys) -> LEDs 46-53
        { 54, 55, 56, 57, 58, 59, 60, 61 }, // row 4 (8 keys) -> LEDs 54-61
        { 62, 63, 64, 65, 66, 67, 68, 69 }, // row 5 (8 keys) -> LEDs 62-69
        { 70, 71, 72, 73, 74, 75, 76, 77 }, // row 6 (8 keys) -> LEDs 70-77
        { 78, 79, 80, 81, 82, 83, 84, 85 }, // row 7 (8 keys) -> LEDs 78-85
        { 86, 87, 88, 89, NO_LED, NO_LED, NO_LED, NO_LED }  // row 8 (4 keys) -> LEDs 86-89
    },

    /* Physical XY positions (90 total LEDs) */
    {
        // Underglow (22 LEDs) - a rectangle around the board
        // Top edge (11 LEDs)
        {   0,  -10 }, {  18,  -10 }, {  36,  -10 }, {  54,  -10 }, {  72,  -10 }, {  90,  -10 }, { 108,  -10 }, { 126,  -10 }, { 144,  -10 }, { 162,  -10 }, { 180,  -10 },
        // Bottom edge (11 LEDs)
        {   0,  190 }, {  18,  190 }, {  36,  190 }, {  54,  190 }, {  72,  190 }, {  90,  190 }, { 108,  190 }, { 126,  190 }, { 144,  190 }, { 162,  190 }, { 180,  190 },

        // Keylights (68 LEDs) - based on the 9x8 matrix above (pitch: 20)
        // Row 0
        { 20, 0 }, { 40, 0 }, { 60, 0 }, { 80, 0 }, { 100, 0 }, { 120, 0 }, { 140, 0 }, { 160, 0 },
        // Row 1
        { 20, 20 }, { 40, 20 }, { 60, 20 }, { 80, 20 }, { 100, 20 }, { 120, 20 }, { 140, 20 }, { 160, 20 },
        // Row 2
        { 20, 40 }, { 40, 40 }, { 60, 40 }, { 80, 40 }, { 100, 40 }, { 120, 40 }, { 140, 40 }, { 160, 40 },
        // Row 3
        { 20, 60 }, { 40, 60 }, { 60, 60 }, { 80, 60 }, { 100, 60 }, { 120, 60 }, { 140, 60 }, { 160, 60 },
        // Row 4
        { 20, 80 }, { 40, 80 }, { 60, 80 }, { 80, 80 }, { 100, 80 }, { 120, 80 }, { 140, 80 }, { 160, 80 },
        // Row 5
        { 20, 100 }, { 40, 100 }, { 60, 100 }, { 80, 100 }, { 100, 100 }, { 120, 100 }, { 140, 100 }, { 160, 100 },
        // Row 6
        { 20, 120 }, { 40, 120 }, { 60, 120 }, { 80, 120 }, { 100, 120 }, { 120, 120 }, { 140, 120 }, { 160, 120 },
        // Row 7
        { 20, 140 }, { 40, 140 }, { 60, 140 }, { 80, 140 }, { 100, 140 }, { 120, 140 }, { 140, 140 }, { 160, 140 },
        // Row 8 (4 keys)
        { 20, 160 }, { 40, 160 }, { 60, 160 }, { 80, 160 }
    },

    /* Flags (90 total LEDs) */
    {
        // Underglow indices: 1, 3, 5, 8, 9, 11, 12, 14, 16, 32, 33, 48, 50, 66-74
        LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    // 0-4
        LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   // 5-9
        LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   // 10-14
        LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    // 15-19
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    // 20-24
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    // 25-29
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    // 30-34
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    // 35-39
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    // 40-44
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    // 45-49
        LED_FLAG_UNDERGLOW,   LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    // 50-54
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    // 55-59
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    // 60-64
        LED_FLAG_KEYLIGHT,    LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   // 65-69
        LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   LED_FLAG_UNDERGLOW,   // 70-74
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    // 75-79
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    // 80-84
        LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT,    LED_FLAG_KEYLIGHT     // 85-89
    }
};

/* --- simple breathing helper ------------------------------------------ */
static uint8_t breath_step = 0;          // 0‥255, wraps automatically

static uint8_t breathe_wave(uint8_t t) { // triangle-wave 0‥255
    return t < 128 ? t * 2 : (255 - t) * 2;
}
/* ---------------------------------------------------------------------- */

bool rgb_matrix_indicators_user(void) {
    uint8_t v = breathe_wave(breath_step);   // brightness for this frame
    breath_step++;                           // advance for next frame

    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        if (HAS_FLAGS(g_led_config.flags[i], LED_FLAG_UNDERGLOW)) {
            // blue underglow with breathing brightness
            rgb_matrix_set_color(i, 0, 0, v);
        }
    }
    // rgb_matrix_set_color(1, 0, 0, 140);
    // rgb_matrix_set_color(3, 0, 0, 140);
    // rgb_matrix_set_color(5, 0, 0, 140);
    // rgb_matrix_set_color(8, 0, 0, 140);
    // rgb_matrix_set_color(9, 0, 0, 140);
    // rgb_matrix_set_color(11, 0, 0, 140);
    // rgb_matrix_set_color(12, 0, 0, 140);
    // rgb_matrix_set_color(14, 0, 0, 140);
    // rgb_matrix_set_color(16, 0, 0, 140);
    // rgb_matrix_set_color(32, 0, 0, 140);
    // rgb_matrix_set_color(33, 0, 0, 140);
    // rgb_matrix_set_color(48, 0, 0, 140);
    // rgb_matrix_set_color(50, 0, 0, 140);
    // rgb_matrix_set_color(66, 0, 0, 140);
    // rgb_matrix_set_color(67, 0, 0, 140);
    // rgb_matrix_set_color(68, 0, 0, 140);
    // rgb_matrix_set_color(69, 0, 0, 140);
    // rgb_matrix_set_color(70, 0, 0, 140);
    // rgb_matrix_set_color(71, 0, 0, 140);
    // rgb_matrix_set_color(72, 0, 0, 140);
    // rgb_matrix_set_color(73, 0, 0, 140);
    // rgb_matrix_set_color(74, 0, 0, 140);
    return false;
}

void keyboard_post_init_kb(void) {
    chThdSleepMilliseconds(3000);
    uprintf("CALLED HERE\r\n");
}
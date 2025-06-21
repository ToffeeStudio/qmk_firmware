#include "quantum.h"
#include "print.h"   // For uprintf
#include "ch.h"
#include "rgb_matrix.h"
#include "rgb_matrix_types.h"   // for led_point_t and led_config_t
#define ID_SET_LED_RED 0x70

static bool custom_led_state[RGB_MATRIX_LED_COUNT] = {false};

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    // We are looking for a packet with our custom command.
    // Packet format from Python script:
    // data[0]: Magic (0x09)
    // data[1]: Command ID (0x70)
    // data[2-5]: Packet ID (unused)
    // data[6]: LED index (0-89)
    //
    if (length >= 7 && data[0] == 0x09 && data[1] == ID_SET_LED_RED) {
        uint8_t led_index = data[6];

        // Validate the index to prevent out-of-bounds access
        if (led_index < RGB_MATRIX_LED_COUNT) {
            uprintf("RAW HID: Activating LED index %u\n", led_index);
            custom_led_state[led_index] = true;

        }
    }
}

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
        // This array will be filled at runtime by generate_led_positions()
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}
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

// The indices of keys, where the layout of this matrix corresponds to the physical position of these LEDs in real life.
const uint8_t led_physical_map[RGB_MATRIX_LED_COUNT] = {
    // KEYLIGHT LEDs (relevant)
    75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,   88, // First row of LEDs (physical positioning)
     51,  52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,  64, 65, // Second row of LEDs
    49, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36,   35,   34, // Third row of LEDs
      18,    19, 20, 21, 22, 23, 24, 25, 26, 27, 28,  29,   30, 31, // Fourth row of LEDs
    17,  15,  13,                10,              7,  6,   4, 2, 0, // Fifth row
    // UNDERGLOW LEDs
    1, 3, 5, 8, 9, 11, 12, 14, 16, 32, 33, 48, 50,
    66, 67, 68, 69, 70, 71, 72, 73, 74
};

const led_point_t led_physical_pos[RGB_MATRIX_LED_COUNT] = {
    {   8,   0}, {  24,   0}, {  40,   0}, {  56,   0}, {  72,   0}, {  88,   0}, { 104,   0}, { 120,   0}, { 136,   0}, { 152,   0}, { 168,   0}, { 184,   0}, { 200,   0}, { 224,   0},
    {  12,  50}, {  32,  50}, {  48,  50}, {  64,  50}, {  80,  50}, {  96,  50}, { 112,  50}, { 128,  50}, { 144,  50}, { 160,  50}, { 176,  50}, { 192,  50}, { 208,  50}, { 228,  50}, { 244,  50},
    {   8, 100}, {  24, 100}, {  40, 100}, {  56, 100}, {  72, 100}, {  88, 100}, { 104, 100}, { 120, 100}, { 136, 100}, { 152, 100}, { 168, 100}, { 184, 100}, { 200, 100}, { 224, 100}, { 244, 100},
    {  24, 150}, {  52, 150}, {  68, 150}, {  84, 150}, { 100, 150}, { 116, 150}, { 132, 150}, { 148, 150}, { 164, 150}, { 180, 150}, { 196, 150}, { 216, 150}, { 236, 150}, { 252, 150},
    {   8, 200}, {  28, 200}, {  52, 200}, { 124, 200}, { 192, 200}, { 208, 200}, { 228, 200}, { 240, 200}, { 252, 200},
    // Useless below
    {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
    {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
    {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
    {255, 255}
};

typedef struct { uint8_t row; uint8_t col; } logical_pos_t;
const logical_post_t key_logical_map[KEYLIGHT_COUNT] = {
//    `~`     `1`     `2`    `3`     `4`      `5`     `6`     `7`     `8`     `9`     `0`     `-`     `=`   `Bksp`   `Del`
    (0, 0), (1, 0), (0, 5), (1, 5), (0, 4), (1, 4), (0, 3), (1, 3), (0, 2), (1, 2), (0, 1), (1, 1), (0, 6), (1, 6), (0, 7)
//   `Tab`    `Q`     `W`     `E`     `R`     `T`     `Y`     U`      `I`     `O`     `P`     `[`     `]`     `\`    `Del`
    (2, 0), (3, 0), (2, 1), (3, 1), (2, 2), (3, 2), (2, 3), (3, 3), (2, 4), (3, 4), (2, 5), (3, 5), (2, 6), (3, 6), (2, 7)
//   Caps`    `A`     `S`     `D`     `F`     `G`     `H`     `J`     `K`     `L`     `;`     `'`   `Enter` `Home`
    (4, 0), (5, 0), (4, 1), (5, 1), (4, 2), (5, 2), (4, 3), (5, 3), (4, 4), (5, 4), (4, 5), (5, 5), (4, 6), (4, 7)
// `LShift`        `<`       `Z`     `X`      `C`     `V`     `B`     `N`    `M`      `,`     `.`     `/`   `RShift`   `Up`    `End`
    (7, 0),       (6, 0)    (6, 1), (7, 1), (6, 2), (7, 2), (6, 3), (7, 3), (7, 4), (6, 4), (7, 5), (6, 5), (6, 6),    (7, 6),  (6, 7)
// `Ctrl`       `Win`     `Alt`     `Space?`    `RAlt`    `MO(1)`    `Left`     `Down`    `Right`
    (8, 0),    (8, 1),   (8, 2),     (8, 3),    (8, 4),   (8, 5),    (8, 6),    (5, 6),    (7, 7)





}


void generate_led_positions(void) {
    for(int i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        g_led_config.point[led_physical_map[i]] = led_physical_pos[i];
    }
}

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

    // OVERRIDE CODE
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        if (custom_led_state[i]) {
            rgb_matrix_set_color(i, 255, 0, 0); // Set to RED
        }
    }

    return false;
}

void keyboard_post_init_kb(void) {
    generate_led_positions();
    chThdSleepMilliseconds(3000);
    uprintf("TURNED ON\r\n");
    uprintf("--- Verifying generated LED positions (Index: {X, Y}) ---\n");
    for (int i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        // Print each LED's hardware index and its generated (x, y) coordinates
        uprintf("LED %u: {%u, %u}\n", i, g_led_config.point[i].x, g_led_config.point[i].y);
        // A tiny delay per line can help prevent overwhelming the serial buffer
        chThdSleepMilliseconds(5);
    }
    uprintf("--- Verification complete. Keyboard ON. ---\n");
}

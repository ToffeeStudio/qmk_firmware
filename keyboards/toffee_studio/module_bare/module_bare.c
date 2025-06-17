#include "quantum.h"
#include "print.h"   // For uprintf
#include "ch.h"
#include "rgb_matrix.h"

led_config_t g_led_config = {
    /* Key-matrix → LED index
     * 9 rows × 8 cols  (COL2ROW diode dir, see info.json)
     */
    {
        {  0,  1, NO_LED, NO_LED, NO_LED,  2, NO_LED, NO_LED }, // row 0
        {  3,  4, NO_LED, NO_LED, NO_LED,  5, NO_LED, NO_LED }, // row 1
        {  6,  7, NO_LED, NO_LED, NO_LED,  8, NO_LED, NO_LED }, // row 2
        {  9, 10, NO_LED, NO_LED, NO_LED, 11, NO_LED, NO_LED }, // row 3
        { 12, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    },

    /* Physical XY positions (13 entries) */
    {
        {  0, 16}, {16, 16}, {64, 16},
        {  0, 32}, {16, 32}, {64, 32},
        {  0, 48}, {16, 48}, {64, 48},
        {  0, 64}, {16, 64},
        { 32,  0}, {48,  0},
    },

    /* Flags – keylight vs underglow, one per LED */
    {
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW
    }
};

// bool rgb_matrix_indicators_user(void) {
//     for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
//         if (HAS_FLAGS(g_led_config.flags[i], LED_FLAG_UNDERGLOW))
//             rgb_matrix_set_color(i, 0, 0, 255);        // blue strip
//         else
//             rgb_matrix_set_color(i, 255, 0, 0);        // red keys
//     }
//     return false;
// }

void keyboard_post_init_kb(void) {
    chThdSleepMilliseconds(3000);
    rgb_matrix_set_flags(LED_FLAG_KEYLIGHT);
    uprintf("CALLED HERE\r\n");
}

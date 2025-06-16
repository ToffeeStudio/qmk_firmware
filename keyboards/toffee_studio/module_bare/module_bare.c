#include "quantum.h"
#include "print.h"   // For uprintf
#include "ch.h"

led_config_t g_led_config = {
    // The .matrix_co map can be left empty for this test.
    {
        {0}
    },

    // Define a dummy physical position for each of your 13 LEDs.
    {
        {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
        {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
    },

    // Define a default flag for each of your 13 LEDs.
    {
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT
    }
};

void keyboard_post_init_kb(void) {
    chThdSleepMilliseconds(3000);
    uprintf("CALLED HERE\r\n");
}

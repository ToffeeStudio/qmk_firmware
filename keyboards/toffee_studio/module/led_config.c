// keyboards/toffee_studio/module/led_config.c
#include "quantum.h"

#ifdef RGB_MATRIX_ENABLE
// This is a minimal LED configuration for a "proof of life" test.
// It gives every LED a placeholder position and a default flag.
// This is required for the RGB Matrix feature to work.
led_config_t g_led_config = {
    // The .matrix_co map can be left empty for this test.
    .matrix_co = {{0}},

    // Define a dummy physical position for each of your 13 LEDs.
    .point = {
        {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
        {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
    },

    // Define a default flag for each of your 13 LEDs.
    .flags = {
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT
    }
};
#endif // RGB_MATRIX_ENABLE

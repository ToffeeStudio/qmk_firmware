// Board-local final-transmit boundary; the underlying RP2040 WS2812 transport
// remains QMK's vendor driver. No shared QMK source changes or saved-state writes.
#include "rgb_matrix.h"
#include "ws2812.h"
#include "power.h"
#include <string.h>
#ifdef MODULE_EXTENSIVE_TEST
#include "test_mode.h"
#endif

rgb_led_t rgb_matrix_ws2812_array[RGB_MATRIX_LED_COUNT];
static rgb_led_t output[RGB_MATRIX_LED_COUNT];

static void init(void) {
    memset(rgb_matrix_ws2812_array, 0, sizeof(rgb_matrix_ws2812_array));
    memset(output, 0, sizeof(output));
}

static void flush(void) {
#ifdef MODULE_EXTENSIVE_TEST
    // Recheck the watchdog after any late indicators and immediately before TX.
    lighting_test_apply_limits();
#endif
    lighting_power_prepare(rgb_matrix_ws2812_array, output);
    ws2812_setleds(output, RGB_MATRIX_LED_COUNT);
}

static void setled(int index, uint8_t r, uint8_t g, uint8_t b) {
    rgb_matrix_ws2812_array[index].r = r;
    rgb_matrix_ws2812_array[index].g = g;
    rgb_matrix_ws2812_array[index].b = b;
}

static void setled_all(uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) setled(i, r, g, b);
}

const rgb_matrix_driver_t rgb_matrix_driver = {
    .init = init, .flush = flush, .set_color = setled, .set_color_all = setled_all,
};

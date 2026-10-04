// Test production group rendering/map/autosave; QMK and rear renderer are explicit adapters.
#include <assert.h>
#include "../../lighting/groups.h"
#include "../../lighting/lighting.h"
#include "../../lighting/power.h"
#include "../../animations/manager.h"
#include "usb_device_state.h"
#include <lib/lib8tion/lib8tion.h>

rgb_led_t rgb_matrix_ws2812_array[201];
animation_config_t g_underglow_config;
static uint32_t now;
static uint8_t qmk_mode, qmk_flags, qmk_speed;
static HSV qmk_hsv;
static bool qmk_enabled, saved_available;
static unsigned saves;
static module_lighting_state_t saved;
enum usb_device_state usb_device_state = USB_DEVICE_STATE_CONFIGURED;
void notify_usb_device_state_change_user(enum usb_device_state state) {(void)state;}
uint32_t timer_read32(void) {return now;}
uint32_t timer_elapsed32(uint32_t last) {return now - last;}
void rgb_matrix_enable_noeeprom(void) {qmk_enabled = true;}
void rgb_matrix_set_flags_noeeprom(uint8_t flags) {qmk_flags = flags;}
void rgb_matrix_mode_noeeprom(uint8_t mode) {qmk_mode = mode;}
void rgb_matrix_set_speed_noeeprom(uint8_t speed) {qmk_speed = speed;}
void rgb_matrix_sethsv_noeeprom(uint8_t h, uint8_t s, uint8_t v) {qmk_hsv = (HSV){h, s, v};}
void rgb_matrix_set_color(uint16_t i, uint8_t r, uint8_t g, uint8_t b) {
    assert(i < 201); rgb_matrix_ws2812_array[i] = (RGB){r, g, b};
}
void rgb_matrix_set_color_all(uint8_t r, uint8_t g, uint8_t b) {
    for (unsigned i = 0; i < 201; i++) rgb_matrix_set_color(i, r, g, b);
}
RGB hsv_to_rgb_nocie(HSV hsv) {return (RGB){hsv.v, hsv.s, hsv.h};}
void underglow_manager_init(void) {memset(&g_underglow_config, 0, sizeof(g_underglow_config));}
void underglow_manager_set_brightness(uint8_t value) {g_underglow_config.brightness = value;}
void underglow_manager_next_anim(void) {}
void underglow_manager_task(void) {
    for (unsigned i = 0; i < 201; i++)
        if (g_led_config.flags[i] == LED_FLAG_UNDERGLOW) rgb_matrix_set_color(i, 30, 40, 50);
}
void load_underglow_config(void) {}
void underglow_persistence_task(void) {}
bool lighting_storage_load(module_lighting_state_t *s) {
    if (saved_available) *s = saved;
    return saved_available;
}
bool lighting_storage_save(const module_lighting_state_t *s) {saved = *s; saves++; saved_available = true; return true;}

static void qmk_render(void) {
    for (unsigned i = 0; i < 201; i++) {
        if (g_led_config.flags[i] == LED_FLAG_KEYLIGHT) rgb_matrix_set_color(i, 120, 80, 40);
        else rgb_matrix_set_color(i, 255, 255, 255); // Must be cleared/overwritten.
    }
    lighting_task();
    // This suite inspects the renderer and startup contract independently of
    // electrical profile selection. The actual final driver has its own suite.
    uint16_t startup = lighting_startup_scale();
    for (unsigned i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        RGB c = rgb_matrix_ws2812_array[i];
        rgb_matrix_ws2812_array[i] = (RGB){c.r * startup / 1000, c.g * startup / 1000, c.b * startup / 1000};
    }
}
#include "parser_test.c"
#include "startup_test.c"
#ifdef MODULE_EXTENSIVE_TEST
#include "test_mode_test.c"
#endif
int main(void) {
    test_startup_lighting();
    // Start the pre-existing regression suites with a clean persistence fixture.
    saved_available = false;
    saves = 0;
    now = 0;
#ifdef MODULE_EXTENSIVE_TEST
    test_temporary_firmware();
#else
    lighting_init();
    qmk_render();
    now = MODULE_LIGHTING_STARTUP_DELAY_MS + MODULE_LIGHTING_STARTUP_FADE_MS;
    qmk_render();
    now = 0; // Existing debounce tests below use a zero-based clock.
    unsigned front = 0, badge = 0, rear = 0;
    for (unsigned i = 0; i < 201; i++) {
        if (g_led_config.flags[i] == LED_FLAG_KEYLIGHT) front++;
        if (g_led_config.flags[i] == LED_FLAG_INDICATOR) badge++;
        if (g_led_config.flags[i] == LED_FLAG_UNDERGLOW) rear++;
        assert(lighting_is_front_led(i) == (g_led_config.flags[i] == LED_FLAG_KEYLIGHT));
    }
    assert(front == 66 && badge == 16 && rear == 22);
    assert(g_led_config.matrix_co[0][6] == 103); // Repaired top-row package.
    assert(g_led_config.matrix_co[0][7] == 83 && g_led_config.matrix_co[1][6] == 83);
    assert(g_led_config.matrix_co[7][0] == 18 && g_led_config.matrix_co[6][0] == 18);
    assert(qmk_enabled && qmk_flags == LED_FLAG_KEYLIGHT && qmk_hsv.v == scale8(18, 160));
    for (uint8_t effect = 0; effect < 9; effect++) {
        front_config_t f = {effect, 200, 37, 80, 190};
        badge_config_t b = {1, 25, 170, 255};
        assert(lighting_set_front(&f));
        assert(lighting_set_badge(&b));
        assert(qmk_enabled && qmk_mode && qmk_speed == 200);
        assert(qmk_hsv.v == (effect ? scale8(37, 160) : 0));
        assert(memcmp(&module_lighting_state.front, &f, 5) == 0);
        qmk_render();
        for (unsigned i = 0; i < 201; i++) {
            RGB c = rgb_matrix_ws2812_array[i];
            if (g_led_config.flags[i] == LED_FLAG_INDICATOR) assert(c.r == 25 && c.g == 255 && c.b == 170);
            if (g_led_config.flags[i] == LED_FLAG_UNDERGLOW) assert(c.r == 30 && c.g == 40 && c.b == 50);
            if (!g_led_config.flags[i]) assert(!c.r && !c.g && !c.b);
        }
    }
    for (unsigned i = 0; i < 104; i++) {
        if (!lighting_is_front_led(i)) continue;
        for (unsigned brightness = 0; brightness <= 255; brightness++) {
            keylight_set_brightness(i, brightness);
            qmk_render();
            assert(lighting_get_led_brightness(i) == brightness);
            RGB c = rgb_matrix_ws2812_array[i];
            assert(c.r == scale8(120, brightness) && c.g == scale8(80, brightness) && c.b == scale8(40, brightness));
            assert(rgb_matrix_ws2812_array[68].r == 25 && rgb_matrix_ws2812_array[90].r == 30);
        }
    }
    module_lighting_state_t previous = module_lighting_state;
    front_config_t invalid_front = {9, 1, 2, 3, 4};
    badge_config_t invalid_badge = {2, 1, 2, 3};
    assert(!lighting_set_front(&invalid_front) && !lighting_set_badge(&invalid_badge));
    assert(memcmp(&previous, &module_lighting_state, sizeof(previous)) == 0);
    now = 999; lighting_groups_persistence_task(); assert(saves == 0);
    lighting_groups_schedule_save(); now = 1998; lighting_groups_persistence_task(); assert(saves == 0);
    now = 1999; lighting_groups_persistence_task(); assert(saves == 1);
    lighting_groups_persistence_task(); assert(saves == 1);
    lighting_groups_init(); assert(memcmp(&saved, &module_lighting_state, sizeof(saved)) == 0);
    now = UINT32_MAX - 500; lighting_groups_schedule_save();
    now = 498; lighting_groups_persistence_task(); assert(saves == 1);
    now = 499; lighting_groups_persistence_task(); assert(saves == 2);
    puts("Groups PASS: 104-package map, 9 front modes, independent badge/rear, 16,896 per-key levels, debounce/reboot/timer wrap.");
    test_parser();
#endif
}

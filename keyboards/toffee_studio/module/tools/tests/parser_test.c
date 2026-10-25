// Included by groups_test.c. The build generates module_parser.inc directly
// from the production lighting handlers and complete packet dispatcher.
// Legacy display/filesystem adapters must never be reached in screenless mode.
#include <stdlib.h>
#include "../../rawhid/module_raw_hid.h"
#include "../../display/wpm_indicator.h"
#ifdef MODULE_EXTENSIVE_TEST
#include "../../lighting/test_mode.h"
#endif

static uint8_t *return_buf;
const uint8_t UNDERGLOW_ANIMATION_COUNT = 10;
void underglow_manager_set_anim(uint8_t value) {g_underglow_config.current_animation_id = value;}
void underglow_manager_set_speed(uint8_t value) {g_underglow_config.speed = value;}
void underglow_manager_set_color_hsv(uint8_t h, uint8_t s, uint8_t v) {g_underglow_config.color = (HSV){h, s, v};}
void underglow_manager_set_color_hs(uint8_t h, uint8_t s) {g_underglow_config.color.h = h; g_underglow_config.color.s = s;}
void underglow_manager_save(void) {}
void wpm_indicator_set_anim(const char *value, wpm_mode_t mode) {abort();}
void wpm_indicator_set_config(uint8_t a, uint8_t b, uint8_t c) {abort();}
static bool bootloader_expected;
static unsigned bootloader_entries;
void bootloader_jump(void) {assert(bootloader_expected); bootloader_entries++;}

#define FORBIDDEN_HANDLER(name) static int name(uint8_t *data, uint8_t length) {abort();}
FORBIDDEN_HANDLER(parse_ls)
FORBIDDEN_HANDLER(parse_cd)
FORBIDDEN_HANDLER(parse_pwd)
FORBIDDEN_HANDLER(parse_rm)
FORBIDDEN_HANDLER(parse_mkdir)
FORBIDDEN_HANDLER(parse_touch)
FORBIDDEN_HANDLER(parse_cat)
FORBIDDEN_HANDLER(parse_open)
FORBIDDEN_HANDLER(parse_write)
FORBIDDEN_HANDLER(parse_close)
FORBIDDEN_HANDLER(parse_format_filesystem)
FORBIDDEN_HANDLER(parse_flash_remaining)
FORBIDDEN_HANDLER(parse_choose_image)
FORBIDDEN_HANDLER(parse_write_display)
FORBIDDEN_HANDLER(parse_set_time)
FORBIDDEN_HANDLER(parse_ls_next)
FORBIDDEN_HANDLER(parse_ls_all)
#undef FORBIDDEN_HANDLER

#include "module_parser.inc"

static unsigned parser_attempts;
static int packet(uint8_t command, const uint8_t *payload, size_t size) {
    uint8_t data[RAW_EPSIZE] = {9, command, 0x12, 0x34, 0x56, 0x78};
    assert(size <= RAW_EPSIZE - 6);
    memcpy(data + 6, payload, size);
    int result = module_raw_hid_parse_packet(data, sizeof(data));
    parser_attempts++;
    assert(data[26] == 0xA5 && data[27] == command);
    assert(memcmp(data + 28, (uint8_t[]){0x12, 0x34, 0x56, 0x78}, 4) == 0);
    assert(data[0] == result);
    return result;
}
static void reject_packet(uint8_t command, const uint8_t *payload, size_t size) {
    module_lighting_state_t groups_before = module_lighting_state;
    animation_config_t rear_before = g_underglow_config;
    RGB pixels_before[RGB_MATRIX_LED_COUNT];
    lighting_task();
    memcpy(pixels_before, rgb_matrix_ws2812_array, sizeof(pixels_before));
    assert(packet(command, payload, size) == module_ret_invalid_command);
    assert(memcmp(&groups_before, &module_lighting_state, sizeof(groups_before)) == 0);
    assert(memcmp(&rear_before, &g_underglow_config, sizeof(rear_before)) == 0);
    lighting_task();
    assert(memcmp(pixels_before, rgb_matrix_ws2812_array, sizeof(pixels_before)) == 0);
}

static void test_parser(void) {
    // Real telemetry handler writes only the 25-byte reply payload and keeps
    // command/packet correlation, renderer and persisted preferences unchanged.
    module_lighting_state_t settings_before = module_lighting_state;
    animation_config_t rear_before = g_underglow_config;
    RGB rendered_before[RGB_MATRIX_LED_COUNT];
    memcpy(rendered_before, rgb_matrix_ws2812_array, sizeof(rendered_before));
    for (unsigned fill = 0; fill < 256; fill++) {
        uint8_t data[32]; memset(data, fill, sizeof(data));
        data[0] = 9; data[1] = id_lighting_power_status;
        uint8_t packet_id[4]; memcpy(packet_id, data + 2, 4);
        assert(module_raw_hid_parse_packet(data, sizeof(data)) == 0);
        parser_attempts++;
        assert(data[0] == 0);
#ifdef MODULE_EXTENSIVE_TEST
        assert(data[1] == 1 && data[25] == 0);
#else
        assert(data[1] == 2 && data[25] == module_power_profile.front_badge_max);
#endif
        assert(data[26] == 0xA5 && data[27] == id_lighting_power_status);
        assert(memcmp(packet_id, data + 28, 4) == 0);
        assert(memcmp(&settings_before, &module_lighting_state, sizeof(settings_before)) == 0);
        assert(memcmp(&rear_before, &g_underglow_config, sizeof(rear_before)) == 0);
        assert(memcmp(rendered_before, rgb_matrix_ws2812_array, sizeof(rendered_before)) == 0);
#ifdef MODULE_EXTENSIVE_TEST
        memset(data, fill, sizeof(data));
        data[0] = 9; data[1] = id_lighting_test_rgb_sums;
        assert(module_raw_hid_parse_packet(data, sizeof(data)) == 0);
        parser_attempts++;
        assert(data[1] == 1 && data[25] == 0);
        assert(data[26] == 0xA5 && data[27] == id_lighting_test_rgb_sums);
        assert(memcmp(packet_id, data + 28, 4) == 0);
        assert(memcmp(&settings_before, &module_lighting_state, sizeof(settings_before)) == 0);
        assert(memcmp(&rear_before, &g_underglow_config, sizeof(rear_before)) == 0);
        assert(memcmp(rendered_before, rgb_matrix_ws2812_array, sizeof(rendered_before)) == 0);
#endif
    }
    const uint8_t off[5] = {255, 255, 255, 255, 255};
    const RGB colors[5] = {{.r=18}, {.b=18}, {.g=18}, {.r=18,.g=18}, {.r=18,.g=9,.b=13}};
    assert(packet(id_lighting_discovery, off, sizeof(off)) == 0);
    lighting_task();
    for (unsigned i = 0; i < RGB_MATRIX_LED_COUNT; i++)
        assert(memcmp(&rgb_matrix_ws2812_array[i], &(RGB){0, 0, 0}, sizeof(RGB)) == 0);

    // Every configured diagnostic index is valid in each of the five slots.
    // Production renderer must clear every unselected slot, including padding.
    for (unsigned index = 0; index < 201; index++) {
        for (unsigned slot = 0; slot < 5; slot++) {
            uint8_t selected[5]; memcpy(selected, off, sizeof(selected));
            selected[slot] = index;
            assert(packet(id_lighting_discovery, selected, sizeof(selected)) == 0);
            lighting_task();
            for (unsigned i = 0; i < 201; i++) {
                RGB color = rgb_matrix_ws2812_array[i];
                if (i == index) assert(memcmp(&color, &colors[slot], sizeof(color)) == 0);
                else assert(!color.r && !color.g && !color.b);
            }
        }
        // Duplicates fail regardless of which pair of color slots uses them.
        for (unsigned first = 0; first < 5; first++) {
            for (unsigned second = first + 1; second < 5; second++) {
                uint8_t duplicate[5]; memcpy(duplicate, off, sizeof(duplicate));
                duplicate[first] = duplicate[second] = index;
                reject_packet(id_lighting_discovery, duplicate, sizeof(duplicate));
            }
        }
    }
    for (unsigned index = 201; index < 255; index++) {
        for (unsigned slot = 0; slot < 5; slot++) {
            uint8_t invalid[5]; memcpy(invalid, off, sizeof(invalid));
            invalid[slot] = index;
            reject_packet(id_lighting_discovery, invalid, sizeof(invalid));
        }
    }

    // Exact-sized allocations let ASan detect even short-packet overreads.
    for (unsigned length = 0; length <= 255; length++) {
        if (length == RAW_EPSIZE) continue;
        uint8_t *data = calloc(length ? length : 1, 1);
        if (length) data[0] = 9;
        if (length > 1) data[1] = id_lighting_front_set;
        module_lighting_state_t before = module_lighting_state;
        assert(module_raw_hid_parse_packet(data, length) ==
               (length < 6 || length > 32 ? -1 : module_ret_invalid_command));
        parser_attempts++;
        assert(memcmp(&before, &module_lighting_state, sizeof(before)) == 0);
        free(data);
    }
    for (unsigned command = 0; command <= 255; command++) {
#ifdef MODULE_MANUAL_MEASUREMENT
        if (command == id_lighting_test_mixed) continue; // Exact mixed-output tests below.
#endif
#ifdef MODULE_POWER_CALIBRATION
        if (command == id_lighting_test_raw) continue; // Exercised by raw-pattern boundary tests.
#endif
        if (command == id_lighting_power_status) continue;
#ifdef MODULE_EXTENSIVE_TEST
        if (command == id_lighting_test_status || command == id_lighting_test_keepalive || command == id_lighting_test_rgb_sums) continue;
#endif
        if ((command >= 0x71 && command <= 0x76 && command != 0x73) ||
            (command >= 0x79 && command <= 0x85) ||
            command == id_lighting_badge_animation_get || command == id_lighting_badge_animation_set) continue;
        reject_packet(command, off, sizeof(off));
    }
    uint8_t bad_magic[32] = {0, id_lighting_discovery};
    memcpy(bad_magic + 6, off, sizeof(off));
    assert(module_raw_hid_parse_packet(bad_magic, sizeof(bad_magic)) == -1);
    parser_attempts++;

    // Validate all 256 indices at the real HID per-key membership boundary.
    for (unsigned index = 0; index <= 255; index++) {
        uint8_t value[2] = {index, 17};
        if (lighting_is_front_led(index)) {
            assert(packet(id_lighting_set_led_brightness, value, 2) == 0);
            assert(packet(id_lighting_led_get, value, 1) == 0);
            assert(lighting_get_led_brightness(index) == 17);
        } else {
            reject_packet(id_lighting_set_led_brightness, value, 2);
            reject_packet(id_lighting_led_get, value, 1);
        }
    }
    reject_packet(id_lighting_front_set, (uint8_t[]){MODULE_FRONT_EFFECT_COUNT, 100, 18, 0, 255}, 5);
    reject_packet(id_lighting_badge_animation_set, (uint8_t[]){1, 18, 0, 255, MODULE_BADGE_EFFECT_COUNT, 100}, 6);
    reject_packet(id_lighting_badge_animation_set, (uint8_t[]){2, 18, 0, 255, 1, 100}, 6);
    for (uint8_t effect = 0; effect < MODULE_BADGE_EFFECT_COUNT; effect++) {
        uint8_t config[] = {1, 18, 33, 255, effect, 180};
        assert(packet(id_lighting_badge_animation_set, config, sizeof(config)) == 0);
        uint8_t reply[32] = {9, id_lighting_badge_animation_get};
        assert(module_raw_hid_parse_packet(reply, 32) == 0);
        assert(memcmp(reply + 1, config, sizeof(config)) == 0);
        assert(reply[26] == 0xa5 && reply[27] == id_lighting_badge_animation_get);
    }
    uint8_t caps[32] = {9, id_lighting_capabilities};
    assert(module_raw_hid_parse_packet(caps, 32) == 0);
    assert(memcmp(caps + 1, (uint8_t[]){3, 66, 22, 16, 104, MODULE_FRONT_EFFECT_COUNT, MODULE_BADGE_EFFECT_COUNT}, 7) == 0);
    reject_packet(id_lighting_badge_set, (uint8_t[]){2, 18, 0, 255}, 4);
    reject_packet(id_lighting_set_animation, (uint8_t[]){10}, 1);
    reject_packet(id_lighting_discovery_back, (uint8_t[]){2, 64}, 2);

    // Adjacent and interleaved physical groups remain exactly selected.
    const uint8_t transitions[] = {64, 65, 80, 81, 82, 83, 84, 103};
    for (unsigned i = 0; i < sizeof(transitions); i++) {
        assert(packet(id_lighting_discovery, (uint8_t[]){transitions[i], 255, 255, 255, 255}, 5) == 0);
        lighting_task();
        assert(rgb_matrix_ws2812_array[transitions[i]].r == 18);
        assert(packet(id_lighting_discovery, off, 5) == 0);
        lighting_task();
        for (unsigned index = 0; index < 201; index++) {
            RGB c = rgb_matrix_ws2812_array[index]; assert(!c.r && !c.g && !c.b);
        }
    }
    printf("Parser PASS: %u attempts; actual production handlers/dispatcher and renderer; diagnostic bounds, duplicates, sentinel, lengths, denied commands, membership and clearing.\n", parser_attempts);
}

static void test_persistent_rescue(void) {
    // Explicit rescue bypasses test-save suppression ONLY for saved zero brightness.
    // Stored colours/effects/overrides win over temporary RAM changes.
    module_lighting_state_t target = module_lighting_state;
    target.front.brightness = 231; target.badge.brightness = 213;
    target.front.hue = 37; target.badge.hue = 91;
    saved = target; saved_available = true;
    module_lighting_state.front.hue = 123;
    module_lighting_state.badge.hue = 124;
    unsigned flushes = rescue_flushes;
    uint8_t rescue[32] = {9, id_lighting_rescue_off, 1, 2, 3, 4, 'Z', 'E', 'R', 'O'};
    assert(module_raw_hid_parse_packet(rescue, sizeof(rescue)) == 0);
    assert(memcmp(rescue + 1, (uint8_t[]){1, 7, 0, 0, 0}, 5) == 0);
    assert(rescue[26] == 0xa5 && rescue[27] == id_lighting_rescue_off);
    assert(memcmp(rescue + 28, (uint8_t[]){1, 2, 3, 4}, 4) == 0);
    assert(rescue_flushes == flushes + 1);
    target.front.brightness = target.badge.brightness = 0;
    assert(memcmp(&target, &saved, sizeof(target)) == 0);
    assert(memcmp(&target, &module_lighting_state, sizeof(target)) == 0);
    assert(g_underglow_config.brightness == 0);
    qmk_render();
    for (unsigned i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        RGB c = rgb_matrix_ws2812_array[i]; assert(!c.r && !c.g && !c.b);
    }
    lighting_groups_init(); // Reload the saved zero state, without host setters.
    assert(memcmp(&target, &module_lighting_state, sizeof(target)) == 0);
    storage_fail = true;
    assert(packet(id_lighting_rescue_off, (const uint8_t *)"ZERO", 4) == module_ret_storage_failed);
    storage_fail = false; rear_fail = true;
    assert(packet(id_lighting_rescue_off, (const uint8_t *)"ZERO", 4) == module_ret_storage_failed);
    rear_fail = false;
    puts("Rescue PASS: black transmitted before saves; stored preferences retained; zero reload; correlated verified reply; failed saves never report success.");
}

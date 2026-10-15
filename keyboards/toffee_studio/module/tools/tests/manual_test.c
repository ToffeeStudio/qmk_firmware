// Host-only: never deliver these full-drive patterns to hardware automatically.
static void test_manual_firmware(void) {
    uint8_t status[24];
    now = 100;
    lighting_init();
    qmk_render(); assert_black();
    assert(!module_lighting_state.front.brightness && !module_lighting_state.badge.brightness);
    assert(!g_underglow_config.brightness);
    now = 10000; qmk_render(); assert_black();
    assert(MODULE_TEST_CHANNEL_CAP == 255 && RGB_MATRIX_MAXIMUM_BRIGHTNESS == 255);

    // Atomic mixed groups: exact final bytes, all 104 positions, and no timeout.
    for (unsigned level = 0; level <= 255; level++) {
        uint8_t mixed[9] = {level, 255-level, 17, 41, level, 255-level, 255-level, 73, level};
        assert(packet(id_lighting_test_mixed, mixed, sizeof(mixed)) == 0);
        now += 6000;
        lighting_task();
        rgb_led_t output[201];
        lighting_power_prepare(rgb_matrix_ws2812_array, output);
        for (unsigned i = 0; i < 201; i++) {
            unsigned group = HAS_FLAGS(g_led_config.flags[i], LED_FLAG_KEYLIGHT) ? 0 :
                             g_led_config.flags[i] == LED_FLAG_UNDERGLOW ? 1 : 2;
            RGB expected = i < 104 ? (RGB){.r=mixed[group*3], .g=mixed[group*3+1], .b=mixed[group*3+2]} : (RGB){0};
            assert(memcmp(&output[i], &expected, sizeof(RGB)) == 0);
        }
    }
    assert(packet(id_lighting_discovery, (uint8_t[]){255,255,255,255,255}, 5) == 0);
    now += 6000; lighting_task(); assert_black();
    // Commands during startup are accepted, but cannot bypass dark/fade gating.
    lighting_init(); qmk_render();
    assert(packet(id_lighting_test_mixed, (uint8_t[]){255,0,0,0,255,0,0,0,255}, 9) == 0);
    qmk_render(); assert_black();
    now += 1500; qmk_render();
    assert(rgb_matrix_ws2812_array[0].r == 127);
    now += 500; qmk_render();
    assert(rgb_matrix_ws2812_array[0].r == 255);
    lighting_init(); qmk_render(); now += 2000; qmk_render(); assert_black();

    // Standard configurator front values are not rescaled to the production160 cap.
    for (unsigned level = 0; level <= 255; level++) {
        front_config_t front = {1, 100, level, 30, 200};
        assert(lighting_set_front(&front));
        assert(qmk_hsv.v == level);
        assert(!module_lighting_state.badge.brightness && !g_underglow_config.brightness);
    }
    uint8_t raw[16] = {255, 255, 255};
    // Every population1..104 and level0..255 is accepted, independent of budget.
    for (unsigned count = 1; count <= 104; count++) {
        memset(raw + 3, 0, 13);
        for (unsigned i = 0; i < count; i++) raw[3 + i / 8] |= 1u << (i % 8);
        for (unsigned level = 0; level <= 255; level++) {
            raw[0] = level; raw[1] = 255 - level; raw[2] = (level * 17) & 255;
            assert(packet(id_lighting_test_raw, raw, sizeof(raw)) == 0);
            lighting_task();
            rgb_led_t output[201];
            lighting_power_prepare(rgb_matrix_ws2812_array, output);
            for (unsigned i = 0; i < 201; i++) {
                RGB expected = i < count ? (RGB){.r=raw[0], .g=raw[1], .b=raw[2]} : (RGB){0};
                assert(memcmp(&output[i], &expected, sizeof(RGB)) == 0);
            }
        }
    }
    memset(raw, 255, sizeof(raw)); // Includes LED103; exceeds every old budget.
    assert(packet(id_lighting_test_raw, raw, sizeof(raw)) == 0);
    for (unsigned step = 0; step < 3; step++) {
        now += 600000; // No heartbeat, no five-second timeout, read-only status.
        lighting_task(); lighting_test_status(status);
        assert(status[2] && !status[3] && status[1] == 255);
        assert(rgb_matrix_ws2812_array[103].r == 255);
    }
    now = UINT32_MAX - 3; lighting_test_keepalive();
    now = 600000; lighting_task(); assert(rgb_matrix_ws2812_array[103].r == 255);
    lighting_groups_persistence_task(); assert(saves == 0);
    assert(packet(id_lighting_discovery, (uint8_t[]){255,255,255,255,255}, 5) == 0);
    lighting_task(); assert_black();
    now += 600000; lighting_test_keepalive(); lighting_task(); assert_black();
    memset(raw + 3, 0, 13); reject_packet(id_lighting_test_raw, raw, sizeof(raw));

    // Simulate restart with high saved preferences: RAM clears, saved record stays.
    saved_available = true;
    saved = module_lighting_state;
    saved.front.brightness = saved.badge.brightness = 255;
    module_lighting_state_t stored = saved;
    lighting_init();
    now += 600000; qmk_render(); assert_black();
    assert(memcmp(&saved, &stored, sizeof(saved)) == 0);
    assert(!module_lighting_state.front.brightness && !module_lighting_state.badge.brightness);
    assert(!g_underglow_config.brightness);
    test_parser();
    lighting_groups_persistence_task(); assert(saves == 0);
    puts("Manual PASS: 256 mixed frames, mixed startup/fade/stop, 26,624 exact raw patterns, 256 front values, no current/package cap or timeout, startup dark, no saves, rescue and parser; NO hardware.");
}

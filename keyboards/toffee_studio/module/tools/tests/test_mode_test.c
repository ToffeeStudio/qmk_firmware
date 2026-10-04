// Uses the same explicit host adapters as the normal group/parser tests.
static void test_temporary_firmware(void) {
    uint8_t status[24];
    now = 100;
    lighting_init();
    qmk_render();
    for (unsigned i = 0; i < 201; i++) {
        RGB c = rgb_matrix_ws2812_array[i]; assert(!c.r && !c.g && !c.b);
    }
    lighting_test_status(status);
    assert(status[0] == 1 && status[1] == MODULE_TEST_CHANNEL_CAP && !status[2] && !status[3]);
    uint32_t build; memcpy(&build, status + 8, 4); assert(build == MODULE_TEST_BUILD_ID);

    front_config_t front = {1, 100, 255, 0, 0};
    badge_config_t badge = {1, 255, 0, 0};
    assert(lighting_set_front(&front) && lighting_set_badge(&badge));
    qmk_render();
    for (unsigned i = 0; i < 201; i++) {
        RGB c = rgb_matrix_ws2812_array[i]; assert(c.r <= MODULE_TEST_CHANNEL_CAP && c.g <= MODULE_TEST_CHANNEL_CAP && c.b <= MODULE_TEST_CHANNEL_CAP);
    }
    // Sweep all channel input levels over all 201 slots, including padding.
    for (unsigned value = 0; value <= 255; value++) {
        RGB raw = {value, 255 - value, (value * 17) & 255};
        for (unsigned i = 0; i < 201; i++) rgb_matrix_ws2812_array[i] = raw;
        lighting_test_apply_limits();
        for (unsigned i = 0; i < 201; i++) {
            RGB c = rgb_matrix_ws2812_array[i];
            assert(c.r == (raw.r > MODULE_TEST_CHANNEL_CAP ? MODULE_TEST_CHANNEL_CAP : raw.r));
            assert(c.g == (raw.g > MODULE_TEST_CHANNEL_CAP ? MODULE_TEST_CHANNEL_CAP : raw.g));
            assert(c.b == (raw.b > MODULE_TEST_CHANNEL_CAP ? MODULE_TEST_CHANNEL_CAP : raw.b));
        }
    }
    now = 5099;
    lighting_test_status(status); // Read-only status must not refresh the deadline.
    assert(status[2] && !status[3]);
    qmk_render(); assert(rgb_matrix_ws2812_array[0].r);
    now = 5100; qmk_render();
    lighting_test_status(status); assert(!status[2] && status[3]);
    for (unsigned i = 0; i < 201; i++) {
        RGB c = rgb_matrix_ws2812_array[i]; assert(!c.r && !c.g && !c.b);
    }
    now = 5101; assert(packet(id_lighting_test_keepalive, (uint8_t[]){0}, 0) == 0);
    qmk_render(); assert(!rgb_matrix_ws2812_array[0].r);
    lighting_test_status(status); assert(!status[2] && status[3]);
    assert(packet(id_lighting_discovery, (uint8_t[]){0, 255, 255, 255, 255}, 5) == 0);
    lighting_task(); assert(rgb_matrix_ws2812_array[0].r == 18);
    now = 9000; lighting_test_keepalive();
    now = 13999; lighting_task(); assert(rgb_matrix_ws2812_array[0].r == 18);
    now = 14000; lighting_task(); assert(!rgb_matrix_ws2812_array[0].r);

    // Unsigned timer wrap preserves the exact five-second timeout.
    now = UINT32_MAX - 1000;
    assert(packet(id_lighting_discovery, (uint8_t[]){0, 255, 255, 255, 255}, 5) == 0);
    now = 3998; lighting_task(); assert(rgb_matrix_ws2812_array[0].r == 18);
    now = 3999; lighting_task(); assert(!rgb_matrix_ws2812_array[0].r);

    // A heartbeat arriving after the deadline but before the next render must
    // not rescue an expired pattern by resetting its timestamp first.
    now = 20000;
    assert(packet(id_lighting_discovery, (uint8_t[]){0, 255, 255, 255, 255}, 5) == 0);
    now = 25000; lighting_test_keepalive();
    lighting_test_status(status); assert(!status[2] && status[3]);
    lighting_task(); assert(!rgb_matrix_ws2812_array[0].r);

    // Test group changes must never call the production storage adapter.
    now = 50000;
    lighting_groups_persistence_task(); assert(saves == 0);
    assert(packet(id_lighting_test_status, (uint8_t[]){0}, 0) == 0);
    now = 0;
    test_parser();
    now = 60000;
    lighting_groups_persistence_task(); assert(saves == 0);
#ifdef MODULE_POWER_CALIBRATION
    now = 100000;
    module_lighting_state_t original = module_lighting_state;
    uint8_t raw[16] = {255, 128, 32};
    // Every mapped index, including 103, supports literal RGB without HSV/CIE.
    for (unsigned selected = 0; selected < MODULE_MAPPED_LED_COUNT; selected++) {
        memset(raw + 3, 0, 13);
        raw[3 + selected / 8] = 1u << (selected % 8);
        assert(packet(id_lighting_test_raw, raw, sizeof(raw)) == 0);
        lighting_task();
        lighting_test_apply_limits(); // Final-boundary repeat must be idempotent.
        for (unsigned i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
            RGB expected = i == selected ? (RGB){255, 128, 32} : (RGB){0, 0, 0};
            assert(memcmp(&rgb_matrix_ws2812_array[i], &expected, sizeof(RGB)) == 0);
        }
    }
    // Exact population boundaries: 50 at full drive; >50 only at <=150.
    for (unsigned count = 1; count <= 104; count++) {
        memset(raw + 3, 0, 13);
        for (unsigned i = 0; i < count; i++) raw[3 + i / 8] |= 1u << (i % 8);
        for (unsigned channel = 0; channel < 3; channel++) {
            memset(raw, 150, 3); raw[channel] = 255;
            if (count <= 50) assert(packet(id_lighting_test_raw, raw, sizeof(raw)) == 0);
            else reject_packet(id_lighting_test_raw, raw, sizeof(raw));
        }
        memset(raw, 150, 3);
        assert(packet(id_lighting_test_raw, raw, sizeof(raw)) == 0);
    }
    memset(raw + 3, 0, 13);
    reject_packet(id_lighting_test_raw, raw, sizeof(raw)); // Empty selection fails.
    raw[3] = 1; memset(raw, 255, 3);
    assert(packet(id_lighting_test_raw, raw, sizeof(raw)) == 0);
    now += 4999; lighting_task(); assert(rgb_matrix_ws2812_array[0].r == 255);
    now++; lighting_test_keepalive(); lighting_task(); assert_black();
    lighting_test_status(status); assert(!status[2] && status[3]);
    assert(packet(id_lighting_test_raw, raw, sizeof(raw)) == 0);
    lighting_task(); assert(rgb_matrix_ws2812_array[0].r == 255);
    assert(packet(id_lighting_discovery, (uint8_t[]){255,255,255,255,255}, 5) == 0);
    lighting_task(); assert_black();
    assert(memcmp(&original, &module_lighting_state, sizeof(original)) == 0);
    lighting_groups_persistence_task(); assert(saves == 0);
    puts("Raw calibration PASS: all 104 positions, exact 50/150 bounds, no padding, literal RGB, repeat flush, timeout, rescue and preserved settings.");
#endif
    puts("Temporary firmware PASS: startup off; all 201 RGB slots capped through 256 levels; exact 5000ms timeout/wrap; status read-only; late heartbeat cannot relight; group saves disabled.");
}

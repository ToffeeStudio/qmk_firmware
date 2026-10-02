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
    assert(status[0] == 1 && status[1] == 18 && !status[2] && !status[3]);
    uint32_t build; memcpy(&build, status + 8, 4); assert(build == MODULE_TEST_BUILD_ID);

    front_config_t front = {1, 100, 255, 0, 0};
    badge_config_t badge = {1, 255, 0, 0};
    assert(lighting_set_front(&front) && lighting_set_badge(&badge));
    qmk_render();
    for (unsigned i = 0; i < 201; i++) {
        RGB c = rgb_matrix_ws2812_array[i]; assert(c.r <= 18 && c.g <= 18 && c.b <= 18);
    }
    // Sweep all channel input levels over all 201 slots, including padding.
    for (unsigned value = 0; value <= 255; value++) {
        RGB raw = {value, 255 - value, (value * 17) & 255};
        for (unsigned i = 0; i < 201; i++) rgb_matrix_ws2812_array[i] = raw;
        lighting_test_apply_limits();
        for (unsigned i = 0; i < 201; i++) {
            RGB c = rgb_matrix_ws2812_array[i];
            assert(c.r == (raw.r > 18 ? 18 : raw.r));
            assert(c.g == (raw.g > 18 ? 18 : raw.g));
            assert(c.b == (raw.b > 18 ? 18 : raw.b));
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
    puts("Temporary firmware PASS: startup off; all 201 RGB slots capped through 256 levels; exact 5000ms timeout/wrap; status read-only; late heartbeat cannot relight; group saves disabled.");
}

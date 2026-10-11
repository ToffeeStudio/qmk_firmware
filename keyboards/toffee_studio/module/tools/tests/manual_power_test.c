// Included after the real final-driver suite's adapters; no hardware access.
static void test_manual_power(void) {
    assert(module_power_profile.revision == 0x8006);
    assert(module_power_profile.board_limit_ma == 1000 && module_power_profile.margin_ma == 50);
    assert(module_power_led_budget(&module_power_profile) == 830); // Informational only.
    module_power_profile_t ignored_limits = module_power_profile;
    ignored_limits.board_limit_ma = 1;
    ignored_limits.front_badge_max = 0; // Unlimited means the new normal cap is ignored too.
    memset(ignored_limits.channel_ceiling, 0, sizeof(ignored_limits.channel_ceiling));
    rgb_led_t input[201], output[201], original[201];
    module_power_frame_t result;
    for (unsigned trial = 0; trial < 20000; trial++) {
        for (unsigned i = 0; i < 201; i++) input[i] = (RGB){.r=random32(), .g=random32(), .b=random32()};
        memcpy(original, input, sizeof(input));
        unsigned fade = trial % 1001;
        module_power_limit(&ignored_limits, input, g_led_config.flags, fade, output, &result);
        assert(memcmp(original, input, sizeof(input)) == 0);
        assert(!result.limited && result.limit_scale == UINT16_MAX);
        assert(result.requested_ua == (demand(input, &ignored_limits) + 254) / 255);
        assert(result.effective_ua == (demand(output, &ignored_limits) + 254) / 255);
        for (unsigned i = 0; i < 201; i++) {
            RGB expected = group(i) >= 0 ? (RGB){.r=input[i].r * fade / 1000,
                .g=input[i].g * fade / 1000, .b=input[i].b * fade / 1000} : (RGB){0};
            assert(memcmp(&output[i], &expected, sizeof(RGB)) == 0);
        }
    }
    rgb_matrix_driver.init();
    rgb_matrix_driver.set_color_all(255, 255, 255);
    for (unsigned phase = 0; phase <= 1000; phase++) {
        startup = phase; now = phase;
        rgb_matrix_driver.flush();
        for (unsigned i = 0; i < 201; i++) {
            uint8_t value = i < 104 ? 255 * phase / 1000 : 0;
            assert(wire[i].r == value && wire[i].g == value && wire[i].b == value);
        }
    }
    uint8_t status[25], sums[25];
    lighting_power_status(status); lighting_power_rgb_sums(sums);
    assert(status[1] == (1 | 4 | 16 | 32));
    assert(u16(status + 18) == 1000 && u16(status + 22) == 830);
    assert(u16(status + 14) == u16(status + 16) && u16(status + 16) > 830);
    for (unsigned channel = 0; channel < 3; channel++) {
        assert(u16(sums + 6 + channel * 2) == 66 * 255);
        assert(u16(sums + 12 + channel * 2) == 22 * 255);
        assert(u16(sums + 18 + channel * 2) == 16 * 255);
    }
    usb_device_state = USB_DEVICE_STATE_SUSPEND;
    rgb_matrix_driver.flush();
    for (unsigned i = 0; i < 201; i++) assert(!wire[i].r && !wire[i].g && !wire[i].b);
    puts("Manual TX PASS:20,000 exact frames, 1001 fade phases, all104 full255, truthful unbounded telemetry, padding/suspend black; NO hardware.");
}

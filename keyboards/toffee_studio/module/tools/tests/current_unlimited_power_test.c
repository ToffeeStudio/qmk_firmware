// Same normal firmware, only current-budget scaling removed.
static void test_current_unlimited_power(void) {
    assert(module_power_profile.revision == 12);
    assert(module_power_profile.front_badge_max == 200);
    assert(module_power_led_budget(&module_power_profile) == 865);
    rgb_led_t input[201], original[201], output[201];
    module_power_frame_t result;
    for (unsigned trial = 0; trial < 20000; trial++) {
        for (unsigned i = 0; i < 201; i++) input[i] = (RGB){.r=random32(), .g=random32(), .b=random32()};
        memcpy(original, input, sizeof(input));
        unsigned fade = trial % 1001;
        module_power_limit(&module_power_profile, input, g_led_config.flags, fade, output, &result);
        assert(memcmp(original, input, sizeof(input)) == 0);
        assert(!result.limited && result.limit_scale == UINT16_MAX);
        assert(result.requested_ua == (demand(input, &module_power_profile) + 254) / 255);
        assert(result.effective_ua == (demand(output, &module_power_profile) + 254) / 255);
        for (unsigned i = 0; i < 201; i++) {
            RGB expected = {0};
            if (group(i) >= 0) {
                unsigned max = group(i) == 1 ? 255 : 200;
                expected = (RGB){.r=(input[i].r * max / 255) * fade / 1000,
                    .g=(input[i].g * max / 255) * fade / 1000,
                    .b=(input[i].b * max / 255) * fade / 1000};
            }
            assert(memcmp(&expected, &output[i], sizeof(RGB)) == 0);
        }
    }
    rgb_matrix_driver.init();
    rgb_matrix_driver.set_color_all(255, 255, 255);
    for (unsigned phase = 0; phase <= 1000; phase++) {
        startup = phase; now = phase; rgb_matrix_driver.flush();
        for (unsigned i = 0; i < 201; i++) {
            unsigned max = group(i) < 0 ? 0 : group(i) == 1 ? 255 : 200;
            assert(wire[i].r == max * phase / 1000);
            assert(wire[i].g == wire[i].r && wire[i].b == wire[i].r);
        }
    }
    uint8_t status[25];
    lighting_power_status(status);
    assert(status[0] == 2 && status[1] == (1 | 4 | 16 | 64) && status[24] == 200);
    assert(u16(status + 2) == 12 && u16(status + 18) == 1000);
    assert(u16(status + 16) > 865);
    usb_device_state = USB_DEVICE_STATE_SUSPEND;
    rgb_matrix_driver.flush();
    for (unsigned i = 0; i < 201; i++) assert(!wire[i].r && !wire[i].g && !wire[i].b);
    puts("Normal current-unlimited PASS:20,000 frames, caps200, exact estimates,1001 fade phases,suspend,padding; NO hardware.");
}

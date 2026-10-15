// Actual independent badge renderer; no USB, camera or live LEDs.
static void test_badge_animations(void) {
    saved_available = false; now = 0; lighting_init(); qmk_render();
    now = 2000; qmk_render();
    assert(HAS_FLAGS(g_led_config.flags[17], LED_FLAG_MODIFIER));
    assert(lighting_is_front_led(17));
    badge_config_t config = {1, 200, 40, 255};
    for (unsigned speed = 0; speed <= 255; speed += 51) {
        for (uint8_t effect = 0; effect < MODULE_BADGE_EFFECT_COUNT; effect++) {
            assert(lighting_set_badge_animation(&config, effect, speed));
            module_lighting_state_t before = module_lighting_state;
            for (unsigned tick = 0; tick < 5000; tick += 37) {
                now += 37; qmk_render();
                for (unsigned i = 0; i < 201; i++) {
                    if (g_led_config.flags[i] == LED_FLAG_INDICATOR)
                        assert(rgb_matrix_ws2812_array[i].r <= 200); // Stub exposes HSV.v in r.
                    if (g_led_config.flags[i] == LED_FLAG_UNDERGLOW)
                        assert(rgb_matrix_ws2812_array[i].r == 30);
                }
            }
            assert(memcmp(&before, &module_lighting_state, sizeof(before)) == 0);
        }
    }
    now = UINT32_MAX - 100;
    assert(lighting_set_badge_animation(&config, 2, 0));
    qmk_render(); assert(rgb_matrix_ws2812_array[68].r == 0);
    now += 1000; qmk_render(); uint8_t halfway = rgb_matrix_ws2812_array[68].r;
    now += 1000; qmk_render(); assert(rgb_matrix_ws2812_array[68].r > halfway);
    assert(lighting_set_badge_animation(&config, 3, 255));
    qmk_render(); uint8_t hue = rgb_matrix_ws2812_array[68].b;
    now += 250; qmk_render(); assert(rgb_matrix_ws2812_array[68].b != hue);
    assert(lighting_set_badge_animation(&config, 4, 255));
    qmk_render(); uint8_t start = rgb_matrix_ws2812_array[65].r;
    now += 250; qmk_render(); assert(rgb_matrix_ws2812_array[65].r != start);
    assert(lighting_set_badge_animation(&config, 5, 255));
    qmk_render(); assert(rgb_matrix_ws2812_array[68].r == 0);
    lighting_badge_keypress(); qmk_render(); assert(rgb_matrix_ws2812_array[68].r == 200);
    front_config_t front = {33, 150, 18, 40, 255};
    assert(lighting_set_front(&front)); // Front changes must not reset a badge pulse.
    now += 100; qmk_render(); assert(rgb_matrix_ws2812_array[68].r == 100);
    lighting_badge_keypress(); qmk_render(); assert(rgb_matrix_ws2812_array[68].r == 200);
    now += 200; qmk_render(); assert(rgb_matrix_ws2812_array[68].r == 0);
    config.enabled = 0;
    assert(lighting_set_badge_animation(&config, 5, 255));
    lighting_badge_keypress(); qmk_render(); assert(rgb_matrix_ws2812_array[68].r == 0);
    config.enabled = 1;
    assert(lighting_set_badge(&config));
    assert(module_lighting_state.badge_effect == 1 && module_lighting_state.badge_speed == 100);
    qmk_render(); assert(rgb_matrix_ws2812_array[68].r == 200);
    puts("Badge PASS: 6 modes, speed endpoints, independence, retrigger, off and timer wrap.");
}

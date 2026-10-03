// Actual production startup gate + renderer/parser; only USB/time/storage/QMK
// are host adapters from groups_test.c. Runs for normal and temporary builds.
static void set_usb_state(enum usb_device_state state) {
    usb_device_state = state;
    notify_usb_device_state_change_kb(state);
}

static void assert_black(void) {
    for (unsigned i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        RGB c = rgb_matrix_ws2812_array[i];
        assert(!c.r && !c.g && !c.b);
    }
}

static void assert_uniform(uint8_t value) {
    for (unsigned i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        RGB c = rgb_matrix_ws2812_array[i];
        assert(c.r == value && c.g == value && c.b == value);
    }
}

static void test_startup_lighting(void) {
    const uint32_t delay = MODULE_LIGHTING_STARTUP_DELAY_MS;
    const uint32_t fade = MODULE_LIGHTING_STARTUP_FADE_MS;
    const uint8_t off[5] = {255, 255, 255, 255, 255};
    assert(delay == 1000 && fade == 1000);
    now = 0;
    saved_available = true;
    memset(&saved, 0, sizeof(saved));
    saved.front = (front_config_t){1, 100, 255, 36, 255};
    saved.badge = (badge_config_t){1, 255, 36, 255};
    memset(saved.overrides, 255, sizeof(saved.overrides));
    set_usb_state(USB_DEVICE_STATE_INIT);
    lighting_init();
    assert_black();
    assert(memcmp(&saved, &module_lighting_state, sizeof(saved)) == 0);

    // Even an all-white diagnostic cannot bypass the USB-ready/off gate.
    assert(packet(id_lighting_discovery_all, off, 0) == 0);
    qmk_render(); assert_black();
    now = 60000;
    qmk_render(); assert_black();
    assert(packet(id_lighting_discovery_all, off, 0) == 0); // Refresh test-mode deadline.
    // Loading saved state/USB enumeration time must not consume the rescue window.
    set_usb_state(USB_DEVICE_STATE_CONFIGURED);
    uint32_t ready = now;
    qmk_render(); assert_black();
    now = ready + delay - 1; qmk_render(); assert_black();
    now = ready + delay; qmk_render(); assert_black();

    module_lighting_state_t settings = module_lighting_state;
    animation_config_t rear = g_underglow_config;
    unsigned saves_before = saves;
    // Exact linear ramp on every address, endpoints and repeated fresh frames.
    for (unsigned elapsed = 0; elapsed <= fade; elapsed++) {
        now = ready + delay + elapsed;
        qmk_render();
        assert_uniform(MODULE_LED_DISCOVERY_BRIGHTNESS * elapsed / fade);
        qmk_render();
        assert_uniform(MODULE_LED_DISCOVERY_BRIGHTNESS * elapsed / fade);
    }
    assert(memcmp(&settings, &module_lighting_state, sizeof(settings)) == 0);
    assert(memcmp(&rear, &g_underglow_config, sizeof(rear)) == 0);
    assert(saves == saves_before);
    now = 3; // A later 32-bit timer wrap cannot restart a completed startup.
    assert(packet(id_lighting_discovery_all, off, 0) == 0);
    qmk_render(); assert_uniform(MODULE_LED_DISCOVERY_BRIGHTNESS);

    // Reconfiguration/suspend re-arm the full window, including across wrap.
    set_usb_state(USB_DEVICE_STATE_SUSPEND);
    qmk_render(); assert_black();
    now = UINT32_MAX - 400;
    set_usb_state(USB_DEVICE_STATE_CONFIGURED);
    ready = now;
    assert(packet(id_lighting_discovery_all, off, 0) == 0);
    qmk_render(); assert_black();
    now = ready + delay; qmk_render(); assert_black();
    now = ready + delay + fade / 2;
    qmk_render(); assert_uniform(MODULE_LED_DISCOVERY_BRIGHTNESS / 2);
    now = ready + delay + fade;
    qmk_render(); assert_uniform(MODULE_LED_DISCOVERY_BRIGHTNESS);

    // A real parser all-off command during the delay stays off after the fade.
    set_usb_state(USB_DEVICE_STATE_INIT);
    set_usb_state(USB_DEVICE_STATE_CONFIGURED);
    now = 10000; qmk_render(); assert_black();
    now = 10500;
    assert(packet(id_lighting_discovery, off, sizeof(off)) == 0);
    assert(packet(id_lighting_front_get, off, 0) == 0); // Commands remain serviceable.
    assert(packet(id_lighting_badge_get, off, 0) == 0);
    assert(packet(id_lighting_get_state, off, 0) == 0);
    qmk_render(); assert_black();
    now = 13000; qmk_render(); assert_black();

    // Rescue also interrupts a fade already in progress, without being undone.
    set_usb_state(USB_DEVICE_STATE_INIT);
    set_usb_state(USB_DEVICE_STATE_CONFIGURED);
    now = 15000;
    assert(packet(id_lighting_discovery_all, off, 0) == 0);
    qmk_render(); assert_black();
    now = 16500; qmk_render(); assert_uniform(MODULE_LED_DISCOVERY_BRIGHTNESS / 2);
    assert(packet(id_lighting_discovery, off, sizeof(off)) == 0);
    qmk_render(); assert_black();
    now = 18000; qmk_render(); assert_black();

    // Ordinary front/rear/badge share one multiplier, after per-key dimming.
    lighting_groups_init();
    now = 20000;
    assert(packet(id_lighting_front_set, (uint8_t[]){1, 100, 255, 20, 230}, 5) == 0);
    assert(packet(id_lighting_badge_set, (uint8_t[]){1, 18, 14, 16}, 4) == 0);
    assert(packet(id_lighting_set_led_brightness, (uint8_t[]){0, 127}, 2) == 0);
    qmk_render();
    RGB full[RGB_MATRIX_LED_COUNT];
    memcpy(full, rgb_matrix_ws2812_array, sizeof(full));
    settings = module_lighting_state;
    set_usb_state(USB_DEVICE_STATE_INIT);
    qmk_render(); assert_black();
    set_usb_state(USB_DEVICE_STATE_CONFIGURED);
    ready = now;
    qmk_render(); assert_black();
    now = ready + delay + fade / 2;
    qmk_render();
    for (unsigned i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        RGB c = rgb_matrix_ws2812_array[i];
        assert(c.r == full[i].r / 2 && c.g == full[i].g / 2 && c.b == full[i].b / 2);
    }
    now = ready + delay + fade;
    qmk_render();
    assert(memcmp(full, rgb_matrix_ws2812_array, sizeof(full)) == 0);
    assert(memcmp(&settings, &module_lighting_state, sizeof(settings)) == 0);
    assert(packet(id_lighting_discovery, off, sizeof(off)) == 0);
    qmk_render(); assert_black(); // Also effective after fading has completed.
    puts("Startup PASS: USB-ready 1000ms black, 1000ms linear fade, all 201 addresses/groups, live parser all-off, unchanged settings, reconnect and timer wrap.");
}

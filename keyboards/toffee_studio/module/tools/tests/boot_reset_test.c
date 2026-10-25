// Actual production bootmagic override + lighting initialization/rendering.
// Physical switches, clock and bootloader/EEPROM-reset entry are host adapters.
static bool physical_switches[MATRIX_ROWS][MATRIX_COLS];
static unsigned boot_scans, boot_wait_ms, eeprom_resets;
uint8_t matrix_scan(void) { boot_scans++; return 1; }
bool matrix_is_on(uint8_t row, uint8_t col) { return physical_switches[row][col]; }
void wait_ms(uint16_t milliseconds) { boot_wait_ms += milliseconds; now += milliseconds; }
void eeconfig_disable(void) { eeprom_resets++; }

static void boot_keys(unsigned keys) {
    memset(physical_switches, 0, sizeof(physical_switches));
    physical_switches[7][0] = keys & 1; // Physical left Shift.
    physical_switches[6][6] = keys & 2; // Physical right Shift.
    physical_switches[0][0] = keys & 4; // Physical Escape.
    boot_scans = boot_wait_ms = eeprom_resets = bootloader_entries = 0;
    bootloader_expected = (keys & 4) && (keys & 3) != 3;
    bootmagic_lite();
    bootloader_expected = false;
#if defined(DEBOUNCE) && DEBOUNCE > 0
    assert(boot_scans == 2 && boot_wait_ms == DEBOUNCE * 2);
#else
    assert(boot_scans == 2 && boot_wait_ms == 30);
#endif
}

static void render_after_boot(void) {
    set_usb_state(USB_DEVICE_STATE_CONFIGURED);
    qmk_render(); assert_black();
    now += MODULE_LIGHTING_STARTUP_DELAY_MS + MODULE_LIGHTING_STARTUP_FADE_MS;
    qmk_render();
}

static void test_boot_lighting_reset(void) {
    // Every subset of the three physical switches; mappings never participate.
    for (unsigned keys = 0; keys < 8; keys++) {
        now = 0;
        memset(&saved, 0, sizeof(saved));
        saved.front = (front_config_t){4, 137, 231, 72, 220};
        saved.badge = (badge_config_t){1, 213, 91, 209};
        saved.badge_effect = 4; saved.badge_speed = 167;
        for (unsigned i = 0; i < ARRAY_SIZE(saved.overrides); i++) saved.overrides[i] = i;
        saved_available = true;
        module_lighting_state_t expected = saved;
        unsigned before = saves, flushes = rescue_flushes;
        boot_keys(keys);
        bool reset = (keys & 3) == 3;
        bool bootloader = !reset && (keys & 4);
        assert(eeprom_resets == bootloader && bootloader_entries == bootloader);
        assert(saves == before && rescue_flushes == flushes); // No RGB/storage before init.
        if (bootloader) continue; // Real bootloader_jump() does not return.
        set_usb_state(USB_DEVICE_STATE_INIT);
        lighting_init();
        if (!reset) {
            assert(saves == before && rescue_flushes == flushes);
            assert(memcmp(&expected, &saved, sizeof(saved)) == 0);
            continue;
        }
        expected.front.brightness = expected.badge.brightness = 0;
        assert(saves == before + 1 && rescue_flushes == flushes + 1);
        assert(memcmp(&expected, &saved, sizeof(saved)) == 0);
        assert(memcmp(&expected, &module_lighting_state, sizeof(saved)) == 0);
        assert(g_underglow_config.brightness == 0);
        render_after_boot(); assert_black();
        assert(eeprom_resets == 0 && bootloader_entries == 0);
        // Persisted front/badge zeros reload on a normal boot without the chord.
        boot_keys(0);
        lighting_init();
        assert(memcmp(&expected, &saved, sizeof(saved)) == 0);
        assert(memcmp(&expected, &module_lighting_state, sizeof(saved)) == 0);
        assert(saves == before + 1);
    }
    // Either failed storage record still leaves black output latched after fade.
    for (unsigned failure = 0; failure < 2; failure++) {
        storage_fail = failure == 0; rear_fail = failure == 1;
        boot_keys(3);
        lighting_init();
        render_after_boot(); assert_black();
        assert(eeprom_resets == 0 && bootloader_entries == 0);
    }
    storage_fail = rear_fail = false;
    boot_keys(0); // Leave the old regression suites in ordinary startup mode.
    puts("Boot reset PASS: both physical Shifts, all 8 key subsets, Shift priority over Escape, two debounced scans, black before saves, zero reload, preferences preserved, failed saves stay dark; Escape retains EEPROM reset/bootloader.");
}

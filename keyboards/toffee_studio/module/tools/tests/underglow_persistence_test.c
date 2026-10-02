// Production rear persistence with an explicit byte-array EEPROM adapter.
#include <assert.h>
#include "../../animations/manager.h"
#define EECONFIG_KB_DATA_SIZE 0
#define EECONFIG_USER_DATA_SIZE 0
#define EECONFIG_KEYBOARD ((void *)(uintptr_t)15)
animation_config_t g_underglow_config;
const uint8_t UNDERGLOW_ANIMATION_COUNT = 10;
static uint8_t memory[23];
static uint32_t now;
static unsigned writes;
uint32_t timer_read32(void) {return now;}
uint32_t timer_elapsed32(uint32_t last) {return now - last;}
void underglow_manager_set_anim(uint8_t id) {g_underglow_config.current_animation_id = id;}
static void eeprom_read_block(void *out, const void *address, size_t length) {
    assert((uintptr_t)address == 15 && length == 8);
    memcpy(out, memory + 15, length);
}
static void eeprom_update_block(const void *in, void *address, size_t length) {
    assert((uintptr_t)address == 15 && length == 8);
    memcpy(memory + 15, in, length); writes++;
}
static void eeprom_update_word(uint16_t *address, uint16_t value) {
    assert((uintptr_t)address == 15);
    memcpy(memory + 15, &value, 2); writes++;
}
void save_underglow_config(void);
#include "module_persistence.inc"

int main(void) {
    memset(memory, 0xa5, sizeof(memory));
    const screenless_underglow_storage_t initial = {UNDERGLOW_STORAGE_MAGIC, 1, 0, 100, 11, 36, 255};
    memcpy(memory + 15, &initial, 8);
    load_underglow_config();
    assert(g_underglow_config.brightness == 11 && g_underglow_config.color.h == 36);
    assert(g_underglow_config.speed == 100 && g_underglow_config.current_animation_id == 0);
    save_underglow_config(); assert(writes == 0); // Unchanged record is not rewritten.

    g_underglow_config.brightness = 18;
    now = 100; schedule_underglow_save();
    now = 1099; underglow_persistence_task(); assert(writes == 0);
    now = 1100; underglow_persistence_task();
#ifdef MODULE_EXTENSIVE_TEST
    assert(writes == 0);
    save_underglow_config(); assert(writes == 0);
    now = UINT32_MAX; underglow_persistence_task(); assert(writes == 0);
    assert(memcmp(memory + 15, &initial, 8) == 0);
    load_underglow_config(); assert(g_underglow_config.brightness == 11);
    puts("Temporary rear persistence PASS: saved state loads; debounce and explicit Save write zero EEPROM bytes.");
#else
    assert(writes == 3);
    save_underglow_config(); assert(writes == 3);
    g_underglow_config.brightness = 0;
    load_underglow_config(); assert(g_underglow_config.brightness == 18);
    g_underglow_config.brightness = 7;
    now = UINT32_MAX - 500; schedule_underglow_save();
    now = 498; underglow_persistence_task(); assert(writes == 3);
    now = 499; underglow_persistence_task(); assert(writes == 6);
    for (unsigned i = 0; i < 15; i++) assert(memory[i] == 0xa5);
    // Reject invalid magic, version and effect without replacing current state.
    for (unsigned field = 0; field < 3; field++) {
        memcpy(memory + 15, &initial, 8);
        memory[15 + (field == 0 ? 0 : field == 1 ? 2 : 3)] = 255;
        g_underglow_config.brightness = 42;
        load_underglow_config(); assert(g_underglow_config.brightness == 42);
    }
    puts("Production rear persistence PASS: saved startup; 1000ms debounce; unchanged-write suppression; timer wrap; invalid records; EEPROM boundary preservation.");
#endif
}

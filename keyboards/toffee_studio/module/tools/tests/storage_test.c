// Compile the production journal with a NOR flash simulator and power-cut injection.
#include <assert.h>
#include <setjmp.h>
#include "../../lighting/storage.c"

static uint8_t flash_bytes[8192];
static int budget = -1;
static unsigned programs, erases;
static jmp_buf crash;

static void step(void) {
    if (budget == 0) longjmp(crash, 1);
    if (budget > 0) budget--;
}
const uint8_t *lighting_flash_read(uint32_t offset) {
    assert(offset < sizeof(flash_bytes));
    return flash_bytes + offset;
}
void lighting_flash_program(uint32_t offset, const uint8_t *data) {
    assert(offset % 256 == 0 && offset + 256 <= sizeof(flash_bytes));
    programs++;
    for (unsigned i = 0; i < 256; i++) {step(); flash_bytes[offset + i] &= data[i];}
}
void lighting_flash_erase(uint32_t offset) {
    assert(offset % 4096 == 0 && offset + 4096 <= sizeof(flash_bytes));
    erases++;
    for (unsigned i = 0; i < 4096; i++) {step(); flash_bytes[offset + i] = 255;}
}
static module_lighting_state_t state(uint32_t number) {
    module_lighting_state_t s = {.front = {number % 9, number >> 8, number, number >> 16, 255},
                                 .badge = {number % 2, number + 1, number >> 8, 200},
                                 .badge_effect = number % MODULE_BADGE_EFFECT_COUNT, .badge_speed = number};
    for (unsigned i = 0; i < 104; i++) s.overrides[i] = number + i;
    return s;
}
static void expect_loaded(const module_lighting_state_t *expected) {
    module_lighting_state_t loaded;
    assert(lighting_storage_load(&loaded));
    assert(memcmp(&loaded, expected, sizeof(loaded)) == 0);
}
static void reset(void) {
    memset(flash_bytes, 255, sizeof(flash_bytes));
    module_lighting_state_t loaded;
    assert(!lighting_storage_load(&loaded));
    programs = erases = 0;
}

int main(void) {
    // A real v1 record has the same prefix and 0xFF padding where v2 appends fields.
    reset();
    module_lighting_state_t legacy = state(123);
    legacy.badge_effect = 1; legacy.badge_speed = 100;
    lighting_record_t v1;
    memset(&v1, 255, sizeof(v1));
    v1.magic = LIGHTING_MAGIC; v1.sequence = 1; v1.version = 1;
    memcpy(&v1.state, &legacy, offsetof(module_lighting_state_t, badge_effect));
    v1.crc = checksum((uint8_t *)&v1, 252);
    memcpy(flash_bytes, &v1, 256);
    expect_loaded(&legacy);
    assert(programs == 0 && erases == 0); // Loading never changes flash.
    module_lighting_state_t migrated = legacy;
    migrated.badge_effect = 5; migrated.badge_speed = 220;
    assert(lighting_storage_save(&migrated));
    expect_loaded(&migrated);
    assert(memcmp(flash_bytes, &v1, 256) == 0); // Old recovery record remains intact.
    reset();
    // Append/rollover endurance, rebooted after every save, and unchanged-state wear avoidance.
    for (unsigned i = 0; i < 10000; i++) {
        module_lighting_state_t s = state(i);
        assert(lighting_storage_save(&s));
        expect_loaded(&s);
        unsigned count = programs;
        assert(lighting_storage_save(&s));
        assert(programs == count);
    }
    assert(programs == 10000 && erases == 624);
    // Corruption of every record byte must retain the preceding committed state.
    reset();
    module_lighting_state_t first = state(123), second = state(456);
    assert(lighting_storage_save(&first));
    assert(lighting_storage_save(&second));
    uint8_t baseline[8192];
    memcpy(baseline, flash_bytes, sizeof(baseline));
    for (unsigned i = 0; i < 256; i++) {
        memcpy(flash_bytes, baseline, sizeof(baseline));
        flash_bytes[256 + i] ^= 1;
        expect_loaded(&first);
    }
    // Valid CRC but unknown version/invalid state must be rejected too.
    for (unsigned field = 0; field < 4; field++) {
        memcpy(flash_bytes, baseline, sizeof(baseline));
        lighting_record_t record;
        memcpy(&record, flash_bytes + 256, 256);
        if (field == 0) record.version = 3;
        if (field == 1) record.state.front.effect = MODULE_FRONT_EFFECT_COUNT;
        if (field == 2) record.state.badge.enabled = 2;
        if (field == 3) record.state.badge_effect = MODULE_BADGE_EFFECT_COUNT;
        record.crc = checksum((uint8_t *)&record, 252);
        memcpy(flash_bytes + 256, &record, 256);
        expect_loaded(&first);
    }
    // Cut power at every byte during append and sector rollover, including a partial erase.
    for (unsigned rollover = 0; rollover < 2; rollover++) {
        reset();
        module_lighting_state_t previous;
        unsigned records = rollover ? 32 : 1;
        for (unsigned i = 0; i < records; i++) {
            previous = state(i);
            assert(lighting_storage_save(&previous));
        }
        memcpy(baseline, flash_bytes, sizeof(baseline));
        module_lighting_state_t next = state(50000);
        unsigned bytes = rollover ? 4352 : 256;
        for (unsigned cut = 0; cut <= bytes; cut++) {
            memcpy(flash_bytes, baseline, sizeof(baseline));
            expect_loaded(&previous);
            budget = cut;
            if (setjmp(crash) == 0) assert(lighting_storage_save(&next));
            budget = -1;
            expect_loaded(cut == bytes ? &next : &previous);
        }
    }
    // Sequence wrap-around retains the latest state and permits another save.
    reset();
    assert(lighting_storage_save(&first));
    lighting_record_t record;
    memcpy(&record, flash_bytes, 256);
    record.sequence = UINT32_MAX;
    record.crc = checksum((uint8_t *)&record, 252);
    memcpy(flash_bytes, &record, 256);
    expect_loaded(&first);
    assert(lighting_storage_save(&second));
    expect_loaded(&second);
    puts("Storage PASS: 10,000 saves, 256 corruption cases, 4,610 byte-level power cuts, sequence wrap.");
}

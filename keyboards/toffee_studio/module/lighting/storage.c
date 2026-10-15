#include "groups.h"
#include <string.h>
#ifndef MODULE_LIGHTING_HOST_TEST
#include "hardware/flash.h"
#include "hardware/sync.h"
#endif

// Two reserved sectors, clear of the 8MB legacy filesystem and final 8KB EEPROM.
// Keep all existing QMK/VIA EEPROM offsets and capacities unchanged.
#define LIGHTING_FLASH_BASE (PICO_FLASH_SIZE_BYTES - 65536)
#define LIGHTING_SECTORS 2
#define LIGHTING_PAGE_SIZE 256
#define LIGHTING_SECTOR_SIZE 4096
#define LIGHTING_MAGIC 0x544C4954
#define LIGHTING_VERSION 2

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t sequence;
    uint8_t version;
    module_lighting_state_t state;
    uint8_t padding[252 - 9 - sizeof(module_lighting_state_t)];
    uint32_t crc;
} lighting_record_t;
_Static_assert(sizeof(lighting_record_t) == LIGHTING_PAGE_SIZE, "Lighting record must be one flash page");
_Static_assert(PICO_FLASH_SIZE_BYTES == 16 * 1024 * 1024, "Module lighting storage requires its 16MB flash");
_Static_assert(LIGHTING_FLASH_BASE >= 9 * 1024 * 1024, "Storage must not overlap legacy filesystem");
static int latest_page = -1;
static uint32_t latest_sequence;

#ifdef MODULE_LIGHTING_HOST_TEST
extern const uint8_t *lighting_flash_read(uint32_t offset);
extern void lighting_flash_program(uint32_t offset, const uint8_t *data);
extern void lighting_flash_erase(uint32_t offset);
#else
static const uint8_t *lighting_flash_read(uint32_t offset) {
    return (const uint8_t *)(XIP_BASE + LIGHTING_FLASH_BASE + offset);
}
static void lighting_flash_program(uint32_t offset, const uint8_t *data) {
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_program(LIGHTING_FLASH_BASE + offset, data, LIGHTING_PAGE_SIZE);
    restore_interrupts(interrupts);
}
static void lighting_flash_erase(uint32_t offset) {
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(LIGHTING_FLASH_BASE + offset, LIGHTING_SECTOR_SIZE);
    restore_interrupts(interrupts);
}
#endif

static uint32_t checksum(const uint8_t *data, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

bool lighting_storage_load(module_lighting_state_t *state) {
    latest_page = -1;
    latest_sequence = 0;
    for (int page = 0; page < LIGHTING_SECTORS * LIGHTING_SECTOR_SIZE / LIGHTING_PAGE_SIZE; page++) {
        lighting_record_t record;
        memcpy(&record, lighting_flash_read(page * LIGHTING_PAGE_SIZE), sizeof(record));
        if (record.magic != LIGHTING_MAGIC || (record.version != 1 && record.version != LIGHTING_VERSION) ||
            record.crc != checksum((const uint8_t *)&record, 252))
            continue;
        if (record.version == 1) {
            record.state.badge_effect = 1;
            record.state.badge_speed = 100;
        }
        if (record.state.front.effect >= MODULE_FRONT_EFFECT_COUNT || record.state.badge.enabled > 1 ||
            record.state.badge_effect >= MODULE_BADGE_EFFECT_COUNT) continue;
        if (latest_page < 0 || (int32_t)(record.sequence - latest_sequence) > 0) {
            *state = record.state;
            latest_page = page;
            latest_sequence = record.sequence;
        }
    }
    return latest_page >= 0;
}

bool lighting_storage_save(const module_lighting_state_t *state) {
    if (latest_page >= 0) {
        lighting_record_t previous;
        memcpy(&previous, lighting_flash_read(latest_page * LIGHTING_PAGE_SIZE), sizeof(previous));
        if (memcmp(&previous.state, state, sizeof(*state)) == 0) return true;
    }
    int sector = latest_page < 0 ? 0 : latest_page / 16;
    int page = latest_page < 0 ? 0 : latest_page + 1;
    const uint8_t *target = page < 32 ? lighting_flash_read(page * LIGHTING_PAGE_SIZE) : NULL;
    bool erased = target != NULL && page / 16 == sector;
    for (int i = 0; erased && i < LIGHTING_PAGE_SIZE; i++) erased = target[i] == 0xFF;
    if (!erased) {
        sector = latest_page < 0 ? 0 : 1 - sector;
        lighting_flash_erase(sector * LIGHTING_SECTOR_SIZE);
        page = sector * 16;
    }
    lighting_record_t record;
    memset(&record, 0xFF, sizeof(record));
    record.magic = LIGHTING_MAGIC;
    record.version = LIGHTING_VERSION;
    record.sequence = latest_sequence + 1;
    record.state = *state;
    record.crc = checksum((const uint8_t *)&record, 252);
    lighting_flash_program(page * LIGHTING_PAGE_SIZE, (const uint8_t *)&record);
    if (memcmp(lighting_flash_read(page * LIGHTING_PAGE_SIZE), &record, sizeof(record)) != 0) return false;
    latest_page = page;
    latest_sequence = record.sequence;
    return true;
}

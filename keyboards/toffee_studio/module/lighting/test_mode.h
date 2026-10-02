#pragma once
#include "quantum.h"

#define MODULE_TEST_CHANNEL_CAP 18
#define MODULE_TEST_TIMEOUT_MS 5000
enum {MODULE_TEST_OFF, MODULE_TEST_SELECTED, MODULE_TEST_FRONT_WHITE,
      MODULE_TEST_ALL_WHITE, MODULE_TEST_REAR_RAINBOW, MODULE_TEST_GROUPS};
void lighting_test_init(void);
void lighting_test_pattern(uint8_t pattern, uint8_t groups);
void lighting_test_keepalive(void);
void lighting_test_apply_limits(void);
void lighting_test_status(uint8_t *payload);

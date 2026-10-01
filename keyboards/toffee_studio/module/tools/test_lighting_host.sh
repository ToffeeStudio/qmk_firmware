#!/bin/sh
# Host-only tests; no USB access or firmware flashing.
set -eu
module_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d /tmp/toffee-lighting-host.XXXXXX)
cd "$module_dir"
clang -std=c11 -O1 -g -fsanitize=address,undefined -DMODULE_LIGHTING_HOST_TEST \
  -DPICO_FLASH_SIZE_BYTES=16777216 -I tools/tests/include \
  tools/tests/storage_test.c -o "$test_dir/storage"
"$test_dir/storage"
clang -std=c11 -O1 -g -fsanitize=address,undefined -DMODULE_LED_DISCOVERY \
  -I tools/tests/include -I . tools/tests/groups_test.c lighting/groups.c lighting/lighting.c \
  -o "$test_dir/groups"
"$test_dir/groups"
printf 'Test executables retained at %s\n' "$test_dir"

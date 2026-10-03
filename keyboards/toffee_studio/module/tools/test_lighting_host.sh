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
# Compile the production lighting handlers and complete dispatcher verbatim.
# Uninitialized legacy display/filesystem adapters are traps in parser_test.c.
sed -n '/^static int parse_set_animation(/,$p' rawhid/module_raw_hid.c > "$test_dir/module_parser.inc"
test -s "$test_dir/module_parser.inc"
clang -std=c11 -O1 -g -fsanitize=address,undefined -DMODULE_LED_DISCOVERY \
  -I tools/tests/include -I ../../../tmk_core/protocol -I . -I "$test_dir" tools/tests/groups_test.c lighting/groups.c lighting/lighting.c \
  -o "$test_dir/groups"
"$test_dir/groups"
clang -std=c11 -O1 -g -fsanitize=address,undefined -DMODULE_LED_DISCOVERY \
  -DMODULE_EXTENSIVE_TEST -DMODULE_TEST_BUILD_ID=0x26100202 \
  -I tools/tests/include -I ../../../tmk_core/protocol -I . -I "$test_dir" tools/tests/groups_test.c \
  lighting/groups.c lighting/lighting.c lighting/test_mode.c -o "$test_dir/temporary"
"$test_dir/temporary"
# Compile the actual rear persistence functions, excluding unrelated display I/O.
sed -n '/^extern animation_t\* underglow_animations\[\];/,$p' persistence.c > "$test_dir/module_persistence.inc"
test -s "$test_dir/module_persistence.inc"
for mode in production temporary; do
  mode_define=""
  if [ "$mode" = temporary ]; then mode_define="-DMODULE_EXTENSIVE_TEST"; fi
  clang -std=c11 -O1 -g -fsanitize=address,undefined -DMODULE_LED_DISCOVERY $mode_define \
    -I tools/tests/include -I . -I "$test_dir" tools/tests/underglow_persistence_test.c \
    -o "$test_dir/rear-$mode"
  "$test_dir/rear-$mode"
done
printf 'Test executables retained at %s\n' "$test_dir"

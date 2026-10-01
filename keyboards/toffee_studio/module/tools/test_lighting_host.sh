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
  -I tools/tests/include -I . -I "$test_dir" tools/tests/groups_test.c lighting/groups.c lighting/lighting.c \
  -o "$test_dir/groups"
"$test_dir/groups"
printf 'Test executables retained at %s\n' "$test_dir"

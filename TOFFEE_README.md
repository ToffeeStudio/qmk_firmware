# Toffee firmware documentation

Start with the [Module board README](keyboards/toffee_studio/module/readme.md)
for current status, board identification, build/flash instructions, recovery
artifacts and source-code entry points. Read [AGENTS.md](AGENTS.md) for local
hardware safeguards and the remote Kali read-only policy.

## Source orientation

Custom Raw HID reports enter `raw_hid_receive()` in `quantum/via.c`, reach
`via_custom_value_command_kb()` in `keyboards/toffee_studio/module/module.c`,
and dispatch through `module_raw_hid_parse_packet()` in
`keyboards/toffee_studio/module/rawhid/module_raw_hid.c`. The custom report
starts with VIA byte `0x09`, followed by the command defined in
`module_raw_hid.h`. The current parser uses a switch; screenless mode restricts
which commands are accepted.

The tree also retains older littlefs/LVGL changes in `lib/` and flash-storage
integration in `platforms/chibios/drivers/littlefs/file_system.c`. Those are
legacy screen-equipped-board context: their presence does not mean the
screenless build mounts the filesystem or starts the display. Current lighting
persistence uses EEPROM, as documented in the board README.

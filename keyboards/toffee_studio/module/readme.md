# Toffee Studio Module — start here

Last reviewed: **1 October 2026 (Australia/Sydney)**. This is the current board
handover, not a live device-status feed. Read the repository's
[agent safety instructions](../../../AGENTS.md) before working on hardware.

## Current status

The current target is the **screenless RP2040 Module PCB**. The local firmware
is on `screenless-pcb`; the paired [board configurator](../../../../board-configurator/README.md)
is on `lighting-fix`. The latest work is local and uncommitted, including
untracked documentation/tools/tests; a fresh clone does not contain it yet.

| Area | Latest recorded status |
| --- | --- |
| Rear underglow | Ten effects plus speed, brightness and colour control implemented for the 22 verified rear LEDs. Changes auto-save after one second without another adjustment; startup restores that record. |
| Front and badge | Off during normal underglow operation. Diagnostic presets remain available. Production front/per-key/badge controls have not been integrated for this screenless map. |
| Configurator | Underglow reports disconnects, firmware rejection and timeouts, with Retry instead of indefinite loading. HID commands are queued within each Toffee HID wrapper. |
| Latest flash | The unchanged underglow candidate was copied to the identified RP2 drive on 30 September. Keyboard and raw HID returned; 15-second uptime check and keymap/macro/layout comparison passed. A later read still responded at approximately 89 seconds uptime. |
| Physical verification | **Pending for that latest underglow flash:** typing, visible rear lighting, browser control and unplug/replug persistence. USB checks alone do not prove these. |
| Recovery | The pre-change image was restored earlier, and Ethan explicitly confirmed both lights and typing. That confirmation applies to the recovery image, not the subsequent underglow candidate. Both images remain preserved outside the build directory. |
| Earlier outage | Cause unconfirmed. A stalled macOS bootloader volume was observed and cleared by a targeted USB connection reset. Neither a firmware defect nor a transfer defect has been established as the original cause. |

Do not remove `MODULE_LED_DISCOVERY` just to enable lighting: it also keeps
the old screen/filesystem startup disabled. Legacy source and build dependencies
remain in the tree; this pass did not remove them or restore display support.

## Where things live

The local workspace is `/Users/ethanhan/Desktop/projects/toffee/`:

- `firmware/` — this independent QMK repository; board source is `keyboards/toffee_studio/module/`.
- `board-configurator/` — VIA fork for configuring the physical board; not the shopfront.
- `shopfront/` — customer product customisation and purchase flow, with frontend/backend together.
- `ARCHIVE/legacy/display-service/` — former `api_test` host software for the screen-equipped PCB; reference only for the current workflow.
- `ARCHIVE/firmware-recovery/` — preserved UF2 images, private VIA snapshots and flash reports; local files outside this repository.
- `toffee.code-workspace` — opens the active repositories; the legacy entry still references its pre-archive path and needs updating.

The parent [AGENTS.md](../../../../AGENTS.md) is the workspace-wide agent entry point.

The former `/Users/ethanhan/Desktop/projects/qmk_firmware` path is stale.
Run builds from this checkout explicitly; the QMK CLI home or an editor's saved
project can still point elsewhere. Remote Kali is a read-only reference, not
the local USB flashing target; its access rules remain in `AGENTS.md`.

## Work on the board

From `/Users/ethanhan/Desktop/projects/toffee/firmware`:

```sh
make toffee_studio/module:via -j1
```

Output: `toffee_studio_module_via.uf2` in the repository root. Rebuilding can
replace that file; it must never replace the preserved recovery images.

The bootloader accepts an ordinary **`.uf2` file copy** onto `RPI-RP2`, including
Finder drag-and-drop. A custom USB programmer is not required. The local updater
automates bootloader entry, validated copying and post-copy USB checks. Follow
the [current flashing and recovery instructions](documentation/led_discovery.md#automatic-usb-firmware-updates);
close the configurator and connect only the intended Module before flashing.
Do not flash merely to refresh documentation or inspect status.

For browser development, see the [configurator README](../../../../board-configurator/README.md).
The development URL is `http://localhost:5173`; a past successful launch does
not guarantee that a server is still running in a later session.

## Next verification and remaining work

1. Confirm physical typing and rear lighting on the latest underglow build.
2. Reconnect in the local configurator and exercise the underglow controls.
3. After an adjustment, wait more than one second, unplug/replug, and confirm
   both the saved lighting and typing return. Record this separately from a
   software reboot/reflash test.
4. Keep the earlier outage unresolved unless further evidence identifies its cause.
5. Front/per-key/badge integration, removal of legacy display code, unrelated
   TypeScript errors and shopfront work remain separate tasks.

## Appendix — board identity and implementation

| Item | Value / source |
| --- | --- |
| MCU and bootloader | RP2040; [info.json](info.json) |
| Normal USB identity | VID `0x1067`, PID `0x626D`, product Module |
| Raw HID | Usage page `0xFF60`, usage `0x61`; 32-byte reports |
| Bootloader USB | VID `0x2E8A`, PID `0x0003`; verified unit serial `E0C9125B0D9B` |
| Bootloader volume | Observed `/Volumes/RPI-RP2`, `INFO_UF2.TXT` reports `Board-ID: RPI-RP2`; recheck before copying |
| Key matrix | 9 rows × 8 columns, 2 VIA layers; pins/layout in `info.json`, layer count in [config.h](config.h) |
| LED data | WS2812 on `GP13` |
| Verified mapped LEDs | 104: 22 rear + 66 regular front + 16 badge. The configured 201 raw diagnostic addresses are **not** a physical count. |

Current front/rear index tables are in [lighting/lighting.c](lighting/lighting.c).
Physical mapping evidence is local and outside this Git repository:
[front map](/Users/ethanhan/Desktop/projects/toffee-accounting/discord_capture/led_discovery/front_map_2026-09-25.md)
and [rear map / original coordinates](/Users/ethanhan/Desktop/projects/toffee-accounting/discord_capture/led_discovery/back_map_2026-09-26.md).
Do not infer new LED positions from old key maps. Index 103 is the repaired
equals-key LED; older notes describing it as unresolved are superseded.

Implementation entry points:

- [module.c](module.c): startup and custom VIA dispatch entry point.
- [rawhid/module_raw_hid.c](rawhid/module_raw_hid.c) and [header](rawhid/module_raw_hid.h): command parsing/status. Screenless underglow supports `0x71`, `0x72`, `0x74`, `0x75`, `0x76`, `0x7A`; bootloader command is `0x79`; diagnostics are `0x7C`–`0x7F`. Display/filesystem and per-key brightness `0x7B` are rejected in this mode.
- [animations/manager.c](animations/manager.c): effect IDs 0–9 and common brightness scaling. Five effects were corrected to avoid applying brightness twice.
- [persistence.c](persistence.c): one-second firmware-side debounce; versioned eight-byte EEPROM record in unused keyboard/user slots 15–22, without moving VIA keymaps/macros. Unchanged records are not rewritten. The old littlefs lighting file is not migrated.
- [lighting/lighting.c](lighting/lighting.c): saved-state startup and rear-only rendering. With no valid record, starts Solid with speed 100, brightness 18, hue 170, saturation 255. Diagnostic presets temporarily override rendering; an underglow setter or restart resumes normal lighting.
- [tools/flash_module.py](tools/flash_module.py): explicit volume/serial, immutable candidate archive, private VIA snapshot, bounded copy, and verification. The snapshot covers keymaps/macros/layout, not a full flash or lighting-state backup. One targeted USB reset is allowed on a pre-copy read timeout; a copy failure is not blindly retried.

### Preserved artifacts and latest evidence

Recovery directory: `/Users/ethanhan/Desktop/projects/toffee/ARCHIVE/firmware-recovery/`.
These are local artifacts, not automatically included in Git. Never publish
`module-settings.json`; macro contents may contain personal text.
The recovery directory has moved under `ARCHIVE/`; original absolute paths
inside historical flash reports describe the location at the time of the run.

- **Known-working pre-change UF2:** [pre-underglow-8c65e8ad.uf2](../../../../ARCHIVE/firmware-recovery/2026-09-30-pre-underglow/pre-underglow-8c65e8ad.uf2), 143360 bytes. SHA256 `8c65e8ad26255d3a16f7a8de709344917c0fba663cfc0ea2114e4d91031979c6`. Read-only copy of the pre-change local build, not full flash readback. **Do not overwrite or delete.**
- **Latest underglow candidate:** [archived UF2](../../../../ARCHIVE/firmware-recovery/20260930T133548Z-f7170b48/7e3574d7a6fc0bbc3a3e070b6e7a51db9d66f675f1571859a213a6229d86961e.uf2), 168448 bytes. SHA256 `7e3574d7a6fc0bbc3a3e070b6e7a51db9d66f675f1571859a213a6229d86961e`.
- **Latest run:** [flash-report.json](../../../../ARCHIVE/firmware-recovery/20260930T133548Z-f7170b48/flash-report.json), `stage: usb_verified`, no USB reset needed on that retry, human confirmation pending. GET underglow returned `[0, 100, 18, 170, 255]` afterward.

Recorded checks from the underglow work, not a new hardware test on this documentation date:

- Normal QMK build passed with existing warnings.
- Ten updater unit tests passed, including bad UF2/serial rejection, bounded timeout handling, missing keyboard interface, reboot detection and no automatic copy retry.
- Seven configurator HID tests / 53 assertions passed; Vite production build passed. The project-wide TypeScript check still had 13 pre-existing errors outside this fix.
- Earlier live command checks accepted all ten effects, rejected invalid effect 255 and blocked filesystem access. Debounced lighting settings survived a reflash/reboot test, and keymaps/macros matched. That does not substitute for the pending physical cold-boot check of the latest retry.

Host-only updater regression command, from the firmware root (does not flash):

```sh
/Users/ethanhan/.pyenv/shims/python3 -m unittest discover -s keyboards/toffee_studio/module/tools -p test_flash_module.py -v
```

### Historical references

- [LED discovery log and current flashing procedure](documentation/led_discovery.md): dated mapping/deployment evidence; older startup modes and counts are historical.
- [Per-key brightness design](documentation/per_key_led_brightness.md): legacy 89-LED implementation, **not** the current screenless mapping or support status.
- Legacy host tools in `ARCHIVE/legacy/display-service/`: `main.py` for Raw HID commands, `receive_cdc.py` for streamed files, `find_cdc.py` for CDC discovery. Do not run display/filesystem operations against the screenless board as a diagnostic shortcut.

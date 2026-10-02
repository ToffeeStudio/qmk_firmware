# Toffee Studio Module — start here

Last reviewed: **2 October 2026 (Australia/Sydney)**. This is the current board
handover, not a live device-status feed. Read the repository's
[agent safety instructions](../../../AGENTS.md) before working on hardware.

## Current status

The current target is the **screenless RP2040 Module PCB**. The local firmware
is on `screenless-pcb`; the paired [board configurator](../../../../board-configurator/README.md)
is on `lighting-fix`. Starting firmware is preserved in checkpoint `471710689a`.
Independent screenless front/badge/per-key integration is committed as
`015cf1ebed` and deployed to exposed **Module B only**. Live API, actual-app
headless browser, cameras and saved-state reboot checks pass. No push.

| Area | Latest recorded status |
| --- | --- |
| Rear underglow | Ten effects plus speed, brightness and colour control implemented for the 22 verified rear LEDs. Changes auto-save after one second without another adjustment; startup restores that record. |
| Front and badge | Off plus eight front effects, saved dimming for 66 regular front LEDs and an independent 16-LED badge with on/off, brightness and solid colour. Badge effects are deferred. Cameras confirm independent front/badge/rear operation; 65 functioning front packages respond at their expected positions. |
| Configurator | Four menus: Underglow, Front Lighting, Per-Key LED, Badge. Protocol-v2 matched replies, explicit errors and Retry. Shared custom queue; VIA ignores tagged replies and retains replies arriving before its send finishes. Headless actual-app controls, reload and rejection/Retry passed through a real-board native JavaScript HID bridge. |
| Latest flash | Lighting-v2 UF2 copied successfully to Module B's identified RP2 drive on 1 October, followed by two same-image reboot/reflash persistence checks. All three runs passed keyboard/raw HID enumeration, 15-second uptime and VIA keymap/macro/layout verification, without a USB-reset workaround. |
| Saved startup | Distinctive states and all 66 overrides survived the earlier software reboot/reflash checks. Ethan subsequently requested orange-yellow: hue 36/saturation 255, front/rear brightness 11, badge brightness 2, speed 100, overrides 255. Fresh reads after his 2 October hub reconnect match exactly; no test colours were written in the new campaign. |
| Physical verification | Ethan reported a hub unplug/replug; enumeration, saved settings and cameras pass afterwards, but actual VBUS removal was not measured. **Controlled cold cycles and physical typing remain pending.** The equals LED's formerly working manual bridge is now broken, per Ethan; its dark state is excluded as a hardware fault, not a new indexing/firmware regression. |
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
close the configurator and select the intended Module's explicit USB location
and measured bootloader serial before flashing. Module B (`0x01142000`) is the
only authorised target in the current two-board session.
Do not flash merely to refresh documentation or inspect status.

For browser development, see the [configurator README](../../../../board-configurator/README.md).
The development URL is `http://localhost:5173`; a past successful launch does
not guarantee that a server is still running in a later session.

## Next verification and remaining work

**2 October extensive-plan checkpoint:** see the
[new execution log](../../../../AUTORESEARCH/02-09-2026%20Extensive%20Test%20Plan/README.md).
Production/temporary parsers pass 4,312/4,315 sanitizer-tested attempts. Storage,
groups, actual rear persistence, 12 updater tests, 27 JavaScript tests (1,747
assertions) and normal/temporary QMK builds pass. The runner's seven offline
simulations cover all 312 RGB checks, group combinations, 290 invalid commands,
camera failure, wrong firmware, failed all-off and controller loss; simulated
images are not PCB evidence. New camera/read-only checks after Ethan reconnected
the hubs identify the same units with unchanged lighting/VIA data and refreshed
HID handles. Rear framing changed; recalibrate before image scoring.

Ethan explicitly approved the temporary settings and instructed us to proceed
without more firmware-change approval questions. `MODULE_EXTENSIVE_TEST=yes`
with required `MODULE_TEST_BUILD_ID=0x26100202` implements startup off, final RGB
channel clamp 18/255, five-second loss timeout and RAM-only lighting changes.
Status `0x86` is read-only; `0x87` is keepalive. A late heartbeat cannot re-light
an expired pattern. Both front/badge and rear saved records stay unchanged.
Tests found/fixed a deadline race in the earlier `0x26100201` candidate: retain
its failed-test history but **do not deploy that image**. New candidate SHA256
`e67ce04bc1bfe5b787505a9d4b3171c0f4a8ba393edc08da21d6be7390d117c0`,
172,544 bytes, is archived beside the other recovery images.

**Neither temporary candidate has been flashed.** No powered pattern writes ran
in this campaign. The remaining prerequisite is the plan's unplugged physical
safety/support inspection, not firmware permission. Normal saved-startup
behaviour is unchanged. A final normal rebuild is byte-identical to the original
campaign baseline and excludes test-mode symbols. The root UF2 is that normal
build, SHA256
`bfda11cfff92e811b56daded8e9713afa16e62e5cfbe66f147ee7482204a0a02`,
173,056 bytes, preserved with private snapshots in
`ARCHIVE/firmware-recovery/20261002-pre-extensive-S71MzY/`; it is not an
installed-image attestation. The previously deployed and pre-change images
remain preserved. Full frontend TypeScript still has the 11 pre-existing errors.

1. Obtain the plan's unplugged fixture inspection, then controlled cold cycles
   and typing confirmation on exposed Module B;
   a bare PCB without switches may not allow a typing test. Keep software
   reboot/reflash evidence separate from actual power removal.
2. Keep the equals LED at index 103. Ethan says he repaired it previously and
   accidentally broke the repair. Do not resolder, renumber or remove it as part
   of the lighting-software task.
3. Keep the earlier outage unresolved unless new evidence identifies its cause.
   Badge effect design, legacy display removal, unrelated TypeScript errors and
   shopfront work remain separate. See the
   [session log](../../../../AUTORESEARCH/01-09-2026%20Light%20testing/README.md)
   for exact automated tests, original images and their limitations.

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
Do not infer new LED positions from old key maps. Index 103 is the equals LED,
verified after its original repair on 25 September. On 1 October Ethan reported
accidentally breaking that repair; its current dark state does not change indices.

Implementation entry points:

- [module.c](module.c): startup and custom VIA dispatch entry point.
- [rawhid/module_raw_hid.c](rawhid/module_raw_hid.c) and [header](rawhid/module_raw_hid.h): command parsing/status. Rear: `0x71`, `0x72`, `0x74`, `0x75`, `0x76`, `0x7A`; per-key set/get: `0x7B`/`0x84`; front get/set: `0x80`/`0x81`; badge get/set: `0x82`/`0x83`; capabilities: `0x85` → `[2,66,22,16,104,9]`. Screenless replies carry `A5`, command, little-endian packet ID in bytes 26–31. Bootloader is `0x79`; diagnostics `0x7C`–`0x7F`. Display/filesystem remain rejected.
- [animations/manager.c](animations/manager.c): effect IDs 0–9 and common brightness scaling. Five effects were corrected to avoid applying brightness twice.
- [persistence.c](persistence.c): one-second firmware-side debounce; versioned eight-byte EEPROM record in unused keyboard/user slots 15–22, without moving VIA keymaps/macros. Unchanged records are not rewritten. The old littlefs lighting file is not migrated.
- [lighting/lighting.c](lighting/lighting.c), [groups.c](lighting/groups.c), [screenless map](lighting/screenless-map.json): separate front/badge/rear rendering. Unmapped slots are black. No valid record: front solid white at brightness 18/speed 100, badge white at brightness 18, rear solid blue at brightness 18/speed 100. Diagnostic presets temporarily override rendering; a group/lighting setter or restart resumes normal lighting.
- [lighting/storage.c](lighting/storage.c): front/badge/per-key persistence in two 4KB sectors at `0xFF0000`–`0xFF1FFF`; 256-byte CRC32 records append across sector rollover. A prior valid record is retained while the other sector is erased. Existing VIA EEPROM offsets and macro capacity are unchanged. The 16MB flash requirement is compile-time checked.
- [tools/flash_module.py](tools/flash_module.py): explicit volume/serial, immutable candidate archive, private VIA snapshot, bounded copy, and verification. The snapshot covers keymaps/macros/layout, not a full flash or lighting-state backup. One targeted USB reset is allowed on a pre-copy read timeout; a copy failure is not blindly retried.

### Preserved artifacts and latest evidence

Recovery directory: `/Users/ethanhan/Desktop/projects/toffee/ARCHIVE/firmware-recovery/`.
These are local artifacts, not automatically included in Git. Never publish
`module-settings.json`; macro contents may contain personal text.
The recovery directory has moved under `ARCHIVE/`; original absolute paths
inside historical flash reports describe the location at the time of the run.

- **Known-working pre-change UF2:** [pre-underglow-8c65e8ad.uf2](../../../../ARCHIVE/firmware-recovery/2026-09-30-pre-underglow/pre-underglow-8c65e8ad.uf2), 143360 bytes. SHA256 `8c65e8ad26255d3a16f7a8de709344917c0fba663cfc0ea2114e4d91031979c6`. Read-only copy of the pre-change local build, not full flash readback. **Do not overwrite or delete.**
- **Historical underglow-only candidate:** [archived UF2](../../../../ARCHIVE/firmware-recovery/20260930T133548Z-f7170b48/7e3574d7a6fc0bbc3a3e070b6e7a51db9d66f675f1571859a213a6229d86961e.uf2), 168448 bytes. SHA256 `7e3574d7a6fc0bbc3a3e070b6e7a51db9d66f675f1571859a213a6229d86961e`.
- **Current lighting-v2 candidate:** [archived UF2](../../../../ARCHIVE/firmware-recovery/20261001-module-b-before-full-lighting/be2915808f3de406cbe283f96049aab3ec9b6c89a2e5d64237833bd144849845.uf2), **173056 bytes**, from `015cf1ebed`. SHA256 `be2915808f3de406cbe283f96049aab3ec9b6c89a2e5d64237833bd144849845`.
- **First lighting-v2 deployment:** [report](../../../../ARCHIVE/firmware-recovery/20261001T124829Z-361645fb/flash-report.json). Module B's bootloader serial `E0C9125B0D9B` was measured at the same `0x01142000` connection before copying. Stage `usb_verified`; keymaps/macros/layout match the private pre-flash snapshot.
- **Saved-state restart checks:** [public report](../../../../AUTORESEARCH/01-09-2026%20Light%20testing/reboot-live-01.json), with [distinctive-state flash](../../../../ARCHIVE/firmware-recovery/20261001T133319Z-0eda0f62/flash-report.json) and [restored-original-state flash](../../../../ARCHIVE/firmware-recovery/20261001T133358Z-d5f622b1/flash-report.json). Both stages `usb_verified`, no pre-copy USB reset, all 66 overrides and group states match after startup. Module A's VIA settings also remain unchanged. Physical power removal/typing is a separate pending check.
- **Actual hardware and app evidence:** [470 live API checks](../../../../AUTORESEARCH/01-09-2026%20Light%20testing/live-api-01.json), [headless actual-app check](../../../../AUTORESEARCH/01-09-2026%20Light%20testing/browser-live-01.json), [8 front/back group captures](../../../../AUTORESEARCH/01-09-2026%20Light%20testing/camera-groups-01/manifest.json), [66 individual-key cases](../../../../AUTORESEARCH/01-09-2026%20Light%20testing/camera-keys-01/manifest.json), [physical-position analysis](../../../../AUTORESEARCH/01-09-2026%20Light%20testing/camera-keys-01/analysis-physical.json). 65 working front packages emit at their expected archived physical positions; 103 remains dark with its broken bridge. Numerical analysis does not certify the absence of reflected light or exact colour/brightness calibration.

Earlier underglow checks (historical; not proof of the new firmware):

- Normal QMK build passed with existing warnings.
- Ten updater unit tests passed, including bad UF2/serial rejection, bounded timeout handling, missing keyboard interface, reboot detection and no automatic copy retry.
- Seven configurator HID tests / 53 assertions passed; Vite production build passed. The project-wide TypeScript check still had 13 pre-existing errors outside this fix.
- Earlier live command checks accepted all ten effects, rejected invalid effect 255 and blocked filesystem access. Debounced lighting settings survived a reflash/reboot test, and keymaps/macros matched. That does not substitute for the pending physical cold-boot check of the latest retry.

Host-only updater regression command, from the firmware root (does not flash):

```sh
/Users/ethanhan/.pyenv/shims/python3 -m unittest discover -s keyboards/toffee_studio/module/tools -p test_flash_module.py -v
sh keyboards/toffee_studio/module/tools/test_lighting_host.sh
```

New checkpoint checks: 12 updater tests, 17 frontend HID/map tests (1,068
assertions), QMK/Vite builds, and sanitizer-enabled production storage/group
tests passed. These cover 10,000 saves, 256 corruptions, 4,610 power-cut cases,
16,896 per-key levels, debounce and sequence/timer wrap. Host QMK/rear adapters
are stubs; the separate live/camera checks above supply physical evidence. The full
frontend TypeScript check retains 11 unrelated legacy errors.

### Historical references

- [LED discovery log and current flashing procedure](documentation/led_discovery.md): dated mapping/deployment evidence; older startup modes and counts are historical.
- [Per-key brightness design](documentation/per_key_led_brightness.md): legacy 89-LED implementation, **not** the current screenless mapping or support status.
- Legacy host tools in `ARCHIVE/legacy/display-service/`: `main.py` for Raw HID commands, `receive_cdc.py` for streamed files, `find_cdc.py` for CDC discovery. Do not run display/filesystem operations against the screenless board as a diagnostic shortcut.

# Module LED discovery and flashing record

Status index updated **1 October 2026 (Australia/Sydney)**. The
[board README](../readme.md) is the authoritative current handover, including
board identity, source entry points, latest artifact hashes and pending checks.

The latest underglow retry completed via ordinary UF2 copying: both USB
interfaces returned, VIA uptime increased for 15 seconds, and keymap/macros/layout
matched the snapshot. The [run report](../../../../../ARCHIVE/firmware-recovery/20260930T133548Z-f7170b48/flash-report.json)
still has physical typing/lighting confirmation **pending**. Ethan's earlier
“both lights and typing work” confirmation was for the restored pre-change
image, not this later retry. The cause of the original outage remains unresolved.

Use [Automatic USB firmware updates](#automatic-usb-firmware-updates) below
for the current updater. Other discovery procedures and dated observations are
retained as history: old 89/112/151-address descriptions, all-white startup,
`--preserve-startup`, old UF2 hashes and “left on” notes are not current state.
Current mapping is 104 physical LEDs with 201 configured diagnostic slots;
startup restores saved rear underglow. Front/per-key/badge production integration
remains pending.

> Underglow update — 30 September 2026: the screenless build now supports the
> configurator's effect, speed, brightness and colour controls (`71`, `72`, `74`,
> `75`, `76`). Changes auto-save in firmware after **one second without another
> lighting change**, and startup restores the saved settings on the **22 verified
> rear LEDs**. Front/badge stay off in underglow mode. New/uninitialised storage
> starts with Solid at 18/255; the old display-filesystem lighting file is not
> migrated or accessed. No filesystem mounting, formatting or screen startup is
> re-enabled. EEPROM bytes 15–22 (the unused keyboard/user config slots) store a
> versioned lighting record without moving VIA keymaps/macros. `7A` can save
> immediately, but the configurator uses the firmware debounce and needs no Save
> button. Diagnostic presets still work temporarily; an underglow setter resumes
> normal lighting. The updater now always leaves startup lighting untouched;
> it sends no diagnostic lighting preset. Earlier startup notes below are historical.

> Rear update — 26 September 2026: all 22 rear indices verified with the original
> coordinates preserved. `tools/show_back.py` runs the rear-only XY rainbow;
> `--phase 0` freezes it. Front/badge remain off during this preset.

> Current update — 25 September 2026: the manually repaired equals-key LED is
> **index 103 / R1C13**, verified independently in red, blue and green at 18/255.
> `show_front.py` now lights **82** front packages (66 regular + 16 badge),
> including 103. Startup still lights **0–200 inclusive** white at 18/255.
> The 201-address diagnostic range is not a physical-count measurement.
> [Current map](/Users/ethanhan/Desktop/projects/toffee-accounting/discord_capture/led_discovery/front_map_2026-09-25.md).
> Earlier counts and unresolved-LED notes below are historical.

## Historical screenless PCB LED discovery setup

Temporary mapping build on `screenless-pcb`, based on `test_led_off`.
It currently addresses 112 diagnostic raw LED slots on the existing GP13 chain, without using the old LED
geometry or lighting groups. Screen startup, recovery, filesystem mounting,
and saved animation loading are skipped. Normal front/back animations are
paused. Final front/back/badge mapping and independent badge controls follow
after physical discovery.

Ethan initially estimated a net increase of 13 LEDs over the old 89-LED board,
so early discovery used 102 addresses. He subsequently approved testing 104
addresses to investigate two unmapped front packages. Index 102 now lights a
front package; index 103 has no observed front emission. The physical total
is not yet confirmed. A subsequent user-approved diagnostic extends the frame
to 112 slots: 103 still shows no front emission in RGB with trailing off slots,
and 104–111 show none in individual red checks. Known 102 lights normally.
The configured range is not a physical-count claim. The 16-LED badge remains a 4×4 grid.

Compile from the QMK repository root:

```sh
qmk compile -kb toffee_studio/module -km via
```

Flash `toffee_studio_module_via.uf2` from the repository root. Immediately after
boot the following raw indices light up; every other LED is off:

| Colour | Raw LED index |
|---|---:|
| Red | 0 |
| Blue | 1 |
| Green | 2 |

Colours use named RGB fields because QMK's `RGB` structure defaults to GRB
field order. Red, blue, and green now use channel levels of **18/255**, chosen
by Ethan to reduce back-side light leakage (previously 64/255). The earlier
positional initializer swapped red and green; use the rebuilt UF2 containing
this correction. The boot batch now disables the yellow and pink slots.

Report each colour's physical location. Front LEDs use key positions. Badge
rows 1–4 run top-to-bottom and columns 1–4 left-to-right while viewed as when
typing. For back LEDs, specify whether the PCB is viewed from the typing side
or flipped over. Do not treat an unlit index as a missing LED until confirmed
with an individual test.

Change batches without rebuilding or reflashing, with the Module connected
over USB and the configurator closed:

```sh
/Users/ethanhan/.pyenv/shims/python3 keyboards/toffee_studio/module/tools/led_discovery.py 3 4 5
```

Arguments always follow red, blue, green order. Use `255` for an unused
colour, including single-LED checks:

```sh
/Users/ethanhan/.pyenv/shims/python3 keyboards/toffee_studio/module/tools/led_discovery.py 101 255 255
```

The verified Python runtime above has the existing local `hid` package;
the current default Homebrew `python3` does not. No installation or PATH
change is needed. The controller uses the Module raw HID
interface (VID `1067`, PID `626D`, usage page `FF60`, usage `61`). Command `7C`
uses the existing 32-byte custom VIA packet: byte 0 is `09`, byte 1 is `7C`,
bytes 2–5 are the packet ID, and bytes 6–10 hold the five protocol slots. The
three-colour controller always pads the yellow/pink slots with `255`. Firmware
rejects out-of-range or duplicate active indices before changing the batch.
The reply echoes the packet, replaces byte 0 with the status, and reports the
LED address count in byte 11. The last valid batch stays lit until changed or
rebooted; the rebuilt firmware restores indices 0–2 with the last two slots
off. The unchanged wire format allows three-colour control of the preceding
discovery build before flashing this one.

## Automatic USB firmware updates

The running Module supports custom command `0x79`: a 32-byte report with
`09 79` at the start, sent with report-ID prefix `00`. Its existing handler
calls `bootloader_jump()`, which calls the RP2040 ROM's `reset_usb_boot()`.
USB disconnects immediately; no HID acknowledgement is expected. The
bootloader then exposes the `RPI-RP2` UF2 disk.

Build with the normal command above. Leave the UF2 in the QMK root. Close the
configurator and connect exactly one Module, with other UF2 boards disconnected.
The normal flashing operation is simply copying the `.uf2` onto the bootloader
drive, just like Finder drag-and-drop. Run this read-only preflight first:

```sh
/Users/ethanhan/.pyenv/shims/python3 keyboards/toffee_studio/module/tools/flash_module.py toffee_studio_module_via.uf2 \
  --boot-drive /Volumes/RPI-RP2 --bootloader-serial E0C9125B0D9B \
  --recovery-dir /Users/ethanhan/Desktop/projects/toffee/ARCHIVE/firmware-recovery \
  --start-mode application --check
```

To perform the update, omit `--check`:

```sh
/Users/ethanhan/.pyenv/shims/python3 keyboards/toffee_studio/module/tools/flash_module.py toffee_studio_module_via.uf2 \
  --boot-drive /Volumes/RPI-RP2 --bootloader-serial E0C9125B0D9B \
  --recovery-dir /Users/ethanhan/Desktop/projects/toffee/ARCHIVE/firmware-recovery \
  --start-mode application
```

The updater validates every UF2 block's magic, RP2040 family, size, numbering
and sequential flash address before rebooting. It preserves a read-only,
SHA256-named copy of the candidate and a private keymap/macro/layout snapshot
in a unique recovery subdirectory. This is not a readback of the installed firmware.

After rebooting it requires one RP2 bootloader with the explicit serial, waits
for the selected FAT volume, and checks `Board-ID: RPI-RP2`. It uses
`/bin/cp -X` to copy only to that volume, with a 30-second limit. Drive reads
are bounded, including the wait after killing a stalled subprocess. One
targeted USB connection reset is allowed if a pre-copy read stalls; a copy
failure stops the updater without an automatic write retry.

Both keyboard and raw HID interfaces must return. VIA keymap/macros/layout
are compared with the snapshot, restored only if changed, and read back.
VIA uptime must increase for 15 seconds. The updater sends no lighting changes:
the firmware's saved startup state remains authoritative. The run report
records automated verification separately from pending human confirmation
of physical typing and lighting. These checks do not read back the entire
flash or attest its exact installed image hash.

For a board already in bootloader mode, use `--start-mode bootloader` and
`--settings-backup /absolute/path/to/module-settings.json` from a prior run.
Manual drag-and-drop remains valid. The preserved pre-change image is
`/Users/ethanhan/Desktop/projects/toffee/ARCHIVE/firmware-recovery/2026-09-30-pre-underglow/pre-underglow-8c65e8ad.uf2`.
Do not overwrite or delete it. Its SHA256 is
`8c65e8ad26255d3a16f7a8de709344917c0fba663cfc0ea2114e4d91031979c6`.
On 30 September 2026 a targeted bootloader USB reset cleared a stalled macOS
mount, normal file copying restored this image, and Ethan confirmed both
lights and typing. The cause of the earlier outage remains unconfirmed.

Reference: [QMK's RP2040 flashing instructions](https://docs.qmk.fm/flashing#raspberry-pi-rp2040-uf2).

Historical updater observations (17 September): real connected-device preflight and simulated updater tests
(reboot report, UF2 validation, deployment invocation, reconnect, and rejection
of an already-mounted UF2 disk). A live attempt on 17 September successfully
sent the boot command and mounted `RPI-RP2`. However, the updater blocked
opening `INFO_UF2.TXT`, before deployment; its process was terminated without
writing firmware. Finder could read the file and confirmed `Board-ID: RPI-RP2`.
The cause of the command-line file-access blockage is unconfirmed. The mount
deadline does not bound that file read, so the updater is not yet verified for
unattended use. Finder automation then failed with ScreenCaptureKit error
`-3812`; manual copying of the root UF2 was requested to finish the update.
On follow-up, Finder automation recovered but showed the manual copy stalled
at “Preparing to copy”; USB still identified `RP2 Boot` and Module HID was
absent. A read-only disk-list command also stalled. Reconnecting the board
and retrying the manual bootloader copy subsequently restored Module HID.
Discovery acknowledged 102 configured addresses, and camera checks confirmed
the requested red, blue and green at index 0. The underlying disk-access
failure has not been diagnosed; unattended updating remains unverified.

## Camera snapshots and scans

Ethan authorized using this computer's camera to observe one PCB face at a
time: front first, then back. Camera capture uses native AVFoundation, with
no microphone input. Build the local camera tool separately from the firmware:

```sh
swiftc keyboards/toffee_studio/module/tools/capture_camera.swift -o .build/module_camera -Xlinker -sectcreate -Xlinker __TEXT -Xlinker __info_plist -Xlinker keyboards/toffee_studio/module/tools/camera_capture_info.plist
.build/module_camera --list
```

Specify the camera ID explicitly. The observed built-in FaceTime HD Camera
ID is `1FD4B3A2-236E-492B-8CE5-255DD288CE50`; an OBS Virtual Camera is also
listed and is not used. IDs should be re-listed if devices change.

```sh
.build/module_camera 1FD4B3A2-236E-492B-8CE5-255DD288CE50 /tmp/module_camera_framing.png
```

The tool waits for two seconds of camera frames so exposure can settle, saves
one PNG, and stops the camera. macOS may request camera access on first use;
denied access requires the user to change macOS Privacy & Security settings.
The supplied plist explains why camera access is requested. Camera access
and real 1920×1080 still captures have been verified. On 17 September the
entire front PCB was framed with USB upper left and badge upper right.

For each face, place the whole PCB in view, keep it still, and avoid reflections
and overexposed LEDs. Check a framing photo before scanning. Front orientation
uses the typing view, with the badge at the upper right. After the front scan,
pause for Ethan to flip the board and record the back's actual orientation;
do not silently mirror back coordinates into the front view.

The scan command below switches batches and photographs all 112 configured addresses,
three at a time, without reflashing. It first captures an all-off baseline,
then 38 batches (the last contains one index). It writes a manifest containing the side, explicit
orientation, camera ID, and colour-to-index assignments for each image. A
new output directory is required; incomplete scans retain completed images
and have `complete: false`. LEDs are switched off when the scan exits.

```sh
/Users/ethanhan/.pyenv/shims/python3 keyboards/toffee_studio/module/tools/scan_leds.py --side front --led-count 112 --camera 1FD4B3A2-236E-492B-8CE5-255DD288CE50 --capture-tool .build/module_camera --output /Users/ethanhan/Desktop/projects/toffee-accounting/discord_capture/led_discovery/front_112_next --orientation 'Typing view; badge upper right'
```

For the back, run the same command after flipping and checking framing, with
`--side back`, a new output directory, and an accurate `--orientation`
description. Each face scans every raw address because group membership is
being discovered. LEDs hidden on the other face will not appear; absence in
one face's image is not proof that an LED is missing.

The agent can inspect these indexed photos to record front/key, badge-grid,
and back positions, repeating ambiguous indices individually. The capture
script saves evidence; it does not itself recognize key names or generate a
confirmed wiring map. The first front scan completed with uncalibrated colours:
red at index 0 produced green/cyan at the bottom-right front position. After
manual reflashing, Module HID reconnected and separate index-0 camera checks
confirmed the requested red, blue and green. A fresh front scan then completed
all 34 batches, covering configured addresses 0–101, with an all-off baseline
and `complete: true` manifest. Corrected evidence and initial reviewed positions
are indexed in [toffee-accounting](/Users/ethanhan/Desktop/projects/toffee-accounting/discord_capture/led_discovery/front_corrected_2026-09-17/README.md).
LEDs were switched off afterward. The full visual map and back scan remain
pending. Close badge LEDs need individual checks where glare makes positions
ambiguous; LEDs hidden on the back are not classified as missing.
Ethan subsequently reported substantial back-side light leaking into the front
view. Treat **all apparent positions and face assignments as provisional**,
including index 0. A completed scan only confirms capture, and correct colours
do not establish the emitting face. Verify direct emission at the actual LED
package on both sides; repeat ambiguous addresses individually. Lower drive
and an opaque nonconductive card behind the board, clear of exposed contacts,
may reduce spill but do not by themselves prove membership. Ethan selected
**18/255** (~7% of full numeric drive, ~28% of the previous 64/255 setting).
The normal QMK command successfully rebuilt the root UF2 with that value.
These are numeric drive ratios, not calibrated perceived brightness. Existing
scan datasets were captured at 64/255; a dim scan requires this new firmware
to be flashed and should use a new output directory.
See [Apple's camera authorization documentation](https://developer.apple.com/documentation/bundleresources/requesting-authorization-for-media-capture-on-macos).

Latest 18/255 deployment: a retry with an external 55-second process-group timeout succeeded, writing 141,312 bytes via QMK, reconnecting Module HID and accepting the three-colour batch. This verifies one complete automatic update; the earlier intermittent disk-access stall remains unexplained. The user requested a static visual check before the dim scan, which has not started.

## Front map from individual 18/255 checks

The dim front batch scan and a complete individual-red sweep of 0–101 both
finished successfully, with all-off cleanup. The resulting
[front map](/Users/ethanhan/Desktop/projects/toffee-accounting/discord_capture/led_discovery/front_map_2026-09-17.md)
contains 65 regular front positions and all 16 badge positions, backed by
baseline/illuminated package comparisons. The badge occupies raw indices
65–80. JSON/CSV and the per-index image references are adjacent to the map.

The approved 104-address build was compiled and automatically flashed.
Individual red testing confirmed index 102 at R1C12. Index 103 was accepted
but showed no direct front emission in repeated red, blue and green checks.
R1C13 remains unresolved; do not assign it by extrapolation. The compiled RGB
buffer is 312 bytes, the RP2040 output buffer is 416 bytes, and the driver
uses all 104 entries. This finds no obvious software count cap, but does not
diagnose a hardware defect or prove the physical total. The 23 indices with
no direct front emission remain unassigned pending further inspection.
Production groups have not yet been changed. All LEDs were turned off after
the extended tests. Evidence and the current UF2 hash are in the linked map.

Candidate analysis scripts saved with the scan evidence use existing local
NumPy, Pillow and SciPy to compare package-local colour contrast against an
all-off baseline. Every index was then repeated individually in red and the
actual package locations reviewed visually. Review sheets are crops of the
original images; raw evidence is preserved. No image score alone is accepted
as proof of the emitting face.

## All confirmed front LEDs test

From the QMK root, with the latest front-test firmware flashed:

```sh
/Users/ethanhan/.pyenv/shims/python3 keyboards/toffee_studio/module/tools/show_front.py
```

This enables a discovery-only preset: all 112 slots are cleared every frame,
then the 81 mapped front packages (65 regular + 16 badge) are set to white
RGB(18,18,18). Unresolved R1C13 has no assigned index and stays dark. No rear
candidate is in this allowlist. The preset persists until a normal discovery
selection is sent or the board reboots. Boot behaviour remains red 0, blue 1,
green 2. Turn everything off with:

```sh
/Users/ethanhan/.pyenv/shims/python3 keyboards/toffee_studio/module/tools/led_discovery.py 255 255 255
```

Protocol: 32-byte custom report, `09 7D` and report-ID prefix `00`, no payload
required. The successful response has status 0, byte 6 = mapped front count,
byte 7 = channel drive, byte 11 = configured address count. Packets shorter
than 12 bytes are rejected before enabling. Existing command 0x7C disables
this preset and restores normal index selection. Only discovery builds permit
0x7D; the final normal front/back/badge group implementation is still pending.

Verified: actual renderer sanitizer checks of the exact mapped set, clearing
unselected pixels, all-off exit and individual-selection exit; normal QMK
compile; validated UF2; automatic deployment and real USB preset activation;
camera evidence in toffee-accounting. On 18 September the preset was left on
for the user's requested inspection. Current root UF2 SHA256:
`5af305d3de48bd6e3e51dad4afdcbf1a3dc13761c2ef177a149856263f6c8e9a`.


## All addresses 0–150 test (18 September 2026)

The user requested every index 0 through 150 inclusive. The current diagnostic
build configures 151 addresses; this is a test range, not a measured physical
LED count. Brightness remains white RGB(18,18,18). Front, badge, back and any
unmapped physical LEDs in that range receive the same colour.

From the QMK root after flashing this build:

```sh
/Users/ethanhan/.pyenv/shims/python3 keyboards/toffee_studio/module/tools/show_all.py
```

Discovery command `09 7E` enables this preset with no payload. Its response
uses the same fields as `09 7D`: byte 6 = illuminated address count (151),
byte 7 = channel drive (18), byte 11 = configured count (151). Reports shorter
than 12 bytes are rejected. The renderer fills the entire configured buffer.
`show_front.py` switches back to the 81 mapped front packages; any normal
0x7C selection, including `led_discovery.py 255 255 255`, exits this preset.
Reboot still starts the three-colour 0/1/2 discovery selection.

Actual renderer checks under address/undefined-behaviour sanitizers passed:
all 151 addresses white, switch to 81 front LEDs, all-off exit, individual
selection of index 150. Earlier 112-address entries describe prior builds.


### 18 September 2026 — every address 0–150 illuminated

User requested all indices 0 through 150 inclusive, superseding the front-only
inspection. Expanded diagnostic RGB_MATRIX_LED_COUNT from 112 to 151. This
range does not establish the physical LED count or resolve the missing R1C13
mapping. All addresses now receive white RGB(18,18,18), including rear LEDs.
Added discovery command 0x7E and tools/show_all.py. Existing front-only command
0x7D restores the 81 verified front packages; normal 0x7C selection exits either
preset, and `led_discovery.py 255 255 255` turns everything off.

Normal `qmk compile -kb toffee_studio/module -km via` passed. Actual renderer
sanitizer checks passed for all 151 white, front-only transition, off transition,
and individual index 150. Automatic UF2 deployment succeeded and real USB
activation acknowledged count 151 / brightness 18. Left all-address preset ON
for user inspection. Root artifact `toffee_studio_module_via.uf2`: 142336 bytes,
SHA256 `48cbf558cecc72b21f482074aa8123daa5e858a2923081c9312e4484799b6a8d`.
No new physical position assignments were made.


### Persistent startup indices 0–100 — 18 September 2026

User requested the currently connected second PCB permanently start with all
indices 0–100 inclusive on. Firmware startup now selects 101 white addresses
at RGB(18,18,18), and clears indices 101–150 every frame. Kept the configured
151-address buffer so higher physical LEDs explicitly receive black. This is
the compiled boot default and returns after power cycling; discovery commands
can still temporarily override it until the next restart. The previous
three-colour 0/1/2 startup documentation is superseded for this build.

Normal QMK compile passed; actual startup and renderer code passed sanitizer
checks for inclusive endpoints, all higher addresses off, reinitialization,
and manual all-off. Automatic deployment to the connected PCB succeeded and
USB reconnected. No post-flash lighting command was sent, leaving startup active.
Added `--preserve-startup` to tools/flash_module.py to skip its usual three-colour
post-flash test when checking boot defaults. No physical power-cycle test was
performed; the deployment reboot exercised firmware startup.

Root artifact: toffee_studio_module_via.uf2, 142336 bytes; SHA256
`90444a6726f04a4de5844c02b9da1d2e1a8e2ef23bcd756bd2d629f0d08ec95b`.


### Correction: persistent startup indices 0–200 — 18 September 2026

User corrected the requested boot range from 0–100 to 0–200 inclusive. Current
firmware configures 201 addresses and sets every address white RGB(18,18,18)
on every startup, without a host command. This supersedes the prior 101-address
startup behavior. Diagnostic overrides remain temporary until reboot. The
configured address range does not establish the physical LED count.

Normal QMK compile and actual startup renderer sanitizer checks passed for
all 201 addresses, manual off, and restored startup. Flashed the connected PCB
using --preserve-startup; deployment and USB reconnection succeeded, leaving
startup lighting active. Root UF2 size 142848 bytes, SHA256
`1d251fac54d340a9b5c906154138b13376d2fa81c9bd7fc032aebbc7996801be`.


## Repaired LED integration — 25 September 2026

The missing via was manually bridged; existing raw-index firmware can now
light 103 at the formerly dark equals-key package. Added measured index 103
to discovery_front_leds, so command 0x7D returns front count 82. No protocol
change is required; the host reads the returned count. Independent front/back/
badge production animations remain separate work.

Normal QMK compilation passed. Actual renderer sanitizer checks verified exact
equality with the new 82-entry map, 103 included, 119 other slots cleared,
and manual-off behavior. Automatic flashing succeeded. Root UF2: 142848 bytes,
SHA256 `02f81dd8d3ac822012aaf3b6ac40b05323b99928550ae0924789f2ea312a7ada`.

Post-flash USB returned front count 82. Camera rechecks passed for isolated
103 and the full-front preset; left all 82 front packages white at 18/255.


## Rear mapping and XY rainbow — 26 September 2026

Ethan authorized individual rear checks and requested an (x,y)-based cycling
rainbow in place of a simple spatial sweep. The board was turned 180 degrees
about its vertical axis: top remains top, USB upper right in the rear camera
view. Rear lights are table-facing underglow, not per-key LEDs.

All 22 rear candidates were individually photographed in red at 18/255 with
an off baseline and package-location review. Preserved every original rear
(x,y) from test_led_off lighting.c. Ten indices changed: old 74→90,73→89,
72→88,71→87,70→86,69→85,68→84,67→82,66→81,50→49. The other twelve
indices remain unchanged. Rear and front sets are disjoint and cover exactly
0–103: 22 rear +66 regular front +16 badge =104 mapped physical LEDs.

Added discovery-only command 0x7F and tools/show_back.py for the measured rear
table. Hue uses floor((x+y)/2)+phase modulo 256; phase advances every 40ms,
for a 10.24-second cycle. Saturation 255, value 18, no CIE correction. All
non-rear addresses are black. --phase 0/64/128/192 freezes the pattern for
repeatable checks. Existing front/all/individual commands exit the rear mode.
The compiled boot default still lights 0–200 white; rear cycling is activated
by the command and runs on the board without a continuing host process.

Normal QMK compile, exact map equality, all 256-phase renderer sanitizer
checks, brightness ceiling, non-rear clearing, frozen/cycling progression and
wrap, front/off exits passed. Automatic flashing succeeded. Live command
responses confirmed 22 rear LEDs, drive 18, 201 configured addresses. Four
frozen camera phases show the expected smooth colour order in the mirrored
rear view, with no observed index swap. The cycling preset is left ON.

Root UF2: 143360 bytes, SHA256 `663a878da1f498396110fb439a629c6877592b22b10b31c577423d22ed521a83`. Root and .build match.
Original normal-animation legacy tables remain historical and disabled by
discovery mode; the verified new rear table is ready for later production
front/back/badge integration. The 201 diagnostic slots are not a physical count.

[Rear map and original-coordinate comparison](/Users/ethanhan/Desktop/projects/toffee-accounting/discord_capture/led_discovery/back_map_2026-09-26.md)
[Individual captures](/Users/ethanhan/Desktop/projects/toffee-accounting/discord_capture/led_discovery/back_2026-09-25/manifest.json)
[Rainbow phase 0](/Users/ethanhan/Desktop/projects/toffee-accounting/discord_capture/led_discovery/back_2026-09-25/rainbow_phase_000.png)


### Rear rainbow control

From QMK root, use the existing verified pyenv Python:

```sh
/Users/ethanhan/.pyenv/shims/python3 keyboards/toffee_studio/module/tools/show_back.py
/Users/ethanhan/.pyenv/shims/python3 keyboards/toffee_studio/module/tools/show_back.py --phase 64
```

The first command starts cycling; the second freezes a phase (0–255). Off:
`tools/led_discovery.py 255 255 255` with the same interpreter.

32-byte custom raw HID report with report-ID prefix 00: bytes0–1 =09 7F,
byte6 =1 to cycle or0 to freeze; byte7 =initial/frozen phase. Response status0
means success, byte6 =rear count22, byte7 =brightness18, byte11 =configured
count201. Firmware rejects length<12 or a cycling flag>1 before changing state.
Host rejects a frozen phase outside0–255 before opening USB. No new dependency
is needed. This preset uses preserved typing-view coordinates, so the rear
camera view naturally mirrors its horizontal direction.

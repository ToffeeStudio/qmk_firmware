# Toffee Studio Module — start here

Last reviewed: **4 October 2026 (Australia/Sydney)**. This is the current board
handover, not a live device-status feed. Read the repository's
[agent safety instructions](../../../AGENTS.md) before working on hardware.

## Current status

**4 October sparse-drive calibration candidate — built/tested, not yet flashed:**
build `0x26100403`, SHA256 `3b08d2aebad3e15dbe164815a92697ee6b64c3b32cf8a34d5f9dfacc949a9840`.
The calibration-only `0x8A` command accepts literal R/G/B plus a 13-byte bitmap
of mapped positions 0–103. Up to 50 requested LEDs may use 255; larger selections
are restricted to 150 per channel. Production and ordinary 18-cap test builds
reject this command. Requests still pass through the final shared limiter.
Temporary profile 0x8002 allows sparse full-drive but retains a **450 mA initial
test-model ceiling** (120 mA baseline reserve, 100 mA margin, 20 mA/channel
reference; 230 mA LED allowance). This does NOT change the provisional 900 mA
product budget, and is not a calibrated profile. Startup-off, no saves and
five-second watchdog remain. All-off and timeout cancel the raw pattern.
Host regressions pass, including all 104 raw indices, 50/150 boundaries, timeout,
no settings changes and three 20,000-frame limiter variants. Visual-controller
tests: 14 tests / 286 assertions. **Every future meter image must be viewed by
the agent; no OCR/image-reading scripts.** Use `tests/visual-calibration.ts` in
the configurator; earlier OCR runner is historical and must not be restarted.

**4 October calibration preparation:** protected candidate build `0x26100402`
adds test-only read-only command `0x89`: 25-byte payload, schema byte 0 = 1,
frame-valid byte 1, little-endian sequence/age at 2/4, nine u16 final RGB sums
at 6–23 (front/rear/badge; R/G/B), reserved byte 24 = 0. Product builds reject
the command. Counts are after final limiting/startup, not requested sliders.
Opt-in `MODULE_POWER_CALIBRATION=yes` requires `MODULE_EXTENSIVE_TEST=yes`;
its revision 0x8001 is a reference measurement profile, NOT calibrated product
coefficients: 900 mA total, 120 mA baseline reserve, 100 mA margin, 20 mA/channel,
18-byte package cap, existing startup-off/5-second timeout and disabled saves.
Normal profile remains unconfigured. Host parser/storage/startup/power tests
pass, including three 20,000-frame variants and final-sum comparisons. Twelve
JS meter/runner tests (67 assertions) cover nine fail-closed scenarios.
Candidate SHA256 `f6f1918483a0596eb75875c91be5c72c7daabe0b3da438a6c5136182ca81513d`.
Preflight passed at current sole Module location 0x00100000; original recovery
hash verified unchanged. A private pre-flash VIA/lighting/66-override snapshot
and three matching OCR readings are retained in the current calibration log.

**4 October budget decision — 900 mA whole-board provisional limit selected.**
Ethan explicitly chose 0.9 A for now. Source profile revision 2 records
`board_limit_ma = 900`; this includes the electronics and model margin, with
the remainder shared across front/rear/badge LEDs. Calibration is still pending,
so `configured = false` remains and the research profile outputs black. This
records the agreed working budget, not a measured safe rating. No UF2 rebuild,
flash or powered measurement accompanied this change; the connected board
continues to run the previously deployed image.

**4 October latest specification check — Discord and original BOMs reviewed.**
F1's best-supported identification is **BHFUSE BSMD0603-100-6V / C18203128**,
a 0603 resettable fuse rated 1 A hold at 25°C. Both August screenless
`Module4.zip` BOMs (before the September order) and the September 20 BOM specify
1000 mA/C18203128; the new photo's `0` marking matches the manufacturer's table.
November 2025's approved 1.5 A/C2844005 applies to the older `Module-02` BOM,
not confirmation of this sample. Exact fitted identity still needs the assembly
record; 1 A is not an established safe current budget. Root and firmware
`AGENTS.md` now require Discord/attachment lookup for all hardware specs.
See the [revision/evidence trail](../../../../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/README.md#4-october-follow-up-discord-and-f1-revision-check).

**4 October latest follow-up — rescue stopped and new PCB photos reviewed.**
On Ethan's explicit instruction, session `66996` received Ctrl+C and exited 0
after observed ACK write 34288. No rescue process remains; no command restored
brightness or saved settings. Do not automatically restart it. High saved startup
is unchanged, so a reconnect/normal lighting setter can still bring it back.
Ethan confirms direct MacBook Pro → USB meter → PCB, with no upstream hub.
Downloads `IMG_7455.JPG`–`IMG_7460.JPG` were inspected. The last image clearly
shows F1 marked `0`. BHFUSE's BSMD0603-100-6V (1 A hold) also has marking `0`
(2025 datasheet p8): consistent with the archived fuse, not a confirmed bypass
or unique fitted-part identification. No closer photo is needed to read F1.
See the [photo-review log](../../../../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/README.md#4-october-follow-up-rescue-stopped-and-pcb-photo-review).

**4 October — shared limiter and Debug telemetry implemented, NOT flashed.**
The source now proportionally limits final RGB transmission across front/rear/
badge, with a read-only `0x88` status API and companion configurator Debug panel.
The profile is **unconfigured**, so this research build holds LEDs black rather
than inventing a safe current budget. Existing saved lighting is not changed.
Ethan confirmed the design, board-only meter and secure fixture; fitted F1,
source allowance and calibrated RGB coefficients are still unknown. This is
an offline software checkpoint, not electrical qualification or deployed protection.

Current root UF2: **175616 bytes**, SHA256
`31cf12bfbabd7819557f1d89f4f8ca3b2544fe9d0d6993bf76cb81cf839b6f67`, preserved as
[power-debug-unconfigured.uf2](../../../../ARCHIVE/firmware-recovery/20261004-power-debug-dQ3Ake/power-debug-unconfigured.uf2).
Normal and temporary `0x26100401` builds pass; a final normal rebuild is byte-identical.
Both candidates remain unflashed. The protected pre-change and startup-only images
are unchanged. Rescue session `66996` was active at this software checkpoint
and has since been stopped as recorded above; no new setters, flash or powered ramps.

ASan/UBSan checks pass 20,000 randomized frames in each of two profile/driver
variants, parser 4,581/4,584 attempts, startup, storage and rendering suites.
All 24 mocked Python updater/rescue tests pass. Companion frontend: 24 HID/map/
startup tests (1,137 assertions), 11 actual-component simulated-HID browser
checks and Vite build pass. The same 11 legacy TypeScript errors remain.
Browser checks include real slider mouse/keyboard input, queued writes, current
expiry, unsupported firmware, hidden pages and disconnect/retry; **no PCB access**.
Details, limitations and measurement blockers are in the
[research log](../../../../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/README.md).

**4 October — startup rescue window and fade implemented, built, NOT flashed.**
As explicitly chosen by Ethan: **1 second completely dark after USB is configured
and the main loop is running, then a 1-second fade to saved/requested lighting**.
All front/rear/badge/per-key and diagnostic output goes through the gate. USB
commands continue throughout; saved settings remain unchanged, and a rescue
all-off command is not overwritten by the end of startup. Reset/suspend re-arms
the window. Both agent instruction files now make this a permanent requirement.

The earlier startup-only candidate was **173568 bytes**, SHA256
`7ff7946621e927dfa934348bfe771f43c9c9e9dee9796ef2818d3bae1382691a`, also preserved as
[startup-fade.uf2](../../../../ARCHIVE/firmware-recovery/20261004-startup-fade-OGr5zV/startup-fade.uf2).
It is now superseded at the local build root by the power candidate above; the previously deployed `bfda11cf...`
and protected pre-change `8c65e8ad...` images remain archived unchanged.
No deployment or hardware timing test was performed. The separate rescue process
is still running (session `66996`, latest observed ACK at write 3077); recheck
before another controller is used. The connected board's saved high brightness
has **not** been repaired. A fade does not make an excessive steady-state load safe.

Verification: normal QMK build succeeds with existing legacy warnings; ASan/UBSan
host tests pass, including 1001 fade positions, all 201 output addresses, stored
configuration retention, group scaling, repeated frames, USB reset/suspend,
timer wrap and rescue commands before/during/after the fade. Production/temporary
parser totals are 4,326/4,329 attempts. Existing storage and rear debounce tests
and all 24 Python updater/rescue tests pass. Updater tests simulate flashing;
they do not flash hardware. See the [current research/recovery log](../../../../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/README.md).

**4 October — unresolved high-brightness shutdown risk.** The deployed firmware
has no shared current budget across front/rear/badge. The front ceiling of 160
does not globally constrain custom rear/badge output or establish safe current.
Past low-drive tests and flash success are not high-load electrical qualification.
Fitted fuse/source limits and present shutdown mechanism remain unverified.
See [current-budget research](../../../../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/README.md)
for historical evidence, BOM discrepancies and the unflashed firmware limiter.
No high-load tests or flash were performed for this investigation.
The later authorised temporary rescue is recorded below. Power/thermal P0/P1
gates remain open.

**4 October follow-up — standalone plug-in rescue responding on hardware.**
Ethan requested a native helper already waiting before he plugs in the sole
Module, repeating all-off every 200 ms without a browser. See
[temporary lighting rescue](#temporary-lighting-rescue). This is a host tool,
not a firmware change or a current-budget implementation. Saved high startup
settings are not yet repaired; the reported power-cycle failure remains open.
Ethan authorised starting the loop, then reported "I think it's good now."
The helper reported tagged all-off acknowledgements from write 2 through 102
(roughly 20 seconds), with no USB-error messages in that interval. It remains
running at this checkpoint (exec session `66996`); recheck before another controller
is used. This supports successful temporary contact, not verified typing, saved
startup repair or a proven electrical root cause. No brightness setters or flash.

**3 October 2026, 11:59 Sydney — requested flash completed.** The sole connected
PCB at `0x02140000` received the exact normal image previously used on B:
`bfda11cfff92e811b56daded8e9713afa16e62e5cfbe66f147ee7482204a0a02`.
The ordinary UF2 copy needed no USB-reset workaround; keyboard/VIA returned and
passed the updater's 15-second check. The updater restored the backed-up keymap;
final keymap/macros/layout matched. No lighting setters or extended tests were
run. Physical typing and saved-lighting confirmation remain pending.
The measured bootloader serial was `E0C9125B0D9B`, historically assigned to B,
despite this connection/keymap having been labelled A. Do not infer physical
identity from those historical labels. Original recovery firmware is preserved.
Report: `ARCHIVE/firmware-recovery/20261003-module-a-07YqQt/20261003T015911Z-d6a7c1fd/flash-report.json`
under the Toffee root. The dated campaign results below predate this flash.

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
| Configurator | Four menus and protocol-v2 API implemented. **2 October follow-up fixes the startup race:** saved/bundled definitions load before one device scan. Two clean headless actual-app runs pass mouse/keyboard input on all six sliders, PCB readback, repeated reloads, injected read/write errors and simulated WebHID reconnect. See the linked extensive-plan UI report. No firmware change or flash was needed. |
| Latest flash | Temporary build `0x26100202` completed testing, then normal product image `bfda11cf...` was restored at 11:32 UTC on 2 October. Both copies matched serial/location and passed 15-second application/keyboard/VIA verification without a USB-reset workaround. |
| Saved startup | Normal restart restored hue 36/saturation 255, front/rear brightness 11, badge brightness 2, speed 100, all 66 overrides 255, before any product lighting setter. Final reads after browser-test cleanup also match the original. Both VIA snapshots remain unchanged. |
| Physical verification | Ethan reported a hub unplug/replug; enumeration, saved settings and cameras pass afterwards, but actual VBUS removal was not measured. **Controlled cold cycles and physical typing remain pending.** The equals LED's formerly working manual bridge is now broken, per Ethan; its dark state is excluded as a hardware fault, not a new indexing/firmware regression. |
| Recovery | The pre-change image was restored earlier, and Ethan explicitly confirmed both lights and typing. That confirmation applies to the recovery image, not the subsequent underglow candidate. Both images remain preserved outside the build directory. |
| Earlier outage | Cause unconfirmed. A stalled macOS bootloader volume was observed and cleared by a targeted USB connection reset. Neither a firmware defect nor a transfer defect has been established as the original cause. |

Do not remove `MODULE_LED_DISCOVERY` just to enable lighting: it also keeps
the old screen/filesystem startup disabled. Legacy source and build dependencies
remain in the tree; this pass did not remove them or restore display support.

**2 October, 23:16 Sydney — visible native-browser follow-up:** real Chrome
WebHID permission selection, all six slider drags, badge toggles and reload pass
11 complete PCB-state comparisons, without the headless HID bridge. The selected
device's keymap fingerprint matched independently enumerated Module B before
writes. Original lighting/all 66 overrides and both VIA snapshots remain intact;
Module A lighting is unchanged. No firmware changes or flash. See the
[visible UI report](../../../../AUTORESEARCH/02-09-2026%20Extensive%20Test%20Plan/visible-ui-20261002-r5sRGo/README.md).

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

### Persistent iPhone meter camera

**Latest wired retry:** Node 77553 / exec 49593 owns helper 77563. iPhone is
now enumerated over USB at 0x02100000. Three fresh HTTP images were fetched and
viewed; underlying macOS Continuity logs show no disconnects during the test.
Earlier PID references below are historical. The wireless session DID reconnect
despite unchanged local AVFoundation counters; those counters alone are insufficient.
[Wired retry evidence](../../../../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/wired-camera-20261004-VJtE4v/README.md).

**Shared live preview:** http://127.0.0.1:5174. Rotate, zoom and drag the view
to position the meter. `/frame.jpg` supplies a fresh JPEG from that same camera
connection; `/status` reports helper identity, frame age and lifecycle counts.
MJPEG `/stream` shares one capture across all viewers. It listens only on
loopback, records no continuous video/audio, and does not touch the PCB.

```sh
node keyboards/toffee_studio/module/tools/camera_preview.mjs --camera-tool /Users/ethanhan/Desktop/projects/toffee/firmware/.build/module_camera --device 2A398C8F-C0AC-4706-90E0-D2D300000001 --port 5174
```

4 October current owner: Node PID **53454**, exec **51668**, native helper
**53464**. Browser reload, rotation/zoom/Fit and simultaneous viewers passed;
one camera session, no stops/interruptions or JavaScript errors. Recheck status
before reuse. While this server is running, request HTTP snapshots instead of
starting the standalone helper below. SIGTERM to the verified Node owner closes
its camera child cleanly. Do not overwrite a running native executable.

Use `tools/capture_camera.swift` in **`--serve` mode** for repeated snapshots.
One process keeps one AVFoundation session open; it waits for two seconds of
initial frames, then each snapshot request captures a new frame. Idle frames
are discarded. Only requested PNGs are saved; no audio or video file is recorded.
The helper's main run loop remains active for camera lifecycle notifications.

Build from the firmware root while the helper is stopped:

```sh
swiftc keyboards/toffee_studio/module/tools/capture_camera.swift -o .build/module_camera -Xlinker -sectcreate -Xlinker __TEXT -Xlinker __info_plist -Xlinker keyboards/toffee_studio/module/tools/camera_capture_info.plist
.build/module_camera --list
.build/module_camera --serve 2A398C8F-C0AC-4706-90E0-D2D300000001
```

Send lines to that **same process's stdin**:

```text
status
snapshot /absolute/existing/folder/meter-01.png
snapshot /absolute/existing/folder/meter-02.png
quit
```

Spaces in snapshot paths are supported; do not add shell quotes inside the stdin
line. Responses are JSON lines containing PID, snapshot/frame number, frame
timestamp and image dimensions. `status` includes running state, frame age and
observed session starts/stops/interruptions. Bad commands/paths return an error
without reopening the camera. A capture failure or five-second fresh-frame
timeout ends the helper; it does not return a cached frame or silently restart.
`quit` or stdin EOF closes the connection. The old `DEVICE_ID OUTPUT.png` mode
still takes one shot and exits; avoid that mode during persistent meter work.
Future measurement runners should keep one child process and reuse its stdin.

**Earlier 4 October standalone verification (now closed):** helper PID **47541**,
exec **66202**, used Ethan iPhone Camera. Three 1920×1080 snapshots
passed with advancing frame numbers/timestamps, one session start, zero stops
and zero reported interruptions. An invalid `.jpg` request was rejected without
disconnecting. Recheck `status` before reuse; these are recorded runtime IDs.
The meter is visible but currently soft in focus, so these frames do not qualify
automated current readings. Camera-only test: no PCB commands or firmware flash.
[Evidence and images](../../../../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/persistent-camera-20261004-yXj3aN/README.md).

### Temporary lighting rescue

Run this **before plugging in the PCB**, with the configurator/other lighting
controllers closed and only one Module connected:

```sh
/Users/ethanhan/.pyenv/shims/python3 /Users/ethanhan/Desktop/projects/toffee/firmware/keyboards/toffee_studio/module/tools/rescue_lighting.py
```

The [rescue script](tools/rescue_lighting.py) uses the existing native Python
`hid` library. It finds VID/PID `1067:626D`, raw usage `FF60:61` on every cycle,
so changing USB paths after unplugging is expected. Ethan explicitly chose the
single-Module workflow; two matching interfaces cause a clear stop, not an
arbitrary selection. No remembered port, browser permission or slider is needed.

- Sends existing command `0x7C` with **all five slots `255`**. Current firmware
  clears the entire RGB buffer: front, rear, badge and unused transport addresses.
  It is not the rear-only brightness command `0x74`.
- Writes immediately after opening, then on a 200 ms target cadence. Keeps the
  handle open and retries disconnect/open/write errors. Host scheduling/native USB
  stalls can delay the cadence; it is not a hard real-time cutoff or guaranteed rescue.
- Uses 32 firmware bytes plus HIDAPI's leading zero report ID. Nonblocking reads
  check tagged command/packet IDs; a write without a reply is **unconfirmed**.
  No legacy fallback: old firmware may reject it or give no tagged acknowledgement.
- Ctrl+C sends **no restore command**. The override remains in RAM, but **saved
  startup is unchanged**. A normal lighting setter or power cycle can reactivate
  unsafe saved levels. Do not manipulate the configurator or reconnect casually.
- Does not flash, reset EEPROM, modify keymaps/macros or write saved lighting.
  If the PCB still shuts down, unplug it and let it cool; do not deliberately
  repeat the power-failure cycle. Coordinate a supervised plug-in after arming.

Persistence is separate, not a silent Ctrl+C side effect. Repeating group setters
every 200 ms postpones their one-second save debounce; setters also exit diagnostic
blanking and can re-enable still-high groups. The helper avoids both. Saving zero
safely is not implemented in this version.

Offline checks (no USB writes), from the firmware root:

```sh
/Users/ethanhan/.pyenv/shims/python3 keyboards/toffee_studio/module/tools/rescue_lighting.py --dry-run
/Users/ethanhan/.pyenv/shims/python3 -m unittest discover -s keyboards/toffee_studio/module/tools -p test_rescue_lighting.py -v
sh keyboards/toffee_studio/module/tools/test_lighting_host.sh
```

On 4 October, 12 mocked-USB tests and existing sanitizer-enabled production
parser/rendering/persistence tests passed. The production parser test verifies
that five unused slots clear every RGB address. Initially the read-only inventory
found no Module; the later user-authorised live run is recorded in Current status.

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

**Temporary build `0x26100202` completed testing and normal firmware is restored.** Ethan confirmed the
unplugged loose-wire/support inspection and directed execution; do not ask again.
Baseline/transition/gradient capture (31 pairs), all 312 RGB checks (109 pairs),
51 individual ambiguity repeats (52 pairs), eight group combinations and 15
isolated adjustments (24 pairs) completed without unexpected USB errors.
Camera triage supports 283 package/colour matches, retains 26 glare-ambiguous
badge checks and excludes the three colours of broken equals LED 103.
The 290 reviewed invalid commands returned deterministic errors without changing
group state. Closing HID and an actual SIGKILL of a disposable controller both
caused expiry/all-off; late keepalive did not relight. Original paired images
show all-off after both interruptions. Physical cable removal remains separate.
The JavaScript regression now passes 28 tests / 1,750 assertions.
The 30-minute capped observation completed: 1,733 status samples, 30 unchanged
enumerations, no unexpected expiry/reset/disconnect, and successful post-run
baseline. Active-pattern keepalive age peaked at 1,003ms. The full session lasted
1,810.77s including captures/cleanup (mixed-pattern hold 1,800.68s). No electrical,
thermal, switch-input or complete prototype qualification is implied.

Temporary deployment: `ARCHIVE/firmware-recovery/20261002T102514Z-f0e2a34f/flash-report.json`;
normal restoration: `ARCHIVE/firmware-recovery/20261002T113201Z-5fab807b/flash-report.json`.
case logs, originals and camera analyses are under the linked extensive-plan run.
The older `0x26100201` candidate was never deployed.
Normal saved-startup behaviour is unchanged. A final normal rebuild is byte-identical to the original
campaign baseline and excludes test-mode symbols. The root UF2 is that normal
build, SHA256
`bfda11cfff92e811b56daded8e9713afa16e62e5cfbe66f147ee7482204a0a02`,
173,056 bytes, preserved with private snapshots in
`ARCHIVE/firmware-recovery/20261002-pre-extensive-S71MzY/`; it is not an
installed-image attestation. The previously deployed and pre-change images
remain preserved. Full frontend TypeScript still has the 11 pre-existing errors.

1. The final app connected-device/loader regression is fixed without hiding the
   overlay. [UI evidence](../../../../AUTORESEARCH/02-09-2026%20Extensive%20Test%20Plan/ui-fix-20261002-YcMyLx/README.md)
   includes two clean real-board browser runs, 30 offline tests / 1,754 assertions
   and a successful production frontend build. Original groups/66 overrides and
   both boards' keymaps/macros remain unchanged. Controlled cold cycles
   and typing confirmation remain on Module B;
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
- [lighting.h](lighting/lighting.h): explicit 1000 ms startup-black and 1000 ms fade constants. `lighting_startup_scale()` returns a 0–1000 factor, applied at final transmission to the budget-limited frame, never saved configuration. The timer starts on the first flush with configured USB, not before enumeration. `notify_usb_device_state_change_kb()` re-arms on USB loss/suspend and preserves the user hook. QMK's render/flush loop and Raw HID remain live; no blocking delay or new persistence format.
- [power_driver.c](lighting/power_driver.c), [power.c](lighting/power.c), [power_profile.c](lighting/power_profile.c): board-local custom RGB driver using the existing WS2812 vendor transport; immutable rendered buffer plus separate limited output. A common factor enforces aggregate and package caps; 64-bit weighted sums, RGB rounded down/current rounded up, unmapped slots black. Profile revision 1 is unconfigured: 90 mA preliminary base and 12 mA/channel reference coefficients are NOT calibration. Never deploy the synthetic 350 mA test profile as a board rating.
- [lighting/storage.c](lighting/storage.c): front/badge/per-key persistence in two 4KB sectors at `0xFF0000`–`0xFF1FFF`; 256-byte CRC32 records append across sector rollover. A prior valid record is retained while the other sector is erased. Existing VIA EEPROM offsets and macro capacity are unchanged. The 16MB flash requirement is compile-time checked.
- [tools/flash_module.py](tools/flash_module.py): explicit volume/serial, immutable candidate archive, private VIA snapshot, bounded copy, and verification. The snapshot covers keymaps/macros/layout, not a full flash or lighting-state backup. One targeted USB reset is allowed on a pre-copy read timeout; a copy failure is not blindly retried.

### Power status API (candidate only)

Command `0x88` uses the existing 32-byte custom packet and tagged reply trailer;
lighting protocol-v2 capabilities remain `[2,66,22,16,104,9]`. No input payload,
settings write, save, test keepalive or budget override. Unsupported older firmware
returns `0xEF`; the UI stops polling with a clear error, without affecting menus.
Response byte 0 is status; the following 25-byte payload is:

| Payload offset | Field |
| --- | --- |
| 0 | Schema version 1 |
| 1 | Flags: configured, limited, USB configured, startup active, valid frame (bits 0–4) |
| 2, 4, 6 | Model revision, frame sequence, frame age ms (saturates at 65535) |
| 8, 10, 12 | Board ceiling, base reserve, engineering margin in mA |
| 14, 16 | Requested/effective incremental LED estimates in mA, rounded up |
| 18, 20 | Limiter and startup factors in permille |
| 22 | Available LED budget: max(0, ceiling − base − margin), zero if unconfigured |
| 24 | Reserved zero |

All multi-byte fields are unsigned little-endian 16-bit. Payload excludes the
unchanged bytes 26–31 reply tag. Total-board estimate is base + effective LED
estimate; margin is withheld headroom, not measured consumption. Budget usage
is effective LED estimate / available LED budget, unavailable at zero budget.
No per-group telemetry is included in schema 1. Firmware execution time, physical
startup timing, calibrated accuracy and electrical/thermal envelope remain unverified.

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

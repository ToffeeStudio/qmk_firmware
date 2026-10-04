# Module documentation entry point

Read [keyboards/toffee_studio/module/readme.md](keyboards/toffee_studio/module/readme.md)
first for the current screenless board, local project locations, USB identity,
lighting support, verification gaps, recovery images and detailed references.
Keep that README current when board behavior or verified status changes. Record
automated checks separately from human typing/lighting confirmation. Historical
discovery and per-key notes must not override its current-status section.

## Hardware specifications and Discord evidence

Always consult the archived Discord history and original attachments for board
and component specifications, as Ethan explicitly requires. Follow the
[Toffee-root evidence-source rules](../AGENTS.md#hardware-specifications-always-consult-discord-history)
for the full Suzu archive, Topfast conversations and BOM provenance. Do not
infer fitted components from firmware constants or apply older revision decisions
to today's sample. August/September 2026 screenless BOMs specify F1 as
**1000 mA / C18203128, BHFUSE BSMD0603-100-6V**; its `0` marking matches the
photo. This is the best-supported identification, not certified as-built identity.
November 2025's 1.5 A / C2844005 approval is an older `Module-02` BOM decision.
Neither nominal fuse rating is a qualified operating-current ceiling.

## Persistent meter camera

**Permanent instruction from Ethan, 4 October: explicitly view every meter
image yourself and read current/voltage visually. No OCR, Python image-reading
scripts or other automated image-to-number extraction.** Retain the viewed
image with its frame identity and record only legible digits/units; unclear or
stale images stop the measurement. Reuse the existing `/frame.jpg` connection;
do not reopen the phone camera for a snapshot. The old `read_meter` helper and
OCR-based calibration runner must not be used for future meter readings.

Latest 4 October retry: the iPhone is present on USB at 0x02100000. Preview
Node PID 77553 / exec 49593 owns helper 77563; use http://127.0.0.1:5174.
Three fresh snapshots were fetched and viewed, with no underlying Continuity
disconnects in the test log. Earlier PID 53454/53464 below is historical, stopped
after wireless transport reconnects that the helper's own counters missed.
Do not use application session counters alone to claim the phone stayed connected.

For repeated iPhone snapshots, keep `capture_camera.swift --serve DEVICE_ID`
running and send snapshot requests to the same process. Do not spawn the one-shot
mode each time. See the [camera usage and latest runtime IDs](keyboards/toffee_studio/module/readme.md#persistent-iphone-meter-camera).
Use `/frame.jpg` and `/status`; do not write to the helper's stdin or start
another camera owner. Historical helpers 53464 and 47541 are stopped. Final
campaign snapshots and a30-minute underlying Continuity log query found no
matching disconnect/stop events. Camera capture alone does not authorise PCB writes.

## Permanent Module lighting startup contract

Ethan requires an **always-preserved rescue window before LEDs turn on**.
Agreed 4 October 2026: keep every LED black for **1000 ms after USB is configured
and the main task loop is servicing commands**, then fade the complete rendered
output from zero to the saved/requested level over **1000 ms**. Never implement
the window as a blocking delay before Raw HID service. Reset/suspend must re-arm
the window for the next configured connection. Cover front, rear, badge, per-key
and diagnostic paths, without changing saved settings or restoring a high state
over a received rescue all-off command. Keep the host startup/parser tests passing
when modifying initialization, rendering, USB lifecycle or adding output paths.
This is startup behavior, **not a shared current limit**; the saved steady-state
load can still be unsafe. Do not claim it resolves the underlying overcurrent risk.

## Power-budget research checkpoint (4 October 2026)

**Later continuation:** Ethan requests no silent dimming in calibration and says
idle variation is expected. Candidate0x26100405/profile0x8003 admits exact raw
patterns within the provisional900 mA conservative model or rejects them;
startup-off, no saves,5s watchdog and final limiting of other paths remain.
Controller rejects any reduced final RGB sums. Record idle ranges rather than
requiring constant values. [Active evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/exact-output-20261004-gFljV8/README.md)
is authoritative for build/flash progression. Do not claim source/thermal
qualification from these provisional tests. Older checkpoint below is historical.

Installed protected temporary build **0x26100404**, SHA256
`c90c69b7dea2f5eadc3a2ff81b549fd8053376b42fb55de3dcfd6bd35504e11f`,
at sole Module USB `0x00100000`, serial `E0C9125B0D9B`. After the visual
measurement/UI campaign it was reflashed unchanged; saved lighting, all 66
overrides and private VIA settings match the pre-test snapshot. Output is
**dark**, no test/rescue controller remains. Keep the wired camera open.

Normal source profile **revision 3** records the agreed provisional **900 mA
whole-board** budget, 120 mA electronics reserve and 100 mA margin (680 mA LED
allowance). Rough visually fitted nominal coefficients (R/G/B mA at 255):
front 11/11/11, rear 13/14/13, badge 13/13/13; conservative source coefficients
15/15/15,18/19/18,18/18/18. Profile remains **configured=false** pending
intended-range source/thermal qualification. Normal build is archived,
**not flashed**, SHA256
`11e8d49e6d00e3aa17856f93771c6a072df23b2d1b1f1d8d5fc11d14a29df692`.
Do not enable it or substitute synthetic test coefficients as qualified data.

Temporary profile 0x8002 keeps a450 mA model ceiling,120 mA base,100 mA margin,
20 mA/channel reference; saves disabled, startup-off, five-second watchdog.
Its raw test command0x8A allows up to50 LEDs requested at 255, otherwise150.
Larger loads in this campaign were limited; this is not unscaled full-range
qualification. Actual sparse full-drive and held-out combinations were measured;
47 run02 images were explicitly viewed, never OCR. Build03 had a diagnostic
R/G initializer bug; do not use its nonwhite samples. Build04 fixes it using
actual QMK GRB type definitions in host tests.

All output must pass `lighting/power_driver.c`/`lighting/power.c` at final
transmission. Preserve unscaled requests, preferences, all 104 mapped positions
(including broken103), and startup contract above. Production0x88 remains
read-only estimate telemetry;0x89 final RGB sums and 0x8A raw patterns are
temporary-calibration-only. No actual onboard current sensor.

User confirmed direct MacBook Pro → meter → PCB, board-only metering and secure
fixture. Do not repeat those questions or the 900 mA decision. Current documented
USB allocation is500 mA; higher source entitlement and PCB/fuse thermal headroom
are not verified. Need those physical/electrical checks and full-range validation
before normal deployment; no trip-point hunting or fuse bypass. Controlled cold
power/physical typing and hardware startup timing remain pending.

See [current evidence and next requirements](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/visual-calibration-20261004-lL59DO/README.md).
Older deployment/runtime statements elsewhere are historical. Rescue was stopped
by Ethan; **do not automatically restart it or restore high saved lighting**.

# Remote Machine Safety

The remote Kali machine exposed through SSH/ngrok is a GOLDEN STATE reference system.

Rules:
- Treat the remote Kali machine as read-only.
- Do not modify files on the remote Kali machine.
- Do not install, upgrade, remove, or reconfigure packages, tools, shells, PATH, or dependencies on the remote Kali machine.
- Do not run commands on the remote Kali machine that change system state.
- Use the remote Kali machine only for read-only inspection, version checks, and non-mutating build or diagnostic commands.
- Treat the remote Kali machine as the source of truth when comparing behavior against the local machine.

## SSH Access

- Current temporary SSH endpoint:
  - Host: `2.tcp.ngrok.io`
  - Port: `11608`
  - Username: `ethanhaaan`
- Authentication is SSH key only.
- Approved local key for access: `~/.ssh/id_ed25519`
- Preferred connection command:
  - `ssh -i ~/.ssh/id_ed25519 -p 11608 ethanhaaan@2.tcp.ngrok.io`
- This ngrok host and port are temporary. If the connection fails, ask the user for the latest SSH host and port instead of trying to reconfigure anything.
- Do not add, remove, rotate, or manage keys on the remote machine. Key management remains user-controlled.

## Remote Shell Behavior

- Prefer remote commands wrapped in login bash:
  - `ssh -i ~/.ssh/id_ed25519 -p 11608 ethanhaaan@2.tcp.ngrok.io 'bash -lic "<command>"'`
- Reason: the working remote environment loads `~/.local/bin` in login bash, which is required for the remote QMK tooling to resolve correctly.
- Do not assume non-login shells or `zsh` on the remote machine have the same PATH or QMK behavior.

## Correct Remote Repository

- The correct source-of-truth repository on the remote machine is:
  - `~/Desktop/pcb/qmk_firmware`
- Use that repository for all remote comparisons, build checks, and file inspection.
- Do not use stale or unrelated paths such as:
  - `~/qmk_firmware`
  - `~/Desktop/qmk_firmware`
- The verified remote branch for the source-of-truth repo is:
  - `firmware-highspeed-refactor`

## Remote Build Verification

- For read-only remote build checks, prefer running from the correct repo inside login bash.
- Verified example pattern:
  - `ssh -i ~/.ssh/id_ed25519 -p 11608 ethanhaaan@2.tcp.ngrok.io 'bash -lic "cd ~/Desktop/pcb/qmk_firmware && make toffee_studio/module:via -j1"'`
- Keep all remote verification non-mutating and scoped to inspection or compilation only.

## Local Module flashing and recovery

- **3 October 2026 update:** Ethan explicitly authorised the sole connected PCB
  at `0x02140000`; normal image `bfda11cf...` was copied successfully. Its measured
  bootloader serial was `E0C9125B0D9B`, previously assigned to B. Port/keymap-based
  A/B labels below are historical, not trustworthy physical identity. This was
  authorisation for this flash, not blanket authorisation for future boards.

- The RP2040 bootloader accepts a normal `.uf2` file copied onto its `RPI-RP2` drive (Finder drag-and-drop or `/bin/cp -X`). A custom USB programmer is not required.
- Select by freshly enumerated physical USB location (`--usb-location`), not enumeration order. Latest authorised sole-board target is `0x00100000`, serial `E0C9125B0D9B`. Older two-board B=`0x01142000`/A=`0x02140000` labels are historical, not identities. Require exactly one raw HID at the selected connection and exactly one matching RP2 bootloader before copying; recheck after reboot. Do not touch another board implicitly.
- On 1 October 2026, Module B's application-to-bootloader transition confirmed RP2 serial `E0C9125B0D9B` at the same physical connection `0x01142000`. Recheck both for each flash; do not infer identity from an application HID path, which changes on reboot.
- Preserve the known-working pre-underglow image at `/Users/ethanhan/Desktop/projects/toffee/ARCHIVE/firmware-recovery/2026-09-30-pre-underglow/pre-underglow-8c65e8ad.uf2`. SHA256: `8c65e8ad26255d3a16f7a8de709344917c0fba663cfc0ea2114e4d91031979c6`. Do not overwrite or delete it. The user confirmed both lights and typing after restoring it on 30 September 2026.
- Use `keyboards/toffee_studio/module/tools/flash_module.py --help` for the local updater. It archives each candidate and private VIA settings before rebooting, validates UF2 blocks, copies to one explicit drive, and checks keyboard/VIA reconnection and uptime. It must not change startup lighting as a diagnostic side effect.
- A macOS bootloader mount has stalled even while USB descriptor access worked. Resetting only the identified bootloader's USB connection cleared the stalled reads; ordinary UF2 copying then succeeded. This is not proof of what caused the original keyboard outage.
- Bound drive-access subprocesses, including their post-kill wait. A child stuck in kernel I/O can survive a kill until USB resets. Permit one targeted connection reset on a pre-copy read timeout; stop and inspect any copy failure instead of blindly retrying writes.
- USB enumeration and VIA replies do not prove physical typing or correct lighting. Obtain human confirmation and distinguish it from automated checks. Do not claim full flash readback or exact installed-image attestation from these checks.

# Module documentation entry point

Read [keyboards/toffee_studio/module/readme.md](keyboards/toffee_studio/module/readme.md)
first for the current screenless board, local project locations, USB identity,
lighting support, verification gaps, recovery images and detailed references.
Keep that README current when board behavior or verified status changes. Record
automated checks separately from human typing/lighting confirmation. Historical
discovery and per-key notes must not override its current-status section.

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

## Power-budget research candidate (4 October 2026)

The source now routes every RGB flush through `lighting/power_driver.c` and
`lighting/power.c`, before the existing RP2040 WS2812 transport. Never bypass this
final boundary. Preserve an unscaled render buffer, saved preferences, all 104
mapped positions (including broken 103), and the startup contract above.
`lighting/power_profile.c` is intentionally **unconfigured**: builds hold LEDs off.
No safe current ceiling/calibrated coefficients have been established; 90 mA and
12 mA/channel are preliminary/reference numbers, not qualified limits. Do not
turn the synthetic host-test profile into a deployed profile. `0x88` is read-only
estimated-current telemetry, not a physical current sensor or budget override.
The root `31cf12bf...` UF2 is built and archived, **not flashed**. The connected
PCB still runs its previous normal firmware; saved high lighting is unsafe.
Ethan explicitly requested stopping rescue later on 4 October: session `66996`
exited cleanly, process absence verified, no restore/save command. **Do not
automatically restart it.** Never restore high settings as routine cleanup.
Ethan confirmed board-only metering, secure support and direct MacBook Pro →
meter → PCB power (no hub); do not ask these again. New `IMG_7460.JPG` clearly
shows F1 marked `0`, matching the candidate BHFUSE 1 A fuse's marking table;
this does NOT establish a zero-ohm bypass or uniquely identify the fitted part.
Exact fitted-part/source allowance/thermal limits remain unresolved. Follow P0/P1 in
the [current research log](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/README.md).

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
- Select the target by an explicit physical USB location ID (`--usb-location`), not enumeration order. In the active two-board session, only exposed Module B at `0x01142000` is authorised for testing/flashing; Module A at `0x02140000` must stay untouched. Require exactly one raw HID interface at that target connection and exactly one RP2 bootloader before copying. Recheck identity after reboot.
- On 1 October 2026, Module B's application-to-bootloader transition confirmed RP2 serial `E0C9125B0D9B` at the same physical connection `0x01142000`. Recheck both for each flash; do not infer identity from an application HID path, which changes on reboot.
- Preserve the known-working pre-underglow image at `/Users/ethanhan/Desktop/projects/toffee/ARCHIVE/firmware-recovery/2026-09-30-pre-underglow/pre-underglow-8c65e8ad.uf2`. SHA256: `8c65e8ad26255d3a16f7a8de709344917c0fba663cfc0ea2114e4d91031979c6`. Do not overwrite or delete it. The user confirmed both lights and typing after restoring it on 30 September 2026.
- Use `keyboards/toffee_studio/module/tools/flash_module.py --help` for the local updater. It archives each candidate and private VIA settings before rebooting, validates UF2 blocks, copies to one explicit drive, and checks keyboard/VIA reconnection and uptime. It must not change startup lighting as a diagnostic side effect.
- A macOS bootloader mount has stalled even while USB descriptor access worked. Resetting only the identified bootloader's USB connection cleared the stalled reads; ordinary UF2 copying then succeeded. This is not proof of what caused the original keyboard outage.
- Bound drive-access subprocesses, including their post-kill wait. A child stuck in kernel I/O can survive a kill until USB resets. Permit one targeted connection reset on a pre-copy read timeout; stop and inspect any copy failure instead of blindly retrying writes.
- USB enumeration and VIA replies do not prove physical typing or correct lighting. Obtain human confirmation and distinguish it from automated checks. Do not claim full flash readback or exact installed-image attestation from these checks.

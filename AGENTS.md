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

Latest5 October recovery: Ethan requested restart after phone unplug/replug.
Preview Node52797 / exec55687 owns helper52806;
use http://127.0.0.1:5174. All subsequent measurements reuse `/frame.jpg`.
Fresh frame IDs advanced through129822; a5-minute underlying Continuity log
query found no matching start/stop/disconnect events in that bounded window.
Earlier77553/77563 and53454/53464 are stopped/historical. The latter wireless
session had transport reconnects that the helper's own counters missed.
Do not use application session counters alone to claim the phone stayed connected.

For repeated iPhone snapshots, keep `capture_camera.swift --serve DEVICE_ID`
running and send snapshot requests to the same process. Do not spawn the one-shot
mode each time. See the [camera usage and latest runtime IDs](keyboards/toffee_studio/module/readme.md#persistent-iphone-meter-camera).
Use `/frame.jpg` and `/status`; do not write to the helper's stdin or start
another camera owner. Historical helpers53464 and47541 are stopped. Camera
capture alone does not authorise PCB writes. Recheck live ownership and advancing
frames rather than treating these recorded PIDs as permanent identifiers.

## Permanent Module lighting startup contract

**11 October latest follow-up:** same archived normal profile7 flashed to fresh
sole USB0x01100000/RP2 E0C9125B0D9B; user did not specify A/B this request.
15 s USB stability and read-only preservation/profile checks pass;900 mA limit,
791 mA estimate after startup. Physical typing/cold power pending.
[Deployment](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/normal-repeat-20261011-1ZfXCP/README.md).

**11 October newest — NORMAL balanced shared profile7 DEPLOYED.** User explicitly
rejected unlimited mode. Shared A/B baseline103 mA,900 mA estimated whole-board
limit, margin0/797 mA LEDs. Normal saves and1s-dark/1s-fade retained. Fresh sole
USB0x00100000/RP2 E0C9125B0D9B; UF2 copy/15 s stability/read-only verification
pass. VIA/66 overrides/non-brightness preferences unchanged;799 mA estimated total.
Physical typing/cold power pending; last user board label A, not reidentified.
Source checkpoint `df4b31ace1`; manual branch/old estimator unchanged.
[Evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/shared-fit-20261011-tSgJXg/README.md).

**10 October newest: PCB-A explicitly switched to unlimited0x26101002 too.**
Same archived d5c46412... image, fresh USB0x00100000/RP2 E0C9125B0D9B.15 s USB
checks/read-only verification pass; correct build, dark output, preferences/VIA
preserved. No lit tests. Supersedes A-normal6 statements below. Physical typing/
cold-power pending. [Evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/pcb-a-test-20261010-Z5pCeR/README.md).

**10 October newest — PCB-B unlimited build0x26101002 flashed.** Same mixed0x8C
implementation/profile0x8006 as01, rebuilt for five UI high-current cases146–150.
No limiter/timeout/saves; startup contract unchanged. Standard UF2 copy to fresh
USB0x00100000/RP2 E0C9125B0D9B,15 s stability passes. No high-current automated
hardware sweep. PCB-A remains normal6. [Evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/high-mixed-20261010-Ad4Mie/README.md).

**10 October latest — PCB-B unlimited mixed build0x26101001 DEPLOYED.**
Manual-only0x8C atomically applies literal front/rear/badge RGB (9 bytes) to all
104 mapped positions. Original0x8A and informational model0x8006 unchanged.
No limiter/timeout, starts dark, no ordinary test saves; startup/rescue intact.
USB0x00100000/RP2 E0C9125B0D9B verified,15 s uptime and independent low-drive
RGB/hold checks pass, final dark. VIA/66 overrides/non-brightness prefs preserved.
PCB-A stays normal6; physical typing/cold-power pending.145-state UI includes
25 mixed cases, requires this build for mixed output, preserves older records.
[Evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/mixed-checklist-20261010-uLN8o3/README.md).

**10 October latest — official profile6 deployed to user-identified PCB-A.**
Fresh USB0x01100000/RP2 E0C9125B0D9B; archived c6197102... UF2 copied once.
15 s VIA stability and independent normal-profile/preservation checks pass.
900 mA estimated total, unchanged110 mA baseline,790 mA LED allowance; no source
change. Physical typing/cold-power pending. Supersedes historical unflashed or
manual-installed statements below.25 mixed-colour test cases/extended unlimited
firmware are authorised for PCB-B AFTER the user reconnects it, not A.
[Evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/pcb-a-normal-20261010-ElLC0a/README.md).

**10 October complete-data refit:** normal source profile6, built/archived NOT
FLASHED. Single `board_limit_ma=900`, margin0, existing PCB-B idle110 mA and790
mA LED allowance.240 lit observations fit shared RGB microamps front[11175,
11182,11416], rear[12706,12536,12435], badge[12862,12370,11788]. Held-out B MAE
24.30→19.725 mA with this idle value; A accuracy worsens when using B's idle.
Do not describe this as a universal model improvement. Startup and all104 mapping
unchanged; full host suite/normal build pass. Installed manual0x26100601/0x8006
remains unlimited. [Evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/refit-20261010-XsgK38/README.md).

**10 October latest deployment:** explicit request to resume manual measurements.
Archived unlimited0x26100601/profile0x8006 reflashed to fresh sole USB0x00100000,
RP2 serialE0C9125B0D9B verified before copy.15 s VIA stability and independent
read-only preservation/build/output checks pass; RAM brightness/final RGB zero.
No limiter/timeout, no test-lighting saves. Normal fitted profile5 stays unflashed.
Configurator restarted detached on5173 (PID79755), checklist/module HTTP200.
Original recovery intact; physical typing/cold-power unconfirmed for this flash.
[Evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/manual-resume-20261010-zDWOjw/README.md).

**8 October fitted normal source — NOT FLASHED:** profile5 now fits the user's
176 manual records (174 lit + two baselines), using actual final RGB sums.
Shared channel microamps: front[11106,11049,11300], rear[12506,12279,12360],
badge[11931,11610,10486]. Explicit B baseline110 mA;900 mA estimated whole-board
cap,790 mA LED allowance, no padding/extra margin/front160 cap. Held-out MAE14.55
mA versus146.41 previously. Full host suite and normal build pass; UF2 archived.
The currently installed unlimited0x26100601/profile0x8006 remains unchanged:
pending user choice whether to switch measurement mode, no hardware commands.
Do not auto-select A/B from a port/label; A's recorded baseline is80 mA, and B's
badge/combined coverage remains incomplete. This is an estimate, not a sensor or
hardware-safe certification. [Evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/fitted-model-20261008-IuYfWL/README.md).

**8 October PCB-B deployment:** user explicitly identified the connected board
as PCB-B and requested the same unlimited test firmware. Fresh sole Module
USB `0x00100000`, RP2 serial `E0C9125B0D9B` verified before UF2 copy. Archived
build `0x26100601` / profile `0x8006`, SHA256
`15e0697d67b2676b8fee687c9a9b774993a9a5efe3b19b746b399dd4deb434fa`.
15-second VIA/keyboard stability and read-only settings comparison passed;
all 66 overrides/non-brightness preferences preserved, RAM brightness and final
RGB zero, manual no-limiter flag confirmed. Physical typing/cold-power checks
for this deployment remain pending. No firmware code changes or new lit tests.
[Flash evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/pcb-b-unlimited-20261008-fMdDqY/README.md).

**6 October manual build DEPLOYED:** explicit user request; UF2 SHA256
`15e0697d67b2676b8fee687c9a9b774993a9a5efe3b19b746b399dd4deb434fa`,
build0x26100601/profile0x8006, fresh USB0x01100000/RP2 serialE0C9125B0D9B.
Updater archived/restored/verified private VIA and passed15 s response checks;
post-flash read-only checks match VIA/66 overrides/non-brightness preferences,
RAM brightness/final RGB zero. Ethan confirmed typing works. No cold-power test,
new current samples or camera access. Pre-change recovery remains unchanged.
UI `/toffee-debug` now uses120 fixed states plus baseline, row Apply/amps/Save,
PCB-labelled local storage/export; source `DEBUG_ENABLED` gates all debug UI.
No active current limiting: static endpoints approximate1 A using prior rough
coefficients; user must watch the meter and may skip rows. No automated120-state
hardware sweep authorised by this checklist task.
[Deployment/checklist evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/manual-checklist-flash-20261006-1f6iPy/README.md).

**6 October explicit manual-test exception:** Ethan requested no
limiter and chose hold-until-changed, no five-second timeout, dark startup and no
saved test lighting. `MODULE_MANUAL_MEASUREMENT=yes` requires both existing
`MODULE_EXTENSIVE_TEST=yes` and `MODULE_POWER_CALIBRATION=yes` plus build ID.
Only this opt-in build removes current/package limiting, front160 scaling and
raw population/current admission; startup gate,104-map and0x8B rescue retained.
Candidate0x26100601/profile0x8006 has informational1000/120/50 mA values, not
enforced ceilings. Production and earlier protected variants stay unchanged.
Debug0x88 flags bit5 means manual/no limiting/no timeout; UI must label it honestly.
Closing the UI does NOT blank it. Existing persistent preferences remain intact;
returning to normal firmware can reload them. Never relabel this as safe product
firmware or silently deploy it. [Candidate/tests](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/manual-unlimited-20261006-z6b0VE/README.md).

**Later5 October recovery:** user-reported older/different PCB rescued and flashed
with the same normal `6cbbc829...` UF2. Fresh serialE0C9125B0D9B at0x02100000
matches prior records; physical A/B identity unresolved. Private VIA preserved,
0x8B permanent zero verified, then all groups set8/255 and auto-save delay elapsed.
No rescue/handoff process remains.900 mA model unchanged. Physical restart/typing
not tested. [Latest recovery](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/second-pcb-recovery-20261005-6Tn4Ko/README.md).

**Previous normal deployment, 5 October (superseded above):** normal UF2
`6cbbc829dfca7f5d0a880dacf4678744ed3e909199a2834a703bc687021bf547`
is now flashed at freshly verified USB `0x02100000`, RP2 serial `E0C9125B0D9B`.
Enabled production profile4 retains provisional900 mA /120 mA baseline/100 mA
margin/680 mA LED allowance and conservative coefficients. Explicit user normal-
deployment instruction superseded the earlier hold below, NOT the unresolved
source/thermal qualification. No test watchdog/calibration commands. Eight-second
rear output without keepalive passed. New0x8B persistent zero passed storage
verification; other saved preferences/66 overrides/private VIA preserved.
Normal auto-save and1s-dark/1s-fade remain. No rescue/controller left running.
Physical cold-power and typing remain untested. Camera unchanged, UI5173 running.
[Deployment evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/normal-deployment-20261005-jtjo5A/README.md).
Earlier installed-build/deployment-hold statements below are historical.

**Explicit persistent rescue (5 October):** user authorised saving zero brightness
for front/rear/badge. New command `0x8B` requires `ZERO`, transmits black first,
preserves other saved preferences and verifies both storage records before success.
This is the ONLY zero-only persistence exception for protected test firmware.
Keep ordinary test setters/Save nonpersistent. Do not replace it with sequential
normal setters, which could re-enable high output. Rear verification must reload
EEPROM backing storage, not just read QMK's cached bytes. Preserve keymaps/macros.
Candidate `0x26100501` is built, NOT FLASHED; native helper PID89525/session92444
was STOPPED at Ethan's subsequent request and process exit verified. Old firmware
acknowledged only temporary off and rejected persistence; permanent recovery
unverified. Configurator now serves5173 (session84500). Do not restart rescue
while the UI is in use. Recheck hardware before any later deployment.
[Evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/persistent-rescue-20261005-wOY3vr/README.md).
The later explicit stop supersedes this session's rescue-start authorisation.

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

## Power-budget research checkpoint (5 October 2026)

**Later continuation:** Ethan explicitly authorised measurements toward1 A actual
current and says temperature instruments are unavailable. Do not ask again for
them or stop merely because idle readings vary; this is not thermal certification.
Installed0x26100407/profile0x8005 admits exact raw patterns within its temporary
1100 mA modeled envelope (80 mA baseline,100 mA margin,920 mA LED allowance),
allowing nominal board demand up to1000 mA, or rejects them;
startup-off, no saves,5s watchdog and final limiting of other paths remain.
Controller rejects any reduced final RGB sums. Record idle ranges rather than
requiring constant values. [Extension evidence](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/one-amp-20261004-kDqt5u/README.md)
is authoritative for build07/flash progression. Earlier83 accepted readings
through770 mA plus27 accepted extension readings give110 exact-output samples.
Build07 delivered25-front-white255 at910–930 mA,50-front-orangeR255/G136 at
900–940 mA, all104white58 at860–900 mA and all104red150 initially at800 mA.
Nominal channel mA at255: front12/12/12,rear15/15/15,badge13/13/13 plus80 mA
baseline. Observed-minus-estimate spans approximately-170 to+40 mA; retain100 mA
margin and idle ranges. Do not claim source/thermal qualification from these
provisional tests. Older checkpoint below is historical.

Installed protected temporary build **0x26100407**, SHA256
`1133fd7b76e5a630deca92c019048da640c24150e7b2b24559589da71a2f1cf8`,
at sole Module USB `0x02100000`; historical boot serial `E0C9125B0D9B` must be
rechecked at next flash. Latest read-only preservation check after measurements
confirms saved lighting, all66 overrides and private VIA match the baseline. Output is
**dark**, no test/rescue controller remains. Keep the wired camera open.

[5 October resumption](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/resumed-20261005-vftxHC/README.md)
passes fresh identity and read-only preservation checks on the same build.
Capture-then-blackout resolved slow image review:5–30-second requested holds,
45-second active-pulse deadline, all-off/zero-output verification before image
publication; firmware5-second heartbeat timeout unchanged. Captured readings
may be reviewed within10 minutes while dark, with original timestamps logged.
No OCR.19 additional observations (13 lit,6 off) bring the total to129, including
repeats/baselines. All104 orangeR110/G65 held930 mA, blue150 held810, green150
held820, red150 held800; front25 white255 held910. Previous940 mA remains maximum,
not an observed1 A. Nominal residual across all data now spans−189 to+40 mA;
rear mixed colours are especially overestimated.17 runner/numerical tests pass
302 assertions. Final read-only settings check passes with output zero;
controller stopped, camera open. No new flash or firmware logic change.

Normal source profile **revision 3** records the agreed provisional **900 mA
whole-board** budget, 120 mA electronics reserve and 100 mA margin (680 mA LED
allowance). Rough visually fitted nominal coefficients (R/G/B mA at 255):
front 11/11/11, rear 13/14/13, badge 13/13/13; conservative source coefficients
15/15/15,18/19/18,18/18/18. Profile remains **configured=false** pending
intended-range source/thermal qualification. Normal build is archived,
**not flashed**, SHA256
`11e8d49e6d00e3aa17856f93771c6a072df23b2d1b1f1d8d5fc11d14a29df692`.
That deployment hold was later superseded by the explicit normal-deployment
instruction above; never describe provisional coefficients as qualified data.

Historical temporary profile0x8002 kept a450 mA model ceiling,120 mA base,100 mA margin,
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
are not verified. Those physical/electrical checks and full-range validation
remain qualification gaps after the requested normal deployment; no trip-point
hunting or fuse bypass. Controlled cold
power/physical typing and hardware startup timing remain pending.

See [current evidence and next requirements](../AUTORESEARCH/04-09-2026%20PCB%20Lacking%20Current/resumed-20261005-vftxHC/README.md).
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
- Select by freshly enumerated physical USB location (`--usb-location`), not enumeration order. Latest sole-board location is `0x02100000`; last flash measured serial `E0C9125B0D9B` at former `0x00100000`. Recheck both before flashing. Older two-board B=`0x01142000`/A=`0x02140000` labels are historical, not identities. Require exactly one raw HID at the selected connection and exactly one matching RP2 bootloader before copying; recheck after reboot. Do not touch another board implicitly.
- On 1 October 2026, Module B's application-to-bootloader transition confirmed RP2 serial `E0C9125B0D9B` at the same physical connection `0x01142000`. Recheck both for each flash; do not infer identity from an application HID path, which changes on reboot.
- Preserve the known-working pre-underglow image at `/Users/ethanhan/Desktop/projects/toffee/ARCHIVE/firmware-recovery/2026-09-30-pre-underglow/pre-underglow-8c65e8ad.uf2`. SHA256: `8c65e8ad26255d3a16f7a8de709344917c0fba663cfc0ea2114e4d91031979c6`. Do not overwrite or delete it. The user confirmed both lights and typing after restoring it on 30 September 2026.
- Use `keyboards/toffee_studio/module/tools/flash_module.py --help` for the local updater. It archives each candidate and private VIA settings before rebooting, validates UF2 blocks, copies to one explicit drive, and checks keyboard/VIA reconnection and uptime. It must not change startup lighting as a diagnostic side effect.
- A macOS bootloader mount has stalled even while USB descriptor access worked. Resetting only the identified bootloader's USB connection cleared the stalled reads; ordinary UF2 copying then succeeded. This is not proof of what caused the original keyboard outage.
- Bound drive-access subprocesses, including their post-kill wait. A child stuck in kernel I/O can survive a kill until USB resets. Permit one targeted connection reset on a pre-copy read timeout; stop and inspect any copy failure instead of blindly retrying writes.
- USB enumeration and VIA replies do not prove physical typing or correct lighting. Obtain human confirmation and distinguish it from automated checks. Do not claim full flash readback or exact installed-image attestation from these checks.

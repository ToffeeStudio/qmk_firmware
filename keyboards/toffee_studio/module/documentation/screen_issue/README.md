# Screen Issue Dossier

This pack rebuilds the current screen-failure record from first-party repo code, the four provided DOCX files, the screenshot thread artifacts in this conversation, and externally checked module/controller references. It does not treat the existing reset guide as authoritative.

## Pack Contents

- [`timeline.md`](timeline.md): chronology of observations, swaps, reviewer findings, and reset discussions.
- [`evidence_matrix.md`](evidence_matrix.md): claim-by-claim status table.
- [`source_audit.md`](source_audit.md): source hierarchy and audit of earlier guide claims.

## Scope

This dossier is about the unresolved `random blank dark screen with backlight still on` problem on the Toffee Studio Module. It keeps three issue buckets separate because the source material describes them as distinct:

| Bucket | Status | Why it is separated |
| --- | --- | --- |
| Backlight too dim | Resolved in external review | Reported as a daughterboard soldering / low-voltage issue, not the current unresolved blank-screen issue. [D2] |
| Stability / snowflakes / noise | Partially addressed, not closed | External review ties this to noise and missing capacitors. [D2] |
| Random blank dark screen with backlight | Unresolved primary issue | This is the persistent failure still being debated across the docs, screenshots, and current firmware discussion. [D1][D2][A2][A4][A6] |

## Evidence Ladder

This pack ranks evidence in the following order:

1. Repo code and config currently used by this firmware build.
2. Direct artifacts supplied in this investigation, including the DOCX files and screenshot thread.
3. Manufacturer-hosted external references.
4. Clearly labeled mirrors or distributor-hosted references.
5. Inference.

Any statement below that is not directly backed by sources is explicitly marked as `reported`, `unverified`, or `hypothesis`.

## What Is Actually Verified Right Now

### Hardware and module baseline

- The TFT module family cited in the external hardware references is `N085-1212TBWIG41-H12`, a `0.85"` `128 x 128` TFT using a `GC9107` driver and a `SPI4 LINE` interface. The module is specified as `normally black`, which matters because a lit backlight does not imply an initialized panel. [W1]
- The module pin definition in the manufacturer-hosted spec shows a dedicated `RESET` pin on pin 11, and the spec states that the signal is `active low` and `must be applied to properly initialize the chip`. [W1]
- The same manufacturer-hosted spec shows a dedicated `CS` pin on pin 8 with `Low enable, high disable`, and identifies `D/C`, `SCL`, and `SDA` pins for the 4-line serial interface. [W1]
- The GC9107 controller datasheet identifies support for `3-/4-line serial peripheral interface (SPI)` and documents the user commands `Sleep Out (11h)`, `Display ON (29h)`, and `COLMOD / Pixel Format Set (3Ah)`. [W2]

### Current firmware baseline

- The current firmware pin map is explicit in `config.h`: `GP2` is `SPI_SCK_PIN`, `GP3` is `SPI_MOSI_PIN`, `GP1` is `OLED_DC_PIN`, and `GP0` is `OLED_BL_PIN`. [R1]
- The display device is created in `display/ui.c` with `qp_gc9107_make_spi_device(128, 128, 0xFF, OLED_DC_PIN, 0xFF, 8, 0)`. In other words, the current firmware path passes `0xFF` for both the chip-select pin and the reset pin, so this firmware does not drive an MCU-controlled CS line or an MCU-controlled reset line for the display. [R2]
- `keyboard_post_init_kb()` waits `3000 ms`, turns the backlight on immediately with `writePinHigh(0)`, then calls `ui_init()`. It also schedules a one-shot `ui_reinit_display()` after `30000 ms`. [R3]
- The QMK GC9107 driver sends an init sequence that includes internal-register unlock commands, voltage-related commands, `GC9XXX_SET_PIXEL_FORMAT`, gamma commands, `GC9XXX_CMD_SLEEP_OFF`, and `GC9XXX_CMD_DISPLAY_ON`. The driver then writes rotation via `MADCTL`. [R4]
- The current QMK opcode definitions map `GC9107_PIXEL_FORMAT_16_BPP_IFPF` to `0b101`, and the init sequence uses `0x23` for VGH and `0x47` for VGL. Those values line up with the QMK driver implementation, but they should be treated as implementation facts, not proof that the hardware problem is firmware-rooted. [R4][R5]

### What the supplied documents consistently say

- The physical connection chain is described consistently as `Main PCB -> FFC cable -> screen daughterboard -> TFT module`. [D1][D3][D4]
- The strongest repeated observation in the issue dump is that multiple screen assemblies reportedly worked on the `OLD Main PCB`, while multiple screen assemblies reportedly failed on the `LATEST Main PCB`, which pushed the investigation toward the main PCB rather than a single bad screen assembly. [D1]
- The same issue dump later complicates that picture: a newly purchased FFC cable reportedly caused the screen to come back, and after that the old cables reportedly worked again as well. That observation weakens any overly simple `main PCB only` diagnosis. [D1]
- The external review pack aimed at a hardware expert emphasizes interconnect reliability and says mechanical handling of the interconnect could reproduce or worsen the screen-off behavior. [D4]
- The later external review diagnosis explicitly states that the unresolved item is still `Random blank (dark) screen with backlight`, and also records that a dedicated reset circuit was `currently NOT working`. [D2]

### What the screenshot artifacts add

- The screenshot thread records a proposed fix direction to add either an `IO reset from MCU` or an `RC reset circuit` for the LCD. [A1][A4]
- The screenshot thread records a direct report that, in at least one observed failure, the LCD did not display normally until the `MCU button` was pressed. [A2][A3]
- The screenshot thread also records a stronger recovery claim: in one described state, a `manual LCD reset` followed by pressing the `MCU button` could return the LCD to normal. That is important because it argues against the blanket claim that only a full power cycle can recover the system. [A5]
- The screenshot thread records that `RC reset` was the preferred prototype-side test method because it was easier to add than MCU-driven IO reset. [A5]
- The screenshot thread also contains a firmware-init suspicion, but the same thread explicitly reduces that point to `just a guess`. That suspicion should therefore be tracked as a weak hypothesis, not as a fact. [A6]

## Contradictions That Must Stay Visible

- `Old PCB works, latest PCB fails` is an important observation, but it does not fully survive later evidence because the system also showed cable-sensitive recovery behavior. [D1]
- `Interconnect reliability is the likely area` and `the main PCB is the problem` are not identical claims. The pack keeps both because the source material supports both directions at different times. [D1][D4]
- `Reset can recover the display` and `dedicated reset circuit currently NOT working` can both be true if the attempted reset implementation or reset timing was inadequate, or if reset only helps in some failure modes. The present evidence does not resolve that. [D2][A5]
- `Firmware init sequence may be wrong` remains weak because the same thread that raised it also downgraded it to a guess, and the same firmware family reportedly works on the old PCB. [D1][A6]

## Ranked Hypotheses

### 1. Missing or ineffective hardware reset is a real robustness gap, but probably not the whole root cause

This is currently the strongest cross-source conclusion. The module spec says the panel has an active-low reset pin that must be applied to properly initialize the chip, while the firmware path currently configures no MCU-controlled reset pin at all. The screenshot thread also reports that a manual LCD reset plus an MCU reset can recover the display in at least one bad state. [W1][R2][A5]

What this explains well:

- Why the system can show `backlight on` while the display controller is still not in a healthy visible-output state.
- Why manual reset experiments were proposed so early.
- Why an RC reset or GPIO-driven reset is repeatedly discussed as a likely production hardening step. [A1][A4][A5]

What weakens it:

- It explains a lack of recovery leverage, but not necessarily why the system enters the bad state in the first place.
- External review also points to interconnect and power-integrity issues, so reset absence may be a recovery problem layered on top of another trigger. [D2][D4]

What would falsify or weaken it further:

- A trusted schematic or test result proving the panel already receives a clean, reliable reset pulse in the shipped design.
- Reproduction of the blank-screen issue even when a verified-good reset sequence is applied directly to the module. 

### 2. Main-PCB-to-screen interconnect reliability remains a plausible trigger

This remains a serious candidate because the issue history repeatedly ties failures to insertion, cable changes, handling, rerouting, and a later external review request focused almost entirely on interconnect reliability and mechanical stress. [D1][D4]

What this explains well:

- Why replacing or reusing FFC cables changed behavior.
- Why the issue was sensitive to assembly / disassembly history.
- Why a hardware expert was asked to focus on cable routing, strain, pad geometry, shorts to the aluminum case, and continuity under flex. [D1][D4]

What weakens it:

- A purely open-circuit explanation does not fit neatly with reports that reset actions can sometimes recover the screen without changing the cable path.
- The later external diagnosis separates the blank-screen issue from the already identified dim-backlight and noise issues. [D2][A5]

What would falsify or weaken it further:

- Stable oscilloscope or logic-analyzer captures showing healthy power and SPI signaling end-to-end during a failure on the exact bad hardware.
- Controlled flex testing that fails to correlate with the blank-screen state.

### 3. Power integrity may be a contributing trigger, but the present evidence does not prove it is the primary cause of the blank-screen issue

The external review diagnosis attributes earlier problems to low module voltage, higher noise, and missing capacitors. That makes power quality impossible to ignore. [D2]

What this explains well:

- Why separate issues such as dim backlight and snowflake/noise artifacts appeared in the same program.
- Why missing capacitors and local decoupling were called out in the external review. [D2]

What weakens it:

- The same diagnosis document treats the blank dark screen as a separate unconfirmed problem, rather than closing it as a solved power issue. [D2]
- The issue dump reports that coarse voltage and continuity checks looked normal during at least some blank-screen events. [D1]

What would falsify or weaken it further:

- Clean power captures at the screen daughterboard during failing boots, paired with persistent blank-screen behavior.
- Confirmation that the missing-capacitor problem only affected the earlier snowflake/noise bucket and not the later blank-screen bucket.

### 4. A pure firmware initialization-sequence bug is possible, but currently the weakest major hypothesis

The init-sequence idea should stay on the board, but only as a weak candidate. The screenshot thread itself frames it as a guess, and the broader issue history says the same overall firmware approach worked on the old PCB and even worked on the latest PCB for stretches of time. [D1][A6]

What this explains well:

- Why a manual LCD reset followed by MCU restart could matter.
- Why a display could stay dark despite the backlight being on.

What weakens it:

- It does not explain the strong cable / assembly sensitivity documented elsewhere.
- It does not explain why the same code path would reportedly behave differently across PCB revisions unless the hardware environment changed first. [D1][D4]

What would falsify or strengthen it:

- Captured failing SPI transactions showing the GC9107 never leaves the expected post-init state.
- A controlled A/B test proving the old PCB and latest PCB receive different electrical startup conditions while running the same firmware image.

## Bottom Line

The highest-confidence current position is not `it is firmware` and not `it is only the cable`. The best-supported view is:

- the design currently lacks a trusted, firmware-driven hardware reset path for the panel,
- the system has a documented history of interconnect and power-quality sensitivity,
- and the unresolved blank-screen issue may be a controller-state failure that is triggered by hardware conditions rather than caused by the init sequence alone. [W1][D1][D2][D4][A5]

## Next Evidence Needed

- Trusted source schematic or PCB files for the latest main PCB and screen daughterboard, especially the real handling of `CS`, `RESET`, backlight drive, and local decoupling.
- A controlled boot-failure capture on the bad hardware: `3V3`, panel reset line, and SPI `SCL/SDA/DC`.
- A precise record of what the attempted `dedicated reset circuit` actually did, because the current note that it was `NOT working` is too thin to interpret safely. [D2]
- Confirmation whether the `latest main PCB` failures and the later `new cable jump-start` behavior came from the exact same board and screen assembly set. [D1]

## References

- [R1] Repo config: `config.h:15-20`
- [R2] Repo display init: `display/ui.c:14-19`, `display/ui.c:55-59`
- [R3] Repo boot flow: `module.c:141-153`, `module.c:157-222`
- [R4] Repo GC9107 init sequence: `drivers/painter/gc9xxx/qp_gc9107.c:19-52`
- [R5] Repo GC9107 parameter definitions: `drivers/painter/gc9xxx/qp_gc9107_opcodes.h:35-99`
- [D1] `/Users/ethanhan/Downloads/PCB Screen Issues 2/Issue Description Document.docx`
- [D2] `/Users/ethanhan/Downloads/PCB Screen Issues 2/External Review Diagnosis and Findings 22-03-2026.docx`
- [D3] `/Users/ethanhan/Downloads/PCB Screen Issues 2/PCB Specification Sheet.docx`
- [D4] `/Users/ethanhan/Downloads/PCB Screen Issues 2/Toffee_External_Review_Pack_Screen_Interconnect.docx`
- [A1] Screenshot artifact: message proposing RP2040 IO reset or RC reset, with reset-circuit sketch.
- [A2] Screenshot artifact: message stating the LCD did not display normally until the MCU button was pressed.
- [A3] Screenshot artifact: follow-up confirming the MCU button was pressed and sharing `hardware_screen_reset_guide.pdf`.
- [A4] Screenshot artifact: message stating the current design pulls the reset pin high and suggesting MCU IO reset or RC reset.
- [A5] Screenshot artifact: follow-up confirming IO reset and RC reset have the same effect, RC reset is easier for prototype testing, and manual LCD reset plus MCU button can recover the display in one state.
- [A6] Screenshot artifact: follow-up saying the init-command concern is `just a guess`.
- [W1] Wisevision module spec PDF: [SPEC N085-1212TBWIG41-H12 VER A](https://www.jx-wisevision.com/uploads/SPEC-N085-1212TBWIG41-H12-VER-A-.pdf)
- [W2] GC9107 datasheet PDF mirror hosted by Midas Displays support: [GC9107 Datasheet V1.2](https://support.midasdisplays.com/wp-content/uploads/2025/06/GC9107.pdf)
- [W3] Distributor listing for the module family: [LCSC N085-1212TBWIG41-H12](https://www.lcsc.com/product-detail/LCD-Displays-Modules_Shenzhen-Allvision-Tech-N085-1212TBWIG41-H12_C5123573.html)

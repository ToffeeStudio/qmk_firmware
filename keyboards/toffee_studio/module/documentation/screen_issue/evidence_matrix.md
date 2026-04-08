# Screen Issue Evidence Matrix

This matrix is intentionally conservative. `Validated` means the claim is directly supported by repo code, a direct artifact, or an external reference. `Reported` means the claim appears in a supplied artifact but has not been independently proven here. `Weak hypothesis` means it remains possible but is not strong enough to state as fact.

| ID | Claim | Status | Sources | Engineering note |
| --- | --- | --- | --- | --- |
| E01 | The screen path is `main PCB -> FFC cable -> screen daughterboard -> TFT module`. | Validated | [D1][D3][D4] | This is stable across the supplied documents. |
| E02 | The module family in scope is `N085-1212TBWIG41-H12`, a `0.85"` `128 x 128` TFT using `GC9107` and `SPI4 LINE`. | Validated | [W1][W3] | This is the cleanest external hardware baseline for the pack. |
| E03 | The module is specified as `normally black`. | Validated | [W1] | Backlight-on does not guarantee valid image output. |
| E04 | The module has a dedicated active-low `RESET` pin and the spec says it must be applied to properly initialize the chip. | Validated | [W1] | Strong reason to keep reset-path questions central. |
| E05 | The module has a dedicated `CS` pin with low-enable semantics. | Validated | [W1] | This proves the panel supports CS, not how the present daughterboard ties it off. |
| E06 | The current firmware pin map is `GP2=SCK`, `GP3=MOSI`, `GP1=DC`, `GP0=BL`. | Validated | [R1] | Good firm baseline from repo config. |
| E07 | The current firmware config does not drive a dedicated display `CS` pin or display `RESET` pin. | Validated | [R2] | This is a firmware-path fact, not a full schematic fact. |
| E08 | The current boot flow turns on the backlight before display init and schedules a software re-init after 30 seconds. | Validated | [R2][R3] | Useful for understanding observed `backlight on but blank` states. |
| E09 | The QMK GC9107 init sequence includes `Sleep Off`, `Display On`, `Pixel Format Set`, and vendor/internal-register commands. | Validated | [R4][R5][W2] | Good basis for discussing init behavior without overclaiming cause. |
| E10 | The same screen assemblies reportedly work on the old PCB and fail on the latest PCB. | Reported | [D1] | Strongly diagnostic, but later cable behavior prevents treating it as the whole story. |
| E11 | Coarse continuity and voltage checks reportedly looked normal during at least some failures. | Reported | [D1] | Useful, but not enough to clear signal integrity or transient startup problems. |
| E12 | A brand new FFC cable reportedly revived the screen, after which the old cables also worked again. | Reported | [D1] | This is one of the most important complication points in the case. |
| E13 | The external interconnect pack says mechanical stress and handling sensitivity can reproduce or worsen the blank-screen problem. | Reported | [D4] | Keeps interconnect reliability high on the list. |
| E14 | The `backlight too dim` issue was caused by a daughterboard soldering problem that dropped module voltage to `2.74V`. | Reported external finding | [D2] | Important history, but kept separate from the unresolved blank-screen bucket. |
| E15 | The `snowflakes / power noise` issue was linked to missing capacitors and decoupling recommendations. | Reported external finding | [D2] | Makes power integrity relevant even if not yet proven primary for the blank screen. |
| E16 | The unresolved issue is still `random blank (dark) screen with backlight`, and restarting the MCU sometimes helps. | Reported external finding | [D2] | Confirms the active failure bucket. |
| E17 | A dedicated reset circuit was attempted and was `currently NOT working`. | Reported external finding | [D2] | Needs follow-up because the document does not define what the attempted circuit actually did. |
| E18 | In one screenshot account, the LCD reportedly did not display normally until the MCU button was pressed. | Direct artifact report | [A2][A3] | Consistent with a controller-state or startup-state problem. |
| E19 | In one screenshot account, manual LCD reset followed by the MCU button reportedly restored normal display. | Direct artifact report | [A5] | Strong evidence that reset affects at least some failure states. |
| E20 | RC reset was preferred for prototype testing because it was easier to add than MCU-driven IO reset. | Direct artifact report | [A5] | Explains prototype direction; does not prove it is the best production fix. |
| E21 | The current design pulls the reset pin high. | Reported, not independently validated here | [A4] | Plausible, but still needs trusted schematic confirmation. |
| E22 | The physical panel `CS` line is hardwired low in the current hardware. | Unverified inference | [R2][W1] | Firmware uses no CS GPIO, but the exact tie-off should be confirmed in real schematics. |
| E23 | The blank dark screen is purely caused by the firmware initialization command sequence. | Weak hypothesis | [A6][D1][R4][W2] | Present evidence is too weak to close the case this way. |
| E24 | The missing hardware-reset path is a real robustness gap. | Validated for firmware path, hypothesis for root cause | [W1][R2][A5] | Strongly supported as a design-hardening issue; not yet proven as the sole trigger of failure. |
| E25 | The issue is only a cable or connector problem. | Weak hypothesis | [D1][D4][A5] | Cable sensitivity is real, but reset-sensitive recovery argues against a cable-only story. |
| E26 | The issue is only a main-PCB problem. | Weak hypothesis | [D1][D4] | Early evidence supports it, later cable behavior weakens it. |

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
- [A2] Screenshot artifact: LCD only came back after MCU button press.
- [A3] Screenshot artifact: confirmation of MCU-button press plus `hardware_screen_reset_guide.pdf`.
- [A4] Screenshot artifact: claim that current reset pin is pulled high plus recommendation to add MCU IO reset or RC reset.
- [A5] Screenshot artifact: RC reset easier for prototype and manual LCD reset plus MCU button can recover.
- [A6] Screenshot artifact: init-sequence concern explicitly described as a guess.
- [W1] Wisevision module spec PDF: [SPEC N085-1212TBWIG41-H12 VER A](https://www.jx-wisevision.com/uploads/SPEC-N085-1212TBWIG41-H12-VER-A-.pdf)
- [W2] GC9107 datasheet mirror: [GC9107 Datasheet V1.2](https://support.midasdisplays.com/wp-content/uploads/2025/06/GC9107.pdf)
- [W3] LCSC listing: [N085-1212TBWIG41-H12](https://www.lcsc.com/product-detail/LCD-Displays-Modules_Shenzhen-Allvision-Tech-N085-1212TBWIG41-H12_C5123573.html)

# Source Audit

This file explains which sources were trusted, which were only used as artifacts, and which claims from the earlier reset-guide material were downgraded or rejected.

## Source Hierarchy Used In This Pack

### Tier 1: Repo source of truth

- `config.h`
- `display/ui.c`
- `module.c`
- `drivers/painter/gc9xxx/qp_gc9107.c`
- `drivers/painter/gc9xxx/qp_gc9107_opcodes.h`

These files are authoritative for current firmware behavior. They are not authoritative for unshared schematic details.

### Tier 2: Direct supplied artifacts

- `/Users/ethanhan/Downloads/PCB Screen Issues 2/Issue Description Document.docx`
- `/Users/ethanhan/Downloads/PCB Screen Issues 2/External Review Diagnosis and Findings 22-03-2026.docx`
- `/Users/ethanhan/Downloads/PCB Screen Issues 2/PCB Specification Sheet.docx`
- `/Users/ethanhan/Downloads/PCB Screen Issues 2/Toffee_External_Review_Pack_Screen_Interconnect.docx`
- Screenshot thread artifacts in this conversation

These are authoritative for what was observed or reported, but not automatically for root cause.

### Tier 3: Manufacturer-hosted external references

- [Wisevision module spec PDF](https://www.jx-wisevision.com/uploads/SPEC-N085-1212TBWIG41-H12-VER-A-.pdf)

This is the strongest external hardware source used here for module identity, pinout, reset semantics, interface type, and operating conditions.

### Tier 4: Clearly labeled mirror or distributor references

- [GC9107 datasheet mirror hosted by Midas Displays support](https://support.midasdisplays.com/wp-content/uploads/2025/06/GC9107.pdf)
- [LCSC module-family listing](https://www.lcsc.com/product-detail/LCD-Displays-Modules_Shenzhen-Allvision-Tech-N085-1212TBWIG41-H12_C5123573.html)

These were used because the current search pass did not surface a clean, publicly indexed GalaxyCore-hosted GC9107 PDF. The PDF itself identifies `Galaxycore Incorporation` and version `V1.2`, but the hosting is still treated as mirrored rather than official. [W2]

## Treatment Of Existing Reset Guides

- `documentation/hardware_screen_reset_guide.md` is treated as an untrusted derivative note. The user explicitly stated that the corresponding `hardware_screen_reset_guide.pdf` was AI-generated.
- `documentation/screen_reset_sequence.md` was treated as a secondary note, not as authority. Some of its claims do match current repo code, but code was re-checked directly rather than inherited blindly.

## Audit Of Earlier Guide Claims

| Claim from earlier guide material | Status in this pack | Why |
| --- | --- | --- |
| The display module uses `GC9107`, is `0.85"` and `128 x 128`. | Validated | Confirmed by Wisevision module spec and distributor listing. [W1][W3] |
| The panel uses a 4-line serial interface. | Validated | Confirmed by Wisevision module spec and GC9107 datasheet. [W1][W2] |
| `GP2=SCK`, `GP3=MOSI`, `GP1=DC`, `GP0=BL`. | Validated | Confirmed directly by repo config. [R1] |
| The current firmware path does not drive a dedicated panel reset pin. | Validated | Confirmed by `qp_gc9107_make_spi_device(..., 0xFF, ..., 0xFF, ...)`. [R2] |
| The current firmware path does not drive a dedicated panel chip-select pin. | Validated | Confirmed by the same `0xFF` device construction. [R2] |
| The module has a hardware reset pin and it is active low. | Validated | Confirmed by manufacturer-hosted module spec. [W1] |
| The firmware init sequence includes `Sleep Out (11h)` and `Display On (29h)`. | Validated | Confirmed by current repo driver and GC9107 datasheet. [R4][W2] |
| The firmware init sequence uses `16-bit RGB565` pixel format. | Validated | Confirmed by repo opcodes and driver, and consistent with GC9107 `COLMOD (3Ah)`. [R4][R5][W2] |
| `CS` is hardwired low on the real hardware. | Weakened | Plausible from the current firmware path and the limited six-wire interconnect context, but not directly proven by a trusted schematic in this pack. [R2][D1][D3] |
| The reset pin on the present daughterboard is definitely pulled high by a known resistor value. | Not fully validated | A screenshot artifact says the reset pin is pulled high, but this pack does not have a trusted PCB source file to prove the exact implementation or resistor value. [A4] |
| Exact daughterboard BOM details such as `Q2 = DMG1012T`, `R11 = 33 ohm`, or `R13 = 1k` are established facts. | Unverified here | Those details may be true, but they were not accepted without a trusted schematic or manufacturer document. |
| A dark panel with backlight means the controller is still asleep or uninitialized. | Partially validated, still non-unique | The module is `normally black`, the backlight is independent, and the firmware turns BL on before init. That makes the interpretation plausible, but the same visual symptom could still come from multiple controller or signal failures. [W1][R3] |
| Only a full power cycle can recover the display from a bad state. | Rejected | Screenshot evidence says manual LCD reset plus MCU button can recover in at least one state. [A5] |
| The datasheet itself requires a `20 ms` reset pulse. | Rejected / downgraded | The present pack could confirm that reset exists and is active low, but did not confirm a strict official `20 ms` minimum from the trusted external sources used here. |

## Claims Intentionally Downgraded In The Main Dossier

- Any statement about the exact physical handling of `CS` on the daughterboard.
- Any statement about the exact current reset-circuit topology on the shipped hardware.
- Any statement about exact daughterboard part values not proven from trusted schematics.
- Any statement that the blank dark screen is purely firmware or purely interconnect.

## External Source Notes

- [W1] is manufacturer-hosted and is the strongest external source used in this pack.
- [W2] is a mirror-hosted controller datasheet. It is useful for controller capabilities and command semantics, but it remains labeled as a mirror in every file.
- [W3] is a distributor listing and was only used as supporting context, not as the main authority.

## References

- [R1] Repo config: `config.h:15-20`
- [R2] Repo display init: `display/ui.c:14-19`, `display/ui.c:55-59`
- [R3] Repo boot flow: `module.c:157-222`
- [R4] Repo GC9107 init sequence: `drivers/painter/gc9xxx/qp_gc9107.c:19-52`
- [R5] Repo GC9107 parameter definitions: `drivers/painter/gc9xxx/qp_gc9107_opcodes.h:35-99`
- [D1] `/Users/ethanhan/Downloads/PCB Screen Issues 2/Issue Description Document.docx`
- [D3] `/Users/ethanhan/Downloads/PCB Screen Issues 2/PCB Specification Sheet.docx`
- [A4] Screenshot artifact: claim that current reset pin is pulled high plus recommendation to add MCU IO reset or RC reset.
- [A5] Screenshot artifact: RC reset easier for prototype and manual LCD reset plus MCU button can recover.
- [W1] Wisevision module spec PDF: [SPEC N085-1212TBWIG41-H12 VER A](https://www.jx-wisevision.com/uploads/SPEC-N085-1212TBWIG41-H12-VER-A-.pdf)
- [W2] GC9107 datasheet mirror: [GC9107 Datasheet V1.2](https://support.midasdisplays.com/wp-content/uploads/2025/06/GC9107.pdf)
- [W3] LCSC listing: [N085-1212TBWIG41-H12](https://www.lcsc.com/product-detail/LCD-Displays-Modules_Shenzhen-Allvision-Tech-N085-1212TBWIG41-H12_C5123573.html)

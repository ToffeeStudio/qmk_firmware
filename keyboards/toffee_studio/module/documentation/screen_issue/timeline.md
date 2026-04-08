# Screen Issue Timeline

This timeline preserves the order in which claims entered the investigation. It intentionally keeps contradictions visible.

## Early hardware context

- The product definition describes a four-part screen path: `main PCB -> FFC cable -> screen daughterboard -> TFT LCD module`. [D1][D3]
- The TFT module family referenced in the external material is `N085-1212TBWIG41-H12`, a `0.85"` `128 x 128` `GC9107` SPI display. [W1][W3]

## Latest-main-PCB issue appears

- The issue dump says the `LATEST Main PCB` already showed problems when first received. One early complaint was unstable USB connection unless the USB-C plug was held in a very specific orientation. [D1]
- The same issue dump says the screen would sometimes work after connecting a screen assembly to the latest PCB, but once the assembly was removed and reinserted, the display would then consistently fail to turn on again. [D1]
- After leaving a working screen assembly installed, the system reportedly worked for about a month without disassembly, despite frequent unplugging and HID use. [D1]

## Failure becomes persistent

- After that working period, the screen reportedly started failing again with the current signature: black image area with a faintly lit backlight rather than a pitch-black panel. [D1]
- Changing cables and reconnecting the FFC reportedly stopped helping at that stage. [D1]

## Old-vs-new PCB comparison pushes suspicion toward main PCB

- The issue dump says the same screen assembly that failed on the latest PCB worked on the `OLD Main PCB`. [D1]
- It also says other screen assemblies worked on the old PCB, while multiple different screen assemblies still failed on the same defective latest PCB. [D1]
- That cluster of observations led to the working conclusion in the issue dump that the main PCB itself was the problem. [D1]

## Electrical checks do not cleanly close the case

- The same issue dump says continuity looked normal to the screen daughterboard during malfunction. [D1]
- It also says voltage measurements compared against the working PCB looked normal at a coarse level, while still leaving open the possibility that correct voltage was present alongside bad data or a bad startup state. [D1]

## Cable behavior complicates the diagnosis

- In the later `Recent update (27th November)` section, the issue dump records a critical twist: swapping to a newly ordered FFC cable reportedly made the screen start working again. [D1]
- After that recovery, the previously failing old FFC cables reportedly also worked again. [D1]
- The author explicitly flags this as an observation that may be biased and asks future analysis not to treat it as certain fact. [D1]

## External interconnect-reliability framing

- The `Toffee_External_Review_Pack_Screen_Interconnect.docx` document reframes the main question around interconnect reliability and manufacturability, especially for the `main PCB <-> screen daughterboard` link. [D4]
- That pack says the blank-screen issue is intermittent, sensitive to assembly and handling, and historically associated with ZIF/FFC sensitivity. [D4]
- It also says that mechanical stress on the interconnect could replicate screen-off behavior and that heavy handling or rerouting increased later failure likelihood. [D4]

## External review narrows the problem buckets

- The `External Review Diagnosis and Findings 22-03-2026.docx` document separates the investigation into three problems. [D2]
- Problem 1, `backlight too dim`, is marked solved and tied to a daughterboard soldering issue that reduced module voltage to `2.74V`. [D2]
- Problem 2, `stability / snowflakes + noise in power`, is linked to decoupling and missing capacitors. [D2]
- Problem 3, `random blank (dark) screen with backlight`, remains unconfirmed and persistent. The document records that restarting the MCU sometimes helps and that a dedicated reset circuit was currently not working. [D2]

## Firmware-side facts from the current repo

- The repo config maps `GP2` to `SPI_SCK_PIN`, `GP3` to `SPI_MOSI_PIN`, `GP1` to `OLED_DC_PIN`, and `GP0` to `OLED_BL_PIN`. [R1]
- The current display setup does not assign a CS GPIO or reset GPIO in `qp_gc9107_make_spi_device(..., 0xFF, ..., 0xFF, ...)`. [R2]
- Boot flow waits three seconds, enables the backlight, initializes the display, and later performs a one-shot software re-init after thirty seconds. [R2][R3]
- The display init sequence in the QMK driver includes `Sleep Off`, `Display On`, and `Pixel Format Set`. [R4][W2]

## Screenshot-thread reset discussion

- A screenshot thread records a request to add a reset circuit for the RP2040/display path and shows a simple RC-reset sketch. [A1]
- Another screenshot says the LCD did not display normally even after waiting thirty seconds until the MCU button was pressed. [A2]
- A follow-up confirms that the MCU button really was pressed and shares a `hardware_screen_reset_guide.pdf`. [A3]
- Another message says that, in the current design, the reset pin is pulled high and that an MCU-driven IO reset or RC reset should be added. [A4]
- The same thread confirms that MCU-driven IO reset and RC reset should have the same effect, and that RC reset is easier for prototype testing. [A5]
- The same thread says that in one failure state, manually resetting the LCD and then pressing the MCU button returns the LCD to normal. [A5]
- The thread later raises a firmware-initialization suspicion, then explicitly reduces it to `just a guess`. [A6]

## Current state at the end of this pass

- The evidence still supports multiple interacting hardware contributors: interconnect sensitivity, prior power-integrity problems, and the absence of a trusted MCU-controlled panel reset path. [D1][D2][D4][R2][W1]
- The evidence does not currently justify closing the issue as purely firmware-rooted. [D1][A6]
- The evidence also does not justify closing the issue as purely cable-rooted, because reset-driven recovery and old-vs-new PCB behavior remain in the record. [D1][A5]

## References

- [R1] Repo config: `config.h:15-20`
- [R2] Repo display init: `display/ui.c:14-19`, `display/ui.c:55-59`
- [R3] Repo boot flow: `module.c:141-153`, `module.c:157-222`
- [R4] Repo GC9107 init sequence: `drivers/painter/gc9xxx/qp_gc9107.c:19-52`
- [D1] `/Users/ethanhan/Downloads/PCB Screen Issues 2/Issue Description Document.docx`
- [D2] `/Users/ethanhan/Downloads/PCB Screen Issues 2/External Review Diagnosis and Findings 22-03-2026.docx`
- [D3] `/Users/ethanhan/Downloads/PCB Screen Issues 2/PCB Specification Sheet.docx`
- [D4] `/Users/ethanhan/Downloads/PCB Screen Issues 2/Toffee_External_Review_Pack_Screen_Interconnect.docx`
- [A1] Screenshot artifact: reset-circuit proposal.
- [A2] Screenshot artifact: LCD only came back after MCU button press.
- [A3] Screenshot artifact: confirmation of MCU-button press plus `hardware_screen_reset_guide.pdf`.
- [A4] Screenshot artifact: claim that current reset pin is pulled high plus recommendation to add MCU IO reset or RC reset.
- [A5] Screenshot artifact: RC reset easier for prototype and manual LCD reset plus MCU button can recover.
- [A6] Screenshot artifact: init-sequence concern explicitly described as a guess.
- [W1] Wisevision module spec PDF: [SPEC N085-1212TBWIG41-H12 VER A](https://www.jx-wisevision.com/uploads/SPEC-N085-1212TBWIG41-H12-VER-A-.pdf)
- [W2] GC9107 datasheet mirror: [GC9107 Datasheet V1.2](https://support.midasdisplays.com/wp-content/uploads/2025/06/GC9107.pdf)
- [W3] LCSC listing: [N085-1212TBWIG41-H12](https://www.lcsc.com/product-detail/LCD-Displays-Modules_Shenzhen-Allvision-Tech-N085-1212TBWIG41-H12_C5123573.html)

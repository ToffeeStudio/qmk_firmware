# Lily58 Pro

A refreshed Lily58 PCB with RGB, battery, power switch and encoder support.

* Keyboard Maintainer: [dasky](https://github.com/daskygit)
* Hardware Supported: Controllers using promicro footprint.
* Hardware Availability: https://mechboards.co.uk

Make example for this keyboard (after setting up your build environment):

    qmk compile -kb mechboards/lily58/pro -km default

Flashing example for this keyboard:

    qmk flash -kb mechboards/lily58/pro -km default

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 3 ways:

* **Bootmagic reset**: Hold down the top left key (for left half) or top right key (for right half) when plugging in USB.
* **Physical reset button**: Briefly double press the button below the TRRS socket.
* **Keycode in layout**: Press the key mapped to `QK_BOOT` if it is available

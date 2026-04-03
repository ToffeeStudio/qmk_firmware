# module

![module](imgur.com image replace me!)

*A short description of the keyboard/project*

## Host-side tooling

The actual host-side control code for this keyboard currently lives outside this QMK repo at `~/Desktop/projects/api_test`.
The directory name is misleading, but this is the real host-side project for the Toffee Studio module workflow.

Useful entrypoints in that host-side repo:

* `main.py`: primary host CLI for Raw HID-based commands, including filesystem operations, display/image writes, WPM configuration, and bootloader requests
* `receive_cdc.py`: receives CDC-side file transfers from the keyboard and writes them to disk
* `find_cdc.py`: helper for locating the keyboard's CDC interface by VID/PID/product string
* `README.md`: minimal local setup and example invocation notes for the host tooling

At a high level, the host tooling currently uses two transport paths:

* Raw HID for command-and-control style operations
* USB CDC for streaming file/data transfers

If you are debugging host/firmware interaction for this keyboard, check `~/Desktop/projects/api_test` first rather than assuming the host-side code lives in this QMK tree.

* Keyboard Maintainer: [Kelly Helmut Lord](https://github.com/Kelly Helmut Lord)
* Hardware Supported: *The PCBs, controllers supported*
* Hardware Availability: *Links to where you can find this hardware*

Make example for this keyboard (after setting up your build environment):

    make module:default

Flashing example for this keyboard:

    make module:default:flash

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 3 ways:

* **Bootmagic reset**: Hold down the key at (0,0) in the matrix (usually the top left key or Escape) and plug in the keyboard
* **Physical reset button**: Briefly press the button on the back of the PCB - some may have pads you must short instead
* **Keycode in layout**: Press the key mapped to `QK_BOOT` if it is available

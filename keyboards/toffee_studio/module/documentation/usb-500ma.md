# Firmware option for 500 mA USB ports

Build only, without flashing:

```sh
make toffee_studio/module:via -j1 MODULE_USB_500MA=yes
```

This selects normal profile15 with a **450 mA estimated whole-board limit**.
The existing repaired-PCB-B model reserves its fitted 75 mA baseline, leaving
375 mA for LEDs and 50 mA nominal headroom below a configured 500 mA USB allowance.
It does not sense current or detect host-port capability. Physical current and
affected-port verification remain pending; nominal headroom is not an electrical
guarantee across boards, startup transients or cables.

The final-output limiter, front/badge190 cap, lighting-v3 controls, saved
preferences, 1000 ms dark configured-startup window, 1000 ms fade and both-Shift
rescue remain in use. USB still declares 500 mA after configuration. The option
cannot be combined with unlimited or temporary test modes.

The default build remains normal13/940 mA. Existing release images are unchanged.
The configurator distributes this additional image as `usb-500ma.uf2` and verifies
profile15, its 450 mA budget, 75 mA baseline, zero additional margin and output cap
before its usual uptime/settings verification.

Host regression command, with no hardware access:

```sh
sh keyboards/toffee_studio/module/tools/test_lighting_host.sh
```

It includes profile15 limiter/frame tests and production startup, settings,
parser and rescue checks compiled with `MODULE_USB_500MA`.

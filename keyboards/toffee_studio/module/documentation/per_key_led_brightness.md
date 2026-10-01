# Per-Key Front LED Brightness Control

> **Historical design — status clarified 1 October 2026.** This document
> describes the older 89-LED implementation and mapping. It is not the current
> screenless board map. The current `MODULE_LED_DISCOVERY` build rejects per-key
> brightness command `0x7B`; normal front/badge lighting remains off while rear
> underglow runs. A visible Per-Key LED tab does not mean this feature is
> supported by the flashed firmware. Use the [board README](../readme.md) for
> current status and the verified 104-LED map. Retained below for implementation history.

## Context

This document describes the feature that lets you dim or turn off a **single
front-lighting (per-key) LED** from the web configurator. It covers the LED
architecture of the Module keyboard, the firmware override path, the RAW HID
command, the key→LED mapping, and the web UI.

This was built on the firmware branch `test_led_off` and the
`board_configurator` branch `lighting-fix`.

---

## 1. Background: Two Independent LED Systems

The Module has **89 RGB LEDs** (`RGB_MATRIX_LED_COUNT = 89`, defined in
[`config.h`](../config.h)). They are split into two logically separate groups by
the `flags` array in [`lighting/lighting.c`](../lighting/lighting.c):

| Group | Flag | Driven by | Controlled from |
|-------|------|-----------|-----------------|
| **Front / per-key** | `LED_FLAG_KEYLIGHT` | Stock QMK `rgb_matrix` effects | "Front Lighting" tab (standard VIA protocol) |
| **Underglow** | `LED_FLAG_UNDERGLOW` | Custom animation manager (`animations/manager.c`) | "Underglow" tab (custom `0x71`–`0x7A` HID) |

The custom underglow manager only writes to `LED_FLAG_UNDERGLOW` LEDs; it
explicitly skips everything else. The front/per-key LEDs are rendered by QMK's
built-in `rgb_matrix` effect engine.

Both are flushed to the physical WS2812 string (data pin `GP13`) at the end of
each `rgb_matrix` frame.

### Why this matters

The first implementation of this feature mistakenly targeted the **underglow**
group. The user wanted the **front** LEDs (e.g. turning off the LED under a
specific key). Those are a different group, rendered by a different code path,
so the override had to live in a different place — see §3.

---

## 2. The Render Pipeline & Where We Hook In

Every `rgb_matrix` frame runs in this order:

1. The active `rgb_matrix` **effect renders** all LEDs into the driver's working
   buffer `rgb_matrix_ws2812_array[]` (declared in
   `quantum/rgb_matrix/rgb_matrix_drivers.c`).
2. QMK calls the indicators callback **`rgb_matrix_indicators_user()`**
   ([`module.c`](../module.c)), which calls `lighting_task()`.
3. The buffer is **flushed** to the physical LEDs.

Because the indicators callback runs *after* the effect has rendered but
*before* the flush, it is the correct place to override individual LEDs. This is
the standard QMK pattern for indicator/override LEDs.

```c
// module.c
bool rgb_matrix_indicators_user(void) {
    lighting_task();
    return false;
}
```

`lighting_task()` now does two things:

```c
// lighting/lighting.c
void lighting_task(void) {
    underglow_manager_task();   // paints the underglow LEDs
    keylight_apply_overrides(); // dims any front LED with an override
}
```

---

## 3. Firmware: The Per-LED Override

All of the front-LED override logic lives in
[`lighting/lighting.c`](../lighting/lighting.c).

### 3.1 State

```c
extern rgb_led_t rgb_matrix_ws2812_array[RGB_MATRIX_LED_COUNT];

// 255 = unchanged, 0 = off. One entry per LED.
static uint8_t g_keylight_brightness[RGB_MATRIX_LED_COUNT];
```

`g_keylight_brightness[]` is initialised to all `255` (full brightness, no
effect) in `lighting_init()`.

### 3.2 Applying the override — true dimming via read-back

QMK exposes `rgb_matrix_set_color()` to **write** an LED, but there is **no API
to read** a per-LED color. To support arbitrary dimming (not just on/off), we
read the rendered color straight out of the driver's working buffer
(`rgb_matrix_ws2812_array`) and scale each channel:

```c
static void keylight_apply_overrides(void) {
    for (uint16_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        if (g_keylight_brightness[i] == 255) {
            continue; // full brightness, leave the effect's color untouched
        }
        rgb_led_t c = rgb_matrix_ws2812_array[i];
        rgb_matrix_set_color(i,
            scale8(c.r, g_keylight_brightness[i]),
            scale8(c.g, g_keylight_brightness[i]),
            scale8(c.b, g_keylight_brightness[i]));
    }
}
```

Notes:
- `scale8(x, f)` returns `x * f / 255`, so `f = 0` → off, `f = 128` → ~half,
  `f = 255` → unchanged.
- Reading back the rendered color means the dim **tracks the active effect's
  color** — if the effect cycles the hue, the dimmed LED follows.
- This loop runs over *all* LEDs, but only LEDs with a non-255 override are
  touched, so underglow LEDs are unaffected unless explicitly overridden.
- The read-back assumes a plain **RGB** WS2812 string (no white channel).
  `info.json` declares `"driver": "ws2812"` and there is no RGBW conversion, so
  `.r/.g/.b` are exact.

### 3.3 The setter

```c
void keylight_set_brightness(uint16_t led_index, uint8_t brightness) {
    if (led_index >= RGB_MATRIX_LED_COUNT) {
        return;
    }
    g_keylight_brightness[led_index] = brightness;
}
```

Declared in [`lighting/lighting.h`](../lighting/lighting.h).

---

## 4. The RAW HID Command

A new command carries `(led_index, brightness)` from the host to the firmware.

### 4.1 Command ID

[`rawhid/module_raw_hid.h`](../rawhid/module_raw_hid.h):

```c
id_lighting_set_led_brightness = 0x7B,
```

### 4.2 Parser

[`rawhid/module_raw_hid.c`](../rawhid/module_raw_hid.c):

```c
static int parse_set_led_brightness(uint8_t *data, uint8_t length) {
    if (length < 8) { // Header (6) + LED index (1) + Brightness (1)
        return module_ret_invalid_command;
    }
    uint8_t led_index  = data[6];
    uint8_t brightness = data[7];
    keylight_set_brightness(led_index, brightness);
    return module_ret_success;
}
```

Wired into the dispatch `switch` under `case id_lighting_set_led_brightness:`.
`module_raw_hid.c` now also `#include "lighting/lighting.h"`.

### 4.3 Packet layout

The 32-byte RAW HID packet:

| Offset | Bytes | Field |
|--------|-------|-------|
| 0 | 1 | Magic (`0x09`) |
| 1 | 1 | Command ID (`0x7B`) |
| 2 | 4 | Packet ID (little-endian) |
| 6 | 1 | **LED index** (0–88) |
| 7 | 1 | **Brightness** (0–255) |
| 8 | 24 | Unused |

---

## 5. The Key → LED Mapping

The hardest part is knowing **which LED index belongs to which key**. The
firmware already holds this in `key_logical_map[]`
([`lighting/lighting.c`](../lighting/lighting.c)), which maps
`matrix(row,col) → led_index`. Joining that with the default keymap legends
([`keymaps/default/keymap.c`](../keymaps/default/keymap.c)) gives a key→LED
table, which is hard-coded in the web UI dropdown.

The physical layout is a **standard 65% minus the top-right key**.

### 5.1 Full table

| Key | LED | | Key | LED | | Key | LED |
|-----|----:|-|-----|----:|-|-----|----:|
| Esc | 75 | | Tab | 51 | | Caps Lock | 49 |
| 1 | 76 | | Q | 52 | | **Caps indicator** | **47** |
| 2 | 77 | | W | 53 | | A | 46 |
| 3 | 78 | | E | 54 | | S | 45 |
| 4 | 79 | | R | 55 | | D | 44 |
| 5 | 80 | | T | 56 | | F | 43 |
| 6 | 81 | | Y | 57 | | G | 42 |
| 7 | 82 | | U | 58 | | H | 41 |
| 8 | 83 | | I | 59 | | J | 40 |
| 9 | 84 | | O | 60 | | K | 39 |
| 0 | 85 | | P | 61 | | L | 38 |
| - | 86 | | [ | 62 | | ; | 37 |
| = | 87 | | ] | 63 | | ' | 36 |
| Backspace | 88 | | \ | 64 | | Enter | 35 |
| Fn (top-right) | 88 | | Del | 65 | | PgUp | 34 |

| Key | LED | | Key | LED |
|-----|----:|-|-----|----:|
| L.Shift (left half) | 18 | | L.Ctrl | 17 |
| L.Shift (right half) | 18 | | L.GUI | 15 |
| Z | 19 | | L.Alt | 13 |
| X | 20 | | Space | 10 |
| C | 21 | | R.GUI | 7 |
| V | 22 | | Fn | 6 |
| B | 23 | | Left | 4 |
| N | 24 | | Down | 2 |
| M | 25 | | Right | 0 |
| , | 26 | | | |
| . | 27 | | | |
| / | 28 | | | |
| R.Shift | 29 | | | |
| Up | 30 | | | |
| PgDn | 31 | | | |

### 5.2 Quirks of the mapping

These come from how the hardware is wired (`key_logical_map`), not from the UI:

- **Shared LEDs.** Backspace and the top-right Fn key both map to LED **88**.
  The two left-shift halves both map to LED **18**. Dimming one of a pair dims
  the other.
- **The extra "Caps indicator" LED 47.** The physical Caps-row LED order is
  `49 (Caps) → 47 → 46 (A) → …`. LED **47** sits physically just to the right of
  Caps Lock but is **not assigned to any key** in `key_logical_map`, so it was
  initially invisible in the dropdown. It is flagged `LED_FLAG_UNDERGLOW`, but
  the override mechanism is flag-agnostic (it reads back and scales whatever
  color is in the buffer), so dimming it works the same way. It is exposed in
  the dropdown as **"Caps indicator (right of Caps)"**.
- LED 47 is the **only** "extra" front LED among the keys; every other physical
  row is 1:1 with its keys (accounting for the two shared-LED pairs above).

---

## 6. Web UI

Branch `lighting-fix` of `board_configurator`.

### 6.1 HID method

`src/utils/toffee_studio/hid.ts`:

```ts
MODULE_CMD_SET_LED_BRIGHTNESS = 0x7b,
// ...
public async setLedBrightness(ledIndex: number, brightness: number): Promise<void> {
  const payload = new Uint8Array([ledIndex, brightness]);
  await this.hid.executeCommand(CommandID.MODULE_CMD_SET_LED_BRIGHTNESS, payload);
}
```

### 6.2 The pane

A new **"Per-Key LED"** tab was added to the Lighting pane
(`src/components/panes/toffee-lighting.tsx`), rendering
`src/components/toffee_studio/front-led-menu.tsx`:

- A **key dropdown** (`AccentSelect`) populated from the table in §5.
- A **brightness slider** (`GlowRange`, 0–255). Moving it calls
  `setLedBrightness(selectedKey.value, value)` and sends the `0x7B` packet live.

The component obtains the underlying WebHID device the same way the existing
underglow menu does:
`(keyboardAPI.getHID() as any)._hidDevice._device`.

---

## 7. End-to-End Data Flow

```
Web UI (Per-Key LED tab)
  user picks "Esc" (LED 75), drags slider to 100
        │
        ▼
ToffeeLightingAPI.setLedBrightness(75, 100)
        │  RAW HID packet: 09 7B <pktid> 4B 64 00 ...
        ▼
module_raw_hid.c  parse_set_led_brightness()
        │
        ▼
lighting.c  keylight_set_brightness(75, 100)
        │  g_keylight_brightness[75] = 100
        ▼
(next rgb_matrix frame)
  effect renders LED 75 → rgb_matrix_ws2812_array[75]
        │
        ▼
rgb_matrix_indicators_user() → lighting_task() → keylight_apply_overrides()
        │  scale8(color, 100) → rgb_matrix_set_color(75, ...)
        ▼
buffer flushed → LED 75 shows the effect color at ~39% brightness
```

The override is **volatile** — it lives in RAM and resets to 255 (full) on each
power cycle / reset. It is not persisted to flash.

---

## 8. How to Use

1. Flash the `test_led_off` firmware to the board.
2. Open the configurator (Chrome/Edge — **WebHID** is required; Firefox/Safari
   will not work) and connect the keyboard.
3. Go to **Lighting → Per-Key LED**.
4. Pick a key in the dropdown and drag the brightness slider. `0` turns that
   key's LED fully off.

---

## 9. Extending

- **Add another non-key LED to the dropdown:** find its index in
  `led_physical_map[]`, add an entry to `KEY_LED_OPTIONS` in
  `front-led-menu.tsx`. No firmware change needed (the override is per raw LED
  index).
- **Persist overrides across reboots:** add `g_keylight_brightness[]` to the
  saved config in [`persistence.c`](../persistence.c) and restore it in
  `lighting_init()`. Currently it is intentionally volatile.
- **Read current overrides back to the UI:** add a `get` HID command that
  returns the `g_keylight_brightness[]` array, analogous to
  `id_lighting_get_state`.

---

## 10. File Change Summary

**Firmware (`test_led_off`):**

| File | Change |
|------|--------|
| `lighting/lighting.c` | Override array, `keylight_set_brightness()`, `keylight_apply_overrides()`, init, call in `lighting_task()` |
| `lighting/lighting.h` | Declare `keylight_set_brightness()` |
| `rawhid/module_raw_hid.h` | `id_lighting_set_led_brightness = 0x7B` |
| `rawhid/module_raw_hid.c` | Parser + dispatch + include `lighting/lighting.h` |

**Web (`board_configurator`, `lighting-fix`):**

| File | Change |
|------|--------|
| `src/utils/toffee_studio/hid.ts` | `0x7b` command + `setLedBrightness()` |
| `src/components/toffee_studio/front-led-menu.tsx` | New: key dropdown + slider |
| `src/components/panes/toffee-lighting.tsx` | New "Per-Key LED" tab |

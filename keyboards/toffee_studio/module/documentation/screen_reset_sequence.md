# Screen Reset & Power-On Sequence: Deep Technical Documentation

## Context

This document details exactly how the GC9107 display on the Toffee Studio Module keyboard powers on, resets, initializes, and receives image data. It traces every layer from the user-facing `ui_init()` call down through LVGL, Quantum Painter, ChibiOS SPI, and finally the hardware pins and SPI bus.

---

## 1. Hardware Overview

### MCU & Display

- **MCU:** RP2040 (dual-core ARM Cortex-M0+, 125 MHz system clock)
- **Display Controller:** GC9107 (128x128 pixel TFT LCD)
- **Color Format:** RGB565 (16-bit, 2 bytes per pixel)
- **Interface:** SPI (Mode 0, ~15.625 MHz clock)
- **Display Framework:** QMK Quantum Painter + LVGL

### Pin Map

| Function | Pin | Direction | Purpose |
|----------|-----|-----------|---------|
| SPI Clock (SCK) | GP2 | Output (Alt Func) | Serial clock to GC9107 |
| SPI Data (MOSI) | GP3 | Output (Alt Func) | Serial data to GC9107 |
| Data/Command (DC) | GP1 | GPIO Output | LOW = SPI byte is a command; HIGH = SPI byte is data |
| Backlight (BL) | GP0 | GPIO Output (Push-Pull) | HIGH = backlight on |
| Chip Select (CS) | **Not used** (0xFF) | — | Display CS is likely hardwired LOW (always selected) |
| Reset (RST) | **Not used** (0xFF) | — | No hardware reset line; relies on power-on reset + software init |

### Key Implication: No CS and No RST

- **No CS pin** means the GC9107 is the sole device on SPI0 and is permanently selected. The SPI bus is dedicated to this display.
- **No RST pin** means there is no way to hardware-reset the display after power-on. The initialization sequence relies entirely on software commands sent over SPI. If the display gets into a bad state, only a full power cycle recovers it.

---

## 2. ChibiOS / HAL Configuration

### Files

- `mcuconf.h` — Enables RP2040 SPI0 peripheral: `RP_SPI_USE_SPI0 TRUE`
- `halconf.h` — Enables SPI in ChibiOS HAL: `HAL_USE_SPI TRUE`

### SPI Clock Math

- System clock: 125 MHz
- SPI divisor: 8 (passed to `qp_gc9107_make_spi_device`)
- Effective SPI clock: **125 / 8 = 15.625 MHz**
- SPI Mode 0: CPOL=0, CPHA=0 (clock idle LOW, data sampled on rising edge)
- MSB-first transmission

### ChibiOS SPI Transaction Model

The RP2040 ChibiOS SPI driver (`platforms/chibios/drivers/spi_master.c`) works as:

1. `spi_init()` — Configures GP2 (SCK) and GP3 (MOSI) as SPI alternate-function pins
2. `spi_start(cs_pin, lsb_first, mode, divisor)` — Configures SPI registers (SSPCPSR=divisor, frame=8-bit Motorola), asserts CS LOW (skipped when CS=NO_PIN)
3. `spi_transmit(data, length)` — Sends bytes over MOSI, max 1024 bytes per chunk
4. `spi_stop()` — Deasserts CS HIGH (skipped when CS=NO_PIN)

---

## 3. Full Power-On / Initialization Sequence

### Entry Point: `keyboard_post_init_kb()`

**File:** `module.c:206-213`

```
keyboard_post_init_kb()
  ├── setPinOutputPushPull(0)    // GP0 (backlight) → push-pull output
  ├── writePinHigh(0)            // GP0 → HIGH (backlight ON)
  └── ui_init()                  // Initialize display + LVGL
```

### Step-by-step: `ui_init()`

**File:** `display/ui.c:14-53`

#### Step 1: Create Device Handle

```c
oled = qp_gc9107_make_spi_device(128, 128, 0xFF, OLED_DC_PIN, 0xFF, 8, 0);
```

**What happens internally** (`drivers/painter/gc9xxx/qp_gc9107.c:88-120`):
- Allocates a `tft_panel_dc_reset_painter_device_t` struct from a static pool
- Sets panel dimensions: 128x128
- Sets hardcoded pixel offsets: offset_x=2, offset_y=1 (GC9107 has a larger internal GRAM than the visible area)
- Stores SPI config: CS=0xFF, divisor=8, lsb_first=false, mode=0, DC=GP1, RST=0xFF
- Assigns vtables for driver operations and SPI comms
- Registers device in global device list via `qp_internal_register_device()`

#### Step 2: Initialize Display Hardware

```c
qp_init(oled, QP_ROTATION_180);
```

**What happens internally** (`quantum/painter/qp.c:29-65`):

```
qp_init(oled, QP_ROTATION_180)
  ├── validate_driver_integrity()        // Checks vtable pointers
  ├── qp_comms_init(device)              // → qp_comms_spi_dc_reset_init()
  │     ├── spi_init()                   // Configure GP2/GP3 as SPI pins
  │     ├── setPinOutput(DC_PIN=GP1)     // GP1 → output
  │     ├── writePinLow(GP1)             // GP1 → LOW (command mode default)
  │     ├── [RST pin sequence SKIPPED - RST=0xFF/NO_PIN]
  │     │   // If RST were connected:
  │     │   //   setPinOutput(RST)
  │     │   //   writePinLow(RST)        ← hold RST LOW
  │     │   //   wait_ms(20)             ← 20ms reset pulse
  │     │   //   writePinHigh(RST)       ← release RST
  │     │   //   wait_ms(20)             ← 20ms recovery time
  │     │
  ├── qp_comms_start(device)             // → spi_start() (no CS toggle)
  ├── driver->driver_vtable->init()      // → qp_gc9107_init()
  │     └── [SEE SECTION 4 BELOW]
  └── qp_comms_stop(device)              // → spi_stop() (no CS toggle)
```

#### Step 3: Power On Display

```c
qp_power(oled, true);
```

Sends the `DISPLAY_ON` command (0x29) via SPI. This is technically redundant here since the init sequence already sends DISPLAY_ON, but ensures the display is definitely enabled.

#### Step 4: Attach LVGL

```c
qp_lvgl_attach(oled);
```

**What happens internally** (`quantum/painter/lvgl/qp_lvgl.c:59-126`):

1. Allocates LVGL draw buffer: `128 * 128 / 10 = 1,638 pixels` (~3.2 KB) — this is a partial-screen buffer, LVGL renders in strips
2. Registers display driver with flush callback `qp_lvgl_flush()`
3. Schedules two deferred executors:
   - **Tick task** (1ms interval): calls `lv_tick_inc(1)` — LVGL's internal clock
   - **Handler task** (40ms interval = 25 Hz): calls `lv_timer_handler()` — processes LVGL events, triggers redraws

#### Step 5: Initialize LittleFS Driver for LVGL

```c
lv_fs_littlefs_init();
```

Registers LittleFS as a filesystem driver in LVGL under drive letter `'L'`. Files can be loaded via paths like `L:/image.raw`.

#### Step 6: Load Initial Content

After init, `ui_init()` calls either `ui_display_static_image()` or `ui_display_gradient()` to show the first screen content, based on persisted state.

---

## 4. GC9107 SPI Init Command Sequence (The Actual Bytes on the Wire)

**File:** `drivers/painter/gc9xxx/qp_gc9107.c:19-55`

The init sequence is sent as a bulk command array with format: `[CMD, DELAY_MS, NUM_PARAMS, PARAM1, PARAM2, ...]`

For each entry, the DC pin toggles: **LOW for the command byte, HIGH for parameter bytes.**

### Command-by-Command Breakdown

| # | Command | Hex | Delay | Params | Description |
|---|---------|-----|-------|--------|-------------|
| 1 | `SET_INTER_REG_ENABLE1` | `0xFE` | 5ms | 0 | Unlock inter-register access (bank 1) |
| 2 | `SET_INTER_REG_ENABLE2` | `0xEF` | 5ms | 0 | Unlock inter-register access (bank 2) |
| 3 | `SET_FUNCTION_CTL1` | vendor | 0ms | 1: `ALLOW_SET_VGH_VGL_CLK` | Unlock VGH/VGL/clock configuration |
| 4 | `SET_FUNCTION_CTL2` | vendor | 0ms | 1: `ALLOW_SET_VGH \| ALLOW_SET_VGL` | Unlock voltage rail configuration |
| 5 | `SET_FUNCTION_CTL3` | vendor | 0ms | 1: `ALLOW_SET_GAMMA1 \| ALLOW_SET_GAMMA2` | Unlock gamma curve configuration |
| 6 | `SET_FUNCTION_CTL6` | vendor | 0ms | 1: `ALLOW_SET_COMPLEMENT_RGB \| 0x08 \| ALLOW_SET_FRAMERATE` | Unlock color + framerate config |
| 7 | `SET_COMPLEMENT_RGB` | vendor | 0ms | 1: `COMPLEMENT_WITH_LSB` | Set color complement mode |
| 8 | `SET_VGH` | vendor | 0ms | 1: `0x23` | Set VGH (gate high voltage) = +13V |
| 9 | `SET_VGL` | vendor | 0ms | 1: `0x47` | Set VGL (gate low voltage) = -13V |
| 10 | `SET_VGH_VGL_CLK` | vendor | 0ms | 1: `0x99` | Set voltage rail clock divisors |
| 11 | Unknown cmd | `0xAB` | 0ms | 1: `0x0E` | Undocumented vendor register |
| 12 | `SET_FRAME_RATE` | vendor | 0ms | 1: `0x19` | Set display refresh frame rate |
| 13 | `SET_PIXEL_FORMAT` | `0x3A` | 0ms | 1: `0x05` | **Set pixel format = 16-bit RGB565** |
| 14 | `SET_GAMMA1` | vendor | 0ms | 14 bytes | Positive gamma correction curve |
| 15 | `SET_GAMMA2` | vendor | 0ms | 14 bytes | Negative gamma correction curve |
| 16 | **`SLEEP_OFF`** | **`0x11`** | **120ms** | 0 | **Exit sleep mode — internal DC-DC converters start, display begins accepting data. 120ms mandatory wait for power stabilization.** |
| 17 | **`DISPLAY_ON`** | **`0x29`** | **20ms** | 0 | **Turn on display output — pixels from GRAM are now driven to the panel** |

### Post-Init: Set Rotation via MADCTL

After the bulk sequence, `qp_gc9107_init()` sends:

```
Command 0x36 (MADCTL) with value: BGR | MX | MY
```

- `BGR` — Blue-Green-Red byte order (matches RGB565 with `LV_COLOR_16_SWAP=1`)
- `MX` — Mirror X axis
- `MY` — Mirror Y axis
- Combined MX+MY = 180-degree rotation

---

## 5. How an Image Gets from RAM to the Display

### The Rendering Pipeline

```
Application Layer (ui.c / animation.c)
    │
    ▼
LVGL Scene Graph
    │  lv_img_set_src() → marks widget dirty
    │  lv_obj_invalidate() → schedules redraw
    │
    ▼
LVGL Timer Handler (every 40ms)
    │  lv_timer_handler() → calculates dirty areas
    │  Renders scene into partial draw buffer (~1638 pixels)
    │  Calls flush_cb for each rendered strip
    │
    ▼
qp_lvgl_flush() — [quantum/painter/lvgl/qp_lvgl.c:25-33]
    │  Receives: area coordinates (x1,y1,x2,y2) + pixel buffer
    │
    ├── qp_viewport(device, x1, y1, x2, y2)
    │     │
    │     ▼
    │   qp_tft_panel_viewport() — [drivers/painter/tft_panel/qp_tft_panel.c:35-79]
    │     ├── DC=LOW: Send cmd 0x2A (Set Column Address)
    │     │   DC=HIGH: Send 4 bytes [x1_hi, x1_lo, x2_hi, x2_lo]
    │     │   (x values offset by +2 for GC9107 panel offset)
    │     │
    │     ├── DC=LOW: Send cmd 0x2B (Set Row Address)
    │     │   DC=HIGH: Send 4 bytes [y1_hi, y1_lo, y2_hi, y2_lo]
    │     │   (y values offset by +1 for GC9107 panel offset)
    │     │
    │     └── DC=LOW: Send cmd 0x2C (Memory Write)
    │         // GC9107 is now ready to accept pixel data
    │
    ├── qp_pixdata(device, color_buffer, num_pixels)
    │     │
    │     ▼
    │   qp_tft_panel_pixdata() — [drivers/painter/tft_panel/qp_tft_panel.c:82-86]
    │     └── DC=HIGH: Send pixel_count * 2 bytes of RGB565 data
    │         // Sent in 1024-byte chunks via spi_transmit()
    │
    ├── qp_flush(device)      // No-op for streaming displays
    │
    └── lv_disp_flush_ready() // Tell LVGL this strip is done
```

### SPI Wire-Level Detail for One Pixel Strip

For a 128-wide, 13-row strip (one LVGL render pass):

```
[DC=LOW]  TX: 0x2A                          // Set Column Address command
[DC=HIGH] TX: 0x00 0x02 0x00 0x81           // Columns 2-129 (offset +2)
[DC=LOW]  TX: 0x2B                          // Set Row Address command
[DC=HIGH] TX: 0x00 0x01 0x00 0x0D           // Rows 1-13 (offset +1)
[DC=LOW]  TX: 0x2C                          // Memory Write command
[DC=HIGH] TX: [128 * 13 * 2 = 3328 bytes]   // RGB565 pixel data
           // Sent as 3 chunks: 1024 + 1024 + 1024 + 256
```

### Full-Screen Image: Total Bytes

- 128 x 128 pixels x 2 bytes = **32,768 bytes** of pixel data
- Plus ~15 bytes of command overhead per strip
- At 15.625 MHz SPI clock: 32,768 bytes / (15,625,000 / 8) = ~16.8ms for raw pixel data
- With LVGL strip rendering (10 passes): ~10 x (command overhead + strip data) ≈ **17-20ms total**

---

## 6. Animation System (Double-Buffered Frame Streaming)

**File:** `display/animation.c`

### Architecture

```
Background Thread (frame loader)          LVGL Timer (every ~83ms at 12 FPS)
    │                                          │
    │  Reads next frame from LittleFS          │  Swaps buffer pointers
    │  into frame_buffers[next_buffer]         │  lv_img_set_src(new_buffer)
    │  (32,768 bytes per frame)                │  lv_obj_invalidate()
    │                                          │
    └──────── double buffer ──────────────────►│
              frame_buffers[0][32768]
              frame_buffers[1][32768]
```

### Frame Buffer Details

- Two static buffers: `frame_buffers[2][128 * 128 * 2]` = 2 x 32 KB = **64 KB total**
- `current_buffer` index: the buffer currently being displayed
- `next_buffer` index: the buffer being loaded by the background thread
- On each timer tick: swap indices, update LVGL image source, invalidate widget
- Background thread immediately starts loading next frame into the now-free buffer

### Image Descriptors

```c
lv_img_dsc_t images[2] = {
    { .header.w=128, .header.h=128, .data_size=32768,
      .header.cf=LV_IMG_CF_TRUE_COLOR, .data=frame_buffers[0] },
    { .header.w=128, .header.h=128, .data_size=32768,
      .header.cf=LV_IMG_CF_TRUE_COLOR, .data=frame_buffers[1] },
};
```

---

## 7. LVGL Configuration Details

**File:** `lv_conf.h`

| Setting | Value | Meaning |
|---------|-------|---------|
| `LV_COLOR_DEPTH` | 16 | RGB565 color mode |
| `LV_COLOR_16_SWAP` | 1 | Swap high/low bytes of RGB565 (required for GC9107's byte order) |
| `LV_MEM_SIZE` | 32KB | LVGL's internal heap for widgets, styles, etc. |
| `LV_USE_IMG` | 1 | Image widget enabled |
| `LV_USE_GIF` | 1 | GIF decoder enabled |
| `LV_USE_FS_LITTLEFS` | 1 | LittleFS filesystem driver enabled |
| `LV_FS_LITTLEFS_LETTER` | 'L' | Drive letter for LittleFS |

---

## 8. "Screen Reset" Summary: What Actually Happens

When power is applied or the keyboard boots:

### Timeline

| Time | Event | Pins Affected |
|------|-------|---------------|
| T+0ms | RP2040 boots, runs ChibiOS startup | — |
| T+~Xms | `keyboard_post_init_kb()` called | — |
| T+~Xms | `setPinOutputPushPull(GP0)` | GP0 configured as output |
| T+~Xms | `writePinHigh(GP0)` | **GP0 → HIGH: Backlight turns ON** |
| T+~Xms | `spi_init()` | GP2 (SCK), GP3 (MOSI) configured as SPI |
| T+~Xms | `setPinOutput(GP1)` + `writePinLow(GP1)` | **GP1 (DC) → LOW** |
| T+~Xms | SPI commands 1-12 sent | GP1 toggles LOW/HIGH per cmd/data |
| T+~Xms | **cmd 0x3A (pixel format = RGB565)** | GP1: LOW then HIGH |
| T+~Xms | Gamma curves sent (commands 14-15) | GP1 toggles |
| T+~Xms | **cmd 0x11 (SLEEP OFF)** | GP1 → LOW, 1 byte sent |
| T+120ms | **120ms mandatory wait** (DC-DC converters stabilize) | — |
| T+120ms | **cmd 0x29 (DISPLAY ON)** | GP1 → LOW, 1 byte sent |
| T+140ms | **20ms wait** | — |
| T+140ms | MADCTL rotation set (cmd 0x36) | GP1 toggles |
| T+~141ms | LVGL attached, draw buffer allocated | — |
| T+~141ms | LittleFS driver registered | — |
| T+~142ms | First image/gradient loaded and rendered | GP1 toggles rapidly as pixel strips are sent |
| T+~160ms | **Display fully visible with content** | — |

### Pins Active During Normal Operation

After initialization, during normal rendering:

- **GP0 (BL):** Stays HIGH (backlight on, never toggled unless display goes to sleep)
- **GP1 (DC):** Rapidly toggles between LOW (commands: viewport setup) and HIGH (data: pixel streaming) — this is the most active pin
- **GP2 (SCK):** 15.625 MHz clock during SPI transfers, idle LOW between transfers
- **GP3 (MOSI):** Data line, carries command bytes and pixel data synchronized to SCK

---

## 9. Software Architecture Stack

```
┌─────────────────────────────────────────────────┐
│  Application Layer                               │
│  ui.c, animation.c, wpm_indicator.c              │
│  - Creates LVGL widgets                          │
│  - Loads images from LittleFS                    │
│  - Double-buffer animation frame swap            │
├─────────────────────────────────────────────────┤
│  LVGL (v8.x)                                     │
│  - Scene graph & dirty-area tracking             │
│  - Partial rendering into ~3.2 KB draw buffer    │
│  - Calls flush_cb per rendered strip             │
├─────────────────────────────────────────────────┤
│  Quantum Painter LVGL Bridge                     │
│  qp_lvgl.c                                       │
│  - qp_lvgl_flush(): viewport → pixdata → flush  │
│  - 1ms tick task + 40ms handler task             │
├─────────────────────────────────────────────────┤
│  Quantum Painter Core                            │
│  qp.c, qp_internal.c                            │
│  - Device registry, init/power lifecycle         │
│  - Delegates to driver vtable                    │
├─────────────────────────────────────────────────┤
│  GC9107 TFT Panel Driver                         │
│  qp_gc9107.c, qp_tft_panel.c                    │
│  - Init command sequence (vendor registers)      │
│  - Viewport: 0x2A/0x2B/0x2C commands             │
│  - Pixdata: streams RGB565 bytes                 │
├─────────────────────────────────────────────────┤
│  SPI Comms Layer (with DC/RST)                   │
│  qp_comms_spi.c                                  │
│  - DC pin toggle for cmd vs data                 │
│  - RST pin pulse (if connected)                  │
│  - 1024-byte chunked transmission                │
├─────────────────────────────────────────────────┤
│  ChibiOS SPI Master Driver                       │
│  platforms/chibios/drivers/spi_master.c          │
│  - RP2040 SPI0 peripheral registers              │
│  - GPIO alternate function configuration         │
├─────────────────────────────────────────────────┤
│  Hardware                                        │
│  RP2040 SPI0 → GC9107 128x128 TFT               │
│  GP0=BL, GP1=DC, GP2=SCK, GP3=MOSI              │
└─────────────────────────────────────────────────┘
```

---

## Critical Files Referenced

| File | Role |
|------|------|
| `module.c` | Entry point: backlight enable + `ui_init()` |
| `config.h` | Pin definitions, SPI config, QP settings |
| `rules.mk` | Feature flags: QP, LVGL, GC9107 driver |
| `halconf.h` | ChibiOS HAL: SPI enable |
| `mcuconf.h` | RP2040: SPI0 enable |
| `lv_conf.h` | LVGL: color depth, memory, filesystem |
| `display/ui.c` | Display init, LVGL attach, image loading |
| `display/animation.c` | Double-buffered frame animation |
| `drivers/painter/gc9xxx/qp_gc9107.c` | GC9107 driver: init sequence, factory |
| `drivers/painter/tft_panel/qp_tft_panel.c` | Viewport + pixel streaming |
| `drivers/painter/comms/qp_comms_spi.c` | SPI DC/RST comms layer |
| `quantum/painter/qp.c` | Quantum Painter core: init lifecycle |
| `quantum/painter/lvgl/qp_lvgl.c` | LVGL↔QP bridge: flush callback |

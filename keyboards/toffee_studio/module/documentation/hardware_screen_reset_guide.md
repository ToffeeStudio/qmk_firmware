# GC9107 Screen Reset & Power-On: Hardware Reference

**Audience:** PCB/hardware designers working with the Toffee Studio Module

---

## IPS Display Module Architecture

![IPS Screen DB schematic](image.png)

The display is not directly soldered to the main PCB. Instead, it lives on a small **daughter board** ("IPS Screen DB") that connects to the RP2040 main board via a 6-pin ribbon cable (J8, 0.64mm pitch).

The daughter board carries two key components:

**J9 (S085TBWIG41) — The display module itself.** This is the 12-pin IPS LCD with an integrated GC9107 driver IC. Its pinout:

| Pin | Name | Function |
|-----|------|----------|
| 1, 5, 6, 12 | GND | Ground |
| 2 | LED Cathode | Backlight LED return path |
| 3 | LED Anode | Backlight LED supply, fed through R11 (33 Ohm current-limiting resistor) |
| 4 | VDD | +3V3 power to the GC9107 IC |
| 7 | D/C | Data/command select |
| 8 | Chip Select | CS pin (directly on the module) |
| 9 | SPI SCLK | SPI clock input |
| 10 | MISO/MOSI | SPI data input |
| 11 | RESET | RESX pin — currently pulled HIGH to +3V3 via R13 (1k resistor), not connected to any MCU GPIO |

**Q2 (DMG1012T) — Backlight MOSFET.** A P-channel MOSFET that switches the backlight LED current. The RP2040's "Brightness" signal (GP0) controls the gate, allowing PWM dimming.

The ribbon cable (J8) carries only 4 signals from the RP2040: Brightness, D/C, SCLK, and MOSI — plus power and ground. The RESET and CS pins are managed locally on the daughter board (RESET pulled high, CS directly on the module).

---

## What the MCU Connects To

The RP2040 communicates with the GC9107 display controller over 4 wires:

| Wire | RP2040 Pin | Direction | Function |
|------|-----------|-----------|----------|
| SCK  | GP2       | MCU → Display | SPI clock signal (~15.6 MHz) |
| MOSI | GP3       | MCU → Display | SPI data line (commands + pixel data) |
| DC   | GP1       | MCU → Display | Tells the GC9107 whether the current SPI byte is a **command** (DC=LOW) or **data** (DC=HIGH) |
| BL   | GP0       | MCU → Display | Backlight enable (HIGH = on) |

**Currently unused:**
- **CS (Chip Select):** Not connected. The GC9107 is the only device on SPI0, so it should be hardwired LOW (always selected) or left managed by the module.
- **RESX (Hardware Reset):** Not connected to any GPIO. The GC9107 IC does have this pin — it is active-low and forces a full hardware reset when pulled LOW. The exact minimum pulse width is not specified in available documentation; the firmware uses a conservative 20ms pulse. Currently, the firmware relies on power-on reset only.

---

## What Happens When the Keyboard Boots

### Step 1: Backlight On

The MCU sets GP0 HIGH. This powers the LED backlight behind the LCD panel. The display itself is not yet initialized — the GC9107 is in sleep mode after power-on, so its internal charge pump is off and the panel is not being driven. On this IPS panel, the undriven state appears as a dark/black screen illuminated by the backlight.

### Step 2: SPI Bus Configured

GP2 (SCK) and GP3 (MOSI) are configured as SPI outputs. GP1 (DC) is set as a regular GPIO output, initially LOW.

### Step 3: GC9107 Initialization Commands

The MCU sends a series of SPI commands to configure the GC9107's internal registers. For each command:
- MCU pulls **DC LOW**, then clocks one byte over MOSI → GC9107 interprets this as a command
- MCU pulls **DC HIGH**, then clocks parameter bytes over MOSI → GC9107 interprets these as data for that command

The key commands in order:

1. **Unlock internal registers** (vendor-specific) — allows writing to voltage and gamma settings
2. **Set gate voltages** — VGH = +13V, VGL = -13V (these are generated internally by the GC9107's built-in charge pump)
3. **Set pixel format** — 16-bit color (RGB565, 2 bytes per pixel)
4. **Set gamma curves** — tunes the brightness response of the panel
5. **SLEEP OFF (0x11)** — **This is the critical power-on command.** It starts the GC9107's internal DC-DC converter, which generates the high voltages needed to actually drive the liquid crystal pixels. The MCU must wait **120ms** after this command for the power supply to stabilize.
6. **DISPLAY ON (0x29)** — Tells the GC9107 to start outputting pixel data from its internal memory (GRAM) to the LCD panel. The MCU waits **20ms**.
7. **Set rotation** — Configures which direction the pixels are scanned (180-degree rotation in our case).

After this, the display is fully alive and accepting pixel data.

### Step 4: Pixel Data Transfer

To draw an image, the MCU sends:
1. A command (DC=LOW) to set the column range (which horizontal pixels to write)
2. A command (DC=LOW) to set the row range (which vertical pixels to write)
3. A command (DC=LOW) to begin a memory write
4. Raw pixel data (DC=HIGH) — 2 bytes per pixel in RGB565 format, streamed continuously

For a full 128x128 screen: 128 x 128 x 2 = **32,768 bytes** of pixel data.

**Note on GRAM addressing:** The GC9107 internally has a 128x160 pixel GRAM (larger than the 128x128 visible panel). The firmware applies column offset +2 and row offset +1 when setting the drawing window, so pixel (0,0) on screen maps to GRAM address (2,1). This is handled in firmware and is invisible on the SPI bus — but it means the column/row address bytes sent over SPI will be offset from zero.

---

## What "Reset" Means

There are two ways to reset the GC9107:

### Software Reset (current approach)
The MCU re-sends the full initialization command sequence over SPI (steps 3-4 above). This works as long as the GC9107 is correctly receiving SPI data.

### Hardware Reset (via RESX pin)

Pulling the RESX pin LOW forces the GC9107 into a known default state at the electrical level, regardless of its internal state. The firmware uses a 20ms LOW pulse followed by a 20ms recovery. This is useful if:
- The SPI bus gets out of sync (e.g., after a firmware crash mid-transfer)
- The GC9107 stops responding to commands
- You need a guaranteed recovery without power-cycling the whole board

**If RESX were connected to an RP2040 GPIO, the reset sequence would be:**
1. Pull RESX LOW → hold for 20ms
2. Release RESX HIGH → wait 20ms
3. Then send the full SPI init command sequence as normal

---

## Summary for PCB Design

- 4 signal wires required: SCK (GP2), MOSI (GP3), DC (GP1), BL (GP0)
- RESX is optional but recommended — connect it to a spare GPIO for robustness
- CS can be hardwired LOW if the display is the only SPI device
- No MISO needed (write-only communication)
- SPI clock: ~15.6 MHz, Mode 0 (CPOL=0, CPHA=0)

---
---

# GC9107 屏幕复位与上电：硬件参考文档

**目标读者：** 与 Toffee Studio Module 合作的 PCB/硬件设计师

---

## IPS 显示模块架构

![IPS 屏幕子板原理图](image.png)

屏幕并非直接焊接在主 PCB 上，而是安装在一块小型**子板**（"IPS Screen DB"）上，通过 6 芯排线（J8，0.64mm 间距）连接到 RP2040 主板。

子板上有两个关键元器件：

**J9（S085TBWIG41）—— 显示模块本体。** 这是一个带有集成 GC9107 驱动 IC 的 12 引脚 IPS LCD。引脚定义如下：

| 引脚 | 名称 | 功能 |
|------|------|------|
| 1, 5, 6, 12 | GND | 接地 |
| 2 | LED Cathode | 背光 LED 回流路径 |
| 3 | LED Anode | 背光 LED 供电，经 R11（33 Ohm 限流电阻）供给 |
| 4 | VDD | +3V3 为 GC9107 IC 供电 |
| 7 | D/C | 数据/指令选择 |
| 8 | Chip Select | CS 引脚（位于模块上） |
| 9 | SPI SCLK | SPI 时钟输入 |
| 10 | MISO/MOSI | SPI 数据输入 |
| 11 | RESET | RESX 引脚——目前通过 R13（1k 电阻）上拉至 +3V3，未连接到任何 MCU GPIO |

**Q2（DMG1012T）—— 背光 MOSFET。** P 沟道 MOSFET，用于开关背光 LED 电流。RP2040 的 "Brightness" 信号（GP0）控制其栅极，可实现 PWM 调光。

排线（J8）仅从 RP2040 传输 4 个信号：Brightness、D/C、SCLK 和 MOSI——外加电源和接地。RESET 和 CS 引脚在子板上本地管理（RESET 上拉为高电平，CS 直接接在模块上）。

---

## MCU 与屏幕的连接

RP2040 通过 4 根线与 GC9107 显示驱动芯片通信：

| 信号线 | RP2040 引脚 | 方向 | 功能 |
|--------|-----------|------|------|
| SCK    | GP2       | MCU → 屏幕 | SPI 时钟信号（约 15.6 MHz） |
| MOSI   | GP3       | MCU → 屏幕 | SPI 数据线（指令 + 像素数据） |
| DC     | GP1       | MCU → 屏幕 | 告诉 GC9107 当前 SPI 字节是**指令**（DC=低电平）还是**数据**（DC=高电平） |
| BL     | GP0       | MCU → 屏幕 | 背光使能（高电平 = 开启） |

**目前未使用：**
- **CS（片选）：** 未连接。GC9107 是 SPI0 上唯一的设备，因此应将 CS 硬接低电平（始终选中），或由模块自行管理。
- **RESX（硬件复位）：** 未连接到任何 GPIO。GC9107 芯片本身确实有此引脚——低电平有效，拉低可强制芯片完全硬件复位。现有文档中未明确最小脉冲宽度；固件使用保守的 20ms 脉冲。目前固件仅依赖上电复位。

---

## 键盘启动时的流程

### 第 1 步：背光开启

MCU 将 GP0 设为高电平，点亮 LCD 面板后面的 LED 背光。此时屏幕本身尚未初始化——GC9107 上电后处于睡眠模式，内部电荷泵未启动，面板未被驱动。此 IPS 面板在未驱动状态下呈现黑色，背光照亮的是一块暗屏。

### 第 2 步：SPI 总线配置

GP2（SCK）和 GP3（MOSI）配置为 SPI 输出。GP1（DC）配置为普通 GPIO 输出，初始为低电平。

### 第 3 步：GC9107 初始化指令序列

MCU 通过 SPI 发送一系列指令来配置 GC9107 的内部寄存器。对于每条指令：
- MCU 将 **DC 拉低**，然后通过 MOSI 时钟发送一个字节 → GC9107 将其解释为指令
- MCU 将 **DC 拉高**，然后通过 MOSI 时钟发送参数字节 → GC9107 将其解释为该指令的数据

关键指令按顺序如下：

1. **解锁内部寄存器**（厂商专用指令）——允许写入电压和伽马设置
2. **设置栅极电压** —— VGH = +13V，VGL = -13V（由 GC9107 内置电荷泵生成）
3. **设置像素格式** —— 16 位颜色（RGB565，每像素 2 字节）
4. **设置伽马曲线** —— 调节面板的亮度响应
5. **退出睡眠模式（0x11）** —— **这是关键的上电指令。** 它启动 GC9107 内部的 DC-DC 转换器，产生驱动液晶像素所需的高电压。发送此指令后，MCU 必须等待 **120ms**，让电源稳定。
6. **开启显示（0x29）** —— 告诉 GC9107 开始将内部显存（GRAM）中的像素数据输出到 LCD 面板。MCU 等待 **20ms**。
7. **设置旋转方向** —— 配置像素扫描方向（本设计中为 180 度旋转）。

此后，屏幕完全就绪，可以接收像素数据。

### 第 4 步：像素数据传输

要绘制图像，MCU 发送：
1. 一条指令（DC=低电平）设置列范围（要写入的水平像素区间）
2. 一条指令（DC=低电平）设置行范围（要写入的垂直像素区间）
3. 一条指令（DC=低电平）开始显存写入
4. 原始像素数据（DC=高电平）—— 每像素 2 字节，RGB565 格式，连续传输

整屏 128x128：128 x 128 x 2 = **32,768 字节**像素数据。

**关于 GRAM 寻址：** GC9107 内部 GRAM 为 128x160 像素（大于可见面板的 128x128）。固件在设置绘图窗口时施加列偏移 +2 和行偏移 +1，因此屏幕像素 (0,0) 映射到 GRAM 地址 (2,1)。此偏移在固件中处理，在 SPI 总线上表现为列/行地址字节并非从零开始。

---

## "复位"的含义

复位 GC9107 有两种方式：

### 软件复位（当前方案）
MCU 通过 SPI 重新发送完整的初始化指令序列（上述第 3-4 步）。只要 GC9107 能正常接收 SPI 数据，此方法即可正常工作。

### 硬件复位（通过 RESX 引脚）
将 RESX 引脚拉低，可在电气层面强制 GC9107 回到已知的默认状态，无论其内部处于何种状态。固件使用 20ms 低电平脉冲加 20ms 恢复时间。适用场景：
- SPI 总线失步（例如固件在传输过程中崩溃）
- GC9107 停止响应指令
- 需要在不对整板断电的情况下保证恢复

**若 RESX 连接到 RP2040 的某个 GPIO，复位流程为：**
1. 将 RESX 拉低 → 保持 20ms
2. 释放 RESX 为高电平 → 等待 20ms
3. 然后按正常流程发送完整的 SPI 初始化指令序列

---

## PCB 设计要点

- 需要 4 根信号线：SCK（GP2）、MOSI（GP3）、DC（GP1）、BL（GP0）
- RESX 可选但建议连接——接到一个空闲 GPIO 可提高可靠性
- 若屏幕是唯一的 SPI 设备，CS 可硬接低电平
- 无需 MISO（仅写通信）
- SPI 时钟：约 15.6 MHz，模式 0（CPOL=0，CPHA=0）

# Pico USB Keyboard to UART Terminal

[English_version_readme](./README_EN.md)

一个基于 **RP2040 / Raspberry Pi Pico / RP2040-Zero** 的 USB 键盘转 UART 终端输入器。

它可以让普通 USB 键盘直接变成单片机/嵌入式设备可用的串口输入设备：

```text
USB Keyboard
    ↓
RP2040 Pico / RP2040-Zero 作为 USB Host
    ↓
UART TX 输出终端字节流
    ↓
STC / STM32 / PY32 / ESP32 / MicroPython REPL / 自制终端
```

本项目希望为低成本单片机、OLED 文本界面、MicroPython REPL、串口 Shell、SSH 物理终端等场景提供一个简单、便宜且易于修改的 USB 键盘输入方案，在一些应用中甚至可以替代 CH9350 一类专用芯片。对于爱好者项目来说，自行设计矩阵键盘往往既占用 IO 口，又需要花费不少精力处理扫描、消抖和键位布局；而现成 USB 键盘价格低、选择多、布局完整，几乎是最容易获得、最适合嵌入式文本交互的输入设备。

---

## 功能特性

- 使用 RP2040 的 USB Host 功能读取 USB 键盘输入。
- 使用 TinyUSB Host HID 处理键盘 report。
- 通过 UART 输出标准“终端字节流”。
- 支持常用字符输入：
  - 小写字母；
  - 大写字母；
  - 数字；
  - 常用编程符号，如 `! @ # $ % ^ & * ( ) _ + { } | : " < > ? ~`。
- 支持常用控制键：
  - Enter；
  - Backspace；
  - Delete；
  - Tab；
  - Esc；
  - 上下左右方向键；
  - Home / End；
  - PageUp / PageDown。
- 支持常见 Ctrl 组合键：
  - Ctrl+A 到 Ctrl+Z；
  - Ctrl+C 可用于 MicroPython REPL / Shell 中断；
  - Ctrl+D 可用于 EOF / 退出；
  - Ctrl+L 可用于清屏/刷新。
- 支持按键长按自动重复。
- 提供多个 UART 波特率版本：
  - 115200；
  - 38400；
  - 9600。
- 提供 CH9350-like raw 模式：
  - 输出 `57 AB 01 + 8-byte USB boot keyboard report`。
- 针对低功耗做了优化：
  - 主频降至 48 MHz；
  - 降低核心电压；
  - 主循环加入短 sleep；
  - 未使用 GPIO 设置为高阻低功耗状态；
  - 关闭无用 stdio 输出。
- 适合 RP2040-Zero：
  - 默认使用 `GP0 / GP1` 作为 UART0；
  - 更方便插面包板；
  - 实际只需要连接一根 TX 线即可输出键盘数据。

---

## 默认 UART 接线

默认使用 UART0：

| RP2040 引脚 | 功能 | 连接到 |
|---|---|---|
| GP0 | UART0 TX | 目标 MCU / USB-TTL 的 RX |
| GP1 | UART0 RX | 可选，暂时不用 |
| GND | Ground | 目标设备 GND |

默认波特率根据固件版本不同而不同，例如：

```text
115200 8N1
38400 8N1
9600 8N1
```

如果只是做“USB 键盘 → UART 输出”，只需要：

```text
GP0 TX → 目标设备 RX
GND    → 目标设备 GND
```

也就是说，**一根 TX 线 + 一根 GND 就可以工作**。

---

## USB 键盘连接方式

RP2040 在本项目中作为 **USB Host**，因此 USB-C / Micro-USB 口不再是普通的 USB Device 口，而是用来接 USB 键盘。

### 方式 1：使用带外接供电的 USB Hub

推荐方式：

```text
外接供电 USB Hub
    ├── USB 键盘
    └── Pico / RP2040-Zero 的 USB-C 口
```

这种方式最方便测试。USB Hub 提供键盘所需的 5V 供电，Pico 负责 USB Host 通信。

注意：

- 某些 Hub 不会向上游口反向供电，这种情况下 Pico 仍需要单独供电。
- 某些廉价 Hub 反向供电设计不规范，使用时请注意电源冲突。
- 如果键盘不能枚举，优先检查 Hub 供电、线缆和接口方向。

<img width="2275" height="1279" alt="a791742bd743eff702621a6622608542" src="https://github.com/user-attachments/assets/b00f8c58-15f8-4a2f-8f9d-1bdbd1874fde" />

### 方式 2：板子 5V/VBUS 外接供电，USB-C 接键盘

理论上可行，但本项目尚未充分测试：

```text
5V 电源 → RP2040 板子的 5V / VBUS
USB-C 口 → USB 键盘
```

这种方式相当于让 RP2040 板子给 USB 键盘提供 VBUS，同时自己作为 Host 与键盘通信。

注意：

- 不要把 5V 接到 3.3V 引脚。
- 需要确认你的 RP2040 板子 USB VBUS 走线和供电方式。
- 不同 RP2040-Zero / Pico 兼容板电源设计可能不一样。
- 如果不确定，优先使用“带外接供电的 USB Hub”方案。

---

## 输出协议

默认 `TERM` 模式输出标准终端字节流。

### 普通字符

普通字符直接输出 ASCII，例如：

| 键盘输入 | UART 输出 |
|---|---|
| `a` | `0x61` |
| `A` | `0x41` |
| `1` | `0x31` |
| `!` | `0x21` |
| Space | `0x20` |

默认使用 US 键盘布局。

### 控制键

| 按键 | UART 输出 |
|---|---|
| Enter | `\r` |
| Backspace | `0x08` |
| Delete | `ESC [ 3 ~` |
| ↑ | `ESC [ A` |
| ↓ | `ESC [ B` |
| → | `ESC [ C` |
| ← | `ESC [ D` |
| Home | `ESC [ H` |
| End | `ESC [ F` |
| PageUp | `ESC [ 5 ~` |
| PageDown | `ESC [ 6 ~` |
| Esc | `0x1B` |
| Tab | `0x09` |

### Ctrl 组合键

| 快捷键 | UART 输出 |
|---|---|
| Ctrl+A | `0x01` |
| Ctrl+B | `0x02` |
| Ctrl+C | `0x03` |
| Ctrl+D | `0x04` |
| ... | ... |
| Ctrl+Z | `0x1A` |

这使它可以直接用于：

- MicroPython REPL；
- 串口 Shell；
- 简易文本编辑器；
- 自制 OLED 终端；
- SSH / Telnet 物理终端前端。

---

## 固件版本

可以直接在 [GitHub Release 页面](../../releases/latest)下载已经编译好的 `.uf2` 固件，也可以根据自己的需求修改源码后自行编译。

工程默认会生成多个 UF2 文件，例如：

```text
pico_usb_kbd_uart_lp48_term_115200.uf2
pico_usb_kbd_uart_lp48_term_38400.uf2
pico_usb_kbd_uart_lp48_term_9600.uf2
pico_usb_kbd_uart_lp48_ch9350raw_115200.uf2
pico_usb_kbd_uart_lp24_term_115200_exp.uf2
```

推荐使用：

```text
pico_usb_kbd_uart_lp48_term_115200.uf2
```

这是 48 MHz 低功耗终端模式，实测可以正常工作。

如果不希望使用专门的低功耗降频版本，也可以使用：

```text
pico_usb_kbd_uart_lp48_term_115200.uf2
```
这是普通 115200 波特率终端模式版本，未专门降低系统主频，适合用于初次测试、兼容性验证或对功耗不敏感的场景。

### 24 MHz 实验版

`lp24` 版本进一步降低主频，实测电流更低，但 USB Host 功能可能异常，因此只作为实验版本保留。

---

## 功耗测试

在当前测试条件下：

| 版本 | 电流 | 状态 |
|---|---:|---|
| 默认高主频版本 | 约 31 mA | 正常 |
| 48 MHz 低功耗版本 | 约 16 mA | 正常 |
| 24 MHz 实验版本 | 约 12.5 mA | 功能异常 |

因此目前推荐使用：

```text
48 MHz 低功耗版本
```

它在功耗和稳定性之间比较均衡。

---

## 编译方式

### 依赖

需要：

- Pico SDK；
- TinyUSB；
- CMake；
- arm-none-eabi-gcc；
- picotool，可选但推荐。

Ubuntu / Debian 示例：

```bash
sudo apt update
sudo apt install -y cmake build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi
```

### 设置 Pico SDK

如果已经安装 Pico SDK：

```bash
export PICO_SDK_PATH=$HOME/pico/pico-sdk
```

如果安装了 picotool：

```bash
export picotool_DIR=$HOME/pico/picotool-install/picotool
```

### 编译

```bash
mkdir build
cd build

cmake \
  -DPICO_SDK_PATH=$HOME/pico/pico-sdk \
  -Dpicotool_DIR=$HOME/pico/picotool-install/picotool \
  ..

cmake --build . -j$(nproc)
```

编译完成后，在 `build/` 目录下可以找到 `.uf2` 文件。

---

## 烧录方式

按住 Pico / RP2040-Zero 的 BOOTSEL 按钮，然后插入电脑。

电脑会出现一个名为 `RPI-RP2` 的 U 盘。

将对应的 `.uf2` 文件拖进去即可。

例如：

```text
pico_usb_kbd_uart_lp48_term_115200.uf2
```

烧录后，RP2040 会自动重启并开始作为 USB Host 工作。

最简单的测试方法是使用 USB-TTL 模块在电脑上查看 Pico 的 UART 输出：

1. 烧录固件后，重新给 Pico 上电或按复位键重启。
2. 将 USB 键盘接入 Pico 的 USB 口或外接供电 USB Hub。
3. 使用 USB-TTL 模块连接 Pico 的 UART 输出：

```text
Pico GP0 (UART0 TX)  →  USB-TTL RX
Pico GND             →  USB-TTL GND
```

4.在电脑上打开串口监视器，选择对应串口，并设置为固件对应的波特率。如：115200 8N1。
5.在 USB 键盘上输入字符。如果连接和供电正常，串口监视器中应能看到对应的字符、控制字符或 ANSI 转义序列。

注意：Pico 与 USB-TTL / 目标单片机必须共地。通常只需要连接 Pico 的 GP0(TX) 到接收端 RX，不需要连接 GP1(RX)。

---

## 使用示例


### 示例 1：连接 STC / PY32 / STM32 文本界面

```text
USB Keyboard
    ↓
Pico USB Host
    ↓ GP0 UART TX
STC / PY32 / STM32 RX
    ↓
OLED 文本窗口 / 菜单 / 编辑器
```

### 示例 2：连接 ESP32 MicroPython REPL

```text
USB Keyboard
    ↓
Pico
    ↓ UART
ESP32 MicroPython REPL
```

支持：

- 字符输入；
- 回车执行；
- Backspace 删除；
- Ctrl+C 中断；
- Ctrl+D EOF / 软复位相关操作；
- 方向键历史命令，取决于 REPL 支持情况。

### 示例 3：作为 SSH 物理终端输入

```text
USB Keyboard
    ↓
Pico
    ↓ UART
ESP32 / Linux SBC / 自制终端
    ↓
SSH / Telnet / Shell
```

---

## CH9350-like Raw 模式

除了终端模式，本项目还提供一个 CH9350-like raw 输出模式。

输出格式：

```text
57 AB 01 + 8-byte USB boot keyboard report
```

例如按键 report 会以类似方式输出：

```text
57 AB 01 MM 00 K1 K2 K3 K4 K5 K6
```

其中：

- `MM` 是 modifier；
- `K1-K6` 是 USB HID boot keyboard keycode。

这个模式更接近 CH9350 的思路，但下游 MCU 需要自己解析 USB HID keycode、Shift、Ctrl、Alt、按下/释放等信息。

对于低成本单片机文本输入，通常更推荐默认 `TERM` 模式。

---

## 已知限制

- 默认按 US 键盘布局处理符号。
- 不直接支持中文输入法。中文输入应由后端系统或上位终端处理。
- 某些 NKRO 键盘、复合 HID 键盘、带特殊协议的键盘可能需要额外适配。
- 24 MHz 实验版本功耗更低，但实测 USB Host 功能不稳定。
- 本项目主要面向键盘输入，鼠标输入尚未作为主要功能实现。

---

## 可能的使用方向

- 给 STC / 8051 / PY32 / CH32V003 等低成本 MCU 添加 USB 键盘输入。
- 自制 OLED 文本编辑器。
- 自制串口终端。
- ESP32 MicroPython 物理键盘 REPL。
- SSH / Telnet 物理终端。
- 嵌入式计算器 / 便携开发终端。
- 单片机菜单系统输入。
- 键盘宏转换器。
- USB 键盘转 TTL 串口输入模块。
- CH9350 的可编程替代方案。
- 后续扩展为 USB Keyboard → Macro → UART / HID 的中间件。

---

## 致谢

本项目基于和参考了以下开源项目与库：

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)  
  提供 RP2040 底层 SDK、CMake 构建系统、硬件抽象、时钟、GPIO、UART 等支持。

- [TinyUSB](https://github.com/hathach/tinyusb)  
  提供轻量级 USB Host / Device 协议栈。本项目使用 TinyUSB Host HID 读取 USB 键盘输入。

- [Raspberry Pi Pico / RP2040](https://www.raspberrypi.com/products/raspberry-pi-pico/)  
  提供低成本、资源充足、社区活跃的 RP2040 平台。

- 相关社区项目，例如 Pico USB Host、Pico-PIO-USB、PiKVM Pico HID 等，为本项目的设计和实现提供了重要参考。

感谢这些项目和社区让低成本 MCU 上的 USB Host / HID 实验变得可行。

---

## License

```text
MIT License
```

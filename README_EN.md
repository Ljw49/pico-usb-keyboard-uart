# Pico USB Keyboard to UART Terminal

A USB keyboard to UART terminal input adapter based on **RP2040 / Raspberry Pi Pico / RP2040-Zero** and **TinyUSB Host HID**.

```text
USB Keyboard
    ↓
RP2040 Pico / RP2040-Zero as USB Host
    ↓
UART TX terminal byte stream
    ↓
STC / STM32 / PY32 / ESP32 / MicroPython REPL / DIY terminal
```

---

## Features

- Uses RP2040 USB Host to read USB keyboard input
- Uses TinyUSB Host HID for keyboard report handling
- Outputs terminal-friendly UART byte stream
- Supports common character input:
  - lowercase letters
  - uppercase letters
  - numbers
  - programming symbols, such as `! @ # $ % ^ & * ( ) _ + { } | : " < > ? ~`
- Supports common control keys:
  - Enter
  - Backspace
  - Delete
  - Tab
  - Esc
  - Arrow keys
  - Home / End
  - PageUp / PageDown
- Supports common Ctrl combinations:
  - Ctrl+A to Ctrl+Z
  - Ctrl+C for MicroPython REPL / Shell interrupt
  - Ctrl+D for EOF / exit-like behavior
  - Ctrl+L for clear screen / refresh
- Supports key repeat
- Provides multiple UART baud rate firmware variants:
  - 115200
  - 38400
  - 9600
- Provides a CH9350-like raw mode:
  - outputs `57 AB 01 + 8-byte USB boot keyboard report`
- Low-power optimized variants:
  - 48 MHz system clock
  - reduced core voltage
  - short sleep in main loop
  - unused GPIOs configured to high-impedance low-power state
  - unused stdio disabled
- Convenient for RP2040-Zero:
  - default UART uses `GP0 / GP1`
  - easy to use on breadboards
  - only TX + GND are required for keyboard-to-UART output

---

## Hardware Wiring

Default UART configuration:

| RP2040 Pin | Function | Connect To |
|---|---|---|
| GP0 | UART0 TX | RX of target MCU / USB-TTL adapter |
| GP1 | UART0 RX | Optional, currently not required |
| GND | Ground | GND of target device |

Default serial format depends on the firmware variant, for example:

```text
115200 8N1
38400 8N1
9600 8N1
```

For simple **USB keyboard → UART output**, only the following wires are required:

```text
GP0 TX → Target RX
GND    → Target GND
```

In other words, **one TX wire plus GND is enough**.

---

## USB Keyboard Connection

In this project, the RP2040 works as a **USB Host**, so the USB-C / Micro-USB port is used to connect a USB keyboard, not a computer.

### Method 1: Powered USB Hub

Recommended for testing:

```text
Powered USB Hub
    ├── USB Keyboard
    └── USB-C / Micro-USB port of Pico / RP2040-Zero
```

The powered hub provides 5V VBUS for the keyboard, while the Pico handles USB Host communication.

Notes:

- Some USB hubs do not back-power the upstream port. In that case, the Pico still needs separate power.
- Some cheap USB hubs have non-standard back-power behavior. Be careful about power conflicts.
- If the keyboard does not enumerate, first check hub power, cable, and connector direction.

### Method 2: External 5V/VBUS Power, USB-C Connected to Keyboard

Theoretically possible, but not fully tested:

```text
5V Power → 5V / VBUS pin of RP2040 board
USB-C    → USB Keyboard
```

This lets the RP2040 board provide VBUS to the USB keyboard while acting as a USB Host.

Notes:

- Do not connect 5V to the 3.3V pin.
- Check your RP2040 board's VBUS and USB power design before trying this.
- Different Pico-compatible or RP2040-Zero boards may have different power circuits.
- If unsure, use the powered USB hub method first.

---

## UART Output Protocol

Default mode is `TERM` mode, which outputs a terminal-friendly byte stream.

### Normal Characters

Normal characters are sent directly as ASCII:

| Keyboard Input | UART Output |
|---|---|
| `a` | `0x61` |
| `A` | `0x41` |
| `1` | `0x31` |
| `!` | `0x21` |
| Space | `0x20` |

The default keyboard layout is **US layout**.

### Control Keys

| Key | UART Output |
|---|---|
| Enter | `\r` |
| Backspace | `0x08` |
| Delete | `ESC [ 3 ~` |
| Up | `ESC [ A` |
| Down | `ESC [ B` |
| Right | `ESC [ C` |
| Left | `ESC [ D` |
| Home | `ESC [ H` |
| End | `ESC [ F` |
| PageUp | `ESC [ 5 ~` |
| PageDown | `ESC [ 6 ~` |
| Esc | `0x1B` |
| Tab | `0x09` |

### Ctrl Combinations

| Shortcut | UART Output |
|---|---|
| Ctrl+A | `0x01` |
| Ctrl+B | `0x02` |
| Ctrl+C | `0x03` |
| Ctrl+D | `0x04` |
| ... | ... |
| Ctrl+Z | `0x1A` |

This makes it suitable for:

- MicroPython REPL
- serial shell
- simple text editor
- OLED terminal
- SSH / Telnet physical terminal frontend

---

## Firmware Variants

The project may generate UF2 files such as:

```text
pico_usb_kbd_uart_lp48_term_115200.uf2
pico_usb_kbd_uart_lp48_term_38400.uf2
pico_usb_kbd_uart_lp48_term_9600.uf2
pico_usb_kbd_uart_lp48_ch9350raw_115200.uf2
pico_usb_kbd_uart_lp24_term_115200_exp.uf2
```

Recommended default firmware:

```text
pico_usb_kbd_uart_lp48_term_115200.uf2
```

This is the 48 MHz low-power terminal mode variant and has been tested to work normally.

### 24 MHz Experimental Variant

The `lp24` variant reduces clock frequency further. It has lower current consumption, but USB Host functionality may become unstable. It is kept only as an experimental build.

---

## Power Consumption

Measured in the current test setup:

| Firmware | Current | Status |
|---|---:|---|
| Default high-clock version | ~31 mA | Works |
| 48 MHz low-power version | ~16 mA | Works |
| 24 MHz experimental version | ~12.5 mA | Abnormal USB Host behavior |

Current may vary depending on board, USB keyboard, hub, cable, and power supply.

Recommended practical version:

```text
48 MHz low-power version
```

It provides a good balance between power consumption and USB Host stability.

---

## Build Instructions

### Dependencies

Required:

- Pico SDK
- TinyUSB
- CMake
- arm-none-eabi-gcc
- picotool, optional but recommended

Ubuntu / Debian example:

```bash
sudo apt update
sudo apt install -y cmake build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi
```

### Pico SDK Path

If Pico SDK is installed:

```bash
export PICO_SDK_PATH=$HOME/pico/pico-sdk
```

If picotool is installed:

```bash
export picotool_DIR=$HOME/pico/picotool-install/picotool
```

### Compile

```bash
mkdir build
cd build

cmake \
  -DPICO_SDK_PATH=$HOME/pico/pico-sdk \
  -Dpicotool_DIR=$HOME/pico/picotool-install/picotool \
  ..

cmake --build . -j$(nproc)
```

After compilation, `.uf2` files will be generated in the `build/` directory.

---

## Flashing

Hold the BOOTSEL button on Pico / RP2040-Zero, then plug it into the computer.

A USB mass-storage drive named `RPI-RP2` should appear.

Drag the desired `.uf2` file into it, for example:

```text
pico_usb_kbd_uart_lp48_term_115200.uf2
```

The board will reboot automatically and start working as a USB Host keyboard-to-UART adapter.

---

## Usage Examples

### Example 1: STC / PY32 / STM32 Text Interface

```text
USB Keyboard
    ↓
Pico USB Host
    ↓ GP0 UART TX
STC / PY32 / STM32 RX
    ↓
OLED text window / menu / editor
```

### Example 2: ESP32 MicroPython REPL

```text
USB Keyboard
    ↓
Pico
    ↓ UART
ESP32 MicroPython REPL
```

Supported operations may include:

- character input
- Enter to execute
- Backspace deletion
- Ctrl+C interrupt
- Ctrl+D EOF / soft reset related behavior
- arrow keys for history navigation, depending on REPL support

### Example 3: SSH Physical Terminal Input

```text
USB Keyboard
    ↓
Pico
    ↓ UART
ESP32 / Linux SBC / DIY terminal
    ↓
SSH / Telnet / Shell
```

---

## CH9350-like Raw Mode

In addition to terminal mode, the project provides a CH9350-like raw output mode.

Output format:

```text
57 AB 01 + 8-byte USB boot keyboard report
```

Example report format:

```text
57 AB 01 MM 00 K1 K2 K3 K4 K5 K6
```

Where:

- `MM` is the modifier byte
- `K1-K6` are USB HID boot keyboard keycodes

This mode is closer to CH9350-style raw keyboard data, but the downstream MCU needs to parse HID keycodes, Shift, Ctrl, Alt, key press/release, etc.

For low-cost MCU text input, the default `TERM` mode is usually easier to use.

---

## Known Limitations

- Default keyboard layout is US layout.
- Chinese input method is not implemented. Chinese input should be handled by the downstream system or host terminal.
- Some NKRO keyboards, composite HID keyboards, or keyboards with special vendor protocols may require additional adaptation.
- The 24 MHz experimental firmware has lower current consumption but may not work reliably as USB Host.
- The project currently focuses on keyboard input. Mouse input is not the main target yet.

---

## Possible Applications

- Add USB keyboard input to STC / 8051 / PY32 / CH32V003 / other low-cost MCUs
- DIY OLED text editor
- DIY serial terminal
- ESP32 MicroPython physical keyboard REPL
- SSH / Telnet physical terminal
- Embedded calculator / portable development terminal
- MCU menu system input
- Keyboard macro converter
- USB keyboard to TTL UART input module
- Programmable alternative to CH9350-like keyboard-to-UART adapters
- Future USB Keyboard → Macro → UART / HID middleware

---

## Acknowledgements

This project is based on and inspired by the following open-source projects and libraries:

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)  
  Provides the RP2040 SDK, CMake build system, hardware abstraction, clocks, GPIO, UART, and low-level support.

- [TinyUSB](https://github.com/hathach/tinyusb)  
  Provides a lightweight USB Host / Device stack. This project uses TinyUSB Host HID to read USB keyboard input.

- [Raspberry Pi Pico / RP2040](https://www.raspberrypi.com/products/raspberry-pi-pico/)  
  Provides a low-cost, resource-rich, and community-friendly RP2040 platform.

- Related community projects, including Pico USB Host examples, Pico-PIO-USB, and PiKVM Pico HID, which provided important inspiration for USB Host/HID design and testing.

Thanks to these projects and communities for making low-cost USB Host / HID experiments practical on RP2040.

---

## License

This project is released under the MIT License.

See [LICENSE](LICENSE) for details.

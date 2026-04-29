# Pico USB Keyboard -> UART Terminal Adapter, low-power variants

Default pins:

```text
GP0 = UART0 TX -> target MCU RX
GP1 = UART0 RX <- target MCU TX, optional
GND = common ground
```

Generated targets:

```text
pico_usb_kbd_uart_lp48_term_115200.uf2
pico_usb_kbd_uart_lp48_term_38400.uf2
pico_usb_kbd_uart_lp48_term_9600.uf2
pico_usb_kbd_uart_lp48_ch9350raw_115200.uf2
pico_usb_kbd_uart_lp24_term_115200_exp.uf2
```

Recommended first: `pico_usb_kbd_uart_lp48_term_115200.uf2`.

Low-power changes:

```text
48 MHz sys clock for normal low-power builds
0.95 V VREG for 48 MHz builds
1 ms sleep in main loop
status LED disabled
unused GPIOs high-Z, pulls off, input buffer disabled
```

The 24 MHz build uses 0.90 V and is experimental. If USB keyboard enumeration is unstable, use the 48 MHz build.

Build:

```bash
cd pico_usb_kbd_uart_lowpower
rm -rf build
mkdir build
cd build
cmake -DPICO_SDK_PATH=$HOME/pico/pico-sdk -Dpicotool_DIR=$HOME/pico/picotool-install/picotool ..
cmake --build . -j$(nproc)
```

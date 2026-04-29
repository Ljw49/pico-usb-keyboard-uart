/*
 * Pico USB Keyboard -> UART Terminal Input Adapter
 *
 * Native Pico USB port is USB Host. UART output defaults can be overridden
 * from CMake compile definitions. The provided variants use UART0:
 *   GPIO0 TX, GPIO1 RX, 115200/38400/9600 8N1.
 *
 * Default protocol is terminal byte stream:
 *   printable keys -> ASCII, Enter -> CR, Backspace -> 0x08,
 *   arrows/Delete/etc -> ANSI/xterm escape sequences,
 *   Ctrl+A..Z -> 0x01..0x1A, Alt+key -> ESC prefix + key.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "tusb.h"

#ifndef KBD_UART
#define KBD_UART              uart0
#endif
#ifndef KBD_UART_TX_PIN
#define KBD_UART_TX_PIN       0u
#endif
#ifndef KBD_UART_RX_PIN
#define KBD_UART_RX_PIN       1u
#endif
#ifndef KBD_UART_BAUD
#define KBD_UART_BAUD         115200u
#endif

#ifndef SYS_CLOCK_KHZ
#define SYS_CLOCK_KHZ         48000u
#endif
#ifndef LOW_POWER_VREG_LEVEL
#define LOW_POWER_VREG_LEVEL  95u
#endif
#ifndef IDLE_SLEEP_US
#define IDLE_SLEEP_US         1000u
#endif
#ifndef CONFIGURE_UNUSED_GPIOS
#define CONFIGURE_UNUSED_GPIOS 1
#endif
#ifndef USE_STATUS_LED
#define USE_STATUS_LED        0
#endif

#define OUTPUT_MODE_TERM          1
#define OUTPUT_MODE_CH9350_RAW    2
#ifndef OUTPUT_MODE
#define OUTPUT_MODE OUTPUT_MODE_TERM
#endif

#define ENTER_SEND_CRLF       0
#define BACKSPACE_SEND_DEL    0
#define ALT_SEND_ESC_PREFIX   1

#define KEY_REPEAT_ENABLE     1
#define KEY_REPEAT_DELAY_MS   480u
#define KEY_REPEAT_PERIOD_MS  45u

#define CH9350_KEYBOARD_ID    0x01u

#if USE_STATUS_LED && defined(PICO_DEFAULT_LED_PIN)
#define STATUS_LED_PIN        PICO_DEFAULT_LED_PIN
#else
#define STATUS_LED_PIN        (-1)
#endif

static hid_keyboard_report_t s_prev_report = {0};
static bool s_caps_lock = false;

#if KEY_REPEAT_ENABLE
typedef struct {
    bool active;
    uint8_t keycode;
    uint8_t modifier;
    uint32_t next_ms;
} repeat_state_t;
static repeat_state_t s_repeat = {0};
#endif

static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

#if OUTPUT_MODE == OUTPUT_MODE_CH9350_RAW
static void uart_write_bytes(uint8_t const *data, size_t len) {
    if (len) uart_write_blocking(KBD_UART, data, len);
}
#endif

static void uart_write_str(char const *s) {
    uart_write_blocking(KBD_UART, (uint8_t const *)s, strlen(s));
}

static void uart_write_ch(uint8_t ch) {
    uart_write_blocking(KBD_UART, &ch, 1);
}

static void uart_write_dec_u8(uint8_t v) {
    char buf[4];
    int n = snprintf(buf, sizeof(buf), "%u", (unsigned)v);
    if (n > 0) uart_write_blocking(KBD_UART, (uint8_t const *)buf, (size_t)n);
}

static void led_init_safe(void) {
#if STATUS_LED_PIN >= 0
    gpio_init((uint)STATUS_LED_PIN);
    gpio_set_dir((uint)STATUS_LED_PIN, GPIO_OUT);
    gpio_put((uint)STATUS_LED_PIN, 0);
#endif
}

static uint32_t s_led_off_at_ms = 0;
static void led_pulse(void) {
#if STATUS_LED_PIN >= 0
    gpio_put((uint)STATUS_LED_PIN, 1);
    s_led_off_at_ms = now_ms() + 20u;
#endif
}

static void led_task(void) {
#if STATUS_LED_PIN >= 0
    if (s_led_off_at_ms && (int32_t)(now_ms() - s_led_off_at_ms) >= 0) {
        gpio_put((uint)STATUS_LED_PIN, 0);
        s_led_off_at_ms = 0;
    }
#endif
}

static void apply_low_power_clocking(void) {
#if SYS_CLOCK_KHZ > 0
    (void)set_sys_clock_khz(SYS_CLOCK_KHZ, true);
#endif
#if LOW_POWER_VREG_LEVEL == 85
    vreg_set_voltage(VREG_VOLTAGE_0_85); sleep_ms(10);
#elif LOW_POWER_VREG_LEVEL == 90
    vreg_set_voltage(VREG_VOLTAGE_0_90); sleep_ms(10);
#elif LOW_POWER_VREG_LEVEL == 95
    vreg_set_voltage(VREG_VOLTAGE_0_95); sleep_ms(10);
#elif LOW_POWER_VREG_LEVEL == 100
    vreg_set_voltage(VREG_VOLTAGE_1_00); sleep_ms(10);
#elif LOW_POWER_VREG_LEVEL == 105
    vreg_set_voltage(VREG_VOLTAGE_1_05); sleep_ms(10);
#elif LOW_POWER_VREG_LEVEL == 110
    vreg_set_voltage(VREG_VOLTAGE_1_10); sleep_ms(10);
#endif
}

static bool is_protected_gpio(uint pin) {
    if (pin == KBD_UART_TX_PIN || pin == KBD_UART_RX_PIN) return true;
#if STATUS_LED_PIN >= 0
    if (pin == (uint)STATUS_LED_PIN) return true;
#endif
    if (pin == 23u || pin == 24u || pin == 29u) return true;
    return false;
}

static void configure_unused_gpios_low_power(void) {
#if CONFIGURE_UNUSED_GPIOS
    for (uint pin = 0; pin < 30u; ++pin) {
        if (is_protected_gpio(pin)) continue;
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_disable_pulls(pin);
        gpio_set_input_enabled(pin, false);
    }
#endif
}

static void uart_init_terminal(void) {
    uart_init(KBD_UART, KBD_UART_BAUD);
    gpio_set_function(KBD_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(KBD_UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(KBD_UART, false, false);
    uart_set_format(KBD_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(KBD_UART, true);
}

static inline bool mod_shift(uint8_t mod) {
    return mod & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
}
static inline bool mod_ctrl(uint8_t mod) {
    return mod & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL);
}
static inline bool mod_alt(uint8_t mod) {
    return mod & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT);
}

// xterm modifier parameter: Shift=2, Alt=3, Shift+Alt=4, Ctrl=5, ...
static uint8_t ansi_modifier_param(uint8_t mod) {
    uint8_t p = 1;
    if (mod_shift(mod)) p += 1;
    if (mod_alt(mod))   p += 2;
    if (mod_ctrl(mod))  p += 4;
    return p;
}

static void emit_csi_arrow(char final, uint8_t mod) {
    uint8_t p = ansi_modifier_param(mod);
    if (p == 1) {
        uart_write_ch(0x1b);
        uart_write_ch('[');
        uart_write_ch((uint8_t)final);
    } else {
        uart_write_ch(0x1b);
        uart_write_str("[1;");
        uart_write_dec_u8(p);
        uart_write_ch((uint8_t)final);
    }
}

static void emit_csi_tilde(uint8_t code, uint8_t mod) {
    uint8_t p = ansi_modifier_param(mod);
    uart_write_ch(0x1b);
    uart_write_ch('[');
    uart_write_dec_u8(code);
    if (p != 1) {
        uart_write_ch(';');
        uart_write_dec_u8(p);
    }
    uart_write_ch('~');
}

static void emit_alt_prefix_if_needed(uint8_t mod) {
#if ALT_SEND_ESC_PREFIX
    if (mod_alt(mod)) uart_write_ch(0x1b);
#else
    (void)mod;
#endif
}

static bool emit_ctrl_combo(uint8_t keycode, uint8_t mod) {
    if (!mod_ctrl(mod)) return false;
    uint8_t out = 0;

    if (keycode >= HID_KEY_A && keycode <= HID_KEY_Z) {
        out = (uint8_t)(1u + keycode - HID_KEY_A);  // Ctrl+A -> 0x01
    } else {
        switch (keycode) {
            case HID_KEY_BRACKET_LEFT:  out = 0x1b; break; // Ctrl+[ = ESC
            case HID_KEY_BACKSLASH:     out = 0x1c; break; // Ctrl+backslash
            case HID_KEY_BRACKET_RIGHT: out = 0x1d; break; // Ctrl+]
            case HID_KEY_6:             out = 0x1e; break; // Ctrl+^ on US layout
            case HID_KEY_MINUS:         out = 0x1f; break; // Ctrl+_
            case HID_KEY_BACKSPACE:     out = 0x08; break;
            case HID_KEY_ENTER:         out = 0x0d; break;
            case HID_KEY_TAB:           out = 0x09; break;
            default: break;
        }
    }

    if (!out) return false;
    emit_alt_prefix_if_needed(mod);
    uart_write_ch(out);
    return true;
}

static bool ascii_from_hid(uint8_t keycode, uint8_t mod, uint8_t *out_ch) {
    bool shift = mod_shift(mod);

    if (keycode >= HID_KEY_A && keycode <= HID_KEY_Z) {
        bool upper = shift ^ s_caps_lock;
        *out_ch = (uint8_t)((upper ? 'A' : 'a') + (keycode - HID_KEY_A));
        return true;
    }

    switch (keycode) {
        case HID_KEY_1: *out_ch = shift ? '!' : '1'; return true;
        case HID_KEY_2: *out_ch = shift ? '@' : '2'; return true;
        case HID_KEY_3: *out_ch = shift ? '#' : '3'; return true;
        case HID_KEY_4: *out_ch = shift ? '$' : '4'; return true;
        case HID_KEY_5: *out_ch = shift ? '%' : '5'; return true;
        case HID_KEY_6: *out_ch = shift ? '^' : '6'; return true;
        case HID_KEY_7: *out_ch = shift ? '&' : '7'; return true;
        case HID_KEY_8: *out_ch = shift ? '*' : '8'; return true;
        case HID_KEY_9: *out_ch = shift ? '(' : '9'; return true;
        case HID_KEY_0: *out_ch = shift ? ')' : '0'; return true;

        case HID_KEY_SPACE:         *out_ch = ' '; return true;
        case HID_KEY_MINUS:         *out_ch = shift ? '_' : '-'; return true;
        case HID_KEY_EQUAL:         *out_ch = shift ? '+' : '='; return true;
        case HID_KEY_BRACKET_LEFT:  *out_ch = shift ? '{' : '['; return true;
        case HID_KEY_BRACKET_RIGHT: *out_ch = shift ? '}' : ']'; return true;
        case HID_KEY_BACKSLASH:     *out_ch = shift ? '|' : '\\'; return true;
        case HID_KEY_SEMICOLON:     *out_ch = shift ? ':' : ';'; return true;
        case HID_KEY_APOSTROPHE:    *out_ch = shift ? '"' : '\''; return true;
        case HID_KEY_GRAVE:         *out_ch = shift ? '~' : '`'; return true;
        case HID_KEY_COMMA:         *out_ch = shift ? '<' : ','; return true;
        case HID_KEY_PERIOD:        *out_ch = shift ? '>' : '.'; return true;
        case HID_KEY_SLASH:         *out_ch = shift ? '?' : '/'; return true;

        case HID_KEY_KEYPAD_0:       *out_ch = '0'; return true;
        case HID_KEY_KEYPAD_1:       *out_ch = '1'; return true;
        case HID_KEY_KEYPAD_2:       *out_ch = '2'; return true;
        case HID_KEY_KEYPAD_3:       *out_ch = '3'; return true;
        case HID_KEY_KEYPAD_4:       *out_ch = '4'; return true;
        case HID_KEY_KEYPAD_5:       *out_ch = '5'; return true;
        case HID_KEY_KEYPAD_6:       *out_ch = '6'; return true;
        case HID_KEY_KEYPAD_7:       *out_ch = '7'; return true;
        case HID_KEY_KEYPAD_8:       *out_ch = '8'; return true;
        case HID_KEY_KEYPAD_9:       *out_ch = '9'; return true;
        case HID_KEY_KEYPAD_DECIMAL: *out_ch = '.'; return true;
        case HID_KEY_KEYPAD_DIVIDE:  *out_ch = '/'; return true;
        case HID_KEY_KEYPAD_MULTIPLY:*out_ch = '*'; return true;
        case HID_KEY_KEYPAD_SUBTRACT:*out_ch = '-'; return true;
        case HID_KEY_KEYPAD_ADD:     *out_ch = '+'; return true;
        case HID_KEY_KEYPAD_EQUAL:   *out_ch = '='; return true;
        default: break;
    }
    return false;
}

static bool emit_key_terminal(uint8_t keycode, uint8_t mod) {
    if (keycode < HID_KEY_A) return false; // Ignore 0 and HID error-rollover codes 1..3.

    if (keycode == HID_KEY_CAPS_LOCK) {
        s_caps_lock = !s_caps_lock;
        return false;
    }

    if (emit_ctrl_combo(keycode, mod)) return true;

    switch (keycode) {
        case HID_KEY_ENTER:
            emit_alt_prefix_if_needed(mod);
#if ENTER_SEND_CRLF
            uart_write_str("\r\n");
#else
            uart_write_ch('\r');
#endif
            return true;

        case HID_KEY_ESCAPE:
            uart_write_ch(0x1b);
            return true;

        case HID_KEY_BACKSPACE:
            emit_alt_prefix_if_needed(mod);
#if BACKSPACE_SEND_DEL
            uart_write_ch(0x7f);
#else
            uart_write_ch(0x08);
#endif
            return true;

        case HID_KEY_TAB:
            emit_alt_prefix_if_needed(mod);
            uart_write_ch('\t');
            return true;

        case HID_KEY_ARROW_UP:    emit_csi_arrow('A', mod); return true;
        case HID_KEY_ARROW_DOWN:  emit_csi_arrow('B', mod); return true;
        case HID_KEY_ARROW_RIGHT: emit_csi_arrow('C', mod); return true;
        case HID_KEY_ARROW_LEFT:  emit_csi_arrow('D', mod); return true;

        case HID_KEY_INSERT:      emit_csi_tilde(2, mod); return true;
        case HID_KEY_DELETE:      emit_csi_tilde(3, mod); return true;
        case HID_KEY_PAGE_UP:     emit_csi_tilde(5, mod); return true;
        case HID_KEY_PAGE_DOWN:   emit_csi_tilde(6, mod); return true;

        case HID_KEY_HOME:
            if (ansi_modifier_param(mod) == 1) uart_write_str("\x1b[H");
            else emit_csi_arrow('H', mod);
            return true;

        case HID_KEY_END:
            if (ansi_modifier_param(mod) == 1) uart_write_str("\x1b[F");
            else emit_csi_arrow('F', mod);
            return true;

        case HID_KEY_F1:  uart_write_str("\x1bOP"); return true;
        case HID_KEY_F2:  uart_write_str("\x1bOQ"); return true;
        case HID_KEY_F3:  uart_write_str("\x1bOR"); return true;
        case HID_KEY_F4:  uart_write_str("\x1bOS"); return true;
        case HID_KEY_F5:  emit_csi_tilde(15, mod); return true;
        case HID_KEY_F6:  emit_csi_tilde(17, mod); return true;
        case HID_KEY_F7:  emit_csi_tilde(18, mod); return true;
        case HID_KEY_F8:  emit_csi_tilde(19, mod); return true;
        case HID_KEY_F9:  emit_csi_tilde(20, mod); return true;
        case HID_KEY_F10: emit_csi_tilde(21, mod); return true;
        case HID_KEY_F11: emit_csi_tilde(23, mod); return true;
        case HID_KEY_F12: emit_csi_tilde(24, mod); return true;

        case HID_KEY_KEYPAD_ENTER:
#if ENTER_SEND_CRLF
            uart_write_str("\r\n");
#else
            uart_write_ch('\r');
#endif
            return true;

        default:
            break;
    }

    uint8_t ch = 0;
    if (ascii_from_hid(keycode, mod, &ch)) {
        emit_alt_prefix_if_needed(mod);
        if (ch == '\n') ch = '\r';
        uart_write_ch(ch);
        return true;
    }
    return false;
}

static bool key_in_report(hid_keyboard_report_t const *report, uint8_t keycode) {
    for (uint8_t i = 0; i < 6; ++i) {
        if (report->keycode[i] == keycode) return true;
    }
    return false;
}

#if KEY_REPEAT_ENABLE
static bool is_repeatable_key(uint8_t keycode, uint8_t mod) {
    if (mod_ctrl(mod)) return false; // Avoid repeating Ctrl+C, Ctrl+D, etc.

    switch (keycode) {
        case HID_KEY_ARROW_UP:
        case HID_KEY_ARROW_DOWN:
        case HID_KEY_ARROW_RIGHT:
        case HID_KEY_ARROW_LEFT:
        case HID_KEY_BACKSPACE:
        case HID_KEY_DELETE:
        case HID_KEY_HOME:
        case HID_KEY_END:
        case HID_KEY_PAGE_UP:
        case HID_KEY_PAGE_DOWN:
            return true;
        default:
            break;
    }

    uint8_t ch;
    return ascii_from_hid(keycode, mod, &ch);
}

static void repeat_start(uint8_t keycode, uint8_t mod) {
    if (!is_repeatable_key(keycode, mod)) {
        s_repeat.active = false;
        return;
    }
    s_repeat.active = true;
    s_repeat.keycode = keycode;
    s_repeat.modifier = mod;
    s_repeat.next_ms = now_ms() + KEY_REPEAT_DELAY_MS;
}

static void repeat_stop_if_released(hid_keyboard_report_t const *report) {
    if (s_repeat.active && !key_in_report(report, s_repeat.keycode)) {
        s_repeat.active = false;
    }
}

static void repeat_task(void) {
    if (!s_repeat.active) return;
    uint32_t t = now_ms();
    if ((int32_t)(t - s_repeat.next_ms) >= 0) {
        if (emit_key_terminal(s_repeat.keycode, s_repeat.modifier)) led_pulse();
        s_repeat.next_ms = t + KEY_REPEAT_PERIOD_MS;
    }
}
#endif

static void process_boot_keyboard_report(hid_keyboard_report_t const *report) {
#if KEY_REPEAT_ENABLE
    repeat_stop_if_released(report);
#endif

    for (uint8_t i = 0; i < 6; ++i) {
        uint8_t keycode = report->keycode[i];
        if (keycode == 0) continue;

        // Only process newly pressed keys. Holding is handled by repeat_task().
        if (!key_in_report(&s_prev_report, keycode)) {
            if (emit_key_terminal(keycode, report->modifier)) {
                led_pulse();
#if KEY_REPEAT_ENABLE
                repeat_start(keycode, report->modifier);
#endif
            }
        }
    }

    s_prev_report = *report;
}

#if OUTPUT_MODE == OUTPUT_MODE_CH9350_RAW
static void emit_ch9350_keyboard_frame(uint8_t const *report, uint16_t len) {
    uint8_t frame_head[3] = {0x57u, 0xABu, CH9350_KEYBOARD_ID};
    uint8_t boot_report[8] = {0};

    if (len > sizeof(boot_report)) len = sizeof(boot_report);
    memcpy(boot_report, report, len);

    uart_write_bytes(frame_head, sizeof(frame_head));
    uart_write_bytes(boot_report, sizeof(boot_report));
    led_pulse();
}
#endif

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const *desc_report, uint16_t desc_len) {
    (void)desc_report;
    (void)desc_len;
    uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);

    if (proto == HID_ITF_PROTOCOL_KEYBOARD) {
        s_prev_report = (hid_keyboard_report_t){0};
#if KEY_REPEAT_ENABLE
        s_repeat.active = false;
#endif
        tuh_hid_receive_report(dev_addr, instance);
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    (void)dev_addr;
    (void)instance;
    s_prev_report = (hid_keyboard_report_t){0};
#if KEY_REPEAT_ENABLE
    s_repeat.active = false;
#endif
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                uint8_t const *report, uint16_t len) {
    uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);

    if (proto == HID_ITF_PROTOCOL_KEYBOARD) {
#if OUTPUT_MODE == OUTPUT_MODE_CH9350_RAW
        emit_ch9350_keyboard_frame(report, len);
#else
        if (len >= sizeof(hid_keyboard_report_t)) {
            process_boot_keyboard_report((hid_keyboard_report_t const *)report);
        }
#endif
    }

    // Continue polling this HID interface.
    tuh_hid_receive_report(dev_addr, instance);
}

int main(void) {
    apply_low_power_clocking();
    configure_unused_gpios_low_power();
    led_init_safe();
    uart_init_terminal();

    // Initialize TinyUSB host stack on native USB port.
    tusb_init();

    while (true) {
        tuh_task();
#if KEY_REPEAT_ENABLE && (OUTPUT_MODE == OUTPUT_MODE_TERM)
        repeat_task();
#endif
        led_task();
#if IDLE_SLEEP_US > 0
        sleep_us(IDLE_SLEEP_US);
#else
        tight_loop_contents();
#endif
    }
}

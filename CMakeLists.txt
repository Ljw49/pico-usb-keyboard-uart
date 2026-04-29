cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico CACHE STRING "Board type")
include(pico_sdk_import.cmake)

project(pico_usb_kbd_uart_lowpower C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

pico_sdk_init()

function(add_kbd_uart_variant target_name baud output_mode sys_khz vreg_level sleep_us)
    add_executable(${target_name} main.c)
    target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_link_libraries(${target_name} PRIVATE
        pico_stdlib
        hardware_uart
        hardware_clocks
        hardware_vreg
        tinyusb_host
        tinyusb_board
    )
    target_compile_definitions(${target_name} PRIVATE
        KBD_UART=uart0
        KBD_UART_TX_PIN=0u
        KBD_UART_RX_PIN=1u
        KBD_UART_BAUD=${baud}u
        OUTPUT_MODE=${output_mode}
        SYS_CLOCK_KHZ=${sys_khz}u
        LOW_POWER_VREG_LEVEL=${vreg_level}u
        IDLE_SLEEP_US=${sleep_us}u
        CONFIGURE_UNUSED_GPIOS=1
        USE_STATUS_LED=0
    )
    pico_enable_stdio_usb(${target_name} 0)
    pico_enable_stdio_uart(${target_name} 0)
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    pico_add_extra_outputs(${target_name})
endfunction()

add_kbd_uart_variant(pico_usb_kbd_uart_lp48_term_115200 115200 OUTPUT_MODE_TERM        48000 95 1000)
add_kbd_uart_variant(pico_usb_kbd_uart_lp48_term_38400   38400  OUTPUT_MODE_TERM        48000 95 1000)
add_kbd_uart_variant(pico_usb_kbd_uart_lp48_term_9600    9600   OUTPUT_MODE_TERM        48000 95 1000)
add_kbd_uart_variant(pico_usb_kbd_uart_lp48_ch9350raw_115200 115200 OUTPUT_MODE_CH9350_RAW 48000 95 1000)
add_kbd_uart_variant(pico_usb_kbd_uart_lp24_term_115200_exp 115200 OUTPUT_MODE_TERM     24000 90 1000)

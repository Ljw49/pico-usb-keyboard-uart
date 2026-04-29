#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_
#ifdef __cplusplus
extern "C" {
#endif
#define CFG_TUSB_OS                 OPT_OS_PICO
#define CFG_TUH_ENABLED             1
#define CFG_TUD_ENABLED             0
#define BOARD_TUH_RHPORT            0
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_HOST | OPT_MODE_FULL_SPEED)
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__((aligned(4)))
#endif
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HUB                 0
#define CFG_TUH_DEVICE_MAX          1
#define CFG_TUSB_HOST_DEVICE_MAX    CFG_TUH_DEVICE_MAX
#define CFG_TUH_HID                 2
#define CFG_TUH_HID_EPIN_BUFSIZE    64
#define CFG_TUH_HID_EPOUT_BUFSIZE   64
#ifdef __cplusplus
}
#endif
#endif

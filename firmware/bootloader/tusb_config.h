#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

// bootloader/tusb_config.h — minimal CDC-only TinyUSB config for the bench
// bootloader. TX buffer raised to 256 to avoid the app's H2-style truncation
// when responding with more than 64 bytes.

#ifdef __cplusplus
 extern "C" {
#endif

#define CFG_TUSB_MCU              OPT_MCU_STM32F0
#define CFG_TUSB_RHPORT0_MODE     OPT_MODE_DEVICE
#define CFG_TUSB_OS               OPT_OS_NONE

#define CFG_TUD_ENDPOINT0_SIZE    64

// CDC
#define CFG_TUD_CDC               1
#define CFG_TUD_CDC_RX_BUFSIZE    256
#define CFG_TUD_CDC_TX_BUFSIZE    256

#ifdef __cplusplus
 }
#endif

#endif

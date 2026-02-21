#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define CFG_TUSB_MCU              OPT_MCU_STM32F0
#define CFG_TUSB_RHPORT0_MODE     OPT_MODE_DEVICE
#define CFG_TUSB_OS               OPT_OS_NONE

#define CFG_TUD_ENDPOINT0_SIZE    64

// CDC
#define CFG_TUD_CDC               1
#define CFG_TUD_CDC_RX_BUFSIZE    64
#define CFG_TUD_CDC_TX_BUFSIZE    64

#ifdef __cplusplus
 }
#endif

#endif

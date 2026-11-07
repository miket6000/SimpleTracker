// boot_common.h — shared defines between the bench bootloader and the app.
//
// Flash layout (STM32F042G6, 32 KB, 1 KB pages):
//   0x08000000..0x080017FF  bootloader   (<= 6 KB)
//   0x08001800..0x08007BFF  application  (25 KB)
//   0x08007C00              info page    (build state written by bootloader)
//
// Update-mode entry: the app writes BL_BOOTFLAG_MAGIC to BL_BOOTFLAG_ADDR
// (top of SRAM, .noinit, survives HAL_NVIC_SystemReset) and resets; the
// bootloader reads it first thing in main() and stays in update mode.
// The word sits 16 bytes below the stack top, so it is dead by the time the
// boot flag is checked — read it in the first statement of main().
//
// Info page layout (little-endian):
//   +0  u32  magic 0x4946424C ("LBFI")
//   +4  u32  app base (0x08001800)
//   +8  u32  image length in bytes
//   +12 u16  image CRC16-CCITT-FALSE (matches V2 spec CRC)
//   +14 u16  reserved 0xFFFF
//   +16 u32  build id (monotonic counter, host-assigned)

#ifndef BOOT_COMMON_H
#define BOOT_COMMON_H

#include <stdint.h>

// rom trigger: app REBOOTs into bootloader for flashing 
#define BL_BOOTFLAG_ADDR       (*(volatile uint32_t *)0x200017F0UL)
#define BL_BOOTFLAG_MAGIC      0xB007B01DUL

// memory map
#define BL_APP_BASE  0x08002400UL
#define BL_APP_END   0x08007C00UL
#define BL_INFO_ADDR 0x08007C00UL

#define BL_INFO_MAGIC 0x4946424CUL
#define BL_INFO_SIZE  0x20UL

#define BL_PROTO_VERSION 1

#endif // BOOT_COMMON_H

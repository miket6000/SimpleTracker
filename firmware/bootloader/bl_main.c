// bl_main.c — bench bootloader for the STM32F042G6 tracker (bench-comms, plan Phase 1).
//
// Layout:  BL @ 0x08000000 (6 KB) | app @ 0x08001800 (25 KB) | info page 0x08007C00
// Entry:   app command "REBOOT BL" leaves BL_BOOTFLAG_MAGIC at top of SRAM
//          (survives software reset); this bootloader picks it up at boot and
//          waits in update mode. No BOOT0 pin, no power cycle needed.
// Safety:  a corrupt/absent app image (CRC or vector check failure) boots into
//          update mode, so a torn flash is always recoverable over USB alone.
//          IWDG is deliberately NOT used: on F0 it stays enabled across a
//          software reset, which would kill the just-flashed app 26 s later.
// Protocol: binary frames over CDC; CRC16-CCITT-FALSE (same CRC as the V2 spec).
// Frame:   C6 6C | TYPE | LEN | payload(<=255) | CRC16 LE (over TYPE..payload)
//
// Types (response type = request | 0x80, errors 0xFE payload[0]=code):
//   0x10 INFO     -> [0]=proto [1]=ver [2]=rev [3]=flags bit0 app_valid
//                    [4..11] uid ascii [12..15] stored build id u32
//   0x20 ERASE    -> resp pages erased u16 LE; arms a sequential WRITE stream
//   0x30 WRITE    -> payload addr u32 + data (even len); addresses must arrive
//                    back-to-back starting at APP_BASE
//   0x40 FINALIZE -> payload len u32 + image crc u16 + build id u32; verifies
//                    and writes the info page
//   0x50 GO       -> re-verify, then jump to app
//   0x60 RESET    -> software reset (fallback)
// Errors: 1 bad crc, 2 bad type, 3 bad len, 4 bad addr, 5 flash error,
//         6 crc mismatch, 7 invalid image.

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f0xx.h"
#include "boot_common.h"
#include "tusb.h"

// ---------------------------------------------------------------------------
// parameters and constants

#define BL_VERSION      1
#define BL_REVISION     0

#define PAGE_SIZE       0x400UL

#define FRAME_MAX       (6 + 255)   // sof0 sof1 type len payload crc0 crc1

// error codes
#define BL_ERR_CRC      1
#define BL_ERR_TYPE     2
#define BL_ERR_LEN      3
#define BL_ERR_ADDR     4
#define BL_ERR_FLASH    5
#define BL_ERR_CRC2     6
#define BL_ERR_IMAGE    7

// frame types
#define FR_INFO     0x10
#define FR_ERASE    0x20
#define FR_WRITE    0x30
#define FR_FINAL    0x40
#define FR_GO       0x50
#define FR_RESET    0x60

// update-mode state
static struct {
  bool     armed;       // ERASE done, WRITE stream accepted
  uint32_t next;        // next expected flash address
  uint16_t crc;         // rolling CRC over the image so far
  uint32_t written;     // bytes written so far
  bool     finalized;   // info page written this session
  bool     appValid;    // current app image usable
} g;

// saving the boot flag before anything can touch top-of-SRAM; main() reads it
// as its very first statement (SP = 0x20001800, flag 16 B below)
static uint32_t bl_bootflag;

// ---------------------------------------------------------------------------
// CRC16-CCITT-FALSE (poly 0x1021, init 0xFFFF, MSB first) — bit loop, matches
// the V2 comm spec version. crc16_usb() computes a fresh CRC; crc16_cont()
// chains onto a previous value so a multi-chunk image CRC equals the
// whole-image CRC.

// noinline: trivially inlinable arithmetic loop; one shared copy beats
// per-site copies (LTO otherwise inlines this at 4 call sites)
static __attribute__((noinline))
uint16_t crc16_cont(uint16_t crc, const uint8_t *p, uint16_t n) {
  for (uint16_t i = 0; i < n; i++) {
    crc ^= (uint16_t)(p[i] << 8);
    for (int k = 0; k < 8; k++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static uint16_t crc16_usb(const uint8_t *p, uint16_t n) {
  return crc16_cont(0xFFFF, p, n);
}

// ---------------------------------------------------------------------------
// flash low-level (STM32F042: 1 KB pages, halfword program)

static void bl_usbp(void) { tud_task(); }

static void bl_clear_flash_err(void) {
  FLASH->SR = FLASH_SR_PGERR | FLASH_SR_WRPERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP;
}

static bool bl_flash_err(void) {
  return (FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPERR | FLASH_SR_WRPRTERR)) != 0;
}

static void bl_flash_wait(void) {
  while (FLASH->SR & FLASH_SR_BSY) { }
}

static void bl_flash_unlock(void) {
  bl_flash_wait();
  if (FLASH->CR & FLASH_CR_LOCK) {
    FLASH->KEYR = 0x45670123UL;
    FLASH->KEYR = 0xCDEF89ABUL;
  }
}

static bool bl_page_erase(uint32_t pageAddr) {
  bl_flash_wait();
  bl_clear_flash_err();
  FLASH->CR |= FLASH_CR_PER;
  FLASH->AR = pageAddr;
  FLASH->CR |= FLASH_CR_STRT;
  bl_flash_wait();
  bool ok = !bl_flash_err();
  FLASH->CR &= ~FLASH_CR_PER;
  bl_clear_flash_err();
  return ok;
}

static bool bl_word_program(uint32_t addr, uint16_t val) {
  volatile uint16_t *fa = (volatile uint16_t *)addr;
  *fa = val;
  bl_flash_wait();
  return !bl_flash_err();
}

// ---------------------------------------------------------------------------
// info page + image validation

typedef struct {
  uint32_t magic;
  uint32_t base;
  uint32_t len;
  uint16_t crc;
  uint16_t rsvd;
  uint32_t buildId;
} InfoPage;

static const InfoPage *g_info = (const InfoPage *)BL_INFO_ADDR;

// full verify: info page fields + whole-image CRC (nibble CRC over 25 KB is
// a few ms — affordable on every boot)
static bool info_valid(void) {
  if (g_info->magic != BL_INFO_MAGIC) return false;
  if (g_info->base != BL_APP_BASE) return false;
  if (g_info->len == 0 || g_info->len > (BL_APP_END - BL_APP_BASE)) return false;
  if (g_info->crc == 0xFFFF) return false;
  if (g_info->crc != crc16_usb((const uint8_t *)BL_APP_BASE,
                               (uint16_t)(g_info->len & 0xFFFF))) return false;
  return true;
}

static bool app_vectors_ok(void) {
  uint32_t sp = *(const volatile uint32_t *)BL_APP_BASE;
  uint32_t pc = *(const volatile uint32_t *)(BL_APP_BASE + 4UL);
  if (sp < 0x20000100UL || sp > 0x20001800UL) return false;
  if (pc < BL_APP_BASE || pc >= BL_APP_END) return false;
  return true;
}

static bool app_valid(void) {
  return info_valid() && app_vectors_ok();
}

// ---------------------------------------------------------------------------
// app jump

static void bl_jump(void) {
  uint32_t sp = *(const volatile uint32_t *)BL_APP_BASE;
  uint32_t pc = *(const volatile uint32_t *)(BL_APP_BASE + 4UL);

  NVIC_DisableIRQ(USB_IRQn);
  __disable_irq();
  // VTOR is present on F042 but not modelled in generic core_cm0.h (ARMv6-M
  // optional) — write the register directly
  *(volatile uint32_t *)0xE000ED08UL = BL_APP_BASE;
  __set_MSP(sp);
  __ISB();
  void (*f)(void) = (void (*)(void))pc;
  f();
  while (1) { }
}

// ---------------------------------------------------------------------------
// UID (identical hash to Core/Src/uid.c; F0 96-bit UID at 0x1FFFF7AC)

static uint32_t mix32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dUL;
  x ^= x >> 15;
  x *= 0x846ca68bUL;
  x ^= x >> 16;
  return x;
}

static uint32_t uid32(void) {
  uint32_t h = 0x811C9DC5UL;
  h ^= *(uint32_t *)0x1FFFF7ACUL; h = mix32(h);
  h ^= *(uint32_t *)0x1FFFF7B0UL; h = mix32(h);
  h ^= *(uint32_t *)0x1FFFF7B4UL; h = mix32(h);
  return h;
}

static const char bl_hexch[] = "0123456789abcdef";

// non-static: bl_descriptors.c externs this for the USB serial number
void uid_string(char buf[9]) {
  uint32_t v = uid32();
  for (int i = 7; i >= 0; i--) { buf[i] = bl_hexch[v & 0xF]; v >>= 4; }
  buf[8] = 0;
}

// ---------------------------------------------------------------------------
// CDC plumbing

// startup vector table's USB_IRQHandler weak-points to Default_Handler — a
// wedge on the first USB interrupt; wire it to TinyUSB like the app does
void USB_IRQHandler(void) {
  tud_int_handler(0);
}

static void bl_tud_ready(void) {
  // USB pin remap: the tracker board wires D-/D+ onto PA9/PA10; the app does
  // this in HAL_MspInit() (__HAL_REMAP_PIN_ENABLE(HAL_REMAP_PA11_PA12)) —
  // without it the PHY stays on PA11/PA12 and enumeration fails
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGCOMPEN;
  SYSCFG->CFGR1 |= SYSCFG_CFGR1_PA11_PA12_RMP;

  // USB clock enable (mirrors app MX_USB_PCD_Init; TinyUSB sees bare clocks)
  RCC->APB1ENR |= RCC_APB1ENR_USBEN;
  NVIC_EnableIRQ(USB_IRQn);
  tusb_init();
}

// noinline: called from ~15 protocol sites; inlining each copy (frame build
// + flush loop) cost ~90 B per site
static __attribute__((noinline))
void bl_resp(uint8_t type, const uint8_t *payload, uint8_t len) {
  uint8_t f[FRAME_MAX];
  f[0] = 0xC6; f[1] = 0x6C;
  f[2] = type; f[3] = len;
  if (len && payload) memcpy(&f[4], payload, len);
  uint16_t c = crc16_usb(&f[2], (uint16_t)(2 + len));
  f[4 + len] = (uint8_t)(c & 0xFF);
  f[5 + len] = (uint8_t)(c >> 8);

  if (tud_cdc_connected()) {
    tud_cdc_write(f, 6 + len);
    tud_cdc_write_flush();
  }
  for (int i = 0; i < 200; i++) bl_usbp();   // let the final IN go out
}

static __attribute__((noinline))
void bl_err(uint8_t code, uint8_t extra) {
  uint8_t p[2];
  p[0] = code;
  p[1] = extra;
  bl_resp(0xFE, p, 2);
}

// ---------------------------------------------------------------------------
// protocol

static uint8_t  frm_buf[FRAME_MAX];
static uint8_t  frm_type, frm_len;
static uint32_t frm_have;

// per-frame handlers defined below the parser
static void bl_fr_info(void);
static void bl_fr_erase(void);
static void bl_fr_write(const uint8_t *pl);
static void bl_fr_final(const uint8_t *pl);
static void bl_fr_go(void);

static void bl_handle_frame(void) {
  const uint8_t *pl = &frm_buf[4];   // payload
  const uint8_t t = frm_type;

  if      (t == FR_INFO)   bl_fr_info();
  else if (t == FR_ERASE)  bl_fr_erase();
  else if (t == FR_WRITE)  bl_fr_write(pl);
  else if (t == FR_FINAL)  bl_fr_final(pl);
  else if (t == FR_GO)     bl_fr_go();
  else if (t == FR_RESET)  { bl_resp(FR_RESET | 0x80, NULL, 0); NVIC_SystemReset(); }
  else                     bl_err(BL_ERR_TYPE, t);
}

// per-frame handlers (split out of the dispatcher: one shared copy each)
static void bl_fr_info(void) {
  uint8_t out[16];
  char uid[9];
  uid_string(uid);
  for (int i = 0; i < 16; i++) out[i] = 0;
  out[0] = BL_PROTO_VERSION;
  out[1] = BL_VERSION;
  out[2] = BL_REVISION;
  out[3] = (uint8_t)(g.appValid ? 0x01 : 0x00);
  memcpy(&out[4], uid, 8);
  uint32_t bid = info_valid() ? g_info->buildId : 0;
  memcpy(&out[12], &bid, 4);
  bl_resp(FR_INFO | 0x80, out, sizeof(out));
}

static void bl_fr_erase(void) {
  uint16_t pages = 0;
  bool ok = true;
  bl_flash_unlock();
  for (uint32_t a = BL_APP_BASE; a < BL_APP_END && ok; a += PAGE_SIZE) {
    ok = bl_page_erase(a);
    if (ok) pages++;
  }
  if (ok) {
    ok = bl_page_erase(BL_INFO_ADDR);
    if (ok) pages++;
  }
  bl_clear_flash_err();
  if (!ok) { g.armed = false; bl_err(BL_ERR_FLASH, 0); return; }
  g.armed = true;
  g.next = BL_APP_BASE;
  g.crc = 0xFFFF;
  g.written = 0;
  g.finalized = false;
  uint8_t sw[2] = { (uint8_t)pages, (uint8_t)(pages >> 8) };
  bl_resp(FR_ERASE | 0x80, sw, 2);
}

static void bl_fr_write(const uint8_t *pl) {
  if (!g.armed) { bl_err(BL_ERR_ADDR, 0); return; }
  if (frm_len < 6) { bl_err(BL_ERR_LEN, 0); return; }
  uint32_t addr;
  memcpy(&addr, pl, 4);
  uint8_t dl = (uint8_t)(frm_len - 4);
  if (addr != g.next)         { g.armed = false; bl_err(BL_ERR_ADDR, 1); return; }
  if (dl & 1)                 { bl_err(BL_ERR_LEN, 1); return; }
  if (addr + dl > BL_APP_END) { bl_err(BL_ERR_ADDR, 2); return; }
  for (uint32_t i = 0; i < dl; i += 2) {
    uint16_t hw = (uint16_t)(pl[4 + i] | (pl[5 + i] << 8));
    if (!bl_word_program(addr + i, hw)) {
      bl_clear_flash_err();
      g.armed = false;
      bl_err(BL_ERR_FLASH, 1);
      return;
    }
  }
  bl_clear_flash_err();
  g.crc = crc16_cont(g.crc, &pl[4], dl);
  g.next = addr + dl;
  g.written += dl;
  bl_resp(FR_WRITE | 0x80, NULL, 0);   // addr echo dropped: host tracks next
}

static void bl_fr_final(const uint8_t *pl) {
  if (!g.armed) { bl_err(BL_ERR_ADDR, 1); return; }
  if (frm_len < 10) { bl_err(BL_ERR_LEN, 2); return; }
  uint32_t len; uint16_t hcr; uint32_t bid;
  memcpy(&len, pl, 4);
  memcpy(&hcr, pl + 4, 2);
  memcpy(&bid, pl + 6, 4);
  if (len == 0 || len > (BL_APP_END - BL_APP_BASE) || len != g.written) {
    bl_err(BL_ERR_LEN, 3);
    return;
  }
  if (hcr != g.crc) { bl_err(BL_ERR_CRC2, 0); return; }
  if (!bl_page_erase(BL_INFO_ADDR)) { bl_err(BL_ERR_FLASH, 2); return; }
  InfoPage ip;
  ip.magic = BL_INFO_MAGIC;
  ip.base = BL_APP_BASE;
  ip.len = len;
  ip.crc = hcr;
  ip.rsvd = 0xFFFF;
  ip.buildId = bid;
  const uint16_t *h = (const uint16_t *)&ip;
  for (uint32_t i = 0; i < sizeof(ip) / 2; i++) {
    if (!bl_word_program(BL_INFO_ADDR + 2 * i, h[i])) {
      bl_clear_flash_err();
      bl_err(BL_ERR_FLASH, 3);
      return;
    }
  }
  bl_clear_flash_err();
  g.finalized = true;
  g.appValid = app_valid();
  bl_resp(FR_FINAL | 0x80, NULL, 0);
}

static void bl_fr_go(void) {
  if (!g.finalized && !g.appValid) { bl_err(BL_ERR_IMAGE, 0); return; }
  if (!app_valid()) { bl_err(BL_ERR_IMAGE, 1); return; }
  uint8_t ack = 0;
  bl_resp(FR_GO | 0x80, &ack, 1);
  bl_jump();
}

// input parser: accumulates bytes into frm_buf, dispatches complete frames
static void bl_parse_tick(void) {
  while (tud_cdc_available()) {
    uint8_t b;
    if (!tud_cdc_read(&b, 1)) break;

    if (frm_have == 0) {
      if (b == 0xC6) { frm_buf[0] = b; frm_have = 1; }
    } else if (frm_have == 1) {
      if (b == 0x6C) { frm_buf[1] = b; frm_have = 2; }
      else if (b == 0xC6) { frm_have = 1; }   // resync
      else frm_have = 0;
    } else if (frm_have == 2) {
      frm_type = b; frm_buf[2] = b; frm_have = 3;
    } else if (frm_have == 3) {
      frm_buf[3] = b; frm_have = 4;
      frm_len = b;
      if (4 + frm_len + 2 > FRAME_MAX) { frm_have = 0; bl_err(BL_ERR_LEN, 4); }
    } else if (frm_have < (uint32_t)(4 + frm_len)) {
      frm_buf[frm_have++] = b;
    } else if (frm_have < (uint32_t)(4 + frm_len + 2)) {
      frm_buf[frm_have++] = b;
      if (frm_have == (uint32_t)(4 + frm_len + 2)) {
        uint16_t c = crc16_usb(&frm_buf[2], (uint16_t)(2 + frm_len));
        uint16_t r = (uint16_t)(frm_buf[4 + frm_len] | (frm_buf[5 + frm_len] << 8));
        if (c != r) {
          uint8_t pe[1];
          pe[0] = BL_ERR_CRC;
          bl_resp(0xFE, pe, 1);
        } else {
          bl_handle_frame();
        }
        frm_have = 0;
      }
    } else {
      frm_have = 0;   // overshoot: resync
    }
  }
}

// ---------------------------------------------------------------------------

// SystemInit stub: this build has no system_stm32f0xx.c — its clock-update
// math pulls libgcc dividers, and the bootloader configures clocks itself.
// Same reason we link with -nostartfiles: our own startup asm does data/bss
// init; _init/_fini/__libc_init_array are stubs so libc's init.c is never
// linked (we have no constructors).
void SystemInit(void) { }
void _init(void) { }
void _fini(void) { }
void __libc_init_array(void) { }

// libc memcpy/memset overrides: TinyUSB's FIFO bulk ops are the only users.
// Local byte-loop versions drop the entire libc_nano memset/memcpy machinery
// (the app build uses libc freely; the BL build cannot afford it).
void *memset(void *dst, int val, unsigned n) {
  unsigned char *d = dst;
  unsigned char c = (unsigned char)val;
  while (n--) *d++ = c;
  return dst;
}

void *memcpy(void *dst, const void *src, unsigned n) {
  unsigned char *d = dst;
  const unsigned char *s = src;
  while (n--) *d++ = *s++;
  return dst;
}

int main(void) {
  // first statement: capture + clear the boot flag before the stack can
  // wander into it (SP = 0x20001800, flag 16 B below)
  bl_bootflag = BL_BOOTFLAG_ADDR;
  BL_BOOTFLAG_ADDR = 0;

  bool forceUpdate = (bl_bootflag == BL_BOOTFLAG_MAGIC);

  // clock: mirror Core/Src/clock.c (HSI48, flash latency 1, USB clock src)
  RCC->CR2 |= RCC_CR2_HSI48ON;
  uint32_t t = 1000000;
  while (!(RCC->CR2 & RCC_CR2_HSI48RDY)) {
    if (--t == 0) break;
  }
  FLASH->ACR |= FLASH_ACR_LATENCY;
  RCC->CFGR |= RCC_CFGR_SW_1;    // SW = 10 -> HSI48
  t = 1000000;
  while (((RCC->CFGR >> RCC_CFGR_SWS_Pos) & 0x3UL) != 0x2UL) {
    if (--t == 0) break;
  }

  bl_tud_ready();

  g.armed = false;
  g.next = BL_APP_BASE;
  g.crc = 0xFFFF;
  g.written = 0;
  g.finalized = false;
  g.appValid = app_valid();

  if (!forceUpdate && g.appValid) {
    bl_jump();
  }

  for (;;) {
    tud_task();
    bl_parse_tick();
  }
}

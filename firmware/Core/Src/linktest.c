// linktest.c — APP_LINKTEST build: comms-band debug firmware.
//
// Purpose: debug the LoRa/RF link with a boring, race-free measurement path.
// Everything that makes v1 hard to debug (GPS/UART2, settings/filesystem,
// LED sequences, event queue + buffer swap, arbitration/discovery) is
// excluded from this build. Both bench units flash the same image; roles and
// radio parameters are assigned at runtime from the USB console, so no
// reflash is needed per experiment.
//
// Commands:
//   VER            build banner
//   GET            current radio config
//   CFG f|s|b|c|d|p|w|i|l|t <val>   freq/SF/BW/CR/power/preamble/sync/IQ/LDRO/CRC
//                  applies immediately (ApplyConfig + restart RX)
//   TX <hex>       one raw burst (no UID prefix), single hex token
//   TXTEST [n] [gap_ms]   stream the 64 B signature pattern (seq in bits 0-1)
//   RX             one-shot blocking receive, hex + RSSI + SNR
//   RXS on|off     continuous receive printing (hex + RSSI + SNR per burst)
//   CAP on|off     capture received pattern bursts into the corruption ring
//   MAP            corruption map over captured frames (per-position XOR)
//   CLEAR          reset ring + counters
//   REG <addr> [n] read n bytes from radio registers
//   WR <addr> <val>  write one radio register byte
//   STAT           event counters
//   RST            radio reset + re-apply config
//   REBOOT         system reset
//
// The test pattern is 64 bytes: bytes 0-1 = burst sequence (LE16), bytes
// 2-63 = known constant (00 FF A5 5A single-bit family ...). CAP stores
// received bursts; MAP XORs positions 2-63 against the pattern and prints
// the per-position XOR masks and hit counts.

#include "linktest.h"
#include "command.h"
#include "tasks.h"
#include "lora.h"
#include "config.h"
#include "usb.h"
#include "app_context.h"
#include "stm32f0xx_hal.h"

#include <string.h>
#include <stdlib.h>

#define LT_PATTERN_LEN 64
#define LT_CAP_MAX     8

// bitflip-friendly payload from index 2: 00 FF, A5 5A, runs, single bits
static const uint8_t LT_PATTERN[LT_PATTERN_LEN] = {
  0x00, 0x00, 0xA5, 0x5A, 0x3C, 0xC3, 0x00, 0xFF,
  0x0F, 0xF0, 0x18, 0xE7, 0x80, 0x7F, 0x01, 0xFE,
  0x40, 0xBF, 0x20, 0xDF, 0x10, 0xE7, 0x08, 0xF7,
  0x04, 0xFB, 0x02, 0xFD, 0x01, 0x36, 0xC9, 0xFF,
  0xFF, 0x99, 0x66, 0xCC, 0x33, 0xF0, 0x0F, 0x81,
  0x7E, 0xBD, 0x42, 0x7F, 0x80, 0xAA, 0x55, 0x0C,
  0xF3, 0xDA, 0x25, 0xE0, 0x1F, 0x87, 0x78, 0x3F,
  0xC0, 0x24, 0xDB, 0x60, 0x9F, 0xE0, 0x08, 0xF2
};

typedef struct {
  uint32_t tick;
  uint8_t  len;
  int8_t   rssi;
  int8_t   snr;
  uint8_t  data[LT_PATTERN_LEN];
} LTCap;

static LTCap     lt_caps[LT_CAP_MAX];
static uint8_t   lt_capCount = 0;

static struct {
  uint32_t freq;
  uint8_t  sf, bw, cr, power, preamble;
  uint8_t  crc, ldro, iq;
  uint8_t  syncWord[2];
} lt = {
  .freq = 434000000UL, .sf = LORA_SF9, .bw = LORA_BW_62, .cr = LORA_CR_4_5,
  .power = 22, .preamble = 8, .crc = 0, .ldro = 0, .iq = 0,
  .syncWord = { 0x14, 0x24 },
};

// TX burst scheduler (non-blocking, pattern or custom)
static struct {
  bool     active;
  bool     customMode;
  uint16_t remaining;
  uint16_t gap;
  uint32_t nextAt;
  uint16_t seq;
  bool     inFlight;
  uint8_t  customBuf[LORA_MAX_MSG_LEN];
  uint8_t  customLen;
} lt_tx;

static bool    lt_rxStream = false;
static bool    lt_capture = false;
static LoRa_t   *lt_lora = NULL;
static AppContext_t *lt_app = NULL;

static struct {
  uint32_t rx, printed, txReq, txDone, timeouts;
} lt_stats;

static LoRaRxInfo_t lt_info;
static uint8_t      lt_rxFrameBuf[LORA_MAX_MSG_LEN];

// ---------- small helpers ----------

static const char lt_hex[] = "0123456789ABCDEF";

static void lt_hexbyte(uint8_t b) {
  char out[3] = { lt_hex[b >> 4], lt_hex[b & 0xF], 0 };
  print(out);
}

static int8_t lt_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static void lt_u32(uint32_t v) {
  char buf[11]; int i = 10;
  buf[i] = 0;
  do { buf[--i] = '0' + (v % 10); v /= 10; } while (v);
  print(&buf[i]);
}

static void lt_i8(int8_t v) {
  if (v < 0) { print("-"); lt_u32((uint32_t)(-v)); }
  else       { lt_u32((uint32_t)v); }
}

static void lt_hex16(uint16_t v) {
  lt_hexbyte((uint8_t)(v >> 8)); lt_hexbyte((uint8_t)(v & 0xFF));
}

static int lt_parse_hex(const char *tok, uint8_t *out, int max) {
  int n = 0;
  while (*tok && *(tok + 1) && n < max) {
    int8_t hi = lt_nibble(*tok), lo = lt_nibble(*(tok + 1));
    if (hi < 0 || lo < 0) return n;
    out[n++] = (uint8_t)((hi << 4) | lo);
    tok += 2;
  }
  return n;
}

static AppContext_t *lt_ctx(void *p) { return (AppContext_t *)p; }

// forward decl (defined at bottom)
static void lt_register_commands(void);

static void lt_apply_and_restart(void) {
  LoRa_t *l = lt_lora;
  l->frequency       = lt.freq;
  l->spreadingFactor = lt.sf;
  l->bandwidth       = lt.bw;
  l->codingRate      = lt.cr;
  l->preambleLength  = lt.preamble;
  l->crcEnabled      = lt.crc;
  l->txPower         = (LoRaTxPower_t)lt.power;
  l->ldroEnabled     = lt.ldro;

  LoRa_ApplyConfig(l, lt.freq, lt.sf, lt.bw);
  LoRa_SetSyncWord(l, lt.syncWord[0], lt.syncWord[1]);
  if (lt.iq) {
    // invert IQ at both TX (bit2) and RX (bit0) sides of reg 0x0033
    uint8_t v = LoRa_ReadRegisterDbg(l, 0x0033);
    v |= 0x04 | 0x01;
    LoRa_WriteRegisterDbg(l, 0x0033, v);
  } else {
    uint8_t v = LoRa_ReadRegisterDbg(l, 0x0033);
    v &= (uint8_t)~0x05;
    LoRa_WriteRegisterDbg(l, 0x0033, v);
  }
  LoRa_Receive(l, 5000);
}

// ---------- commands ----------

static void lt_ver(void *parameter) {
  print("LT v1 " __DATE__ " " __TIME__);
}

static void lt_get(void *parameter) {
  print("f=");  lt_u32(lt.freq);
  print(" s="); lt_u32(lt.sf);
  print(" b=0x"); lt_hexbyte(lt.bw);
  print(" c="); lt_u32(lt.cr);
  print(" d="); lt_u32(lt.power);
  print(" p="); lt_u32(lt.preamble);
  print(" w="); lt_hex16(((uint16_t)lt.syncWord[0] << 8) | lt.syncWord[1]);
  print(" i="); lt_u32(lt.iq);
  print(" l="); lt_u32(lt.ldro);
  print(" t="); lt_u32(lt.crc);
  print("\n");
}

static void lt_cfg(void *parameter) {
  char *key = cmd_get_param();
  char *val = cmd_get_param();
  if (!key || !val) { print("ERR"); return; }

  uint32_t v = (uint32_t)strtoul(val, NULL, 0);
  bool ok = true;
  switch (key[0]) {
    case 'f': if (v >= 300000000UL && v <= 700000000UL) lt.freq = v; else ok = false; break;
    case 's': if (v >= 5 && v <= 12) lt.sf = (uint8_t)v; else ok = false; break;
    case 'b': if (v <= 10) lt.bw = (uint8_t)v; else ok = false; break;
    case 'c': if (v >= 1 && v <= 4) lt.cr = (uint8_t)v; else ok = false; break;
    case 'd': if (v <= 22) lt.power = (uint8_t)v; else ok = false; break;
    case 'p': if (v >= 6 && v <= 65535) lt.preamble = (uint8_t)v; else ok = false; break;
    case 'w': {
      uint8_t sw[2];
      if (lt_parse_hex(val, sw, 2) == 2) {
        lt.syncWord[0] = sw[0]; lt.syncWord[1] = sw[1];
        if (*((uint16_t *)sw) == 0) ok = false;    // reject 0000
      } else ok = false;
      break;
    }
    case 'i': if (v <= 1) lt.iq = (uint8_t)v; else ok = false; break;
    case 'l': if (v <= 1) lt.ldro = (uint8_t)v; else ok = false; break;
    case 't': if (v <= 1) lt.crc = (uint8_t)v; else ok = false; break;
    default: ok = false;
  }
  if (!ok) { print("ERR"); return; }

  lt_apply_and_restart();
  print("OK");
}

static void lt_tx_cmd(void *parameter) {
  char *hex = cmd_get_param();
  if (!hex) { print("ERR"); return; }
  uint8_t buf[LORA_MAX_MSG_LEN];
  int n = lt_parse_hex(hex, buf, sizeof(buf));
  if (n == 0) { print("ERR"); return; }
  LoRa_Transmit(lt_lora, buf, (uint8_t)n);
  lt_stats.txReq++;
  print("OK");
}

static void lt_txtest(void *parameter) {
  char *nstr = cmd_get_param();
  char *gstr = cmd_get_param();
  uint16_t n = nstr ? (uint16_t)atoi(nstr) : 10;
  uint16_t g = gstr ? (uint16_t)atoi(gstr) : 300;
  if (n == 0) n = 10;
  lt_tx.active = true;
  lt_tx.customMode = false;
  lt_tx.remaining = n;
  lt_tx.gap = g;
  lt_tx.nextAt = HAL_GetTick();
  lt_tx.inFlight = false;
  print("OK");
}

static void lt_rx_cmd(void *parameter) {
  LoRa_t *l = lt_lora;
  for (uint32_t waited = 0; waited < 5000; waited += 8) {
    if (l->events & LORA_EVENT_RX_DONE) break;
    HAL_Delay(8);
  }
  if (!(l->events & LORA_EVENT_RX_DONE)) {
    print("TIMEOUT");
    l->events = 0;
    LoRa_Receive(l, 5000);
    return;
  }
  uint8_t len = LoRa_ReadPacket(l, lt_rxFrameBuf, LORA_MAX_MSG_LEN, &lt_info);
  l->events = 0;
  LoRa_Receive(l, 5000);

  lt_stats.rx++;
  print("RSSI="); lt_i8(lt_info.rssi);
  print(" SNR="); lt_i8(lt_info.snr);
  print(" LEN="); lt_u32(len);
  print(" HEX=");
  for (int i = 0; i < len; i++) { lt_hexbyte(lt_rxFrameBuf[i]); print(" "); }
  print("\n");
}

static void lt_rxs(void *parameter) {
  char *v = cmd_get_param();
  if (!v) { print("ERR"); return; }
  if ((v[0] == 'o' && v[1] == 'n') || v[0] == '1') { lt_rxStream = true; print("ON"); }
  else { lt_rxStream = false; print("OFF"); }
}

static void lt_cap(void *parameter) {
  char *v = cmd_get_param();
  if (!v) { print("ERR"); return; }
  if ((v[0] == 'o' && v[1] == 'n') || v[0] == '1') {
    lt_capture = true; print("ON");
  } else { lt_capture = false; print("OFF"); }
}

static void lt_map(void *parameter) {
  if (lt_capCount == 0) { print("MAP n=0"); return; }

  uint8_t  posXor[LT_PATTERN_LEN] = {0};
  uint16_t posHit[LT_PATTERN_LEN] = {0};
  int8_t rssiMin = 127, rssiMax = -128, snrMin = 127, snrMax = -128;
  uint32_t cleanCount = 0, mismatchCount = 0, corruptCount = 0;
  uint8_t n = lt_capCount > LT_CAP_MAX ? LT_CAP_MAX : lt_capCount;

  for (uint8_t i = 0; i < n; i++) {
    LTCap *c = &lt_caps[i];
    if (c->rssi < rssiMin) rssiMin = c->rssi;
    if (c->rssi > rssiMax) rssiMax = c->rssi;
    if (c->snr < snrMin) snrMin = c->snr;
    if (c->snr > snrMax) snrMax = c->snr;
    if (c->len != LT_PATTERN_LEN) { mismatchCount++; continue; }
    uint8_t burstCorrupt = 0;
    for (int p = 2; p < LT_PATTERN_LEN; p++) {
      uint8_t x = (uint8_t)(c->data[p] ^ LT_PATTERN[p]);
      if (x) { posXor[p] |= x; posHit[p]++; burstCorrupt++; }
    }
    if (burstCorrupt == 0) cleanCount++; else corruptCount++;
  }

  print("MAP n="); lt_u32(lt_capCount);
  print(" clean="); lt_u32(cleanCount);
  print(" corrupt="); lt_u32(corruptCount);
  print(" mismatch="); lt_u32(mismatchCount);
  print(" rssi="); lt_i8(rssiMin); print(".."); lt_i8(rssiMax);
  print(" snr="); lt_i8(snrMin); print(".."); lt_i8(snrMax);
  print("\n");
  for (int p = 2; p < LT_PATTERN_LEN; p++) {
    if (posHit[p] == 0) continue;
    print("pos "); lt_u32(p);
    print(" xor=0x"); lt_hexbyte(posXor[p]);
    print(" hits="); lt_u32(posHit[p]);
    print("\n");
  }
}

static void lt_clear(void *parameter) {
  lt_capCount = 0; lt_capture = false;
  memset(&lt_stats, 0, sizeof(lt_stats));
  print("OK");
}

static void lt_reg_read(void *parameter) {
  char *astr = cmd_get_param();
  char *nstr = cmd_get_param();
  if (!astr) { print("ERR"); return; }
  uint16_t addr = (uint16_t)strtoul(astr, NULL, 16);
  int n = nstr ? atoi(nstr) : 1;
  if (n <= 0 || n > 8) n = 1;
  print("REG 0x"); lt_hex16(addr);
  print(":");
  for (int i = 0; i < n; i++) {
    print(" "); lt_hexbyte(LoRa_ReadRegisterDbg(lt_lora, addr + i));
  }
  print("\n");
}

static void lt_reg_write(void *parameter) {
  char *astr = cmd_get_param();
  char *vstr = cmd_get_param();
  if (!astr || !vstr) { print("ERR"); return; }
  uint16_t addr = (uint16_t)strtoul(astr, NULL, 16);
  uint8_t val = (uint8_t)strtoul(vstr, NULL, 16);
  LoRa_WriteRegisterDbg(lt_lora, addr, val);
  print("OK");
}

static void lt_stat(void *parameter) {
  print("rx=");    lt_u32(lt_stats.rx);
  print(" printed="); lt_u32(lt_stats.printed);
  print(" txreq=");   lt_u32(lt_stats.txReq);
  print(" txdone=");  lt_u32(lt_stats.txDone);
  print(" to=");      lt_u32(lt_stats.timeouts);
  print(" cap=");     lt_u32(lt_capCount);
  if (lt_tx.active) {
    print(" txtest: "); lt_u32(lt_tx.remaining);
    print(" left gap="); lt_u32(lt_tx.gap);
  } else print(" txtest: off");
  print("\n");
}

static void lt_reset(void *parameter) {
  LoRa_Reset(lt_lora);
  lt_apply_and_restart();
  print("OK");
}

static void lt_reboot(void *parameter) {
  HAL_NVIC_SystemReset();
}

// ---------- main-loop services ----------

void linktest_tick(void) {
  task_usb(lt_app);   // USB pump + command line parse

  LoRa_t *l = lt_lora;

  // atomic-ish event drain (byte reads atomic on M0)
  uint8_t ev = l->events;
  l->events = 0;

  if (ev & LORA_EVENT_RX_DONE) {
    uint8_t len = LoRa_ReadPacket(l, lt_rxFrameBuf, LORA_MAX_MSG_LEN, &lt_info);
    lt_stats.rx++;
    if (len > 0) {
      lt_stats.printed++;
      if (lt_rxStream) {
        print("[RX ");
        lt_u32(HAL_GetTick());
        print(" rssi="); lt_i8(lt_info.rssi);
        print(" snr=");  lt_i8(lt_info.snr);
        print(" len=");  lt_u32(len);
        print(" hex=");
        for (int j = 0; j < len; j++) { lt_hexbyte(lt_rxFrameBuf[j]); print(" "); }
        print("\n");
      }
      if (lt_capture) {
        LTCap *c = &lt_caps[lt_capCount % LT_CAP_MAX];
        c->tick = HAL_GetTick();
        c->len = len;
        c->rssi = lt_info.rssi;
        c->snr = lt_info.snr;
        int cp = len < (int)sizeof(c->data) ? len : (int)sizeof(c->data);
        memcpy(c->data, lt_rxFrameBuf, cp);
        lt_capCount++;
      }
    }
    LoRa_Receive(l, 5000);
  }

  if (ev & LORA_EVENT_TIMEOUT) {
    lt_stats.timeouts++;
    LoRa_Receive(l, 5000);
  }

  if (ev & LORA_EVENT_TX_DONE) {
    lt_stats.txDone++;
    if (lt_tx.inFlight) {
      lt_tx.inFlight = false;
      lt_tx.nextAt = HAL_GetTick() + lt_tx.gap;
    }
    LoRa_Receive(l, 5000);   // restart RX after any TX (harmless on TX-only role)
  }

  if (lt_tx.active && !lt_tx.inFlight && lt_tx.remaining > 0 &&
      (int32_t)(HAL_GetTick() - lt_tx.nextAt) >= 0) {
    uint8_t buf[LT_PATTERN_LEN];
    buf[0] = (uint8_t)lt_tx.seq;
    buf[1] = (uint8_t)(lt_tx.seq >> 8);
    if (lt_tx.customMode) {
      memcpy(&buf[2], &lt_tx.customBuf[2], LT_PATTERN_LEN - 2);
    } else {
      memcpy(&buf[2], &LT_PATTERN[2], LT_PATTERN_LEN - 2);
    }
    LoRa_Transmit(l, buf, LT_PATTERN_LEN);
    lt_stats.txReq++;
    lt_tx.seq++;
    lt_tx.remaining--;
    lt_tx.inFlight = true;
    if (lt_tx.remaining == 0) lt_tx.active = false;
  }
}

void linktest_boot(AppContext_t *context) {
  lt_app = context;
  lt_lora = context->lora;
  lt_apply_and_restart();
  lt_register_commands();
  cmd_set_print_function(print);
  lt_ver(NULL);
  print("\n");
  lt_get(NULL);
}

static void lt_register_commands(void) {
  cmd_add("VER",    lt_ver, NULL);
  cmd_add("GET",    lt_get, NULL);
  cmd_add("CFG",    lt_cfg, NULL);
  cmd_add("TX",     lt_tx_cmd, NULL);
  cmd_add("TXTEST", lt_txtest, NULL);
  cmd_add("RX",     lt_rx_cmd, NULL);
  cmd_add("RXS",    lt_rxs, NULL);
  cmd_add("CAP",    lt_cap, NULL);
  cmd_add("MAP",    lt_map, NULL);
  cmd_add("CLEAR",  lt_clear, NULL);
  cmd_add("REG",    lt_reg_read, NULL);
  cmd_add("WR",     lt_reg_write, NULL);
  cmd_add("STAT",   lt_stat, NULL);
  cmd_add("RST",    lt_reset, NULL);
  cmd_add("REBOOT", lt_reboot, NULL);
}

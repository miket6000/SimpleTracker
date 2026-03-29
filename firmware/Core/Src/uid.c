#include <stdint.h>
#include "main.h"

static const char hex_chars[] = "0123456789abcdef";

static uint32_t mix32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

static uint32_t uid96_to_32(uint32_t uid0, uint32_t uid1, uint32_t uid2)
{
    uint32_t h = 0x811C9DC5;   // arbitrary seed (optional)

    h ^= uid0;
    h = mix32(h);

    h ^= uid1;
    h = mix32(h);

    h ^= uid2;
    h = mix32(h);

    return h;
}

static void uint32_to_hex(uint32_t val, char *buf)
{
    for (int i = 7; i >= 0; i--) {
        buf[i] = hex_chars[val & 0x0F];
        val >>= 4;
    }
    buf[8] = '\0';
}

uint32_t UID_Get(void) {
  return uid96_to_32(HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());
}

char *UID_GetStr(void) {
  static char buf[9];
  uint32_to_hex(UID_Get(), buf);
  return buf;
}

void UID_GetStrTo(char *buf) {
  uint32_to_hex(UID_Get(), buf);
}

#include "lora_discovery.h"
#include "stm32f0xx_hal.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
  char     uids[DISCOVERY_MAX_RESPONSES][DISCOVERY_UID_LEN + 1];
  int16_t  rssi[DISCOVERY_MAX_RESPONSES];
  uint8_t  count;
  bool     active;
  uint32_t startTime;
} DiscoveryState_t;

static DiscoveryState_t discovery = { .count = 0, .active = false };

void discovery_start(void) {
  discovery.count = 0;
  discovery.active = true;
  discovery.startTime = HAL_GetTick();
}

bool discovery_add_response(const char *uid, int16_t rssi) {
  if (!discovery.active) return false;

  // Check for duplicate
  for (uint8_t i = 0; i < discovery.count; i++) {
    if (strncmp(discovery.uids[i], uid, DISCOVERY_UID_LEN) == 0) {
      return false;  // Already have this device
    }
  }

  // Add if space available
  if (discovery.count >= DISCOVERY_MAX_RESPONSES) {
    return false;
  }

  strncpy(discovery.uids[discovery.count], uid, DISCOVERY_UID_LEN);
  discovery.uids[discovery.count][DISCOVERY_UID_LEN] = '\0';
  discovery.rssi[discovery.count] = rssi;
  discovery.count++;
  return true;
}

bool discovery_is_complete(void) {
  if (!discovery.active) return false;
  return (HAL_GetTick() - discovery.startTime) >= DISCOVERY_WINDOW_MS;
}

bool discovery_is_active(void) {
  return discovery.active;
}

uint8_t discovery_get_count(void) {
  return discovery.count;
}

void discovery_get_results(char *outBuffer, uint16_t maxLen) {
  // Mark discovery as finished
  discovery.active = false;

  // Format: "<count> <uid1>,<rssi1> <uid2>,<rssi2> ..."
  char tmp[8];
  itoa(discovery.count, tmp, 10);
  strncpy(outBuffer, tmp, maxLen);

  for (uint8_t i = 0; i < discovery.count; i++) {
    strncat(outBuffer, " ", maxLen - strlen(outBuffer) - 1);
    strncat(outBuffer, discovery.uids[i], maxLen - strlen(outBuffer) - 1);
    strncat(outBuffer, ",", maxLen - strlen(outBuffer) - 1);
    itoa(discovery.rssi[i], tmp, 10);
    strncat(outBuffer, tmp, maxLen - strlen(outBuffer) - 1);
  }
}

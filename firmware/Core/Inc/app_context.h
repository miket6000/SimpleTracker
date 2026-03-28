#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include <stdbool.h>
#include "led.h"
#include "lora.h"
#include "usb.h"

#define UID_STR_LENGTH        9
#define MODE_TRACKER          1
#define MODE_GROUND_STATION   2

typedef struct {
  uint32_t uid;
  char uidStr[UID_STR_LENGTH];
  uint32_t remoteUid;
  char remoteUidStr[UID_STR_LENGTH];
  bool gpsFix;
  LedHandle *led;
  LoRa_t *lora;
  char *lastGpsSentence;
  char *lastLoraMessage;
  int16_t rssi;
  uint8_t loraRxLen;
  uint8_t mode;
  usb_state_t *usb;
  uint16_t voltage;
  // Pending LoRa config (applied after ACK TX completes)
  uint32_t pendingFreq;
  uint8_t  pendingSF;
  uint8_t  pendingBW;
  bool     pendingConfigSwitch;
} AppContext_t;

#endif // APP_CONTEXT_H

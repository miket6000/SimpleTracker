#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include <stdbool.h>
#include "led.h"
#include "lora.h"


#define UID_STR_LENGTH    9
#define MODE_TRANSMIT     1
#define MODE_RECEIVE      2

typedef struct {
  uint32_t uid;
  char uidStr[UID_STR_LENGTH];
  bool gpsFix;
  LedHandle *gpsLed;
  LedHandle *loraLed;
  LoRa *lora;
  char *lastGpsSentence;
  char *lastLoraMessage;
  int16_t rssi;
  uint8_t mode;
} AppContext_t;

#endif // APP_CONTEXT_H

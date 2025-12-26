#ifndef SETTING_H
#define SETTING_H
#include "main.h"
#include "lora.h"

#define MAX_SETTING_LIST_SIZE 10

#define DEFAULT_FREQ      434
#define DEFAULT_SF        SF_9
#define DEFAULT_BW        BW_62_5KHz
#define DEFAULT_CRC       CR_4_5
#define DEFAULT_POWER     POWER_20db
#define DEFAULT_OC        150
#define DEFAULT_PREAMBLE  8
#define DEFAULT_MODE    1

typedef struct {
  char label;
  uint32_t value;
  uint32_t initial;
} Setting;

Setting **get_settings();
void setting_reset();
Setting *setting(char label);

#endif // SETTING_H

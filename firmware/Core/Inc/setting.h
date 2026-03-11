#ifndef SETTING_H
#define SETTING_H
#include "main.h"
#include "lora.h"
#include "app_context.h"

#define MAX_SETTING_LIST_SIZE 10

#define DEFAULT_FREQ      434000000UL
#define DEFAULT_SF        LORA_SF9
#define DEFAULT_BW        LORA_BW_125
#define DEFAULT_CR        LORA_CR_4_5
#define DEFAULT_POWER     LORA_TX_POWER_22_DBM
#define DEFAULT_OC        150
#define DEFAULT_PREAMBLE  8
#define DEFAULT_MODE      MODE_TRACKER

typedef struct {
  char label;
  uint32_t value;
  uint32_t initial;
} Setting;

Setting **get_settings();
void setting_reset();
Setting *setting(char label);

#endif // SETTING_H

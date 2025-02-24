#include "record.h"

/*
  A setting is identified by a lower case character. 
  All settings are stored as a 32bit number.
*/

static Setting mode_setting = {'m', DEFAULT_MODE,    DEFAULT_MODE};
static Setting freq_setting = {'f', DEFAULT_FREQ,    DEFAULT_FREQ};

Setting *settingList[] = {
  &mode_setting,
  &freq_setting,
  NULL
};

Setting **get_settings() {
  return settingList;
}

void setting_reset() {
  uint8_t i = 0;
  while (settingList[i] != NULL) {
    settingList[i]->value = settingList[i]->initial;
    i++;
  }
}

Setting *setting(char label) {
  uint8_t i = 0;
  while (settingList[i] != NULL) {
    if (settingList[i]->label == label) {
      return settingList[i];
    }
    i++;
  }
  return NULL;
}

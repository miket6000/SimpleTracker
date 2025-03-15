#include "setting.h"

/*
  A setting is identified by a lower case character. 
  All settings are stored as a 32bit number, it is up to the user to cast as required
  If a setting is not found, the default value is returned instead.
*/

//                                     LBL, DEFAULT_VALUE,    FACTORY_VALUE
static Setting freq_setting         = {'f', DEFAULT_FREQ,     DEFAULT_FREQ};
static Setting sf_setting           = {'s', DEFAULT_SF,       DEFAULT_SF};
static Setting bandwidth_setting    = {'b', DEFAULT_BW,       DEFAULT_BW};
static Setting crc_setting          = {'c', DEFAULT_CRC,      DEFAULT_CRC};
static Setting power_setting        = {'d', DEFAULT_POWER,    DEFAULT_POWER};
static Setting overcurrent_setting  = {'o', DEFAULT_OC,       DEFAULT_OC};
static Setting preamble_setting     = {'p', DEFAULT_PREAMBLE, DEFAULT_PREAMBLE};
static Setting mode_setting         = {'m', DEFAULT_MODE,     DEFAULT_MODE};

Setting *settingList[] = {
  &freq_setting,
  &sf_setting,
  &bandwidth_setting,
  &crc_setting,
  &power_setting,
  &overcurrent_setting,
  &preamble_setting,
  &mode_setting,
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

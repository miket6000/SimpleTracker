#ifndef RECORD_H
#define RECORD_H
#include "main.h"

#define MAX_SETTING_LIST_SIZE 10

#define DEFAULT_MODE    1
#define DEFAULT_FREQ    434

typedef struct {
  char label;
  uint32_t value;
  uint32_t initial;
} Setting;

Setting **get_settings();
void setting_reset();
Setting *setting(char label);

#endif // RECORD_H

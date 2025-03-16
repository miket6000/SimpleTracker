#include "main.h"
#include "gps.h"
#include "command.h"
#include "commands.h"
#include "setting.h"
#include "filesystem.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

void print_uint32(void *parameter) {
  char buffer[8];
  itoa(*(uint32_t *)parameter, buffer, 10);
  print(buffer, strlen(buffer));
}

void print_int32(void *parameter) {
  char buffer[8];
  itoa(*(int32_t *)parameter, buffer, 10);
  print(buffer, strlen(buffer));
}

void print_uint16(void *parameter) {
  char buffer[8];
  itoa(*(uint16_t *)parameter, buffer, 10);
  print(buffer, strlen(buffer));
}

void print_int16(void *parameter) {
  char buffer[8];
  itoa(*(int16_t *)parameter, buffer, 10);
  print(buffer, strlen(buffer));
}

void print_uint8(void *parameter) {
  char buffer[8];
  itoa(*(uint8_t *)parameter, buffer, 10);
  print(buffer, strlen(buffer));
}

void print_int8(void *parameter) {
  char buffer[8];
  itoa(*(int8_t *)parameter, buffer, 10);
  print(buffer, strlen(buffer));
}

void reboot(void *parameter) {
  HAL_NVIC_SystemReset();
}

void set_report(void *parameter) {
   *(bool *)parameter = true;
}

void unset_report(void *parameter) {
   *(bool *)parameter = false; 
}  

void write_gps(void *parameter) {
  //Buffer *buffer = (Buffer *)parameter;
  char *param = cmd_get_param();
  if (param != NULL) {
//    gps_write((uint8_t *)param, strlen(param));
  } 
}

void read_gps(void *parameter) {
  char *nmea = (char *)gps_read()->data;
  uint8_t len = gps_read()->index;
  print(nmea, len);
}

void set_config(void *parameter) {
  char *label = cmd_get_param();
  uint32_t value = atoi(cmd_get_param());
  fs_save_config(label[0], &value);
  Setting *s = setting(label[0]);
  if (s != NULL) {
    s->value = value;
    print("OK", 2);
  } else {
    print("ERR", 3);
  }
}

void get_config(void *parameter) {
  char *label = cmd_get_param();
  uint32_t value = 0xFFFFFFFF;
  fs_read_config(label[0], &value);
  char str_buf[10] = {0};
  print(itoa(value, str_buf, 10), strlen(str_buf));
}

void erase_flash(void *parameter) {
  // delete everything
  fs_erase();

  // restore current config so it can be loaded on power up
  uint8_t i = 0;
  Setting **settingList = get_settings();
  while (settingList[i] != NULL) {
    fs_save_config(settingList[i]->label, &settingList[i]->value);
    i++;
  }
  print("OK", 2);
}

void factory_reset(void *parameter) {
  // set the default config, then call erase flash. 
  setting_reset(); 
  erase_flash(NULL);
}

void write_lora(void *parameter) {
}

void read_lora(void *parameter) {
}

void get_uid(void *parameter) {
  uint32_t uid = *((uint32_t *)parameter);
  char str_buf[10] = {0};
  print(itoa(uid, str_buf, 16), strlen(str_buf));
}


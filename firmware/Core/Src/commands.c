#include "usb.h"
#include "gps.h"
#include "command.h"
#include "commands.h"
#include "app_context.h"
#include "setting.h"
#include "filesystem.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

void help(void *prameter) {
  Command *cmd_list = cmd_get_list();
  for (int i = 0; i < cmd_get_num_commands(); i++) {
    print(cmd_list[i].command);
    print(" - ");
    print(cmd_list[i].description);
    print("\n");
  }
}

void print_uint32(void *parameter) {
  char buffer[8];
  itoa(*(uint32_t *)parameter, buffer, 10);
  print(buffer);
}

void print_int32(void *parameter) {
  char buffer[8];
  itoa(*(int32_t *)parameter, buffer, 10);
  print(buffer);
}

void print_uint16(void *parameter) {
  char buffer[8];
  itoa(*(uint16_t *)parameter, buffer, 10);
  print(buffer);
}

void print_int16(void *parameter) {
  char buffer[8];
  itoa(*(int16_t *)parameter, buffer, 10);
  print(buffer);
}

void print_uint8(void *parameter) {
  char buffer[8];
  itoa(*(uint8_t *)parameter, buffer, 10);
  print(buffer);
}

void print_int8(void *parameter) {
  char buffer[8];
  itoa(*(int8_t *)parameter, buffer, 10);
  print(buffer);
}

void print_str(void *parameter) {
  char *buffer = parameter;
  print(buffer);
}

void print_str_ptr(void *parameter) {
  char **buffer_ptr_ptr = parameter;
  print(*buffer_ptr_ptr);
}

void print_remote(void *parameter) {
  AppContext_t *context = parameter;
  print(context->lastLoraMessage);
  print(" ");
  print_int16(&context->rssi);
}

void reboot(void *parameter) {
  HAL_NVIC_SystemReset();
}

void transmit(void *parameter) {
  AppContext_t  *context = parameter;
  char *cmd = cmd_get_param();
  LoRa_Transmit(context->lora, (uint8_t *)cmd, strlen(cmd)); 
}

void set_config(void *parameter) {
  char *label = cmd_get_param();
  Setting *s = setting(label[0]);
  if (s != NULL) {
    uint32_t value = atoi(cmd_get_param());
    s->value = value;
    if (fs_save_config(label[0], &value) == FS_OK) {
      print("OK");
    } else { // flash is full, so erase and try again...
      erase_flash(NULL); // this prints "OK", so we don't need to
      fs_save_config(label[0], &value);
    }
  } else {
    print("ERR"); // invalid label
  }
}

void get_config(void *parameter) {
  char *label = cmd_get_param();
  Setting *s = setting(label[0]);
  if (s != NULL) {
    char str_buf[10] = {0};
    print(itoa(s->value, str_buf, 10));
  } else {
    print("ERR");
  }
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
  print("OK");
}

void factory_reset(void *parameter) {
  // set the default config, then call erase flash and reboot so settings take effect. 
  setting_reset(); 
  erase_flash(NULL);
  reboot(NULL);
}


#include "main.h"
#include "gps.h"
#include "command.h"
#include "commands.h"
#include "record.h"
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

void write_gps(void *parameter) {
  //Buffer *buffer = (Buffer *)parameter;
  char *param = cmd_get_param();
  if (param != NULL) {
//    gps_write((uint8_t *)param, strlen(param));
  } 
}

void read_gps(void *parameter) {
  print((char *)gps_read(), 16);
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


void set_mode(void *parameter) {
  bool *mode = (bool *)parameter;
  char *msg;
  char *param = cmd_get_param();
  if (param[0] == 'R' || param[0] == 'r') {
    *mode = true;
    msg = "RX Mode";
  } else {
    *mode = false;
    msg = "TX Mode"; 
  }
  print(msg, strlen(msg));
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


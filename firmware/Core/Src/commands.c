#include "main.h"
#include "gps.h"
#include "command.h"
#include "commands.h"
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

void write_lora(void *parameter) {
}

void read_lora(void *parameter) {
}

void get_uid(void *parameter) {
  uint32_t uid = *((uint32_t *)parameter);
  char str_buf[10] = {0};
  print(itoa(uid, str_buf, 16), strlen(str_buf));
}


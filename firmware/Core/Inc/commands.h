#ifndef COMMANDS_H
#define COMMANDS_H
#include <stdint.h>

void help(void *prameter);
void print_uint32(void *parameter);
void print_int32(void *parameter);
void print_uint16(void *parameter);
void print_int16(void *parameter);
void print_uint8(void *parameter);
void print_int8(void *parameter);
void print_str(void *parameter);
void print_remote(void *parameter);
void print_str_ptr(void *parameter);
void set_config(void *parameter);
void get_config(void *parameter);
void set_mode(void *parameter);
void erase_flash(void *parameter);
void factory_reset(void *parameter);
void reboot(void *parameter);

#endif // COMMANDS_H

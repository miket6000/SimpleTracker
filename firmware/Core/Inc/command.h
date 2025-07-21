#ifndef COMMAND_H
#define COMMAND_H
#include <stdint.h>

#define COMMAND_LEN 8U

typedef struct {
  char command[COMMAND_LEN];
  void (*callback)(void *);
  void *parameter;
  char *description;
} Command;

Command *cmd_get_list(void);
int cmd_get_num_commands(void);
void cmd_set_interactive(void *parameter);
void cmd_unset_interactive(void *parameter);
void cmd_set_print_function(void(*function)(char *));
void cmd_add(const char *command, void (*callback)(void *), void *parameter);
void cmd_read_input(char *buffer, uint8_t len);
char *cmd_get_param(void);

#endif //COMMAND_H

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>
#include "spi.h"
#include "gpio.h"

typedef enum {
  FS_STOPPED = 0,
  FS_CLEAN,
  FS_DIRTY,
} FSState;

typedef enum {
  FS_OK = 0,
  FS_ERR,
} FSResult;

FSResult fs_init();
FSResult fs_stop();
FSResult fs_flush();
FSResult fs_save(char label, void *data, uint16_t len);
FSResult fs_save_config(char label, void *value);
FSResult fs_read_config(char label, uint32_t *variable);
FSResult fs_raw_read(uint32_t address, uint16_t *buffer, uint16_t len);
FSResult fs_raw_write(uint32_t address, uint16_t *buffer, uint16_t len);
FSResult fs_erase();

#endif //FILESYSTEM_H

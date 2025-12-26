#include "filesystem.h"
#include <stdbool.h>
#include "flash.h"

/* WARNING
 * This code directly accesses the STM32F0 flash, this access MUST be 16bit aligned
 * or the Cortex M0 will hard fault!
 */


#define INDEX_START_ADDRESS     0x000000
#define INDEX_END_ADDRESS       0x000400
#define BLANK       0xFFFFFFFF

static FSState fs_state = FS_STOPPED;
static uint32_t next_free_index = INDEX_START_ADDRESS;
static const uint8_t block_size = 3;

FSResult fs_init() {
  uint16_t data[block_size];

  flash_read(next_free_index, data, block_size);
  while ((next_free_index + block_size) <= INDEX_END_ADDRESS && data[0] != 0xFFFF) {
    next_free_index += block_size * 2;
    flash_read(next_free_index, data, block_size);  
  }
  
  fs_state = FS_CLEAN;
  return FS_OK;
}

FSResult fs_stop() {
  fs_state = FS_STOPPED;
  return FS_OK;
}

FSResult fs_read_config(char label, uint32_t *variable) {
  uint32_t address = INDEX_START_ADDRESS; 
  uint16_t buffer[3] = {0};

  while (address < INDEX_END_ADDRESS && buffer[0] != 0xFFFF) {
    flash_read(address, buffer, block_size);
    if ((char)buffer[0] == label) {
      *variable = *(uint32_t *)&buffer[1];
    }
    address += block_size * 2;
  }
  
  return FS_OK;
}

FSResult fs_save_config(char label, void *data) {
  uint16_t buffer[] = {
    (uint16_t)label, 
    ((uint16_t *)data)[0], 
    ((uint16_t *)data)[1], 
  };
  
  if ((next_free_index + block_size * 2) < INDEX_END_ADDRESS) {
    flash_write(next_free_index, buffer, block_size); // len needs to be the number of halfwords
    next_free_index += block_size * 2;
    return FS_OK;
  }
  return FS_ERR;
}

FSResult fs_raw_read(uint32_t address, uint16_t *buffer, uint16_t len) {
  if (flash_read(address, buffer, len) != Flash_Ok) {
    return FS_ERR;
  }
  return FS_OK;
}

FSResult fs_raw_write(uint32_t address, uint16_t *buffer, uint16_t len) {
  if (flash_write(address, buffer, len) != Flash_Ok) {
    return FS_ERR;
  }
  return FS_OK;
}

FSResult fs_erase() {
  if (flash_erase() != Flash_Ok) {
    return FS_ERR;
  }
  next_free_index = INDEX_START_ADDRESS;

  return FS_OK;
}


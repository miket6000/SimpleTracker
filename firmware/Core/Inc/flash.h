#ifndef FLASH_H
#define FLASH_H
#include <stdint.h>

typedef enum {
    Flash_Ok,     // 0
    Flash_Err,    // 1
    Flash_Timeout // 2
} Flash_Result;

Flash_Result flash_read(uint32_t address, uint16_t *data, uint8_t len);
Flash_Result flash_write(uint32_t address, uint16_t *data, uint8_t len);
Flash_Result flash_erase();

#endif //FLASH_H

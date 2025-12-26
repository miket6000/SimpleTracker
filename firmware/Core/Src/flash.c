#include "flash.h"
#include "stm32f0xx_hal.h"

const uint32_t UserPageAddress = 0x08007C00;

Flash_Result flash_read(uint32_t address, uint16_t *data, uint8_t len) {
  for (int i = 0; i < len; i++) { 
    uint32_t addr = UserPageAddress + address + i * 2;
    data[i] = *((uint16_t *)(addr));
  }

  return Flash_Ok;
}

// Writes 'len' bytes from 'data' to flash starting at the UserPageAddress + 'address'
Flash_Result flash_write(uint32_t address, uint16_t *data, uint8_t len) {
  HAL_FLASH_Unlock();

  for (int i = 0; i < len; i++) {
    uint32_t addr = UserPageAddress + address + i * 2; 
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, data[i]) != HAL_OK)
    {
      /* Error occurred while writing data in Flash memory*/
      return Flash_Err;
    }
  }

  HAL_FLASH_Lock();

  return Flash_Ok;  
}

// Erases the entire UserSector of flash
Flash_Result flash_erase() {
  static FLASH_EraseInitTypeDef EraseInitStruct;
  uint32_t SectorError;

  /* Fill EraseInit structure*/
  EraseInitStruct.TypeErase     = TYPEERASE_PAGES;
  EraseInitStruct.PageAddress   = UserPageAddress;
  EraseInitStruct.NbPages       = 1;
  
  HAL_FLASH_Unlock();

  if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
    return Flash_Err;
  }

  HAL_FLASH_Lock();

  return Flash_Ok;
}

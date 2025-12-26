#include "main.h"
/**
 * @brief System Clock Configuration
 * @retval None
 */

void RCC_Clock_Config(uint8_t divider) {
  uint32_t tickstart;

  MODIFY_REG(RCC->CFGR, RCC_CFGR_PPRE, RCC_HCLK_DIV16);
  MODIFY_REG(RCC->CFGR, RCC_CFGR_HPRE, divider);
  __HAL_RCC_SYSCLK_CONFIG(RCC_SYSCLKSOURCE_HSI48);

  tickstart = HAL_GetTick();

  while (__HAL_RCC_GET_SYSCLK_SOURCE() != (RCC_SYSCLKSOURCE_HSI48 << RCC_CFGR_SWS_Pos)) {
    if((HAL_GetTick() - tickstart ) > CLOCKSWITCH_TIMEOUT_VALUE) {
      Error_Handler();
    }
  }

  MODIFY_REG(RCC->CFGR, RCC_CFGR_PPRE, RCC_HCLK_DIV1);
  SystemCoreClock = HAL_RCC_GetSysClockFreq() >> AHBPrescTable[(RCC->CFGR & RCC_CFGR_HPRE)>> RCC_CFGR_HPRE_BITNUMBER];

  /* Configure the source of time base considering new system clocks settings*/
  HAL_InitTick (TICK_INT_PRIORITY);
}

void RCC_Oscillator_Config() {
  uint32_t tickstart;

  __HAL_RCC_HSI14ADC_DISABLE();
  __HAL_RCC_HSI14_ENABLE();
  tickstart = HAL_GetTick();
  /* Wait till HSI is ready */  
  while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSI14RDY) == RESET) {
    if((HAL_GetTick() - tickstart) > HSI14_TIMEOUT_VALUE) {
      Error_Handler();
    }      
  } 
  __HAL_RCC_HSI14_CALIBRATIONVALUE_ADJUST(16);

  __HAL_RCC_HSI48_ENABLE();
  tickstart = HAL_GetTick();
  /* Wait till HSI48 is ready */  
  while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSI48RDY) == RESET) {
    if((HAL_GetTick() - tickstart) > HSI48_TIMEOUT_VALUE) {
      Error_Handler();
    }
  } 
}


/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_Oscillator_Config();
  RCC_Clock_Config(RCC_SYSCLK_DIV1);

  __HAL_RCC_USB_CONFIG(RCC_USBCLKSOURCE_HSI48);
}



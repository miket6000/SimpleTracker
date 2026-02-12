#include "main.h"
/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    uint32_t timeout;

    /* Enable HSI48 */
    __HAL_RCC_HSI48_ENABLE();

    timeout = 1000000;
    while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSI48RDY) == RESET)
        if(--timeout == 0) Error_Handler();

    /* Set Flash latency */
    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

    /* Switch SYSCLK to HSI48 */
    __HAL_RCC_SYSCLK_CONFIG(RCC_SYSCLKSOURCE_HSI48);

    timeout = 1000000;
    while(__HAL_RCC_GET_SYSCLK_SOURCE() != (RCC_SYSCLKSOURCE_HSI48 << RCC_CFGR_SWS_Pos))
        if(--timeout == 0) Error_Handler();

    SystemCoreClock = 48000000;

    HAL_InitTick(TICK_INT_PRIORITY);

    /* Configure USB clock */
    __HAL_RCC_USB_CONFIG(RCC_USBCLKSOURCE_HSI48);
}


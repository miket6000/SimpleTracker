/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "clock.h"
#include "board.h"
#include "usb.h"
#include "led.h"
#include "led_sequences.h"
#include "app_context.h"
#include "task.h"
#include "tasks.h"
#include "setting.h"
#include "filesystem.h"
#include "command.h"
#include "commands.h"
#include "gps.h"
#include "lora.h"
#include "event.h"
#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
LedHandle status_led;
LoRa hlora;

AppContext_t appContext = {
  .gpsFix = false, 
  .led = &status_led, 
  .lora = &hlora,
  .mode = 0,
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void load_settings() {
  uint8_t i = 0;
  Setting **settingList = get_settings();
  while (settingList[i] != NULL) {
    fs_read_config(settingList[i]->label, &settingList[i]->value);
    i++;
  }
}

void lora_init(LoRa *hlora) {
  *hlora = newLoRa(); 
  hlora->frequency = setting('f')->value;
  hlora->spreadingFactor = setting('s')->value;
  hlora->bandwidth = setting('b')->value;
  hlora->crcRate = setting('c')->value;
  hlora->power = setting('d')->value;
  hlora->overCurrentProtection = setting('o')->value;
  hlora->preamble = setting('p')->value;

  hlora->CS_port = LORA_CS_GPIO_Port;
  hlora->CS_pin = LORA_CS_Pin;
  hlora->reset_port = LORA_RST_GPIO_Port;
  hlora->reset_pin = LORA_RST_Pin;
  hlora->DIO0_port = LORA_DIO1_GPIO_Port;
  hlora->DIO0_pin = LORA_DIO1_Pin;
  hlora->hSPIx = &hspi1;

  if (LoRa_init(hlora) == LORA_OK) {
    print("LoRa Init OK\n");
  } else {
    print("LoRa Init Failed\n"); 
  }

}


/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_USB_DEVICE_Init();
  MX_TIM16_Init();
  /* USER CODE BEGIN 2 */
  /* set up LED sequencer for each of the two LEDs */
  led_init(&status_led, LED_GPIO_Port, LED_Pin);
  led_add_sequence(&status_led, off_sequence);

  /* Initialize flash filesystem and load settings */
  fs_init();
  load_settings();

  /* populate appContext with values that are now available */
  appContext.mode = setting('m')->value;
  appContext.uid = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();
  itoa(appContext.uid, appContext.uidStr, 16);

  /* init command line interpreter */
  cmd_add("REBOOT", reboot, NULL);
  cmd_add("I", cmd_set_interactive, NULL);
  cmd_add("i", cmd_unset_interactive, NULL);
  cmd_add("R", print_remote, &appContext);
  cmd_add("L", print_str_ptr, &appContext.lastGpsSentence);
  cmd_add("SET", set_config, NULL);
  cmd_add("GET", get_config, NULL);
  cmd_add("UID", print_str, &appContext.uidStr);
  cmd_add("ERASE", erase_flash, NULL); 
  cmd_set_print_function(print); 

  /* Architecture description.
   * 
   * Low level drivers are primarily handled by a task scheduler. This includes polling 
   * the LoRa module, flashing LEDs and managing the USB Tx and Rx buffers. These task 
   * must happen in a timely manner for the operation of the tracker.
   *
   * Some task are interrupt driven, most notably the GPS receiver / parser.
   *
   * Higher level logic is taken care of using an event driven framework. For example,
   * the GPS rx interrupt will generate an event if it detects a valid GNGGA packet,
   * which then potentially creates a lora_tx event. 
   *
   *
   */

  /* init lora & gps modules */
  lora_init(&hlora);
  gps_init(&huart2);

  /* create task, arguments are init_delay, period, callback, parameter */
  task_build(0, 15, task_led, &appContext);
  task_build(0, 100, task_lora_rx, &appContext);
  task_build(0, 0, task_usb, USB_getStatePointer());
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // Time dependant task
    task_run();
    eventDispatcher(&appContext);
    //power_management();
  }

  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

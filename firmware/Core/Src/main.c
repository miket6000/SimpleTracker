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
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "led.h"
#include "task.h"
#include "usbd_cdc_if.h"
#include "command.h"
#include "commands.h"
#include "gps.h"
#include "lora.h"
#include <stdbool.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct {
  uint8_t *buffer;
} Context;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UPDATE_PERIOD_MS  2000
#define UID_STR_LENGTH    9
#define HEADER_LEN        UID_STR_LENGTH
#define TX_MESG_LEN       (HEADER_LEN + NMEA_SENTENCE_SIZE)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
bool rx_mode = true;

extern USBD_HandleTypeDef hUsbDeviceFS;
extern uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
extern uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];
uint8_t rx_buffer[APP_RX_DATA_SIZE];
uint16_t tx_buffer_index = 0;
uint16_t rx_buffer_index = 0;

uint32_t uid = 0;
char uid_str[UID_STR_LENGTH];

const int8_t idle_sequence[] = {1, -1};
const int8_t flash_sequence[] = {1, NOTHING, -1};
const int8_t double_flash_sequence[] = {2, NOTHING, -1};
const int8_t off_sequence[] = {NOTHING, -1};

LedHandle led_blue, led_green;
LoRa hlora;
bool usb_connected = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void USBD_CDC_RxHandler(uint8_t *rxBuffer, uint32_t len);
void USB_Connect(void);
void USB_Disconnect(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void print(char *tx_buffer, uint16_t len) {
  // if we are going to overflow the buffer, drop the message entirely.
  if (tx_buffer_index + len < sizeof(UserTxBufferFS)) {
    memcpy(&UserTxBufferFS[tx_buffer_index], tx_buffer, len);
    tx_buffer_index += len;
  }
}

void transmit(char *buffer, uint16_t len) {
  LoRa_transmit(&hlora, (uint8_t *)buffer, len, UPDATE_PERIOD_MS / 2);
}

void task_led(void *param) {
  led_blink(&led_blue);
  led_blink(&led_green); 
}

void task_gps(void *param) {
  Buffer *gps_buffer;
  gps_buffer = gps_read();
  char *fix_status = gps_get_field(gps_buffer, FIX);

  if (gps_buffer->index > 0) {
    if (fix_status != NULL && fix_status[0] == '1') {
      led_add_sequence(&led_blue, double_flash_sequence);
    } else {
      led_add_sequence(&led_blue, flash_sequence);
    }

/*
    print("<- ", 3);
    print(uid_str, strlen(uid_str));
    print(" ", 1);
    print((char *)gps_buffer->data, gps_buffer->index);
*/    
    if (!rx_mode) {
      led_add_sequence(&led_green, double_flash_sequence);
      char tmpBuffer[TX_MESG_LEN];
      strncpy(tmpBuffer, uid_str, UID_STR_LENGTH);
      strncat(tmpBuffer, " ", 2); //include \0
      strncat(tmpBuffer, (char *)gps_buffer->data, gps_buffer->index);

      transmit(tmpBuffer, strlen(tmpBuffer));
    }
    gps_buffer->index = 0; //Mark as transmitted by setting index to zero.
  }
}

void task_lora_rx(void *param) {
  if (rx_mode) {
    uint8_t bytes_received = 0;
    int16_t rssi = 0;
    bytes_received = LoRa_receive(&hlora, (uint8_t *)param, TX_MESG_LEN);
    if (bytes_received > 0) {
      led_add_sequence(&led_green, flash_sequence);
      print("-> ", 3);
      print((char *)param, bytes_received);
      print("RSSI: ", 6);
      rssi = LoRa_getRSSI(&hlora);
      print_int16(&rssi);
      print("\r\n", 2);
    }
  }
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  hlora = newLoRa(); 
  uint8_t lora_rx_buffer[NMEA_SENTENCE_SIZE];
  uint32_t uid;

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
  led_init(&led_blue, LED_BLUE_GPIO_Port, LED_BLUE_Pin);
  led_init(&led_green, LED_GREEN_GPIO_Port, LED_GREEN_Pin);
  led_add_sequence(&led_blue, off_sequence);
//  led_add_sequence(&led_green, idle_sequence);
  
  uid = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();
  itoa(uid, uid_str, 16);

  // init command line interpreter
  cmd_add("REBOOT", reboot, NULL);
  cmd_add("I", cmd_set_interactive, NULL);
  cmd_add("i", cmd_unset_interactive, NULL);
  cmd_add("R", read_gps, NULL);
  cmd_add("W", write_gps, NULL);
  cmd_add("UID", print_uint32, &uid);
  cmd_set_print_function(print); 
  
  hlora.CS_port = CS_LORA_GPIO_Port;
  hlora.CS_pin = CS_LORA_Pin;
  hlora.reset_port = GPIOB;
  hlora.reset_pin = 5;
  hlora.DIO0_port = GPIOB;
  hlora.DIO0_pin = 4;
  hlora.hSPIx = &hspi1;
  
  // According to the datasheet we need to wait 10ms after power on before any SPI 
  // communications with the SX127x. Experience has shown this to be critical.
  HAL_Delay(10);

  if (LoRa_init(&hlora) == LORA_OK) {
    print("LoRa Init OK\n\r", 14);
  } else {
    print("LoRa Init Failed\n\r", 18); 
  }
  
  gps_init(&huart2);
  
  task_build(0, 30, task_led, NULL);
  task_build(0, 100, task_lora_rx, lora_rx_buffer);
  task_build(0, UPDATE_PERIOD_MS, task_gps, NULL);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // Time dependant task
    task_run();

    /* Deal with USB data */
    if (usb_connected) {
      if (rx_buffer_index > 0) {
        //led_add_sequence(&led_green, flash_sequence);
        cmd_read_input((char *)rx_buffer, rx_buffer_index);
        rx_buffer_index = 0;
      }
  
      /* Transmit any data in the output buffer */
      if (tx_buffer_index > 0) {
        //led_add_sequence(&led_green, flash_sequence);
        if (CDC_Transmit_FS(UserTxBufferFS, tx_buffer_index) == USBD_OK) {
          tx_buffer_index = 0;
        }
      }
    }
    //power_management();
  }

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI48;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void USBD_CDC_RxHandler(uint8_t *rxBuffer, uint32_t len) {
  //DANGER - does not check for rx_buffer over run.
  memcpy(&rx_buffer[rx_buffer_index], rxBuffer, len);
  rx_buffer_index += len;
}

void USB_Connect(void) {
  usb_connected = true;
  LoRa_gotoMode(&hlora, RXCONTIN_MODE);
  
  //power_set_mode(AWAKE);
  //RCC_Clock_Config(RCC_SYSCLK_DIV1);
  //HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/100);
}

void USB_Disconnect(void) {
  usb_connected = false;
  LoRa_gotoMode(&hlora, STNBY_MODE);
  
  //power_set_mode(SNOOZE);
  //RCC_Clock_Config(RCC_SYSCLK_DIV4);
  //HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/100);
}


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

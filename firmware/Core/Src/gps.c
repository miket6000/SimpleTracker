#include "gps.h"
#include "usart.h"
#include "main.h"
#include "led.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

extern LedHandle led_green;

Buffer rxBufferA = {{0}, 0};
Buffer rxBufferB = {{0}, 0};
uint8_t rx_char;

Buffer *uartRxBuffer = &rxBufferA;
Buffer *gpsSentence = &rxBufferB;
char header[7];

char *gps_get_field(Buffer *NMEA_sentence, uint8_t field) {
  const char delim[] = ",*";
  uint8_t field_start = 0;
  uint8_t field_size = 0;
  uint8_t count = 0;
  static char ret_field[10];


  for (uint8_t i = 0; i < NMEA_sentence->index; i++){
    for (uint8_t d = 0; d < strlen(delim); d++) {
      if (NMEA_sentence->data[i] == delim[d]) {
        count++;
        if (count > field) {
          field_size = i - field_start;
          strncpy(ret_field, (char *)&NMEA_sentence->data[field_start], field_size);
          return ret_field;
        }
        field_start = i + 1;
      }
    }
  }
  return NULL;
}

void gps_init(UART_HandleTypeDef *huart) {
  //It's vital to clear the OREF flag if there has been any chance that more than a single character has been received before we get to this point.
  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
  HAL_UART_Receive_IT(huart, &rx_char, 1);
}

// Receives a character from UART. 
// If charcter is '$' then a new NMEA sentence is starting so check the current on to see
// if it's the GPGGA sentence we wanted. If so, then we can swap it to the user buffer
// allowing the user to see the change in length and process the data. 
// If not just start writing over the internal buffer instead.
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  Buffer *tmpBuffer;

  if (rx_char == '$') {
    if (strncmp((char *)uartRxBuffer->data, "$GPGGA", 6) == 0) { 
      tmpBuffer = uartRxBuffer;
      uartRxBuffer = gpsSentence;
      gpsSentence = tmpBuffer;
    }
    uartRxBuffer->index = 0;
  }

  uartRxBuffer->data[uartRxBuffer->index++] = rx_char;

  // clear overflow flag just in case we missed a byte, and get next byte
  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
  HAL_UART_Receive_IT(&huart2, &rx_char, 1);
}

//void gps_write(uint8_t *data, uint8_t len) {
//  HAL_UART_Transmit_DMA(&huart2, data, len);
//}

Buffer *gps_read() {
  return gpsSentence;
} 

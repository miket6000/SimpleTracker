#include "gps.h"
#include "usb.h"
#include "usart.h"
#include <string.h>
#include "event.h"

static uint8_t rx_char;
static char bufferA[NMEA_SENTENCE_SIZE];
static char bufferB[NMEA_SENTENCE_SIZE];

char *rxBuffer = bufferA;
char *lastSentence = bufferB;

char *gps_get_field(char *nmeaSentence, uint8_t field) {
  const char delim[] = ",*";
  uint8_t field_start = 0;
  uint8_t field_size = 0;
  uint8_t count = 0;
  static char ret_field[10];

  for (uint8_t i = 0; i < NMEA_SENTENCE_SIZE; i++){
    if (nmeaSentence[i] == '\0') return NULL;
    for (uint8_t d = 0; d < strlen(delim); d++) {
      if (nmeaSentence[i] == delim[d]) {
        count++;
        if (count > field) {
          field_size = i - field_start;
          strncpy(ret_field, &nmeaSentence[field_start], field_size);
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
// If charcter is '\n' then a new NMEA sentence is starting so check the current on to see
// if it's the GPGGA sentence we wanted. If so, then we can trigger a new event to deal 
// with it.
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  static uint8_t index = 0;

  if (rx_char != '\n' && index < NMEA_SENTENCE_SIZE - 1) {
    rxBuffer[index++] = rx_char;
  } else {
    rxBuffer[index] = '\0';
    index = 0;
    if (strncmp(rxBuffer, "$GNGGA", 6) == 0) { 
      char *tmp = lastSentence;
      lastSentence = rxBuffer;
      rxBuffer = tmp;
      
      Event_t newEvent = {
        .type = EVENT_GPS_UPDATE,
        .data = lastSentence
      };
      eventQueue_push(newEvent);
    }
  }

  // clear overflow flag just in case we missed a byte, and get next byte
  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
  HAL_UART_Receive_IT(&huart2, &rx_char, 1);
}


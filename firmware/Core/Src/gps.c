#include "gps.h"
#include "usart.h"
#include <string.h>
#include "usb.h"

static uint8_t rx_char;
static char bufferA[NMEA_SENTENCE_SIZE];
static char bufferB[NMEA_SENTENCE_SIZE];
uint8_t sentenceReady = 0;

char *rxBuffer = bufferA;
char *lastSentence = bufferB;


typedef enum {
  GPS_WAIT_START,
  GPS_MATCH_HEADER,
  GPS_COLLECT
} gps_state_t;

static volatile gps_state_t gpsState = GPS_WAIT_START;
static const char header[] = "$GNGGA";
static volatile uint8_t headerIndex = 0;
static volatile uint8_t sentenceIndex = 0;

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
 switch (gpsState) {

    case GPS_WAIT_START:
      if (rx_char == header[0]) {
        headerIndex = 1;
        gpsState = GPS_MATCH_HEADER;
      }
      break;

    case GPS_MATCH_HEADER:
      if (rx_char == header[headerIndex]) {
        headerIndex++;
        if (headerIndex == sizeof(header) - 1) {
          sentenceIndex = headerIndex;
          memcpy(rxBuffer, header, headerIndex);
          gpsState = GPS_COLLECT;
        }
        
      } else {
        gpsState = GPS_WAIT_START;
      }
      break;

    case GPS_COLLECT:
      if (rx_char == '\n') {
        rxBuffer[sentenceIndex] = '\0';
        sentenceReady = 1;
        gpsState = GPS_WAIT_START;
      } else if (sentenceIndex < NMEA_SENTENCE_SIZE - 1 && rx_char != '\r') {
        rxBuffer[sentenceIndex++] = rx_char;
      }
      break;
 }
    // clear overflow flag just in case we missed a byte, and get next byte
  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
  HAL_UART_Receive_IT(&huart2, &rx_char, 1);
}


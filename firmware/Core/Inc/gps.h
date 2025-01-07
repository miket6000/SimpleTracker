#ifndef GPS_H
#define GPS_H
#include <stdint.h>
#include "usart.h"

#define NMEA_SENTENCE_SIZE  83
#define NMEA_DELIM_CHAR     ','

typedef struct {
  uint8_t data[NMEA_SENTENCE_SIZE];
  uint16_t index;
} Buffer;

enum {
  HEADER = 0,
  TIME,
  LATITUDE,
  NS,
  LONGITUDE,
  EW,
  FIX,
  NUM_SATS,
  HDOP,
  MSL_ALT,
  UNIT_ALT,
  GEOIDAL_SEP,
  UNIT_SEP,
  DIFF_CORR_AGE,
  DIFF_ID,
  CHECKSUM,
};

void gps_init(UART_HandleTypeDef *huart);
void gps_write(uint8_t *data, uint8_t len);
Buffer *gps_read();
char *gps_get_field(Buffer* NEMA_sentence, uint8_t field);
#endif //GPS_H

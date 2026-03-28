#ifndef GPS_H
#define GPS_H
#include <stdint.h>
#include "usart.h"

#define NMEA_SENTENCE_SIZE  83
#define NMEA_DELIM_CHAR     ','

extern uint8_t sentenceReady;

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

// Compact binary GPS packet for LoRa transmission (19 bytes)
typedef struct __attribute__((packed)) {
  uint32_t  time_s;       // Seconds since midnight UTC (0–86399)
  int32_t   latitude;     // ddmm.mmmmm as integer x100000, negative = South
  int32_t   longitude;    // dddmm.mmmmm as integer x100000, negative = West
  int16_t   altitude;     // MSL altitude in whole metres
  int16_t   geoid_sep;    // Geoidal separation in decimetres (divide by 10 for metres)
  uint8_t   satellites;   // Number of satellites in use
  uint8_t   hdop;         // HDOP x10 (0.1 resolution, max 25.5)
  uint8_t   flags;        // Bit 2..0: fix quality (0–6), Bit 7..3: reserved
} GpsPacket_t;

#define GPS_PACKET_SIZE      sizeof(GpsPacket_t)
#define GPS_FLAG_FIX_MASK    0x07

// Pack an NMEA GNGGA sentence into a compact binary packet.
// Returns 1 on success, 0 if the sentence could not be parsed.
uint8_t gps_pack_sentence(const char *nmeaSentence, GpsPacket_t *packet);

// Unpack a binary GPS packet into a reconstructed NMEA GNGGA sentence.
// buf must be at least NMEA_SENTENCE_SIZE bytes. Returns length written (excluding '\0').
uint8_t gps_unpack_to_nmea(const GpsPacket_t *packet, char *buf, uint8_t bufSize);

void gps_init(UART_HandleTypeDef *huart);
void gps_write(uint8_t *data, uint8_t len);
Buffer *gps_read();
char *gps_get_field(char *nmeaSentence, uint8_t field);
#endif //GPS_H

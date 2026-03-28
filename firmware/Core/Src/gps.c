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
          ret_field[field_size] = '\0';
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

// ---------------------------------------------------------------------------
// Integer-only NMEA field parsers (no floating point, no external libraries)
// ---------------------------------------------------------------------------

// Parse a decimal string (with optional decimal point) into an integer,
// treating it as a fixed-point number with 'decimals' fractional digits.
// e.g. "4807.03800" with decimals=5 -> 480703800
// Returns the parsed value via *result. Returns 1 on success, 0 on failure.
static uint8_t parse_fixed(const char *s, uint8_t decimals, int32_t *result) {
  if (s == NULL || *s == '\0') return 0;
  int32_t intPart = 0;
  int32_t fracPart = 0;
  uint8_t fracDigits = 0;
  uint8_t pastDot = 0;
  uint8_t negative = 0;
  const char *p = s;

  if (*p == '-') { negative = 1; p++; }

  for (; *p != '\0'; p++) {
    if (*p == '.') { pastDot = 1; continue; }
    if (*p < '0' || *p > '9') break;
    if (pastDot) {
      if (fracDigits < decimals) {
        fracPart = fracPart * 10 + (*p - '0');
        fracDigits++;
      }
      // ignore extra fractional digits
    } else {
      intPart = intPart * 10 + (*p - '0');
    }
  }
  // Pad fractional part if fewer digits than expected
  while (fracDigits < decimals) { fracPart *= 10; fracDigits++; }

  // Compute multiplier for integer part
  int32_t mult = 1;
  for (uint8_t i = 0; i < decimals; i++) mult *= 10;

  *result = intPart * mult + fracPart;
  if (negative) *result = -(*result);
  return 1;
}

// Parse a simple unsigned decimal integer from a string (e.g. "08" -> 8)
static uint32_t parse_uint(const char *s) {
  if (s == NULL) return 0;
  uint32_t v = 0;
  for (const char *p = s; *p >= '0' && *p <= '9'; p++)
    v = v * 10 + (*p - '0');
  return v;
}

uint8_t gps_pack_sentence(const char *nmeaSentence, GpsPacket_t *packet) {
  memset(packet, 0, sizeof(GpsPacket_t));

  // Use gps_get_field — note it returns a pointer to a static buffer,
  // so we must copy or consume each field before calling it again.

  // Time: "hhmmss.ss" -> seconds since midnight
  char *field = gps_get_field((char *)nmeaSentence, TIME);
  if (field != NULL && field[0] != '\0') {
    uint32_t hh = (field[0] - '0') * 10 + (field[1] - '0');
    uint32_t mm = (field[2] - '0') * 10 + (field[3] - '0');
    uint32_t ss = (field[4] - '0') * 10 + (field[5] - '0');
    packet->time_s = hh * 3600 + mm * 60 + ss;
  }

  // Latitude: "ddmm.mmmmm" -> integer x100000
  field = gps_get_field((char *)nmeaSentence, LATITUDE);
  int32_t latVal = 0;
  if (field != NULL && field[0] != '\0')
    parse_fixed(field, 5, &latVal);
  // N/S
  field = gps_get_field((char *)nmeaSentence, NS);
  if (field != NULL && field[0] == 'S') latVal = -latVal;
  packet->latitude = latVal;

  // Longitude: "dddmm.mmmmm" -> integer x100000
  field = gps_get_field((char *)nmeaSentence, LONGITUDE);
  int32_t lonVal = 0;
  if (field != NULL && field[0] != '\0') parse_fixed(field, 5, &lonVal);
  // E/W
  field = gps_get_field((char *)nmeaSentence, EW);
  if (field != NULL && field[0] == 'W') lonVal = -lonVal;
  packet->longitude = lonVal;

  // Fix quality
  field = gps_get_field((char *)nmeaSentence, FIX);
  if (field != NULL && field[0] != '\0')
    packet->flags = (uint8_t)(field[0] - '0') & GPS_FLAG_FIX_MASK;

  // Satellites
  field = gps_get_field((char *)nmeaSentence, NUM_SATS);
  if (field != NULL && field[0] != '\0')
    packet->satellites = (uint8_t)parse_uint(field);

  // HDOP: "x.x" -> x10
  field = gps_get_field((char *)nmeaSentence, HDOP);
  if (field != NULL && field[0] != '\0') {
    int32_t hdopVal = 0;
    parse_fixed(field, 1, &hdopVal);
    packet->hdop = (hdopVal > 255) ? 255 : (uint8_t)hdopVal;
  }

  // MSL Altitude: "x.x" -> whole metres (truncate fractional)
  field = gps_get_field((char *)nmeaSentence, MSL_ALT);
  if (field != NULL && field[0] != '\0') {
    int32_t altVal = 0;
    parse_fixed(field, 0, &altVal);
    packet->altitude = (int16_t)altVal;
  }

  // Geoidal separation: "x.x" -> decimetres (x10)
  field = gps_get_field((char *)nmeaSentence, GEOIDAL_SEP);
  if (field != NULL && field[0] != '\0') {
    int32_t sepVal = 0;
    parse_fixed(field, 1, &sepVal);
    packet->geoid_sep = (int16_t)sepVal;
  }

  return 1;
}

// ---------------------------------------------------------------------------
// Unpack binary GPS packet back to a GNGGA-style NMEA string
// ---------------------------------------------------------------------------

// Append a signed integer to buf at position pos. Returns new position.
static uint8_t append_int(char *buf, uint8_t pos, uint8_t bufSize, int32_t value) {
  char tmp[12];
  uint8_t len = 0;
  int32_t v = value;
  uint8_t negative = 0;

  if (v < 0) { negative = 1; v = -v; }
  if (v == 0) { tmp[len++] = '0'; }
  else {
    while (v > 0) { tmp[len++] = '0' + (v % 10); v /= 10; }
  }
  if (negative) tmp[len++] = '-';

  // Reverse into buf
  for (uint8_t i = 0; i < len && pos < bufSize - 1; i++)
    buf[pos++] = tmp[len - 1 - i];
  return pos;
}

// Append an unsigned integer with zero-padding to width. Returns new position.
static uint8_t append_uint_padded(char *buf, uint8_t pos, uint8_t bufSize,
                                   uint32_t value, uint8_t width) {
  char tmp[10];
  uint8_t len = 0;
  uint32_t v = value;
  if (v == 0) { tmp[len++] = '0'; }
  else { while (v > 0) { tmp[len++] = '0' + (v % 10); v /= 10; } }
  // Pad
  while (len < width) tmp[len++] = '0';
  // Reverse into buf
  for (uint8_t i = 0; i < len && pos < bufSize - 1; i++)
    buf[pos++] = tmp[len - 1 - i];
  return pos;
}

// Append a fixed-point value: intDigits digits of integer, then '.', then fracDigits digits.
// value is in units of 10^(-fracDigits).
static uint8_t append_fixed(char *buf, uint8_t pos, uint8_t bufSize,
                             int32_t value, uint8_t intDigits, uint8_t fracDigits) {
  uint8_t negative = 0;
  int32_t v = value;
  if (v < 0) { negative = 1; v = -v; }

  int32_t divisor = 1;
  for (uint8_t i = 0; i < fracDigits; i++) divisor *= 10;

  int32_t intPart = v / divisor;
  int32_t fracPart = v % divisor;

  if (negative && pos < bufSize - 1) buf[pos++] = '-';
  pos = append_uint_padded(buf, pos, bufSize, (uint32_t)intPart, intDigits);
  if (pos < bufSize - 1) buf[pos++] = '.';
  pos = append_uint_padded(buf, pos, bufSize, (uint32_t)fracPart, fracDigits);
  return pos;
}

uint8_t gps_unpack_to_nmea(const GpsPacket_t *packet, char *buf, uint8_t bufSize) {
  uint8_t pos = 0;

  // Header
  const char hdr[] = "$GNGGA,";
  for (uint8_t i = 0; hdr[i] && pos < bufSize - 1; i++)
    buf[pos++] = hdr[i];

  // Time: seconds since midnight -> hhmmss.00
  uint32_t t = packet->time_s;
  uint32_t hh = t / 3600;
  uint32_t mm = (t % 3600) / 60;
  uint32_t ss = t % 60;
  pos = append_uint_padded(buf, pos, bufSize, hh, 2);
  pos = append_uint_padded(buf, pos, bufSize, mm, 2);
  pos = append_uint_padded(buf, pos, bufSize, ss, 2);
  if (pos < bufSize - 1) buf[pos++] = '.';
  pos = append_uint_padded(buf, pos, bufSize, 0, 2); // fractional seconds
  if (pos < bufSize - 1) buf[pos++] = ',';

  // Latitude: absolute value as ddmm.mmmmm
  int32_t lat = packet->latitude;
  uint8_t latSouth = 0;
  if (lat < 0) { latSouth = 1; lat = -lat; }
  pos = append_fixed(buf, pos, bufSize, lat, 4, 5);
  if (pos < bufSize - 1) buf[pos++] = ',';
  if (pos < bufSize - 1) buf[pos++] = latSouth ? 'S' : 'N';
  if (pos < bufSize - 1) buf[pos++] = ',';

  // Longitude: absolute value as dddmm.mmmmm
  int32_t lon = packet->longitude;
  uint8_t lonWest = 0;
  if (lon < 0) { lonWest = 1; lon = -lon; }
  pos = append_fixed(buf, pos, bufSize, lon, 5, 5);
  if (pos < bufSize - 1) buf[pos++] = ',';
  if (pos < bufSize - 1) buf[pos++] = lonWest ? 'W' : 'E';
  if (pos < bufSize - 1) buf[pos++] = ',';

  // Fix quality
  if (pos < bufSize - 1) buf[pos++] = '0' + (packet->flags & GPS_FLAG_FIX_MASK);
  if (pos < bufSize - 1) buf[pos++] = ',';

  // Satellites
  pos = append_uint_padded(buf, pos, bufSize, packet->satellites, 2);
  if (pos < bufSize - 1) buf[pos++] = ',';

  // HDOP: value/10
  pos = append_fixed(buf, pos, bufSize, packet->hdop, 1, 1);
  if (pos < bufSize - 1) buf[pos++] = ',';

  // MSL Altitude: whole metres with .0
  pos = append_int(buf, pos, bufSize, packet->altitude);
  if (pos < bufSize - 1) buf[pos++] = '.';
  if (pos < bufSize - 1) buf[pos++] = '0';
  if (pos < bufSize - 1) buf[pos++] = ',';
  if (pos < bufSize - 1) buf[pos++] = 'M';
  if (pos < bufSize - 1) buf[pos++] = ',';

  // Geoidal separation: stored as decimetres, output as x.x
  pos = append_fixed(buf, pos, bufSize, packet->geoid_sep, 1, 1);
  if (pos < bufSize - 1) buf[pos++] = ',';
  if (pos < bufSize - 1) buf[pos++] = 'M';
  if (pos < bufSize - 1) buf[pos++] = ',';

  // Diff correction age and station ID: empty fields
  if (pos < bufSize - 1) buf[pos++] = ',';

  // Checksum: XOR of everything between '$' (exclusive) and '*' (exclusive)
  if (pos < bufSize - 1) buf[pos++] = '*';
  uint8_t cksum = 0;
  for (uint8_t i = 1; i < pos - 1; i++) // skip '$' at 0 and '*' at pos-1
    cksum ^= (uint8_t)buf[i];
  // Append checksum as two hex digits
  const char hex[] = "0123456789ABCDEF";
  if (pos < bufSize - 1) buf[pos++] = hex[(cksum >> 4) & 0x0F];
  if (pos < bufSize - 1) buf[pos++] = hex[cksum & 0x0F];

  buf[pos] = '\0';
  return pos;
}

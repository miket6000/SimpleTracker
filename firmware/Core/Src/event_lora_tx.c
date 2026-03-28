#include "event_processors.h"
#include "config.h"
#include "led_sequences.h"
#include "lora.h"
#include "gps.h"
#include <string.h>

char loraTxBuffer[LORA_MAX_MSG_LEN];

void processLoRaTx(AppContext_t *context, char *loraTxPayload) {
//  led_add_sequence(context->led, lora_tx_sequence);
  strncpy(loraTxBuffer, context->uidStr, UID_STR_LENGTH);
  strncat(loraTxBuffer, " ", 2); //include \0
  strncat(loraTxBuffer, (char *)loraTxPayload, strlen(loraTxPayload));
  //if (LoRa_checkTransmit(context->lora) != LORA_STATUS_BUSY) {
  LoRa_Transmit(context->lora, (uint8_t *)loraTxBuffer, strlen(loraTxBuffer));
  //}
}

void processLoRaGpsTx(AppContext_t *context, GpsPacket_t *packet) {
  // Build binary frame: "<uid> " (9 bytes) + GPS_PACKET_SIZE (19 bytes)
  uint8_t len = 0;
  memcpy(&loraTxBuffer[0], context->uidStr, 8);
  len += 8;
  loraTxBuffer[len++] = ' ';
  memcpy(&loraTxBuffer[len], packet, GPS_PACKET_SIZE);
  len += GPS_PACKET_SIZE;
  LoRa_Transmit(context->lora, (uint8_t *)loraTxBuffer, len);
}


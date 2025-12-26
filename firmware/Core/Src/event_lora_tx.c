#include "event_processors.h"
#include "config.h"
#include "led_sequences.h"
#include "lora.h"
#include <string.h>

char loraTxBuffer[LORA_MAX_MSG_LEN];

void processLoRaTx(AppContext_t *context, char *loraTxPayload) {
//  led_add_sequence(context->led, lora_tx_sequence);
  strncpy(loraTxBuffer, context->uidStr, UID_STR_LENGTH);
  strncat(loraTxBuffer, " ", 2); //include \0
  strncat(loraTxBuffer, (char *)loraTxPayload, strlen(loraTxPayload));

  LoRa_transmit(context->lora, (uint8_t *)loraTxBuffer, strlen(loraTxBuffer), LORA_TX_TIMEOUT);
}


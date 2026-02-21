#include "event_processors.h"
#include "led_sequences.h"
#include "lora.h"

void processLoRaRx(AppContext_t *context, char *loraMessage) {
  // Check if it's a command
  // [0] '&'
  // [1..8] UID
  // [9] command
  if (loraMessage[0] == '&') {
    //if the message is for us...
    if (strncmp(&loraMessage[1], context->uidStr, 8) == 0) {   
      switch (loraMessage[9]) {
        case 'T':
          LoRa_Transmit(context->lora, (uint8_t *)"ACK", 4);
          context->mode = MODE_TRACKER;
          break;
        case 'V':
          char buf[5];
          itoa(context->voltage, buf, 10);
          LoRa_Transmit(context->lora, (uint8_t *)buf, strlen(buf));
          break;
        default:
          LoRa_Transmit(context->lora, (uint8_t *)"NAK", 4);
          break;
      }
    }
  } else {
    context->lastLoraMessage = loraMessage;
  }

  //led_add_sequence(context->loraLed, lora_rx_sequence);
}
      


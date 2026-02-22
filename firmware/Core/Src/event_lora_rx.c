#include "event_processors.h"
#include "led_sequences.h"
#include <string.h>
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
          if (context->gpsFix) {
            led_add_sequence(context->led, gps_lock_sequence);
          } else {
            led_add_sequence(context->led, gps_search_sequence); 
          }
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
    char rssi[5] = {0};
    itoa(context->rssi, rssi, 10);
    strncat(loraMessage, " ", 2);
    strncat(loraMessage, rssi, 5);
    context->lastLoraMessage = loraMessage;
  }

  //led_add_sequence(context->loraLed, lora_rx_sequence);
}
      


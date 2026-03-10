#include "event_processors.h"
#include "led_sequences.h"
#include <string.h>
#include "lora.h"

#define BROADCAST_ADDRESS "ffffffff"

void processLoRaRx(AppContext_t *context, char *loraMessage) {
  // Check if it's a command
  // [0] '&'
  // [1..8] UID
  // [9] command
  if (loraMessage[0] == '&') {

    // is it addressed to us, or a broadcast?
    if (strncmp(&loraMessage[1], context->uidStr, 8) == 0 ||
        strncmp(&loraMessage[1], BROADCAST_ADDRESS, 8) == 0) {   
      // do what it says...
      switch (loraMessage[9]) {
        case 'U': // Get UID
          LoRa_Transmit(context->lora, (uint8_t *)context->uidStr, strlen(context->uidStr));
          break;
        case 'T': // Change to Transmit Mode
          LoRa_Transmit(context->lora, (uint8_t *)"ACK", 4);
          context->mode = MODE_TRACKER;
          if (context->gpsFix) {
            led_add_sequence(context->led, gps_lock_sequence);
          } else {
            led_add_sequence(context->led, gps_search_sequence); 
          }
          break;
        case 'V': // Get voltage
          char buf[5];
          itoa(context->voltage, buf, 10);
          LoRa_Transmit(context->lora, (uint8_t *)buf, strlen(buf));
          break;
        default: // Unknown command, send NAK
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
      


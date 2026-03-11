#include "event_processors.h"
#include "led_sequences.h"
#include "lora_arbitration.h"
#include "lora_discovery.h"
#include <string.h>
#include "lora.h"
#include "config.h"

#define BROADCAST_ADDRESS "ffffffff"

// On-air message format (sent via processLoRaTx):
//   [0..7]  sender UID
//   [8]     space
//   [9]     '&'
//   [10..17] destination UID
//   [18]    command character
//   [19..]  optional payload
//
// Offset constants for clarity
#define OFS_SENDER    0
#define OFS_CMD_START 9
#define OFS_DEST      10
#define OFS_CMD_CHAR  18
#define OFS_PAYLOAD   19
#define MIN_CMD_LEN   19  // minimum valid command message length

void processLoRaRx(AppContext_t *context, char *loraMessage) {
  uint16_t len = strlen(loraMessage);

  // Check if the message is a structured command with sender prefix
  if (len >= MIN_CMD_LEN && loraMessage[OFS_CMD_START] == '&') {

    // Is it addressed to us, or a broadcast?
    if (strncmp(&loraMessage[OFS_DEST], context->uidStr, 8) == 0 ||
        strncmp(&loraMessage[OFS_DEST], BROADCAST_ADDRESS, 8) == 0) {

      // Extract sender UID for building reply-to address
      char senderUid[9];
      strncpy(senderUid, &loraMessage[OFS_SENDER], 8);
      senderUid[8] = '\0';

      switch (loraMessage[OFS_CMD_CHAR]) {
        case 'U': // Get UID
          if (discovery_is_active()) {
            // We initiated this discovery — collect the response
            // The responder's UID is in the sender prefix [0..7]
            discovery_add_response(senderUid, context->rssi);
          } else {
            // We received a discovery request — reply with our UID
            // Build payload: "&<sender_uid>U<my_uid>"
            char response[28];
            response[0] = '&';
            strncpy(&response[1], senderUid, 8);
            response[9] = 'U';
            strncpy(&response[10], context->uidStr, 8);
            response[18] = '\0';
            lora_schedule_delayed_response((uint8_t *)response, strlen(response));
          }
          break;

        case 'T': // Change to Transmit Mode
          // Respond immediately addressed to sender, then switch mode
          {
            char ack[14];
            ack[0] = '&';
            strncpy(&ack[1], senderUid, 8);
            ack[9] = 'T';
            strncpy(&ack[10], "ACK", 3);
            ack[13] = '\0';
            LoRa_Transmit(context->lora, (uint8_t *)ack, strlen(ack));
          }
          context->mode = MODE_TRACKER;
          if (context->gpsFix) {
            led_add_sequence(context->led, gps_lock_sequence);
          } else {
            led_add_sequence(context->led, gps_search_sequence);
          }
          break;

        case 'V': // Get voltage
          if (discovery_is_active()) {
            // We initiated a voltage query — store the response for readback
            context->lastLoraMessage = loraMessage;
          } else {
            // We received a voltage request — reply with our voltage
            char response[28];
            char vbuf[6];
            itoa(context->voltage, vbuf, 10);
            response[0] = '&';
            strncpy(&response[1], senderUid, 8);
            response[9] = 'V';
            strncpy(&response[10], vbuf, sizeof(vbuf));
            response[10 + strlen(vbuf)] = '\0';
            lora_schedule_delayed_response((uint8_t *)response, strlen(response));
          }
          break;

        default: // Unknown command, send NAK immediately
          LoRa_Transmit(context->lora, (uint8_t *)"NAK", 3);
          break;
      }
    }
  } else {
    // Non-command message (e.g. GPS tracking data): append RSSI and store
    char rssi[5] = {0};
    itoa(context->rssi, rssi, 10);
    strncat(loraMessage, " ", 2);
    strncat(loraMessage, rssi, 5);
    context->lastLoraMessage = loraMessage;
  }
}
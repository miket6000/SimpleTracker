#include "event_processors.h"
#include "led_sequences.h"
#include "lora_arbitration.h"
#include "lora_discovery.h"
#include <string.h>
#include <stdlib.h>
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

// Parse comma-separated decimal config: "<freq>,<sf>,<bw>"
// Returns true if all three fields were parsed successfully.
static bool parse_config_payload(const char *payload,
                                 uint32_t *freq, uint8_t *sf, uint8_t *bw) {
  // freq
  const char *p = payload;
  if (*p == '\0') return false;
  *freq = (uint32_t)atol(p);
  if (*freq == 0) return false;

  // skip to first comma
  p = strchr(p, ',');
  if (p == NULL) return false;
  p++; // skip ','

  // sf
  *sf = (uint8_t)atoi(p);

  // skip to second comma
  p = strchr(p, ',');
  if (p == NULL) return false;
  p++; // skip ','

  // bw
  *bw = (uint8_t)atoi(p);
  return true;
}

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

        case 'T': // Change to Transmit Mode (with optional config payload)
          {
            const char *payload = &loraMessage[OFS_PAYLOAD];
            uint16_t payloadLen = len - OFS_PAYLOAD;

            if (context->mode == MODE_GROUND_STATION &&
                payloadLen >= 3 && strncmp(payload, "ACK", 3) == 0) {
              // Ground Station received ACK from tracker — apply stored config
              if (context->pendingFreq != 0) {
                LoRa_ApplyConfig(context->lora,
                                 context->pendingFreq,
                                 context->pendingSF,
                                 context->pendingBW);
                context->pendingFreq = 0;
                LoRa_Receive(context->lora, LORA_RX_TIMEOUT_MS);
              }
              // Store the ACK so the PC can read it back via 'R'
              context->lastLoraMessage = loraMessage;
            } else {
              // Tracker received a T command — parse config and ACK

              // Parse config payload if present: "<freq>,<sf>,<bw>"
              if (payloadLen > 0) {
                uint32_t freq;
                uint8_t sf, bw;
                if (parse_config_payload(payload, &freq, &sf, &bw)) {
                  context->pendingFreq = freq;
                  context->pendingSF   = sf;
                  context->pendingBW   = bw;
                  context->pendingConfigSwitch = true;
                }
              }

              // ACK immediately on the current (discovery) channel
              char ack[14];
              ack[0] = '&';
              strncpy(&ack[1], senderUid, 8);
              ack[9] = 'T';
              strncpy(&ack[10], "ACK", 3);
              ack[13] = '\0';
              LoRa_Transmit(context->lora, (uint8_t *)ack, strlen(ack));

              // If no config payload, switch to tracker mode immediately
              // If config payload present, task_lora_rx will apply config
              // after TX_DONE and then set MODE_TRACKER
              if (!context->pendingConfigSwitch) {
                context->mode = MODE_TRACKER;
              }

              if (context->gpsFix) {
                led_add_sequence(context->led, gps_lock_sequence);
              } else {
                led_add_sequence(context->led, gps_search_sequence);
              }
            }
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

void processLoRaTxDone(AppContext_t *context) {
  // Apply pending config switch (only set by remote tracker after ACK TX)
  if (context->pendingConfigSwitch) {
    LoRa_ApplyConfig(context->lora,
                     context->pendingFreq,
                     context->pendingSF,
                     context->pendingBW);
    context->pendingConfigSwitch = false;
    context->mode = MODE_TRACKER;
  }
  LoRa_Receive(context->lora, LORA_RX_TIMEOUT_MS);
}
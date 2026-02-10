#include <stdlib.h>
#include "lora.h"
#include "config.h"
#include "app_context.h"
#include "event.h"

char bufferA[LORA_MAX_MSG_LEN];
char bufferB[LORA_MAX_MSG_LEN];

char *lastLoraMessage = bufferA;
char *workingBuffer = bufferB;

void task_lora_rx(AppContext_t *context, void *param) {
  LoRaRxInfo_t info;

  if (context->mode & MODE_GROUND_STATION) {
    // LoRa Rx Complete, collect packet
    if (context->lora->events & LORA_EVENT_RX_DONE) {

      uint8_t len = LoRa_ReadPacket(context->lora, (uint8_t *)workingBuffer, LORA_MAX_MSG_LEN, &info);
      workingBuffer[len] = '\0';

      if (len > 0) {
        char *tmpBuffer = lastLoraMessage;
        lastLoraMessage = workingBuffer;
        workingBuffer = tmpBuffer;

        context->rssi = info.rssi;
        context->lastLoraMessage = lastLoraMessage;

        // Push new event for higher level code
        Event_t newEvent;
        newEvent.type = EVENT_LORA_RX;
        newEvent.data = lastLoraMessage;
        eventQueue_push(newEvent);
      }

      context->lora->events &= ~LORA_EVENT_RX_DONE;

      LoRa_Receive(context->lora, LORA_RX_TIMEOUT_MS);
    }

    // LoRa Rx Timeout, start again
    else if (context->lora->events & LORA_EVENT_TIMEOUT) {
      context->lora->events &= ~LORA_EVENT_TIMEOUT;
      LoRa_Receive(context->lora, LORA_RX_TIMEOUT_MS);
    }
    
    // Not in Rx mode, start Receiving
    else if (context->lora->currentMode != LORA_MODE_RX) {
      LoRa_Receive(context->lora, LORA_RX_TIMEOUT_MS); 
    }
  }
}

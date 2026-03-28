#include <stdlib.h>
#include "lora.h"
#include "config.h"
#include "app_context.h"
#include "event.h"

char bufferA[LORA_MAX_MSG_LEN];
char bufferB[LORA_MAX_MSG_LEN];

char *lastLoraMessage = bufferA;
char *workingBuffer = bufferB;

void task_lora(AppContext_t *context, void *param) {
  LoRaRxInfo_t info;

  if (context->mode & MODE_GROUND_STATION) {
    // LoRa Rx Complete, collect packet, swap buffers, publish event
    if (context->lora->events & LORA_EVENT_RX_DONE) {

      uint8_t len = LoRa_ReadPacket(context->lora, (uint8_t *)workingBuffer, LORA_MAX_MSG_LEN, &info);
      workingBuffer[len] = '\0';

      if (len > 0) {
        char *tmpBuffer = lastLoraMessage;
        lastLoraMessage = workingBuffer;
        workingBuffer = tmpBuffer;

        context->rssi = info.rssi;
        context->loraRxLen = len;

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

    // LoRa transmission complete, push event for higher level handling
    else if (context->lora->events & LORA_EVENT_TX_DONE) {
      context->lora->events &= ~LORA_EVENT_TX_DONE;

      Event_t txDoneEvent;
      txDoneEvent.type = EVENT_LORA_TX_DONE;
      txDoneEvent.data = NULL;
      eventQueue_push(txDoneEvent);
    }
  }
}

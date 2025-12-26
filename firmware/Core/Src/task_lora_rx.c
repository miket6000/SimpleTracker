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
  if (context->mode & MODE_GROUND_STATION) {
    uint8_t message_len = 0;
    
    message_len = LoRa_receive(context->lora, (uint8_t *)workingBuffer, LORA_MAX_MSG_LEN);
    workingBuffer[message_len] = '\0';
    
    if (message_len > 0) {
      char *tmpBuffer = lastLoraMessage;
      lastLoraMessage = workingBuffer;
      workingBuffer = tmpBuffer;

      context->rssi = LoRa_getRSSI(context->lora);      
      context->lastLoraMessage = lastLoraMessage;

      Event_t newEvent;
      newEvent.type = EVENT_LORA_RX;
      newEvent.data = lastLoraMessage;
      eventQueue_push(newEvent);
    }
  }
}

#include "event.h"
#include "event_processors.h"

static EventQueue_t eventQueue = { .head = 0, .tail = 0, .count = 0 };

uint8_t eventQueue_push(Event_t event) {
    if (eventQueue.count >= QUEUE_SIZE) {
      return 0;  // Queue full
    }

    eventQueue.buffer[eventQueue.head] = event;
    eventQueue.head = (eventQueue.head + 1) % QUEUE_SIZE;
    eventQueue.count++;
    
    return 1;  // Success
}

uint8_t eventQueue_pop(Event_t *event) {
    if (eventQueue.count == 0) {
        return 0;  // Queue empty
    }

    *event = eventQueue.buffer[eventQueue.tail];
    eventQueue.tail = (eventQueue.tail + 1) % QUEUE_SIZE;
    eventQueue.count--;
    
    return 1;  // Success
}

void eventDispatcher(AppContext_t *appContext) {
    Event_t event;
    
    if (eventQueue_pop(&event)) {
        switch (event.type) {
            case EVENT_GPS_UPDATE:
                processGPSData(appContext, event.data);
                break;
            case EVENT_LORA_RX:
                processLoRaRx(appContext, event.data);
                break;
            case EVENT_LORA_TX:
                processLoRaTx(appContext, event.data);
            default:
                break;
        }
    }
}

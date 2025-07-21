#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>

#define QUEUE_SIZE 5

typedef enum {
    EVENT_NONE,
    EVENT_GPS_UPDATE,
    EVENT_LORA_TX,
    EVENT_LORA_RX,
    EVENT_USB_COMMAND
} EventType_t;

typedef struct {
    EventType_t type;
    void *data;
} Event_t;

typedef struct {
    Event_t buffer[QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} EventQueue_t;

uint8_t eventQueue_push(Event_t event);
uint8_t eventQueue_pop(Event_t *event);
void eventDispatcher();
 
#endif // EVENT_H

#include "task.h"
#include "gps.h"
#include "event.h"

extern char *rxBuffer;
extern char *lastSentence;

void task_gps(void *param) {
  if (sentenceReady) {
    sentenceReady = 0;
    char *tmp = lastSentence;
    lastSentence = rxBuffer;
    rxBuffer = tmp;
    
    Event_t newEvent = {
      .type = EVENT_GPS_UPDATE,
      .data = lastSentence
    };
    
    eventQueue_push(newEvent);
  }
}

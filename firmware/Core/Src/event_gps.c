#include "event_processors.h"
#include "gps.h"
#include "config.h"
#include "led_sequences.h"

void processGPSData(AppContext_t *context, char *gpsSentence) {
  static uint8_t counter = 0;
  counter++;
  
  context->lastGpsSentence = gpsSentence;

  context->gpsFix = 
    (gps_get_field(gpsSentence, FIX) != NULL && 
     gps_get_field(gpsSentence, FIX)[0] == '1') ? 1 : 0;

  if (context->gpsFix && (counter & 0x01)) {
    led_add_sequence(context->gpsLed, gps_lock_sequence);
  } else {
    led_add_sequence(context->gpsLed, gps_search_sequence);
  }

  if (context->mode & MODE_TRANSMIT && context->gpsFix) {
    Event_t newEvent;
    newEvent.type = EVENT_LORA_TX;
    newEvent.data = gpsSentence;
    eventQueue_push(newEvent);
  }
}

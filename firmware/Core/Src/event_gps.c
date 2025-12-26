#include "event_processors.h"
#include "gps.h"
#include "config.h"
#include "led_sequences.h"

void processGPSData(AppContext_t *context, char *gpsSentence) {
  static uint8_t counter = 0;
  uint8_t hadFix = context->gpsFix;
  counter++;
  
  context->lastGpsSentence = gpsSentence;

  context->gpsFix = 
    (gps_get_field(gpsSentence, FIX) != NULL && 
     gps_get_field(gpsSentence, FIX)[0] != '0') ? 1 : 0; // 0 or null = no fix

  if (context->mode & MODE_TRACKER) {
    if (context->gpsFix) {
      if (!hadFix) {
        led_add_sequence(context->led, gps_lock_sequence);
      }
      
      Event_t newEvent;
      newEvent.type = EVENT_LORA_TX;
      newEvent.data = gpsSentence;
      eventQueue_push(newEvent);
    } else {
      if (hadFix) {
        led_add_sequence(context->led, gps_search_sequence);
      }
    }
  }
}

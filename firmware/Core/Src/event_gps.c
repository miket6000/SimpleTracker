#include "event_processors.h"
#include "gps.h"
#include "config.h"
#include "led_sequences.h"

/* CAUTION:
 * This code will "break" if the GPS UART IRQ swaps the buffer that gpsSentence points to.
 * This should be corrected by making this atomic, or stopping the UART until we return.
 */



void processGPSData(AppContext_t *context, char *gpsSentence) {
  static uint8_t counter = 0;
  uint8_t hadFix = context->gpsFix;
  counter++;
  
  context->lastGpsSentence = gpsSentence;

  context->gpsFix = 
    (gps_get_field(gpsSentence, FIX) != NULL && 
     gps_get_field(gpsSentence, FIX)[0] != '0') ? 1 : 0; // 0 or null = no fix

  if (context->mode & MODE_TRACKER) {
    // push an event for every second message to avoid swamping the lora modules
    if (counter & 0x01) {
        Event_t newEvent;
        newEvent.type = EVENT_LORA_TX;
        newEvent.data = context->lastGpsSentence;
        eventQueue_push(newEvent);
    }
    if (context->gpsFix) {
      if (!hadFix) {
        led_add_sequence(context->led, gps_lock_sequence);
      }
    } else {
      if (hadFix) {
        led_add_sequence(context->led, gps_search_sequence);
      }
    }
  }
}

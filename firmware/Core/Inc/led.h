#ifndef LED_H
#define LED_H

#include "stdint.h"

#define SEQUENCE_LEN 12

#define SHORT_PAUSE 10
#define PAUSE 11
#define NOTHING 12
#define END_OF_SEQUENCE 0x80

typedef enum {
  LED_OFF, 
  LED_ON, 
  LED_TOGGLE
} LedState;

typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
  uint8_t sequence_index;
  uint8_t blink_index;
  uint8_t blink_off_counter;
  uint8_t sequence_head;
  int8_t sequence[SEQUENCE_LEN];
} LedHandle;

void led(LedHandle *hled, const LedState state);
void led_init(LedHandle *hled, GPIO_TypeDef *port, uint16_t pin);
void led_reset_sequence(LedHandle *hled);
void led_add_number_sequence(LedHandle *hled, const uint16_t number);
void led_add_sequence(LedHandle *hled, const int8_t *const seq);
void led_blink(LedHandle *hled);
#endif

#include "main.h"
#include "led.h"
#include "gpio.h"
#include <string.h>

#define MAX_BLINK (32 / 2 - 1)
#define LED_MASK 0x01
#define VALID_MASK 0x02
#define BLINK_OFF_TIME 10

void led_init(LedHandle *hled, GPIO_TypeDef *port, uint16_t pin) {
  const uint8_t init_sequence[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}; 
  hled->port = port;
  hled->pin = pin;
  hled->sequence_index = 0;
  hled->blink_index = 0;
  hled->blink_off_counter = BLINK_OFF_TIME;
  hled->sequence_head = 0;
  memcpy(hled->sequence, init_sequence, sizeof(init_sequence));

  led(hled, LED_OFF); 
}

/* Sequencer state variables */

/* Seqence codes are read right to left, two bits at a time. The first bit of each pair
 * indicates that this code is still valid. The second bit indicates whether the LED is 
 * on (1) or off (0) for this cycle.
 * LEDs are on for 1 sub-cycle, then off for BLINK_OFF_TIME sub-cycles. 
 * Cycle length is determined by time between calls to Led_Blink().
 */
static const uint32_t codes[] = {  
  0x02AFFFFF, //0
  0x000000AB, //1 0000000010101011
  0x000002AF, //2 0000001010101111
  0x00000ABF, //3 0000101010111111
  0x00002AFF, //4 0010101011111111
  0x0000ABFF, //5
  0x0002AFFF, //6
  0x000ABFFF, //7
  0x002AFFFF, //8
  0x00ABFFFF, //9
  0x0000AAAA, //SHORT_PAUSE
  0xAAAAAAAA, //PAUSE
  0x00000000  //NOTHING
};

static void step_sequencer(LedHandle *hled);

void led(LedHandle *hled, const LedState state) {
  switch (state) {
    case LED_ON:
      HAL_GPIO_WritePin(hled->port, hled->pin, GPIO_PIN_SET);
      break;
    case LED_TOGGLE:
      HAL_GPIO_TogglePin(hled->port, hled->pin);
      break;
    default:
      HAL_GPIO_WritePin(hled->port, hled->pin, GPIO_PIN_RESET);
      break;
  }
}

void led_reset_sequence(LedHandle *hled) {
  hled->sequence_index = 0;
  hled->blink_index = 0;
  hled->blink_off_counter = BLINK_OFF_TIME;
  hled->sequence_head = 0;
}

void led_add_number_sequence(LedHandle *hled, const uint16_t number) {
  int8_t s[] = {NOTHING, NOTHING, NOTHING, NOTHING, PAUSE, -5};
  uint16_t whole = 0;

  if ((number / 1000) > 0) {
    s[0] = number / 1000;
    whole = s[0] * 1000;
  }
  if ((number / 100) > 0) {
    s[1] = (number - whole) / 100;
    whole = whole + s[1] * 100;
  }
  if ((number / 10) > 0) {
    s[2] = (number - whole) / 10;
    whole = whole + s[2] * 10;
  }

  s[3] = (number - whole);
  led_add_sequence(hled, s);
}


void led_add_sequence(LedHandle *hled, const int8_t *const new_sequence) {
  uint8_t i = 0;

  while (new_sequence[i] >= 0) {
    hled->sequence[hled->sequence_head] = new_sequence[i];
    hled->sequence_head++;
    hled->sequence_head %= SEQUENCE_LEN;
    i++;
  }
  hled->sequence[hled->sequence_head] = new_sequence[i];
}

void led_blink(LedHandle *hled) {
  uint8_t blink;

  if (hled->blink_off_counter == 0) {
    blink = (codes[hled->sequence[hled->sequence_index]] >> (2 * hled->blink_index)) & 0x00000003;
    
    if (blink & LED_MASK) {
      led(hled, LED_ON);
    } else {
      led(hled, LED_OFF);
    } 

    if ((blink & VALID_MASK) && (hled->blink_index <= MAX_BLINK)) {
      hled->blink_index++;
    } else {
      hled->blink_index = 0;
      step_sequencer(hled);
    }
    
    hled->blink_off_counter = BLINK_OFF_TIME;
  } else {
    led(hled, LED_OFF);
    hled->blink_off_counter--;
  }
}

static void step_sequencer(LedHandle *hled) {
  hled->sequence_index++;
  if (hled->sequence_index >= SEQUENCE_LEN) {
    hled->sequence_index = 0;
  };

  if (hled->sequence[hled->sequence_index] < 0) {
    if (hled->sequence_index + hled->sequence[hled->sequence_index] < 0) {
      hled->sequence_index += hled->sequence[hled->sequence_index] + SEQUENCE_LEN;
    } else {
      hled->sequence_index += hled->sequence[hled->sequence_index];
    }
  }
}



#include "main.h"
#include "led.h"
#include "gpio.h"
#include <string.h>
#include <stdbool.h>

/*
 * Code encoding: uint32_t
 *   Bits [31:24] = number of valid bits (1-24)
 *   Bits [23:0]  = pattern, read LSB first. 1 = LED on, 0 = LED off.
 *
 * Each call to led_blink() consumes one bit from the active code.
 * When all bits are consumed the sequencer advances to the next entry.
 *
 * The sequence engine takes in an int8_t array of len <= SEQUENCE_LEN.
 * Non-negative values are indices into codes[]. Negative values are
 * relative jumps (e.g. -3 jumps back 3 positions).
 */

/* Helper: build a code from a bit count and pattern */
#define CODE(n, pattern) (((uint32_t)(n) << 24) | ((pattern) & 0x00FFFFFF))

/*
 * Blink patterns: each blink = 1 on bit + 1 off bit (2 bits per blink),
 * followed by a trailing gap of 4 off bits after the last blink.
 * Pattern is read LSB-first: blink unit = 0b01.
 *
 * N blinks = 2*N + 4 bits.  Max is 9 blinks = 22 bits (fits in 24).
 */
static const uint32_t codes[NUM_CODES] = {
  /* 0: 01 x10 + 0000 = 24 bits */
  CODE(24, 0x055555),
  /* 1: 01 0000 = 6 bits */
  CODE(6,  0x000001),
  /* 2: 01 01 0000 = 8 bits */
  CODE(8,  0x000005),
  /* 3: 01 01 01 0000 = 10 bits */
  CODE(10, 0x000015),
  /* 4: 01 01 01 01 0000 = 12 bits */
  CODE(12, 0x000055),
  /* 5: 01 x5 + 0000 = 14 bits */
  CODE(14, 0x000155),
  /* 6: 01 x6 + 0000 = 16 bits */
  CODE(16, 0x000555),
  /* 7: 01 x7 + 0000 = 18 bits */
  CODE(18, 0x001555),
  /* 8: 01 x8 + 0000 = 20 bits */
  CODE(20, 0x005555),
  /* 9: 01 x9 + 0000 = 22 bits */
  CODE(22, 0x015555),
  /* SHORT_PAUSE: 8 cycles off */
  CODE(8,  0x000000),
  /* PAUSE: 24 cycles off */
  CODE(24, 0x000000),
  /* NOTHING: 1 cycle off */
  CODE(1,  0x000000),
  /* ON_HOLD: LED on for 24 cycles */
  CODE(24, 0xFFFFFF),
};

static void step_sequencer(LedHandle *hled);

/* How many ticks each "off" bit should last (permanently stretched).
 * Value of 1 = normal (each bit = 1 tick). Use 2 to make off twice as long as on.
 */
#define OFF_TICKS 4

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

void led_init(LedHandle *hled, GPIO_TypeDef *port, uint16_t pin) {
  hled->port = port;
  hled->pin = pin;
  hled->sequence_index = 0;
  hled->bit_index = 0;
  hled->sequence_head = 0;
  memset(hled->sequence, NOTHING, SEQUENCE_LEN);
  hled->off_counter = 0;
  led(hled, LED_OFF);
}

void led_reset_sequence(LedHandle *hled) {
  hled->sequence_index = 0;
  hled->bit_index = 0;
  hled->sequence_head = 0;
  hled->off_counter = 0;
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
    if (hled->sequence_head >= SEQUENCE_LEN) {
      hled->sequence_head = 0;
    }
    i++;
  }
  hled->sequence[hled->sequence_head] = new_sequence[i];
}

void led_blink(LedHandle *hled) {
  int8_t seq_entry = hled->sequence[hled->sequence_index];
  if (seq_entry < 0 || seq_entry >= NUM_CODES) {
    led(hled, LED_OFF);
    return;
  }

  uint32_t code = codes[seq_entry];
  uint8_t length = (code >> 24) & 0xFF;
  uint32_t pattern = code & 0x00FFFFFF;

  /* Read current bit and drive LED. Off bits are stretched by OFF_STRETCH_TICKS. */
  bool bit_on = ((pattern >> hled->bit_index) & 0x01) != 0;

  if (bit_on) {
    led(hled, LED_ON);
    /* when on bit is consumed, advance to next bit immediately */
    hled->bit_index++;
    /* reset any off-counter since we saw an on */
    hled->off_counter = 0;
  } else {
    /* off bit: if off_counter > 0 keep LED off and decrement; otherwise
     * set counter to OFF_TICKS (we consume one tick now)
     * and advance to next bit when counter reaches 0
     */
    led(hled, LED_OFF);
    if (hled->off_counter > 0) {
      hled->off_counter--;
      /* do not advance bit_index until off_counter reaches zero */
    } else {
      hled->off_counter = OFF_TICKS;
      /* advance to next bit once we have consumed this off-bit's first tick */
      hled->bit_index++;
    }
  }

  /* If we've consumed all bits in the code advance sequencer */
  if (hled->bit_index >= length) {
    hled->bit_index = 0;
    step_sequencer(hled);
  }
}

static void step_sequencer(LedHandle *hled) {
  hled->sequence_index++;
  if (hled->sequence_index >= SEQUENCE_LEN) {
    hled->sequence_index = 0;
  }

  while (hled->sequence[hled->sequence_index] < 0) {
    int8_t jump = hled->sequence[hled->sequence_index];
    int16_t target = (int16_t)hled->sequence_index + jump;
    if (target < 0) {
      target += SEQUENCE_LEN;
    }
    hled->sequence_index = (uint8_t)target;
  }
}



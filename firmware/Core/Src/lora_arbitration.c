#include "lora_arbitration.h"
#include "event.h"
#include "stm32f0xx_hal.h"
#include <string.h>
#include <stdint.h>

// Static buffer to hold the pending response payload
static char lora_response_buffer[64];
static uint16_t lora_response_length = 0;
static uint32_t lora_response_target_time = 0;
static bool lora_response_pending = false;

// Simple Linear Congruential Generator (LCG) for pseudo-random numbers
// This avoids linking in the full rand() library (~200+ bytes)
static uint32_t lora_prng_state = 0x12345678;

void lora_seed_prng(uint32_t seed) {
  lora_prng_state = seed ? seed : 0x12345678;
}

// Fast 32-bit LCG: x = (a*x + c) mod m
// Parameters from POSIX.1c: a=1103515245, c=12345, m=2^31
static uint32_t lora_prng_next(void) {
  lora_prng_state = (lora_prng_state * 1103515245U + 12345U) & 0x7fffffffU;
  return lora_prng_state;
}

uint32_t lora_calculate_random_delay(void) {
  // Generate a random number of slots using the LCG
  uint32_t slot = lora_prng_next() % ((LORA_MAX_SLOT_DELAY_MS - LORA_MIN_SLOT_DELAY_MS) / LORA_SLOT_INTERVAL_MS + 1);
  uint32_t delay_ms = LORA_MIN_SLOT_DELAY_MS + (slot * LORA_SLOT_INTERVAL_MS);
  return delay_ms;
}

void lora_schedule_delayed_response(uint8_t *responseData, uint16_t responseLength) {
  if (!responseData || responseLength == 0 || responseLength > sizeof(lora_response_buffer)) {
    return;
  }

  // Copy response data to the static buffer
  memcpy(lora_response_buffer, responseData, responseLength);
  lora_response_buffer[responseLength] = '\0';
  lora_response_length = responseLength;

  // Calculate random slot delay and set target time
  uint32_t delay = lora_calculate_random_delay();
  lora_response_target_time = HAL_GetTick() + delay;
  lora_response_pending = true;
}

void lora_process_delayed_response(AppContext_t *context) {
  if (!lora_response_pending) return;

  if ((HAL_GetTick() - lora_response_target_time) < 0x80000000U) {
    // Time has elapsed — push the response through the TX event pipeline
    // so that processLoRaTx() prefixes our UID automatically
    Event_t txEvent;
    txEvent.type = EVENT_LORA_TX;
    txEvent.data = lora_response_buffer;
    eventQueue_push(txEvent);

    lora_response_pending = false;
    lora_response_length = 0;
  }
}

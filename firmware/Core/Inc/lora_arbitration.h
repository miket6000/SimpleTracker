#ifndef LORA_ARBITRATION_H
#define LORA_ARBITRATION_H

#include "app_context.h"
#include <stdint.h>
#include <stdbool.h>

// Configuration for random slot delay
// Note: LoRa packets can take up to 500ms to transmit, so slot width should be >= 500ms
#define LORA_MIN_SLOT_DELAY_MS   0     // Minimum delay in milliseconds
#define LORA_MAX_SLOT_DELAY_MS   5000  // Maximum delay in milliseconds (5 seconds for 9 devices)
#define LORA_SLOT_INTERVAL_MS    500   // Duration of each time slot in ms

/**
 * Schedule a delayed LoRa response using random slot allocation.
 * The response payload will be routed through EVENT_LORA_TX so that
 * the sender UID is automatically prefixed by processLoRaTx().
 *
 * @param responseData   Pointer to the response payload string (will be copied)
 * @param responseLength Length of the response data
 */
void lora_schedule_delayed_response(uint8_t *responseData, uint16_t responseLength);

/**
 * Poll for a pending delayed response and fire it when the delay expires.
 * Call this from the main loop. When the delay has elapsed it pushes an
 * EVENT_LORA_TX event rather than transmitting directly, keeping all
 * transmissions routed through processLoRaTx().
 *
 * @param context The application context (needed to push the TX event)
 */
void lora_process_delayed_response(AppContext_t *context);

/**
 * Calculate a random delay within the configured slot range.
 *
 * @return Random delay in milliseconds
 */
uint32_t lora_calculate_random_delay(void);

/**
 * Seed the pseudo-random number generator for slot delay calculation.
 *
 * @param seed The seed value (typically use the device UID for uniqueness)
 * Call this early in main() before any LoRa commands are received.
 */
void lora_seed_prng(uint32_t seed);

#endif // LORA_ARBITRATION_H

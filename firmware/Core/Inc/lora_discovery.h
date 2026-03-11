#ifndef LORA_DISCOVERY_H
#define LORA_DISCOVERY_H

#include <stdint.h>
#include <stdbool.h>

#define DISCOVERY_MAX_RESPONSES  10
#define DISCOVERY_UID_LEN        8
#define DISCOVERY_WINDOW_MS      6000  // Must be > LORA_MAX_SLOT_DELAY_MS + worst-case TX time

/**
 * Start a discovery window. Clears any previous results.
 */
void discovery_start(void);

/**
 * Add a discovered device response. Deduplicates by UID.
 *
 * @param uid    Pointer to 8-char UID string (not null-terminated required)
 * @param rssi   RSSI of the received response
 * @return true if added (new device), false if duplicate or full
 */
bool discovery_add_response(const char *uid, int16_t rssi);

/**
 * Check if the discovery window has elapsed.
 *
 * @return true if the window has closed
 */
bool discovery_is_complete(void);

/**
 * Check if a discovery is currently in progress.
 *
 * @return true if active
 */
bool discovery_is_active(void);

/**
 * Format all discovery results into a string for readback by the PC.
 * Format: "<count> <uid1>,<rssi1> <uid2>,<rssi2> ..."
 *
 * @param outBuffer  Buffer to write the formatted string into
 * @param maxLen     Size of the output buffer
 */
void discovery_get_results(char *outBuffer, uint16_t maxLen);

/**
 * Get the number of responses collected so far.
 *
 * @return number of unique devices discovered
 */
uint8_t discovery_get_count(void);

#endif // LORA_DISCOVERY_H

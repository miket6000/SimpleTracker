#ifndef LED_SEQUENCES_H
#define LED_SEQUENCES_H
#include <stdint.h>

extern const int8_t idle_sequence[];
extern const int8_t off_sequence[];

extern const int8_t gps_search_sequence[];
extern const int8_t gps_lock_sequence[];

extern const int8_t lora_rx_sequence[];
extern const int8_t lora_tx_sequence[];

#endif // LED_SEQUENCES_H


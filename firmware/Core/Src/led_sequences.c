#include "led_sequences.h"
#include "led.h"

const int8_t idle_sequence[] = {1, -1};
const int8_t off_sequence[] = {NOTHING, -1};

const int8_t gps_search_sequence[] = {1, NOTHING, -1};
const int8_t gps_lock_sequence[] = {2, NOTHING, -1};

const int8_t lora_rx_sequence[] = {2, NOTHING, -1};
const int8_t lora_tx_sequence[] = {1, NOTHING, -1};



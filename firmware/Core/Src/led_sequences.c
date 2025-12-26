#include "led_sequences.h"
#include "led.h"

const int8_t idle_sequence[] = {1, -1};
const int8_t off_sequence[] = {NOTHING, -1};

const int8_t gps_search_sequence[] = {1, 1, SHORT_PAUSE, -3};
const int8_t gps_lock_sequence[] = {1, 2, SHORT_PAUSE, -3};

const int8_t gs_connected_sequence[] = {2, 1, SHORT_PAUSE, -3};
const int8_t gs_fix_sequence[] = {2, 2, SHORT_PAUSE, -3};

const int8_t gs_waiting_sequence[] = {3, SHORT_PAUSE, -2};

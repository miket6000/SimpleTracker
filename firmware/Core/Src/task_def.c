/*
#include "task_def.h"
#include "usb.h"
#include <stdlib.h>
#include "gps.h"
#include "lora.h"
#include "gpio.h"
#include "led.h"
#include "setting.h"

#define UPDATE_PERIOD_MS  2000
#define HEADER_LEN        UID_STR_LENGTH
#define MESSAGE_LEN       (HEADER_LEN + NMEA_SENTENCE_SIZE)
#define MODE_TRANSMIT     0x0001
#define MODE_RECEIVE      0x0002

uint32_t uid = 0;
char uid_str[UID_STR_LENGTH];
LoRa hlora;
bool report = false;

void transmit(char *buffer, uint16_t len) {
  LoRa_transmit(&hlora, (uint8_t *)buffer, len, UPDATE_PERIOD_MS);
}


void task_lora_rx(void *param) {
  if (setting('m')->value & MODE_RECEIVE) {
    Buffer *lora_buffer = param;
    int16_t rssi = 0;
    lora_buffer->index = LoRa_receive(&hlora, lora_buffer->data, MESSAGE_LEN);
    if (lora_buffer->index > 0) {
      led_add_sequence(&led_green, flash_sequence);
      if (report) {
        print("-> ", 3);
        print((char *)lora_buffer->data, lora_buffer->index);
        print("RSSI: ", 6);
        rssi = LoRa_getRSSI(&hlora);
        print_int16(&rssi);
        print("\n", 1);
      }
    }
  }
}

void task_usb(void *param) {
   Deal with USB data 
  usb_process_buffers();
}

*/

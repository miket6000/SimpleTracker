#ifndef USB_H
#define USB_H

#include <stdbool.h>
#include "usb_device.h"
#include "usbd_cdc_if.h"

extern uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
extern uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

typedef struct {
  uint8_t tx_buffer[APP_RX_DATA_SIZE];
  uint8_t rx_buffer[APP_RX_DATA_SIZE];
  uint16_t tx_buffer_index;
  uint16_t rx_buffer_index;
  bool connected;
} usb_state_t;

void print(char *tx_buffer);
void print_byte(uint8_t byte);
void print_bytes(uint8_t *bytes, uint8_t len);

void USB_Connect(void);
void USB_Disconnect(void);
usb_state_t *USB_getStatePointer(void);
void MX_USB_PCD_Init(void);
#endif // USB_H

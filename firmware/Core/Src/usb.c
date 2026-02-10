#include "usb.h"

usb_state_t usb = {.rx_buffer_index = 0, .tx_buffer_index = 0, .connected = false};

static int min(int a, int b) {
  return a < b ? a : b;
}

void flush() {
  if (usb.connected) {
    if (usb.tx_buffer_index > 0) {
      if (CDC_Transmit_FS(UserTxBufferFS, usb.tx_buffer_index) == USBD_OK) {
        usb.tx_buffer_index = 0;
      }
    }  
  }    
}

void print(char *tx_buffer) {
  uint16_t tx_len = 0;
  // copy as much of the buffer as we can, truncate the rest.
  if (usb.connected) {
    tx_len = min(strlen(tx_buffer), sizeof(UserTxBufferFS) - usb.tx_buffer_index);

    memcpy(&UserTxBufferFS[usb.tx_buffer_index], tx_buffer, tx_len);
    usb.tx_buffer_index += tx_len;
    flush();
  }
}

void print_byte(uint8_t byte) {
    char buf[3];
    if (byte < 16) {
      buf[0] = '0';
      itoa(byte, buf+1, 16);
    } else {
      itoa(byte, buf, 16);
    }
    print(buf);
}

void print_bytes(uint8_t *bytes, uint8_t len) {
  for(int i = 0; i < len; i++) {
    print_byte(bytes[i]);
    print(" ");
  }
}

usb_state_t *USB_getStatePointer(void) {
  return &usb;
}

void USBD_CDC_RxHandler(uint8_t *rxBuffer, uint32_t len) {
  //DANGER - does not check for rx_buffer over run.
  memcpy(&usb.rx_buffer[usb.rx_buffer_index], rxBuffer, len);
  usb.rx_buffer_index += len;
}

void USB_Connect(void) {
  usb.connected = true;
}

void USB_Disconnect(void) {
  usb.connected = false;
}


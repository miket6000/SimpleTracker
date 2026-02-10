#include "usb.h"
#include "command.h"

void task_usb(void *param) {
  usb_state_t *usb = (usb_state_t *)param;
  if (usb->connected) {
    if (usb->rx_buffer_index > 0) {
      cmd_read_input((char *)usb->rx_buffer, usb->rx_buffer_index);
      usb->rx_buffer_index = 0;
    }
    flush();
  }  
}



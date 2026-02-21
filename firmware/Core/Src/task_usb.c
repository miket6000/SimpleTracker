#include "usb.h"
#include "app_context.h"

#include "command.h"

void task_usb(AppContext_t *context) {
  usb_state_t *usb = context->usb;
  if (usb->connected) {
    if (usb->rx_buffer_index > 0) {
      cmd_read_input((char *)usb->rx_buffer, usb->rx_buffer_index);
      usb->rx_buffer_index = 0;
    }
    flush();
  }  
}



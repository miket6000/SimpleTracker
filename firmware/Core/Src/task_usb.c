#include "usb.h"
#include "tusb.h"
#include "app_context.h"

#include "command.h"

void task_usb(AppContext_t *context)
{
    tud_task();  // MUST call often

    if (tud_cdc_connected())
    {
        while (tud_cdc_available())
        {
            uint8_t buf[64];
            uint32_t count = tud_cdc_read(buf, sizeof(buf));

            cmd_read_input((char *)buf, count);
        }
    }
}

#include "usb.h"
#include "tusb.h"

PCD_HandleTypeDef hpcd_USB_FS;

usb_state_t usb = {
    .rx_buffer_index = 0,
    .tx_buffer_index = 0,
    .connected = false
};

void print(char *str)
{
    if (!tud_cdc_connected()) return;

    uint32_t len = strlen(str);

    tud_cdc_write(str, len);
    tud_cdc_write_flush();
}

void print_byte(uint8_t byte)
{
    char buf[3];
    //snprintf(buf, sizeof(buf), "%02X", byte);
    if (byte < 0x10) {
      buf[0] = '0';
      itoa(byte, buf+1, 2);
    } else {
      itoa(byte, buf, 3);
    }
    
    print(buf);
}

void print_bytes(uint8_t *bytes, uint8_t len)
{
    for (int i = 0; i < len; i++)
    {
        print_byte(bytes[i]);
        print(" ");
    }
}

void USB_Connect(void)
{
    usb.connected = true;
}

void USB_Disconnect(void)
{
    usb.connected = false;
}

void MX_USB_PCD_Init(void)
{
  __HAL_RCC_USB_CLK_ENABLE();      // Enable USB peripheral clock

  hpcd_USB_FS.Instance = USB;

  hpcd_USB_FS.Init.dev_endpoints = 8;
  hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_FS.Init.ep0_mps = PCD_EP0MPS_64;
  hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_FS.Init.battery_charging_enable = DISABLE;

  if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
  {
    Error_Handler();
  }

  // Enable USB interrupt
  HAL_NVIC_SetPriority(USB_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USB_IRQn);
}

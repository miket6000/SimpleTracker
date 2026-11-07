// bl_descriptors.c — TinyUSB CDC descriptors for the bench bootloader.
//
// Same VID as the app; PID 0x4001 and product string "SimpleTracker BL" so
// host tooling can distinguish the bootloader port from the app port purely
// from enumeration (by-id path contains "_BL_"). Serial number is the same
// UID hash the app reports, so flash.py can match a unit across the
// app->BL->app re-enumerations that happen during an automated flash.

#include "tusb.h"
#include "boot_common.h"

// from bl_main.c: 8-char lowercase hex serial, hash of the 96-bit MCU UID
extern void uid_string(char buf[9]);

// UTF-16 string descriptor buffer
static uint16_t _desc_str[32];

// String table
static const char *string_desc_arr[] =
{
  (const char[]) { 0x09, 0x04 },  // 0: English (0x0409)
  "Blueskin Design Ltd",          // 1: Manufacturer
  "SimpleTracker BL",             // 2: Product
  NULL,                           // 3: Serial (filled from UID)
  "Bootloader CDC"                // 4: CDC Interface
};

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint8_t chr_count;

  if (index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }
  else {
    const char *str;

    if (index == 3)
    {
      char uid[9];
      uid_string(uid);
      str = uid;
    }
    else
    {
      if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
        return NULL;

      str = string_desc_arr[index];
    }

    // manual length: avoids a libc strlen reference
    chr_count = 0;
    while (str[chr_count] != 0) chr_count++;
    if (chr_count > 31) chr_count = 31;

    for (uint8_t i = 0; i < chr_count; i++)
    {
       _desc_str[1 + i] = str[i];
    }
  }

  // first word: descriptor type and length
  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2 * chr_count + 2);

  return _desc_str;
}

tusb_desc_device_t const desc_device =
{
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,
  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

  .idVendor           = 0xCafe,
  .idProduct          = 0x4001,
  .bcdDevice          = 0x0100,

  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,

  .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

enum {
  ITF_NUM_CDC = 0,
  ITF_NUM_CDC_DATA,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

uint8_t const desc_configuration[] =
{
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, 0x81, 8, 0x02, 0x82, 64)
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index;
  return desc_configuration;
}

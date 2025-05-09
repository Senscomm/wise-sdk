/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"
#include "tusb_main.h"
#include "class/dfu/dfu_rt_device.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )

#define USB_VID   0x3464
#define USB_BCD   0x0200

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
static tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,

    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum
{
#if CFG_TUD_DFU_RUNTIME
  ITF_NUM_DFU_RT,
#endif
#if CFG_TUD_VENDOR
  ITF_NUM_VENDOR,
#endif
  ITF_NUM_TOTAL
};

#if CFG_TUD_DFU_RUNTIME
#define CONFIG_DFU_RT_LEN	(TUD_DFU_RT_DESC_LEN)
#else
#define CONFIG_DFU_RT_LEN	(0)
#endif

#if CFG_TUD_VENDOR
#define CONFIG_VENDOR_LEN	(TUD_VENDOR_DESC_LEN)
#else
#define CONFIG_VENDOR_LEN	(0)
#endif

#define CONFIG_TOTAL_LEN	(TUD_CONFIG_DESC_LEN + CONFIG_DFU_RT_LEN + CONFIG_VENDOR_LEN)

#define FUNC_ATTRS (DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_MANIFESTATION_TOLERANT)

enum
{
  EPNUM_CTRL0 = 0,
  EPNUM_VENDOR_IN,
  EPNUM_VENDOR_OUT = EPNUM_VENDOR_IN,
};

uint8_t const desc_fs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

#if CFG_TUD_DFU_RUNTIME
  // Interface number, string index, attributes, detach timeout, transfer size */
  TUD_DFU_RT_DESCRIPTOR(ITF_NUM_DFU_RT, 4, FUNC_ATTRS, 1000, 4096),
#endif
#if CFG_TUD_VENDOR
  // Interface number, string index, EP Out & IN address, EP size
  TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, 7, EPNUM_VENDOR_OUT, 0x80 | EPNUM_VENDOR_IN, 64)
#endif
};

#if TUD_OPT_HIGH_SPEED
// Per USB specs: high speed capable device must report device_qualifier and other_speed_configuration

// high speed configuration
uint8_t const desc_hs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

#if CFG_TUD_DFU_RUNTIME
  // Interface number, string index, attributes, detach timeout, transfer size */
  TUD_DFU_RT_DESCRIPTOR(ITF_NUM_DFU_RT, 4, FUNC_ATTRS, 1000, 4096),
#endif
#if CFG_TUD_VENDOR
  /* XXX: TUD_VENDOR_DESCRIPTOR only supports a single pair of Endpoints. */
  /* Interface */
  9, TUSB_DESC_INTERFACE, ITF_NUM_VENDOR, 0, CFG_TUD_VENDOR_EPOUT_NUM+CFG_TUD_VENDOR_EPIN_NUM,
  TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, 7,
  /* Endpoint Out 1 */
  7, TUSB_DESC_ENDPOINT, EPNUM_VENDOR_OUT+0,   TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
  /* XXX: Endpoint Out 2 can't be used in HS because its FIFO is only 128 bytes. */
#if (CFG_TUD_VENDOR_EPOUT_NUM > 1)
  /* Endpoint Out 3 */
  7, TUSB_DESC_ENDPOINT, EPNUM_VENDOR_OUT+2,   TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
#endif
#if (CFG_TUD_VENDOR_EPOUT_NUM > 2)
  /* Endpoint Out 4 */
  7, TUSB_DESC_ENDPOINT, EPNUM_VENDOR_OUT+3,   TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
#endif
#if (CFG_TUD_VENDOR_EPOUT_NUM > 3)
  /* Endpoint Out 5 */
  7, TUSB_DESC_ENDPOINT, EPNUM_VENDOR_OUT+4,   TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
#endif
#if (CFG_TUD_VENDOR_EPOUT_NUM > 4)
  /* Endpoint Out 6 */
  7, TUSB_DESC_ENDPOINT, EPNUM_VENDOR_OUT+5,   TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
#endif
  /* Endpoint In 1 */
  7, TUSB_DESC_ENDPOINT, 0x80|EPNUM_VENDOR_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
#if (CFG_TUD_VENDOR_EPIN_NUM > 1)
  /* Endpoint In 5 */
  7, TUSB_DESC_ENDPOINT, 0x80|(EPNUM_VENDOR_IN+4), TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
#endif
#endif
};

// other speed configuration
uint8_t desc_other_speed_config[CONFIG_TOTAL_LEN];

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
tusb_desc_device_qualifier_t const desc_device_qualifier =
{
  .bLength            = sizeof(tusb_desc_device_qualifier_t),
  .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
  .bcdUSB             = USB_BCD,

  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,

  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .bNumConfigurations = 0x01,
  .bReserved          = 0x00
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
uint8_t const* tud_descriptor_device_qualifier_cb(void)
{
#if CFG_TUD_DFU
  if (tusb_dfu_detached()) {
    return NULL;
  }
#endif

  return (uint8_t const*) &desc_device_qualifier;
}

// Invoked when received GET OTHER SEED CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
// Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

#if CFG_TUD_DFU
  if (tusb_dfu_detached()) {
    return NULL;
  }
#endif

  // if link speed is high return fullspeed config, and vice versa
  // Note: the descriptor type is OHER_SPEED_CONFIG instead of CONFIG
  memcpy(desc_other_speed_config,
         (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration : desc_hs_configuration,
         CONFIG_TOTAL_LEN);

  desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;

  return desc_other_speed_config;
}

#endif // highspeed

#if CFG_TUD_DFU

// Number of Alternate Interface (each for 1 flash partition)
#define ALT_COUNT   2

#define CONFIG_TOTAL_LEN_DFU    (TUD_CONFIG_DESC_LEN + TUD_DFU_DESC_LEN(ALT_COUNT))

#define FUNC_ATTRS (DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_MANIFESTATION_TOLERANT)

static uint8_t const desc_configuration_dfu[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN_DFU, 0x00, 100),

  // Interface number, Alternate count, starting string index, attributes, detach timeout, transfer size
  TUD_DFU_DESCRIPTOR(0, ALT_COUNT, 5, FUNC_ATTRS, 1000, CFG_TUD_DFU_XFER_BUFSIZE),
};

#endif

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

#if CFG_TUD_DFU
  if (tusb_dfu_detached()) {
    return desc_configuration_dfu;
  }
#endif

#if TUD_OPT_HIGH_SPEED
  // Although we are highspeed, host may be fullspeed.
  return (tud_speed_get() == TUSB_SPEED_HIGH) ?  desc_hs_configuration : desc_fs_configuration;
#else
  return desc_fs_configuration;
#endif
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const* string_desc_arr [] =
{
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "Senscomm",                    // 1: Manufacturer
  "scm2010",              		 // 2: Product
  "123456789012",                // 3: Serials, should use chip ID
  "DFU RT",                      // 4: DFU runtime
  "FLASH",                       // 5: DFU Partition 1
  "EEPROM",                      // 6: DFU Partition 2
  "Vendor",                      // 7: Vendor
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint8_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];

    // Cap at max char
    chr_count = strlen(str);
    if ( chr_count > 31 ) chr_count = 31;

    // Convert ASCII string into UTF-16
    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

  return _desc_str;
}

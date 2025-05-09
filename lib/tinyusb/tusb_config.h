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

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// defined by autoconf.h
#ifdef CONFIG_SOC_SCM2010
  #define CFG_TUSB_MCU OPT_MCU_SCM2010
#else
  #error TinyUSB is not supported for CONFIG_SYS_SOC
#endif

#ifndef CFG_TUSB_MCU
  #error CFG_TUSB_MCU must be defined
#endif

#ifndef CONFIG_TUSB_DEVICE_RHPORT_NUM
  #define BOARD_DEVICE_RHPORT_NUM   0
#else
  #define BOARD_DEVICE_RHPORT_NUM   CONFIG_TUSB_DEVICE_RHPORT_NUM
#endif

#ifdef CONFIG_TUSB_DEVICE_SPEED_HIGH
  #define BOARD_DEVICE_RHPORT_SPEED OPT_MODE_HIGH_SPEED
#else
  #define BOARD_DEVICE_RHPORT_SPEED OPT_MODE_FULL_SPEED
#endif

// Device mode with rhport and speed
#if   BOARD_DEVICE_RHPORT_NUM == 0
  #define CFG_TUSB_RHPORT0_MODE     (OPT_MODE_DEVICE | BOARD_DEVICE_RHPORT_SPEED)
#elif CONFIG_TUSB_DEVICE_RHPORT_NUM == 1
  #define CFG_TUSB_RHPORT1_MODE     (OPT_MODE_DEVICE | BOARD_DEVICE_RHPORT_SPEED)
#else
  #error "Incorrect RHPort configuration"
#endif

#ifdef CONFIG_TUSB_OS_FREERTOS
  #define CFG_TUSB_OS               OPT_OS_FREERTOS
#elif defined(CONFIG_TUSB_OS_CMSIS)
  #define CFG_TUSB_OS               OPT_OS_CUSTOM
#else
  #error "Invalid OS specified"
#endif
#define CFG_TUSB_DEBUG              CONFIG_TUSB_DEBUG

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM_SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#define CFG_TUSB_MEM_SECTION       __attribute__ (( section(CONFIG_TUSB_MEM_SECTION) ))
#define CFG_TUSB_MEM_ALIGN         __attribute__ ((aligned(CONFIG_TUSB_MEM_ALIGN)))
#define CFG_TUSB_EPIN_SECTION       __attribute__ (( section(CONFIG_TUSB_EPIN_SECTION) ))
#define CFG_TUSB_EPIN_ALIGN         __attribute__ ((aligned(CONFIG_TUSB_EPIN_ALIGN)))

#define CFG_TUSB_DEBUG_PRINTF      CONFIG_TUSB_DEBUG_PRINT

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUD_ENDPOINT0_SIZE     CONFIG_TUSB_TUD_ENDPOINT0_SIZE

#ifdef CONFIG_TUSB_DMA
#define CFG_TUD_DMA                1
#define CFG_TUD_NUM_DMA_CHANNELS   CONFIG_TUSB_NUM_DMA_CHANNELS
#ifdef CONFIG_TUSB_DMA_EP0
#define CFG_TUD_DMA_EP0            1
#else
#define CFG_TUD_DMA_EP0            0
#endif
#ifdef CONFIG_TUSB_DMA_BULK_OUT_MODE0
#define CFG_TUD_DMA_BULK_OUT_MODE0    1
#else
#define CFG_TUD_DMA_BULK_OUT_MODE0    0
#endif
#ifdef CONFIG_TUSB_DMA_BULK_IN_MODE0
#define CFG_TUD_DMA_BULK_IN_MODE0    1
#else
#define CFG_TUD_DMA_BULK_IN_MODE0    0
#endif
#else
#define CFG_TUD_DMA                0
#define CFG_TUD_NUM_DMA_CHANNELS   0
#define CFG_TUD_DMA_EP0            0
#endif

#ifdef CONFIG_TUSB_WIFI_FILTER
#define CFG_TUD_WIFI_FILTER                1
#endif

//------------- CLASS -------------//

#ifdef CONFIG_TUSB_TUD_CDC
  #define CFG_TUD_CDC              1
#else
  #define CFG_TUD_CDC              0
#endif
#ifdef CONFIG_TUSB_TUD_MSC
  #define CFG_TUD_MSC              1
#else
  #define CFG_TUD_MSC              0
#endif
#ifdef CONFIG_TUSB_TUD_HID
  #define CFG_TUD_HID              1
#else
  #define CFG_TUD_HID              0
#endif
#ifdef CONFIG_TUSB_TUD_AUDIO
  #define CFG_TUD_AUDIO            1
#else
  #define CFG_TUD_AUDIO            0
#endif
#ifdef CONFIG_TUSB_TUD_VIDEO
  #define CFG_TUD_VIDEO            1
#else
  #define CFG_TUD_VIDEO            0
#endif
#ifdef CONFIG_TUSB_TUD_MIDI
  #define CFG_TUD_MIDI             1
#else
  #define CFG_TUD_MIDI             0
#endif
#ifdef CONFIG_TUSB_TUD_VENDOR
  #define CFG_TUD_VENDOR           1
  #ifdef CONFIG_TUD_VENDOR_TEST
    #define CFG_TUD_VENDOR_TEST      1
  #else
    #define CFG_TUD_VENDOR_TEST      0
  #endif
  #ifdef CONFIG_TUD_VENDOR_DUMP_BULK_STATUS
    #define CFG_TUD_VENDOR_DUMP_BULK_STATUS      1
  #else
    #define CFG_TUD_VENDOR_DUMP_BULK_STATUS      0
  #endif
  #define CFG_TUD_VENDOR_EPOUT_NUM   (CONFIG_TUD_VENDOR_EPOUT_NUM)
  #define CFG_TUD_VENDOR_EPIN_NUM    (CONFIG_TUD_VENDOR_EPIN_NUM)

#else
  #define CFG_TUD_VENDOR           0
#endif

#ifdef CONFIG_TUSB_TUD_USBTMC
  #define CFG_TUD_USBTMC           1
#else
  #define CFG_TUD_USBTMC           0
#endif
#ifdef CONFIG_TUSB_TUD_DFU_RUNTIME
  #define CFG_TUD_DFU_RUNTIME      1
#else
  #define CFG_TUD_DFU_RUNTIME      0
#endif
#ifdef CONFIG_TUSB_TUD_DFU
  #define CFG_TUD_DFU              1
#else
  #define CFG_TUD_DFU              0
#endif
#ifdef CONFIG_TUSB_TUD_BTH
  #define CFG_TUD_BTH              1
#else
  #define CFG_TUD_BTH              0
#endif
#ifdef CONFIG_TUSB_TUD_ECM_RNDIS
  #define CFG_TUD_ECM_RNDIS        1
#else
  #define CFG_TUD_ECM_RNDIS        0
#endif
#ifdef CONFIG_TUSB_TUD_NCM
  #define CFG_TUD_NCM              1
#else
  #define CFG_TUD_NCM              0
#endif

// DFU
#ifdef CONFIG_TUSB_TUD_DFU
#define CFG_TUD_DFU_XFER_BUFSIZE   CONFIG_TUD_DFU_XFER_BUFSIZE
#endif

// Vendor-specific
#ifdef CONFIG_TUSB_TUD_VENDOR
#define CFG_TUD_VENDOR_RX_BUFSIZE  CONFIG_TUD_VENDOR_RX_BUFSIZE
#define CFG_TUD_VENDOR_TX_BUFSIZE  CONFIG_TUD_VENDOR_TX_BUFSIZE
#define CFG_TUD_VENDOR_EPSIZE      CONFIG_TUD_VENDOR_EPSIZE
#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */

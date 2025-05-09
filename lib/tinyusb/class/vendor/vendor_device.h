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
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_VENDOR_DEVICE_H_
#define _TUSB_VENDOR_DEVICE_H_

#include "common/tusb_common.h"

#ifndef CFG_TUD_VENDOR_EPSIZE
#define CFG_TUD_VENDOR_EPSIZE     64
#endif

// Define a packet-oriented vendor Interfaces
// instead of stream-oriented one provided as a default.

#define CFG_TUD_VENDOR_PACKET_BASED

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Application API (Multiple Interfaces)
//--------------------------------------------------------------------+
int      tud_vendor_n_epout_idx       (uint8_t itf, uint8_t ep_addr);
uint8_t  tud_vendor_n_ep_addr         (uint8_t itf, int epout_idx);

bool     tud_vendor_n_mounted         (uint8_t itf);

uint32_t tud_vendor_n_available       (uint8_t itf, uint8_t ep_addr);
uint32_t tud_vendor_n_read            (uint8_t itf, uint8_t ep_addr, void* buffer, uint32_t bufsize);
bool     tud_vendor_n_peek            (uint8_t itf, uint8_t ep_addr, uint8_t* ui8, uint16_t n);
void     tud_vendor_n_read_flush      (uint8_t itf, uint8_t ep_addr);

#ifdef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
void     tud_vendor_n_write_epin      (uint8_t itf, void* buffer, uint32_t bufsize, bool isevent);
#endif

#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
uint32_t tud_vendor_n_write           (uint8_t itf, void const* buffer, uint32_t bufsize);
static inline
uint32_t tud_vendor_n_write_str       (uint8_t itf, char const* str);
uint32_t tud_vendor_n_write_available (uint8_t itf);

#ifdef CFG_TUD_VENDOR_PACKET_BASED

uint32_t tud_vendor_n_write_fifo      (uint8_t itf, void const* buffer, uint32_t bufsize);
void     tud_vendor_n_write_flush     (uint8_t itf);

#endif

bool     tud_vendor_n_write_empty     (uint8_t itf);
#endif

void     tud_vendor_n_get_read_info   (uint8_t itf, uint8_t ep_addr, bool rx, tu_fifo_buffer_info_t *info);
void     tud_vendor_n_get_write_info  (uint8_t itf, uint8_t ep_addr, bool rx, tu_fifo_buffer_info_t *info);
void     tud_vendor_n_advance_wptr    (uint8_t itf, uint8_t ep_addr, bool rx, uint16_t n);
void     tud_vendor_n_advance_rptr    (uint8_t itf, uint8_t ep_addr, bool rx, uint16_t n);
void     tud_vendor_n_prep_out_trans  (uint8_t itf, uint8_t ep_addr, bool async, bool in_isr);

#ifdef CONFIG_TUSB_BULK_OUT_DMA_CP_FIFO
void     tud_vendor_n_array_linearize (uint8_t itf, uint8_t ep_addr, bool rx, tu_fifo_buffer_info_t* pfinfo);
bool     tud_vendor_n_update_wrap     (uint8_t itf, uint8_t ep_addr,  bool rx, tu_fifo_buffer_info_t  *pre_finfo, tu_fifo_buffer_info_t *out_finfo);
bool     tud_vendor_fifo_linear_space_available(tu_fifo_t* f);
void     tud_vendor_n_advance_write   (tu_fifo_t* f, uint16_t n);

#endif

void vendor_dump_bulk_out_pkts(void);
void vendor_dump_fifo_usage(void);


//--------------------------------------------------------------------+
// Application API (Single Port)
//--------------------------------------------------------------------+
static inline bool     tud_vendor_mounted         (void);
static inline uint32_t tud_vendor_available       (uint8_t);
static inline uint32_t tud_vendor_read            (uint8_t, void* buffer, uint32_t bufsize);
static inline bool     tud_vendor_peek            (uint8_t, uint8_t* ui8, uint16_t n);
static inline void     tud_vendor_read_flush      (uint8_t);
#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
static inline uint32_t tud_vendor_write           (void const* buffer, uint32_t bufsize);
static inline uint32_t tud_vendor_write_str       (char const* str);
static inline uint32_t tud_vendor_write_available (void);
#ifdef CFG_TUD_VENDOR_PACKET_BASED

static inline uint32_t tud_vendor_write_fifo      (void const* buffer, uint32_t bufsize);
static inline void     tud_vendor_write_flush     (void);

#endif
static inline bool     tud_vendor_write_empty     (void);
#endif
static inline void	   tud_vendor_get_read_info   (uint8_t ep_addr, bool rx, tu_fifo_buffer_info_t *info);
static inline void	   tud_vendor_get_write_info  (uint8_t ep_addr, bool rx, tu_fifo_buffer_info_t *info);
static inline void     tud_vendor_advance_wptr    (uint8_t ep_addr, bool rx, uint16_t n);
static inline void     tud_vendor_advance_rptr    (uint8_t ep_addr, bool rx, uint16_t n);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

// Invoked when vendor class driver init
TU_ATTR_WEAK void tud_vendor_reg(void);

// Invoked when vendor class driver is opened
TU_ATTR_WEAK void tud_vendor_open(uint8_t itf);

// Invoked when received new data
TU_ATTR_WEAK void tud_vendor_rx_cb(uint8_t itf, uint8_t ep_addr, uint16_t n);

//--------------------------------------------------------------------+
// Inline Functions
//--------------------------------------------------------------------+

static inline int tud_vendor_epout_idx(uint8_t ep_addr)
{
  return tud_vendor_n_epout_idx(0, ep_addr);
}

static inline bool tud_vendor_mounted (void)
{
  return tud_vendor_n_mounted(0);
}

static inline uint32_t tud_vendor_available (uint8_t ep_addr)
{
  return tud_vendor_n_available(0, ep_addr);
}

static inline uint32_t tud_vendor_read (uint8_t ep_addr, void* buffer, uint32_t bufsize)
{
  return tud_vendor_n_read(0, ep_addr, buffer, bufsize);
}

static inline bool tud_vendor_peek (uint8_t ep_addr, uint8_t* ui8, uint16_t n)
{
  return tud_vendor_n_peek(0, ep_addr, ui8, n);
}

static inline void tud_vendor_read_flush(uint8_t ep_addr)
{
    tud_vendor_n_read_flush(0, ep_addr);
}

#ifdef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
static inline void tud_vendor_write_epin(void* buffer, uint32_t bufsize, bool isevent)
{
  tud_vendor_n_write_epin(0, buffer, bufsize, isevent);
}
#endif

#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
static inline uint32_t tud_vendor_n_write_str (uint8_t itf, char const* str)
{
  return tud_vendor_n_write(itf, str, strlen(str));
}

static inline uint32_t tud_vendor_write (void const* buffer, uint32_t bufsize)
{
  return tud_vendor_n_write(0, buffer, bufsize);
}

static inline uint32_t tud_vendor_write_str (char const* str)
{
  return tud_vendor_n_write_str(0, str);
}

static inline uint32_t tud_vendor_write_available (void)
{
  return tud_vendor_n_write_available(0);
}

#ifdef CFG_TUD_VENDOR_PACKET_BASED

static inline uint32_t tud_vendor_write_fifo (void const* buffer, uint32_t bufsize)
{
  return tud_vendor_n_write_fifo(0, buffer, bufsize);
}

static inline void     tud_vendor_write_flush(void)
{
    tud_vendor_n_write_flush(0);
}

#endif

static inline bool     tud_vendor_write_empty     (void)
{
  return tud_vendor_n_write_empty(0);
}
#endif

static inline void     tud_vendor_get_read_info   (uint8_t ep_addr, bool rx, tu_fifo_buffer_info_t *info)
{
  return tud_vendor_n_get_read_info(0, ep_addr, rx, info);
}

static inline void     tud_vendor_get_write_info  (uint8_t ep_addr, bool rx, tu_fifo_buffer_info_t *info)
{
  return tud_vendor_n_get_write_info(0, ep_addr, rx, info);
}

static inline void     tud_vendor_advance_wptr    (uint8_t ep_addr, bool rx, uint16_t n)
{
  return tud_vendor_n_advance_wptr(0, ep_addr, rx, n);
}

static inline void     tud_vendor_advance_rptr    (uint8_t ep_addr, bool rx, uint16_t n)
{
  return tud_vendor_n_advance_rptr(0, ep_addr, rx, n);
}

static inline void     tud_vendor_prep_out_trans  (uint8_t ep_addr, bool async, bool in_isr)
{
  return tud_vendor_n_prep_out_trans(0, ep_addr, async, in_isr);
}

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+

void     vendord_init(void);
void     vendord_reset(uint8_t rhport);
uint16_t vendord_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     vendord_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
bool     vendord_xfer(uint8_t rhport, uint8_t ep_addr);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_VENDOR_DEVICE_H_ */

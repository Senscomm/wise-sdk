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

#include "tusb_option.h"

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_VENDOR)

#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "device/dcd.h"

#include "vendor_device.h"
#include "scdc.h"


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
struct scdc_ops _vendord_scdc_ops;

typedef struct
{
  uint8_t itf_num;

  uint8_t ep_in[CFG_TUD_VENDOR_EPIN_NUM];
  uint8_t ep_in_count;
  uint8_t ep_in_next;
#ifdef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
  uint8_t ep_in_qnext;
#endif

  uint8_t ep_out[CFG_TUD_VENDOR_EPOUT_NUM];
  uint8_t *out_buf_desc[CFG_TUD_VENDOR_EPOUT_NUM];
  uint8_t *out_cache_buf[CFG_TUD_VENDOR_EPOUT_NUM];
  uint8_t ep_out_count;

  /*------------- From this point, data is not cleared by bus reset -------------*/
  tu_fifo_t rx_ff[CFG_TUD_VENDOR_EPOUT_NUM];

  uint8_t rx_ff_buf[CFG_TUD_VENDOR_EPOUT_NUM][CFG_TUD_VENDOR_RX_BUFSIZE];

#if CFG_FIFO_MUTEX
  osal_mutex_def_t rx_ff_mutex[CFG_TUD_VENDOR_EPOUT_NUM];
#endif

#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
  tu_fifo_t tx_ff[CFG_TUD_VENDOR_EPIN_NUM];

  uint8_t tx_ff_buf[CFG_TUD_VENDOR_EPIN_NUM][CFG_TUD_VENDOR_TX_BUFSIZE];

#if CFG_FIFO_MUTEX
  osal_mutex_def_t tx_ff_mutex[CFG_TUD_VENDOR_EPIN_NUM];
#endif
#endif

  // Endpoint Transfer buffer
#ifndef CONFIG_TUSB_BULK_OUT_DMA_CP_FIFO
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_VENDOR_EPOUT_NUM][CFG_TUD_VENDOR_EPSIZE];
#endif

#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_VENDOR_EPIN_NUM][CFG_TUD_VENDOR_EPSIZE];
#endif
} vendord_interface_t;

#define ITF_MEM_RESET_SIZE   offsetof(vendord_interface_t, rx_ff)

#ifdef CONFIG_TUSB_BULK_OUT_DMA_CP_FIFO

#define FIFO_BOUNDARY_ADDR(f, idx) ((&((f)->rx_ff_buf[idx][CFG_TUD_VENDOR_RX_BUFSIZE])) - CONFIG_TUSB_FIFO_MIN_LINEAR_SIZE)
#define FIFO_START_ADDR(f, idx) (&((f)->rx_ff_buf[idx][0]))
#define space(s, e, t, h) ({ \
  uint16_t __e; \
  if (h < t) \
    __e = t - h; \
  else \
    __e = (e - s) - (h - t); \
    __e; \
})

#endif

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION static vendord_interface_t _vendord_itf[CFG_TUD_VENDOR];
#ifdef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
    /* one more buffer to prepare next transmit in advance for each bulk-in */
  #define EPIN_Q_DEPTH 2
    CFG_TUSB_EPIN_SECTION CFG_TUSB_EPIN_ALIGN uint8_t _epin_buf[EPIN_Q_DEPTH][CFG_TUD_VENDOR_EPIN_NUM][CFG_TUD_VENDOR_EPSIZE];
#endif

static uint8_t ep_out_addr(vendord_interface_t *p_itf, int epout_idx)
{
  TU_ASSERT(epout_idx < p_itf->ep_out_count);
  return p_itf->ep_out[epout_idx];
}

static int ep_out_idx(vendord_interface_t *p_itf, uint8_t ep_addr)
{
  uint8_t i;

  for(i=0; i<p_itf->ep_out_count; i++)
  {
    if (p_itf->ep_out[i] == ep_addr)
    {
      break;
    }
  }

  return (i < p_itf->ep_out_count ? i : -1);
}

static bool is_epout(vendord_interface_t *p_itf, uint8_t ep_addr)
{
  return ep_out_idx(p_itf, ep_addr) != -1;
}

static tu_fifo_t* tud_vendor_n_get_fifo (uint8_t itf, uint8_t ep_addr, bool rx)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];

#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
  tu_fifo_t* f = rx ? &p_itf->rx_ff[ep_out_idx(p_itf, ep_addr)] : &p_itf->tx_ff[p_itf->ep_in_next];
#else
  if (!rx)
    return NULL;

  tu_fifo_t* f = &p_itf->rx_ff[ep_out_idx(p_itf, ep_addr)];
#endif
  return f;
}

#ifdef CONFIG_TUSB_BULK_OUT_DMA_CP_FIFO

void tud_vendor_n_array_linearize(uint8_t itf, uint8_t ep_addr, bool rx, tu_fifo_buffer_info_t* pfinfo)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  tu_fifo_t* f = tud_vendor_n_get_fifo (itf, ep_addr, rx);

  if (!f)
    return;

  if (((uint8_t *)pfinfo->ptr_lin) >
    (FIFO_BOUNDARY_ADDR(p_itf, ep_out_idx(p_itf, ep_addr))))
  {
    // pre list contains a wrap packet
    if (f->len_waste)
    {
      TU_ASSERT(((uint32_t)(pfinfo->ptr_lin + f->len_waste) >= (uint32_t) &p_itf->rx_ff_buf[ep_out_idx(p_itf, ep_addr)][CFG_TUD_VENDOR_RX_BUFSIZE]), );
      pfinfo->ptr_lin = (pfinfo->ptr_lin + f->len_waste) - f->depth;

      TU_LOG2("%s ep[%d] already wrap = %d pfinfo->ptr_lin = 0x%X\n",
          __func__, ep_out_idx(p_itf, ep_addr), f->len_waste, pfinfo->ptr_lin);
    }
    else if (!f->len_waste)
    {
      f->len_waste = &p_itf->rx_ff_buf[ep_out_idx(p_itf, ep_addr)][CFG_TUD_VENDOR_RX_BUFSIZE] - (uint8_t *)pfinfo->ptr_lin;

      TU_LOG2("%s ep[%d] just cross boundary len_waste = %d pfinfo->ptr_lin = 0x%X\n",
          __func__, ep_out_idx(p_itf, ep_addr), f->len_waste, pfinfo->ptr_lin);

      pfinfo->ptr_lin = &f->buffer[0];
    }
    else
      TU_ASSERT(0, );
  }
}

static uint16_t tud_vendor_n_array_advance_write(uint8_t itf, uint8_t ep_addr, bool rx, uint16_t n)
{
  tu_fifo_t* f = tud_vendor_n_get_fifo (itf, ep_addr, rx);

  if (!f)
    return 0;

  if ( n == 0 ){
    TU_LOG1("%s Error n = 0\n",__func__);
    return 0;
  }

  tu_fifo_array_advance_write_locked(f, n);

  return n;

}

__inline__ static uint8_t *vendord_get_buf(uint8_t itf, int outidx, uint8_t op)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  uint8_t *buf = NULL;

  if (op != SCDC_CACHE_BUF && op != SCDC_BUF_DESC)
    return NULL;

  if (p_itf)
    buf = (op == SCDC_CACHE_BUF ? p_itf->out_cache_buf[outidx] : p_itf->out_buf_desc[outidx]);

  return buf;
}

__inline__ static void vendord_set_buf(uint8_t itf, int outidx, uint8_t op, uint8_t *value)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];

  if (p_itf) {
    if (op == SCDC_CACHE_BUF)
      p_itf->out_cache_buf[outidx] = value;
    else if (op == SCDC_BUF_DESC)
      p_itf->out_buf_desc[outidx] = value;
  }
}

__inline__ static int tud_vendor_get_buf(int size, uint32_t *offset, uint8_t **buf, uint8_t **buf_desc, uint8_t op)
{
  return scdc_vendor_get_buf(size , offset, buf, buf_desc, op);
}

#endif /* CONFIG_TUSB_BULK_OUT_DMA_CP_FIFO */

static void _prep_out_transaction (vendord_interface_t* p_itf, uint8_t ep_addr)
{
  uint8_t const rhport = TUD_OPT_RHPORT;
  uint8_t epout = ep_out_idx(p_itf, ep_addr);
  uint8_t *pbuffer;
  uint16_t out_bytes;
  uint32_t offset = 0;

  TU_ASSERT( epout != -1 ,);

  // skip if previous transfer not complete
  if ( usbd_edpt_busy(rhport, p_itf->ep_out[epout]) ) return;

#ifdef CONFIG_TUSB_BULK_OUT_DMA_CP_FIFO

    /*
     * If there is no enough linear space try to alloc external buffer from scdc
     */
    if (!tu_fifo_array_linear_space_available(&p_itf->rx_ff[epout])) {

#ifdef CONFIG_TUSB_BULK_OUT_DMA_CP_EXT_BUF
      if (p_itf->out_cache_buf[epout]) {
        return;
      }
      /*
       * try to get buffer descriptor and external buffer
       */
      if(tud_vendor_get_buf(CONFIG_TUSB_FIFO_MIN_LINEAR_SIZE, &offset, &p_itf->out_cache_buf[epout],
        &p_itf->out_buf_desc[epout], SCDC_BUF_DESC | SCDC_CACHE_BUF)) {
          return;
        }
#endif

      if (p_itf->out_buf_desc[epout] == NULL || p_itf->out_cache_buf[epout] == NULL)
        return;

      pbuffer = (uint8_t *)(p_itf->out_cache_buf[epout] + offset);
      out_bytes = CONFIG_TUSB_FIFO_MIN_LINEAR_SIZE;

    } else {

      /*
       * must confirm that there are available buffer descriptor
       */
      if (tud_vendor_get_buf(0, NULL,
          NULL, &p_itf->out_buf_desc[epout], SCDC_BUF_DESC))
        return;

      pbuffer = tu_fifo_array_get_write_buf(&p_itf->rx_ff[epout]);
      out_bytes = CONFIG_TUSB_FIFO_MIN_LINEAR_SIZE;
    }



#else
    // Prepare for incoming data but only allow what we can store in the ring buffer.
    // TODO Actually we can still carry out the transfer, keeping count of received bytes
    // and slowly move it to the FIFO when read().
    // This pre-check reduces endpoint claiming
    if (tu_fifo_remaining(&p_itf->rx_ff[epout]) < sizeof(p_itf->epout_buf[epout]))
    {
      return;
    }
    pbuffer = p_itf->epout_buf[epout];
    out_bytes = sizeof(p_itf->epout_buf[epout]);
#endif

  usbd_edpt_xfer(rhport, p_itf->ep_out[epout], pbuffer, out_bytes);

}

static int ep_in_idx(vendord_interface_t *p_itf, uint8_t ep_addr)
{
  uint8_t i;

  for(i=0; i<p_itf->ep_in_count; i++)
  {
    if (p_itf->ep_in[i] == ep_addr)
    {
      break;
    }
  }

  return (i < p_itf->ep_in_count ? i : -1);
}

static bool is_epin(vendord_interface_t *p_itf, uint8_t ep_addr)
{
  return ep_in_idx(p_itf, ep_addr) != -1;
}

static void advance_epin(vendord_interface_t *p_itf)
{
  p_itf->ep_in_next++;
  if (p_itf->ep_in_next == p_itf->ep_in_count)
  {
    p_itf->ep_in_next = 0;
#ifdef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
    p_itf->ep_in_qnext++;
    if (p_itf->ep_in_qnext == EPIN_Q_DEPTH)
    {
      p_itf->ep_in_qnext = 0;
    }
#endif
  }
}

#ifdef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
static bool force_transmit_in(vendord_interface_t* p_itf, uint8_t epin, uint8_t qidx, uint32_t count)
{
  if (count > 0)
  {
    TU_ASSERT( usbd_edpt_xfer(TUD_OPT_RHPORT, p_itf->ep_in[epin], _epin_buf[qidx][epin], count) );
  }

  return true;
}
#endif

#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
static bool maybe_transmit(vendord_interface_t* p_itf, uint8_t epin)
{
  // skip if previous transfer not complete
  TU_VERIFY( !usbd_edpt_busy(TUD_OPT_RHPORT, p_itf->ep_in[epin]) );

  uint16_t count = tu_fifo_read_n(&p_itf->tx_ff[epin], p_itf->epin_buf[epin], sizeof(p_itf->epin_buf[epin]));
  if (count > 0)
  {
    TU_ASSERT( usbd_edpt_xfer(TUD_OPT_RHPORT, p_itf->ep_in[epin], p_itf->epin_buf[epin], count) );
  }
  return true;
}
#endif

#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
#ifdef CFG_TUD_VENDOR_PACKET_BASED

static uint32_t force_transmit(vendord_interface_t* p_itf, uint8_t epin, bool flush)
{
  uint32_t count;

  // wait if previous transfer not complete
  while ( usbd_edpt_busy(TUD_OPT_RHPORT, p_itf->ep_in[epin]) );

  if (flush)
  {
    return 0;
  }

  TU_ASSERT(tu_fifo_count(&p_itf->tx_ff[epin]) <= sizeof(p_itf->epin_buf[epin]));

  count = tu_fifo_read_n(&p_itf->tx_ff[epin], p_itf->epin_buf[epin], sizeof(p_itf->epin_buf[epin]));
  if (count)
  {
    TU_ASSERT( usbd_edpt_xfer(TUD_OPT_RHPORT, p_itf->ep_in[epin], p_itf->epin_buf[epin], count) );
  }

  return count;
}
#endif
#endif

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

int tud_vendor_n_epout_idx (uint8_t itf, uint8_t ep_addr)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  return ep_out_idx(p_itf, ep_addr);
}

uint8_t tud_vendor_n_ep_addr (uint8_t itf, int epout_idx)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  return ep_out_addr(p_itf, epout_idx);
}

bool tud_vendor_n_mounted (uint8_t itf)
{
  return _vendord_itf[itf].ep_in_count && _vendord_itf[itf].ep_out_count;
}


//--------------------------------------------------------------------+
// Read API
//--------------------------------------------------------------------+

uint32_t tud_vendor_n_available (uint8_t itf, uint8_t ep_addr)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  return tu_fifo_count(&_vendord_itf[itf].rx_ff[ep_out_idx(p_itf, ep_addr)]);
}

uint32_t tud_vendor_n_read (uint8_t itf, uint8_t ep_addr, void* buffer, uint32_t bufsize)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  uint32_t num_read = tu_fifo_read_n(&p_itf->rx_ff[ep_out_idx(p_itf, ep_addr)], buffer, bufsize);
  _prep_out_transaction(p_itf, ep_addr);
  return num_read;
}

bool tud_vendor_n_peek(uint8_t itf, uint8_t ep_addr, uint8_t* u8, uint16_t n)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  return tu_fifo_peek_n(&p_itf->rx_ff[ep_out_idx(p_itf, ep_addr)], u8, n);
}

void tud_vendor_n_read_flush (uint8_t itf, uint8_t ep_addr)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  tu_fifo_clear(&p_itf->rx_ff[ep_out_idx(p_itf, ep_addr)]);
  _prep_out_transaction(p_itf, ep_addr);
}

//--------------------------------------------------------------------+
// Write API
//--------------------------------------------------------------------+

#ifdef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF

#include <hal/dma.h>
#include "mbuf.h"

#define MASK_DMA_ISR	true
#define DMA_DESC_NUM	CONFIG_SCM2010_DMA_DESC_NUM

static int tud_vendor_epin_copy(void* buf, int len, void* cp, bool ismemcpy)
{
  struct device *dma_dev = device_get_by_name("dmac.0");
  u_int count;
  int desc_idx = 0;
  struct dma_desc_chain dma_desc[DMA_DESC_NUM];
  int remainder = 0;
  const struct mbuf *m0, *m;
  bool remain_m_copydata = false;
  void* cp0 = cp;

  m = m0 = (struct mbuf *) buf;

  TU_ASSERT(len >= 0);

  if (ismemcpy) {
    memcpy(cp, buf, len);
    return 0;
  }

#ifdef CONFIG_SUPPORT_RX_AMSDU_SPLIT_HEAP
  if (m0->m_flags & M_HEAP) {
    m_copydata(m0, 0, m0->m_pkthdr.len, (caddr_t)cp0);
    return 0;
  }
#endif

  while (len > 0) {
    TU_ASSERT(m != NULL);
    count = min(m->m_len, len);

    if (count == 0) {
		m = m->m_next;
		continue;
    }
    /* desc_idx should not large than DMA_DESC_NUM, use manual copy just in case */
    if (desc_idx >= DMA_DESC_NUM) {
      remain_m_copydata = true;
      break;
    }

    dma_desc[desc_idx].dst_addr = (u32) cp;
    dma_desc[desc_idx].src_addr = (u32) (mtod(m, caddr_t));
    dma_desc[desc_idx].len = count;
    remainder |= ((dma_desc[desc_idx].src_addr & 0x3) | (dma_desc[desc_idx].dst_addr & 0x3));

    len -= count;
    cp += count;
    m = m->m_next;
    desc_idx++;

    TU_LOG2("[%s, %d] len:%d, count:%d, cp: 0x%x, idx:%d\n", __func__, __LINE__, len, count, cp, desc_idx);
  }

  if (dma_copy(dma_dev, false, false, dma_desc, desc_idx, MASK_DMA_ISR, remainder, NULL, NULL)) {
    m_copydata(m0, 0, m0->m_pkthdr.len, (caddr_t)cp0);
    return 0;
  }

  if (remain_m_copydata) {
    /* manual copy the remaining len */
    m_copydata(m, 0, len, (caddr_t)cp);
  }

  return 0;
}

void tud_vendor_n_write_epin(uint8_t itf, void* buffer, uint32_t bufsize, bool isevent)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];

  uint8_t epin = p_itf->ep_in_next;
  uint8_t qidx = p_itf->ep_in_qnext;

  tud_vendor_epin_copy(buffer, bufsize, (void *)_epin_buf[qidx][epin], isevent);

  // wait if previous transfer not complete
  while ( usbd_edpt_busy(TUD_OPT_RHPORT, p_itf->ep_in[epin]) );

  force_transmit_in(p_itf, epin, qidx, bufsize);
  advance_epin(p_itf);
}
#endif

#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
uint32_t tud_vendor_n_write (uint8_t itf, void const* buffer, uint32_t bufsize)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  uint16_t ret = tu_fifo_write_n(&p_itf->tx_ff[p_itf->ep_in_next], buffer, bufsize);
  maybe_transmit(p_itf, p_itf->ep_in_next);
  advance_epin(p_itf);
  return ret;
}

uint32_t tud_vendor_n_write_available (uint8_t itf)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  return tu_fifo_remaining(&_vendord_itf[itf].tx_ff[p_itf->ep_in_next]);
}

#ifdef CFG_TUD_VENDOR_PACKET_BASED

/* XXX: tud_vendor_n_write_fifo shall always be followed by tud_vendor_n_write_flush. */
uint32_t tud_vendor_n_write_fifo (uint8_t itf, void const* buffer, uint32_t bufsize)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  uint16_t ret = tu_fifo_write_n(&p_itf->tx_ff[p_itf->ep_in_next], buffer, bufsize);
  return ret;
}

void tud_vendor_n_write_flush (uint8_t itf)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  force_transmit(p_itf, p_itf->ep_in_next, false);
  advance_epin(p_itf);
}

#endif

bool tud_vendor_n_write_empty (uint8_t itf)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  return tu_fifo_empty(&_vendord_itf[itf].tx_ff[p_itf->ep_in_next]);
}
#endif

void tud_vendor_n_get_read_info (uint8_t itf, uint8_t ep_addr, bool rx, tu_fifo_buffer_info_t *info)
{
  tu_fifo_t* f = tud_vendor_n_get_fifo (itf, ep_addr, rx);

  if (!f)
    return;

#ifdef CONFIG_TUSB_BULK_OUT_DMA_CP_FIFO
  tu_fifo_array_get_read_linear_info(f, info);
#else
  tu_fifo_get_read_info(f, info);
#endif

}

void tud_vendor_n_get_write_info (uint8_t itf, uint8_t ep_addr,  bool rx, tu_fifo_buffer_info_t *info)
{
  tu_fifo_t* f = tud_vendor_n_get_fifo (itf, ep_addr, rx);

  if (!f)
    return;

  tu_fifo_get_write_info(f, info);
}

void tud_vendor_n_advance_wptr (uint8_t itf, uint8_t ep_addr,  bool rx, uint16_t n)
{
  tu_fifo_t* f = tud_vendor_n_get_fifo (itf, ep_addr, rx);

  if (!f)
    return;

  tu_fifo_advance_write_pointer_locked(f, n);
}

void tud_vendor_n_advance_rptr (uint8_t itf, uint8_t ep_addr,  bool rx, uint16_t n)
{
  tu_fifo_t* f = tud_vendor_n_get_fifo (itf, ep_addr, rx);

  if (!f)
    return;

#ifdef CONFIG_TUSB_BULK_OUT_DMA_CP_FIFO
  tu_fifo_array_advance_read_locked(f, n);
#else
  tu_fifo_advance_read_pointer_locked(f, n);
#endif
}

void tud_vendor_n_prep_out_trans(uint8_t itf, uint8_t ep_addr, bool async,
		bool in_isr)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];

  if (async)
  {
    uint8_t const rhport = TUD_OPT_RHPORT;
    dcd_event_xfer(rhport, ep_addr, in_isr);
  }
  else
  {
    _prep_out_transaction(p_itf, ep_addr);
  }
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+

void tud_vendor_reg(void)
{
  if (scdc_vendor_reg) scdc_vendor_reg(&_vendord_scdc_ops);
}

void vendord_init(void)
{
  tu_memclr(_vendord_itf, sizeof(_vendord_itf));

  for(uint8_t i=0; i<CFG_TUD_VENDOR; i++)
  {
    vendord_interface_t* p_itf = &_vendord_itf[i];

    // config rx
    for(uint8_t j=0; j<CFG_TUD_VENDOR_EPOUT_NUM; j++)
    {
      tu_fifo_config(&p_itf->rx_ff[j], p_itf->rx_ff_buf[j], CFG_TUD_VENDOR_RX_BUFSIZE, 1, false);
      p_itf->out_buf_desc[j] = NULL;
      p_itf->out_cache_buf[j] = NULL;
    }

#if CFG_FIFO_MUTEX
    for(uint8_t j=0; j<CFG_TUD_VENDOR_EPOUT_NUM; j++)
    {
      tu_fifo_config_mutex(&p_itf->rx_ff[j], NULL, osal_mutex_create(&p_itf->rx_ff_mutex[j]));
    }
#endif

#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
    // config tx
    for(uint8_t j=0; j<CFG_TUD_VENDOR_EPIN_NUM; j++)
    {
      tu_fifo_config(&p_itf->tx_ff[j], p_itf->tx_ff_buf[j], CFG_TUD_VENDOR_TX_BUFSIZE, 1, false);
    }

#if CFG_FIFO_MUTEX
    for(uint8_t j=0; j<CFG_TUD_VENDOR_EPIN_NUM; j++)
    {
      tu_fifo_config_mutex(&p_itf->tx_ff[j], osal_mutex_create(&p_itf->tx_ff_mutex[j]), NULL);
    }
#endif
#endif
  }

  if (tud_vendor_reg) tud_vendor_reg();

}

void vendord_reset(uint8_t rhport)
{
  (void) rhport;

  for(uint8_t i=0; i<CFG_TUD_VENDOR; i++)
  {
    vendord_interface_t* p_itf = &_vendord_itf[i];

#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
#ifdef CFG_TUD_VENDOR_PACKET_BASED

    // Flush before initialization
    for(uint8_t j=0; j<p_itf->ep_in_count; j++)
	{
      force_transmit(p_itf, j, true);
	}

#endif
#endif

    tu_memclr(p_itf, ITF_MEM_RESET_SIZE);
    for(uint8_t j=0; j<CFG_TUD_VENDOR_EPOUT_NUM; j++)
	{
      tu_fifo_clear(&p_itf->rx_ff[j]);
	}
#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
    for(uint8_t j=0; j<CFG_TUD_VENDOR_EPIN_NUM; j++)
	{
      tu_fifo_clear(&p_itf->tx_ff[j]);
	}
#endif
  }
}

// Parse consecutive endpoint descriptors (IN & OUT)
static bool vendord_open_edpt_pair(uint8_t rhport, uint8_t const* p_desc, uint8_t ep_count,
		vendord_interface_t *p_itf)
{

  uint8_t epout = 0, epin = 0;

  for(uint8_t i=0; i<ep_count; i++)
  {
    tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;

    TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType && TUSB_XFER_BULK == desc_ep->bmAttributes.xfer);
    TU_ASSERT(usbd_edpt_open(rhport, desc_ep));

    if ( tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN )
    {
	  TU_ASSERT(epin < TU_ARRAY_SIZE(p_itf->ep_in));
      p_itf->ep_in[epin] = desc_ep->bEndpointAddress;
	  epin++;
    }
	else
    {
	  TU_ASSERT(epout < TU_ARRAY_SIZE(p_itf->ep_out));
      p_itf->ep_out[epout] = desc_ep->bEndpointAddress;
	  epout++;
    }

    p_desc = tu_desc_next(p_desc);
  }
  p_itf->ep_out_count = epout;
  p_itf->ep_in_count = epin;
  p_itf->ep_in_next = 0;
#ifdef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
  p_itf->ep_in_qnext = 0;
#endif

  return true;
}

void tud_vendor_open(uint8_t itf)
{
	if (scdc_vendor_open) scdc_vendor_open(itf);
}

uint16_t vendord_open(uint8_t rhport, tusb_desc_interface_t const * desc_itf, uint16_t max_len)
{
  TU_VERIFY(TUSB_CLASS_VENDOR_SPECIFIC == desc_itf->bInterfaceClass, 0);

  uint8_t const * p_desc = tu_desc_next(desc_itf);
  uint8_t const * desc_end = p_desc + max_len;

  // Find available interface
  vendord_interface_t* p_itf = NULL;
  for(uint8_t i=0; i<CFG_TUD_VENDOR; i++)
  {
    if ( _vendord_itf[i].ep_in_count == 0 && _vendord_itf[i].ep_out_count == 0 )
    {
      p_itf = &_vendord_itf[i];
      break;
    }
  }
  TU_VERIFY(p_itf, 0);

  p_itf->itf_num = desc_itf->bInterfaceNumber;
  if (desc_itf->bNumEndpoints)
  {
    // skip non-endpoint descriptors
    while ( (TUSB_DESC_ENDPOINT != tu_desc_type(p_desc)) && (p_desc < desc_end) )
    {
      p_desc = tu_desc_next(p_desc);
    }

    // Open endpoint pair
    TU_ASSERT(vendord_open_edpt_pair(rhport, p_desc, desc_itf->bNumEndpoints, p_itf), 0);

    p_desc += desc_itf->bNumEndpoints*sizeof(tusb_desc_endpoint_t);

    // Prepare for incoming data
    if ( p_itf->ep_out_count )
    {
      for(uint8_t i=0; i<p_itf->ep_out_count; i++)
      {
#ifdef CONFIG_TUSB_BULK_OUT_DMA_CP_FIFO
      TU_ASSERT(usbd_edpt_xfer(rhport, p_itf->ep_out[i], &p_itf->rx_ff_buf[i][0], CONFIG_TUSB_FIFO_MIN_LINEAR_SIZE), 0);
#else
      TU_ASSERT(usbd_edpt_xfer(rhport, p_itf->ep_out[i], p_itf->epout_buf[i], sizeof(p_itf->epout_buf[i])), 0);
#endif
      }
    }

#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
    if ( p_itf->ep_in_count )
    {
	  for(uint8_t i=0; i<p_itf->ep_in_count; i++)
	  {
	    maybe_transmit(p_itf, i);
	  }
	}
#endif
  }

  // Invoked callback if any
  if (tud_vendor_open) tud_vendor_open(p_itf->itf_num);

  return (uintptr_t) p_desc - (uintptr_t) desc_itf;
}

/*
 * Host-to-target: control and data
 */

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  void *msgbuf = NULL;
  bool ret = true;
  // Handle class request only
  TU_VERIFY(request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR);

  // Get ptr of msg buffer
  scdc_vendor_control_xfer_cb(request->wIndex, SCDC_REQUEST_GET, &msgbuf, request->wLength);

  if (stage == CONTROL_STAGE_ACK) {
    if (request->bRequest == SCDC_REQUEST_SET) {
      ret = scdc_vendor_control_xfer_cb(request->wIndex, SCDC_REQUEST_SET, &msgbuf, request->wLength);
    }
  }
  // No matter set or get, assign parameters into _ctrl_xfer at CONTROL_STAGE_SETUP stage
  else if (stage == CONTROL_STAGE_SETUP) {
    if (request->wLength)
      ret = tud_control_xfer(rhport, request, msgbuf, request->wLength);
  }

  return ret;
}

#if !CFG_TUD_VENDOR_TEST
void tud_vendor_rx_cb(uint8_t itf, uint8_t ep_addr, uint16_t n)
{
    int epout = tud_vendor_n_epout_idx(itf, ep_addr);
    if (scdc_vendor_rx_cb) scdc_vendor_rx_cb(itf, epout, n, NULL);
}
#endif

bool vendord_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) rhport;
  (void) result;

  uint8_t itf = 0;
  vendord_interface_t* p_itf = _vendord_itf;

  for ( ; ; itf++, p_itf++)
  {
    if (itf >= TU_ARRAY_SIZE(_vendord_itf)) return false;

    if ( ( is_epout(p_itf, ep_addr) ) || ( is_epin(p_itf, ep_addr) ) ) break;
  }

  if ( is_epin(p_itf, ep_addr) )
  {

#ifndef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
#ifndef CFG_TUD_VENDOR_PACKET_BASED
    // Send complete, try to send more if possible
    maybe_transmit(p_itf, ep_in_idx(p_itf, ep_addr));
#endif
#endif
  }
  else
  {
    uint16_t n = 0;
    uint8_t epout = ep_out_idx(p_itf, ep_addr);

#ifdef CONFIG_TUSB_BULK_OUT_DMA_CP_FIFO

    if (p_itf->out_cache_buf[epout]) {
      n = xferred_bytes;
    } else {
      // Receive new date and advance write index for next data
      n = tud_vendor_n_array_advance_write(itf, ep_addr, true, xferred_bytes);
    }

#else
    // Receive new data
    n = tu_fifo_write_n(&p_itf->rx_ff[epout], p_itf->epout_buf[epout], xferred_bytes);
#endif

    // Invoked callback if any
    if (tud_vendor_rx_cb) tud_vendor_rx_cb(itf, ep_addr, n);
    else _prep_out_transaction(p_itf, ep_addr);
  }

  return true;
}

bool vendord_xfer(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t itf = 0;
  vendord_interface_t* p_itf = _vendord_itf;

  for ( ; ; itf++, p_itf++)
  {
    if (itf >= TU_ARRAY_SIZE(_vendord_itf)) return false;

    if ( is_epout(p_itf, ep_addr) || ( is_epin(p_itf, ep_addr) ) ) break;
  }

  if ( is_epout(p_itf, ep_addr) )
  {
    _prep_out_transaction(p_itf, ep_addr);
  }

  return true;
}

#ifdef CONFIG_SUPPORT_SCDC
//--------------------------------------------------------------------+
// SCDC API
//--------------------------------------------------------------------+

void vendord_write(void* buffer, uint32_t bufsize, bool isevent)
{
#ifdef CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
  TU_LOG2("[%s, %d] totlen:%d\n", __func__, __LINE__, bufsize);
  if (bufsize > CFG_TUD_VENDOR_EPSIZE) {
    return;
  }
  tud_vendor_n_write_epin(0, buffer, bufsize, isevent);
#else
  if (isevent) {
    uint32_t written = 0;
    while (written < bufsize)
    {
      written += tud_vendor_write_fifo(buffer + written,
                    bufsize - written);
    }
    tud_vendor_write_flush();
  }
  else
  {
    struct mbuf *m0 = (struct mbuf *)buffer;
    uint32_t totlen;
    struct mbuf *m;
    uint32_t written, len;

    totlen = m0->m_pkthdr.len;
    written = 0;
    for (m = buffer; m != NULL; m = m->m_next)
    {
      len = 0;
      while (len < m->m_len)
      {
        len += tud_vendor_write_fifo(mtod(m, uint8_t *) + len,
          m->m_len - len);
      }
      written += m->m_len;
    }

    TU_LOG2("[%s, %d] written:%d(%d)\n", __func__, __LINE__,
              written, totlen);
    TU_ASSERT(totlen == written,);
    tud_vendor_write_flush();
  }
#endif
}

void vendord_get_read_info (uint8_t itf, uint8_t outidx, void *pinfo)
{
  uint8_t ep_addr = tud_vendor_n_ep_addr(itf, outidx);
  tud_vendor_n_get_read_info (itf, ep_addr, true, pinfo);
}

void vendord_array_linearize(uint8_t itf, uint8_t outidx, void* pfinfo)
{
  uint8_t ep_addr = tud_vendor_n_ep_addr(itf, outidx);
  tud_vendor_n_array_linearize(itf, ep_addr, true, (tu_fifo_buffer_info_t*) pfinfo);
}

void vendor_n_advance_rptr (uint8_t itf, uint8_t outidx, uint16_t n)
{
  uint8_t ep_addr = tud_vendor_n_ep_addr(itf, outidx);
  tud_vendor_n_advance_rptr (itf, ep_addr, true, n);
}

void vendord_prep_out(uint8_t itf, uint8_t outidx, bool async, void *pinfo)
{
  uint8_t ep_addr = tud_vendor_n_ep_addr(itf, outidx);
  tud_vendor_n_prep_out_trans(itf, ep_addr, async, false);
}

uint8_t vendord_get_out_pipe_num(void)
{
  return CFG_TUD_VENDOR_EPOUT_NUM;
}

uint32_t vendord_get_out_size(bool cnt_in_pkts)
{
  uint32_t outbufisze = (CFG_TUD_VENDOR_EPOUT_NUM * CFG_TUD_VENDOR_RX_BUFSIZE);

  if (cnt_in_pkts)
    outbufisze = (outbufisze / CONFIG_TUSB_FIFO_MIN_LINEAR_SIZE);

  return outbufisze;
}

struct scdc_ops _vendord_scdc_ops = {
  .write            = vendord_write,
  .read_info        = vendord_get_read_info,
  .array_linearize  = vendord_array_linearize,
  .advance_outptr   = vendor_n_advance_rptr,
  .prep_out         = vendord_prep_out,
  .get_out_pipe_num = vendord_get_out_pipe_num,
  .get_out_size     = vendord_get_out_size,
  .get_buf      = vendord_get_buf,
  .set_buf      = vendord_set_buf,
};
#endif

#if CFG_TUD_VENDOR_DUMP_BULK_STATUS

void vendor_dump_bulk_out_pkts(void)
{
  scdc_vendor_dump_out_pkts();
}

void vendor_dump_fifo_usage(void)
{
  vendord_interface_t* p_itf = &_vendord_itf[0];
  int epout;
  int free_space, used_space;
  int ratio __maybe_unused = 0;

  for (epout = 0; epout < CFG_TUD_VENDOR_EPOUT_NUM; epout++)
  {
    free_space = space(0, p_itf->rx_ff[epout].depth, p_itf->rx_ff[epout].wr_idx, p_itf->rx_ff[epout].rd_idx);
    used_space = p_itf->rx_ff[epout].depth - free_space;
    ratio = ((used_space * 100)/p_itf->rx_ff[epout].depth);
    TU_LOG1("ep[%d] use:%2d%\n", epout, ratio);
  }
}
#endif
#endif

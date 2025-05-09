/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Koji KITAYAMA
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

#include <string.h>

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/kmem.h>
#include <hal/pinctrl.h>
#include <hal/spi-flash.h>
#include <hal/timer.h>
#include <hal/unaligned.h>
#include <hal/clk.h>

#include "tusb_option.h"
#include "device/dcd.h"
#include "device/usbd_pvt.h"
#include "musbhsfc_type.h"

/*------------------------------------------------------------------
 * MACRO TYPEDEF CONSTANT ENUM DECLARATION
 *------------------------------------------------------------------*/
#define REQUEST_TYPE_INVALID  (0xFFu)
#define ISSUE_WISE_1122   /* (1) Remove a race condition
							 between end of STATUS stage of one control transfer
							 and start of SETUP stage of the next control transfer.
							 (2) Mitigate missing STATUS OUT interrupt
						  */


typedef struct TU_ATTR_PACKED {
  uint8_t  FADDR;        /* 00h */
  uint8_t  POWER;        /* 01h */
  uint16_t INTRTX;       /* 02h */
  uint16_t INTRRX;       /* 04h */
  uint16_t INTRTXE;      /* 06h */
  uint16_t INTRRXE;      /* 08h */
  uint8_t  INTRUSB;      /* 0Ah */
  uint8_t  INTRUSBE;     /* 0Bh */
  uint16_t FRAME;        /* 0Ch */
  uint8_t  INDEX;        /* 0Eh */
  uint8_t  TESTMODE;     /* 0Fh */
} hw_common_t;

typedef struct TU_ATTR_PACKED {
  uint16_t TXMAXP;       /* 10h */
  uint8_t  TXCSRL;       /* 12h */
  uint8_t  TXCSRH;       /* 13h */
  uint16_t RXMAXP;       /* 14h */
  uint8_t  RXCSRL;       /* 16h */
  uint8_t  RXCSRH;       /* 17h */
  uint16_t RXCOUNT;      /* 18h */
  uint16_t RESERVED[3];  /* TO DO: Dynamic FIFO */
} hw_endpoint_t;

/* Aliases for Endpoint 0 */
#define    CSRL0   TXCSRL
#define    COUNT0  RXCOUNT

typedef union {
  uint8_t   byte;
  uint16_t  hword;
  uint32_t  word;
} hw_fifo_t;

typedef struct TU_ATTR_PACKED {
  uint16_t RESERVED1[6];
  uint16_t HWVERS;       /* 6Ch */
  uint16_t RESERVED2[5];
  uint8_t  EPINFO;       /* 78h */
  uint8_t  RAMINFO;      /* 79h */
  uint8_t  SOFTRST;      /* 7Ah */
  uint8_t  ERLYDMA;      /* 7Bh */
  uint16_t RESERVED3[2];
  uint16_t C_T_UCH;      /* 80h */
  uint16_t RESERVED4;
  uint16_t C_T_HSRTN;    /* 84h */
} hw_misc_cfg_t;

typedef struct TU_ATTR_PACKED {
  hw_common_t   common;
  hw_endpoint_t endpt;
  hw_fifo_t     fifo[16];/* 20h - 5Ch */
  hw_misc_cfg_t mcfg;
} hw_regmap_t;

typedef struct TU_ATTR_PACKED {
  uint32_t INTR;         /* reserved when ch != 0 */
  uint32_t CNTL;
  uint32_t ADDR;
  uint32_t COUNT;
} hw_dmactl_t;

typedef struct TU_ATTR_PACKED {
  hw_dmactl_t   dmach[CFG_TUD_NUM_DMA_CHANNELS];
} hw_dmamap_t;

struct pipe_state_s;
typedef struct
{
  int8_t ch;              /* -1 if not used */
  int8_t mode;
  struct pipe_state_s *pipe;
  bool sticky;
} dma_state_t;

typedef struct pipe_state_s
{
  void        *buf;      /* the start address of a transfer data buffer */
  uint16_t    length;    /* the number of bytes in the buffer */
  uint16_t    remaining; /* the number of bytes remaining in the buffer */
  int8_t      rhport;
  int8_t      epnum;
  int8_t      dir_in;
  dma_state_t *dma;
} pipe_state_t;

typedef struct
{
  int8_t       rhport;
  int8_t       irq;
  int8_t       irq_dma;
  volatile hw_regmap_t *base;
  volatile hw_dmamap_t *dmac;

  tusb_control_request_t setup_packet;
  uint16_t     remaining_ctrl; /* The number of bytes remaining in data stage of control transfer. */
  int8_t       status_out;
#ifdef ISSUE_WISE_1122
  int8_t       status_in;
#endif
  pipe_state_t pipe0;
  pipe_state_t pipe[2][DCD_ATTR_ENDPOINT_MAX - 1];   /* pipe[direction][endpoint number - 1] */
  uint16_t     pipe_buf_is_fifo[2]; /* Bitmap. Each bit means whether 1:TU_FIFO or 0:POD. */
  dma_state_t  dma[CFG_TUD_NUM_DMA_CHANNELS];
} dcd_data_t;

/*------------------------------------------------------------------
 * INTERNAL OBJECT & FUNCTION DECLARATION
 *------------------------------------------------------------------*/
static dcd_data_t _dcd;

#define COMM(r)  (_dcd.base->common.r)
#define ENPT(r)  (_dcd.base->endpt.r)
#define FIFO(e)  (&(_dcd.base->fifo[e]))
#define MCFG(r)  (_dcd.base->mcfg.r)
#define DMAC(c)  (&(_dcd.dmac->dmach[c]))

/* Selecting an endpoint and getting/setting from/to
 * its register should be atomic.
 */

#define GET_ENPT_REG(e, r)    ({unsigned __v, __flags; \
        local_irq_save(__flags); \
        COMM(INDEX) = e; \
        __v = (unsigned) ENPT(r); \
        local_irq_restore(__flags); \
        __v; })

#define SET_ENPT_REG(e, r, v) do {unsigned __flags; \
        local_irq_save(__flags); \
        COMM(INDEX) = e; \
        ENPT(r) = v; \
        local_irq_restore(__flags); \
        } while (0)

#if CFG_TUD_DMA

static dma_state_t *alloc_dma(pipe_state_t *pipe)
{
  dma_state_t *dma = NULL;
  int i = -1;
  unsigned long flags;

  local_irq_save(flags);

  /* Check pre-allocated one first. */
  if (pipe->dma && pipe->dma->sticky)
  {
    dma = pipe->dma;
    goto done;
  }

  /* If not, try to allocate from the pool. */
  for (i = 0; i < ARRAY_SIZE(_dcd.dma); i++)
  {
    if (_dcd.dma[i].ch == -1)
    {
	  break;
	}
  }

  if (i != ARRAY_SIZE(_dcd.dma))
  {
    dma = &_dcd.dma[i];
    dma->ch = i;
    dma->pipe = pipe;
    if (pipe->epnum == 0)
	{
      dma->mode = 0;
    }
	else
	{
      dma->mode = 1;
#if CFG_TUD_DMA_BULK_OUT_MODE0
	  if (!pipe->dir_in)
	  {
	    dma->mode = 0;
	  }
#endif
#if CFG_TUD_DMA_BULK_IN_MODE0
    if (pipe->dir_in)
    {
      dma->mode = 0;
    }
#endif
    }
    pipe->dma = dma;
  }

done:

  local_irq_restore(flags);

  return dma;
}

static void free_dma(dma_state_t *dma)
{
  pipe_state_t *pipe = dma->pipe;
  unsigned long flags;

  TU_VERIFY(dma->sticky == false,);
  TU_ASSERT(pipe != NULL,);

  local_irq_save(flags);

  dma->ch = -1;
  dma->pipe = NULL;
  pipe->dma = NULL;

  local_irq_restore(flags);
}

static dma_state_t *find_dma(int8_t ch)
{
  dma_state_t *dma = &_dcd.dma[ch];

  TU_VERIFY(dma && dma->ch == ch, NULL);

  return dma;
}

static void dma_transfer(pipe_state_t *pipe, uint16_t len)
{
  dma_state_t *dma = alloc_dma(pipe);

  TU_VERIFY(dma,);

  TU_LOG2("[%s] dma->ch:%d, pipe:%d, %s\n", __func__, dma->ch, pipe->epnum, pipe->dir_in ? "IN" : "OUT");

  DMAC(dma->ch)->ADDR  = (uint32_t)pipe->buf;
  DMAC(dma->ch)->COUNT = len ? len : pipe->length;
  DMAC(dma->ch)->CNTL  = (pipe->dir_in << 1) | (dma->mode << 2) | (1 << 3) | (pipe->epnum << 4);

  TU_LOG3("[%s] ADDR:0x%x, COUNT:%d, CNTL:0x%x\n", __func__,
		  DMAC(dma->ch)->ADDR, DMAC(dma->ch)->COUNT, DMAC(dma->ch)->CNTL);
  TU_LOG3("[%s] RXCSRL:0x%x, RXCSRH:0x%x\n", __func__, GET_ENPT_REG(pipe->epnum, RXCSRL),
		  GET_ENPT_REG(pipe->epnum, RXCSRH));
  TU_LOG3("[%s] TXCSRL:0x%x, TXCSRH:0x%x\n", __func__, GET_ENPT_REG(pipe->epnum, TXCSRL),
		  GET_ENPT_REG(pipe->epnum, TXCSRH));

  DMAC(dma->ch)->CNTL |= 0x1;
}

#endif

static void pipe_write_packet(void *buf, volatile void *fifo, unsigned len)
{
  volatile hw_fifo_t *reg = (volatile hw_fifo_t*)fifo;
  uintptr_t addr = (uintptr_t)buf;
  while (len >= 4) {
    reg->word = *(uint32_t const *)addr;
    addr += 4;
    len  -= 4;
  }
  if (len >= 2) {
    reg->hword = *(uint16_t const *)addr;
    addr += 2;
    len  -= 2;
  }
  if (len) {
    reg->byte = *(uint8_t const *)addr;
  }
}

static void pipe_read_packet(void *buf, volatile void *fifo, unsigned len)
{
  volatile hw_fifo_t *reg = (volatile hw_fifo_t*)fifo;
  uintptr_t addr = (uintptr_t)buf;
  while (len >= 4) {
    *(uint32_t *)addr = reg->word;
    addr += 4;
    len  -= 4;
  }
  if (len >= 2) {
    *(uint32_t *)addr = reg->hword;
    addr += 2;
    len  -= 2;
  }
  if (len) {
    *(uint32_t *)addr = reg->byte;
  }
}

static void pipe_read_write_packet_ff(tu_fifo_t *f, volatile void *fifo, unsigned len, unsigned dir)
{
  static const struct {
    void (*tu_fifo_get_info)(tu_fifo_t *f, tu_fifo_buffer_info_t *info);
    void (*tu_fifo_advance)(tu_fifo_t *f, uint16_t n);
    void (*pipe_read_write)(void *buf, volatile void *fifo, unsigned len);
  } ops[] = {
    /* OUT */ {tu_fifo_get_write_info, tu_fifo_advance_write_pointer, pipe_read_packet},
    /* IN  */ {tu_fifo_get_read_info,  tu_fifo_advance_read_pointer,  pipe_write_packet},
  };
  tu_fifo_buffer_info_t info;
  ops[dir].tu_fifo_get_info(f, &info);
  unsigned total_len = len;
  len = TU_MIN(total_len, info.len_lin);
  ops[dir].pipe_read_write(info.ptr_lin, fifo, len);
  unsigned rem = total_len - len;
  if (rem) {
    len = TU_MIN(rem, info.len_wrap);
    ops[dir].pipe_read_write(info.ptr_wrap, fifo, len);
    rem -= len;
  }
  ops[dir].tu_fifo_advance(f, total_len - rem);
}

static void process_setup_packet(uint8_t rhport)
{
  uint32_t *p = (void*)&_dcd.setup_packet;

  TU_ASSERT(_dcd.rhport == rhport,);

  p[0]        = FIFO(0)->word;
  p[1]        = FIFO(0)->word;

#ifdef ISSUE_WISE_1122
  if (_dcd.pipe0.buf && _dcd.pipe0.dir_in && _dcd.status_out)
  {
    /* This happens when an interrupt was missing at the end of
     * DATA IN stage probably because the next SETUP packet has
	 * already arrived and triggered a RXRDY interrupt.
	 * _dcd.status_out should be cleared here not to make edpt0_xfer
	 * misinterprete the start of a new control xfer as an end of the
	 * previous control xfer, i.e., STATUS OUT.
	 */

    TU_LOG1("Ignore CONTROL_STAGE_ACK (bRequest:%d)\n", _dcd.setup_packet.bRequest);

	_dcd.status_out = 0;
  }
#endif
  _dcd.pipe0.buf       = NULL;
  _dcd.pipe0.length    = 0;
  _dcd.pipe0.remaining = 0;
  dcd_event_setup_received(rhport, (const uint8_t*)(uintptr_t)&_dcd.setup_packet, true);

  const unsigned len    = _dcd.setup_packet.wLength;
  _dcd.remaining_ctrl   = len;
  const unsigned dir_in = tu_edpt_dir(_dcd.setup_packet.bmRequestType);
#ifdef ISSUE_WISE_1122
  if (dir_in == TUSB_DIR_OUT && _dcd.remaining_ctrl == 0) {
	  /* There will be no DATA stage. */
	  _dcd.status_in = 1;
  }
#endif
  /* Clear RX FIFO and reverse the transaction direction */
  if (len && dir_in) {
    SET_ENPT_REG(0, CSRL0, USB_CSRL0_RXRDYC);
  }
}

static bool handle_xfer_in(uint_fast8_t ep_addr)
{
  unsigned epnum = tu_edpt_number(ep_addr);
  pipe_state_t  *pipe = &_dcd.pipe[tu_edpt_dir(ep_addr)][epnum - 1];
  const unsigned rem  = pipe->remaining;

  if (!rem) {
    pipe->buf = NULL;
    return true;
  }

  const unsigned mps = GET_ENPT_REG(epnum, TXMAXP);
  const unsigned len = TU_MIN(mps, rem);
  void          *buf = pipe->buf;
  TU_LOG2("   %p mps %d len %d rem %d\n", buf, mps, len, rem);
#if CFG_TUD_DMA
#if !CFG_TUD_DMA_BULK_IN_MODE0
  if (pipe->dma && pipe->remaining) {
    /* Supposed to take care of kicking the last 'short' packet loaded.
     * The last 'short' or 'null' packet has already been loaded.
     */
    pipe->remaining = 0;
    free_dma(pipe->dma);
  } else
#endif
#endif
  if (len) {
    if (_dcd.pipe_buf_is_fifo[TUSB_DIR_IN] & TU_BIT(epnum - 1)) {
      pipe_read_write_packet_ff(buf, FIFO(epnum), len, TUSB_DIR_IN);
    } else {
#if CFG_TUD_DMA
#if CFG_TUD_DMA_BULK_IN_MODE0
      TU_ASSERT(rem >= len);
      /* MODE0 will only work given an exact length to move. */
      dma_transfer(pipe, len);
      /* Will be completed when the DMA operation finishes. */
      return false;
#else
      dma_transfer(pipe, 0);
      return false;
#endif
#else
      pipe_write_packet(buf, FIFO(epnum), len);
      pipe->buf       = buf + len;
#endif
    }
    pipe->remaining = rem - len;
  }
  SET_ENPT_REG(epnum, TXCSRL, USB_TXCSRL1_TXRDY);
  TU_LOG3(" TXCSRL%d = %x %d\n", epnum, GET_ENPT_REG(epnum, TXCSRL), rem - len);
  return false;
}

static bool handle_xfer_out(uint_fast8_t ep_addr)
{
  unsigned epnum = tu_edpt_number(ep_addr);
  pipe_state_t  *pipe = &_dcd.pipe[tu_edpt_dir(ep_addr)][epnum - 1];

  TU_LOG3(" RXCSRL%d = %x\n", epnum, GET_ENPT_REG(epnum, RXCSRL));

  TU_ASSERT(GET_ENPT_REG(epnum, RXCSRL) & USB_RXCSRL1_RXRDY);

  const unsigned mps = GET_ENPT_REG(epnum, RXMAXP);
  const unsigned rem = pipe->remaining;
  const unsigned len = GET_ENPT_REG(epnum, RXCOUNT);
  void          *buf = pipe->buf;
#if CFG_TUD_DMA
#if CFG_TUD_DMA_BULK_OUT_MODE0
  if (len)
  {
    TU_ASSERT(rem >= len);
    /* MODE0 will only work given an exact length to move. */
    dma_transfer(pipe, len);
    /* Will be completed when the DMA operation finishes. */
    return false;
  }
  else
  {
    TU_LOG2("An empty bulk out packet arrived.\n");
	pipe->remaining = 0;
	return true;
  }
#else
  if (pipe->dma && pipe->remaining) {
    /* This occurs when the total data length of OUT transfer
	 * is not a multiple of MPS, in other words, the last packet
	 * is a 'short packet' that is 'len' bytes long.
	 * This 'short pakcet' is not handled by DMA controller, which
	 * means we need to read it out by directly accessing FIFO
	 * and clear OutPktRdy.
	 */

    uint32_t rlen = DMAC(pipe->dma->ch)->ADDR - (uint32_t)pipe->buf;
    buf += rlen;
    pipe_read_packet(buf, FIFO(epnum), len);
	/* Modify length to report transferred length correctly. */
	pipe->length = rlen + len;
    pipe->remaining = 0;
	pipe->buf = NULL;
    free_dma(pipe->dma);
	return true;
  } else
#endif
#endif
  if (len) {
    if (_dcd.pipe_buf_is_fifo[TUSB_DIR_OUT] & TU_BIT(epnum - 1)) {
      pipe_read_write_packet_ff(buf, FIFO(epnum), len, TUSB_DIR_OUT);
    } else {
      pipe_read_packet(buf, FIFO(epnum), len);
      pipe->buf = buf + len;
    }
    pipe->remaining = rem - len;
  }
  if ((len < mps) || (rem == len)) {
    pipe->buf = NULL;
    return NULL != buf;
  }
  SET_ENPT_REG(epnum, RXCSRL, 0); /* Clear RXRDY bit */
  return false;
}

static bool edpt_n_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes)
{
  unsigned epnum     = tu_edpt_number(ep_addr);
  unsigned dir_in    = tu_edpt_dir(ep_addr);

  TU_ASSERT(_dcd.rhport == rhport);

  pipe_state_t *pipe = &_dcd.pipe[dir_in][epnum - 1];
  pipe->buf          = buffer;
  pipe->length       = total_bytes;
  pipe->remaining    = total_bytes;

  if (dir_in) {
    handle_xfer_in(ep_addr);
  } else {
	/* Bulk OUT */
#if CFG_TUD_DMA
#if CFG_TUD_DMA_BULK_OUT_MODE0

    /* There are two things to do.
	 *
	 * (1) Set up the pipe with give buffer and length, which has
	 *     already been done above although pipe->length will be
	 *     updated later when INTRRX occurs.
	 *
	 * (2) Clear RXRDY bit to enable next packet to come, which will
	 *     be done below.
	 *
	 * There is nothing more to do because the actual DMA
	 * will be set up and initiated by INTRRX interrupt.
	 */
	/* In MODE0, pipe->length should be equal to the actual length of
	 * data received in the buffer.
	 */
	pipe->length = 0;
#else
    if (!(_dcd.pipe_buf_is_fifo[TUSB_DIR_OUT] & TU_BIT(epnum - 1))) {
      dma_transfer(pipe, 0);
    }
#endif
#endif

    if (GET_ENPT_REG(epnum, RXCSRL) & USB_RXCSRL1_RXRDY) {
      SET_ENPT_REG(epnum, RXCSRL, 0);
    }
  }
  return true;
}

static bool edpt0_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes)
{
  TU_ASSERT(_dcd.rhport == rhport);
  TU_ASSERT(total_bytes <= 64); /* Current implementation supports for only up to 64 bytes. */

  const unsigned req = _dcd.setup_packet.bmRequestType;
  TU_ASSERT(req != REQUEST_TYPE_INVALID || total_bytes == 0);

  if (req == REQUEST_TYPE_INVALID || _dcd.status_out) {
    /* STATUS OUT stage.
     * MUSB controller automatically handles STATUS OUT packets without
     * software helps. We do not have to do anything. And STATUS stage
     * may have already finished and received the next setup packet
     * without calling this function, so we have no choice but to
     * invoke the callback function of status packet here. */
    TU_LOG3(" STATUS OUT ENPT(CSRL0) = %x\n", GET_ENPT_REG(0, CSRL0));
    _dcd.status_out = 0;
    if (req == REQUEST_TYPE_INVALID) {
      dcd_event_xfer_complete(rhport, ep_addr, total_bytes, XFER_RESULT_SUCCESS, false);
    } else {
      /* The next setup packet has already been received, it aborts
       * invoking callback function to avoid confusing TUSB stack. */
      TU_LOG1("Drop CONTROL_STAGE_ACK (bRequest:%d)\n", _dcd.setup_packet.bRequest);
    }
    return true;
  }
  const unsigned dir_in = tu_edpt_dir(ep_addr);
  if (tu_edpt_dir(req) == dir_in) { /* DATA stage */
    TU_ASSERT(total_bytes <= _dcd.remaining_ctrl);
    const unsigned rem = _dcd.remaining_ctrl;
    const unsigned len = TU_MIN(TU_MIN(rem, 64), total_bytes);
    if (dir_in) {
#if CFG_TUD_DMA && CFG_TUD_DMA_EP0
      _dcd.pipe0.dir_in     = 1;
      _dcd.pipe0.buf        = buffer;
      _dcd.pipe0.length     = len;

      dma_transfer(&_dcd.pipe0, 0);
#else
      pipe_write_packet(buffer, FIFO(0), len);

      _dcd.pipe0.buf       = buffer + len;
      _dcd.pipe0.length    = len;
      _dcd.pipe0.remaining = 0;

      _dcd.remaining_ctrl  -= len;
      if ((len < 64) || (_dcd.remaining_ctrl == 0)) {
        _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID; /* Change to STATUS/SETUP stage */
        _dcd.status_out = 1;
        /* Flush TX FIFO and reverse the transaction direction. */
        SET_ENPT_REG(0, CSRL0, USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
      } else {
        SET_ENPT_REG(0, CSRL0, USB_CSRL0_TXRDY); /* Flush TX FIFO to return ACK. */
      }
#endif
      TU_LOG3(" IN ENPT(CSRL0) = %x\n", GET_ENPT_REG(0, CSRL0));
    } else {
      TU_LOG3(" OUT ENPT(CSRL0) = %x\n", GET_ENPT_REG(0, CSRL0));
      _dcd.pipe0.dir_in    = 0;
      _dcd.pipe0.buf       = buffer;
      _dcd.pipe0.length    = len;
      _dcd.pipe0.remaining = len;
      SET_ENPT_REG(0, CSRL0, USB_CSRL0_RXRDYC); /* Clear RX FIFO to return ACK. */
    }
  } else if (dir_in) {
    TU_LOG3(" STATUS IN ENPT(CSRL0) = %x\n", GET_ENPT_REG(0, CSRL0));
    _dcd.pipe0.buf = NULL;
    _dcd.pipe0.length    = 0;
    _dcd.pipe0.remaining = 0;
#ifdef ISSUE_WISE_1122
    _dcd.status_in = 1;
#endif

    /* Clear RX FIFO and reverse the transaction direction */
    SET_ENPT_REG(0, CSRL0, USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND);
  }
  return true;
}

static void process_ep0(uint8_t rhport)
{
  uint_fast8_t csrl;

  csrl = GET_ENPT_REG(0, CSRL0);

  TU_LOG3(" EP0 CSRL0 = %x\n", csrl);

  if (csrl & USB_CSRL0_STALLED) {
    /* Returned STALL packet to HOST. */
    SET_ENPT_REG(0, CSRL0, 0); /* Clear STALL */
    return;
  }

  unsigned req = _dcd.setup_packet.bmRequestType;
  if (csrl & USB_CSRL0_SETEND) {
    TU_LOG1("   ABORT by the next packets\n");
    SET_ENPT_REG(0, CSRL0, USB_CSRL0_SETENDC); /* Clear STALL */
    if (req != REQUEST_TYPE_INVALID && _dcd.pipe0.buf) {
      /* DATA stage was aborted by receiving STATUS or SETUP packet. */
      _dcd.pipe0.buf = NULL;
      _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
      dcd_event_xfer_complete(rhport,
                              req & TUSB_DIR_IN_MASK,
                              _dcd.pipe0.length - _dcd.pipe0.remaining,
                              XFER_RESULT_SUCCESS, true);
    }
    req = REQUEST_TYPE_INVALID;
    if (!(csrl & USB_CSRL0_RXRDY)) return; /* Received SETUP packet */
  }

  if (csrl & USB_CSRL0_RXRDY) {
    /* Received SETUP or DATA OUT packet */
#ifdef ISSUE_WISE_1122
    if (req == REQUEST_TYPE_INVALID || _dcd.status_in) {
      if (_dcd.status_in) {
		/*
		 * This is the race condition where the next SETUP packet
		 * arrived right before the previous STATUS IN stage has been
		 * finalized.
		 * We can't simply drop CONTROL_STAGE_ACK for STATUS IN
		 * because TUSB stack might rely on it.
		 * Let's take care of it now instead.
		 * Please note that _dcd.setup_packet still hold the
		 * previous SETUP packet.
		 */
		/* STATUS IN */
		if (*(const uint16_t*)(uintptr_t)&_dcd.setup_packet == 0x0500) {
		  /* The address must be changed on completion of the control transfer. */
		  COMM(FADDR) = (uint8_t)_dcd.setup_packet.wValue;
		}

		_dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
		_dcd.status_in = 0;
		dcd_event_xfer_complete(rhport,
								tu_edpt_addr(0, TUSB_DIR_IN),
								_dcd.pipe0.length - _dcd.pipe0.remaining,
								XFER_RESULT_SUCCESS, true);
      }
      /* SETUP */
      TU_ASSERT(sizeof(tusb_control_request_t) == GET_ENPT_REG(0, COUNT0),);
      process_setup_packet(rhport);
      return;
    }
#else
    if (req == REQUEST_TYPE_INVALID) {
      /* SETUP */
      TU_ASSERT(sizeof(tusb_control_request_t) == GET_ENPT_REG(0, COUNT0),);
      process_setup_packet(rhport);
      return;
    }
#endif
    if (_dcd.pipe0.buf) {
      /* DATA OUT */
#if CFG_TUD_DMA && CFG_TUD_DMA_EP0
      dma_transfer(&_dcd.pipe0, 0);
#else
      const unsigned vld = GET_ENPT_REG(0, COUNT0);
      const unsigned rem = _dcd.pipe0.remaining;
      const unsigned len = TU_MIN(TU_MIN(rem, 64), vld);

      pipe_read_packet(_dcd.pipe0.buf, FIFO(0), len);

      _dcd.pipe0.remaining -= len;
      _dcd.remaining_ctrl -= len;

      _dcd.pipe0.buf = NULL;
      dcd_event_xfer_complete(rhport,
                              tu_edpt_addr(0, TUSB_DIR_OUT),
                              _dcd.pipe0.length - _dcd.pipe0.remaining,
                              XFER_RESULT_SUCCESS, true);
#endif
    }
    return;
  }

  /* When CSRL0 is zero, it means that completion of sending a any length packet
   * or receiving a zero length packet. */
#ifdef ISSUE_WISE_1122
  if (_dcd.status_in && req != REQUEST_TYPE_INVALID && !tu_edpt_dir(req)) {
    /* STATUS IN */
    if (*(const uint16_t*)(uintptr_t)&_dcd.setup_packet == 0x0500) {
      /* The address must be changed on completion of the control transfer. */
      COMM(FADDR) = (uint8_t)_dcd.setup_packet.wValue;
    }

    _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
	_dcd.status_in = 0;
    dcd_event_xfer_complete(rhport,
                            tu_edpt_addr(0, TUSB_DIR_IN),
                            _dcd.pipe0.length - _dcd.pipe0.remaining,
                            XFER_RESULT_SUCCESS, true);
    return;
  }
#else
  if (req != REQUEST_TYPE_INVALID && !tu_edpt_dir(req)) {
    /* STATUS IN */
    if (*(const uint16_t*)(uintptr_t)&_dcd.setup_packet == 0x0500) {
      /* The address must be changed on completion of the control transfer. */
      COMM(FADDR) = (uint8_t)_dcd.setup_packet.wValue;
    }

    _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
    dcd_event_xfer_complete(rhport,
                            tu_edpt_addr(0, TUSB_DIR_IN),
                            _dcd.pipe0.length - _dcd.pipe0.remaining,
                            XFER_RESULT_SUCCESS, true);
    return;
  }
#endif

  if (_dcd.pipe0.buf) {
    /* DATA IN */
    _dcd.pipe0.buf = NULL;
    dcd_event_xfer_complete(rhport,
                            tu_edpt_addr(0, TUSB_DIR_IN),
                            _dcd.pipe0.length - _dcd.pipe0.remaining,
                            XFER_RESULT_SUCCESS, true);
    TU_LOG3("[%s] DATA IN done.\n", __func__);
  }
}

static void process_edpt_n(uint8_t rhport, uint_fast8_t ep_addr)
{
  bool completed;
  const unsigned dir_in     = tu_edpt_dir(ep_addr);
  const unsigned epn        = tu_edpt_number(ep_addr);

  if (dir_in) {
    TU_LOG3(" TXCSRL%d = %x\n", epn, GET_ENPT_REG(epn, TXCSRL));
    if (GET_ENPT_REG(epn, TXCSRL) & USB_TXCSRL1_STALLED) {
      SET_ENPT_REG(epn, TXCSRL, GET_ENPT_REG(epn, TXCSRL) & ~(USB_TXCSRL1_STALLED | USB_TXCSRL1_UNDRN));
      return;
    }
    completed = handle_xfer_in(ep_addr);
  } else {
    TU_LOG3(" RXCSRL%d = %x\n", epn, GET_ENPT_REG(epn, RXCSRL));
    if (GET_ENPT_REG(epn, RXCSRL) & USB_RXCSRL1_STALLED) {
      return;
    }
    completed = handle_xfer_out(ep_addr);
  }

  if (completed) {
    pipe_state_t *pipe = &_dcd.pipe[dir_in][epn - 1];
    dcd_event_xfer_complete(rhport, ep_addr,
                            pipe->length - pipe->remaining,
                            XFER_RESULT_SUCCESS, true);
  }
}

static void process_bus_reset(uint8_t rhport)
{
  TU_ASSERT(_dcd.rhport == rhport,);

  /* When bmRequestType is REQUEST_TYPE_INVALID(0xFF),
   * a control transfer state is SETUP or STATUS stage. */
  _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
  _dcd.status_out = 0;
#ifdef ISSUE_WISE_1122
  _dcd.status_in = 0;
#endif
  /* When pipe0.buf has not NULL, DATA stage works in progress. */
  _dcd.pipe0.buf = NULL;

#if CFG_TUD_DMA
  {
    int i;
    dma_state_t *dma = NULL;
    for (i = 0; i < ARRAY_SIZE(_dcd.dma); i++) {
      dma = &_dcd.dma[i];
	  if (dma->pipe)
	  {
	    free_dma(dma);
	  }
    }
  }
#endif

  COMM(INTRTXE) = 1; /* Enable only EP0 */
  COMM(INTRRXE) = 0;

  if (COMM(POWER) & USB_POWER_HSMODE) {
    dcd_event_bus_reset(rhport, TUSB_SPEED_HIGH, true);
  } else {
    dcd_event_bus_reset(rhport, TUSB_SPEED_FULL, true);
  }
}

/*------------------------------------------------------------------
 * Device API
 *------------------------------------------------------------------*/

void dcd_init(uint8_t rhport)
{
  int dir_in, i;

  TU_ASSERT(_dcd.rhport == rhport,);

  COMM(INTRUSBE) |= (USB_IE_RESET | USB_IE_RESUME | USB_IE_SUSPND);
  COMM(POWER) 	 |= (USB_POWER_HSENAB | USB_POWER_PWRDNPHY);

  _dcd.pipe0.rhport = rhport;
  _dcd.pipe0.epnum = 0;

  for (dir_in = 0; dir_in < 2; dir_in++)
  {
    for (i = 0; i < DCD_ATTR_ENDPOINT_MAX - 1; i++)
	{
      _dcd.pipe[dir_in][i].rhport = rhport;
      _dcd.pipe[dir_in][i].epnum = i + 1;
      _dcd.pipe[dir_in][i].dir_in = dir_in;
    }
  }

  for (i = 0; i < CFG_TUD_NUM_DMA_CHANNELS; i++)
  {
    _dcd.dma[i].ch = -1;
  }

  dcd_connect(rhport);
}

void dcd_int_enable(uint8_t rhport)
{
  TU_ASSERT(_dcd.rhport == rhport,);

  enable_irq(_dcd.irq);
}

void dcd_int_disable(uint8_t rhport)
{
  TU_ASSERT(_dcd.rhport == rhport,);

  disable_irq(_dcd.irq);
}

// Receive Set Address request, mcu port must also include status IN response
void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  TU_ASSERT(_dcd.rhport == rhport,);

  (void)dev_addr;
  _dcd.pipe0.buf       = NULL;
  _dcd.pipe0.length    = 0;
  _dcd.pipe0.remaining = 0;

  /* Clear RX FIFO to return ACK. */
  SET_ENPT_REG(0, CSRL0, USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND);
}

// Wake up host
void dcd_remote_wakeup(uint8_t rhport)
{
  TU_ASSERT(_dcd.rhport == rhport,);

  COMM(POWER) |= USB_POWER_RESUME;

  osal_task_delay(1); /* 1 ~ 15 ms */

  COMM(POWER) &= ~USB_POWER_RESUME;
}

// Connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport)
{
  TU_ASSERT(_dcd.rhport == rhport,);

  COMM(POWER) |= USB_POWER_SOFTCONN;
}

// Disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport)
{
  TU_ASSERT(_dcd.rhport == rhport,);

  COMM(POWER) &= ~USB_POWER_SOFTCONN;
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// Configure endpoint's registers according to descriptor
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * ep_desc)
{
  TU_ASSERT(_dcd.rhport == rhport);

  const unsigned ep_addr = ep_desc->bEndpointAddress;
  const unsigned epn     = tu_edpt_number(ep_addr);
  const unsigned dir_in  = tu_edpt_dir(ep_addr);
  const unsigned xfer    = ep_desc->bmAttributes.xfer;
  const unsigned mps     = tu_edpt_packet_size(ep_desc);
  uint8_t csrh;
  dma_state_t *dma;

  TU_ASSERT(epn < DCD_ATTR_ENDPOINT_MAX);

  pipe_state_t *pipe = &_dcd.pipe[dir_in][epn - 1];
  pipe->buf       = NULL;
  pipe->length    = 0;
  pipe->remaining = 0;

  if (dir_in) {
    SET_ENPT_REG(epn, TXMAXP, mps);
    csrh = (xfer == TUSB_XFER_ISOCHRONOUS) ? USB_TXCSRH1_ISO : 0;
#if CFG_TUD_DMA
#if !CFG_TUD_DMA_BULK_IN_MODE0
    csrh |= (USB_TXCSRH1_AUTOSET | USB_TXCSRH1_DMAMOD | USB_TXCSRH1_DMAEN);
#endif
#endif
    SET_ENPT_REG(epn, TXCSRH, csrh);
    if (GET_ENPT_REG(epn, TXCSRL) & USB_TXCSRL1_TXRDY)
      SET_ENPT_REG(epn, TXCSRL, USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH);
    else
      SET_ENPT_REG(epn, TXCSRL, USB_TXCSRL1_CLRDT);
#if CFG_TUD_DMA
#if CFG_TUD_DMA_BULK_IN_MODE0
  COMM(INTRTXE) |= TU_BIT(epn);
#else
	/* DMA Mode 1 for IN endpoints shall not use INTRTX. */
    COMM(INTRTXE) &= ~TU_BIT(epn);
#endif
#else
    COMM(INTRTXE) |= TU_BIT(epn);
#endif
  } else {
    SET_ENPT_REG(epn, RXMAXP, mps);
    csrh = (xfer == TUSB_XFER_ISOCHRONOUS) ? USB_RXCSRH1_ISO : 0;
#if CFG_TUD_DMA
#if !CFG_TUD_DMA_BULK_OUT_MODE0
    csrh |= (USB_RXCSRH1_AUTOCL | USB_RXCSRH1_DMAMOD | USB_RXCSRH1_DMAEN);
#endif
#endif
    SET_ENPT_REG(epn, RXCSRH, csrh);
    if (GET_ENPT_REG(epn, RXCSRL) & USB_RXCSRL1_RXRDY)
      SET_ENPT_REG(epn, RXCSRL, USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH);
    else
      SET_ENPT_REG(epn, RXCSRL, USB_RXCSRL1_CLRDT);
    COMM(INTRRXE) |= TU_BIT(epn);

	/* Pre-allocate a DMA channel. */
	dma = alloc_dma(pipe);
	dma->sticky = true;
  }

  return true;
}

void dcd_edpt_close_all(uint8_t rhport)
{
  unsigned long flags;

  TU_ASSERT(_dcd.rhport == rhport,);

  local_irq_save(flags);

  COMM(INTRTXE) = 1; /* Enable only EP0 */
  COMM(INTRRXE) = 0;

  for (unsigned i = 1; i < DCD_ATTR_ENDPOINT_MAX; ++i) {
    SET_ENPT_REG(i, TXMAXP, 0);
    SET_ENPT_REG(i, TXCSRH, 0);
    if (GET_ENPT_REG(i, TXCSRL) & USB_TXCSRL1_TXRDY)
      SET_ENPT_REG(i, TXCSRL, USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH);
    else
      SET_ENPT_REG(i, TXCSRL, USB_TXCSRL1_CLRDT);

    SET_ENPT_REG(i, RXMAXP, 0);
    SET_ENPT_REG(i, RXCSRH, 0);
    if (GET_ENPT_REG(i, RXCSRL) & USB_RXCSRL1_RXRDY)
      SET_ENPT_REG(i, RXCSRL, USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH);
    else
      SET_ENPT_REG(i, RXCSRL, USB_RXCSRL1_CLRDT);
  }

  local_irq_restore(flags);
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr)
{
  unsigned const epn    = tu_edpt_number(ep_addr);
  unsigned const dir_in = tu_edpt_dir(ep_addr);
  unsigned long flags;

  TU_ASSERT(_dcd.rhport == rhport,);

  local_irq_save(flags);

  if (dir_in) {
    COMM(INTRTXE) &= ~TU_BIT(epn);

    SET_ENPT_REG(epn, TXMAXP, 0);
    SET_ENPT_REG(epn, TXCSRH, 0);
    if (GET_ENPT_REG(epn, TXCSRL) & USB_TXCSRL1_TXRDY)
      SET_ENPT_REG(epn, TXCSRL, USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH);
    else
      SET_ENPT_REG(epn, TXCSRL, USB_TXCSRL1_CLRDT);
  } else {
    COMM(INTRRXE) &= ~TU_BIT(epn);

    SET_ENPT_REG(epn, RXMAXP, 0);
    SET_ENPT_REG(epn, RXCSRH, 0);
    if (GET_ENPT_REG(epn, RXCSRL) & USB_RXCSRL1_RXRDY)
      SET_ENPT_REG(epn, RXCSRL, USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH);
    else
      SET_ENPT_REG(epn, RXCSRL, USB_RXCSRL1_CLRDT);
  }

  local_irq_restore(flags);
}

// Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  bool ret;
  unsigned const epnum = tu_edpt_number(ep_addr);
  unsigned long flags;

  TU_ASSERT(_dcd.rhport == rhport, false);

  TU_LOG3("X %x %d\n", ep_addr, total_bytes);

  local_irq_save(flags);

  if (epnum)
  {
    _dcd.pipe_buf_is_fifo[tu_edpt_dir(ep_addr)] &= ~TU_BIT(epnum - 1);
    ret = edpt_n_xfer(rhport, ep_addr, buffer, total_bytes);
  }
  else
  {
    ret = edpt0_xfer(rhport, ep_addr, buffer, total_bytes);
  }

  local_irq_restore(flags);

  return ret;
}

// Submit a transfer where is managed by FIFO, When complete dcd_event_xfer_complete() is invoked to notify the stack - optional, however, must be listed in usbd.c
bool dcd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t * ff, uint16_t total_bytes)
{
  bool ret;
  unsigned const epnum = tu_edpt_number(ep_addr);
  unsigned long flags;

  TU_ASSERT(epnum);

  TU_ASSERT(_dcd.rhport == rhport);

  TU_LOG3("X %x %d\n", ep_addr, total_bytes);

  local_irq_save(flags);

  _dcd.pipe_buf_is_fifo[tu_edpt_dir(ep_addr)] |= TU_BIT(epnum - 1);

  ret = edpt_n_xfer(rhport, ep_addr, (uint8_t*)ff, total_bytes);

  local_irq_restore(flags);

  return ret;
}

// Stall endpoint
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  unsigned const epn = tu_edpt_number(ep_addr);
  unsigned long flags;

  TU_ASSERT(_dcd.rhport == rhport,);

  local_irq_save(flags);

  if (0 == epn) {
    if (!ep_addr) { /* Ignore EP80 */
      _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
      _dcd.pipe0.buf = NULL;
      SET_ENPT_REG(0, CSRL0, USB_CSRL0_STALL);
    }
  } else {
    if (tu_edpt_dir(ep_addr)) { /* IN */
      SET_ENPT_REG(epn, TXCSRL, USB_TXCSRL1_STALL);
    } else { /* OUT */
      TU_ASSERT(!(GET_ENPT_REG(epn, RXCSRL) & USB_RXCSRL1_RXRDY),);
      SET_ENPT_REG(epn, RXCSRL, USB_RXCSRL1_STALL);
    }
  }

  local_irq_restore(flags);
}

// clear stall, data toggle is also reset to DATA0
void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  unsigned const epn = tu_edpt_number(ep_addr);
  unsigned long flags;

  TU_ASSERT(_dcd.rhport == rhport,);

  local_irq_save(flags);

  if (tu_edpt_dir(ep_addr)) { /* IN */
    SET_ENPT_REG(epn, TXCSRL, USB_TXCSRL1_STALLED);
  } else { /* OUT */
    SET_ENPT_REG(epn, RXCSRL, USB_RXCSRL1_STALLED);
  }

  local_irq_restore(flags);
}

/*-------------------------------------------------------------------
 * ISR
 *-------------------------------------------------------------------*/
void dcd_int_handler(uint8_t rhport)
{
  uint_fast8_t is, txis, rxis;

  is   = COMM(INTRUSB); /* read and clear interrupt status */
  txis = COMM(INTRTX);  /* read and clear interrupt status */
  rxis = COMM(INTRRX);  /* read and clear interrupt status */
  TU_LOG3("D%2x T%2x R%2x\n", is, txis, rxis);

  is &= COMM(INTRUSBE); /* Clear disabled interrupts */
  if (is & USB_IS_SOF) {
    dcd_event_bus_signal(rhport, DCD_EVENT_SOF, true);
  }
  if (is & USB_IS_RESET) {
    process_bus_reset(rhport);
  }
  if (is & USB_IS_RESUME) {
    dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
  }
  if (is & USB_IS_SUSPEND) {
    dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
  }

  txis &= COMM(INTRTXE); /* Clear disabled interrupts */
  if (txis & USB_TXIE_EP0) {
    process_ep0(rhport);
    txis &= ~TU_BIT(0);
  }
  while (txis) {
    unsigned const num = __builtin_ctz(txis);
    process_edpt_n(rhport, tu_edpt_addr(num, TUSB_DIR_IN));
    txis &= ~TU_BIT(num);
  }
  rxis &= COMM(INTRRXE); /* Clear disabled interrupts */
  while (rxis) {
    unsigned const num = __builtin_ctz(rxis);
    process_edpt_n(rhport, tu_edpt_addr(num, TUSB_DIR_OUT));
    rxis &= ~TU_BIT(num);
  }
}

static int musbhsfc_irq(int irq, void *data)
{
  dcd_data_t *dcd = data;

  TU_ASSERT(dcd->irq == irq);

  dcd_int_handler(dcd->rhport);

  return 0;
}

#if CFG_TUD_DMA

static void dma_int_ep0(pipe_state_t *pipe)
{
  TU_LOG2("[%s] pipe:%d, %s\n", __func__, pipe->epnum, pipe->dir_in ? "IN" : "OUT");

  if (pipe->dir_in) {
    pipe->buf += pipe->length;
    pipe->remaining = 0;
    _dcd.remaining_ctrl  -= pipe->length;
    TU_LOG2("[%s] pipe->length:%d, _dcd.remaining_ctrl:%d\n", __func__, pipe->length, _dcd.remaining_ctrl);
    if ((pipe->length < 64) || (_dcd.remaining_ctrl == 0)) {
      _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID; /* Change to STATUS/SETUP stage */
      _dcd.status_out = 1;
      /* Flush TX FIFO and reverse the transaction direction. */
      SET_ENPT_REG(0, CSRL0, USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
    } else {
      SET_ENPT_REG(0, CSRL0, USB_CSRL0_TXRDY); /* Flush TX FIFO to return ACK. */
    }
  } else {
    uint32_t len = DMAC(pipe->dma->ch)->ADDR - (uint32_t)pipe->buf;
    pipe->remaining -= len;
    _dcd.remaining_ctrl -= len;

    pipe->buf = NULL;
    dcd_event_xfer_complete(pipe->rhport,
                            tu_edpt_addr(0, TUSB_DIR_OUT),
                            pipe->length - pipe->remaining,
                            XFER_RESULT_SUCCESS, true);
  }
}

static void dma_int_ep_n(pipe_state_t *pipe)
{
  bool completed = (DMAC(pipe->dma->ch)->COUNT == 0);
  uint32_t xferred = DMAC(pipe->dma->ch)->ADDR - (uint32_t)pipe->buf;

  TU_LOG2("[%s] pipe:%d, %s\n", __func__, pipe->epnum, pipe->dir_in ? "IN" : "OUT");
  TU_LOG2("[%s] pipe->buf:0x%x, ADDR:0x%x\n", __func__, (uint32_t)pipe->buf, DMAC(pipe->dma->ch)->ADDR);
  TU_LOG2("[%s] pipe->remaining:%d, COUNT:%d\n", __func__, pipe->remaining, DMAC(pipe->dma->ch)->COUNT);
  TU_LOG3("[%s] OUTCOUNT:%d\n", __func__, GET_ENPT_REG(pipe->epnum, RXCOUNT));
  TU_LOG3("[%s] CNTL:0x%x\n", __func__, DMAC(pipe->dma->ch)->CNTL);
  TU_LOG3("[%s] RXCSRL:0x%x, RXCSRH:0x%x\n", __func__, GET_ENPT_REG(pipe->epnum, RXCSRL),
		  GET_ENPT_REG(pipe->epnum, RXCSRH));
  TU_LOG3("[%s] TXCSRL:0x%x, TXCSRH:0x%x\n", __func__, GET_ENPT_REG(pipe->epnum, TXCSRL),
		  GET_ENPT_REG(pipe->epnum, TXCSRH));

  if (pipe->dir_in) {
	/* According to int_dma.docx,
	 * the processor should set the InPktRdy bit to allow the last
	 * 'short packet' to be sent. If the last packet to be loaded was
	 * of the maximum packet size, the processor should still set the
	 * InPktRdy bit to send a null packet to signify the end of the
	 * transfer.
	 */
    SET_ENPT_REG(pipe->epnum, TXCSRL, USB_TXCSRL1_TXRDY);
  }

  if (completed) {
    free_dma(pipe->dma);
#if CFG_TUD_DMA_BULK_OUT_MODE0
if (!pipe->dir_in)
{
    const unsigned mps = GET_ENPT_REG(pipe->epnum, RXMAXP);
	pipe->length += xferred;
    if(xferred == mps)
	{
	  /* More packets, and possibly an empty packet,
	   * will follow. Be ready and wait for the next
	   * INTRRX interrupt.
	   */
      pipe->buf += xferred;
      pipe->remaining -= xferred;
	  completed = false;
	  /* Let the next packet come in. */
      SET_ENPT_REG(pipe->epnum, RXCSRL, 0);
	}
	else
	{
	  /* Let xferred be the total length. */
	  xferred = pipe->length;
	}
}
#endif
#if CFG_TUD_DMA_BULK_IN_MODE0
  if (pipe->dir_in)
  {
    pipe->remaining -= xferred;

    if (pipe->remaining) {
        pipe->buf += xferred;
        completed = false;
      }
    else {
      /* Let xferred be the total length. */
      xferred = pipe->length;
    }
  }
#endif
  }
  if (completed)
  {

    if (pipe->dir_in)
    {
        /* bulk-in do nothing at event handler, clean busy stasus earlier */
        usbd_edpt_clean_busy(pipe->rhport, tu_edpt_addr(pipe->epnum, pipe->dir_in));
    }
    else
        dcd_event_xfer_complete(pipe->rhport, tu_edpt_addr(pipe->epnum, pipe->dir_in),
                                xferred, XFER_RESULT_SUCCESS, true);
  }
}

static void dcd_dma_int_handler(uint8_t rhport)
{
  uint_fast8_t is;
  dma_state_t *dma;
  pipe_state_t *pipe;

  /* It's cleared by reading out. */
  is   = DMAC(0)->INTR;

  while (is)
  {
    unsigned const ch = __builtin_ctz(is);
    TU_LOG3("[%s] ch:%d\n", __func__, ch);
    dma = find_dma(ch);
    TU_ASSERT(dma != NULL,);

    pipe = dma->pipe;
    if (pipe->epnum == 0) {
      dma_int_ep0(pipe);
      /* EP0 uses MODE 0 that is an one-shot DMA. */
      free_dma(dma);
    } else {
      dma_int_ep_n(pipe);
    }
    is &= ~TU_BIT(ch);
  }
}

static int musbhsfc_dma_irq(int irq, void *data)
{
  dcd_data_t *dcd = data;

  TU_ASSERT(dcd->irq_dma == irq);

  dcd_dma_int_handler(dcd->rhport);

  return 0;
}

#endif

static int musbhsfc_probe(struct device *dev)
{
  _dcd.rhport = dev_id(dev);
  _dcd.irq = dev->irq[0];
  _dcd.irq_dma = dev->irq[1];
  _dcd.base = (hw_regmap_t *)dev->base[0];
  _dcd.dmac = (hw_dmamap_t *)dev->base[1];

  request_irq(_dcd.irq, musbhsfc_irq, dev_name(dev), dev->pri[0], &_dcd);
  /* irq will be enabled explicitly by dcd_int_enable() later. */
  disable_irq(_dcd.irq);

#if CFG_TUD_DMA

  request_irq(_dcd.irq_dma, musbhsfc_dma_irq, dev_name(dev), dev->pri[1], &_dcd);

#endif

  return 0;
}

static declare_driver(musbhsfc) = {
  .name  = "musbhsfc",
  .probe = musbhsfc_probe,
};

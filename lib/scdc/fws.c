/*
 * Copyright 2022-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <hal/kernel.h>
#include <hal/console.h>
#include <hal/wlan.h>
#include <hal/unaligned.h>

#include <cli.h>

#include "compat_param.h"
#include "mutex.h"
#include "compat_if.h"

#include "fwil_types.h"
#include "fweh.h"
#include "scdc.h"
#include "fws.h"

#if CFG_SUPPORT_FWS

#define FWS_ASSERT SCDC_ASSERT
#define FWS_LOG1   SCDC_LOG1
#define FWS_LOG2   SCDC_LOG2
#define FWS_LOG3   SCDC_LOG3

/*
 * flags used to enable tlv signalling from firmware.
 */
#define SNCMF_FWS_FLAGS_RSSI_SIGNALS              0x0001
#define SNCMF_FWS_FLAGS_XONXOFF_SIGNALS           0x0002
#define SNCMF_FWS_FLAGS_CREDIT_STATUS_SIGNALS     0x0004
#define SNCMF_FWS_FLAGS_HOST_PROPTXSTATUS_ACTIVE  0x0008
#define SNCMF_FWS_FLAGS_PSQ_GENERATIONFSM_ENABLE  0x0010
#define SNCMF_FWS_FLAGS_PSQ_ZERO_BUFFER_ENABLE    0x0020
#define SNCMF_FWS_FLAGS_HOST_RXREORDER_ACTIVE     0x0040
#define SNCMF_FWS_FLAGS_INIT                      0x0080

#define SNCMF_FWS_CREDIT_EN(f) ((f & SNCMF_FWS_FLAGS_CREDIT_STATUS_SIGNALS) >> 2)
#define SNCMF_FWS_SET_CREDIT_EN(f) (f |= SNCMF_FWS_FLAGS_CREDIT_STATUS_SIGNALS)
#define SNCMF_FWS_SET_CREDIT_DIS(f) (f &= ~SNCMF_FWS_FLAGS_CREDIT_STATUS_SIGNALS)

#define SNCMF_FWS_INIT(f) (f & SNCMF_FWS_FLAGS_INIT)
#define SNCMF_FWS_SET_INIT(f) (f |= SNCMF_FWS_FLAGS_INIT)
/**
 * enum sncmf_fws_txstatus - txstatus flag values.
 *
 * @SNCMF_FWS_TXSTATUS_DISCARD:
 *	host is free to discard the packet.
 * @SNCMF_FWS_TXSTATUS_CORE_SUPPRESS:
 *	802.11 core suppressed the packet.
 * @SNCMF_FWS_TXSTATUS_FW_PS_SUPPRESS:
 *	firmware suppress the packet as device is already in PS mode.
 * @SNCMF_FWS_TXSTATUS_FW_TOSSED:
 *	firmware tossed the packet.
 * @SNCMF_FWS_TXSTATUS_FW_DISCARD_NOACK:
 *	firmware tossed the packet after retries.
 * @SNCMF_FWS_TXSTATUS_FW_SUPPRESS_ACKED:
 *	firmware wrongly reported suppressed previously, now fixing to acked.
 * @SNCMF_FWS_TXSTATUS_HOST_TOSSED:
 *	host tossed the packet.
 */
enum sncmf_fws_txstatus {
  SNCMF_FWS_TXSTATUS_DISCARD,
  SNCMF_FWS_TXSTATUS_CORE_SUPPRESS,
  SNCMF_FWS_TXSTATUS_FW_PS_SUPPRESS,
  SNCMF_FWS_TXSTATUS_FW_TOSSED,
  SNCMF_FWS_TXSTATUS_FW_DISCARD_NOACK,
  SNCMF_FWS_TXSTATUS_FW_SUPPRESS_ACKED,
  SNCMF_FWS_TXSTATUS_HOST_TOSSED
};

enum sncmf_fws_fifo {
  SNCMF_FWS_FIFO_FIRST,
  SNCMF_FWS_FIFO_AC_BK = SNCMF_FWS_FIFO_FIRST,
  SNCMF_FWS_FIFO_AC_BE,
  SNCMF_FWS_FIFO_AC_VI,
  SNCMF_FWS_FIFO_AC_VO,
  SNCMF_FWS_FIFO_BCMC,
  SNCMF_FWS_FIFO_ATIM,
  SNCMF_FWS_FIFO_COUNT
};

/*
 * sk_buff control packet identifier
 *
 * 32-bit packet identifier used in PKTTAG tlv from host to dongle.
 *
 * - Generated at the host (e.g. dhd)
 * - Seen as a generic sequence number by firmware except for the flags field.
 *
 * Generation : b[31]	=> generation number for this packet [host->fw]
 *             OR, current generation number [fw->host]
 * Flags      : b[30:27] => command, status flags
 * FIFO-AC    : b[26:24] => AC-FIFO id
 * h-slot     : b[23:8] => hanger-slot
 * freerun    : b[7:0] => A free running counter
 */
#define SNCMF_SKB_HTOD_TAG_GENERATION_MASK    0x80000000
#define SNCMF_SKB_HTOD_TAG_GENERATION_SHIFT   31
#define SNCMF_SKB_HTOD_TAG_FLAGS_MASK         0x78000000
#define SNCMF_SKB_HTOD_TAG_FLAGS_SHIFT        27
#define SNCMF_SKB_HTOD_TAG_FIFO_MASK          0x07000000
#define SNCMF_SKB_HTOD_TAG_FIFO_SHIFT         24
#define SNCMF_SKB_HTOD_TAG_HSLOT_MASK         0x00ffff00
#define SNCMF_SKB_HTOD_TAG_HSLOT_SHIFT        8
#define SNCMF_SKB_HTOD_TAG_FREERUN_MASK       0x000000ff
#define SNCMF_SKB_HTOD_TAG_FREERUN_SHIFT      0

/* This hanger size must same with host driver */
#define SNCMF_FWS_HANGER_MAXITEMS 3072
#define SNCMF_FWS_EVENT_COMP_TXSTATUS_CNT CONFIG_FWS_COMP_TXSTATUS_CNT
#define SNCMF_FWS_PIGGYBACK_COMP_TXSTATUS_CNT CONFIG_PIGGYBACK_COMP_TXSTATUS_CNT
#define WORD_BITS 32

typedef struct
{
  uint32_t flag;
  struct {
      uint32_t fifo_credit_type;
      uint32_t fifo_credit_data[SNCMF_FWS_FIFO_COUNT];
  } fifo_credit;
  uint32_t hslot_tab[SNCMF_FWS_HANGER_MAXITEMS/WORD_BITS];
  uint32_t available_hslot;
  int16_t indicate_hslot;
  osMutexId_t mtx_hslot;
} sncmf_fws_t;

sncmf_fws_t _sncmf_fws;

enum sncmf_fws_fifo_credit_mode {
	SNCMF_FWS_FIFO_CREDIT_TYPE_PACKET = 0,
	SNCMF_FWS_FIFO_CREDIT_TYPE_BYTE
};

#if CFG_SUPPORT_CREDIT_BYTE
#define SNCMF_FWS_FIFO_CREDIT_TYPE SNCMF_FWS_FIFO_CREDIT_TYPE_BYTE
#else
#define SNCMF_FWS_FIFO_CREDIT_TYPE SNCMF_FWS_FIFO_CREDIT_TYPE_PACKET
#endif

/*
 * single definition for firmware-driver flow control tlv's.
 *
 * each tlv is specified by SNCMF_FWS_TLV_DEF(name, ID, length).
 * A length value 0 indicates variable length tlv.
 */
#define SNCMF_FWS_TLV_DEFLIST \
	SNCMF_FWS_TLV_DEF(MAC_OPEN, 1, 1) \
	SNCMF_FWS_TLV_DEF(MAC_CLOSE, 2, 1) \
	SNCMF_FWS_TLV_DEF(MAC_REQUEST_CREDIT, 3, 2) \
	SNCMF_FWS_TLV_DEF(TXSTATUS, 4, 4) \
	SNCMF_FWS_TLV_DEF(PKTTAG, 5, 4) \
	SNCMF_FWS_TLV_DEF(MACDESC_ADD,	6, 8) \
	SNCMF_FWS_TLV_DEF(MACDESC_DEL, 7, 8) \
	SNCMF_FWS_TLV_DEF(RSSI, 8, 1) \
	SNCMF_FWS_TLV_DEF(INTERFACE_OPEN, 9, 1) \
	SNCMF_FWS_TLV_DEF(INTERFACE_CLOSE, 10, 1) \
	SNCMF_FWS_TLV_DEF(FIFO_CREDITBACK, 11, 6) \
	SNCMF_FWS_TLV_DEF(PENDING_TRAFFIC_BMP, 12, 2) \
	SNCMF_FWS_TLV_DEF(MAC_REQUEST_PACKET, 13, 3) \
	SNCMF_FWS_TLV_DEF(HOST_REORDER_RXPKTS, 14, 10) \
	SNCMF_FWS_TLV_DEF(TRANS_ID, 18, 6) \
	SNCMF_FWS_TLV_DEF(COMP_TXSTATUS, 19, 1) \
	SNCMF_FWS_TLV_DEF(FILLER, 255, 0)

/*
 * enum sncmf_fws_tlv_type - definition of tlv identifiers.
 */
#define SNCMF_FWS_TLV_DEF(name, id, len) \
	SNCMF_FWS_TYPE_ ## name =  id,
enum sncmf_fws_tlv_type {
	SNCMF_FWS_TLV_DEFLIST
	SNCMF_FWS_TYPE_INVALID
};
#undef SNCMF_FWS_TLV_DEF

/*
 * enum sncmf_fws_tlv_len - definition of tlv lengths.
 */
#define SNCMF_FWS_TLV_DEF(name, id, len) \
	SNCMF_FWS_TYPE_ ## name ## _LEN = (len),
enum sncmf_fws_tlv_len {
	SNCMF_FWS_TLV_DEFLIST
};
#undef SNCMF_FWS_TLV_DEF

#if CFG_SUPPORT_CREDIT_MGMT

int fws_credit_en(void)
{
  return SNCMF_FWS_CREDIT_EN(_sncmf_fws.flag);
}

static inline uint8_t *fws_txstatus_indicate(uint8_t *buf)
{
  uint32_t *report_fws_hdr_tag = (uint32_t *) buf;

  *report_fws_hdr_tag |= SNCMF_FWS_TXSTATUS_FW_TOSSED << SNCMF_SKB_HTOD_TAG_FLAGS_SHIFT;
  *report_fws_hdr_tag |= _sncmf_fws.indicate_hslot << SNCMF_SKB_HTOD_TAG_HSLOT_SHIFT;

  buf += sizeof(uint32_t);

  return buf;
}

static inline uint8_t *fws_build_txstatus_tlv(uint8_t *buf, int *len)
{
  int fillers = 0;

  /*
   *  type (1 byte) | Lens (1 byte) | body (4 byte) | padding
   */

  *buf = SNCMF_FWS_TYPE_TXSTATUS;
   buf++;
   *len = *len + 1;

  *buf = SNCMF_FWS_TYPE_TXSTATUS_LEN;
  buf++;
  *len = *len + 1;
  buf = fws_txstatus_indicate(buf);
  *len = *len + SNCMF_FWS_TYPE_TXSTATUS_LEN;

  fillers = roundup(*len, 4) - *len;
  *len += fillers;

  return buf;
}

static inline uint8_t *fws_build_comp_txstatus_tlv(uint8_t *buf, int *len, uint8_t comp)
{
  int fillers = 0;

  /*
   *  type (1 byte) | Lens (1 byte) | txs_body (4 byte) | comp_body (1 byte) | padding
   */

  *buf = SNCMF_FWS_TYPE_COMP_TXSTATUS;
   buf++;
   *len = *len + 1;

  *buf = SNCMF_FWS_TYPE_TXSTATUS_LEN + SNCMF_FWS_TYPE_COMP_TXSTATUS_LEN;
  buf++;
  *len = *len + 1;
  buf = fws_txstatus_indicate(buf);
  *len = *len + SNCMF_FWS_TYPE_TXSTATUS_LEN;

  *buf = comp;
  buf++;
  *len = *len + SNCMF_FWS_TYPE_COMP_TXSTATUS_LEN;

  fillers = roundup(*len, 4) - *len;
  *len += fillers;

  FWS_ASSERT(*len <= FWS_COMP_TXSTATUS_LEN);

  return buf;
}

void fws_update_done_hslot(uint8_t* _wlh)
{
  uint8_t *wlh = _wlh;
  uint32_t *sncmf_fws_htod_tag = (uint32_t *)&_wlh[2];
  uint32_t hslot = 0;
  uint32_t Quotient = 0;
  uint32_t Remainder = 0;

  if (!fws_credit_en())
    return;

  if (wlh[0] != SNCMF_FWS_TYPE_PKTTAG)
    assert(0);

  if (wlh[1] != SNCMF_FWS_TYPE_PKTTAG_LEN)
    assert(0);

  hslot = (*sncmf_fws_htod_tag & SNCMF_SKB_HTOD_TAG_HSLOT_MASK) >> SNCMF_SKB_HTOD_TAG_HSLOT_SHIFT;

  Quotient = hslot / WORD_BITS;
  Remainder = hslot % WORD_BITS;

  osMutexAcquire(_sncmf_fws.mtx_hslot, osWaitForever);
  _sncmf_fws.hslot_tab[Quotient] |= BIT(Remainder);
  _sncmf_fws.available_hslot++;
  osMutexRelease(_sncmf_fws.mtx_hslot);

  FWS_LOG2("%s h:%d R:%d _sncmf_fws.hslot_tab[%d] = 0x%X\n", __func__, hslot, Remainder, Quotient, _sncmf_fws.hslot_tab[Quotient]);

}

uint8_t fws_report_txstatus(bool force_report, bool event_mode,uint8_t *piggy_buf)
{
  struct scdc_buffer *scdc_buf;
  uint8_t report_data[FWS_COMP_TXSTATUS_LEN] = {0};
  uint32_t quotient = 0;
  uint32_t remainder = 0;
  uint8_t compn = 0;
  uint32_t head = 0;
  uint32_t middle1 = 0, middle2 = 0;
  uint32_t tail = 0;
  uint32_t bits_head = 0;
  uint32_t bits_tail = 0;
  uint32_t bits_remainder = SNCMF_FWS_EVENT_COMP_TXSTATUS_CNT;
  uint32_t expect = SNCMF_FWS_EVENT_COMP_TXSTATUS_CNT;
  uint32_t wrap = SNCMF_FWS_HANGER_MAXITEMS/WORD_BITS;
  uint32_t quotient_temp = 0;
  uint8_t *write_buf = event_mode ? report_data : piggy_buf;
  int len = 0;

  if (!fws_credit_en())
    return 0;

  if (!event_mode) {
    bits_remainder = SNCMF_FWS_PIGGYBACK_COMP_TXSTATUS_CNT;
    expect = SNCMF_FWS_PIGGYBACK_COMP_TXSTATUS_CNT;
  }

  if (!force_report && _sncmf_fws.available_hslot < expect)
    return 0;

  osMutexAcquire(_sncmf_fws.mtx_hslot, osWaitForever);

  quotient_temp = quotient = (_sncmf_fws.indicate_hslot / WORD_BITS);
  remainder = _sncmf_fws.indicate_hslot % WORD_BITS;

  if (force_report)
  {
    while ((_sncmf_fws.hslot_tab[quotient] & BIT(remainder)) != 0)
    {
      _sncmf_fws.hslot_tab[quotient] = _sncmf_fws.hslot_tab[quotient] & ~(BIT(remainder));
      remainder ++;
      if (remainder == WORD_BITS) {
        quotient = (quotient + 1) % wrap;
        remainder = 0;
      }
      compn++;
    }
  }
  else
  {
    if (remainder + expect < WORD_BITS)
    {
      head = expect;
      middle2 = middle1 = 0;
      tail = 0;
    }
    else
    {
      head = (WORD_BITS - remainder);
      bits_remainder -= head;
      middle2 = middle1 = (bits_remainder / WORD_BITS);
      tail = (bits_remainder % WORD_BITS);
    }

    FWS_LOG2("Q:%d R:%d  h=%d m=%d t=%d\n",
              quotient, remainder, head, middle1, tail);
    FWS_LOG2("hslot_tab[%d] = 0x%X, hslot_tab[%d] = 0x%X \n",
      quotient, _sncmf_fws.hslot_tab[quotient],
      (quotient + 1) % wrap, _sncmf_fws.hslot_tab[(quotient + 1) % wrap]);

      /*
       * verify number of FWS_COMP_TXSTATUS_AGG_CNT bits are 1
       */

      bits_head = (BIT(head) - 1) << remainder;
      if ((_sncmf_fws.hslot_tab[quotient_temp] & bits_head) != bits_head)
        goto verify_done;

      while (middle1 > 0)
      {
        quotient_temp = (quotient_temp + 1) % wrap;
        if ((_sncmf_fws.hslot_tab[quotient_temp] & 0xFFFFFFFF) != 0xFFFFFFFF)
         goto verify_done;

        middle1--;
      }

      if (tail > 0) {
        quotient_temp = (quotient_temp + 1) % wrap;
        bits_tail = BIT(tail) -1;
        if ((_sncmf_fws.hslot_tab[quotient_temp] & bits_tail) != bits_tail)
          goto verify_done;
      }

      /*
       * pass all bits checking so clear bits
       */
      compn = expect;
      quotient_temp = quotient;
      _sncmf_fws.hslot_tab[quotient_temp] = (_sncmf_fws.hslot_tab[quotient_temp] & (~bits_head));
      while (middle2 > 0)
      {
        quotient_temp = (quotient_temp + 1) % wrap;
        _sncmf_fws.hslot_tab[quotient_temp] = 0;
        middle2--;
      }
      if (tail > 0)
      {
        quotient_temp = (quotient_temp + 1) % wrap;
        _sncmf_fws.hslot_tab[quotient_temp] = _sncmf_fws.hslot_tab[quotient_temp] & (~bits_tail);
      }
  }

verify_done:

  if (compn  > 0) {
    fws_build_comp_txstatus_tlv(write_buf, &len, compn);
    _sncmf_fws.indicate_hslot = (_sncmf_fws.indicate_hslot + compn) % SNCMF_FWS_HANGER_MAXITEMS;
    _sncmf_fws.available_hslot = _sncmf_fws.available_hslot - compn;

    if (event_mode)
      {
        scdc_buf = scdc_alloc_buffer(len);

        memcpy(scdc_buf->body, write_buf, len);
        scdc_event(0, scdc_buf, len);
      }
  }

  osMutexRelease(_sncmf_fws.mtx_hslot);

  return len;
}

uint8_t fws_piggyback_txstatus(uint8_t *buf)
{
  return fws_report_txstatus(false, false, buf);
}

uint8_t fws_comp_txstatus_len(void)
{
  if (!fws_credit_en())
    return 0;

  return FWS_COMP_TXSTATUS_LEN;
}

static uint32_t fws_get_credit_quota(void)
{
  uint32_t fifo_credit_size = scdc_get_out_size(CFG_SUPPORT_CREDIT_PACKET);

  _sncmf_fws.fifo_credit.fifo_credit_type = SNCMF_FWS_FIFO_CREDIT_TYPE;
  _sncmf_fws.fifo_credit.fifo_credit_data[SNCMF_FWS_FIFO_AC_BK] = fifo_credit_size;

  FWS_LOG1("%s credit type: %d size: %d \n", __func__,
            _sncmf_fws.fifo_credit.fifo_credit_type,
            fifo_credit_size);

  return fifo_credit_size;
}

void fws_enable_credit_mgmt(void)
{
  if (SNCMF_FWS_CREDIT_EN(_sncmf_fws.flag) && !SNCMF_FWS_INIT(_sncmf_fws.flag))
    {
      memset(_sncmf_fws.hslot_tab, 0, sizeof(_sncmf_fws.hslot_tab));
      /* host start from 1*/
      _sncmf_fws.indicate_hslot = 1;
      if (!!fws_get_credit_quota())
        fweh_send_fws_credit((uint32_t *)&_sncmf_fws.fifo_credit, sizeof(_sncmf_fws.fifo_credit));
      else
        SNCMF_FWS_SET_CREDIT_DIS(_sncmf_fws.flag);
      SNCMF_FWS_SET_INIT(_sncmf_fws.flag);
    }
}

#endif /* CFG_SUPPORT_CREDIT_MGMT */

void fws_set_features(uint32_t flags)
{

  FWS_LOG1("%s f:0x%X \n", __func__, flags);

  memset(&_sncmf_fws.fifo_credit, 0, sizeof(_sncmf_fws.fifo_credit));
#if CFG_SUPPORT_CREDIT_MGMT
    if (SNCMF_FWS_CREDIT_EN(flags))
      SNCMF_FWS_SET_CREDIT_EN(_sncmf_fws.flag);

    FWS_LOG1("%s support:CREDIT_STATUS 0x%X\n", __func__, SNCMF_FWS_CREDIT_EN(_sncmf_fws.flag));
#endif

}

void fws_init(void)
{
  memset(&_sncmf_fws, 0, sizeof(_sncmf_fws));
  _sncmf_fws.mtx_hslot = osMutexNew(NULL);
}

#if CFG_FWS_DBG

#define FWS_DBG_INFO_CNT 60

struct fws_dbg_info
{
  int outidx;
  int16_t hslot;
  uint32_t seq;
  uint8_t data[4];
};

struct fws_history
{
  int idx;
  struct fws_dbg_info fws_info[FWS_DBG_INFO_CNT];
};

struct fws_history out_history;

void fws_update_dbg_info (void* tlv, uint32_t seqno, int outidx)
{
  uint8_t *dbg_ptr = tlv;
  uint32_t pkttag;
  int idx = out_history.idx;

  if (dbg_ptr[0] != 0x5)
      assert(0);

  if (dbg_ptr[1] != 0x4)
    assert(0);

  dbg_ptr = dbg_ptr + 2;
  pkttag = get_unaligned_le32(dbg_ptr);

  out_history.fws_info[idx].outidx = outidx;
  out_history.fws_info[idx].hslot = (pkttag >>8) & 0xffff;
  out_history.fws_info[idx].seq = seqno;
  out_history.fws_info[idx].data[0] = *(dbg_ptr);
  out_history.fws_info[idx].data[1] = *(dbg_ptr+1);
  out_history.fws_info[idx].data[2] = *(dbg_ptr+2);
  out_history.fws_info[idx].data[3] = *(dbg_ptr+3);
  idx = (idx + 1) % FWS_DBG_INFO_CNT;
  out_history.idx = idx;
}

void fws_dump_dbg_info(void)
{
  int i;

  FWS_LOG1("bulk_dbg_idx = %d wait %d\n", out_history.idx, _sncmf_fws.indicate_hslot);
  for(i = 0; i < FWS_DBG_INFO_CNT; i++)
  {
    FWS_LOG1("[%d] %d h:%d s:%d, 0x%X 0x%X 0x%X 0x%X\n",
      i, out_history.fws_info[i].outidx, out_history.fws_info[i].hslot,
      out_history.fws_info[i].seq, out_history.fws_info[i].data[0],
      out_history.fws_info[i].data[1], out_history.fws_info[i].data[2],
      out_history.fws_info[i].data[3]);
  }
}

static int do_fws_info(int argc, char *argv[])
{
	fws_dump_dbg_info();
	return 0;
}

static const struct cli_cmd fws_cmd[] =
{
  CMDENTRY(info, do_fws_info, "", ""),
};

static int do_fws(int argc, char *argv[])
{
  const struct cli_cmd *cmd;

  argc--;
  argv++;

  cmd = cli_find_cmd(argv[0], fws_cmd, ARRAY_SIZE(fws_cmd));
  if (cmd == NULL)
  {
    return CMD_RET_USAGE;
  }

  return cmd->handler(argc, argv);
}

CMD(fws, do_fws,
  "test routines for FWS",
  "fws info"
);
#endif /* CFG_FWS_DBG */
#endif /* CFG_SUPPORT_FWS */

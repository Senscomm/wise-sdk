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

#include <sys/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <version.h>
#include <soc.h>
#include <hal/kernel.h>
#include <hal/console.h>
#include <hal/wlan.h>
#include <hal/kmem.h>

#include "compat_param.h"

#include "mbuf.h"
#include "kernel.h"
#include "atomic.h"
#include "systm.h"
#include "malloc.h"
#include "mutex.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include "compat_if.h"
#include "if_arp.h"
#include "if_llc.h"
#include "ethernet.h"
#include "if_dl.h"
#include "if_media.h"
#include "compat_if_types.h"
#include "if_ether.h"
#include "lwip-glue.h"

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_ratectl.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_proto_wise.h>
#ifdef CONFIG_SUPPORT_HE
#include <net80211/ieee80211_he.h>
#endif

#include "cmsis_os.h"
#include "fwil.h"
#include "fweh.h"
#include "scdc.h"
#include "fws.h"

#include "mtsmp.h"

#include "sdio/sdioif.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

/* Define for extra header of features
 * ex. FWS_COMP_TXSTATUS_LEN + XXX + XXX
 * Now only FWS use this
 */
#define SCDC_EXT_HDR_LEN FWS_COMP_TXSTATUS_LEN

typedef struct
{
  uint32_t cmd;		/* dongle command value */
  uint32_t len;		/* lower 16: output buflen;
		 		 	 * upper 16: input buflen (excludes header) */
  uint32_t flags;	/* flag defns given below */
  uint32_t status;	/* status code returned from the device */
} scdc_dcmd_t ;

/* SCDC flag definitions */
#define SCDC_DCMD_ERROR_MASK	0x01		/* 1=cmd failed */
#define SCDC_DCMD_ERROR_SHIFT	0
#define SCDC_DCMD_SET_MASK		0x02		/* 0=get, 1=set cmd */
#define SCDC_DCMD_SET_SHIFT 	1
#define SCDC_DCMD_IF_MASK		0xF000		/* I/F index */
#define SCDC_DCMD_IF_SHIFT		12
#define SCDC_DCMD_ID_MASK		0xFFFF0000	/* id an cmd pairing */
#define SCDC_DCMD_ID_SHIFT		16		/* ID Mask shift bits */
#define SCDC_DCMD_ID(flags)	\
	(((flags) & SCDC_DCMD_ID_MASK) >> SCDC_DCMD_ID_SHIFT)

/*
 * SCDC header - Senscomm specific extension of CDC.
 * Used on data packets to convey priority across USB.
 */
#define	SCDC_HEADER_LEN		8
#define SCDC_PROTO_VER		2	/* Protocol version */
#define SCDC_FLAG_VER_MASK	0xf0	/* Protocol version mask */
#define SCDC_FLAG_VER_SHIFT	4	/* Protocol version shift */
#define SCDC_FLAG_SUM_GOOD	0x04	/* Good RX checksums */
#define SCDC_FLAG_SUM_NEEDED	0x08	/* Dongle needs to do TX checksums */
#define SCDC_FLAG_SEQ_SYNC	0x02 /* seqno synchronization required */
#define SCDC_PRIORITY_MASK	0x7
#define SCDC_FLAG2_IF_MASK	0x0f	/* packet rx interface in APSTA */
#define SCDC_FLAG2_IF_SHIFT	0

#define SCDC_GET_IF_IDX(hdr) \
	((int)((((hdr)->flags2) & SCDC_FLAG2_IF_MASK) >> SCDC_FLAG2_IF_SHIFT))
#define SCDC_SET_IF_IDX(hdr, idx) \
	((hdr)->flags2 = (((hdr)->flags2 & ~SCDC_FLAG2_IF_MASK) | \
	((idx) << SCDC_FLAG2_IF_SHIFT)))

#define SCDC_SET_PROTO_VER(hdr, ver) \
	((hdr)->flags = (((hdr)->flags & ~SCDC_FLAG_VER_MASK) | \
	((ver) << SCDC_FLAG_VER_SHIFT)))

/**
 * struct sncmf_proto_scdc_header - SCDC header format
 *
 * @flags: flags contain protocol and checksum info.
 * @priority: 802.1d priority and USB flow control info (bit 4:7).
 * @flags2: additional flags containing dongle interface index.
 * @data_offset: start of packet data. header is following by firmware signals.
 * @seqno: monotonically increasing sequence number.
 */
struct sncmf_proto_scdc_header
{
  u8 flags;
  u8 priority;
  u8 flags2;
  u8 data_offset;
  u32 seqno;
};

/*
 * maximum length of firmware signal data between
 * the SCDC header and packet data in the tx path.
 */
#define SNCMF_PROT_FW_SIGNAL_MAX_TXBYTES	12

#ifdef CONFIG_SDIO
/* SDIO willnot use msgbuf for copying ctrl msg */
#define SCDC_DCMD_MAXLEN	0
#else
#define SCDC_DCMD_MAXLEN	8192
#endif

#define ifq_len(ifq)	 ({ int __len = _IF_QLEN(ifq); __len; })
#define ifq_dequeue(ifq) ({ struct mbuf *__m; IFQ_DEQUEUE(ifq, __m); __m; })
#define ifq_peek(ifq) 	 ({ struct mbuf *__m; IFQ_POLL(ifq, __m); __m; })
#define ifq_enqueue(ifq, m) ({ int __ret; IFQ_ENQUEUE(ifq, m, __ret); __ret; })

typedef struct
{
  scdc_dcmd_t msg; // message header from host.
  uint8_t buf[SCDC_DCMD_MAXLEN]; //message body from host.
} scdc_dcmdbuf_t;

typedef struct
{
  uint8_t itf;
  uint16_t reqid;
  uint16_t len;
  scdc_dcmdbuf_t msgbuf;
  struct list_head event_queue;
  struct ifqueue data_queue;
  osSemaphoreId_t sync;
  osThreadId_t tid;
  uint8_t out_pipe_num;
  struct list_head *out_pkts;
  osMutexId_t *mtx_out_pkts;
  struct list_head reorder_q;
  uint32_t out_next_seqno;
  u32 in_seqno;
  bool in_seq_sync_req;
  struct scdc_ops *ops;
} scdc_ctx_t;

/* XXX: Assumed a singleton would suffice because all FWIL commands are
 * synchronized from a host. */

scdc_ctx_t _scdc_ctx;

/* An outstanding packet residing in the rx fifo */
typedef struct
{
  uint8_t itf;
  int outidx;
  uint32_t seqno;
  uint8_t *ext_buf;
  scdc_buf_ptr_info_t finfo;
  struct list_head list;
} out_pkt_t;

/* A list entry for a mbuf set aside due to reordering based on seqno. */
typedef struct
{
  struct mbuf *m;
  uint32_t seqno;
  struct list_head list;
} seq_mbuf;

/* NB: consider multiple wlan instances. */

/*
 * Target-to-host: event and data
 */

/* =====scdc ops===== */

static __inline__ void scdc_write(void* buffer, uint32_t bufsize, bool isevent)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->write))
    return;

  ops->write(buffer, bufsize, isevent);
}

static __inline__ void scdc_chainwrite(struct ifqueue *data_queue)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->chainwrite))
    return;

  ops->chainwrite(data_queue);
}

static __inline__ void scdc_array_linearize(uint8_t itf, uint8_t outidx, void *pinfo)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->array_linearize))
    return;

  ops->array_linearize(itf, outidx, pinfo);
}

static __inline__ void scdc_advance_outptr(uint8_t itf, uint8_t outidx, uint16_t len)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->advance_outptr))
    return;

  ops->advance_outptr(itf, outidx, len);
}

static __inline__ void scdc_prep_out(uint8_t itf, uint8_t outidx, bool async, void *pinfo)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->prep_out))
    return;

  ops->prep_out(itf, outidx, async, pinfo);
}

static __inline__ void scdc_read_info(uint8_t itf, uint8_t outidx, void *pinfo)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->read_info))
    return;

  ops->read_info(itf, outidx, pinfo);
}

static __inline__ uint8_t scdc_get_out_pipe_num(void)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->get_out_pipe_num))
    return 1; /* default 1 pipe */

  return ops->get_out_pipe_num();
}

static __inline__ uint8_t scdc_get_extra_hdr_len(void)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->get_extra_hdr_len))
    return 0;

  return ops->get_extra_hdr_len();
}

uint32_t scdc_get_out_size(bool cnt_in_pkts)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->get_out_size))
    return 0;

  return ops->get_out_size(cnt_in_pkts);
}

static __inline__ uint8_t* scdc_get_buf(uint8_t itf, int outidx, uint8_t op)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->get_buf))
    return 0;

  return ops->get_buf(itf, outidx, op);
}

static __inline__ void scdc_set_buf(uint8_t itf, int outidx, uint8_t op, uint8_t *value)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->set_buf))
    return;

  ops->set_buf(itf, outidx, op, value);
}

static __inline__ int scdc_prep_in(uint8_t itf, uint8_t outidx, void *pinfo)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->prep_in))
    return 0;

  return ops->prep_in(itf, outidx, pinfo);
}

static __inline__ int scdc_awake_host(void)
{
  struct scdc_ops *ops = _scdc_ctx.ops;

  if (!(ops && ops->awake_host))
    return 0;

  return ops->awake_host();
}

/* ========== */

static void
scdc_set_hdr(int ifidx, struct sncmf_proto_scdc_header *hdr, uint8_t data_offset)
{
  SCDC_SET_IF_IDX(hdr, ifidx);
  SCDC_SET_PROTO_VER(hdr, SCDC_PROTO_VER);
  hdr->flags &= ~(SCDC_FLAG_SUM_GOOD | SCDC_FLAG_SUM_NEEDED | SCDC_FLAG_SEQ_SYNC);
  hdr->flags |= SCDC_FLAG_SUM_GOOD;
  hdr->flags |= (_scdc_ctx.in_seq_sync_req ? SCDC_FLAG_SEQ_SYNC : 0);
  hdr->seqno = _scdc_ctx.in_seqno++;
  hdr->data_offset = data_offset >> 2;
  _scdc_ctx.in_seq_sync_req = false;
}

struct scdc_buffer *scdc_alloc_buffer(uint32_t payloadlen)
{
  uint8_t             ext_hdr = scdc_get_extra_hdr_len();
  uint32_t            len = ext_hdr + sizeof(struct sncmf_proto_scdc_header) + payloadlen;
  struct scdc_buffer *scdc_buf;

  scdc_buf = (struct scdc_buffer *)kmalloc(sizeof(*scdc_buf));
  SCDC_ASSERT(scdc_buf);
  scdc_buf->start = kmalloc(len);
  SCDC_ASSERT(scdc_buf->start);

  scdc_buf->hdr  = (struct sncmf_proto_scdc_header *)(scdc_buf->start + ext_hdr);
  scdc_buf->body = (uint8_t *)&scdc_buf->hdr[1];
  scdc_buf->len = len;

  return scdc_buf;
}

static void scdc_queue_handler(struct list_head *queue)
{
  LIST_HEAD_DEF(waiting);
  struct list_head *entry, *tmp1;
  struct scdc_buffer *scdc_buf, *tmp2;
  uint32_t flags;

  local_irq_save(flags);

  if (list_empty(queue))
  {
    local_irq_restore(flags);
    return;
  }

  list_for_each_safe(entry, tmp1, queue)
  {
    list_move_tail(entry, &waiting);
  }

  local_irq_restore(flags);

  list_for_each_entry_safe(scdc_buf, tmp2, &waiting, list)
  {
    list_del(&scdc_buf->list);
    scdc_write(scdc_buf->start, scdc_buf->len, true);
    kfree(scdc_buf->start);
    kfree(scdc_buf);
  }
}

int scdc_event(int ifidx, struct scdc_buffer *scdc_buf, uint8_t data_offset)
{
  struct sncmf_proto_scdc_header *hdr = scdc_buf->hdr;
  uint32_t flags;

  memset(hdr, 0, sizeof(*hdr));

  scdc_set_hdr(ifidx, hdr, data_offset);

  local_irq_save(flags);

  list_add_tail(&scdc_buf->list, &_scdc_ctx.event_queue);

  local_irq_restore(flags);

  osSemaphoreRelease(_scdc_ctx.sync);

  return 0;
}

static void scdc_event_handler(void)
{
    scdc_queue_handler(&_scdc_ctx.event_queue);
}

static int scdc_hdr_ext(struct mbuf *m, uint8_t *buf, uint8_t *data_offset)
{
  uint8_t ext_hdr_len = 0;

  /* FWS txstatus header*/
  *data_offset = fws_piggyback_txstatus(buf);
  ext_hdr_len += fws_comp_txstatus_len();

  return ext_hdr_len;
}

#ifdef CONFIG_SCDC_DATA_TEST
int scdc_data_test(struct mbuf *m)
{
    struct sncmf_proto_scdc_header *hdr;
    int                             ifidx;
    int                             ret;
    uint8_t                         data_offset = 0;

    ifidx = 0;

    SCDC_ASSERT(M_LEADINGSPACE(m) >= sizeof(*hdr));

    M_PREPEND(m, sizeof(*hdr), M_NOWAIT);
    hdr = mtod(m, struct sncmf_proto_scdc_header *);
    memset(hdr, 0, sizeof(*hdr));

    scdc_set_hdr(ifidx, hdr, data_offset);

    m_fixhdr(m);

    SCDC_LOG3("[%s, %d] len:%d(0x%x)\n", __func__, __LINE__, m_length(m, NULL), m);

    ret = ifq_enqueue(&_scdc_ctx.data_queue, m);
    SCDC_ASSERT(ret == 0);

    osSemaphoreRelease(_scdc_ctx.sync);

    return 0;
}
#endif

static int scdc_data(struct mbuf *m)
{
  struct sncmf_proto_scdc_header *hdr;
  struct device *dev = wlandev(0);
  struct ifnet *ifp = m->m_pkthdr.rcvif;
  struct ieee80211vap *vap = ifp->if_softc;
  int ifidx;
  int ret;
  uint8_t data_offset = 0;

  uint8_t ext_hdr_len = 0;
  uint8_t ext_hdr_buf[SCDC_EXT_HDR_LEN] = {0};

  SCDC_ASSERT(ifp && vap);

  ifidx = get_vap_idx(dev, vap);

  SCDC_ASSERT(ifidx != -1);

  SCDC_ASSERT(M_LEADINGSPACE(m) >= sizeof(*hdr));

  ext_hdr_len = scdc_hdr_ext(m, ext_hdr_buf, &data_offset);

  if ((M_LEADINGSPACE(m) < sizeof(*hdr) + ext_hdr_len)
    || (data_offset == 0))
  {
      ext_hdr_len = 0;
  }

  M_PREPEND(m, sizeof(*hdr) + ext_hdr_len, M_NOWAIT);
  hdr = mtod(m, struct sncmf_proto_scdc_header *);

  SCDC_ASSERT(ext_hdr_len <= sizeof(ext_hdr_buf));

  if (ext_hdr_len)
    memcpy(hdr + 1, ext_hdr_buf, ext_hdr_len);

  memset(hdr, 0, sizeof(*hdr));

  scdc_set_hdr(ifidx, hdr, data_offset);

  m_fixhdr(m);

  SCDC_LOG2("[%s, %d] len:%d(0x%x)\n", __func__, __LINE__,
    m_length(m, NULL), m);

  ret = ifq_enqueue(&_scdc_ctx.data_queue, m);
  SCDC_ASSERT(ret == 0);

  osSemaphoreRelease(_scdc_ctx.sync);

  return 0;
}

static void scdc_data_handler(void)
{
  scdc_ctx_t *sctx = &_scdc_ctx;
  struct mbuf *m;
  uint32_t totlen;

  if (sctx->ops && sctx->ops->chainwrite) {
    while (ifq_len(&sctx->data_queue) > 0) {
      scdc_chainwrite(&sctx->data_queue);
    }
    return;
  }
  // else not support chainwrite
  while((m = ifq_dequeue(&sctx->data_queue)))
  {
    totlen = m->m_pkthdr.len;
    scdc_write(m, totlen, false);

    tsmp_time_in(m, end_t);
    tsmp_set_idx(m, 0);
    tsmp_clean_in(m);

    m_freem(m);
  }
}

static void scdc_del_rxs(struct mbuf *m)
{
  ieee80211_del_rxs(m);
}

void scdc_input(struct ifnet *ifp, struct mbuf *m)
{
  SCDC_ASSERT(ifp == m->m_pkthdr.rcvif,);
  scdc_del_rxs(m);
  scdc_data(m);
}

void scdc_ifattach(struct ifnet *ifp)
{
  ifp->if_etherinput = ifp->if_input;
  ifp->if_input = scdc_input;
}

void scdc_ifdetach(struct ifnet *ifp)
{
  ifp->if_input = ifp->if_etherinput;
}

static out_pkt_t *prev_outp(out_pkt_t *outp)
{
  out_pkt_t *prev;
  int outidx = outp->outidx;

  /* Should have acquired the mutex. */
  SCDC_ASSERT(osMutexGetOwner(_scdc_ctx.mtx_out_pkts[outidx]) != NULL);

  SCDC_ASSERT(list_empty(&_scdc_ctx.out_pkts[outidx]) == false);

  if (outp == list_first_entry(&_scdc_ctx.out_pkts[outidx], out_pkt_t, list))
  {
    prev = NULL;
  }
  else
  {
    prev = list_last_entry(&outp->list, out_pkt_t, list);
  }

  return prev;
}

static uint16_t credit(out_pkt_t *outp)
{
  return outp ? outp->finfo.len_lin + outp->finfo.len_wrap : 0;
}

static bool is_wrapped(out_pkt_t *outp)
{
  return outp->finfo.len_wrap != 0;
}

static bool is_linear(out_pkt_t *outp)
{
  return !is_wrapped(outp);
}

static bool is_any_outpkts_wrapped(uint8_t outidx)
{
  out_pkt_t *outp;

  /* Should have acquired the mutex. */
  SCDC_ASSERT(osMutexGetOwner(_scdc_ctx.mtx_out_pkts[outidx]) != NULL);

  list_for_each_entry(outp, &_scdc_ctx.out_pkts[outidx], list)
  {
    if (is_wrapped(outp))
    {
      return true;
    }
  }

  return false;
}

static bool inline out_pkts_empty(void)
{
  int outidx;

  for (outidx = 0; outidx < _scdc_ctx.out_pipe_num; outidx++)
  {
    if(!list_empty(&_scdc_ctx.out_pkts[outidx]))
      {
        break;
      }
  }
  return (outidx == _scdc_ctx.out_pipe_num);
}

static void purge_outp(out_pkt_t *outp, bool async, bool need_mutex)
{
  out_pkt_t *prev = NULL;
  int outidx = outp->outidx;

  fws_update_done_hslot(outp->finfo.ptr_lin + sizeof(struct sncmf_proto_scdc_header));

  /* Just free the m_cache if it's external buffer case*/
  if (outp->ext_buf) {
    struct mbuf *m_cache = (struct mbuf *)outp->ext_buf;

    m_freem(m_cache);
    fws_report_txstatus(out_pkts_empty(), true, NULL);
    return;
  }

  if(need_mutex)
    osMutexAcquire(_scdc_ctx.mtx_out_pkts[outidx], osWaitForever);

  prev = prev_outp(outp);
  /* Remove outp from the outstandling packets list. */
  list_del(&outp->list);

  /* And do the post processing. */
  if (prev == NULL)
  {
    /* This is the oldest outstanding packet.
     * Advance the read pointer in rx_ff and let
     * a new bulk out transfer to come in.
     */

    fws_report_txstatus(out_pkts_empty(), true, NULL);

    scdc_advance_outptr(outp->itf, outidx, credit(outp));
    scdc_prep_out(outp->itf, outidx, async, &outp->finfo);
  }
  else
  {
    /* This is NOT the oldest outstanding packet.
     * Coalesce this one and the previous.
     */
    if (is_linear(prev) && is_linear(outp))
    {
      prev->finfo.len_lin += outp->finfo.len_lin;
    }
    else if (is_linear(prev) && is_wrapped(outp))
    {
      prev->finfo.len_lin += outp->finfo.len_lin;
      prev->finfo.len_wrap = outp->finfo.len_wrap;
      prev->finfo.ptr_wrap = outp->finfo.ptr_wrap;
    }
    else if (is_wrapped(prev) && is_linear(outp))
    {
      prev->finfo.len_wrap += outp->finfo.len_lin;
    }
    else
    {
      /* Cannot happen. */
      SCDC_ASSERT(false,);
    }
  }

  if(need_mutex)
    osMutexRelease(_scdc_ctx.mtx_out_pkts[outidx]);

  free(outp);
}

static void read_out_hdr(out_pkt_t *outp,
    struct sncmf_proto_scdc_header *hdr)
{
  int off = min(outp->finfo.len_lin, sizeof(*hdr));

  memcpy(hdr, outp->finfo.ptr_lin, off);
  memcpy((uint8_t *)hdr + off, outp->finfo.ptr_wrap, sizeof(*hdr) - off);
}

static void scdc_mbuf_free_cb(struct mbuf *m)
{
  out_pkt_t *outp = (out_pkt_t *)m->m_ext.ext_arg1;
  out_pkt_t local_outp = {0};

  SCDC_LOG2("[%s] credit:%d(0x%x) \n", __func__, credit(outp), (uint32_t)m);
  if (outp == NULL) {
    local_outp.ext_buf = (uint8_t *)m->m_ext.ext_arg2;
    assert(local_outp.ext_buf);
    purge_outp(&local_outp, true, true);
  } else {
    purge_outp(outp, true, true);
  }
}

#ifdef CONFIG_SCDC_DATA_REORDER
static seq_mbuf *find_seq(uint32_t seqno)
{
  seq_mbuf *leq = NULL, *iter;

  list_for_each_entry(iter, &_scdc_ctx.reorder_q, list)
  {
    /* Find seq_mbuf with a seqno less than or equal. */
    if (iter->seqno <= seqno)
    {
      leq = iter;
    }
  }

  return leq;
}

static void insert_pkt(seq_mbuf *smbuf)
{
  seq_mbuf *pred = find_seq(smbuf->seqno);

  /* Should not be duplicate. */
  SCDC_ASSERT(!(pred && pred->seqno == smbuf->seqno),);

  if (pred)
  {
    list_add(&smbuf->list, &pred->list);
  }
  else
  {
    list_add(&smbuf->list, &_scdc_ctx.reorder_q);
  }
}

static void flush_reorderq(bool discard)
{
  seq_mbuf *smbuf, *tmp;
  struct ifnet *ifp;
  struct mbuf *m;

  list_for_each_entry_safe(smbuf, tmp, &_scdc_ctx.reorder_q, list)
  {
    list_del(&smbuf->list);
    m = smbuf->m;
    if (discard)
    {
      m_freem(m);
    }
    else
    {
      ifp = m->m_pkthdr.rcvif;
      ifp->if_output(ifp, m, NULL, NULL);
      /* how about out_next_seqno? */
    }
    free(smbuf);
  }
}

static void print_reorder_q(uint32_t seqno) __maybe_unused;
static void print_reorder_q(uint32_t seqno)
{
  seq_mbuf *smbuf;

  SCDC_LOG1("[%s] new:0x%X\n", __func__, seqno);
  list_for_each_entry(smbuf, &_scdc_ctx.reorder_q, list)
  {
    SCDC_LOG1("(0x%x)", smbuf->seqno);
  }
  SCDC_LOG1("\n");
}

static void reorder_pkt(struct mbuf *m, uint32_t seqno, bool sync)
{
  seq_mbuf *smbuf;

  /* We will include a smbuf node even if mbuf couldn't be allocated
   * for it. It will make things simpler especially in terms of maintain
   * sequence numbers.
   */

  if (sync)
  {
    flush_reorderq(true);
    _scdc_ctx.out_next_seqno = seqno;
  }

  if (seqno == _scdc_ctx.out_next_seqno)
  {
    /* Now we can deliver this one and all queued packets
     * until we meet a hole.
     */

    struct ifnet *ifp;
    seq_mbuf *tmp;

    if (m)
    {
      ifp = m->m_pkthdr.rcvif;
      ifp->if_output(ifp, m, NULL, NULL);
    }

    _scdc_ctx.out_next_seqno++;

    list_for_each_entry_safe(smbuf, tmp, &_scdc_ctx.reorder_q, list)
    {
      if (smbuf->seqno == _scdc_ctx.out_next_seqno)
      {
        list_del(&smbuf->list);
        if (smbuf->m)
        {
          ifp = smbuf->m->m_pkthdr.rcvif;
          ifp->if_output(ifp, smbuf->m, NULL, NULL);
        }
        free(smbuf);
        _scdc_ctx.out_next_seqno++;
      }
    }
  }
  else
  {
    /* Add this packet to the right position in the
     * reorder queue.
     */
    smbuf = kzalloc(sizeof(*smbuf));
    smbuf->m = m;
    smbuf->seqno = seqno;
    insert_pkt(smbuf);
  }

#if 0
  print_reorder_q(seqno);
#endif
}
#endif

static void scdc_main(void *argument)
{

  (void)argument;

  while (1)
  {
    osSemaphoreAcquire(_scdc_ctx.sync, osWaitForever);
    scdc_awake_host();
    scdc_event_handler();
    scdc_data_handler();
  }
}

int scdc_reset(void)
{
  _scdc_ctx.in_seq_sync_req = true;

  return 0;
}

/* ===== call by interface ===== */

int scdc_init(void)
{
  osThreadAttr_t attr = {
    .name = "scdc",
    .stack_size = CONFIG_DEFAULT_STACK_SIZE,
    .priority = osPriorityNormal,
  };

  memset(&_scdc_ctx, 0, sizeof(_scdc_ctx));

  INIT_LIST_HEAD(&_scdc_ctx.event_queue);

  _scdc_ctx.sync = osSemaphoreNew(1, 0, NULL);
  SCDC_ASSERT(_scdc_ctx.sync);

  ifq_init(&_scdc_ctx.data_queue, NULL);
  IFQ_SET_MAX_LEN(&_scdc_ctx.data_queue, CONFIG_MEMP_NUM_MBUF_CACHE);

  INIT_LIST_HEAD(&_scdc_ctx.reorder_q);

  fwil_init();
  fweh_init();
  fws_init();

  _scdc_ctx.tid = osThreadNew(scdc_main, &_scdc_ctx, &attr);
  SCDC_ASSERT(_scdc_ctx.tid);

  return 0;
}

static void scdc_out_entry_init(void)
{
  _scdc_ctx.out_pipe_num = scdc_get_out_pipe_num();

  SCDC_ASSERT(_scdc_ctx.out_pipe_num);

  _scdc_ctx.out_pkts = kmalloc(sizeof(struct list_head) * _scdc_ctx.out_pipe_num);
  _scdc_ctx.mtx_out_pkts = kmalloc(sizeof(osMutexId_t) * _scdc_ctx.out_pipe_num);

  for(uint8_t i = 0; i < _scdc_ctx.out_pipe_num; i++)
  {
    INIT_LIST_HEAD(&_scdc_ctx.out_pkts[i]);
    _scdc_ctx.mtx_out_pkts[i] = osMutexNew(NULL);
    SCDC_ASSERT(_scdc_ctx.mtx_out_pkts[i]);
  }
}

void scdc_vendor_reg(struct scdc_ops *ops)
{
  _scdc_ctx.ops = ops;

  /* init vendor related when ops registered, need information from cb */
  scdc_out_entry_init();
}

void scdc_vendor_open(uint8_t itf)
{
  (void)itf;

  scdc_reset();
}

static void scdc_update_buf_ptr_info(uint8_t itf, uint8_t outidx, uint16_t len,
          scdc_buf_ptr_info_t finfo, scdc_buf_ptr_info_t *pfinfo)
{
  out_pkt_t *prev = list_last_entry(&_scdc_ctx.out_pkts[outidx], out_pkt_t, list);

  if (finfo.len_wrap == 0 || is_any_outpkts_wrapped(outidx))
  {
    uint8_t *pptr;
    uint16_t plen;

    /* Not wrapped at all, or one of previous outpkts has wrapped.
     * So, the current one must be linear.
     */
    if (finfo.len_wrap == 0 || prev->finfo.len_wrap == 0)
    {
      pptr = prev->finfo.ptr_lin;
      plen = prev->finfo.len_lin;
    }
    else
    {
      pptr = prev->finfo.ptr_wrap;
      plen = prev->finfo.len_wrap;
    }
    pfinfo->len_lin = len;
    pfinfo->ptr_lin = pptr + plen;

    scdc_array_linearize(itf, outidx, pfinfo);
  }
  else if (!is_any_outpkts_wrapped(outidx) && finfo.len_wrap != 0)
  {
    /* The current outpkt has wrapped the fifo around. */
    pfinfo->len_wrap = finfo.len_wrap;
    pfinfo->len_lin = len - pfinfo->len_wrap;
    pfinfo->ptr_wrap = finfo.ptr_wrap;
    pfinfo->ptr_lin = prev->finfo.ptr_lin + prev->finfo.len_lin;
  }
}

void scdc_vendor_rx_cb(uint8_t itf, int outidx, uint16_t len, void *priv)
{
  struct sncmf_proto_scdc_header hdr;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  struct ifnet *ifp;
  struct mbuf *m = NULL;
  struct mbuf *m_cache = (struct mbuf *)scdc_get_buf(itf, outidx, SCDC_CACHE_BUF);
  uint8_t headroom;
  out_pkt_t *pkt, local_pkt = {0};
  scdc_buf_ptr_info_t finfo = {0}, *pfinfo;
  bool mutex_get = false;

  if (len == 0)
  {
    SCDC_LOG1("[%s, %d] no data\n", __func__, __LINE__);
    return;
  }

  if (!m_cache) {
    osMutexAcquire(_scdc_ctx.mtx_out_pkts[outidx], osWaitForever);
    mutex_get = true;
  }


  finfo.priv_data = priv;
  finfo.pre_len = 0;
  finfo.pad_len = 0;

  pkt = (m_cache ? &local_pkt : kzalloc(sizeof(*pkt)));
  assert(pkt != NULL);
  pkt->itf = itf;
  pkt->outidx = outidx;
  pfinfo = &pkt->finfo;
  pfinfo->priv_data = priv;

  if (m_cache) {
    finfo.ptr_lin = (void *) mtod(m_cache, caddr_t);
    finfo.len_lin = len;
    pkt->ext_buf = (uint8_t *) m_cache;
  } else {
    scdc_read_info(itf, outidx, (void *)&finfo);
  }

  /* Set the outpkt entry up to represent its own territory
   * inside the whole rx fifo which is represented by
   * finfo.
   */
  if (pkt->ext_buf)
  {
    SCDC_ASSERT(len == finfo.len_lin + finfo.len_wrap,);
    memcpy(pfinfo, &finfo, sizeof(finfo));
  }
  else
  {
    if (list_empty(&_scdc_ctx.out_pkts[outidx]))
    {
      /* Note that the only outpkt might possibly have wrapped
       * because the previous one was located at near the end of
       * rx_ff.
       * Anyway, its fifo info should be the same as finfo.
       */
      SCDC_ASSERT(len == finfo.len_lin + finfo.len_wrap,);
      memcpy(pfinfo, &finfo, sizeof(finfo));
    }
    else
    {
      scdc_update_buf_ptr_info(itf, outidx, len, finfo, pfinfo);
    }

  }


  /* Add this new outpkt entry to the list. */
  if (!pkt->ext_buf)
    list_add_tail(&pkt->list, &_scdc_ctx.out_pkts[outidx]);

  if (scdc_prep_in(itf, outidx, pfinfo)) {
    purge_outp(pkt, false, !mutex_get);
    goto reorder;
  }

  /*
   * Cannot change pre_len and ptr_lin directly here
   * + Advancing the FIFO read idx use pre_len
   * + Compute next pfinfo use both: pfinfo->ptr_lin = pptr + plen;
   */
  pfinfo->ptr_lin += pfinfo->pre_len;
  read_out_hdr(pkt, &hdr);
  pkt->seqno = hdr.seqno;
  pfinfo->ptr_lin -= pfinfo->pre_len;

  fws_update_dbg_info(pkt->finfo.ptr_lin + sizeof(hdr), pkt->seqno, outidx);

  SCDC_ASSERT(((hdr.flags & SCDC_FLAG_VER_MASK) >> SCDC_FLAG_VER_SHIFT) ==
    SCDC_PROTO_VER,);

  headroom = sizeof(hdr) + (hdr.data_offset << 2) + SNCMF_FW_MAC_HDR_LEN;

  SCDC_LOG2("[%s] outidx:%04d, lin:%04d(0x%08x), wrap:%04d(0x%08x), seqno:%04d\n", __func__, outidx
    , pfinfo->len_lin, pfinfo->ptr_lin, pfinfo->len_wrap, pfinfo->ptr_wrap, hdr.seqno);

  vap = wlan_get_vap(dev, SCDC_GET_IF_IDX(&hdr));
  ifp = vap->iv_ifp;

  SCDC_ASSERT(vap != NULL && ifp != NULL,);

  /* Set up an appropriate type of mbuf(s) and load them with the packet
   * received in the rx fifo pointed by pfinfo.
   */

  if (pfinfo->len_lin <= headroom)
  {
    /* We (might) need to have a linear headroom down the road.
    * Let's just copy the packet into a chain of MBUF_CACHE type mbufs.
    */
    m = m_gethdr(M_NOWAIT, MT_DATA);
    if (m == NULL)
    {
      SCDC_LOG1("[%s, %d] mbuf alloc failed. (seqno=0x%04x)\n", __func__, __LINE__, hdr.seqno);
      purge_outp(pkt, false, !mutex_get);
      goto reorder;
    }
    m->m_len = MHLEN;
    m_copyback(m, 0, pfinfo->len_lin, pfinfo->ptr_lin, 0);
    m_copyback(m, pfinfo->len_lin, pfinfo->len_wrap, pfinfo->ptr_wrap, 0);
    if (m->m_pkthdr.len < pfinfo->len_lin + pfinfo->len_wrap)
    {
      /* m_copyback failed. Give up. */
        SCDC_LOG1("[%s, %d] m_copyback failed.\n", __func__, __LINE__);
      purge_outp(pkt, false, !mutex_get);
      m_freem(m);
      m = NULL;
      goto reorder;
    }
    if (!m->m_next)
    {
      m->m_len = m->m_pkthdr.len;
    }

    if ((m = (struct mbuf *) scdc_get_buf(itf, outidx, SCDC_BUF_DESC)))
      m_freem(m);
    scdc_set_buf(itf, outidx, SCDC_BUF_DESC, NULL);
    /* We can safely release the space used by the outpkt in the rx fifo
    * because data payload has already been copied into a chain of mbufs.
    */
    purge_outp(pkt, false, !mutex_get);
    goto pass;
  }

  m = (struct mbuf *) scdc_get_buf(itf, outidx, SCDC_BUF_DESC);

  if (!m) {
    m = m_getexthdr(M_NOWAIT, MT_DATA);
  }

  if (m)
  {
    m_dyna_extadd(m, (caddr_t)(pfinfo->ptr_lin + pfinfo->pre_len),
    pfinfo->len_lin - pfinfo->pad_len - pfinfo->pre_len,
    scdc_mbuf_free_cb, pkt->ext_buf ? 0 : (uint32_t)pkt, (uint32_t)pkt->ext_buf, 0, EXT_IPCRING);
  }
  else
  {
    SCDC_LOG1("[%s, %d] mbuf alloc failed.(seqno=0x%04x)\n", __func__, __LINE__, hdr.seqno);
    purge_outp(pkt, false, !mutex_get);
    if (pkt->ext_buf) {
      scdc_set_buf(itf, outidx, SCDC_CACHE_BUF, NULL);
    }

    goto reorder;
  }

  if (is_wrapped(pkt))
  {
    m->m_next = m_getext(M_NOWAIT, MT_DATA);
    if (m->m_next)
    {
      m_dyna_extadd(m->m_next, (caddr_t)(pfinfo->ptr_wrap), pfinfo->len_wrap,
        0, 0, 0, 0, EXT_IPCRING);
    }
    else
    {
      SCDC_LOG1("[%s, %d] mbuf alloc failed. (seqno=0x%04x)\n", __func__, __LINE__, hdr.seqno);
      m_freem(m);
      purge_outp(pkt, false, !mutex_get);
      goto reorder;
    }
  }

  scdc_set_buf(itf, outidx, SCDC_BUF_DESC, NULL);

  if (pkt->ext_buf)
    scdc_set_buf(itf, outidx, SCDC_CACHE_BUF, NULL);

  scdc_prep_out(itf, outidx, false, pfinfo);

pass:

  m->m_data += headroom;
  m->m_len -= headroom;
  m_fixhdr(m);
  m->m_pkthdr.rcvif = ifp;

  if (vap->iv_opmode == IEEE80211_M_HOSTAP)
  {
    const struct ether_header *eh = mtod(m, struct ether_header *);
    if (IEEE80211_IS_MULTICAST(eh->ether_dhost))
    {
      m->m_flags |= M_MCAST;
    }
  }

reorder:

  if (!m_cache)
    osMutexRelease(_scdc_ctx.mtx_out_pkts[outidx]);

  SCDC_LOG2("[%s, %d] m:0x%08x   (%04d, 0x%08x), seqno:%08d\n", __func__, __LINE__,
    (uint32_t)m, m_length(m, NULL), m->m_flags, hdr.seqno);

#ifdef CONFIG_SUPPORT_MTSMP
  if (m->m_pkthdr.len > PACKET_LEN_THR) {
    if (tsmp_get_count(outcount) >= COUNT_NUM)
      tsmp_set_count(outcount, 0);
      SCDC_LOG2("[%s, %d] len: %d m: %p\n", __func__, __LINE__, m->m_pkthdr.len, m);
      if (!tsmp_valid(out[tsmp_get_count(outcount)])) {
        tsmp_set_idx(m, tsmp_get_count(outcount) + 1);
        tsmp_set_out(m, m_out, (void * )m);
        tsmp_time_out(m, start_t);
      }
      tsmp_inc_count(outcount);
  }
#endif

#ifdef CONFIG_SCDC_DATA_REORDER
  reorder_pkt(m, hdr.seqno, !!(hdr.flags & SCDC_FLAG_SEQ_SYNC));
#else
  if (m) {
    ifp = m->m_pkthdr.rcvif;
    ifp->if_output(ifp, m, NULL, NULL);
  }
#endif
}

bool scdc_vendor_control_xfer_cb(uint8_t itf, uint8_t request, void** buffer, uint16_t len)
{
  scdc_ctx_t* p_ctx = &_scdc_ctx;
  scdc_dcmdbuf_t* p_msgbuf = &p_ctx->msgbuf;

  fwil_handler_t *fh;

  p_ctx->itf = itf;
  p_ctx->len = len;

  switch (request)
  {
    case SCDC_REQUEST_SET:

      p_msgbuf = (scdc_dcmdbuf_t*)*buffer;

      SCDC_LOG3("  cmd: %d, len: %d, flags: 0x%x, status: 0x%x\r\n",
                p_msgbuf->msg.cmd, p_msgbuf->msg.len, p_msgbuf->msg.flags,
                p_msgbuf->msg.status);

          fh = fwil_find_handler(p_msgbuf->msg.cmd);
      if (fh && fh->cb)
      {
        uint32_t flags = p_msgbuf->msg.flags;
#define   FLAG_FIELD(f)	((flags & f ##_MASK) >> f ##_SHIFT)
        fh->len = p_msgbuf->msg.len;
        fh->buf = p_msgbuf->buf;
        fh->ifn = FLAG_FIELD(SCDC_DCMD_IF);
        fh->set = FLAG_FIELD(SCDC_DCMD_SET);

        SCDC_LOG1("  cmd: %d, len: %d, buf: 0x%x\r\n",
                  fh->cmd, fh->len, fh->buf);

        if (0 != fh->cb(fh))
          return false;
      }
      else
      {
        SCDC_LOG1("Unsupported - cmd: %d, len: %d, flags: 0x%x, status: 0x%x\r\n",
                  p_msgbuf->msg.cmd, p_msgbuf->msg.len, p_msgbuf->msg.flags,
                  p_msgbuf->msg.status);
      }

    break;

    case SCDC_REQUEST_GET:

      *buffer = p_msgbuf;

      SCDC_LOG3("%s: buffer:%p *buffer:%p\n", __func__, buffer, *buffer);

    break;

    default: return false; // stall unsupported request
  }

  return true;
}

bool scdc_vendor_out_pkts_empty(int outidx)
{
	bool empty = false;

	osMutexAcquire(_scdc_ctx.mtx_out_pkts[outidx], osWaitForever);
	if (list_empty(&_scdc_ctx.out_pkts[outidx])) {
		empty = true;
	}
	osMutexRelease(_scdc_ctx.mtx_out_pkts[outidx]);

	return empty;
}

int scdc_vendor_get_buf(int size, uint32_t *offset, uint8_t **buf, uint8_t **buf_desc, uint8_t op)
{
  struct mbuf *m_cache = NULL;
  struct mbuf *m_desc = NULL;

  if (!(op & (SCDC_BUF_DESC | SCDC_CACHE_BUF))) {
    SCDC_LOG1("%s invalid operation %u\n", __func__, op);
    return -1;
  }

  if (op & SCDC_CACHE_BUF) {
    if (size > MHLEN || size <= 0) {
        SCDC_LOG1("%s error: %d > %d \n", __func__, size, MHLEN);
        return -1;
    }

    m_cache = m_gethdr(M_NOWAIT, MT_DATA);
    if (m_cache == NULL)
      return -1;

    *buf = (uint8_t *) m_cache;
    *offset = (uint32_t) (mtod(m_cache, caddr_t) - (caddr_t)m_cache);
  }

  if (op & SCDC_BUF_DESC) {
    m_desc = m_getexthdr(M_NOWAIT, MT_DATA);
    if (m_desc == NULL) {
      SCDC_LOG1("%s m_desc get fail \n", __func__);
      if (m_cache) {
        m_freem(m_cache);
        *offset = 0;
        *buf = NULL;
      }
      *buf_desc = NULL;
      return -1;
    }
    *buf_desc = (uint8_t *) m_desc;
  }

  return 0;
}

#ifdef CONFIG_SCDC_DUMP_OUT_PKTS
int scdc_vendor_dump_out_pkts(void)
{
  int outidx = 0;
  int count __maybe_unused = 0;
  seq_mbuf *smbuf;


  for(outidx = 0; outidx < _scdc_ctx.out_pipe_num; outidx++)
  {
    osMutexAcquire(_scdc_ctx.mtx_out_pkts[outidx], osWaitForever);
    if (!list_empty(&_scdc_ctx.out_pkts[outidx]))
    {
      out_pkt_t *each_pkt;

      list_for_each_entry(each_pkt, &_scdc_ctx.out_pkts[outidx], list)
      {
        SCDC_LOG1("ep[%d] seqno:%d ptr:0x%X len:%d \n",
          outidx, each_pkt->seqno, each_pkt->finfo.ptr_lin, each_pkt->finfo.len_lin);
      }
    }

    osMutexRelease(_scdc_ctx.mtx_out_pkts[outidx]);
  }

  count  = 0;
  SCDC_LOG1("next_seqno (%4d) \n", _scdc_ctx.out_next_seqno);

  list_for_each_entry(smbuf, &_scdc_ctx.reorder_q, list)
  {
    SCDC_LOG1("reorder_q[%2d] seqno (%4d) \n", count++, smbuf->seqno);
  }

  return 0;
}

/* ========== */

#endif /* CONFIG_SCDC_DUMP_OUT_PKTS */

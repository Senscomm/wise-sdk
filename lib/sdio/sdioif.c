/*
 * Copyright 2022-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* check the MBUF_DYNA_EXT definition */
#ifndef CONFIG_FREEBSD_MBUF_DYNA_EXT
#error FREEBSD_MBUF_DYNA_EXT did not define
#endif

/* important!!! */
/* For using RX_DESC_DEF , you should reduce CONFIG_MEMP_NUM_MBUF_CACHE should be reduce as 64 */
#if CONFIG_MEMP_NUM_MBUF_CACHE > 64
#error should reduce MEMP_NUM_MBUF_CACHE(64)/CONFIG_RX_BUF_NUM_LOG2(6)
#endif

#include <mbuf.h>
#include <kernel.h>
#include <atomic.h>
#include <systm.h>
#include <malloc.h>
#include <mutex.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <compat_if.h>
#include <if_arp.h>
#include <if_llc.h>
#include <ethernet.h>
#include <if_dl.h>
#include <if_media.h>
#include <compat_if_types.h>
#include <if_ether.h>
#include <lwip-glue.h>
#include <freebsd/if_var.h>
#include <cmsis_os.h>
#include <hal/timer.h>
#include <hal/unaligned.h>
#include <hal/wlan.h>
#include <net80211/ieee80211_var.h>
#include <scm2020_var.h>
#include <hal/sdio.h>
#include <scdc.h>
#include <hal/kmem.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sdioif.h"
#include "sdio-fifo.h"
#include "sdio-filter.h"

#define SDIO_FN_TX 1
#define SDIO_FN_RX 2

struct sdio_netif_context {
	struct ifnet *ifp;
	struct list_head event_queue;
	struct ifqueue txdata_queue;
	struct list_head rx_pkts;
	osThreadId_t rxtid;
	osSemaphoreId_t rxsync;
#ifdef CONFIG_SDIO_VERIFICATION_TEST
	bool loopback;
#endif
#ifdef CONFIG_SDIO_RECOVERY
	bool recover;
	osThreadId_t recov_tid;
	osSemaphoreId_t recov_sync;
#endif
	osThreadId_t resetrxbuf_tid;
	sdio_fifo_t rx_fifo;
	struct device *dev;
};

struct sdio_netif_context sdio_netif_ctx;

/* Reserved RX Buffer in Func WiFi */
__sdio_dma_desc__ static u8 rcv_buffer[CONFIG_SDIO_RCV_BUFFER_SIZE];


// #define SDIO_RX_DEBUG
// #define SDIO_LOG_RX_PKT
#ifdef SDIO_RX_DEBUG
#define SDIO_FLOG printk  /* err debug log */
#define SDIO_FLOG1(...)   /* info debug log */

uint32_t g_sdio_debug_rxidx = 0;
#ifdef SDIO_LOG_RX_PKT
struct sdio_dbg_rx_info {
	uint32_t sdio_rxidx;
	uint16_t offset;
	uint16_t len;
};

static struct sdio_dbg_rx_info sdio_dbg_rx[256] = {0};

#define SDIO_LOG_RX(pkt_offset, pkt_len) \
	u8 dbg_logidx = g_sdio_debug_rxidx & 0xFF; \
	sdio_dbg_rx[dbg_logidx].sdio_rxidx = g_sdio_debug_rxidx; \
	sdio_dbg_rx[dbg_logidx].offset = pkt_offset; \
	sdio_dbg_rx[dbg_logidx].len = pkt_len;

void sdio_dbg_show_rx_info(void)
{
	int i;

	printk("rxidx offset len\n");
	for (i = 0; i < 256; i++) {
		printk("%u  %u  %u\n", sdio_dbg_rx[i].sdio_rxidx,
			sdio_dbg_rx[i].offset, sdio_dbg_rx[i].len);
	}
}
#else
#define SDIO_LOG_RX(...)
#define sdio_dbg_show_rx_info(...)
#endif
#else
#define SDIO_FLOG(...)
#define SDIO_FLOG1(...)
#define SDIO_LOG_RX(...)
#define sdio_dbg_show_rx_info(...)
#endif

#ifdef GPIO_DBG
extern void control_gpio_out(uint8_t gpio, uint8_t high);

#define SDIO_GPIO_ASSERT()  \
		sdio_dbg_show_rx_info();  \
		control_gpio_out(24, 1);  \
		assert(0);
#else
#define SDIO_GPIO_ASSERT()
#endif

// #define SDIO_TRX_DUMP
#ifdef SDIO_TRX_DUMP
void sdio_dump_data(u8 *buf, int len)
{
	for (int i = 0; i < len; i++) {
		if (i % 16 == 0)
			printk("\n");
		printk("%02x ", buf[i]);
	}
	printk("\n buf ptr addr %p len %d\n", buf, len);
}
#else
#define sdio_dump_data(...)
#endif

static inline void sdio_update_hwhdr(u8 *header, u16 frm_length)
{
	*(u16 *)header = htole16(frm_length);
	*(((u16 *)header) + 1) = htole16(~frm_length);
}

static void sdio_hdr_pack(u8 *header, u16 len, u8 type, u16 head_pad)
{
	u32 hdrval = 0;
	u8 hdr_offset;

	sdio_update_hwhdr(header, len);
	hdr_offset = SDPCM_HWHDR_LEN;

	hdrval |= (type << SDPCM_CHANNEL_SHIFT) &
		SDPCM_CHANNEL_MASK;
	hdrval |= ((SDPCM_HDRLEN + head_pad) << SDPCM_DOFFSET_SHIFT) &
		SDPCM_DOFFSET_MASK;
	*((u32 *)(header + hdr_offset)) = htole32(hdrval);
}

static void sdio_hdr_fill_firstfrag(u8 *header, u16 totallen)
{
	u32 hdrval = 0;
	hdrval |= (totallen << SDPCM_TLEN_SHIFT) & SDPCM_TLEN_MASK;

	*((u32 *)(header + SDPCM_HWHDR_LEN)) |= htole32(hdrval);
}

#if CONFIG_SDIO_PM
int sdio_notify_host_reenum(void)
{
	struct sdio_netif_context *ctx = &sdio_netif_ctx;
	struct device *dev = ctx->dev;
	return sdio_reenum_host(dev);
}
#endif

void sdio_vendor_write(void *buffer, uint32_t bufsize, bool isevent)
{
	struct sdio_netif_context *ctx = &sdio_netif_ctx;
	struct device *dev = ctx->dev;
	struct mbuf *m, *m0;
	uint32_t len;
	u8 *buf = NULL;
	u16 head_pad;

#ifdef CONFIG_SDIO_RECOVERY
	/* block until recover done */
	while (ctx->recover == true) {
		osDelay(1);
	}
#endif
	if (isevent) {
		/* Reserve different m->m_data with max SDIO_DMA_ALIGNMENT */
		u8 head_res = SDPCM_HDRLEN + SDIO_DMA_ALIGNMENT;
		m0 = m_gethdr(M_NOWAIT, MT_DATA);
		if (m0 == NULL) {
			sdio_dbg_log("[%s, %d] mbuf alloc failed\n", __func__, __LINE__);
			return;
		}
		m0->m_data += head_res;
		m0->m_len = MHLEN - head_res;

		m_copyback(m0, 0, bufsize, buffer, head_res);
		if (m0->m_pkthdr.len < bufsize) {
			/* m_copyback failed. Give up. */
			sdio_dbg_log("[%s, %d] m_copyback failed.\n", __func__, __LINE__);
			m_freem(m0);
			return;
		}
		if (!m0->m_next) {
			m0->m_len = m0->m_pkthdr.len;
		}

		buffer = m0;
		sdio_dbg_log("%s sdio tx evt(%d)\n", __func__, bufsize);
	}

	for (m = (struct mbuf *)buffer; (m != NULL) && (m->m_len != 0); m = m->m_next) {
		head_pad = ((unsigned long)m->m_data % SDIO_DMA_ALIGNMENT);

		/* Although RX Mbufs reserve ic->ic_headroom,
		 * but in the RX data path,
		 * there is still appending Mbuf without reserved headroom.
		 */
		if (M_LEADINGSPACE(m) < (SDPCM_HDRLEN + head_pad)) {
			return;
		}
		M_PREPEND(m, SDPCM_HDRLEN + head_pad, M_NOWAIT);
		assert(m != NULL);

		/* SDIO Hdr Packing */
		if (isevent)
			sdio_hdr_pack(mtod(m, u8 *), m->m_len, SDPCM_EVENT_CHANNEL, head_pad);
		else
			sdio_hdr_pack(mtod(m, u8 *), m->m_len, SDPCM_DATA_CHANNEL, head_pad);
	}

	m0 = (struct mbuf *)buffer;
	m_fixhdr(m0);

	/* Set frag total len */
	if (m0->m_next != NULL) {
		sdio_hdr_fill_firstfrag(mtod(m0, u8 *), m0->m_pkthdr.len);
	}

	for (m = m0; (m != NULL) && (m->m_len != 0); m = m->m_next) {
		len = m->m_len;
		buf = mtod(m, u8 *);

		sdio_dump_data(buf, len);
		sdio_tx(dev, SDIO_FN_TX, buf, len);
	}

	/* allocated mbuf should be released */
	if (isevent)
		m_freem(m0);
}

#ifdef CONFIG_SDIO_TXCHAIN
void sdio_vendor_chainwrite(struct ifqueue *data_queue)
{
	u8 send_cnt = 0;
	u16 head_pad;
	struct mbuf *m, *m0;
	struct mbuf *freem_list = NULL;
	struct sdio_netif_context *ctx = &sdio_netif_ctx;
	struct device *dev = ctx->dev;

#ifdef CONFIG_SDIO_RECOVERY
	/* block until recover done */
	while (ctx->recover == true) {
		osDelay(1);
	}
#endif

	sdio_acquire_tx(dev);
	while((m0 = ifq_dequeue(data_queue))) {
		for (m = m0; (m != NULL) && (m->m_len != 0); m = m->m_next) {
			head_pad = ((unsigned long)m->m_data % SDIO_DMA_ALIGNMENT);

			/* Although RX Mbufs reserve ic->ic_headroom,
			 * but in the RX data path,
			 * there is still appending Mbuf without reserved headroom.
			 */
			if (M_LEADINGSPACE(m) < (SDPCM_HDRLEN + head_pad)) {
				m_freem(m0);
				m0 = NULL;
				break;
			}
			M_PREPEND(m, SDPCM_HDRLEN + head_pad, M_NOWAIT);
			assert(m != NULL);

			/* SDIO Hdr Packing */
			sdio_hdr_pack(mtod(m, u8 *), m->m_len, SDPCM_DATA_CHANNEL, head_pad);
		}
		if (m0 == NULL) {
			continue;
		}
		m_fixhdr(m0);

		/* Set frag total len */
		if (m0->m_next != NULL) {
			sdio_hdr_fill_firstfrag(mtod(m0, u8 *), m0->m_pkthdr.len);
		}
		for (m = m0; (m != NULL) && (m->m_len != 0); m = m->m_next) {
			//printk("## send_cnt %u m_len %u\n", send_cnt, m->m_len);
			sdio_txchain_addelem(dev, mtod(m, u8 *), m->m_len, send_cnt);
			send_cnt++;
		}
		if (freem_list == NULL) {
			freem_list = m0;
		} else {
			m0->m_nextpkt = freem_list;
			freem_list = m0;
		}
	}
	if (send_cnt) {
		sdio_txchain_kick(dev, SDIO_FN_TX, send_cnt);
		/* free sent mbufs */
		while ((m0 = freem_list) != NULL) {
			freem_list = freem_list->m_nextpkt;
			m0->m_nextpkt = NULL;
			m_freem(m0);
		}
	} else {
		sdio_release_tx(dev);
	}
}
#endif

void sdio_vendor_get_read_info(u8 itf, u8 outidx, void *pinfo)
{
	struct sdio_netif_context *ctx = &sdio_netif_ctx;
	sdio_fifo_buffer_info_t *pfinfo = (sdio_fifo_buffer_info_t *)pinfo;
	struct sdio_req *req = (struct sdio_req *)pfinfo->priv_data;

	if (req->len == 0) {
		pfinfo->len_lin  = 0;
		pfinfo->len_wrap = 0;
		pfinfo->ptr_lin  = NULL;
		pfinfo->ptr_wrap = NULL;
		SDIO_LOG1("%s error cnt = 0 w = %d r = %d \n", __func__, w, r);
		return;
	}
	pfinfo->len_lin  = req->len;

	sdio_fifo_array_get_read_linear_info(&ctx->rx_fifo, pfinfo);
}

void sdio_vendor_array_linearize(u8 itf, u8 outidx, void *pinfo)
{
	struct sdio_netif_context *ctx = &sdio_netif_ctx;
	sdio_fifo_buffer_info_t *pfinfo = (sdio_fifo_buffer_info_t *)pinfo;

	sdio_fifo_array_linearize(&ctx->rx_fifo, pfinfo);
}

void sdio_vendor_advance_rptr(uint8_t itf, uint8_t outidx, uint16_t n)
{
	struct sdio_netif_context *ctx = &sdio_netif_ctx;

	sdio_fifo_array_advance_read_locked(&ctx->rx_fifo, n);
}

u8 sdio_vendor_get_extra_hdr_len(void)
{
	return 0;
}

uint32_t sdio_vendor_get_out_size(bool cnt_in_pkts)
{
	uint32_t outbufisze = CONFIG_SDIO_RCV_BUFFER_SIZE;

	if (cnt_in_pkts)
		outbufisze = (outbufisze / CONFIG_SDIO_FIFO_MIN_LINEAR_SIZE);

	return outbufisze;
}

void sdio_vendor_prep_out(uint8_t itf, uint8_t bufidx, bool async, void *pinfo)
{
	u16 rd_idx, mbuf_ext_avail;
	struct sdio_netif_context *ctx = &sdio_netif_ctx;

	rd_idx = sdio_fifo_array_get_read_idx(&ctx->rx_fifo);
	mbuf_ext_avail = memp_available(MEMP_MBUF_EXT_NODE);

	sdio_write_flowctrl_info(ctx->dev, rd_idx, mbuf_ext_avail);
}

int sdio_vendor_pre_in(uint8_t itf, uint8_t bufidx, void *pinfo);

struct scdc_ops sdio_scdc_ops = {
	.write = sdio_vendor_write,
	.read_info = sdio_vendor_get_read_info,
	.array_linearize = sdio_vendor_array_linearize,
	.advance_outptr  = sdio_vendor_advance_rptr,
	.prep_out = sdio_vendor_prep_out,
	.get_extra_hdr_len = sdio_vendor_get_extra_hdr_len,
	.get_out_size = sdio_vendor_get_out_size,
	.prep_in = sdio_vendor_pre_in,
#ifdef CONFIG_SDIO_TXCHAIN
	.chainwrite = sdio_vendor_chainwrite,
#endif
#ifdef CONFIG_SDIO_PM
	.awake_host = sdio_notify_host_reenum,
#endif
};

static inline u8 sdio_getdatoffset(u8 *swheader)
{
	u32 hdrvalue;
	hdrvalue = *(u32 *)swheader;
	return (u8)((hdrvalue & SDPCM_DOFFSET_MASK) >> SDPCM_DOFFSET_SHIFT);
}

static int sdio_hdparse(u8 *header, u32 hdr_len, u8 *channel, u8 *dat_offset, u16 *data_len)
{
	u16 len, checksum;
	u32 swheader;

	if (hdr_len < SDPCM_HWHDR_LEN) {
		sdio_err_log("HW header length error %u\n", hdr_len);
		return -EPROTO;
	}

	/* hw header */
	len = get_unaligned_le16(header);
	*data_len = len;
	checksum = get_unaligned_le16(header + sizeof(u16));
	/* All zero means no more to read */
	if (!(len | checksum)) {
		sdio_err_log("checksum fai\n");
		return -ENODATA;
	}
	if ((u16)(~(len ^ checksum))) {
		sdio_err_log("HW header checksum error %x:%x\n", len, checksum);
		return -EIO;
	}
	if (len < SDPCM_HDRLEN) {
		sdio_err_log("HW header length error %u\n", len);
		return -EPROTO;
	}
	/* software header */
	header += SDPCM_HWHDR_LEN;
	swheader = get_unaligned_le32(header);
	*dat_offset = sdio_getdatoffset(header);
	if (*dat_offset > len) {
		sdio_err_log("header len error offset %u len %u\n", *dat_offset, len);
		return -EOVERFLOW;
	}
	if (*data_len > hdr_len) {
		sdio_err_log("data_len[%u] > len[%u]\n", *data_len, hdr_len);
		return -ENOMEM;
	}

	*channel = (swheader & SDPCM_CHANNEL_MASK) >> SDPCM_CHANNEL_SHIFT;

	sdio_dbg_log("%s len=%d chan=%d dat_offset=%u\n", __FUNCTION__, len, *channel, *dat_offset);

	return 0;
}

int sdio_vendor_pre_in(uint8_t itf, uint8_t bufidx, void *pinfo)
{
	int err;
	uint16_t data_len = 0;
	u8 dat_offset = 0;
	u8 chan = SDPCM_CONTROL_CHANNEL;
	struct sdio_netif_context *ctx = &sdio_netif_ctx;
	struct device *dev = ctx->dev;
	sdio_fifo_buffer_info_t *pfinfo = (sdio_fifo_buffer_info_t *)pinfo;

#ifdef SDIO_RX_DEBUG
	g_sdio_debug_rxidx++;
	SDIO_FLOG1("%u pre_in offset %u len %u\n", g_sdio_debug_rxidx,
				((u8*)pfinfo->ptr_lin - rcv_buffer), pfinfo->len_lin);
	sdio_dump_data(pfinfo->ptr_lin, pfinfo->len_lin);
	SDIO_LOG_RX(((u8*)pfinfo->ptr_lin - rcv_buffer), pfinfo->len_lin);
#endif
#ifdef CONFIG_SDIO_VERIFICATION_TEST
	if (ctx->loopback) {
		sdio_tx(dev, SDIO_FN_TX, pfinfo->ptr_lin, pfinfo->len_lin);
		return 1;
	}
#endif

	err = sdio_hdparse(pfinfo->ptr_lin, pfinfo->len_lin, &chan, &dat_offset, &data_len);
	if (err) {
		SDIO_FLOG("%u sdio_hdparse fail %d offset %u len %u\n", g_sdio_debug_rxidx, err,
			((u8*)pfinfo->ptr_lin - rcv_buffer), pfinfo->len_lin);
		SDIO_GPIO_ASSERT();
#ifdef CONFIG_SDIO_RECOVERY
		if (!ctx->recover) {
			ctx->recover = true;
			/* Notify the host to recover immediately */
			sdio_recover_info_proc(ctx->dev, true, 0x1);
			osSemaphoreRelease(ctx->recov_sync);
		}
#endif
		return 1;
	}
	if (chan == SDPCM_CONTROL_CHANNEL) {
		void *msg_buf = pfinfo->ptr_lin + dat_offset;
		scdc_vendor_control_xfer_cb(0, SCDC_REQUEST_SET, &msg_buf, pfinfo->len_lin);

		/* Reply to Host */
		SDIO_FLOG1("Reply to Host\n");
		sdio_dump_data(pfinfo->ptr_lin, pfinfo->len_lin);
		sdio_tx(dev, SDIO_FN_TX, pfinfo->ptr_lin, pfinfo->len_lin);
		return 1;
	} else if (chan == SDPCM_DATA_CHANNEL) {
		pfinfo->pre_len = dat_offset;
		pfinfo->pad_len = pfinfo->len_lin - data_len;
	} else {
		SDIO_FLOG("%u chan err %u offset %u len %u\n", g_sdio_debug_rxidx, chan,
			((u8*)pfinfo->ptr_lin - rcv_buffer), pfinfo->len_lin);
		SDIO_GPIO_ASSERT();
		return 1;
	}

	return 0;
}

static void sdio_rxmain(void *arg)
{
	u32 flags;
	LIST_HEAD_DEF(waiting);
	struct sdio_req *req, *tmp;
	struct list_head *entry, *tmp1;
	struct sdio_netif_context *ctx = arg;
	struct device *dev = ctx->dev;

	while (1) {
		osSemaphoreAcquire(ctx->rxsync, osWaitForever);

		local_irq_save(flags);
		if (list_empty(&ctx->rx_pkts)) {
			local_irq_restore(flags);
			continue;
		}

		list_for_each_safe(entry, tmp1, &ctx->rx_pkts) {
			list_move_tail(entry, &waiting);
		}
		local_irq_restore(flags);

		list_for_each_entry_safe(req, tmp, &waiting, entry) {
			list_del(&req->entry);
#ifdef CONFIG_SDIO_RECOVERY
			if (!ctx->recover)
#endif
				scdc_vendor_rx_cb(0, 0, req->len, (void *)req);
			sdio_release_rx_req(dev, req);
		}
	}
}

/* enqueue the rx_descriptor to the pkt queue */
__ilm__ static int sdio_enqueue_recvpkt(struct sdio_req *req)
{
	struct sdio_netif_context *ctx = &sdio_netif_ctx;

	/* Add this new input pkt entry to the list. */
	list_add_tail(&req->entry, &ctx->rx_pkts);

	/* Transferring flag */
	osSemaphoreRelease(ctx->rxsync);

	return 0;
}

__ilm__ static void sdio_req_filled(struct sdio_req *req)
{
	struct sdio_netif_context *ctx = &sdio_netif_ctx;

	/* Receive new date and advance write index for next data */
	sdio_fifo_array_advance_write_locked(&ctx->rx_fifo, req->len);

	sdio_enqueue_recvpkt(req);
}

static int sdio_req_getbuf(struct sdio_req *req)
{
	struct sdio_netif_context *ctx = &sdio_netif_ctx;

	req->buf = sdio_fifo_array_get_write_buf(&ctx->rx_fifo);

	return 0;
}

static void sdio_reset_rxbufmain(void *arg)
{
	u16 mbuf_ext_avail;
	struct sdio_netif_context *ctx = arg;
	struct device *dev = ctx->dev;

	/* Wait buffered RX to complete which affects the FIFO read&write index */
	while (1) {
		/* No Buffered SCDC RX Pkts */
		SDIO_FLOG1("No Buffered SCDC RX Pkts\n");
		if (scdc_vendor_out_pkts_empty(0) != true) {
			osDelay(1);
			continue;
		}
		/* No Buffered Rx Req and No Buffered IRQ */
		SDIO_FLOG1("No Buffered RX\n");
		if (sdio_buffered_rx(dev) != 0) {
			osDelay(1);
			continue;
		}

		break;
	}

	/* Reset FIFO read&write index */
	SDIO_FLOG1("Reset FIFO\n");
	sdio_fifo_array_reset(&ctx->rx_fifo);
	mbuf_ext_avail = memp_available(MEMP_MBUF_EXT_NODE);
	sdio_write_flowctrl_info(dev, 0, mbuf_ext_avail);

	ctx->resetrxbuf_tid = NULL;
	osThreadExit();
}

static void sdio_reset_rxbuf()
{
	struct sdio_netif_context *ctx = &sdio_netif_ctx;

	osThreadAttr_t sdioresetattr = {
		.name = "sdio-resetrx",
		.stack_size = CONFIG_DEFAULT_STACK_SIZE,
		.priority = osPriorityNormal,
	};

	/* This func will be called in RX context, schedule a thread to wait&reset RX buf  */
	if (ctx->resetrxbuf_tid == NULL) {
		ctx->resetrxbuf_tid = osThreadNew(sdio_reset_rxbufmain, ctx, &sdioresetattr);
	}
}

static void sdio_fifo_init(sdio_fifo_t *f)
{
	memset(f, 0, sizeof(sdio_fifo_t));
	sdio_fifo_config_mutex(f, osMutexNew(NULL), osMutexNew(NULL));
	sdio_fifo_config(f, rcv_buffer, CONFIG_SDIO_RCV_BUFFER_SIZE);
}

#ifdef CONFIG_SDIO_PM
void sdio_fifo_resume(void)
{
	struct sdio_netif_context *netif_ctx = &sdio_netif_ctx;

	sdio_fifo_config_nolock(&netif_ctx->rx_fifo, rcv_buffer, CONFIG_SDIO_RCV_BUFFER_SIZE);
}
#endif

#ifdef CONFIG_SDIO_RECOVERY
//#define SDIO_RLOG printk
#define SDIO_RLOG(...)

static void sdio_recovmain(void *arg)
{
	u16 mbuf_ext_avail;
	struct sdio_netif_context *ctx = arg;
	struct device *dev = ctx->dev;

	while (1) {
		osSemaphoreAcquire(ctx->recov_sync, osWaitForever);

		while (1) {
			/* 1 Wait the Host start recovery */
			SDIO_RLOG("Wait the Host start recovery\n");
			if (sdio_recover_info_proc(dev, false, 0) != 0x3) {
				osDelay(1);
				continue;
			}
			/* 2 No Buffered MBUF EXT Node */
			SDIO_RLOG("No Buffered MBUF EXT Nodes\n");
			mbuf_ext_avail = memp_available(MEMP_MBUF_EXT_NODE);
			if (mbuf_ext_avail != CONFIG_MEMP_NUM_MBUF_DYNA_EXT) {
				osDelay(1);
				continue;
			}
			/* 3 No Buffered SCDC RX Pkts */
			SDIO_RLOG("No Buffered SCDC RX Pkts\n");
			if (scdc_vendor_out_pkts_empty(0) != true) {
				osDelay(1);
				continue;
			}
			/* 4 No Buffered Rx Req and No Buffered IRQ */
			SDIO_RLOG("No Buffered Rx\n");
			if (sdio_buffered_rx(dev) != 0) {
				osDelay(1);
				continue;
			}
			/* Start recover:
			 * Reset FIFO read&write index;
			 * Recover = false;
			 * Notify the Host recover done */
			SDIO_RLOG("Reset FIFO and Notify the Host recover done\n");
			sdio_fifo_array_reset(&ctx->rx_fifo);
			ctx->recover = false;
			sdio_recover_info_proc(dev, true, 0);
			break;
		}
	}
}

static void sdio_recovery_init(struct sdio_netif_context *ctx)
{
	osThreadAttr_t sdiorecovattr = {
		.name = "sdio-recov",
		.stack_size = CONFIG_DEFAULT_STACK_SIZE,
		.priority = osPriorityHigh,
	};

	ctx->recov_sync = osSemaphoreNew(1, 0, NULL);
	assert(ctx->recov_sync);

	ctx->recov_tid = osThreadNew(sdio_recovmain, ctx, &sdiorecovattr);
	assert(ctx->recov_tid);
}

#ifdef CONFIG_CMD_SDIO
static int do_sdio_recover(int argc, char *argv[])
{
	struct sdio_netif_context *ctx = &sdio_netif_ctx;

	if (ctx->recover) {
		printk("recovery is doing");
		return 0;
	}

	ctx->recover = true;
	/* Notify the host to recover immediately */
	sdio_recover_info_proc(ctx->dev, true, 0x1);
	osSemaphoreRelease(ctx->recov_sync);

	return 0;
}
#endif
#else
#define sdio_recovery_init(...)
#endif

int sdio_netif_init(void)
{
	struct sdio_netif_context *ctx = &sdio_netif_ctx;
	struct device *dev;

	dev = device_get_by_name("sdio");

	if (dev == NULL) {
		return -ENODEV;
	}

	osThreadAttr_t sdiorxtattr = {
		.name = "sdio rx handler",
		.stack_size = CONFIG_DEFAULT_STACK_SIZE + 512,
		.priority = osPriorityNormal,
	};

	memset(ctx, 0, sizeof(*ctx));

	ctx->dev = dev;

	INIT_LIST_HEAD(&ctx->event_queue);

	ctx->rxsync = osSemaphoreNew(1, 0, NULL);
	assert(ctx->rxsync);

	INIT_LIST_HEAD(&ctx->rx_pkts);

	ctx->rxtid = osThreadNew(sdio_rxmain, ctx, &sdiorxtattr);
	assert(ctx->rxtid);

	sdio_fifo_init(&ctx->rx_fifo);

	/* Be cautious of this calling order.
	 * Callbacks need to be installed first.
	 */
	sdio_register_cb(dev, sdio_req_filled, sdio_req_getbuf, sdio_reset_rxbuf);
	sdio_start(dev, SDIO_FN_RX, CONFIG_SDIO_RCV_BUFFER_SIZE,
		CONFIG_SDIO_FIFO_MIN_LINEAR_SIZE, CONFIG_MEMP_NUM_MBUF_DYNA_EXT);

	scdc_init();
	scdc_vendor_reg(&sdio_scdc_ops);

#ifdef CONFIG_SUPPORT_SDIO_WIFI_FILTER
	sdio_wifi_filter_init();
#endif

	sdio_recovery_init(ctx);

	sdio_dbg_log("sdio netif initialized\n");

	return 0;
}

#ifdef CONFIG_CMD_SDIO

#include <cli.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <hal/kmem.h>

#ifdef CONFIG_SDIO_TXCHAIN
/* Test tx chain: mbuf1[len 10], mbuf2[len 20], mbuf3[1500] */
static int do_sdio_txchain(int argc, char *argv[])
{
	u8 *test_buf;
	struct mbuf *m;
	struct ifqueue data_queue;

	ifq_init(&data_queue, NULL);
	IFQ_SET_MAX_LEN(&data_queue, 4);

	test_buf = kzalloc(1500);
	if (test_buf == NULL) {
		printk("malloc net buf fail\n");
		return CMD_RET_FAILURE;
	}
	for (int i = 0; i < 1500; i++) {
		test_buf[i] = i;
	}

	/* mbuf1[len 10] */
	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (!m) {
		printk("malloc m fail\n");
		return CMD_RET_FAILURE;
	}
	m->m_data += 20; // for scdc header and alignment
	test_buf[0] = 1;
	memcpy(m->m_data, test_buf, 10);
	m->m_len = m->m_pkthdr.len = 10;
	ifq_enqueue(&data_queue, m);

	/* mbuf2[len 20] */
	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (!m) {
		printk("malloc m fail\n");
		return CMD_RET_FAILURE;
	}
	m->m_data += 20; // for scdc header and alignment
	test_buf[0] = 2;
	memcpy(m->m_data, test_buf, 20);
	m->m_len = m->m_pkthdr.len = 20;
	ifq_enqueue(&data_queue, m);

	/* mbuf3[len 1500] */
	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (!m) {
		printk("malloc m fail\n");
		return CMD_RET_FAILURE;
	}
	m->m_data += 20; // for scdc header and alignment
	m->m_len = MHLEN - 20;
	test_buf[0] = 3;
	m_copyback(m, 0, 1500, (caddr_t)test_buf, 20);
	ifq_enqueue(&data_queue, m);

	sdio_vendor_chainwrite(&data_queue);
	printk("[SDIO] txchain end\n");

	kfree(test_buf);
	return CMD_RET_SUCCESS;
}
#endif
#ifdef CONFIG_SDIO_VERIFICATION_TEST
extern int scdc_data_test(struct mbuf *m);

static void do_net_tx_buf_test(int data_size, int test_max_cnt)
{
	struct mbuf *m;
	int test_cnt;
	int m_buf_fail_cnt;

	u8 *test_buf = kzalloc(data_size);
	if (test_buf == NULL) {
		sdio_err_log("malloc net buf fail\n");
		return;
	}

	for (int i = 0; i < data_size; i++)
		test_buf[i] = i;

	m_buf_fail_cnt = 0;
	for (test_cnt = 0; test_cnt < test_max_cnt; test_cnt++) {
		m = m_gethdr(M_NOWAIT, MT_DATA);
		if (!m) {
			sdio_err_log("m_gethdr fail.\n");
			m_buf_fail_cnt++;
			udelay(100);
			continue;
		}

		m->m_data += 20; // for scdc header and alignment
		m->m_len = MHLEN - 20;
		m_copyback(m, 0, data_size, (caddr_t)test_buf, 20);
		if (m->m_pkthdr.len < data_size) {
			/* m_copyback failed. Give up. */
			sdio_err_log("m_copyback failed.\n");
			m_freem(m);
			continue;
		}
		if (!m->m_next) {
			m->m_len = m->m_pkthdr.len;
		}

#ifdef CONFIG_SCDC_DATA_TEST
		scdc_data_test(m);
#endif
		osDelay(10);
	}

	udelay(1000 * test_max_cnt);
	kfree(test_buf);
	printk("[SDIO] TEST Result Total try(%d), m_buf fail(%d) successful tx\n", test_cnt, m_buf_fail_cnt);
	printk("[SDIO] Test end\n");
}

static int do_sdio_tx_test(int argc, char *argv[])
{
	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	do_net_tx_buf_test(atoi(argv[1]), atoi(argv[2]));

	return CMD_RET_SUCCESS;
}

static int do_sdio_loopback(int argc, char *argv[])
{
	struct sdio_netif_context *ctx = &sdio_netif_ctx;
	int val = atoi(argv[1]);

	if (val != 0 && val != 1)
		return CMD_RET_USAGE;

	ctx->loopback = val == 1 ? true : false;

	return CMD_RET_SUCCESS;
}
#endif

extern void sdio_show_status(void);
static int do_sdio_status(int argc, char *argv[])
{
	sdio_show_status();
	return CMD_RET_SUCCESS;
}

static int do_sdio_rx_info(int argc, char *argv[])
{
	sdio_dbg_show_rx_info();
	return CMD_RET_SUCCESS;
}

extern void sdio_fifo_show_status(sdio_fifo_t *f);
extern int sdio_cli_read_flow_ctrl(void);

static int do_sdio_fifo_status(int argc, char *argv[])
{
	u32 v;
	struct sdio_netif_context *ctx = &sdio_netif_ctx;

	sdio_fifo_show_status(&ctx->rx_fifo);

	v = sdio_cli_read_flow_ctrl();
	printk("flow ctrl info reg %08x\n", v);

	return CMD_RET_SUCCESS;
}

extern int do_sdio_read_status(int argc, char *argv[]);
extern int do_sdio_write_status(int argc, char *argv[]);
extern int do_sdio_read_reg(int argc, char *argv[]);
extern int do_sdio_write_reg(int argc, char *argv[]);
extern int do_sdio_dump_cis_ram(int argc, char *argv[]);
extern int do_sdio_restore(int argc, char *argv[]);

static const struct cli_cmd sdio_cmd[] = {
#ifdef CONFIG_SDIO_VERIFICATION_TEST
	CMDENTRY(tx, do_sdio_tx_test, "", ""),	  CMDENTRY(loopback, do_sdio_loopback, "", ""),
#endif
#ifdef CONFIG_SDIO_RECOVERY
	CMDENTRY(recover, do_sdio_recover, "", ""),
#endif
	CMDENTRY(status, do_sdio_status, "", ""),
	CMDENTRY(fifo_status, do_sdio_fifo_status, "", ""),
	CMDENTRY(r_status, do_sdio_read_status, "", ""),
	CMDENTRY(w_status, do_sdio_write_status, "", ""),
	CMDENTRY(read_reg, do_sdio_read_reg, "", ""),
	CMDENTRY(write_reg, do_sdio_write_reg, "", ""),
	CMDENTRY(rx_info, do_sdio_rx_info, "", ""),
#ifdef CONFIG_SDIO_PM
	CMDENTRY(dump_cis, do_sdio_dump_cis_ram, "", ""),
	CMDENTRY(restore, do_sdio_restore, "", ""),
#endif
#ifdef CONFIG_SDIO_TXCHAIN
	CMDENTRY(txchain, do_sdio_txchain, "", ""),
#endif
};

static int do_sdio(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], sdio_cmd, ARRAY_SIZE(sdio_cmd));
	if (cmd == NULL) {
		return CMD_RET_USAGE;
	}

	return cmd->handler(argc, argv);
}

CMD(sdio, do_sdio, "CLI comamands for SDIO",
	"sdio read_reg" OR "sdio write_reg" OR
#ifdef CONFIG_SDIO_VERIFICATION_TEST
	"sdio tx" OR "sdio loopback" OR
#endif
#ifdef CONFIG_SDIO_RECOVERY
	"sdio recover" OR
#endif
	"sdio to" OR "sdio filter" OR "sdio status" OR "sdio rx_info" OR
	"sdio fifo_status" OR "sdio r_status" OR "sdio w_status");
#endif

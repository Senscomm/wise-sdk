/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/ipc.h>
#ifdef CONFIG_IPC_DMA
#include <hal/dma.h>
#endif
#include <hal/console.h>
#include "compat_param.h"
#include "systm.h"
#include "kernel.h"

#include "compat_if.h"
#include "if_media.h"
#include "compat_if_types.h"

#include "lwip/netif.h"
#include "lwip/netifapi.h"

#include <netinet/in.h>
#include <arpa/inet.h>

#include <net80211/ieee80211_var.h>

ipc_listener_t g_wlan_data_listener;

#define WLAN_HDRLEN               76  /* HW HDR 28 + Qos HDR 26 + Cipher 8 + AMSDU HDR 14 */

#define NUTTX_IPC_TASK_NAME "nx_rx_task"
#define NUTTX_TASK_STACK_SIZE       768


#define ifq_dequeue(ifq) ({ struct mbuf *__m; IFQ_DEQUEUE(ifq, __m); __m; })
#define ifq_peek(ifq) 	 ({ struct mbuf *__m; IFQ_POLL(ifq, __m); __m; })
#define ifq_enqueue(ifq, m) ({ int __ret; IFQ_ENQUEUE(ifq, m, __ret); __ret; })

static struct ifnet *g_wlan_ifnet[2] = {NULL, NULL};

struct nuttx_rx_t
{
#ifdef CONFIG_IPC_DMA
	struct device		* dma_dev;
#endif
	struct device		* ipc_dev;
	struct ifqueue		rx_queue;
	struct taskqueue	*nx_tq; /* deferred state thread */
	struct task			nx_rx_task;
};

static struct nuttx_rx_t g_nx_rx;

#ifdef CONFIG_IPC_DMA
#define MAX_NET_ETH_PKTSIZE 1514
#define IPC_DMA_DESC_NUM		(MAX_NET_ETH_PKTSIZE/MSIZE) + 1 + 1
#define MASK_DMA_ISR	true

__ilm__
int nuttx_dma_copydata(const struct mbuf *m, int len, caddr_t cp)
{
	struct device * dma_dev = g_nx_rx.dma_dev;
	u_int count;
	int desc_idx = 0;
	struct dma_desc_chain dma_desc[4];
	int remainder = 0;

	KASSERT(len >= 0, ("nuttx_dma_copydata, negative len %d", len));

	while (len > 0) {
		KASSERT(m != NULL, ("nuttx_dma_copydata, length > size of mbuf chain"));
		count = min(m->m_len, len);

		dma_desc[desc_idx].dst_addr = (u32) cp;
		dma_desc[desc_idx].src_addr = (u32) (mtod(m, caddr_t));
		dma_desc[desc_idx].len = count;
		remainder |= ((dma_desc[desc_idx].src_addr & 0x3) | (dma_desc[desc_idx].dst_addr & 0x3));

		len -= count;
		cp += count;
		m = m->m_next;
		assert(desc_idx < IPC_DMA_DESC_NUM);
		desc_idx++;
	}
	return dma_copy(dma_dev, false, true, dma_desc, desc_idx, MASK_DMA_ISR, remainder, NULL, NULL);

}

#endif

__ilm__ static
inline void nuttx_rx_runtask(void) {

	taskqueue_enqueue(g_nx_rx.nx_tq, &g_nx_rx.nx_rx_task);

}

__ilm__ static
void nuttx_rx_task(void *data, int pending)
{
	struct device * ipc_dev = g_nx_rx.ipc_dev;
	struct ifnet *ifp;
	uint32_t len = CONFIG_NET_ETH_PKTSIZE;
	struct mbuf *m;
	ipc_payload_t *payload;

	while((m = ifq_peek(&g_nx_rx.rx_queue))) {
		len = len < m->m_pkthdr.len ? m->m_pkthdr.len : len;
		payload = ipc_alloc(ipc_dev, len, true, false, IPC_MODULE_WLAN,
			IPC_CHAN_DATA, IPC_TYPE_REQUEST);

		/* avoid drop this mbuf */
		if (!payload) {
			nuttx_rx_runtask();
			return;
		}
		else
			m = ifq_dequeue(&g_nx_rx.rx_queue);

		ifp = (struct ifnet *) m->m_pkthdr.rcvif;
		if (ifp == g_wlan_ifnet[0]) {
			IPC_SET_WLAN(payload->flag, 0);
		} else {
			IPC_SET_WLAN(payload->flag, 1);
		}

#ifdef CONFIG_IPC_DMA
		if (nuttx_dma_copydata(m, m->m_pkthdr.len, (caddr_t)payload->data)) {
			m_copydata(m, 0, m->m_pkthdr.len, (caddr_t)payload->data);
		}
#else
		m_copydata(m, 0, m->m_pkthdr.len, (caddr_t)payload->data);
#endif
		m_freem(m);
		ipc_transmit(ipc_dev, payload);
	}

}

static int nuttx_ipc_init(struct device *ipc_dev)
{
#ifdef CONFIG_IPC_DMA
	struct device *dma_dev = device_get_by_name("dmac.0");

	assert(dma_dev);
	g_nx_rx.dma_dev = dma_dev;
#endif
	assert(ipc_dev);
	g_nx_rx.ipc_dev = ipc_dev;

	if (!g_nx_rx.nx_tq) {
		g_nx_rx.nx_tq = taskqueue_create_fast("nx_taskq", M_WAITOK | M_ZERO,
			taskqueue_thread_enqueue,
			&g_nx_rx.nx_tq);

		taskqueue_stack_size(&g_nx_rx.nx_tq, NUTTX_TASK_STACK_SIZE);

		taskqueue_start_threads(&g_nx_rx.nx_tq, 1, PI_NET, NUTTX_IPC_TASK_NAME);

		TASK_INIT(&g_nx_rx.nx_rx_task, 0, nuttx_rx_task, NULL);
		ifq_init(&g_nx_rx.rx_queue, NULL);
		IFQ_SET_MAX_LEN(&g_nx_rx.rx_queue, CONFIG_MEMP_NUM_MBUF_CACHE);
	}

	return 0;
}

__ilm__ static
void mbuf_ipc_free(struct mbuf *m)
{
	ipc_payload_t *payload = (ipc_payload_t *)m->m_ext.ext_arg1;
	struct device *dev = (struct device *)m->m_ext.ext_arg2;

	ipc_free(dev, payload);
}

__ilm__
static int nuttx_ipc_data_recvd(ipc_payload_t *payload, void *priv)
{
	struct mbuf *m;
	struct ifnet *ifp;
	struct device *dev = (struct device *)priv;
	struct ieee80211vap *vap;

	if (IPC_GET_WLAN(payload->flag) == 0) {
		ifp = g_wlan_ifnet[0];
		vap = ifp->if_softc;
	} else {
		ifp = g_wlan_ifnet[1];
		vap = ifp->if_softc;
	}

	if (ifp == NULL) {
		ipc_free(dev, payload);
		return 0;
	}

	m = m_getexthdr(M_NOWAIT, MT_DATA);
	if (m == NULL) {
		ipc_free(dev, payload);
		return 0;
	}

	m_dyna_extadd(m, (caddr_t)(payload->data), payload->size,
		mbuf_ipc_free, (u32)payload, (u32)dev, 0, EXT_IPCRING);
	m->m_data += WLAN_HDRLEN;
	m->m_len = *(uint16_t *)payload->data;

	m_fixhdr(m);

	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		const struct ether_header *eh = mtod(m, struct ether_header *);
		if (IEEE80211_IS_MULTICAST(eh->ether_dhost)) {
			m->m_flags |= M_MCAST;
		}
	}

	ifp->if_output(ifp, m, NULL, NULL);
	return 0;
}

void nuttxif_ipc_register(void)
{
	struct device *ipc_dev = device_get_by_name("scm2010-ipc");

	if(g_wlan_data_listener.cb != NULL  && g_wlan_data_listener.type != IPC_TYPE_UNUSED)
		return;

	g_wlan_data_listener.cb = nuttx_ipc_data_recvd;
	g_wlan_data_listener.type = IPC_TYPE_REQUEST;
	g_wlan_data_listener.priv = ipc_dev;
	if (ipc_addcb(ipc_dev, IPC_MODULE_WLAN, IPC_CHAN_DATA, &g_wlan_data_listener))
		printk("%s not success\n", __func__);

	if(nuttx_ipc_init(ipc_dev))
		assert(0);
}

__ilm__
void nuttxif_input(struct ifnet *ifp, struct mbuf *m)
{
	const struct ether_header *eh = NULL;
	int ret;

	eh = mtod(m, struct ether_header *);
	if (eh->ether_type == htons(ETHERTYPE_PAE)) {
		ifp->if_etherinput(ifp, m);
		return;
	}

#ifdef CONFIG_NET_FILTER
	/* PreCheck Ethernet type for NuttX[wlan_ipc_data_recvd] */
	if ((eh->ether_type != htons(ETHERTYPE_IP)) &&
		(eh->ether_type != htons(ETHERTYPE_IPV6)) &&
		(eh->ether_type != htons(ETHERTYPE_ARP))) {
		m_freem(m);
		return;
	}
#endif

	ret = ifq_enqueue(&g_nx_rx.rx_queue, m);
	assert(ret == 0);

	nuttx_rx_runtask();

}

void
nuttx_ifattach(struct ifnet *ifp)
{
	/* replace ether_input */
	ifp->if_etherinput = ifp->if_input;
	ifp->if_input = nuttxif_input;

	if (!strcmp("wlan0", ifp->if_xname)) {
		g_wlan_ifnet[0] = ifp;
	} else if (!strcmp("wlan1", ifp->if_xname)) {
		g_wlan_ifnet[1] = ifp;
	}

	nuttxif_ipc_register();

#ifdef CONFIG_AT_OVER_IPC
	extern void at_ipc_register(void);
	at_ipc_register();
#endif
}

void
nuttx_ifdetach(struct ifnet *ifp)
{
	struct device *ipc_dev = device_get_by_name("scm2010-ipc");

	if (ipc_delcb(ipc_dev, IPC_MODULE_WLAN, IPC_CHAN_DATA, &g_wlan_data_listener))
		printk("%s not success\n", __func__);

	/* restore ether_input */
	ifp->if_input = ifp->if_etherinput;

	if (ifp == g_wlan_ifnet[0]) {
		g_wlan_ifnet[0] = NULL;
	} else if (ifp == g_wlan_ifnet[1]) {
		g_wlan_ifnet[1] = NULL;
	}
}

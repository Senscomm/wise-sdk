/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <lwip/prot/ip.h>
#include <lwip/prot/ip4.h>
#include <lwip/prot/ip6.h>
#include <lwip/prot/udp.h>
#include <lwip/prot/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <hal/kernel.h>
#include <hal/console.h>
#include <hal/wlan.h>
#include <hal/kmem.h>
#include "mbuf.h"
#include "kernel.h"
#include "malloc.h"
#include "mutex.h"
#include <sys/types.h>
#include "compat_if.h"
#include "if_media.h"
#include <net80211/ieee80211_var.h>
#include "wifi_repeater.h"


#define CONFIG_REPEATER_IF_NUM	1
#define IPv4BUF ((const struct ip_hdr *)mtodo(m, ETHER_HDR_LEN))
#define UDPIPv4BUF(iphdr) ((const struct udp_hdr *)mtodo(m, (ETHER_HDR_LEN + (iphdr))))
#define TCPIPv4BUF(iphdr) ((const struct tcp_hdr *)mtodo(m, (ETHER_HDR_LEN + (iphdr))))

#define swap_portnum(x) ((((x)&0xff00) >> 8) | (((x)&0x00ff) << 8))
#define swap_ipaddr(x) ((((x)&0xff000000) >> 24) | (((x)&0x00ff0000) >> 8) | (((x)&0x0000ff00) << 8) | (((x)&0x000000ff) << 24))

// Log Level 1: Error
#define REPEATER_LOG1               printk

#if CONFIG_WIFI_REPEATER_DEBUG >= 2
#define REPEATER_LOG2             REPEATER_LOG1
#else
#define REPEATER_LOG2(...)
#endif

#if CONFIG_WIFI_REPEATER_DEBUG >= 3
#define REPEATER_LOG3             REPEATER_LOG1
#else
#define REPEATER_LOG3(...)
#define wifi_repeater_dump_pkt(m)
#endif


#if CONFIG_LWIP_IPV6
#define CONFIG_SUPPORT_REPEATER_IPV6
#endif

#define REPEATER_IS_HOST_CARRIER_ON() repeater_ctx.host_carrier_on
typedef void (*interface_inputpbuf)(struct ifnet *ifp, struct pbuf *p);

extern void ether_inputpbuf(struct ifnet *ifp, struct pbuf *p);

struct wifi_filter_config {
	struct ifnet *ifp;
	interface_input host_input;	/* input routine to host */
	interface_inputpbuf lwip_input;	/* input routine to wise lwip */
	wifi_packet_filter default_dir;
	int max_ipv4_num;
	int ipv4_used;
	struct wifi_ipv4_filter *ipv4_filters;
#ifdef CONFIG_SUPPORT_REPEATER_IPV6
	int max_ipv6_num;
	int ipv6_used;
	struct wifi_ipv6_filter *ipv6_filters;
#endif
};

typedef struct {
	uint32_t host_carrier_on;
	struct wifi_filter_config filter_cfg[CONFIG_REPEATER_IF_NUM];
	struct repeater_ops *ops;
} repeater_ctx_t;

static int wifi_repeater_ops_init(wifi_repeater_id idx,
								  interface_input func);
static int wifi_repeater_ops_add_filter(wifi_repeater_id idx, char *filter,
										wifi_filter_type type);
static int wifi_repeater_ops_del_filter(wifi_repeater_id idx, char *filter,
										wifi_filter_type type);
static int wifi_repeater_ops_set_def_dir(wifi_repeater_id idx,
										wifi_packet_filter direction);
static int wifi_repeater_ops_query_filter(wifi_repeater_id idx,
										char **filter, int *num,
										wifi_filter_type type);
static int wifi_repeater_ops_deinit(wifi_repeater_id idx);
static int wifi_repeater_ops_free_filters(wifi_repeater_id idx,
										wifi_filter_type type);
static int wifi_repeater_ops_host_carrier_getset(uint32_t set, uint32_t *on);

static struct repeater_ops repeater_ops = {
	.init = wifi_repeater_ops_init,
	.add_filter = wifi_repeater_ops_add_filter,
	.del_filter = wifi_repeater_ops_del_filter,
	.set_default_dir = wifi_repeater_ops_set_def_dir,
	.query_filter = wifi_repeater_ops_query_filter,
	.free_filters = wifi_repeater_ops_free_filters,
	.deinit = wifi_repeater_ops_deinit,
	.host_carrier_getset = wifi_repeater_ops_host_carrier_getset,
};

repeater_ctx_t repeater_ctx = {
	.ops = &repeater_ops,
};

#define REPEATER_GET_IPV4_FILTERS(idx) repeater_ctx.filter_cfg[idx].ipv4_filters
#ifdef CONFIG_SUPPORT_REPEATER_IPV6
#define REPEATER_GET_IPV6_FILTERS(idx) repeater_ctx.filter_cfg[idx].ipv6_filters
#else
#define REPEATER_GET_IPV6_FILTERS(idx) 0
#endif
static mutex filter_mtx;

static void filter_mtx_init(void)
{
	mtx_init(&filter_mtx, NULL, NULL, 0);
}

static void filter_mtx_lock(void)
{
	mtx_lock(&filter_mtx);
}

static void filter_mtx_unlock(void)
{
	mtx_unlock(&filter_mtx);
}

static void filter_mtx_free(void)
{
	mtx_destroy(&filter_mtx);
}


static int wifi_repeater_ops_set_def_dir(wifi_repeater_id idx,
										 wifi_packet_filter direction)
{
	if (idx >= CONFIG_REPEATER_IF_NUM)
		return FAIL;

	repeater_ctx.filter_cfg[idx].default_dir = direction;
	return OK;
}

static int is_ipv4_filter_duplicate(struct wifi_filter_config *cfg,
								struct wifi_ipv4_filter *new_filter)
{
	struct wifi_ipv4_filter *wlan_filter;
	int i = 0, duplicate = 0;;

	filter_mtx_lock();

	for (i = 0; i < cfg->ipv4_used; i++) {
		wlan_filter = &cfg->ipv4_filters[i];
		if (memcmp(wlan_filter, new_filter, sizeof(struct wifi_ipv4_filter)) == 0) {
			duplicate = true;
			break;
		}
	}

	filter_mtx_unlock();

	return (duplicate ? FAIL : OK);
}

static int wifi_adddel_ipv4_filter_check_param(struct wifi_ipv4_filter *filter)
{
	/* Check the Parameter dependency */


	/* port 0 is reserved and should not be used, so we consider port 0 is invalid here */
	if ((filter->match_mask & WIFI_FILTER_MASK_LOCAL_PORT) && !filter->local_port) {
		REPEATER_LOG1("L-port filter dependency\n");
		return FAIL;
	}
	if (filter->match_mask & WIFI_FILTER_MASK_REMOTE_PORT && !filter->remote_port) {
		REPEATER_LOG1("R-port filter dependency\n");
		return FAIL;
	}
	if ((filter->match_mask & WIFI_FILTER_MASK_LOCAL_PORT_RANGE) && (filter->localp_min >= filter->localp_max)) {
		REPEATER_LOG1("L-port range filter dependency\n");
		return FAIL;
	}
	if ((filter->match_mask & WIFI_FILTER_MASK_REMOTE_PORT_RANGE) && (filter->remotep_min >= filter->remotep_max)) {
		REPEATER_LOG1("R-port range filter dependency\n");
		return FAIL;
	}

	return OK;
}

static int wifi_add_ipv4_filter(struct wifi_filter_config *cfg,
								struct wifi_ipv4_filter *new_filter)
{
	struct wifi_ipv4_filter *wlan_filter;

	if (cfg == NULL || new_filter == NULL
		|| cfg->ipv4_used >= cfg->max_ipv4_num) {
		REPEATER_LOG1("Add Failed (Filter is full)...\n");
		return FAIL;
	}

	if (wifi_adddel_ipv4_filter_check_param(new_filter) != OK) {
		return FAIL;
	}

	if (is_ipv4_filter_duplicate(cfg, new_filter)){
		return FAIL;
	}

	filter_mtx_lock();

	wlan_filter = &cfg->ipv4_filters[cfg->ipv4_used];
	memcpy(wlan_filter, new_filter, sizeof(*wlan_filter));
	cfg->ipv4_used++;

	filter_mtx_unlock();

	return OK;
}

/* Adds the filtering rules to the forwarding table of the repeater. */
static int wifi_repeater_ops_add_filter(wifi_repeater_id idx, char *filter,
										wifi_filter_type type)
{
	struct wifi_filter_config *cfg = &repeater_ctx.filter_cfg[idx];

	if (idx >= CONFIG_REPEATER_IF_NUM)
		return FAIL;

	if (type == WIFI_FILTER_TYPE_IPV4 && REPEATER_GET_IPV4_FILTERS(idx))
		return wifi_add_ipv4_filter(cfg,
									(struct wifi_ipv4_filter *) filter);
	else
		return FAIL;

}

static int wifi_del_ipv4_filter(struct wifi_filter_config *cfg,
								struct wifi_ipv4_filter *del_filter)
{
	struct wifi_ipv4_filter *wlan_filter;
	struct wifi_ipv4_filter *filter_table = cfg->ipv4_filters;
	int i;

	if (cfg == NULL || del_filter == NULL)
		return FAIL;

	if (wifi_adddel_ipv4_filter_check_param(del_filter) != OK) {
		return FAIL;
	}

	for (i = 0; i < cfg->ipv4_used; i++) {
		wlan_filter = &cfg->ipv4_filters[i];

		if (memcmp(wlan_filter, del_filter, sizeof(struct wifi_ipv4_filter)) == 0) {
			filter_mtx_lock();

			memmove(&filter_table[i],
					&filter_table[i + 1],
					(cfg->ipv4_used - i - 1) * sizeof(struct wifi_ipv4_filter));

			cfg->ipv4_used--;
			filter_mtx_unlock();

			return OK;
		}
	}

	REPEATER_LOG1("Can't found matching Filter\n");
	return FAIL;


}

static int wifi_repeater_ops_del_filter(wifi_repeater_id idx, char *filter,
										wifi_filter_type type)
{
	struct wifi_filter_config *cfg = &repeater_ctx.filter_cfg[idx];

	if (idx >= CONFIG_REPEATER_IF_NUM)
		return FAIL;

	if (type == WIFI_FILTER_TYPE_IPV4 && REPEATER_GET_IPV4_FILTERS(idx))
		return wifi_del_ipv4_filter(cfg,
									(struct wifi_ipv4_filter *) filter);
	else
		return FAIL;

	return OK;
}

static int wifi_repeater_ops_query_filter(wifi_repeater_id idx,
										  char **filter, int *num,
										  wifi_filter_type type)
{
	struct wifi_filter_config *cfg = &repeater_ctx.filter_cfg[idx];

	if (idx >= CONFIG_REPEATER_IF_NUM)
		return FAIL;

	if (type == WIFI_FILTER_TYPE_IPV4 && REPEATER_GET_IPV4_FILTERS(idx)) {
		*num = cfg->ipv4_used;
		*filter = (char *) cfg->ipv4_filters;
	} else
		return FAIL;

	return OK;
}

static void repeater_if_deattach(struct wifi_filter_config *cfg)
{
	struct ifnet *ifp = cfg->ifp;

	ifp->if_input = ifp->if_etherinput;

	cfg->host_input = NULL;
	cfg->lwip_input = NULL;
}

static int wifi_repeater_ops_free_filters(wifi_repeater_id idx, wifi_filter_type type)
{
	struct wifi_filter_config *cfg = &repeater_ctx.filter_cfg[idx];

	if (type == WIFI_FILTER_TYPE_IPV4) {
		if (cfg->ipv4_filters == NULL)
			return FAIL;
		filter_mtx_lock();
		cfg->ipv4_used = 0;
		memset(cfg->ipv4_filters, 0, sizeof(struct wifi_ipv4_filter) * cfg->max_ipv4_num);
		filter_mtx_unlock();
	} else {
		return FAIL;
	}

	return OK;
}

static int wifi_repeater_ops_host_carrier_getset(uint32_t set, uint32_t *on)
{
	if (set) {
		if (*on)
			repeater_ctx.host_carrier_on = true;
		else
			repeater_ctx.host_carrier_on = false;
	} else {
		*on = repeater_ctx.host_carrier_on;
	}

	return OK;
}

static int wifi_repeater_ops_deinit(wifi_repeater_id idx)
{
	struct wifi_filter_config *cfg = &repeater_ctx.filter_cfg[idx];

	repeater_if_deattach(cfg);
	filter_mtx_lock();
	if (cfg->ipv4_filters)
		kfree(cfg->ipv4_filters);
#ifdef CONFIG_SUPPORT_REPEATER_IPV6
	if (cfg->ipv6_filters)
		kfree(cfg->ipv6_filters);
#endif
	filter_mtx_unlock();
	memset(cfg, 0, sizeof(struct wifi_filter_config));
	filter_mtx_free();

	return OK;
}

static int wifi_ipv4_filter_init(struct wifi_filter_config *cfg)
{
	struct wifi_ipv4_filter *wlan_filters;

	cfg->max_ipv4_num = CONFIG_SUPPORT_WIFI_REPEATER_IPV4_CNT;
	wlan_filters =
		kzalloc(sizeof(struct wifi_ipv4_filter) * cfg->max_ipv4_num);
	if (!wlan_filters) {
		REPEATER_LOG1("%s filter table allocation failed\n", __func__);
		return FAIL;
	}

	cfg->ipv4_filters = wlan_filters;
	filter_mtx_init();
	cfg->ipv4_used = 0;

	return OK;
}

bool match_ip(struct wifi_ipv4_filter *filter, const struct ip_hdr *ipv4,
			  const struct tcp_hdr *tcp, const struct udp_hdr *udp,
			  int index, int len)
{
	unsigned long srcipaddr = ipv4->src.addr;
	if (filter->remote_ip == swap_ipaddr(srcipaddr)) {
		REPEATER_LOG2
			("Matching Found Reg(%d) by src ip(0x%08x), len(%d)\n", index,
			 filter->remote_ip, len);
		return true;
	}
	return false;
}

bool match_protocol(struct wifi_ipv4_filter *filter,
					const struct ip_hdr *ipv4, const struct tcp_hdr *tcp,
					const struct udp_hdr *udp, int index, int len)
{
	if (filter->packet_type == ipv4->_proto) {
		REPEATER_LOG2
			("Matching Found Reg(%d) by protocol(%d), len(%d)\n", index,
			 filter->packet_type, len);
		return true;
	}
	return false;
}

bool match_local_port_and_range(struct wifi_ipv4_filter *filter,
								const struct ip_hdr *ipv4,
								const struct tcp_hdr *tcp,
								const struct udp_hdr *udp, int index,
								int len)
{
	uint16_t port_number;
	bool matched = false;

	if (udp) {
		port_number = swap_portnum(udp->dest);
	} else if (tcp) {
		port_number = swap_portnum(tcp->dest);
	} else {
		return false;
	}

	if (filter->local_port == port_number) {
		REPEATER_LOG2
			("Matching Found Reg(%d) by dest port(%d), len(%d)\n", index,
			 filter->local_port, len);
		matched = true;
	}

	/*
	 * if both the source port number and source port number range are specified in a rule, the
	 * Repeater preferentially compares the source port numbers.
	 *      1) If the source port numbers do not match, the source port number ranges are compared.
	 *       2) If the source port numbers match, the source port number ranges are also considered to match.
	 */
	if ((filter->match_mask & WIFI_FILTER_MASK_LOCAL_PORT_RANGE) &&
		(filter->localp_min <= port_number) &&
		(port_number <= filter->localp_max)) {
		REPEATER_LOG2
			("Matching Found Reg(%d) UDP by dest port range(%d ~ %d), len(%d)\n",
			 index, filter->localp_min, filter->localp_min, len);
		matched = true;
	}
	return matched;
}

bool match_remote_port_and_range(struct wifi_ipv4_filter *filter,
								 const struct ip_hdr *ipv4,
								 const struct tcp_hdr *tcp,
								 const struct udp_hdr *udp, int index,
								 int len)
{
	uint16_t port_number;
	bool matched = false;

	if (udp) {
		port_number = swap_portnum(udp->src);
	} else if (tcp) {
		port_number = swap_portnum(tcp->src);
	} else {
		return false;
	}

	if (filter->remote_port == port_number) {
		REPEATER_LOG2
			("Matching Found Reg(%d) by src port(%d), len(%d)\n", index,
			 filter->remote_port, len);
		matched = true;
	}

	/*
	 * if both the dest port number and dest port number range are specified in a rule, the
	 * Repeater preferentially compares the dest port numbers.
	 *      1) If the dest port numbers do not match, the dest port number ranges are compared.
	 *      2) If the dest port numbers match, the dest port number ranges are also considered to match.
	 */

	if ((filter->match_mask & WIFI_FILTER_MASK_REMOTE_PORT_RANGE) &&
		(filter->remotep_min <= port_number) &&
		(port_number <= filter->remotep_max)) {
		REPEATER_LOG2
			("Matching Found Reg(%d) by src port(%d ~ %d), len(%d)\n",
			 index, filter->remotep_min, filter->remotep_min, len);
		return true;
	}
	return matched;
}


bool match_remote_port_range(struct wifi_ipv4_filter *filter,
							 const struct ip_hdr *ipv4,
							 const struct tcp_hdr *tcp,
							 const struct udp_hdr *udp, int index, int len)
{
	uint16_t port_number;
	if (udp) {
		port_number = swap_portnum(udp->src);
	} else if (tcp) {
		port_number = swap_portnum(tcp->src);
	} else {
		return false;
	}

	if ((filter->remotep_min <= port_number) &&
		(port_number <= filter->remotep_max)) {
		REPEATER_LOG2
			("Matching Found Reg(%d) by src port(%d ~ %d), len(%d)\n",
			 index, filter->remotep_min, filter->remotep_min, len);
		return true;
	}
	return false;
}

typedef bool (*match_func)(struct wifi_ipv4_filter *,
						   const struct ip_hdr *, const struct tcp_hdr *,
						   const struct udp_hdr *, int, int);

match_func match_funcs[] = {
	[0] = match_ip,				//  WIFI_FILTER_MASK_IP
	[1] = match_protocol,		//  WIFI_FILTER_MASK_PROTOCOL
	[2] = match_local_port_and_range,	//  WIFI_FILTER_MASK_LOCAL_PORT
	[3] = match_local_port_and_range,	//  WIFI_FILTER_MASK_LOCAL_PORT_RANGE
	[4] = match_remote_port_and_range,	// WIFI_FILTER_MASK_REMOTE_PORT
	[5] = match_remote_port_and_range,	// WIFI_FILTER_MASK_REMOTE_PORT_RANGE
};

static int ipv4_match_filter(struct wifi_filter_config *cfg,
											struct mbuf *m)
{
	struct wifi_ipv4_filter *wlan_filter;
	int index;
	uint16_t iphdr_hlen;
	const struct tcp_hdr *tcp = NULL;
	const struct udp_hdr *udp = NULL;
	const struct ip_hdr *ipv4 = IPv4BUF;
	int len = m->m_len;
	uint8_t mask_bit;

	iphdr_hlen = IPH_HL_BYTES(ipv4);

	if (ipv4->_proto == IP_PROTO_TCP)
		tcp = TCPIPv4BUF(iphdr_hlen);
	else if (ipv4->_proto == IP_PROTO_UDP)
		udp = UDPIPv4BUF(iphdr_hlen);

	for (index = 0; index < cfg->ipv4_used; index++) {
		wlan_filter = &cfg->ipv4_filters[index];
		uint8_t config_type = wlan_filter->config_type;
		bool match_result = true;

		for (mask_bit = 0; mask_bit < ARRAY_SIZE(match_funcs); mask_bit++) {
			if (wlan_filter->match_mask & (1 << mask_bit)) {
				match_func func = match_funcs[mask_bit];
				if (func) {
					match_result =
						func(wlan_filter, ipv4, tcp, udp, index, len);
					if (!match_result)
						break;
				}
			}
		}
		if (match_result)
			return config_type;
	}

	return WIFI_FILTER_TO_BUTT;
}

static int match_filter(struct wifi_filter_config *cfg, struct mbuf *m)
{
	if (cfg->ipv4_used > 0)
		return ipv4_match_filter(cfg, m);
	else
		return WIFI_FILTER_TO_BUTT;
}

#if CONFIG_WIFI_REPEATER_DEBUG >= 3
void wifi_repeater_dump_pkt(struct mbuf *m)
{
	const struct ether_header *eh = NULL;
	const struct ip_hdr *ipv4 = NULL;
	const struct tcp_hdr *tcp = NULL;
	const struct udp_hdr *udp = NULL;
	uint16_t iphdr_hlen;
	uint16_t dst_port = 0, src_port = 0;

	eh = mtod(m, struct ether_header *);
	ipv4 = IPv4BUF;
	if (ipv4) {
		iphdr_hlen = IPH_HL_BYTES(ipv4);
		if (ipv4->_proto == IP_PROTO_TCP) {
			tcp = TCPIPv4BUF(iphdr_hlen);
			dst_port = swap_portnum(tcp->dest);
			src_port = swap_portnum(tcp->src);
		} else if (ipv4->_proto == IP_PROTO_UDP) {
			udp = UDPIPv4BUF(iphdr_hlen);
			dst_port = swap_portnum(udp->dest);
			src_port = swap_portnum(udp->src);
		}
	}

	REPEATER_LOG3
		("pkt: ether_type(0x%4x), proto(%d) tcp(%d) udp(%d) dst_port(%d) src_port(%d)\n",
		 eh->ether_type, ipv4->_proto, tcp ? 1 : 0, udp ? 1 : 0, dst_port,
		 src_port);

}
#endif

static void repeater_forward_packet(struct wifi_filter_config *cfg, wifi_packet_filter direction,struct mbuf *m)
{
	struct pbuf *p;
	struct ifnet *ifp = cfg->ifp;

	if (direction == WIFI_FILTER_TO_BUTT) {
		REPEATER_LOG2("ret_config_type(%d) Data To default_indicate(%d)\n",
			   cfg->default_dir, m->m_len);
		direction = cfg->default_dir;
	}

	if (direction == WIFI_FILTER_TO_HOST) {
		REPEATER_LOG2("Data To HOST(%d)\n", m->m_len);
		cfg->host_input(ifp, m);
		return;
	} else if (direction == WIFI_FILTER_TO_LWIP) {
		REPEATER_LOG2("Data To LWIP(%d)\n", m->m_len);
		/* m_topbuf will free the mbuf */
		p = m_topbuf(m);
		cfg->lwip_input(ifp, p);
		return;
	} else { /* Both/Invalid: WIFI_FILTER_TO_BOTH */
		REPEATER_LOG2("Data To BOTH(%d)\n", m->m_len);
		/* m_dup here is redundant, directly copy to pbuf
		 * host_input will free the mbuf */
		p = m_topbuf_nofreem(m);
		cfg->host_input(ifp, m);

		if (p) {
			cfg->lwip_input(ifp, p);
		}
	}
}

static void repeater_input(struct ifnet *ifp, struct mbuf *m)
{
	wifi_packet_filter direction = WIFI_FILTER_TO_LWIP;
	struct wifi_filter_config *cfg =
		(repeater_ctx.filter_cfg[0].ifp ==
		 ifp ? &repeater_ctx.filter_cfg[0] : &repeater_ctx.filter_cfg[1]);

	assert(cfg->lwip_input);
	assert(cfg->host_input);
	assert(m);

	wifi_repeater_dump_pkt(m);

	/* host not ready: to lwip */
	if (!REPEATER_IS_HOST_CARRIER_ON()) {
		direction = WIFI_FILTER_TO_LWIP;
		goto forward_pkt;
	}

#ifdef CONFIG_SUPPORT_REPEATER_CMD
	extern int gTxDataDir;
	if (gTxDataDir) {
		direction = gTxDataDir;
		goto forward_pkt;
	}
#endif

	/* 1. PAE or DHCP or ARP Packet is tossed to lwip net stack */
	const struct ether_header *eh = NULL;
	eh = mtod(m, struct ether_header *);

	/* Default filter rule */
	if (eh->ether_type == htons(ETHERTYPE_PAE)
		|| eh->ether_type == htons(ETHERTYPE_IPV6)) {
		direction = WIFI_FILTER_TO_LWIP;
		goto forward_pkt;
	}

	/*
	 * ICMP, IGMP, ARP to both side
	 */
	const struct ip_hdr *ipv4 = IPv4BUF;
	if (eh->ether_type == htons(ETHERTYPE_ARP) ||
		(eh->ether_type == htons(ETHERTYPE_IP) &&
		 (ipv4->_proto == IP_PROTO_ICMP
		  || ipv4->_proto == IP_PROTO_IGMP))) {

		direction = WIFI_FILTER_TO_BOTH;
		goto forward_pkt;
	}

	/* In case of IP Packet, Check the registration packet filter */
	if (eh->ether_type == htons(ETHERTYPE_IP)) {

		/* Check Filter & forwarding rule of Table */
		direction = match_filter(cfg, m);
		goto forward_pkt;
	}

forward_pkt:
	repeater_forward_packet(cfg, direction, m);

}

void repeater_if_attach(struct wifi_filter_config *cfg,
						interface_input func)
{
	struct ifnet *ifp = cfg->ifp;

	ifp->if_etherinput = ifp->if_input;
	ifp->if_input = repeater_input;
	cfg->host_input = func;
	cfg->lwip_input = ether_inputpbuf;
}

static int repeater_set_ifp(int idx, struct wifi_filter_config *cfg)
{
	struct device *dev = wlandev(0);
	struct ieee80211vap *vap;

	if (!dev || !cfg)
		return FAIL;

	vap = wlan_get_vap(dev, idx);

	if (vap)
		cfg->ifp = vap->iv_ifp;

	if (!cfg->ifp)
		return FAIL;
	else
		return OK;
}

static int wifi_repeater_ops_init(wifi_repeater_id idx,
								  interface_input func)
{
	int ret = OK;
	struct wifi_filter_config *cfg = &repeater_ctx.filter_cfg[idx];

	if (idx >= CONFIG_REPEATER_IF_NUM) {
		REPEATER_LOG1("Error: not support wlan1 repeater \n");
		return FAIL;
	}

	if (REPEATER_GET_IPV4_FILTERS(idx) || REPEATER_GET_IPV6_FILTERS(idx)) {
		REPEATER_LOG1("already init\n");
		return FAIL;
	}


	if (!func)
		return FAIL;

	memset(cfg, 0, sizeof(struct wifi_filter_config));

	if (repeater_set_ifp(idx, cfg) < 0)
		return FAIL;

	repeater_if_attach(cfg, func);

	cfg->default_dir = WIFI_FILTER_TO_LWIP;

	if (wifi_ipv4_filter_init(cfg))
		return FAIL;

#if CONFIG_SUPPORT_REPEATER_IPV6
	if (wifi_ipv6_filter_init(cfg))
		return FAIL;
#endif

	return ret;
}

struct repeater_ops *get_repeater_ops(void)
{
	return repeater_ctx.ops;
}

#ifdef CONFIG_SUPPORT_REPEATER_CMD
#include <cli.h>

int gTxDataDir = 0;

/* "repeater" "dir" "1/2/3" (1:lwip/2:sdio/3:both) */
static int do_repeater_dir(int argc, char *argv[])
{
	if (argc != 2) {
		printf("\nUsage: repeater dir [1|2|3]\n1 (lwip), 2 (sdio), or 3 (both)\n");
		return CMD_RET_USAGE;
	}

	int dir = atoi(argv[1]);
	if (dir < 1 || dir > 3) {
		printf("\nInvalid parameter: 1 (lwip), 2 (sdio), or 3 (both)\n");
		return CMD_RET_FAILURE;
	}

	gTxDataDir = dir;

	return CMD_RET_SUCCESS;
}

/* Show Current filtering forwarding table of the repeater. */
static void repeater_show_filter_cmd(void)
{
	struct wifi_ipv4_filter *wlan_filter;
	int filter_index;
	int num;

	wifi_repeater_query_filter(WIFI_REPEATER_WLAN0, (char **) &wlan_filter,
							   &num, WIFI_FILTER_TYPE_IPV4);

	REPEATER_LOG1("\n Allow filter count %d default_dir %d\n",
		   CONFIG_SUPPORT_WIFI_REPEATER_IPV4_CNT,
		   repeater_ctx.filter_cfg[0].default_dir);
	REPEATER_LOG1("\n----- Total Filter Count %d -----\n", num);

	for (filter_index = 0; filter_index < num; filter_index++) {
		REPEATER_LOG1
			("[%d] protocol(%d) dest port(%d) config_type(%d) match_mask(0x%x)\n",
			 filter_index, wlan_filter->packet_type,
			 wlan_filter->local_port, wlan_filter->config_type,
			 wlan_filter->match_mask);
		wlan_filter++;
	}
}

/* add : "repeater" "filter" "add" protocol port_num to(1:lwip/2:sdio/3:both) OR*/
/* del : "repeater" "filter" "del" protocol port_num to(1:lwip/2:sdio/3:both) OR*/
static int do_repeater_filter(int argc, char *argv[])
{
	struct wifi_ipv4_filter filter = { 0 };
	u8 protocol = 0;
	uint16_t port_num = 0;
	u8 direction = 0;

	if (argc == 5) {
		protocol = atoi(argv[2]);
		port_num = atoi(argv[3]);
		direction = atoi(argv[4]);
	}

	if (!strcmp(argv[1], "add")) {
		filter.local_port = port_num;
		filter.packet_type = protocol;
		filter.match_mask =
			WIFI_FILTER_MASK_LOCAL_PORT | WIFI_FILTER_MASK_PROTOCOL;
		filter.config_type = direction;
		wifi_repeater_add_filter(WIFI_REPEATER_WLAN0,
								 (char *) &filter, WIFI_FILTER_TYPE_IPV4);
	} else if (!strcmp(argv[1], "del")) {
		filter.local_port = port_num;
		filter.packet_type = protocol;
		filter.match_mask =
			WIFI_FILTER_MASK_LOCAL_PORT | WIFI_FILTER_MASK_PROTOCOL;
		filter.config_type = direction;
		wifi_repeater_del_filter(WIFI_REPEATER_WLAN0,
								 (char *) &filter, WIFI_FILTER_TYPE_IPV4);
	} else if (!strcmp(argv[1], "show")) {
		repeater_show_filter_cmd();
	} else {
		printf
			("\nUsage :sdio filter add/del protocol port_num to(1:lwip/2:sdio/3:both)\n");
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

/* interface : "repeater" "interface" "1" (1:attach/0:detach) */
static int do_repeater_interface(int argc, char *argv[])
{

	extern void scdc_input(struct ifnet *ifp, struct mbuf *m);
	struct wifi_filter_config *cfg =
		&repeater_ctx.filter_cfg[WIFI_REPEATER_WLAN0];

	if (argc != 2)
		return CMD_RET_FAILURE;

	if (atoi(argv[1]) == 1)
		repeater_if_attach(cfg, scdc_input);
	else if (atoi(argv[1]) == 0)
		repeater_if_deattach(cfg);
	else
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd repeater_cmd[] = {
	CMDENTRY(to, do_repeater_dir, "", ""),
	CMDENTRY(filter, do_repeater_filter, "", ""),
	CMDENTRY(interface, do_repeater_interface, "", ""),
};

static int do_repeater(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], repeater_cmd, ARRAY_SIZE(repeater_cmd));
	if (cmd == NULL) {
		return CMD_RET_USAGE;
	}

	return cmd->handler(argc, argv);
}

CMD(repeater, do_repeater, "CLI comamands for repeater",
	"repeater to" OR "repeater filter");
#endif

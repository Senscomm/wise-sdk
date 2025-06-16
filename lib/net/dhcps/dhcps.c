/*
 * Copyright (c) 2018-2019 Senscomm, Inc. All rights reserved.
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
#include <hal/console.h>
#include <cmsis_os.h>
#include "lwip/opt.h"
#include "lwip/stats.h"
#include "lwip/mem.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/def.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include "lwip/prot/dhcp.h"
#include "lwip/prot/iana.h"
#include "dhcps.h"

#include <string.h>

/*
 * DHCP server implemented as being tightly coupled with
 * lwIP UDP, NETIF modules.
 * An alternative might be the one based on BSD socket APIs
 * without any reference to lwIP's internal structures and
 * functions.
 */

#define rx_option_given(dhcps, idx)          (dhcps->rx_options_given[idx] != 0)
#define rx_got_option(dhcps, idx)            (dhcps->rx_options_given[idx] = 1)
#define rx_clear_option(dhcps, idx)          (dhcps->rx_options_given[idx] = 0)
#define rx_clear_all_options(dhcps)          (memset(dhcps->rx_options_given, 0, \
						sizeof(dhcps->rx_options_given)))
#define rx_get_option_value(dhcps, idx)      (dhcps->rx_options_val[idx])
#define rx_set_option_value(dhcps, idx, val) (dhcps->rx_options_val[idx] = (val))
#define rx_clear_all_option_values(dhcps)    (memset(dhcps->rx_options_val, 0, \
						sizeof(dhcps->rx_options_val)))

#define option_req_en(dhcps, f) (dhcps->rx_options_val[DHCP_OPTION_IDX_PARAM_REQUEST_LIST] & \
							DHCP_OPTION_PARAM_REQ_##f##_FLAGS)

int dhcps_client_id = -1;
bool dhcps_started = false;

/** UDP PDB to be shared by all netif's */
static struct udp_pcb *dhcps_pcb;
/** track PCB ref count */
static u8_t dhcps_pcb_refcount;
static osTimerId_t dhcps_timer = (osTimerId_t)NULL;

#define netif_dhcps_data(netif) ((struct dhcps*)netif_get_client_data(netif, dhcps_client_id))

#define is_valid(s, c, e) (s <= c && c <= e)

/* receive, unfold, parse and free incoming messages */
static void dhcps_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);

/* mark on lease board */
static void do_lease(struct dhcps *dhcps, u32_t ip)
{
	u32_t bit;
	u32_t start_ip = lwip_htonl(ip_addr_get_ip4_u32(&dhcps->start_ip));

	bit = ip - start_ip;
	setbit(dhcps->boards, bit);
}

/* unmark on lease board */
static void cancel_lease(struct dhcps *dhcps, ip_addr_t ipaddr)
{
	u32_t bit;
	ip_addr_t *ipa = &ipaddr;
	u32_t ip = lwip_htonl(ip_addr_get_ip4_u32(ipa));
	u32_t start_ip = lwip_htonl(ip_addr_get_ip4_u32(&dhcps->start_ip));
	u32_t end_ip = lwip_htonl(ip_addr_get_ip4_u32(&dhcps->end_ip));

	if (!is_valid(start_ip, ip, end_ip))
		return;

	bit = ip - start_ip;
	clrbit(dhcps->boards, bit);
}

/* clear all lease board */
static void cancel_all_leases(struct dhcps *dhcps)
{
	memset(dhcps->boards, 0, sizeof(dhcps->boards));
}

/* get the first unmarked lease */
static ip_addr_t get_lease(struct dhcps *dhcps)
{
	int i;
	ip_addr_t ip_addr;
	u32_t ip;
	u32_t start_ip = lwip_htonl(ip_addr_get_ip4_u32(&dhcps->start_ip));
	u32_t end_ip = lwip_htonl(ip_addr_get_ip4_u32(&dhcps->end_ip));

	for (i = 0; i < LEASE_BOARDS_BYTES * NBBY; i++)
	       if (isclr(dhcps->boards, i))
		       break;
	ip = start_ip + i;
	if (is_valid(start_ip, ip, end_ip))
		do_lease(dhcps, ip);
	else
		ip = IPADDR_ANY;

	ip_addr_set_ip4_u32_val(ip_addr, lwip_htonl(ip));
	return ip_addr;
}

static bool is_offered(struct dhcps *dhcps, u8_t *hwaddr,
		struct dhcps_client **cl)
{
	struct dhcps_client *client;

	assert(cl);

	*cl = (struct dhcps_client *)NULL;
	list_for_each_entry(client, &dhcps->clist[OFFERED], list) {
		if (!memcmp(client->hwaddr, hwaddr, NETIF_MAX_HWADDR_LEN)) {
			*cl = client;
			return true;
		}
	}
	return false;
}

static void offer(struct dhcps *dhcps, u8_t *hwaddr, ip_addr_t ipaddr)
{
	struct dhcps_client *cl = (struct dhcps_client *)mem_malloc(sizeof(struct dhcps_client));
	if (cl == NULL)
		return;
	ip_addr_copy(cl->ipaddr, ipaddr);
	memcpy(cl->hwaddr, hwaddr, NETIF_MAX_HWADDR_LEN);
	cl->lease_time = dhcps->lease_time;
	list_add_tail(&cl->list, &dhcps->clist[OFFERED]);
}

static bool is_bound(struct dhcps *dhcps, u8_t *hwaddr,
		struct dhcps_client **cl)
{
	struct dhcps_client *client;

	assert(cl);

	*cl = (struct dhcps_client *)NULL;
	list_for_each_entry(client, &dhcps->clist[BOUND], list) {
		if (!memcmp(client->hwaddr, hwaddr, NETIF_MAX_HWADDR_LEN)) {
			*cl = client;
			return true;
		}
	}
	return false;
}

static bool is_ip_bound(struct dhcps *dhcps, ip_addr_t ip,
		struct dhcps_client **cl)
{
	struct dhcps_client *client;

	list_for_each_entry(client, &dhcps->clist[BOUND], list) {
		if (ip_addr_cmp(&client->ipaddr, &ip)) {
			*cl = client;
			return true;
		}
	}
	return false;
}

#ifndef __WISE__
static void bind(struct dhcps *dhcps, struct dhcps_client *cl)
#else /* Avoid name clash */
static void _bind(struct dhcps *dhcps, struct dhcps_client *cl)
#endif
{
	list_del(&cl->list); /* remove from offered */
	list_add_tail(&cl->list, &dhcps->clist[BOUND]);
}

static void forget(struct dhcps *dhcps, struct dhcps_client *cl)
{
	cancel_lease(dhcps, cl->ipaddr);
	list_del(&cl->list);
	mem_free(cl);
}

/** Ensure DHCPS PCB is allocated and bound */
static err_t
dhcps_inc_pcb_refcount(void)
{
	if (dhcps_pcb_refcount == 0) {
		KASSERT(dhcps_pcb == NULL,
				("dhcps_inc_pcb_refcount(): memory leak\n"));
		/* allocate UDP PCB */
		dhcps_pcb = udp_new();
		if (dhcps_pcb == NULL) {
			return ERR_MEM;
		}

		ip_set_option(dhcps_pcb, SOF_BROADCAST);

		/*
		 * set up local and remote port for the PCB
		 * -> listen on all interfaces on all src/dest IPs
		 */
		udp_bind(dhcps_pcb, IP4_ADDR_ANY, LWIP_IANA_PORT_DHCP_SERVER);
		udp_recv(dhcps_pcb, dhcps_recv, NULL);
	}

	dhcps_pcb_refcount++;

	return ERR_OK;
}

/** Free DHCPS PCB if the last netif stops using it */
static void
dhcps_dec_pcb_refcount(void)
{
	KASSERT(dhcps_pcb_refcount > 0,
			("dhcps_pcb_refcount(): refcount error\n"));
	dhcps_pcb_refcount--;
	if (dhcps_pcb_refcount == 0) {
		udp_disconnect(dhcps_pcb);
		udp_remove(dhcps_pcb);
		dhcps_pcb = NULL;
	}
}

/*
 * Set the DHCP state of a DHCP server.
 */
static void
dhcps_set_state(struct dhcps *dhcps, u8_t new_state) __maybe_unused;
static void
dhcps_set_state(struct dhcps *dhcps, u8_t new_state)
{
	if (new_state != dhcps->state)
		dhcps->state = new_state;
}

static void dhcps_do_coarse_tmr(struct netif *netif)
{
	struct dhcps *dhcps;
	struct dhcps_client *cl, *n;

	if (dhcps_client_id == -1
			|| (dhcps = netif_dhcps_data(netif)) == NULL) {
		return;
	}

	list_for_each_entry_safe(cl, n, &dhcps->clist[BOUND], list) {
		if (cl->lease_time > 0)
			cl->lease_time--;
		if (cl->lease_time == 0)
			forget(dhcps, cl);
	}
}

/**
 * Update lease time per each bound clients
 *
 * @param netif the netif from which to update clients
 */
static void dhcps_coarse_tmr(void *arg)
{
	netifapi_netif_commonv((struct netif *)arg, dhcps_do_coarse_tmr);
}


/**
 * Removes a struct dhcps from a netif.
 *
 * @param netif the netif from which to remove the struct dhcps
 */
void dhcps_cleanup(struct netif *netif)
{
	struct dhcps *data;

	LWIP_ASSERT("netif != NULL", netif != NULL);

	if (dhcps_client_id != -1) {
		data = netif_dhcps_data(netif);
		if (data) {
			mem_free(data);
			netif_set_client_data(netif, dhcps_client_id, NULL);
		}
		if (dhcps_timer != NULL) {
			osTimerDelete(dhcps_timer);
			dhcps_timer = (osTimerId_t)NULL;
		}
	}
}

static struct dhcps *
dhcps_create(struct netif *netif)
{
	struct dhcps *dhcps;

	if (dhcps_client_id == -1)
		dhcps_client_id = netif_alloc_client_data_id();

	if (dhcps_timer == NULL)
		dhcps_timer = osTimerNew(dhcps_coarse_tmr, osTimerPeriodic,
				netif, NULL);

	if ((dhcps = netif_dhcps_data(netif)) == NULL) {
		dhcps = (struct dhcps *)mem_malloc(sizeof(struct dhcps));
		if (dhcps == NULL) {
			LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING,
				("dhcps_start(): could not allocate dhcps\n"));
			return NULL;
		}
		/* clear data structure */
		memset(dhcps, 0, sizeof(struct dhcps));
		dhcps->lease_time = DHCPS_LEASE_TIME_DEF;
		INIT_LIST_HEAD(&dhcps->clist[OFFERED]);
		INIT_LIST_HEAD(&dhcps->clist[BOUND]);
	}
	/* store this dhcp server in the netif */
	netif_set_client_data(netif, dhcps_client_id, dhcps);
	return dhcps;
}
#define	msecs_to_ticks(ms)	(((ms)*1000)/osKernelGetTickFreq())
/**
 * Start DHCP server for a network interface.
 *
 * If no DHCP server instance was attached to this interface,
 * a new server is created first. If a DHCP server instance
 * was already present, it gets ready for incoming requests.
 *
 * @param netif The lwIP network interface
 * @return lwIP error code
 * - ERR_OK - No error
 * - ERR_MEM - Out of memory
 */
err_t dhcps_start(struct netif *netif)
{
	struct dhcps *dhcps;
	ip_addr_t nmask;
	u32_t server_ip = 0;
        u32_t local_ip = 0;
	u32_t start_ip = 0;
	u32_t end_ip = 0;

	LWIP_ASSERT("netif != NULL", netif != NULL);

#ifndef __WISE__
	if (!netif_is_link_up(netif)) {
		LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING,
			("dhcps_start(): netif is not up\n"));
		return ERR_IF;
	}
#endif

	dhcps = dhcps_create(netif);
	if (dhcps->pcb_allocated != 0)
		dhcps_dec_pcb_refcount(); /* free DHCP PCB if not needed any more */

	/* ensure DHCP PCB is allocated */
	if (dhcps_inc_pcb_refcount() != ERR_OK)
		return ERR_MEM;

	dhcps->pcb_allocated = 1;

#ifdef __WISE__
	if (ip_addr_isany_val(*netif_ip_addr4(netif)))
		return ERR_ARG;
#endif

	osTimerStart(dhcps_timer, msecs_to_ticks(DHCP_COARSE_TIMER_MSECS));


	ip_addr_copy(dhcps->server_ip, *netif_ip_addr4(netif));
	ip_addr_copy(dhcps->gw_ip, *netif_ip_gw4(netif));
	ip_addr_copy(nmask, *netif_ip_netmask4(netif));

	server_ip = lwip_htonl(ip_addr_get_ip4_u32(&dhcps->server_ip));
#ifdef __WISE__
	/* Every time dhcps_start, update dhcps IP with server_ip */
	(void)nmask;
	local_ip = server_ip;
	server_ip &= 0xFFFFFF00;
	local_ip &= 0xFF;
	if (local_ip >= 0x80)
		local_ip -= DHCPS_MAX_LEASE;
	else
		local_ip++;
	start_ip = (u32)lwip_htonl(server_ip | local_ip);
	end_ip = (u32)lwip_htonl((server_ip | local_ip) + DHCPS_MAX_LEASE - 1);
	ip_addr_set_ip4_u32(&dhcps->start_ip, start_ip);
	ip_addr_set_ip4_u32(&dhcps->end_ip, end_ip);
#else
	if (!ip_addr_isany_val(dhcps->start_ip)
				&& !ip_addr_isany_val(dhcps->end_ip)) {
		bool server_ip_valid = true;
		bool start_ip_valid = true;
		bool end_ip_valid = true;
		bool lease_range_valid = true;
		start_ip = lwip_htonl(ip_addr_get_ip4_u32(&dhcps->start_ip));
		end_ip = lwip_htonl(ip_addr_get_ip4_u32(&dhcps->end_ip));
		server_ip_valid = !!(server_ip < start_ip || end_ip < server_ip);
		start_ip_valid = !!ip_addr_netcmp(&dhcps->server_ip, &dhcps->start_ip, &nmask);
		end_ip_valid = !!ip_addr_netcmp(&dhcps->server_ip, &dhcps->end_ip, &nmask);
		lease_range_valid = !!(end_ip - start_ip <= DHCPS_MAX_LEASE);
		KASSERT(server_ip_valid, ("server ip invalid\n"));
		KASSERT(start_ip_valid, ("start addr invalid\n"));
		KASSERT(end_ip_valid, ("end addr invalid\n"));
		KASSERT(lease_range_valid, ("lease range invalid\n"));
		if (!(server_ip_valid
					&& start_ip_valid
					&& end_ip_valid
					&& lease_range_valid)) {
			ip_addr_set_any(0, &dhcps->start_ip);
			ip_addr_set_any(0, &dhcps->end_ip);
		}
	}

	if (ip_addr_isany_val(dhcps->start_ip)
			|| ip_addr_isany_val(dhcps->end_ip)) {
		local_ip = server_ip;
		server_ip &= 0xFFFFFF00;
		local_ip &= 0xFF;
		if (local_ip >= 0x80)
			local_ip -= DHCPS_MAX_LEASE;
		else
			local_ip++;
		start_ip = (u32)lwip_htonl(server_ip | local_ip);
		end_ip = (u32)lwip_htonl((server_ip | local_ip) + DHCPS_MAX_LEASE - 1);
		ip_addr_set_ip4_u32(&dhcps->start_ip, start_ip);
		ip_addr_set_ip4_u32(&dhcps->end_ip, end_ip);
	}
#endif
	dhcps_started = true;

	return ERR_OK;
}

/**
* Stop DHCP server for a network interface.
*
* If no DHCP server instance was attached to this interface,
* just return error.
*
* @param netif The lwIP network interface
* @return lwIP error code
* - ERR_OK - No error
* - ERR_IF - Invalid interface
*/
err_t dhcps_stop(struct netif *netif)
{
	struct dhcps *dhcps;

	LWIP_ASSERT("netif != NULL", netif != NULL);

	if (dhcps_client_id == -1
			|| (dhcps = netif_dhcps_data(netif)) == NULL)
		return ERR_IF;

	if (dhcps_started == false) {
		return ERR_IF;
	}
	dhcps_started = false;

	if (dhcps->pcb_allocated != 0) {
		/* free DHCP PCB if not needed any more */
		dhcps_dec_pcb_refcount();
		dhcps->pcb_allocated = 0;
	}

	if (osTimerIsRunning(dhcps_timer))
		osTimerStop(dhcps_timer);

	return ERR_OK;
}

/**
 * Configure DHCP server for a network interface.
 *
 * If no DHCP server instance was attached to this interface,
 * a new server is created first and then configured.
 *
 * @param netif The lwIP network interface
 * @return lwIP error code
 * - ERR_OK - No error
 * - ERR_MEM - Out of memory
 */
err_t dhcps_configure(struct netif *netif, struct dhcps_config *cfg)
{
	struct dhcps *dhcps;
	struct dhcps_client *cl, *n;

	LWIP_ASSERT("netif != NULL", netif != NULL);

	dhcps = dhcps_create(netif);

	/*
	 * Forget all of previously offered, bound clients
	 */
	cancel_all_leases(dhcps);

	list_for_each_entry_safe(cl, n, &dhcps->clist[OFFERED], list)
		forget(dhcps, cl);

	list_for_each_entry_safe(cl, n, &dhcps->clist[BOUND], list)
		forget(dhcps, cl);

	ip_addr_copy(dhcps->start_ip, cfg->start);
	ip_addr_copy(dhcps->end_ip, cfg->end);
	dhcps->lease_time = cfg->lease_to;

	return ERR_OK;
}

static u32_t dhcps_parse_param_request(u8_t *request_list, u8_t list_len)
{
	int i;
	u32_t req_val = 0;

	for (i = 0; i < list_len; i++) {
		switch (request_list[i]) {
			case DHCP_OPTION_SUBNET_MASK:
				req_val |= DHCP_OPTION_PARAM_REQ_SUBNET_MASK_FLAGS;
				break;
			case DHCP_OPTION_ROUTER:
				req_val |= DHCP_OPTION_PARAM_REQ_ROUTER_FLAGS;
				break;
			case DHCP_OPTION_DNS_SERVER:
				req_val |= DHCP_OPTION_PARAM_REQ_DNS_SERVER_FLAGS;
				break;
			case DHCP_OPTION_BROADCAST:
				req_val |= DHCP_OPTION_PARAM_REQ_BROADCAST_FLAGS;
				break;
		}
	}

	return req_val;
}

static err_t
dhcps_parse_request(struct dhcps *dhcps, struct dhcp_msg *msg, int len)
{
	u8_t *option, *end;
	u8_t opt_op;
	int opt_idx;
	u8_t opt_len;
	u32_t opt_val = 0;
	ip_addr_t ciaddr;

	rx_clear_all_options(dhcps);
	rx_clear_all_option_values(dhcps);

	if (msg->cookie != PP_HTONL(DHCP_MAGIC_COOKIE))
		return ERR_VAL;

	ip_addr_set_ip4_u32_val(ciaddr, msg->ciaddr.addr);
	ip_addr_copy(dhcps->client_ip, ciaddr);

	/* parse options */
	option = end = (u8_t *)msg;
	option += DHCP_OPTIONS_OFS;
	end += len;
	while (option < end) {
		opt_op = option[0];
		opt_len = option[1];
		switch (opt_op) {
			case DHCP_OPTION_MESSAGE_TYPE:
				opt_idx = DHCP_OPTION_IDX_MSG_TYPE;
				break;
			case DHCP_OPTION_REQUESTED_IP:
				opt_idx = DHCP_OPTION_IDX_REQUESTED_IP;
				break;
			case DHCP_OPTION_PARAMETER_REQUEST_LIST:
				opt_val = dhcps_parse_param_request(&option[2], opt_len);
				rx_got_option(dhcps, DHCP_OPTION_IDX_PARAM_REQUEST_LIST);
				rx_set_option_value(dhcps, DHCP_OPTION_IDX_PARAM_REQUEST_LIST, opt_val);
				opt_idx = -1;
				break;
			case DHCP_OPTION_END:
				opt_idx = -1;
				end = option;
				break;
			default:
				opt_idx = -1;
				break;
		}
		if (opt_idx >= 0 &&
				!rx_option_given(dhcps, opt_idx)) {
			memcpy(&opt_val, &option[2], opt_len);
			if (opt_len > 4) {
				/* FIXME: handle this */
			} else if (opt_len == 4) {
          			opt_val = lwip_htonl(opt_val);
			} else {
          			LWIP_ERROR("invalid decode_len",
						opt_len == 1, return ERR_VAL;);
				opt_val = ((u8_t *)&opt_val)[0];
			}
			rx_got_option(dhcps, opt_idx);
			rx_set_option_value(dhcps, opt_idx, opt_val);
		}
		option += (opt_len + 2);
	}

	return ERR_OK;
}

/*
 * Concatenate an option type and length field to the outgoing
 * DHCP message.
 *
 */
static u16_t
add_option(u16_t options_out_len, u8_t *options, u8_t option_type, u8_t option_len)
{
	LWIP_ASSERT("dhcps_option: options_out_len + 2 + option_len <= DHCP_OPTIONS_LEN",
			options_out_len + 2U + option_len <= DHCP_OPTIONS_LEN);
	options[options_out_len++] = option_type;
	options[options_out_len++] = option_len;
	return options_out_len;
}

/*
 * Concatenate a single byte to the outgoing DHCP message.
 *
 */
static u16_t
add_option_byte(u16_t options_out_len, u8_t *options, u8_t value)
{
	LWIP_ASSERT("dhcp_option_byte: options_out_len < DHCP_OPTIONS_LEN",
			options_out_len < DHCP_OPTIONS_LEN);
	options[options_out_len++] = value;
	return options_out_len;
}

static u16_t
add_option_short(u16_t options_out_len, u8_t *options, u16_t value) __maybe_unused;
static u16_t
add_option_short(u16_t options_out_len, u8_t *options, u16_t value)
{
	LWIP_ASSERT("dhcp_option_short: options_out_len + 2 <= DHCP_OPTIONS_LEN",
			options_out_len + 2U <= DHCP_OPTIONS_LEN);
	options[options_out_len++] = (u8_t)((value & 0xFF00U) >> 8);
	options[options_out_len++] = (u8_t) (value & 0x00FFU);
	return options_out_len;
}

static u16_t
add_option_long(u16_t options_out_len, u8_t *options, u32_t value)
{
	LWIP_ASSERT("dhcp_option_long: options_out_len + 4 <= DHCP_OPTIONS_LEN",
			options_out_len + 4U <= DHCP_OPTIONS_LEN);
	options[options_out_len++] = (u8_t)((value & 0xFF000000UL) >> 24);
	options[options_out_len++] = (u8_t)((value & 0x00FF0000UL) >> 16);
	options[options_out_len++] = (u8_t)((value & 0x0000FF00UL) >> 8);
	options[options_out_len++] = (u8_t)((value & 0x000000FFUL));
	return options_out_len;
}

/**
 * Create a DHCP reply, fill in common headers
 *
 * @param dhcps dhcps control struct
 * @param message_type message type of the request
 */
static struct pbuf *
create_msg(struct dhcps *dhcps, struct dhcp_msg *msg,
		u8_t message_type, u16_t *options_out_len)
{
	struct pbuf *p_out;
	struct dhcp_msg *msg_out;
	u16_t options_out_len_loc;


	p_out = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct dhcp_msg),
			PBUF_RAM);
	if (p_out == NULL) {
		LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
				("dhcp_create_msg(): could not allocate pbuf\n"));
		return NULL;
	}
  	LWIP_ASSERT("create_msg: check that first pbuf can hold struct dhcp_msg",
              (p_out->len >= sizeof(struct dhcp_msg)));

	msg_out = (struct dhcp_msg *)p_out->payload;
	memset(msg_out, 0, sizeof(*msg_out));
	/*
	 * msg contains original values sent from
	 * client and most of them will not be touched.
	 */
	memcpy(msg_out, msg, DHCP_OPTIONS_OFS);
	msg_out->op = DHCP_BOOTREPLY;
        msg_out->htype = LWIP_IANA_HWTYPE_ETHERNET;
	msg_out->hlen = NETIF_MAX_HWADDR_LEN;
	msg_out->hops = 0;
	msg_out->secs = 0;
	ip4_addr_set_zero(&msg_out->ciaddr);
	ip4_addr_copy(msg_out->yiaddr, *ip_2_ip4(&dhcps->your_ip));
	ip4_addr_copy(msg_out->siaddr, *ip_2_ip4(&dhcps->server_ip));
	ip4_addr_set_zero(&msg_out->giaddr);
	memset(msg_out->sname, 0, sizeof(msg_out->sname));
	memset(msg_out->file, 0, sizeof(msg_out->file));
	msg_out->cookie = PP_HTONL(DHCP_MAGIC_COOKIE);

	/* Add option MESSAGE_TYPE */
	options_out_len_loc = add_option(0, msg_out->options,
			DHCP_OPTION_MESSAGE_TYPE, DHCP_OPTION_MESSAGE_TYPE_LEN);
	options_out_len_loc = add_option_byte(options_out_len_loc,
			msg_out->options, message_type);
	if (options_out_len)
		*options_out_len = options_out_len_loc;

	return p_out;
}

/**
 * Add a DHCP message trailer
 *
 * Adds the END option to the DHCP message, and if
 * necessary, up to three padding bytes.
 */
static void
add_option_trailer(u16_t options_out_len, u8_t *options,
		struct pbuf *p_out)
{
	options[options_out_len++] = DHCP_OPTION_END;
	/* packet is too small, or not 4 byte aligned? */
	while (((options_out_len < DHCP_MIN_OPTIONS_LEN)
				|| (options_out_len & 3))
			&& (options_out_len < DHCP_OPTIONS_LEN)) {
		/* add a fill/padding byte */
		options[options_out_len++] = 0;
	}
	/* shrink the pbuf to the actual content length */
	pbuf_realloc(p_out, (u16_t)(sizeof(struct dhcp_msg)
				- DHCP_OPTIONS_LEN + options_out_len));
}

static u16_t
add_reply_options(struct dhcps *dhcps, struct dhcp_msg *msg,
		u16_t option_out_len)
{
	u16_t opt_out_len = option_out_len;
	opt_out_len = add_option(opt_out_len, msg->options,
			DHCP_OPTION_SERVER_ID, 4);
	opt_out_len = add_option_long(opt_out_len, msg->options,
			lwip_htonl(ip_addr_get_ip4_u32(&dhcps->server_ip)));
	opt_out_len = add_option(opt_out_len, msg->options,
			DHCP_OPTION_LEASE_TIME, 4);
	opt_out_len = add_option_long(opt_out_len, msg->options,
			dhcps->lease_time * 60);
	if (option_req_en(dhcps, SUBNET_MASK)) {
		opt_out_len = add_option(opt_out_len, msg->options,
				DHCP_OPTION_SUBNET_MASK, 4);
		opt_out_len = add_option_long(opt_out_len, msg->options,
				0xFFFFFF00UL);
	}
	if (option_req_en(dhcps, ROUTER) && !ip_addr_isany_val(dhcps->gw_ip)) {
		opt_out_len = add_option(opt_out_len, msg->options,
				DHCP_OPTION_ROUTER, 4);
		opt_out_len = add_option_long(opt_out_len, msg->options,
				lwip_htonl(ip_addr_get_ip4_u32(&dhcps->gw_ip)));
	}
	if (option_req_en(dhcps, DNS_SERVER)) {
		opt_out_len = add_option(opt_out_len, msg->options,
				DHCP_OPTION_DNS_SERVER, 4);
		if (!ip_addr_isany_val(dhcps->dns_ip)) {
			opt_out_len = add_option_long(opt_out_len, msg->options,
					lwip_htonl(ip_addr_get_ip4_u32(&dhcps->dns_ip)));
		} else {
			opt_out_len = add_option_long(opt_out_len, msg->options,
					lwip_htonl(ip_addr_get_ip4_u32(&dhcps->server_ip)));
		}
	}
	if (option_req_en(dhcps, BROADCAST)) {
		opt_out_len = add_option(opt_out_len, msg->options,
				DHCP_OPTION_BROADCAST, 4);
		opt_out_len = add_option_long(opt_out_len, msg->options,
				lwip_htonl(ip_addr_get_ip4_u32(&dhcps->server_ip))
					| 0x000000FFUL);
	}
	return opt_out_len;
}

static err_t
dhcps_offer(struct dhcps *dhcps, struct dhcp_msg *msg_in,
		struct netif *netif)
{
  	err_t result;
	u16_t opt_out_len = 0;
	struct pbuf *p = create_msg(dhcps, msg_in, DHCP_OFFER, &opt_out_len);
	if (p != NULL) {
		struct dhcp_msg *msg = (struct dhcp_msg *)p->payload;
		opt_out_len = add_reply_options(dhcps, msg, opt_out_len);
		add_option_trailer(opt_out_len, msg->options, p);
		result = udp_sendto_if(dhcps_pcb, p, IP_ADDR_BROADCAST,
				LWIP_IANA_PORT_DHCP_CLIENT, netif);
		pbuf_free(p);

	} else
		result = ERR_MEM;

	return result;
}

static err_t
dhcps_ack(struct dhcps *dhcps, struct dhcp_msg *msg_in,
		struct netif *netif)
{
  	err_t result;
	u16_t opt_out_len = 0;
	struct pbuf *p = create_msg(dhcps, msg_in, DHCP_ACK, &opt_out_len);
	if (p != NULL) {
		struct dhcp_msg *msg = (struct dhcp_msg *)p->payload;
		opt_out_len = add_reply_options(dhcps, msg, opt_out_len);
		add_option_trailer(opt_out_len, msg->options, p);
		result = udp_sendto_if(dhcps_pcb, p, IP_ADDR_BROADCAST,
				LWIP_IANA_PORT_DHCP_CLIENT, netif);
		pbuf_free(p);

	} else
		result = ERR_MEM;

	return result;
}

static err_t
dhcps_nak(struct dhcps *dhcps, struct dhcp_msg *msg_in,
		struct netif *netif)
{
  	err_t result;
	u16_t opt_out_len = 0;
	struct pbuf *p = create_msg(dhcps, msg_in, DHCP_NAK, &opt_out_len);
	if (p != NULL) {
		struct dhcp_msg *msg = (struct dhcp_msg *)p->payload;
		add_option_trailer(opt_out_len, msg->options, p);
		result = udp_sendto_if(dhcps_pcb, p, IP_ADDR_BROADCAST,
				LWIP_IANA_PORT_DHCP_CLIENT, netif);
		pbuf_free(p);

	} else
		result = ERR_MEM;

	return result;
}

static void
dhcps_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
		const ip_addr_t *addr, u16_t port)
{
	struct netif *netif = ip_current_input_netif();
	struct dhcps *dhcps = netif_dhcps_data(netif);
	struct dhcp_msg *req_msg = (struct dhcp_msg *)p->payload;
	u8_t msg_type;
	u8_t chwaddr[NETIF_MAX_HWADDR_LEN];
	struct dhcps_client *cl = NULL;
	ip_addr_t leased_ip;
	ip_addr_t request_ip;

	if ((dhcps == NULL) || (dhcps->pcb_allocated == 0))
		goto free_pbuf_and_return;

	if (p->len < DHCP_SNAME_OFS) {
    		LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING,
				("DHCP request message or pbuf too short\n"));
		goto free_pbuf_and_return;
	}

#if 0
	if (p->tot_len > sizeof(struct dhcp_msg)) {
    		LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING,
				("DHCP request message or pbuf too big\n"));
		goto free_pbuf_and_return;
	}
#endif

	if (req_msg->op != DHCP_BOOTREQUEST) {
    		LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING,
				("not a DHCP request message, but type %"U16_F"\n",
				 (u16_t)req_msg->op));
		goto free_pbuf_and_return;
	}

	/*
	 * If pbuf is split into multiple buffers,
	 * we will create a contiguous DHCP msg and
	 * copy pbuf payload onto it to parse it.
	 * It will consume more heap than inline parsing like done in
	 * dhcp_parse_reply() up to 548 bytes.
	 * But we prefer to reducing code size than heap usage for now.
	 */
	if (p->tot_len != p->len) {
		req_msg = (struct dhcp_msg *)mem_malloc(sizeof(*req_msg));
		if (req_msg == NULL) {
			LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING,
					("Allocation failed\n"));
			goto free_pbuf_and_return;
		}
		if (pbuf_copy_partial(p, req_msg, p->tot_len, 0) != p->tot_len) {
			LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING,
					("Copy from pbuf failed\n"));
			goto free_pbuf_and_return;
		}
	}

	memcpy(&chwaddr[0], &req_msg->chaddr[0], sizeof(chwaddr));
	if (dhcps_parse_request(dhcps, req_msg, p->tot_len) != ERR_OK) {
		LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
			("problem unfolding DHCP message\n"));
		goto free_pbuf_and_return;
	}

	/* FIXME: consider DHCP_OPTION_REQUESTED_IP as client_ip? */

	msg_type = (u8_t)rx_get_option_value(dhcps, DHCP_OPTION_IDX_MSG_TYPE);
	switch (msg_type) {
		case DHCP_DISCOVER:
			ip_addr_set_any(0, &leased_ip);
			if (is_bound(dhcps, chwaddr, &cl) && cl != NULL) {
				/*
				 * New discovery will just purge old bonding
				 */
				forget(dhcps, cl);
			}
			if (!is_offered(dhcps, chwaddr, &cl)) {
				if (!ip_addr_isany_val(dhcps->client_ip)) {
					if (!is_ip_bound(dhcps, dhcps->client_ip, &cl)) {
						ip_addr_copy(leased_ip, dhcps->client_ip);
					} else {
						/*
						 * Request already leased IP?
						 * just ignore it.
						 */
						LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
							("ignored discovery\n"));
						/* FIXME: send NAK? */
						break;
					}
				} else {
					ip_addr_t first_lease = get_lease(dhcps);
					/*
					 * New client, lease an ip to it */
					ip_addr_copy(leased_ip, first_lease);
				}
				offer(dhcps, chwaddr, leased_ip);
			} else if (cl != NULL) {
				/*
				 * Already lease was offered.
				 * Just use the same lease again.
				 */
				ip_addr_copy(leased_ip, cl->ipaddr);
			}
			ip_addr_copy(dhcps->your_ip, leased_ip);
			if (ip_addr_isany_val(leased_ip)) {
				LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
							("failed to lease\n"));
				break;
			}
			dhcps_offer(dhcps, req_msg, netif);
			break;
		case DHCP_REQUEST:
			ip_addr_set_ip4_u32_val(request_ip,
					lwip_htonl(rx_get_option_value(dhcps,
						DHCP_OPTION_IDX_REQUESTED_IP)));
			if (is_offered(dhcps, chwaddr, &cl) && cl != NULL) {
				if (!ip_addr_isany_val(request_ip)
						&& !ip_addr_cmp(&request_ip, &cl->ipaddr)) {
					/*
					 * Request a different ip from
					 * the one offered?
					 */
					dhcps_nak(dhcps, req_msg, netif);
					break;
				}
				ip_addr_copy(dhcps->your_ip, cl->ipaddr);
#ifndef __WISE__
				bind(dhcps, cl);
#else
				_bind(dhcps, cl);
#endif
			} else if (is_bound(dhcps, chwaddr, &cl) && cl != NULL) {
				if (ip_addr_cmp(&cl->ipaddr, &dhcps->client_ip)) {
					/*
					 * Same client requesting ip?
					 * Refill lease time
					 */
					cl->lease_time = dhcps->lease_time;
					ip_addr_copy(dhcps->your_ip, cl->ipaddr);
				} else if ((ip_addr_isany_val(dhcps->client_ip)) &&
							ip_addr_cmp(&cl->ipaddr, &request_ip)) {
					/*
					 * For the case that the client has been bound,
					 * but reconnect the SAP or renew the ip
					 */
					cl->lease_time = dhcps->lease_time;
					ip_addr_copy(dhcps->your_ip, cl->ipaddr);
				} else {
					/*
					 * Request a different ip from
					 * the one bound?
					 */
					dhcps_nak(dhcps, req_msg, netif);
					break;
				}
			} else {
				/*
				 * This client might already been forgotten
				 * because lease has been expired.
				 */
				dhcps_nak(dhcps, req_msg, netif);
				break;
			}
			dhcps_ack(dhcps, req_msg, netif);
			break;
		case DHCP_DECLINE:
		case DHCP_RELEASE:
			if ((is_offered(dhcps, chwaddr, &cl)
					|| is_bound(dhcps, chwaddr, &cl))
					&& cl != NULL)
				forget(dhcps, cl);
			break;
		default:
			LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
			("unhandled DHCP message type %d\n", msg_type));
			break;
	}

free_pbuf_and_return:
	pbuf_free(p);
}

#ifdef __WISE__

err_t
dhcps_setdns(struct netif *netif, const ip_addr_t *dnsserver)
{
	struct dhcps *dhcps;

	LWIP_ASSERT("netif != NULL", netif != NULL);

	if (dhcps_client_id == -1
			|| (dhcps = netif_dhcps_data(netif)) == NULL) {
		return ERR_IF;
	}

    if (dnsserver != NULL) {
        dhcps->dns_ip = (*dnsserver);
    } else {
        dhcps->dns_ip = *IP_ADDR_ANY;
    }

	return ERR_OK;
}

err_t
dhcps_getdns(struct netif *netif, ip_addr_t *dnsserver)
{
	struct dhcps *dhcps;

	LWIP_ASSERT("netif != NULL", netif != NULL);

	if (dhcps_client_id == -1
			|| (dhcps = netif_dhcps_data(netif)) == NULL
			|| ip_addr_isany(&dhcps->dns_ip)) {
		return ERR_VAL;
	}

    ip_addr_set(dnsserver, &dhcps->dns_ip);

    return ERR_OK;
}

#endif

#ifdef CONFIG_CMD_DHCPS
#include <netinet/in.h>
#include <arpa/inet.h>
#include "cli.h"

static int INET_rresolve(char *name, size_t len, struct sockaddr_in *sin,
			 int numeric);
static void INET_reserror(char *text);
static char *INET_print(unsigned char *ptr);
static char *INET_sprint(struct sockaddr *sap, int numeric);
static int INET_input(char *bufp, struct sockaddr *sap);
/* This structure defines protocol families and their handlers. */
static struct aftype {
	char *name;
	char *(*print)(unsigned char *);
	char *(*sprint)(struct sockaddr *, int numeric);
	int (*input)(char *bufp, struct sockaddr *);
	void (*herror)(char *text);
} inet_aftype =
{
	"inet",
	INET_print,
	INET_sprint,
	INET_input,
	INET_reserror
};

/* Like strncpy but make sure the resulting string is always 0 terminated. */
static char *safe_strncpy(char *dst, const char *src, size_t size)
{
	dst[size-1] = '\0';
	return strncpy(dst,src,size-1);
}

static int INET_resolve(char *name, struct sockaddr_in *sin)
{
	/* Grmpf. -FvK */
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	/* Default is special, meaning 0.0.0.0. */
	if (!strcmp(name, "default")) {
		sin->sin_addr.s_addr = INADDR_ANY;
		return (1);
	}
	/* Look to see if it's a dotted quad. */
	if (inet_aton(name, &sin->sin_addr)) {
		return 0;
	}
	return -1;
}

/* numeric: & 0x8000: default instead of *,
 *	    & 0x4000: host instead of net,
 *	    & 0x0fff: don't resolve
 */
static int INET_rresolve(char *name, size_t len, struct sockaddr_in *sin,
			 int numeric)
{
	u_int32_t ad;

	/* Grmpf. -FvK */
	if (sin->sin_family != AF_INET) {
#if 0
		printk("rresolve: unsupported address family %d !\n",
				sin->sin_family);
#endif
		errno = EAFNOSUPPORT;
		return (-1);
	}
	ad = sin->sin_addr.s_addr;
#if 0
	printk("rresolve: %08lx, num %08x \n",
			ad, numeric);
#endif
	if (ad == INADDR_ANY) {
		if ((numeric & 0x0FFF) == 0) {
			if (numeric & 0x8000)
				safe_strncpy(name, "default", len);
			else
				safe_strncpy(name, "*", len);
			return (0);
		}
	}

	if (!(numeric & 0x0FFF)) {
		/* host dns - not supported */
		errno = EAFNOSUPPORT;
		return (-1);
	}

	safe_strncpy(name, inet_ntoa(sin->sin_addr), len);
	return (0);
}

static void INET_reserror(char *text)
{
	printk("%s", text);
}

/* Display an Internet socket address. */
static char *INET_print(unsigned char *ptr)
{
	return (inet_ntoa((*(struct in_addr *)ptr)));
}

/* Display an Internet socket address. */
static char *INET_sprint(struct sockaddr *sap, int numeric)
{
	static char buff[128];

	if (sap->sa_family == 0xFFFF
			|| sap->sa_family == 0)
		return safe_strncpy(buff, "[NONE SET]", sizeof(buff));

	if (INET_rresolve(buff, sizeof(buff),
				(struct sockaddr_in *)sap,
				numeric) != 0)
		return (NULL);

	return (buff);
}

static int INET_input(char *bufp, struct sockaddr *sap)
{
	return (INET_resolve(bufp, (struct sockaddr_in *) sap));
}

#define SOCKADDR4_TO_IP4ADDR_PORT(sin, ipaddr) do { \
      ip_addr_set_ip4_u32(ipaddr, (sin)->sin_addr.s_addr); }while(0)

static void dhcps_list(struct netif *netif)
{
	struct dhcps *dhcps;
	struct dhcps_client *cl;
	int i, idx;
	char *list_name[2] = {"offered", "bound"};
	char *list_colors[2] = {"\x1b[36m"/*cyan*/, "\x1b[92m"/*bright green*/};

	if (dhcps_client_id == -1
			|| (dhcps = netif_dhcps_data(netif)) == NULL) {
		printf("DHCP server not running.\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(dhcps->clist); i++) {
		idx = 1;
		list_for_each_entry(cl, &dhcps->clist[i], list) {
			printf("[%s%-8s\x1b[0m - %3d] %u.%u.%u.%u (%2x.%2x.%2x.%2x.%2x.%2x) : %u minutes\n",
					list_colors[i], list_name[i], idx,
					ip4_addr1(ip_2_ip4(&cl->ipaddr)),
					ip4_addr2(ip_2_ip4(&cl->ipaddr)),
					ip4_addr3(ip_2_ip4(&cl->ipaddr)),
					ip4_addr4(ip_2_ip4(&cl->ipaddr)),
					cl->hwaddr[0], cl->hwaddr[1],
					cl->hwaddr[2], cl->hwaddr[3],
					cl->hwaddr[4], cl->hwaddr[5],
					cl->lease_time);
			idx++;
		}
	}
}

static int dhcps(int argc, char *argv[])
{
	int ret;
	char *ifname = NULL;
	char *cmd;
	struct netif *netif;
	char host[128];
	struct sockaddr sa;
	struct aftype *ap = &inet_aftype;
	ip_addr_t saddr, eaddr, *addr;
	u32_t lease_time;

	ifname = argv[1];
	netif = netifapi_netif_find(ifname);
	if (netif == NULL) {
		printf("Unknown interface %s\n", ifname);
		return CMD_RET_USAGE;
	}

	if (argc < 3)
		return CMD_RET_USAGE;

	cmd = argv[2];

	if (!strcmp(cmd, "start")) {
		if (netifapi_dhcps_start(netif) != ERR_OK)
			return CMD_RET_FAILURE;
	} else if (!strcmp(cmd, "stop")) {
		if (netifapi_dhcps_stop(netif) != ERR_OK)
			return CMD_RET_FAILURE;
	} else if (!strcmp(cmd, "lease")) {
		struct dhcps_config cfg;
		if (argc < 6)
			return CMD_RET_USAGE;
		safe_strncpy(host, argv[3], (sizeof host));
		if (ap->input(host, &sa) < 0) {
			if (ap->herror)
				ap->herror(host);
			else
				printf("ifconfig: Error resolving '%s' for ip addr\n", host);
			return CMD_RET_FAILURE;
		}
		addr = &saddr;
		SOCKADDR4_TO_IP4ADDR_PORT((struct sockaddr_in *)&sa, addr);
		safe_strncpy(host, argv[4], (sizeof host));
		if (ap->input(host, &sa) < 0) {
			if (ap->herror)
				ap->herror(host);
			else
				printf("Error resolving '%s' for ip addr\n", host);
			return CMD_RET_FAILURE;
		}
		addr = &eaddr;
		SOCKADDR4_TO_IP4ADDR_PORT((struct sockaddr_in *)&sa, addr);
		lease_time = (u32_t)atoi(argv[5]);

		cfg.start = saddr;
		cfg.end = eaddr;
		cfg.lease_to = lease_time;
		if (netifapi_dhcps_configure(netif, &cfg) != ERR_OK)
			return CMD_RET_FAILURE;
	} else if (!strcmp(cmd, "setdns")) {
		if (argc < 4)
			return CMD_RET_USAGE;
		safe_strncpy(host, argv[3], (sizeof host));
		if (ap->input(host, &sa) < 0) {
			if (ap->herror)
				ap->herror(host);
			else
				printf("Error resolving '%s' for ip addr\n", host);
			return CMD_RET_FAILURE;
		}
		addr = &saddr;
		SOCKADDR4_TO_IP4ADDR_PORT((struct sockaddr_in *)&sa, addr);
		netifapi_dhcps_setdns(netif, &saddr);
	} else if (!strcmp(cmd, "list")) {
		netifapi_netif_commonv(netif, dhcps_list);
	} else
		return CMD_RET_USAGE;

	ret = CMD_RET_SUCCESS;

	return ret;
}

CMD(dhcps, dhcps,
	"Configure, start and stop DHCP server",
	"dhcps ifname start" OR
	"dhcps ifname stop" OR
	"dhcps ifname lease <start ip> <end ip> <duration in minutes>" OR
	"dhcps ifname setdns <dns ip>" OR
	"dhcps ifname list"
);
#endif

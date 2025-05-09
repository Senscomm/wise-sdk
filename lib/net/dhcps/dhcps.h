/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __DHCPS_H
#define __DHCPS_H

#include "lwip/opt.h"
#include "lwip/udp.h"
#include "lwip/netif.h"

#include <u-boot/list.h>

#define DHCPS_LEASE_TIME_DEF	(CONFIG_LWIP_DHCPS_LEASE_TIME)
#define DHCPS_MAX_LEASE 	(CONFIG_LWIP_DHCPS_MAX_LEASE)
#define LEASE_BOARDS_BYTES	howmany(DHCPS_MAX_LEASE, NBBY)
/* DHCP server states */
typedef enum {
	DHCPS_STATE_OFF	= 0,
	DHCPS_STATE_OFFER	= 1,
	DHCPS_STATE_DECLINE	= 2,
	DHCPS_STATE_ACK   	= 3,
	DHCPS_STATE_NAK       	= 4,
	DHCPS_STATE_IDLE	= 5,
	DHCPS_STATE_RELEASE	= 6,
} dhcps_state_enum_t;

#define DHCP_OPTION_PARAM_REQ_SUBNET_MASK_FLAGS     0x0001
#define DHCP_OPTION_PARAM_REQ_ROUTER_FLAGS          0x0002
#define DHCP_OPTION_PARAM_REQ_DNS_SERVER_FLAGS      0x0004
#define DHCP_OPTION_PARAM_REQ_BROADCAST_FLAGS       0x0008

/** Option handling: options are parsed in dhcps_parse_req
 * and saved in an array where other functions can load them from.
 */
enum dhcps_option_idx {
	DHCP_OPTION_IDX_MSG_TYPE = 0,
	DHCP_OPTION_IDX_REQUESTED_IP,
	DHCP_OPTION_IDX_PARAM_REQUEST_LIST,
	DHCP_OPTION_IDX_MAX
};

enum dhcps_client_list {
	OFFERED = 0,
	BOUND
};

struct dhcps
{
	/** track PCB allocation state */
	u8_t pcb_allocated;
	/** current DHCPS state machine state */
	u8_t state;
	/** my ip addr - for easy access */
	ip_addr_t server_ip;
	/** gw ip addr - for easy access */
	ip_addr_t gw_ip;
#ifdef __WISE__
	/** dns ip addr - for easy access */
	ip_addr_t dns_ip;
#endif
	/** client ip addr - for easy access */
	ip_addr_t client_ip;
	/** leased ip */
	ip_addr_t your_ip;
	/** lease start ip */
	ip_addr_t start_ip;
	/** lease end ip */
	ip_addr_t end_ip;
	/** lease time in minutes */
	u32_t lease_time;
	/** lease boards */
	u8_t boards[LEASE_BOARDS_BYTES];
	/** clients' list */
	struct list_head clist[2];
	/** holds the decoded option values */
	u32_t rx_options_val[DHCP_OPTION_IDX_MAX];
	/** holds a flag which option was received */
	u8_t  rx_options_given[DHCP_OPTION_IDX_MAX];
};

struct dhcps_client {
	struct list_head list;
	ip_addr_t ipaddr;
	u8_t hwaddr[NETIF_MAX_HWADDR_LEN];
	u32_t lease_time;
};

struct dhcps_config {
	ip_addr_t start;
	ip_addr_t end;
	u32_t lease_to;
};

void dhcps_cleanup(struct netif *netif);
err_t dhcps_start(struct netif *netif);
err_t dhcps_stop(struct netif *netif);
err_t dhcps_configure(struct netif *netif, struct dhcps_config *cfg);
err_t dhcps_setdns(struct netif *netif, const ip_addr_t *dnsserver);
err_t dhcps_getdns(struct netif *netif, ip_addr_t *dnsserver);

#endif /* !_DHCPS__H */

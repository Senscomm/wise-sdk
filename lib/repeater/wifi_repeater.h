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

#ifndef __WIFI_REPEATER_H__
#define __WIFI_REPEATER_H__

#include <hal/types.h>
#include <u-boot/list.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OK	(0)
#define FAIL	(-1)

typedef enum {
	WIFI_REPEATER_WLAN0, /* wlan0 repeater */
	WIFI_REPEATER_WLAN1, /* wlan1 repeater */
} wifi_repeater_id;

/* Data Filter Bound */
typedef enum {
	WIFI_FILTER_TO_LWIP = 0x01, /* Packet transfer to LWIP Stack */
	WIFI_FILTER_TO_HOST = 0x02, /* Packet transfer to Host */
	WIFI_FILTER_TO_BOTH = 0x03, /* Packet transfer to LWIP & SDIO */
	WIFI_FILTER_TO_BUTT
} wifi_packet_filter;

typedef enum {
	WIFI_FILTER_TYPE_IPV4, /* Packet transfer to LWIP Stack */
	WIFI_FILTER_TYPE_IPV6, /* Packet transfer to Host */
} wifi_filter_type;

/* Data Filter Item */
typedef enum {
	WIFI_FILTER_MASK_IP = 0x01, /* Indicates the source IP address and corresponds to the remote_ip field.
				If this field is not specified, it is not used as a matching condition. */

	WIFI_FILTER_MASK_PROTOCOL = 0x02, /* Indicates the protocol type
				(TCP or UDP) and corresponds to the packet_type field.
				If this field is not specified, it is not used as a matching condition. */

	WIFI_FILTER_MASK_LOCAL_PORT = 0x04, /* Indicates the destination port number of the received packet
				and corresponds to the local_port field.
				If this field is not specified, it is not used as a matching condition. */

	WIFI_FILTER_MASK_LOCAL_PORT_RANGE = 0x08, /* Indicates the range of the destination port number of the received packet
				and corresponds to the localp_min and localp_max fields.
				If this field is not specified, it is not used as a matching condition. */

	WIFI_FILTER_MASK_REMOTE_PORT = 0x10, /* Indicates the source port number of the received packet
				and corresponds to the remote_port field.
				If this field is not specified, it is not used as a matching condition. */

	WIFI_FILTER_MASK_REMOTE_PORT_RANGE = 0x20, /* Indicates the range of the source port number of the received packet
				and corresponds to the remotep_min and remotep_max fields.
				If this field is not specified, it is not used as a matching condition. */

	WIFI_FILTER_MASK_BUTT
} wifi_filter_field_enum;

typedef void (*interface_input)(struct ifnet *ifp, struct mbuf *m);

#define WIFI_IPV6_ADDR_LEN	16

/* Data Filter Structure */
struct wifi_ipv4_filter {
	unsigned int remote_ip; /* Optional. Specifies the peer IP address carried in the received packet, that is,
				the source IP address in the packet.*/

	unsigned short local_port; /* Optional. Specifies the local port number of the received packet. For the
				receiving device, the value is the destination port number carried in the packet.*/

	unsigned short localp_min; /* Optional. Specifies the minimum value of the local port number range of the
				received packet. For the receiving device, the value is the minimum value of the
				destination port number range carried in the packet.*/

	unsigned short localp_max; /*Optional. Specifies the maximum value of the local port number range of the received
				packet. For the receiving device, the value is the maximum value of the destination port number
				range carried in the packet. The maximum value must be greater than the minimum value.*/

	unsigned short remote_port; /* Optional. Specifies the peer port number of the received packet.
				For the receiving device, the value is the source port number carried in the packet.*/

	unsigned short remotep_min; /* Optional. Specifies the minimum value of the peer port number range of the
				received packet. For the receiving device, the value is the minimum value of the source
				port number range carried in the packet.*/

	unsigned short remotep_max; /* Optional. Specifies the maximum value of the peer port number range of the received
				packet. For the receiving device, the value is the maximum value of the source port number
				range carried in the packet. The maximum value must be greater than the minimum value.*/

	unsigned char packet_type; /* Optional. Specifies the transport layer protocol used for the received packet.
				Generally, it is set to 6 (TCP) or 17 (UDP).*/

	unsigned char config_type; /* Mandatory. Sets the forwarding direction of the packets matching this forwarding
				rule. WIFI_FILTER_TO_LWIP indicates that the packets are forwarded to the device,
				WIFI_FILTER_TO_HOST indicates that the packets are forwarded to the host,
				and WIFI_FILTER_TO_BOTH indicates that the packets are forwarded to both sides.*/

	unsigned char match_mask; /* Mandatory. Specifies which optional fields are valid. Each optional field is set
				through the corresponding bit mask. If a field is set, it is a matching condition. For
				example, if WIFI_FILTER_MASK_IP | WIFI_FILTER_MASK_PROTOCOL is set to 0x1 | 0x2 = 0x3,
				only packets that match the source IP address and protocol type (TCP or UDP) conditions
				are covered by this rule, and forwarded according to the config_type settings. */
	unsigned char  resv;
};


struct wifi_ipv6_filter {
    unsigned char  remote_ip[WIFI_IPV6_ADDR_LEN];
    unsigned short local_port;
    unsigned short localp_min;
    unsigned short localp_max;
    unsigned short remote_port;
    unsigned short remotep_min;
    unsigned short remotep_max;
    unsigned char  packet_type;
    unsigned char  config_type;
    unsigned char  match_mask;
    unsigned char  resv;
};

struct repeater_ops{
	int (*init)(wifi_repeater_id idx, interface_input func);
	int (*set_default_dir)(wifi_repeater_id idx, wifi_packet_filter direction);
	int (*add_filter)(wifi_repeater_id idx, char *filter, wifi_filter_type type);
	int (*del_filter)(wifi_repeater_id idx, char *filter, wifi_filter_type type);
	int (*query_filter)(wifi_repeater_id idx, char **filter, int *num, wifi_filter_type type);
	int (*free_filters)(wifi_repeater_id idx, wifi_filter_type type);
	int (*deinit)(wifi_repeater_id idx);
	int (*host_carrier_getset)(uint32_t set, uint32_t *on);
};

struct repeater_ops* get_repeater_ops(void);

static __inline__ int wifi_repeater_host_carrier_getset(uint32_t set, uint32_t *on)
{
	struct repeater_ops *ops = get_repeater_ops();
	if (ops)
		return ops->host_carrier_getset(set, on);
	else
		return FAIL;
}

static __inline__ int wifi_repeater_init(wifi_repeater_id idx, interface_input func)
{
	struct repeater_ops *ops = get_repeater_ops();
	if (ops)
		return ops->init(idx, func);
	else
		return FAIL;
}

static __inline__ int wifi_repeater_set_default_dir(wifi_repeater_id idx, wifi_packet_filter direction)
{
	struct repeater_ops *ops = get_repeater_ops();
	if (ops)
		return ops->set_default_dir(idx, direction);
	else
		return FAIL;
}

static __inline__ int wifi_repeater_add_filter(wifi_repeater_id idx, char *filter, wifi_filter_type type)
{
	struct repeater_ops *ops = get_repeater_ops();
	if (ops)
		return ops->add_filter(idx, filter, type);
	else
		return FAIL;
}

static __inline__ int wifi_repeater_del_filter(wifi_repeater_id idx, char *filter, wifi_filter_type type)
{
	struct repeater_ops *ops = get_repeater_ops();
	if (ops)
		return ops->del_filter(idx, filter, type);
	else
		return FAIL;
}

static __inline__ int wifi_repeater_query_filter(wifi_repeater_id idx, char **filter, int *num, wifi_filter_type type)
{
	struct repeater_ops *ops = get_repeater_ops();
	if (ops)
		return ops->query_filter(idx, filter, num, type);
	else
		return FAIL;
}

static __inline__ int wifi_repeater_deinit (wifi_repeater_id idx)
{
	struct repeater_ops *ops = get_repeater_ops();
	if (ops)
		return ops->deinit(idx);
	else
		return FAIL;
}

static __inline__ int wifi_repeater_free_filters (wifi_repeater_id idx, wifi_filter_type type)
{
	struct repeater_ops *ops = get_repeater_ops();
	if (ops)
		return ops->free_filters(idx, type);
	else
		return FAIL;
}

#endif /* __WIFI_REPEATER_H__ */

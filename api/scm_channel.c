/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
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
#include <hal/wlan.h>
#include <compat_if.h>
#include <if_media.h>
#include <fweh.h>
#include <wise_wifi.h>

#include "scm_channel.h"

#define IPV4_ADDR_LEN	4
#define MAX_IPV4_LEN	(3 * IPV4_ADDR_LEN)
#define MAX_SCM_CH_MSG_LEN 1500

int scm_channel_construct_ip_msg(char *net_if, char *msg, int *msg_len)
{
	uint8_t i = 0;
	wifi_ip_info_t ipinfo = {0};
	wifi_interface_t ifc = (strcmp(net_if, "wlan0") ? WISE_IF_WIFI_AP : WISE_IF_WIFI_STA);
	char ip_str[MAX_IPV4_LEN + 1] = {0};

	if (ifc != WISE_IF_WIFI_STA)
		return WISE_FAIL;

	if (wise_wifi_get_ip_info(ifc,      &ipinfo)) {
		return WISE_FAIL;
	}

	/* copy ip address*/
	memcpy(&ip_str[i], &ipinfo.ip, IPV4_ADDR_LEN);
	i += IPV4_ADDR_LEN;

	/* copy mask address*/
	memcpy(&ip_str[i], &ipinfo.nm, IPV4_ADDR_LEN);
	i += IPV4_ADDR_LEN;

	/* copy getway address*/
	memcpy(&ip_str[i], &ipinfo.gw, IPV4_ADDR_LEN);
	i += IPV4_ADDR_LEN;

	memcpy(msg, ip_str, sizeof(ip_str));
	*msg_len += sizeof(ip_str);
	return WISE_OK;
}


int scm_channel_register_rx_cb(wise_channel_cb_fn fn)
{
	if (!fn)
		return WISE_FAIL;

	return wise_channel_register_rx_cb(fn);

}

uint32_t scm_channel_host_ready(void)
{
	return wise_channel_get_host_status();
}

int scm_channel_send_to_host(char *buf, int len)
{
	system_event_t evt;
	uint8_t *send_buf = NULL;
	int ret = WISE_FAIL;

	if (!buf)
		return WISE_FAIL;

	if (len > MAX_SCM_CH_MSG_LEN || len <= 0) {
		printf("fail: invalid len\n");
		return WISE_FAIL;
	}

	if (!scm_channel_host_ready()) {
		printf("fail: host not ready\n");
		return WISE_FAIL;
	}

	send_buf = malloc(len);
	if (!send_buf)
		return WISE_FAIL;
	memcpy(send_buf, buf, len);
	memset(&evt, 0, sizeof(evt));
	evt.event_id = SYSTEM_EVENT_SCM_CHANNEL;
	evt.event_info.scm_channel_msg.buf = (uint8_t *)send_buf;
	evt.event_info.scm_channel_msg.buf_len = len;

	if ((ret = wise_event_send(&evt))) {
		printf("wise_event_send fail: event handler not ready\n");
		free(send_buf);
	}
	return ret;
}

int scm_channel_add_filter(char *filter, wifi_filter_type type)
{
	struct wifi_ipv4_filter *add_filter = (struct wifi_ipv4_filter *)filter;

	/* Parameter(match_mask and config_type should be checked) checking */
	if (!add_filter->match_mask || (add_filter->config_type < 1 || add_filter->config_type > 3)) {
		return WISE_FAIL;
	}

	return wise_wifi_add_filter(filter, type);
}

int scm_channel_del_filter(char *filter, wifi_filter_type type)
{
	struct wifi_ipv4_filter *del_filter = (struct wifi_ipv4_filter *)filter;

	/* Parameter(match_mask and config_type should be checked) checking */
	if (!del_filter->match_mask || (del_filter->config_type < 1 || del_filter->config_type > 3)) {
		return WISE_FAIL;
	}

	return wise_wifi_del_filter(filter, type);
}

int scm_channel_query_filter(char **filter, int *num, wifi_filter_type type)
{
	return wise_wifi_query_filter(filter, num, type);
}

int scm_channel_set_default_filter(wifi_packet_filter direction)
{
	if (direction < 1 || direction > 3) {
		return WISE_FAIL;
	}

	return wise_wifi_set_default_filter(direction);
}

int scm_channel_reset_filter(wifi_filter_type type)
{

	return wise_wifi_free_filters(type);
}

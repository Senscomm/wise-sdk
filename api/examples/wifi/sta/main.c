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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hal/unaligned.h>
#include <hal/kernel.h>
#include <hal/wlan.h>
#include <hal/kmem.h>

#include "kernel.h"
#include "compat_if.h"
#include "if_media.h"
#include "sncmu_d11.h"
#include "fwil_types.h"
#include "fweh.h"
#include "scdc.h"
#include "task.h"
#include "FreeRTOS.h"

#include <net80211/ieee80211_var.h>
#include <wise_err.h>
#include <wise_log.h>
#include <wise_wifi.h>
#include <wise_event_loop.h>
#include <common.h>

#include <scm_wifi.h>
#include <scm_log.h>
#ifdef CONFIG_API_SCMCHANNEL
#include <scm_channel.h>
#endif

#define STA_APP_TAG "STA_APP"

#define MAX_CMD_LEN		20
#define MAC_ADDR_LEN	6
#define WIFI_STA_NETIF_NAME "wlan0"

#define SCM_NEED_DHCP_START(event) ((event)->event_info.connected.not_en_dhcp == false)
enum {
	HOST_CMD_GET_MAC,
	HOST_CMD_GET_IP,
	HOST_CMD_SET_FILTER,
	HOST_CMD_TBTT
};

#ifdef CONFIG_API_SCMCHANNEL

static int g_host_app_ready = 0;

#define SCM_IS_HOST_APP_READY() g_host_app_ready
#define SCM_SET_HOST_AP_READY(s) { \
	g_host_app_ready = s; \
}

char cmd[][MAX_CMD_LEN] = {
	"cmd_get_mac",
	"cmd_get_ip",
	"cmd_set_filter"
};

static struct wifi_ipv4_filter ipv4_filter_def_setting[] = {
	/* DHCP */
	{
		0,							 /* remote ip */
		68,							 /* local port */
		0,							 /* localp_min */
		0,							 /* localp_max */
		0,							 /* remote_port */
		0,							 /* remotep_min */
		0,							 /* remotep_max */
		17,							 /* packet type */
		WIFI_FILTER_TO_LWIP,					 /* config_type */
		WIFI_FILTER_MASK_LOCAL_PORT | WIFI_FILTER_MASK_PROTOCOL, /* match_mask */
	},
	/* DHCP */
	{
		0,							 /* remote ip */
		67,							 /* local port */
		0,							 /* localp_min */
		0,							 /* localp_max */
		0,							 /* remote_port */
		0,							 /* remotep_min */
		0,							 /* remotep_max */
		17,							 /* packet type */
		WIFI_FILTER_TO_LWIP,					 /* config_type */
		WIFI_FILTER_MASK_LOCAL_PORT | WIFI_FILTER_MASK_PROTOCOL, /* match_mask */
	},
	/* TCP */
	{
		0,							 /* remote ip */
		6001,							 /* local port */
		0,							 /* localp_min */
		0,							 /* localp_max */
		0,							 /* remote_port */
		0,							 /* remotep_min */
		0,							 /* remotep_max */
		6,							 /* packet type */
		WIFI_FILTER_TO_LWIP,					 /* config_type */
		WIFI_FILTER_MASK_LOCAL_PORT | WIFI_FILTER_MASK_PROTOCOL, /* match_mask */
	},
	/* TCP */
	{
		0,							 /* remote ip */
		0,							 /* local port */
		0,							 /* localp_min */
		0,							 /* localp_max */
		6002,							 /* remote_port */
		0,							 /* remotep_min */
		0,							 /* remotep_max */
		6,							 /* packet type */
		WIFI_FILTER_TO_LWIP,					 /* config_type */
		WIFI_FILTER_MASK_REMOTE_PORT | WIFI_FILTER_MASK_PROTOCOL, /* match_mask */
	},
	/* UDP */
	{
		0,							 /* remote ip */
		7001,							 /* local port */
		0,							 /* localp_min */
		0,							 /* localp_max */
		0,							 /* remote_port */
		0,							 /* remotep_min */
		0,							 /* remotep_max */
		17,							 /* packet type */
		WIFI_FILTER_TO_LWIP,					 /* config_type */
		WIFI_FILTER_MASK_LOCAL_PORT | WIFI_FILTER_MASK_PROTOCOL, /* match_mask */
	},
	/* UDP */
	{
		0,							 /* remote ip */
		0,							 /* local port */
		0,							 /* localp_min */
		0,							 /* localp_max */
		7002,							 /* remote_port */
		0,							 /* remotep_min */
		0,							 /* remotep_max */
		17,							 /* packet type */
		WIFI_FILTER_TO_LWIP,					 /* config_type */
		WIFI_FILTER_MASK_REMOTE_PORT | WIFI_FILTER_MASK_PROTOCOL, /* match_mask */
	},
};
#endif
wise_err_t event_handler(void *ctx, system_event_t * event)
{
	__maybe_unused char msg[512];
	__maybe_unused int msg_len = 0;


	switch (event->event_id) {
	case SYSTEM_EVENT_STA_START:
		break;
	case SYSTEM_EVENT_STA_STOP:
		break;
	case SYSTEM_EVENT_STA_GOT_IP:

#ifdef CONFIG_API_SCMCHANNEL
		if (SCM_IS_HOST_APP_READY()) {
			msg[0] = HOST_CMD_GET_IP;
			msg_len++;
			if (scm_channel_construct_ip_msg(WIFI_STA_NETIF_NAME, &msg[1], &msg_len)) {
				return WISE_FAIL;
			}
			scm_channel_send_to_host(msg, msg_len);
			SCM_INFO_LOG(STA_APP_TAG, "WIFI GOT IP indicate\n");
		} else {
			SCM_INFO_LOG(STA_APP_TAG, "WIFI GOT IP wait host app\n");
		}
#endif
		break;
	case SYSTEM_EVENT_AP_START:
		break;
	case SYSTEM_EVENT_AP_STOP:
		break;
	case SYSTEM_EVENT_AP_STACONNECTED:
		break;
	case SYSTEM_EVENT_AP_STADISCONNECTED:
		break;
	case SYSTEM_EVENT_STA_CONNECTED:

		if (SCM_NEED_DHCP_START(event)) {
			scm_wifi_status connect_status;
			netifapi_dhcp_start(scm_wifi_get_netif(WISE_IF_WIFI_STA));
			scm_wifi_sta_get_connect_info(&connect_status);
			scm_wifi_sta_dump_ap_info(&connect_status);
		}
#ifdef CONFIG_API_SCMCHANNEL
		if (scm_channel_host_ready()) {
			scm_wifi_event_send(event, sizeof(system_event_t));
			SCM_INFO_LOG(STA_APP_TAG, "WIFI CONNECTED indicate\n");
		}
#endif
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		SCM_INFO_LOG(STA_APP_TAG, "WIFI DISCONNECT\n");
		break;
	case SYSTEM_EVENT_SCAN_DONE:
		SCM_INFO_LOG(STA_APP_TAG, "WiFi: Scan results available\n");

		break;
	case SYSTEM_EVENT_SCM_CHANNEL:
		SCM_INFO_LOG(STA_APP_TAG, "WiFi: Scm channel send msg\n");
		scm_wifi_event_send(event, sizeof(system_event_t));
		break;
	default:
		break;
	}

	return WISE_OK;
}

scm_wifi_assoc_request g_assoc_req = {
	.ssid = "test_ssid",
	.auth = SCM_WIFI_SECURITY_OPEN,
	.key = "12345678",
	.pairwise = SCM_WIFI_PAIRWISE_NONE,
};

int scm_wifi_start_connect(void)
{
	scm_wifi_assoc_request *assoc_req = &g_assoc_req;

	if (scm_wifi_sta_set_config(assoc_req, NULL))
		return WISE_FAIL;

	return scm_wifi_sta_connect();
}

int scm_wifi_start_fast_connect(void)
{
	scm_wifi_fast_assoc_request fast_request = {0};

	memcpy(&fast_request.req, &g_assoc_req, sizeof(scm_wifi_assoc_request));
	fast_request.channel = 6;
	if (scm_wifi_sta_restore_psk(fast_request.psk))
		return WISE_FAIL;

	return scm_wifi_sta_fast_connect(&fast_request);
}

#ifdef CONFIG_API_SCMCHANNEL

static int scm_wifi_set_default_filter(void)
{
	int index;

	if (ARRAY_SIZE(ipv4_filter_def_setting) > MAX_WIFI_FILTER_IPV4_CNT) {
		SCM_ERR_LOG(STA_APP_TAG, "fail: ipv4 filter table sz over %d \n", MAX_WIFI_FILTER_IPV4_CNT);
		return WISE_FAIL;
	}
	if (scm_channel_set_default_filter(WIFI_FILTER_TO_HOST))
		return WISE_FAIL;

	SCM_INFO_LOG(STA_APP_TAG, "set all net packets forward to camera default.\n");

	for (index = 0; index < ARRAY_SIZE(ipv4_filter_def_setting); index++) {
		if (scm_channel_add_filter((char *) &ipv4_filter_def_setting[index], WIFI_FILTER_TYPE_IPV4)) {
			SCM_ERR_LOG(STA_APP_TAG, "add filter failed: duplicate element or check filter dependency\n");
		}
	}

	return WISE_OK;
}

int scm_wifi_channel_rx_callback(char *buf, int len)
{
	char msg[512];
	int msg_len = 0;
	int index;

	if ((buf == NULL) || (len == 0)) {
		return WISE_FAIL;
	}

	for (index = HOST_CMD_GET_MAC; index < HOST_CMD_TBTT; index ++) {
		if (memcmp(buf, cmd[index], strlen(cmd[index])) == 0) {
			break;
		}
	}

	if (index == HOST_CMD_GET_MAC) {
		uint8_t *mac_addr = NULL;

		scm_wifi_get_wlan_mac(&mac_addr, WISE_IF_WIFI_STA);
		if (mac_addr == NULL) {
			return WISE_FAIL;
		}
		msg[0] = HOST_CMD_GET_MAC;
		msg_len++;
		memcpy(&msg[1], (char *)mac_addr, MAC_ADDR_LEN);
		msg_len += MAC_ADDR_LEN;
		scm_channel_send_to_host(msg, msg_len);
		SCM_SET_HOST_AP_READY(1);

	} else if (index == HOST_CMD_GET_IP) {
		msg[0] = HOST_CMD_GET_IP;
		msg_len++;
		if (scm_channel_construct_ip_msg(WIFI_STA_NETIF_NAME, &msg[1], &msg_len)) {
			return WISE_FAIL;
		}
		scm_channel_send_to_host(msg, msg_len);
	} else if (index == HOST_CMD_SET_FILTER) {
		scm_wifi_set_default_filter();
	}

	return WISE_OK;
}

void scm_wifi_dump_filter(void)
{
	struct wifi_ipv4_filter *wlan_filter;
	int filter_index;
	int num;

	if (scm_channel_query_filter((char **) &wlan_filter, &num, WIFI_FILTER_TYPE_IPV4))
		return;

	for (filter_index = 0; filter_index < num; filter_index++) {
		SCM_DBG_LOG (STA_APP_TAG, "[%d] protocol(%d) dest port(%d) config_type(%d) match_mask(0x%x)\n",
			 filter_index, wlan_filter->packet_type,
			 wlan_filter->local_port, wlan_filter->config_type,
			 wlan_filter->match_mask);
		wlan_filter++;
	}
}

int scm_wifi_channel_init(void)
{

	if (scm_channel_reset_filter(WIFI_FILTER_TYPE_IPV4) != WISE_OK) {
		SCM_ERR_LOG(STA_APP_TAG, "%s: netif reset failed\n", __func__);
		return WISE_FAIL;
	}

	if (scm_wifi_set_default_filter() != WISE_OK) {
		SCM_ERR_LOG(STA_APP_TAG, "%s: set_default_filter failed\n", __func__);
		return WISE_FAIL;
	}

	scm_wifi_dump_filter();
	scm_channel_register_rx_cb(scm_wifi_channel_rx_callback);

	SCM_INFO_LOG(STA_APP_TAG, "ScmChannel  init OK\n");
	return WISE_OK;
}

#endif /* CONFIG_API_SCMCHANNEL */

int main(void)
{
	int ret = WISE_OK;
	char ifname[WIFI_IFNAME_MAX_SIZE + 1] = {0};
	int len = sizeof(ifname);

	SCM_INFO_LOG(STA_APP_TAG, "Sta Hello world!\n");

	scm_wifi_register_event_callback(event_handler, NULL);

	scm_wifi_sta_start(ifname, &len);

#ifdef CONFIG_API_SCMCHANNEL
	scm_wifi_channel_init();
#endif

	//ret = scm_wifi_start_fast_connect();
	ret = scm_wifi_start_connect();

	return ret;
}

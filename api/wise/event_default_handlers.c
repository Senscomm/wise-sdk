/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.
 */
// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * Inspired by ESP8266_RTOS_SDK
 * (https://github.com/espressif/ESP8266_RTOS_SDK)
 * and will provide wise Wi-Fi API as being ESP8266 style
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/task.h"
#include "FreeRTOS/queue.h"
#include "FreeRTOS/semphr.h"

#include "lwip/netif.h"
#include "lwip/netifapi.h"

#ifdef CONFIG_ATCMD_AT_CWDHCP

#include "dhcps.h"
#endif

#include "wise_err.h"
#include "wise_log.h"
#include "wise_wifi.h"
#include "wise_event.h"
#include "wise_event_loop.h"

static const char* TAG __maybe_unused = "event";

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

#define IP2STR(ipaddr) ip4_addr1_16(ipaddr), \
	ip4_addr2_16(ipaddr), \
	ip4_addr3_16(ipaddr), \
	ip4_addr4_16(ipaddr)

#define IPSTR "%d.%d.%d.%d"

#if WISE_EVENT_IPV6
#define IPV62STR(ipaddr) IP6_ADDR_BLOCK1(&(ipaddr)),     \
	IP6_ADDR_BLOCK2(&(ipaddr)),     \
	IP6_ADDR_BLOCK3(&(ipaddr)),     \
	IP6_ADDR_BLOCK4(&(ipaddr)),     \
	IP6_ADDR_BLOCK5(&(ipaddr)),     \
	IP6_ADDR_BLOCK6(&(ipaddr)),     \
	IP6_ADDR_BLOCK7(&(ipaddr)),     \
	IP6_ADDR_BLOCK8(&(ipaddr))

#define IPV6STR "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x"
#endif

#define WIFI_API_CALL_CHECK(info, api_call, ret) \
do{\
	wise_err_t __err = (api_call);\
	if ((ret) != __err) {\
		WISE_LOGE(TAG, "%s %d %s ret=0x%X", __FUNCTION__, __LINE__, (info), __err);\
		return __err;\
	}\
} while(0)

typedef wise_err_t (*system_event_handler_t)(system_event_t *e);

static wise_err_t system_event_ap_start_handle_default(system_event_t *event);
static wise_err_t system_event_ap_stop_handle_default(system_event_t *event);
static wise_err_t system_event_sta_start_handle_default(system_event_t *event);
static wise_err_t system_event_sta_stop_handle_default(system_event_t *event);
static wise_err_t system_event_sta_connected_handle_default(system_event_t *event);
static wise_err_t system_event_sta_disconnected_handle_default(system_event_t *event);
static wise_err_t system_event_sta_got_ip_default(system_event_t *event);
static wise_err_t system_event_sta_lost_ip_default(system_event_t *event);
static wise_err_t system_event_scan_done(system_event_t *event);

/*
 * Default event handler functions
 * Any entry in this table which is disabled by config will have a NULL handler.
*/
static system_event_handler_t default_event_handlers[SYSTEM_EVENT_MAX] = { 0 };

static wise_err_t system_event_sta_got_ip_default(system_event_t *event)
{
	WISE_LOGD(TAG, "sta ip: " IPSTR ", mask: " IPSTR ", gw: " IPSTR,
			IP2STR(&event->event_info.got_ip.ip_info.ip),
			IP2STR(&event->event_info.got_ip.ip_info.netmask),
			IP2STR(&event->event_info.got_ip.ip_info.gw));
	return WISE_OK;
}

static wise_err_t system_event_sta_lost_ip_default(system_event_t *event)
{
	return WISE_OK;
}

wise_err_t system_event_ap_start_handle_default(system_event_t *event)
{
#ifdef CONFIG_ATCMD_AT_APDHCP_AUTO
	struct netif *netif = netifapi_netif_find("wlan0");

	if (netif == NULL ||dhcps_start(netif) != ERR_OK)
		return WISE_FAIL;
#endif
	return WISE_OK;
}

wise_err_t system_event_ap_stop_handle_default(system_event_t *event)
{
#ifdef CONFIG_ATCMD_AT_APDHCP_AUTO
	struct netif *netif = netifapi_netif_find("wlan0");

	if (netif == NULL || dhcps_stop(netif) != ERR_OK)
		return WISE_FAIL;
#endif
	return WISE_OK;
}

wise_err_t system_event_sta_start_handle_default(system_event_t *event)
{
	return WISE_OK;
}

wise_err_t system_event_sta_stop_handle_default(system_event_t *event)
{
	return WISE_OK;
}

wise_err_t system_event_sta_connected_handle_default(system_event_t *event)
{
/*  This is for key setting slow than dhcp request send out.
 *  normal case it will cover by dhcp_fine_tmr.
 *  Just in case if sys_timeouts_deinit be called by PM,
 *  keep code for reference
 */
#if 0
#if LWIP_DHCP
	struct netif *netif = netifapi_netif_find("wlan0");
	if (netif)
		dhcp_network_changed(netif);
#endif /* LWIP_DHCP */
#endif
	return WISE_OK;
}

wise_err_t system_event_sta_disconnected_handle_default(system_event_t *event)
{
	return WISE_OK;
}

static wise_err_t system_event_scan_done(system_event_t *event)
{
	return WISE_OK;
}

static wise_err_t wise_system_event_debug(system_event_t *event)
{
	if (event == NULL) {
		WISE_LOGE(TAG, "event is null!");
		return WISE_FAIL;
	}

	switch (event->event_id) {
	case SYSTEM_EVENT_WIFI_READY:
		WISE_LOGD(TAG, "SYSTEM_EVENT_WIFI_READY");
		break;
	case SYSTEM_EVENT_SCAN_DONE:
	{
		system_event_sta_scan_done_t *scan_done __maybe_unused = &event->event_info.scan_done;
		WISE_LOGD(TAG, "SYSTEM_EVENT_SCAN_DONE, status:%lu, number:%d",  scan_done->status, scan_done->number);
		break;
	}
	case SYSTEM_EVENT_STA_START:
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_START");
		break;
	case SYSTEM_EVENT_STA_STOP:
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_STOP");
		break;
	case SYSTEM_EVENT_STA_CONNECTED:
	{
		system_event_sta_connected_t *connected __maybe_unused = &event->event_info.connected;
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_CONNECTED, ssid:%s, ssid_len:%d, bssid:" MACSTR ", channel:%d, authmode:%d", \
				connected->ssid, connected->ssid_len, MAC2STR(connected->bssid), connected->channel, connected->authmode);
		break;
	}
	case SYSTEM_EVENT_STA_DISCONNECTED:
	{
		system_event_sta_disconnected_t *disconnected __maybe_unused = &event->event_info.disconnected;
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_DISCONNECTED, ssid:%s, ssid_len:%d, bssid:" MACSTR ", reason:%d", \
				disconnected->ssid, disconnected->ssid_len, MAC2STR(disconnected->bssid), disconnected->reason);
		break;
	}
	case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
	{
		system_event_sta_authmode_change_t *auth_change __maybe_unused = &event->event_info.auth_change;
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_AUTHMODE_CHNAGE, old_mode:%d, new_mode:%d", auth_change->old_mode, auth_change->new_mode);
		break;
	}
	case SYSTEM_EVENT_STA_GOT_IP:
	{
		system_event_sta_got_ip_t *got_ip __maybe_unused = &event->event_info.got_ip;
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_GOT_IP, ip:" IPSTR ", mask:" IPSTR ", gw:" IPSTR,
				IP2STR(&got_ip->ip_info.ip),
				IP2STR(&got_ip->ip_info.netmask),
				IP2STR(&got_ip->ip_info.gw));
		break;
	}
	case SYSTEM_EVENT_STA_LOST_IP:
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_LOST_IP");
		break;
	case SYSTEM_EVENT_STA_NO_NETWORK:
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_NO_NETWORK, No suitable network found");
		break;
	case SYSTEM_EVENT_STA_STATE_CHANGE:
	{
		system_event_sta_state_change_t *state_ch __maybe_unused = &event->event_info.sta_state_change;
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_STATE_CHANGE, %s -> %s",
			state_ch->str_old_state, state_ch->str_new_state);
		break;
	}
	case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_WPS_ER_SUCCESS");
		break;
	case SYSTEM_EVENT_STA_WPS_ER_FAILED:
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_WPS_ER_FAILED");
		break;
	case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_WPS_ER_TIMEOUT");
		break;
	case SYSTEM_EVENT_STA_WPS_ER_PIN:
		WISE_LOGD(TAG, "SYSTEM_EVENT_STA_WPS_ER_PIN");
		break;
	case SYSTEM_EVENT_AP_START:
		WISE_LOGD(TAG, "SYSTEM_EVENT_AP_START");
		break;
	case SYSTEM_EVENT_AP_STOP:
		WISE_LOGD(TAG, "SYSTEM_EVENT_AP_STOP");
		break;
	case SYSTEM_EVENT_AP_STACONNECTED:
	{
		system_event_ap_staconnected_t *staconnected __maybe_unused = &event->event_info.sta_connected;
		WISE_LOGD(TAG, "SYSTEM_EVENT_AP_STACONNECTED, mac:" MACSTR ", aid:%d", \
				MAC2STR(staconnected->mac), staconnected->aid);
		break;
	}
	case SYSTEM_EVENT_AP_STADISCONNECTED:
	{
		system_event_ap_stadisconnected_t *stadisconnected __maybe_unused = &event->event_info.sta_disconnected;
		WISE_LOGD(TAG, "SYSTEM_EVENT_AP_STADISCONNECTED, mac:" MACSTR ", aid:%d", \
				MAC2STR(stadisconnected->mac), stadisconnected->aid);
		break;
	}
	case SYSTEM_EVENT_AP_STAIPASSIGNED:
		WISE_LOGD(TAG, "SYSTEM_EVENT_AP_STAIPASSIGNED");
		break;
	case SYSTEM_EVENT_AP_PROBEREQRECVED:
	{
		system_event_ap_probe_req_rx_t *ap_probereqrecved __maybe_unused = &event->event_info.ap_probereqrecved;
		WISE_LOGD(TAG, "SYSTEM_EVENT_AP_PROBEREQRECVED, rssi:%d, mac:" MACSTR, \
				ap_probereqrecved->rssi, \
				MAC2STR(ap_probereqrecved->mac));
		break;
	}
#if WISE_EVENT_IPV6
	case SYSTEM_EVENT_GOT_IP6: {
		ip6_addr_t *addr __maybe_unused = &event->event_info.got_ip6.ip6_info.ip;
		WISE_LOGD(TAG, "SYSTEM_EVENT_AP_STA_GOT_IP6 address %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
				IP6_ADDR_BLOCK1(addr),
				IP6_ADDR_BLOCK2(addr),
				IP6_ADDR_BLOCK3(addr),
				IP6_ADDR_BLOCK4(addr),
				IP6_ADDR_BLOCK5(addr),
				IP6_ADDR_BLOCK6(addr),
				IP6_ADDR_BLOCK7(addr),
				IP6_ADDR_BLOCK8(addr));
		break;
	}
#endif
	case SYSTEM_EVENT_SCM_CHANNEL:
		break;
	case SYSTEM_EVENT_SCM_LINK_UP:
		break;
	case SYSTEM_EVENT_REKEY:
		break;
	case SYSTEM_EVENT_MAX_RETRY:
		break;
	default:
		WISE_LOGW(TAG, "unexpected system event %d!", event->event_id);
		break;
	}

	return WISE_OK;
}

wise_err_t wise_event_process_default(system_event_t *event)
{
	if (event == NULL) {
		WISE_LOGE(TAG, "Error: event is null!");
		return WISE_FAIL;
	}

	wise_system_event_debug(event);
	if ((event->event_id < SYSTEM_EVENT_MAX)) {
		if (default_event_handlers[event->event_id] != NULL) {
			WISE_LOGV(TAG, "enter default callback");
			default_event_handlers[event->event_id](event);
			WISE_LOGV(TAG, "exit default callback");
		}
	} else {
		WISE_LOGE(TAG, "mismatch or invalid event, id=%d", event->event_id);
		return WISE_FAIL;
	}
	return WISE_OK;
}

void wise_event_set_default_wifi_handlers()
{
     default_event_handlers[SYSTEM_EVENT_STA_START]        = system_event_sta_start_handle_default;
     default_event_handlers[SYSTEM_EVENT_STA_STOP]         = system_event_sta_stop_handle_default;
     default_event_handlers[SYSTEM_EVENT_STA_CONNECTED]    = system_event_sta_connected_handle_default;
     default_event_handlers[SYSTEM_EVENT_STA_DISCONNECTED] = system_event_sta_disconnected_handle_default;
     default_event_handlers[SYSTEM_EVENT_STA_GOT_IP]       = system_event_sta_got_ip_default;
     default_event_handlers[SYSTEM_EVENT_STA_LOST_IP]      = system_event_sta_lost_ip_default;
     default_event_handlers[SYSTEM_EVENT_AP_START]         = system_event_ap_start_handle_default;
     default_event_handlers[SYSTEM_EVENT_AP_STOP]          = system_event_ap_stop_handle_default;
     default_event_handlers[SYSTEM_EVENT_SCAN_DONE]        = system_event_scan_done;
}

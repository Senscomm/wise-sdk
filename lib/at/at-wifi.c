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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <at.h>

#include <hal/kernel.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include "compat_param.h"
#include "compat_if.h"
#include "if_dl.h"
#include "if_media.h"
#include "ethernet.h"
#include "route.h"
#include "libifconfig.h"

#include "lwip/netif.h"
#include "lwip/netifapi.h"

#ifdef CONFIG_ATCMD_AT_CWDHCP
#include "lwip/prot/dhcp.h"
#include "dhcps.h"
#endif

#include "lwip/memp.h"

#include <common.h>



#include <wise_err.h>
#include <wise_log.h>
#include <wise_wifi.h>
#include <wise_event_loop.h>
#include "at-wifi.h"

#define	IEEE80211_TXPOWER_MIN	0	/* kill radio */

static bool g_wifi_inited = false;

#define WIFI_IS_INITED() g_wifi_inited
#define WIFI_SET_INITED(s) { \
	g_wifi_inited = s; \
}

struct at_wifi_ops_t *at_wifi_ops = NULL;

void at_wifi_ops_register(struct at_wifi_ops_t *ops)
{
	at_wifi_ops = ops;
}

extern int dhcps_client_id;
wise_err_t at_event_handler(void *ctx, system_event_t * event)
{
	bool event_indicate __maybe_unused = false;

	switch (event->event_id) {
	case SYSTEM_EVENT_STA_START:
		break;
	case SYSTEM_EVENT_STA_STOP:
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		printf("\r\nWIFI GOT IP\r\n");
#ifdef CONFIG_AT_OVER_SCDC
        event_indicate = true;
#endif
		break;
	case SYSTEM_EVENT_GOT_IP6:
		printf("\r\nWIFI GOT IPv6\r\n");
#ifdef CONFIG_AT_OVER_SCDC
		event_indicate = true;
#endif
		break;
	case SYSTEM_EVENT_AP_START:
		event_indicate = true;
		break;
	case SYSTEM_EVENT_AP_STOP:
		event_indicate = true;
		break;
	case SYSTEM_EVENT_AP_STACONNECTED:
		event_indicate = true;
		break;
	case SYSTEM_EVENT_AP_STADISCONNECTED:
		event_indicate = true;
		break;
	case SYSTEM_EVENT_STA_CONNECTED:
		event_indicate = true;
		printf("\r\nWIFI CONNECTED\r\n");
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		event_indicate = true;
		printf("\r\nWIFI DISCONNECT\r\n");
		break;
	case SYSTEM_EVENT_SCAN_DONE:
		event_indicate = true;

#if 0
		{
			uint16_t ap_num;
			wifi_ap_record_t *ap_rec;
			int i;

			WISE_ERROR_CHECK(wise_wifi_scan_get_ap_num(&ap_num));
			if (!ap_num) {
				WISE_LOGW(TAG, "No AP found in scan");
				break;
			}
			ap_rec =
				(wifi_ap_record_t *) zalloc(ap_num *
											sizeof(wifi_ap_record_t));
			WISE_ERROR_CHECK(wise_wifi_scan_get_ap_records
							 (&ap_num, ap_rec));
			for (i = 0; i < ap_num; i++) {
				WISE_LOGI(TAG,
							"%3d.%-32s[%2d|%d][%-19s|%-9s|%-9s][%c|%c|%c]",
							(i + 1), (const char *) ap_rec[i].ssid,
							ap_rec[i].primary, ap_rec[i].rssi,
							ap_rec[i].authmode ==
							WIFI_AUTH_OPEN ? "OPEN" : ap_rec[i].authmode ==
							WIFI_AUTH_WEP ? "WEP" : ap_rec[i].authmode ==
							WIFI_AUTH_WPA_PSK ? "WPA_PSK" : ap_rec[i].
							authmode ==
							WIFI_AUTH_WPA2_PSK ? "WPA2_PSK" : ap_rec[i].
							authmode ==
							WIFI_AUTH_WPA_ENTERPRISE ? "WPA_ENTERPRISE" :
							ap_rec[i].authmode ==
							WIFI_AUTH_WPA2_ENTERPRISE ? "WPA2_ENTERPRISE" :
							ap_rec[i].authmode ==
							WIFI_AUTH_WPA_WPA2_PSK ? "WPA_WPA2_PSK" :
							ap_rec[i].authmode ==
							WIFI_AUTH_WPA_WPA2_ENTERPRISE ?
							"WPA_WPA2_ENTERPRISE" : "NONE",
							ap_rec[i].pairwise_cipher ==
							WIFI_CIPHER_TYPE_WEP40 ? "WEP40" : ap_rec[i].
							pairwise_cipher ==
							WIFI_CIPHER_TYPE_WEP104 ? "WEP104" : ap_rec[i].
							pairwise_cipher ==
							WIFI_CIPHER_TYPE_TKIP ? "TKIP" : ap_rec[i].
							pairwise_cipher ==
							WIFI_CIPHER_TYPE_CCMP ? "CCMP" : ap_rec[i].
							pairwise_cipher ==
							WIFI_CIPHER_TYPE_TKIP_CCMP ? "TKIP_CCMP" :
							"NONE",
							ap_rec[i].group_cipher ==
							WIFI_CIPHER_TYPE_WEP40 ? "WEP40" : ap_rec[i].
							group_cipher ==
							WIFI_CIPHER_TYPE_WEP104 ? "WEP104" : ap_rec[i].
							group_cipher ==
							WIFI_CIPHER_TYPE_TKIP ? "TKIP" : ap_rec[i].
							group_cipher ==
							WIFI_CIPHER_TYPE_CCMP ? "CCMP" : ap_rec[i].
							group_cipher ==
							WIFI_CIPHER_TYPE_TKIP_CCMP ? "TKIP_CCMP" :
							"NONE", ap_rec[i].phy_11b ? 'B' : ' ',
							ap_rec[i].phy_11g ? 'G' : ' ',
							ap_rec[i].phy_11n ? 'N' : ' ');
			}
			free(ap_rec);
		}
#endif
		break;
	default:
		break;
	}

#ifdef CONFIG_AT_OVER_IPC
	if (event_indicate)
		at_ipc_send_event((uint8_t *) event, sizeof(system_event_t));
#endif

#ifdef CONFIG_AT_OVER_SCDC
    if (event_indicate) {
        if (at_wifi_ops && at_wifi_ops->at_send_event_hdl)
            at_wifi_ops->at_send_event_hdl(event, sizeof(system_event_t));
    }
#endif

	return WISE_OK;
}

void at_wifi_init(void)
{
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

#ifdef CONFIG_AT_OVER_SCDC
    uint8_t *mac_addr = NULL;
    if (at_wifi_ops && at_wifi_ops->at_get_mac_hdl)
        at_wifi_ops->at_get_mac_hdl(&mac_addr);

    if (mac_addr)
        memcpy(cfg.mac, mac_addr, ETH_ALEN);

#endif
	wise_event_loop_init(at_event_handler, NULL);
	wise_wifi_init(&cfg);

	WIFI_SET_INITED(true);
}

void at_wifi_deinit(void)
{
#if CONFIG_SUPPORT_DUAL_VIF
	int max_wlan_if = 2;
#else
	int max_wlan_if = 1;
#endif
	for (int i = 0; i < max_wlan_if; i++) {
		wise_wifi_stop(i);
		wise_wifi_set_mode(WIFI_MODE_NULL, i);
	}

	wise_event_loop_deinit();
	wise_wifi_deinit();

	WIFI_SET_INITED(false);
}

static int at_cwinit_set(int argc, char *argv[])
{
	int set = atoi(argv[AT_CMD_PARAM]);

	if (set) {
		if (!WIFI_IS_INITED())
			at_wifi_init();
	} else {
		if (WIFI_IS_INITED())
			at_wifi_deinit();
	}

	return AT_RESULT_CODE_OK;
}

ATPLUS(CWINIT, NULL, NULL, at_cwinit_set, NULL);

#ifdef CONFIG_ATCMD_AT_CWMAC
static int at_cwmac_set(int argc, char *argv[])
{
	uint8_t mac[6] = { 0 };
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	hwaddr_aton(at_strip_args(argv[AT_CMD_PARAM]), mac);

	wise_wifi_set_mac(mac, wlan_if);

	return AT_RESULT_CODE_OK;
}

static int at_cwmac_query(int argc, char *argv[])
{
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);
	uint8_t mac[6] = { 0 };

	wise_wifi_get_mac(mac, wlan_if);

#ifdef CONFIG_AT_OVER_IPC
	at_ipc_ctrl_resp(mac, 6, IPC_MODULE_WLAN);
#endif
	at_printf("%s:%02x:%02x:%02x:%02x:%02x:%02x\r\n", argv[AT_CMD_NAME],
						mac[0], mac[1],
						mac[2], mac[3],
						mac[4], mac[5]);
	return AT_RESULT_CODE_OK;
}

ATPLUS(CWMAC, NULL, at_cwmac_query, at_cwmac_set, NULL);
#endif

#ifdef CONFIG_ATCMD_AT_CWMODE
static int at_cwmode_test(int argc, char *argv[])
{
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);
	wifi_mode_t mode;

	wise_wifi_get_mode(&mode, wlan_if);
	at_printf("%s:%d\r\n", argv[AT_CMD_NAME], mode);

	return AT_RESULT_CODE_OK;
}

static int at_cwmode_query(int argc, char *argv[])
{
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);
	wifi_mode_t mode;

	wise_wifi_get_mode(&mode, wlan_if);

#ifdef CONFIG_AT_OVER_IPC
	at_ipc_ctrl_resp((uint8_t *) & mode, sizeof(wifi_mode_t), IPC_MODULE_WLAN);
#endif
	at_printf("%s:%d\r\n", argv[AT_CMD_NAME], mode);

	return AT_RESULT_CODE_OK;
}

static int at_cwmode_set(int argc, char *argv[])
{
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);
	wifi_mode_t new_mode = atoi(argv[AT_CMD_PARAM]);
	wifi_mode_t old_mode;

	wise_wifi_get_mode(&old_mode, wlan_if);

	if (old_mode == new_mode)
		return AT_RESULT_CODE_OK;
	else if (WISE_OK == wise_wifi_stop(wlan_if)) {
		wise_wifi_set_mode(new_mode, wlan_if);
		return AT_RESULT_CODE_OK;
	} else
		return AT_RESULT_CODE_FAIL;
}

ATPLUS(CWMODE, at_cwmode_test, at_cwmode_query, at_cwmode_set, NULL);
#endif							/* CONFIG_ATCMD_AT_CWMODE */

#ifdef CONFIG_ATCMD_AT_CWDHCP
static int at_cwdhcp_query(int argc, char *argv[])
{
	struct netif *netif;
	struct dhcp *dhcp;
	int enable = 0;
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);
	char *ifc = wlan_if == 0 ? "wlan0" : "wlan1";

	netif = netifapi_netif_find(ifc);
	dhcp = netif_dhcp_data(netif);
	if (dhcp != NULL && dhcp->state > DHCP_STATE_OFF)
		enable = 1;

	at_printf("%s:%d\r\n", argv[AT_CMD_NAME], enable);

	return AT_RESULT_CODE_OK;
}

static int at_cwdhcp_set(int argc, char *argv[])
{
	struct netif *netif;
	int operate = atoi(argv[AT_CMD_PARAM]);
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);
	char *ifc = wlan_if == 0 ? "wlan0" : "wlan1";

	if (s_wifi_mode[wlan_if] != WIFI_MODE_STA)
		return AT_RESULT_CODE_FAIL;

	netif = netifapi_netif_find(ifc);
	if (operate == 1)
		netifapi_dhcp_start(netif);
	else if (operate == 0)
		netifapi_dhcp_stop(netif);
	else
		return AT_RESULT_CODE_ERROR;

	return AT_RESULT_CODE_OK;
}


ATPLUS(CWDHCP, NULL, at_cwdhcp_query, at_cwdhcp_set, NULL);
#endif							/* CONFIG_ATCMD_AT_CWDHCP */

#ifdef CONFIG_ATCMD_AT_CWSTR
static int at_cwstr_exec(int argc, char *argv[])
{
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	if (wise_wifi_start(wlan_if) != WISE_OK)
		return AT_RESULT_CODE_ERROR;

	return AT_RESULT_CODE_OK;
}

ATPLUS(CWSTR, NULL, NULL, NULL, at_cwstr_exec);
#endif

#ifdef CONFIG_ATCMD_AT_CWJAP
static int at_cwjap_query(int argc, char *argv[])
{
	wise_err_t err;
	wifi_ap_record_t ap_info;
	int len;
	char *bssid;
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	memset(&ap_info, 0, sizeof(wifi_ap_record_t));
	err = wise_wifi_sta_get_ap_info(&ap_info, wlan_if);

	if (err == WISE_ERR_WIFI_NOT_CONNECT) {
		at_printf("No AP\r\n");
		return AT_RESULT_CODE_OK;
	} else if (err != WISE_OK)
		return AT_RESULT_CODE_ERROR;

	len = 2 * ETH_ALEN + 5 + 1;
	bssid = zalloc(len);
	if (bssid == NULL)
		return AT_RESULT_CODE_ERROR;

	wpa_snprintf_hex_sep(bssid, len, ap_info.bssid, ETH_ALEN, ':');

#ifdef CONFIG_AT_OVER_IPC
	at_ipc_ctrl_resp((uint8_t *) & ap_info, sizeof(wifi_ap_record_t), IPC_MODULE_WLAN);
#endif

	at_printf("%s:\"%s\",\"%s\",%d,%d,mode: 11n:%d 11ax:%d\r\n",
			argv[AT_CMD_NAME], ap_info.ssid,
			bssid, ap_info.primary,
			ap_info.rssi, ap_info.phy_11n, ap_info.phy_11ax);

	if (bssid)
		free(bssid);

	return AT_RESULT_CODE_OK;
}

static int at_cwjap_set(int argc, char *argv[])
{
	wifi_config_t wifi_config;
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	if (argc < AT_CMD_PARAM + 3)
		return AT_RESULT_CODE_FAIL;

	memset(&wifi_config, 0, sizeof(wifi_config_t));
	strncpy((char *) wifi_config.sta.ssid,
			at_strip_args(argv[AT_CMD_PARAM]),
			FIELD_SIZE(wifi_sta_config_t, ssid));

	if (argc > (AT_CMD_PARAM + 2)) {
		wifi_config.sta.alg = atoi(argv[AT_CMD_PARAM + 2]);

		if (wifi_config.sta.alg != 0) {
			strncpy((char *) wifi_config.sta.password,
					at_strip_args(argv[AT_CMD_PARAM + 1]),
					FIELD_SIZE(wifi_sta_config_t, password));
			wifi_config.sta.proto = atoi(argv[AT_CMD_PARAM + 3]);
			wifi_config.sta.pmf_mode = atoi(argv[AT_CMD_PARAM + 4]);
		} else {
			wifi_config.sta.password[0] = '\0';
		}
	}


	if (argc > (AT_CMD_PARAM + 5)) {
		strncpy((char *) wifi_config.sta.bssid,
				at_strip_args(argv[AT_CMD_PARAM + 5]),
				FIELD_SIZE(wifi_sta_config_t, bssid));
		wifi_config.sta.bssid_set = 1;
	} else
		wifi_config.sta.bssid_set = 0;

	if (wise_wifi_clean_config(WISE_IF_WIFI_STA, wlan_if) != WISE_OK)
		return AT_RESULT_CODE_ERROR;

	if (wise_wifi_set_config(WISE_IF_WIFI_STA, &wifi_config, wlan_if, NULL) !=
		WISE_OK)
		return AT_RESULT_CODE_ERROR;

	return AT_RESULT_CODE_OK;
}

ATPLUS(CWJAP, NULL, at_cwjap_query, at_cwjap_set, NULL);
#endif							/* CONFIG_ATCMD_AT_CWJAP */

#ifdef CONFIG_ATCMD_AT_CWSTOP
static int at_cwstop_exec(int argc, char *argv[])
{
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	if (wise_wifi_stop(wlan_if) != WISE_OK)
		return AT_RESULT_CODE_ERROR;

	return AT_RESULT_CODE_OK;
}

ATPLUS(CWSTOP, NULL, NULL, NULL, at_cwstop_exec);
#endif

#ifdef CONFIG_ATCMD_AT_CWSCAP
static int at_cwcap_exec(int argc, char *argv[])
{
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	if (wise_wifi_connect(wlan_if) != WISE_OK)
		return AT_RESULT_CODE_ERROR;

	return AT_RESULT_CODE_OK;
}

ATPLUS(CWCAP, NULL, NULL, NULL, at_cwcap_exec);
#endif

#ifdef CONFIG_ATCMD_AT_CWQAP
static int at_cwqap_exec(int argc, char *argv[])
{
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	if (wise_wifi_disconnect(wlan_if) != WISE_OK)
		return AT_RESULT_CODE_ERROR;

	return AT_RESULT_CODE_OK;
}

ATPLUS(CWQAP, NULL, NULL, NULL, at_cwqap_exec);
#endif							/* CONFIG_ATCMD_AT_CWQAP */

#if CONFIG_ATCMD_AT_CWLIF
static int at_cwlif_exec(int argc, char *argv[])
{
#if CONFIG_LWIP_IPV6
	printf("SAP does not support IPv6 yet\n");
	return AT_RESULT_CODE_FAIL;
#else
	struct netif *netif;
	struct dhcps *dhcps;
	struct dhcps_client *cl;
	int i = 0;
	wifi_sta_list_t sta_list;

	memset(&sta_list, 0, sizeof(wifi_sta_list_t));

	netif = netifapi_netif_find("wlan1");
	if (netif == NULL) {
		return AT_RESULT_CODE_FAIL;
	}

	if (dhcps_client_id == -1
		|| (dhcps =
			netif_get_client_data(netif, dhcps_client_id)) == NULL) {
		return AT_RESULT_CODE_FAIL;
	}

	wise_wifi_ap_get_sta_list(&sta_list);
	at_printf("%s:", argv[AT_CMD_NAME]);

	while (i < sta_list.num) {
		list_for_each_entry(cl, &dhcps->clist[BOUND], list) {
			if (!memcmp(sta_list.sta[i].mac, cl->hwaddr, 6)) {
				at_printf("%u.%u.%u.%u,%02x:%02x:%02x:%02x:%02x:%02x\n",
						ip4_addr1(&cl->ipaddr),
						ip4_addr2(&cl->ipaddr),
						ip4_addr3(&cl->ipaddr),
						ip4_addr4(&cl->ipaddr),
						cl->hwaddr[0], cl->hwaddr[1],
						cl->hwaddr[2], cl->hwaddr[3],
						cl->hwaddr[4], cl->hwaddr[5]);
				break;
			}
		}
		i++;
	}
	return AT_RESULT_CODE_OK;
#endif
}

ATPLUS(CWLIF, NULL, NULL, NULL, at_cwlif_exec);
#endif							/* CONFIG_ATCMD_AT_CWLIF */

#if CONFIG_ATCMD_AT_CWLAP
#define ENCRYPT_FLAGS 		BIT(0)
#define SSID_FLAGS 			BIT(1)
#define RSSI_FLAGS 			BIT(2)
#define BSSID_FLAGS 			BIT(3)
#define CHANNEL_FLAGS 		BIT(4)
//#define FREQ_P_FLAGS		BIT(5)
//#define FREQ_S_FLAGS		BIT(6)
#define PK_FLAGS 				BIT(5)
#define GK_FLAGS 			BIT(6)
#define BGN_FLAGS 			BIT(7)
#define WPS_FLAGS 			BIT(8)

static int ordered = 0;
static int show_flags = 0x1FF;

/* AP list will show by at_cwlap_query */

#define CONFIG_SHOW_AP_LIST 0

#if CONFIG_SHOW_AP_LIST
static int show_ap_list(void)
{
	uint16_t ap_num;
	wifi_ap_record_t *ap_rec, *temp;
	wifi_ap_record_t **pap_rec;
	s8 buf[100] = { 0 };
	s8 *head, *pos;
	int i, j;
	int ret;

	WISE_ERROR_CHECK(wise_wifi_scan_get_ap_num(&ap_num));
	if (!ap_num) {
		return AT_RESULT_CODE_FAIL;
	}
	ap_rec =
		(wifi_ap_record_t *) zalloc(ap_num * sizeof(wifi_ap_record_t));
	WISE_ERROR_CHECK(wise_wifi_scan_get_ap_records(0, &ap_num, ap_rec));

	pap_rec = malloc(ap_num * sizeof(int));
	if (!pap_rec) {
		free(ap_rec);
		return AT_RESULT_CODE_FAIL;
	}

	for (i = 0; i < ap_num; i++)
		pap_rec[i] = &ap_rec[i];

	if (ordered == 1) {
		for (i = 0; i < ap_num; i++) {
			for (j = 0; j < ap_num - 1 - i; j++)
				if (pap_rec[j]->rssi < pap_rec[j + 1]->rssi) {
					temp = pap_rec[j];
					pap_rec[j] = pap_rec[j + 1];
					pap_rec[j + 1] = temp;
				}
		}
	}

	for (i = 0; i < ap_num; i++) {
		memset(buf, 0, sizeof(buf));
		pos = head = buf;

		ret = sprintf(pos, "+CWLAP:(");
		pos += ret;
		if (show_flags & ENCRYPT_FLAGS) {
			ret =
				sprintf(pos, "%d,",
						pap_rec[i]->authmode ==
						WIFI_AUTH_OPEN ? 0 : pap_rec[i]->authmode ==
						WIFI_AUTH_WEP ? 1 : pap_rec[i]->authmode ==
						WIFI_AUTH_WPA_PSK ? 2 : pap_rec[i]->authmode ==
						WIFI_AUTH_WPA2_PSK ? 3 : pap_rec[i]->authmode ==
						WIFI_AUTH_WPA_WPA2_PSK ? 4 : pap_rec[i]->
						authmode == WIFI_AUTH_WPA2_ENTERPRISE ? 5 : 4);
			pos += ret;
		}
		if (show_flags & SSID_FLAGS) {
			ret = sprintf(pos, "\"%s\",", (const char *) pap_rec[i]->ssid);
			pos += ret;
		}
		if (show_flags & RSSI_FLAGS) {
			ret = sprintf(pos, "%d,", pap_rec[i]->rssi);
			pos += ret;
		}
		if (show_flags & BSSID_FLAGS) {
			ret =
				sprintf(pos, "\"%2x:%02x:%02x:%02x:%02x:%02x\",",
						MAC2STR(pap_rec[i]->bssid));
			pos += ret;
		}
		if (show_flags & CHANNEL_FLAGS) {
			ret = sprintf(pos, "%d,", pap_rec[i]->primary);
			pos += ret;
		}
/*
		if(show_flags & FREQ_P_FLAGS){
			ret = sprintf(pos,"freq offset,");
			pos += ret;
		}
		if(show_flags & FREQ_S_FLAGS){
			ret = sprintf(pos,"freq calibration,");
			pos += ret;
		}
*/
		if (show_flags & PK_FLAGS) {
			ret =
				sprintf(pos, "%d,",
						pap_rec[i]->pairwise_cipher ==
						WIFI_CIPHER_TYPE_NONE ? 0 : pap_rec[i]->
						pairwise_cipher ==
						WIFI_CIPHER_TYPE_WEP40 ? 1 : pap_rec[i]->
						pairwise_cipher ==
						WIFI_CIPHER_TYPE_WEP104 ? 2 : pap_rec[i]->
						pairwise_cipher ==
						WIFI_CIPHER_TYPE_TKIP ? 3 : pap_rec[i]->
						pairwise_cipher ==
						WIFI_CIPHER_TYPE_CCMP ? 4 : pap_rec[i]->
						pairwise_cipher ==
						WIFI_CIPHER_TYPE_TKIP_CCMP ? 5 : 6);
			pos += ret;
		}
		if (show_flags & GK_FLAGS) {
			ret =
				sprintf(pos, "%d,",
						pap_rec[i]->group_cipher ==
						WIFI_CIPHER_TYPE_NONE ? 0 : pap_rec[i]->
						group_cipher ==
						WIFI_CIPHER_TYPE_WEP40 ? 1 : pap_rec[i]->
						group_cipher ==
						WIFI_CIPHER_TYPE_WEP104 ? 2 : pap_rec[i]->
						group_cipher ==
						WIFI_CIPHER_TYPE_TKIP ? 3 : pap_rec[i]->
						group_cipher ==
						WIFI_CIPHER_TYPE_CCMP ? 4 : pap_rec[i]->
						group_cipher ==
						WIFI_CIPHER_TYPE_TKIP_CCMP ? 5 : 6);
			pos += ret;
		}
		if (show_flags & BGN_FLAGS) {
			ret =
				sprintf(pos, "%d,",
						pap_rec[i]->phy_11n << 2 | pap_rec[i]->
						phy_11g << 1 | pap_rec[i]->phy_11b);
			pos += ret;
		}
		if (show_flags & WPS_FLAGS) {
			ret = sprintf(pos, "%d", pap_rec[i]->wps ? 1 : 0);
			pos += ret;
		}
		ret = sprintf(pos, ")");
		at_printf("\r\n%s", head);
	}

	free(pap_rec);
	free(ap_rec);

	return AT_RESULT_CODE_OK;
}
#endif

uint16_t g_qap_num = 0;
#define AT_CWLAP_GET_QUERY_AP_NUM() g_qap_num
#define AT_CWLAP_SET_QAP_NUM(x) { \
		g_qap_num = x; \
	}

static int at_cwlap_set(int argc, char *argv[])
{
	wifi_scan_config_t wif_scan_config;
	char *ssid, *bssid, *param;
	char *value;
	int i = 0;
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);
	int ret = AT_RESULT_CODE_OK;

	memset(&wif_scan_config, 0, sizeof(wifi_scan_config_t));

	if (argc > AT_CMD_NAME) {
		for (i = AT_CMD_PARAM; i < argc; i++) {
			param = at_strip_args(argv[i]);
			if (strncmp(param, "ssid=", 5) == 0) {
				ssid = param + 5;

				if (strlen(ssid)) {
					wif_scan_config.ssid = malloc(strlen(ssid) + 1);
					memcpy(wif_scan_config.ssid, ssid, strlen(ssid) + 1);
				}
			} else if (strncmp(param, "bssid=", 6) == 0) {
				bssid = param + 6;

				if (strlen(bssid)) {
					wif_scan_config.bssid = malloc(strlen(bssid) + 1);
					memcpy(wif_scan_config.bssid, bssid,
							strlen(bssid) + 1);
				}
			} else if (strncmp(param, "ch=", 3) == 0) {
				value = param + 3;

				if (strlen(value)) {
					wif_scan_config.channel = atoi(value);
				}
			} else if (strncmp(param, "scantype=", 9) == 0) {
				value = param + 9;

				if (strlen(value)) {
					wif_scan_config.scan_type = atoi(value);
				}
			} else if (strncmp(param, "actmin=", 7) == 0) {
				value = param + 7;

				if (strlen(value)
					&& (wif_scan_config.scan_type ==
						WIFI_SCAN_TYPE_ACTIVE)) {
					wif_scan_config.scan_time.active.min = atoi(value);
				}
			} else if (strncmp(param, "actmax=", 7) == 0) {
				value = param + 7;

				if (strlen(value)) {
					if (wif_scan_config.scan_type == WIFI_SCAN_TYPE_ACTIVE) {
						wif_scan_config.scan_time.active.min = atoi(value);
						wif_scan_config.scan_time.active.max = atoi(value);
					} else
						wif_scan_config.scan_time.passive = atoi(value);
				}
			} else if (strncmp(param, "num=", 4) == 0) {
				value = param + 4;

				if (strlen(value))
					AT_CWLAP_SET_QAP_NUM(atoi(value));
			}
		}
	}

	if (WISE_OK != wise_wifi_scan_start(&wif_scan_config, true, wlan_if)) {
		ret = AT_RESULT_CODE_FAIL;
		goto exit;
	}

#if CONFIG_SHOW_AP_LIST
	show_ap_list();
#endif

exit:
	if (wif_scan_config.ssid) {
		free(wif_scan_config.ssid);
	}
	if (wif_scan_config.bssid) {
		free(wif_scan_config.bssid);
	}

	return ret;
}

static int at_cwlap_exec(int argc, char *argv[])
{
	wifi_scan_config_t wif_scan_config;
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	memset(&wif_scan_config, 0, sizeof(wifi_scan_config_t));
	if (WISE_OK != wise_wifi_scan_start(&wif_scan_config, true, wlan_if))
		return AT_RESULT_CODE_FAIL;

#if CONFIG_SHOW_AP_LIST
	show_ap_list();
#endif

	return AT_RESULT_CODE_OK;
}

uint16_t g_ap_idx = 0;
uint16_t g_ap_total_num = 0;

#define AT_CWLAP_GET_AP_IDX() g_ap_idx
#define AT_CWLAP_SET_AP_IDX(x) { \
		g_ap_idx += x; \
		if (g_ap_idx >= g_ap_total_num) \
			g_ap_idx = 0; \
	}
#define AT_CWLAP_SET_AP_NUM(x) { \
		g_ap_total_num = x; \
	}

static int at_cwlap_query(int argc, char *argv[])
{
	int i;
	uint8_t ap_idx;
	uint16_t qap_num;
	uint16_t ap_total_num;

	wifi_ap_record_t *ap_rec = NULL;
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	if (wlan_if)
		return AT_RESULT_CODE_ERROR;

	ap_idx = AT_CWLAP_GET_AP_IDX();

	wise_wifi_scan_get_ap_num(&ap_total_num);

	if (ap_idx == 0) {
		AT_CWLAP_SET_AP_NUM(ap_total_num);
	}

	if (AT_CWLAP_GET_QUERY_AP_NUM() != 0)
		qap_num = AT_CWLAP_GET_QUERY_AP_NUM();
	else
		qap_num = ap_total_num;

#if 0
	printf("%d per query, total %d AP\n", qap_num, ap_total_num);
#endif

	if (qap_num > ap_total_num)
		qap_num = ap_total_num;

	ap_rec =
		(wifi_ap_record_t *) zalloc(qap_num * sizeof(wifi_ap_record_t));

	wise_wifi_scan_get_ap_records(ap_idx, &qap_num, ap_rec);
#if 0
	printf("ap %d to %d\n", ap_idx, ap_idx + qap_num);
#endif

#ifdef CONFIG_WPA_SUPPLICANT_NO_SCAN_RES
	if (ap_idx + qap_num >= ap_total_num)
		wise_wifi_flush_bss();

#endif

#ifdef CONFIG_AT_OVER_IPC
	at_ipc_ctrl_resp((uint8_t *) ap_rec,
					 qap_num * sizeof(wifi_ap_record_t), IPC_MODULE_WLAN);
#endif

#ifdef CONFIG_AT_OVER_SCDC
    //currently max transfer size should be less than 1500
    //FIXME
    if (qap_num * sizeof(wifi_ap_record_t) > 1400) {
        if (at_wifi_ops && at_wifi_ops->at_scan_result_resp)
            at_wifi_ops->at_scan_result_resp((uint8_t *) ap_rec, 10 * sizeof(wifi_ap_record_t));
	} else {
        if (at_wifi_ops && at_wifi_ops->at_scan_result_resp)
            at_wifi_ops->at_scan_result_resp((uint8_t *) ap_rec, qap_num * sizeof(wifi_ap_record_t));
	}
#endif

	for (i = 0; i < qap_num; i++) {
		at_printf("%s:ap[%d] = %s authmode = %d b:%d g:%d n:%d ax:%d\n",
				argv[AT_CMD_NAME], i + ap_idx, ap_rec[i].ssid, ap_rec[i].authmode,
				ap_rec[i].phy_11b, ap_rec[i].phy_11g, ap_rec[i].phy_11n,
				ap_rec[i].phy_11ax);
	}

	AT_CWLAP_SET_AP_IDX(qap_num);

	if (ap_rec)
		free(ap_rec);
	return AT_RESULT_CODE_OK;
}

ATPLUS(CWLAP, NULL, at_cwlap_query, at_cwlap_set, at_cwlap_exec);
#endif							/* CONFIG_ATCMD_AT_CWLAP */

#ifdef CONFIG_ATCMD_AT_CWLAPOPT
static int at_cwlapopt_set(int argc, char *argv[])
{
	if (argc != (AT_CMD_PARAM + 2))
		return AT_RESULT_CODE_FAIL;
	else {
		ordered = atoi(argv[AT_CMD_PARAM]);
		show_flags = atoi(argv[AT_CMD_PARAM + 1]);

		return AT_RESULT_CODE_OK;
	}
}

ATPLUS(CWLAPOPT, NULL, NULL, at_cwlapopt_set, NULL);
#endif							/* CONFIG_ATCMD_AT_CWLAPOPT */

#ifdef CONFIG_ATCMD_AT_CWCOUNTRY
static int at_cwcountry_set(int argc, char *argv[])
{
	wifi_country_t country;
	memset(&country, 0, sizeof(wifi_country_t));
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	if (argc != 3)
		return AT_RESULT_CODE_FAIL;

	if (wlan_if)
		return AT_RESULT_CODE_ERROR;

	country.schan = 1;
	country.policy = 0;
	if (strncmp(argv[AT_CMD_PARAM], "US", 2) == 0) {
		country.nchan = 11;
		country.cc[0] = 'U';
		country.cc[1] = 'S';
	} else if (strncmp(argv[AT_CMD_PARAM], "CA", 2) == 0) {
		country.nchan = 11;
		country.cc[0] = 'C';
		country.cc[1] = 'A';
	} else if (strncmp(argv[AT_CMD_PARAM], "JP", 2) == 0) {
		country.nchan = 14;
		country.cc[0] = 'J';
		country.cc[1] = 'P';
	} else if (strncmp(argv[AT_CMD_PARAM], "KR", 2) == 0) {
		country.nchan = 11;
		country.cc[0] = 'K';
		country.cc[1] = 'R';
	} else if (strncmp(argv[AT_CMD_PARAM], "CN", 2) == 0) {
		country.nchan = 13;
		country.cc[0] = 'C';
		country.cc[1] = 'N';
	} else {
		printf("%s not define", argv[AT_CMD_PARAM]);
		return AT_RESULT_CODE_FAIL;
	}

	return wise_wifi_set_country(&country);
}

static int at_cwcountry_query(int argc, char *argv[])
{
	wifi_country_t country;

	memset(&country, 0, sizeof(country));
	wise_wifi_get_country(&country);

#ifdef CONFIG_AT_OVER_IPC
	at_ipc_ctrl_resp((uint8_t *) & country, sizeof(wifi_country_t), IPC_MODULE_WLAN);
#endif

	at_printf("%s:Country:%s support [%d] channels", argv[AT_CMD_NAME], country.cc, country.nchan);
	return AT_RESULT_CODE_OK;
}

ATPLUS(CWCOUNTRY, NULL, at_cwcountry_query, at_cwcountry_set, NULL);
#endif

#ifdef CONFIG_ATCMD_AT_CWPWR

static int at_cwpwr_query(int argc, char *argv[])
{
	int8_t power = IEEE80211_TXPOWER_MIN;
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	if (wlan_if) {
		printf("only support STA mode\n");
#ifdef CONFIG_AT_OVER_IPC
		at_ipc_ctrl_resp((uint8_t *) & power, sizeof(int8_t), IPC_MODULE_WLAN);
#endif
		return AT_RESULT_CODE_ERROR;
	}

	if (wise_wifi_get_max_tx_power(&power, wlan_if) != WISE_OK) {
		return AT_RESULT_CODE_ERROR;
	}

#ifdef CONFIG_AT_OVER_IPC
	at_ipc_ctrl_resp((uint8_t *) & power, sizeof(int8_t), IPC_MODULE_WLAN);
#endif

	at_printf("%s:max limited power = %d", argv[AT_CMD_NAME], power);
	return AT_RESULT_CODE_OK;
}

static int at_cwpwr_set(int argc, char *argv[])
{
	int8_t power;
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	if (argc != 3)
		return AT_RESULT_CODE_FAIL;

	if (wlan_if)
		return AT_RESULT_CODE_ERROR;

	/*
	 * valid range need to define once Xiaohu rf ready
	 */
	if ((atoi(argv[AT_CMD_PARAM]) > 20)
		|| (atoi(argv[AT_CMD_PARAM]) < -20))
		return AT_RESULT_CODE_ERROR;

	power = atoi(argv[AT_CMD_PARAM]);

#if 0
	printf("%s max power limited = %d \n", __func__, power);
#endif

	return wise_wifi_set_max_tx_power(power, wlan_if);
}


ATPLUS(CWPWR, NULL, at_cwpwr_query, at_cwpwr_set, NULL);
#endif


#ifdef CONFIG_ATCMD_AT_CWPS

static int at_cwps_query(int argc, char *argv[])
{
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	if (wlan_if)
		return AT_RESULT_CODE_ERROR;

	const char *ps_type[WIFI_PS_TYPE_MAX] = {
		"WIFI_PS_NONE",
		"WIFI_PS_MAX_MODEM",
		"WIFI_PS_MIN_MODEM",
	};
	wifi_ps_type_t type = -1;
	if (wise_wifi_get_ps(&type) != WISE_OK)
		return AT_RESULT_CODE_FAIL;

#ifdef CONFIG_AT_OVER_IPC
	at_ipc_ctrl_resp((uint8_t *) & type, sizeof(wifi_ps_type_t), IPC_MODULE_WLAN);
#endif

	at_printf("%s:ps type = %s\n", argv[AT_CMD_NAME], ps_type[type]);
	return AT_RESULT_CODE_OK;
}

static int at_cwps_set(int argc, char *argv[])
{
	wifi_ps_type_t type;
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	if (wlan_if)
		return AT_RESULT_CODE_ERROR;

	if (argc != 3)
		return AT_RESULT_CODE_FAIL;

	if ((atoi(argv[AT_CMD_PARAM]) > WIFI_PS_MIN_MODEM)
		|| (atoi(argv[AT_CMD_PARAM]) < WIFI_PS_NONE))
		return AT_RESULT_CODE_ERROR;

	type = atoi(argv[AT_CMD_PARAM]);

	return wise_wifi_set_ps(type);
}

ATPLUS(CWPS, NULL, at_cwps_query, at_cwps_set, NULL);
#endif

#ifdef CONFIG_ATCMD_AT_CWSSCN
static int at_cwsscn_exec(int argc, char *argv[])
{
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	if (wlan_if)
		return AT_RESULT_CODE_ERROR;

	wise_wifi_scan_stop();

	return AT_RESULT_CODE_OK;
}

ATPLUS(CWSSCN, NULL, NULL, NULL, at_cwsscn_exec);
#endif							/* CONFIG_ATCMD_AT_CWSSCN */


#ifdef CONFIG_ATCMD_AT_CWLAPN
static int at_cwlapn_query(int argc, char *argv[])
{
	uint16_t ap_num;

	wise_wifi_scan_get_ap_num(&ap_num);

#ifdef CONFIG_AT_OVER_IPC
	at_ipc_ctrl_resp((uint8_t *) & ap_num, sizeof(uint16_t), IPC_MODULE_WLAN);
#endif

	at_printf("%s:ap_num = %d\n", argv[AT_CMD_NAME], ap_num);
	return AT_RESULT_CODE_OK;
}

ATPLUS(CWLAPN, NULL, at_cwlapn_query, NULL, NULL);
#endif							/* CONFIG_ATCMD_AT_CWLAPN */

#if CONFIG_ATCMD_AT_CWDHCPS
static int at_cwdhcps_query(int argc, char *argv[])
{
	struct netif *netif;
	struct dhcps *dhcps;
	char start[16] = { 0 }, end[16] = { 0 };
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);
	char *ifc = wlan_if == 0 ? "wlan0" : "wlan1";

	if (s_wifi_mode[wlan_if] != WIFI_MODE_AP)
		return AT_RESULT_CODE_FAIL;

	netif = netifapi_netif_find(ifc);
	dhcps = netif_get_client_data(netif, dhcps_client_id);

	if (!dhcps)
		return AT_RESULT_CODE_FAIL;

	ipaddr_ntoa_r(&dhcps->start_ip, start, 16);
	ipaddr_ntoa_r(&dhcps->end_ip, end, 16);
	at_printf("%s:%d,\"%s\",\"%s\"\r\n", argv[AT_CMD_NAME], dhcps->lease_time, start, end);

	return AT_RESULT_CODE_OK;
}

static int at_cwdhcps_set(int argc, char *argv[])
{
#if CONFIG_LWIP_IPV6
	printf("SAP does not support IPv6 yet\n");
	return AT_RESULT_CODE_FAIL;
#else
	struct dhcps_config cfg;
	struct netif *netif;
	int operate = atoi(argv[AT_CMD_PARAM]);
	int lease_time = atoi(argv[AT_CMD_PARAM + 1]);
	ip_addr_t start, end;
	int ret = AT_RESULT_CODE_OK;
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);
	char *ifc = wlan_if == 0 ? "wlan0" : "wlan1";

	if (s_wifi_mode[wlan_if] != WIFI_MODE_AP)
		return AT_RESULT_CODE_FAIL;

	netif = netifapi_netif_find(ifc);

	if (operate == 1) {
		if (ipaddr_aton(at_strip_args(argv[AT_CMD_PARAM + 2]), &start)
			&& ipaddr_aton(at_strip_args(argv[AT_CMD_PARAM + 3]), &end)) {
			cfg.start = start;
			cfg.end = end;
			cfg.lease_to = lease_time;
			netifapi_dhcps_configure(netif, &cfg);
		} else
			ret = AT_RESULT_CODE_ERROR;
	} else if (operate == 0) {
		cfg.start.addr = INADDR_ANY;
		cfg.end.addr = INADDR_ANY;
		cfg.lease_to = DHCPS_LEASE_TIME_DEF;
		netifapi_dhcps_configure(netif, &cfg);	//set as default
	} else
		ret = AT_RESULT_CODE_ERROR;

	netifapi_dhcps_start(netif);

	return ret;
#endif
}

ATPLUS(CWDHCPS, NULL, at_cwdhcps_query, at_cwdhcps_set, NULL);
#endif							/* CONFIG_ATCMD_AT_CWDHCPS */

#if CONFIG_ATCMD_AT_CWSAP
static wifi_config_t saved_ap_config = {
	.ap = {
			.ssid = "wise-ap",
			.password = "12345678",
			.ssid_len = 8,
			.channel = 1,
			.alg = AUTH_ALG_CCMP,
			.proto = 2,
			.ssid_hidden = 0,
			.max_connection = 4,
			.beacon_interval = 100 },
};

static int at_cwsap_query(int argc, char *argv[])
{

	at_printf("%s:\"%s\",\"%s\",%d,%d,%d,%d",
			argv[AT_CMD_NAME], saved_ap_config.ap.ssid, saved_ap_config.ap.password,
			saved_ap_config.ap.channel, saved_ap_config.ap.alg,
			saved_ap_config.ap.max_connection,
			saved_ap_config.ap.ssid_hidden);

#ifdef CONFIG_AT_OVER_IPC
	at_ipc_ctrl_resp((uint8_t *) & saved_ap_config,
					 sizeof(saved_ap_config), IPC_MODULE_WLAN);
#endif

	return AT_RESULT_CODE_OK;
}

static int at_cwsap_set(int argc, char *argv[])
{
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);
	if (argc < 5 || argc > 8)
		return AT_RESULT_CODE_FAIL;

	wifi_config_t ap_config;
	memset(&ap_config, 0, sizeof(wifi_config_t));
	ap_config.ap.max_connection = 4;
	ap_config.ap.ssid_hidden = 0;

	strncpy((char *) ap_config.ap.ssid, at_strip_args(argv[AT_CMD_PARAM]),
			FIELD_SIZE(wifi_ap_config_t, ssid));
	strncpy((char *) ap_config.ap.password,
			at_strip_args(argv[AT_CMD_PARAM + 1]),
			FIELD_SIZE(wifi_ap_config_t, password));

	ap_config.ap.channel = atoi(argv[AT_CMD_PARAM + 2]);

	if (argc >= (AT_CMD_PARAM + 3))
		ap_config.ap.alg = atoi(argv[(AT_CMD_PARAM + 3)]);
	if (argc >= (AT_CMD_PARAM + 4))
		ap_config.ap.proto = atoi(argv[(AT_CMD_PARAM + 4)]);
	if (argc >= (AT_CMD_PARAM + 5))
		ap_config.ap.max_connection = atoi(argv[AT_CMD_PARAM + 5]);
	if (argc >= (AT_CMD_PARAM + 6))
		ap_config.ap.ssid_hidden = atoi(argv[AT_CMD_PARAM + 6]);

	if (wise_wifi_clean_config(WISE_IF_WIFI_AP, wlan_if) != WISE_OK)
		return AT_RESULT_CODE_ERROR;

	// ret dont know if setting is ok or not, we haven't check reply msg,.
	if (wise_wifi_set_config(WISE_IF_WIFI_AP, &ap_config, wlan_if, NULL) !=
		WISE_OK)
		return AT_RESULT_CODE_ERROR;

	if (wise_wifi_start(wlan_if) != WISE_OK)
		return AT_RESULT_CODE_ERROR;

	memcpy(&saved_ap_config, &ap_config, sizeof(wifi_config_t));

	return AT_RESULT_CODE_OK;
}

ATPLUS(CWSAP, NULL, at_cwsap_query, at_cwsap_set, NULL);
#endif							/* CONFIG_ATCMD_AT_CWSAP */

#if CONFIG_ATCMD_AT_CWSMART
extern int do_airkiss(int argc, char *argv[]);
extern int g_exit;
#define CONFIG_SMART_STACK_SIZE 1024
osThreadAttr_t smart_thread_attr = {
	.name = "smartconfig",
	.stack_size = 2 * CONFIG_SMART_STACK_SIZE,
	.priority = osPriorityNormal,
};

osThreadId_t SMART_ID = NULL;

static void smart_thread(void *param)
{
	do_airkiss(0, NULL);
	SMART_ID = NULL;
	osThreadExit();
}

static int at_cwstartsmart_set(int argc, char *argv[])
{
	if (SMART_ID == NULL) {
		if (!
			(SMART_ID =
			 osThreadNew(smart_thread, NULL, &smart_thread_attr)))
			return AT_RESULT_CODE_FAIL;
		return AT_RESULT_CODE_OK;
	} else
		return AT_RESULT_CODE_FAIL;
}

static int at_cwstartsmart_exec(int argc, char *argv[])
{
	if (SMART_ID == NULL) {
		if (!
			(SMART_ID =
			 osThreadNew(smart_thread, NULL, &smart_thread_attr)))
			return AT_RESULT_CODE_FAIL;
		return AT_RESULT_CODE_OK;
	} else
		return AT_RESULT_CODE_FAIL;
}

ATPLUS(CWSTARTSMART, NULL, NULL, at_cwstartsmart_set,
		at_cwstartsmart_exec);

static int at_cwstopsmart_exec(int argc, char *argv[])
{
	if (SMART_ID)
		g_exit = 1;
	return AT_RESULT_CODE_OK;
}

ATPLUS(CWSTOPSMART, NULL, NULL, NULL, at_cwstopsmart_exec);
#endif							/* CONFIG_ATCMD_ATSMART */

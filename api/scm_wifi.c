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
#include <hal/kmem.h>
#include <cli.h>

#include "kernel.h"

#include "wise_wifi.h"
#include "wise_event_loop.h"
#include "wise_wpas.h"
#include "bss.h"

#include "FreeRTOS.h"
#include "task.h"
#include "driver_i.h"
#ifdef CONFIG_LWIP_DHCPS
#include "dhcps.h"
#endif
#include "compat_if.h"
#include "if_media.h"
#ifdef CONFIG_SUPPORT_SCDC
#include "fweh.h"
#endif
#include "net80211/ieee80211_var.h"
#include "libifconfig.h"

#include "scm_wifi.h"
#include "scm_log.h"

static bool g_wifi_inited;

#define WIFI_IS_INITED() g_wifi_inited
#define WIFI_SET_INITED(s) { \
	g_wifi_inited = s; \
}

#define SCM_API_TAG "SCM_API"

#define SECURITY_OPEN 0
#define SECURITY_TKIP 2
#define SECURITY_CCMP 3
#define SECURITY_CCMP_256 4
#define SECURITY_SAE  6
#define SECURITY_UNKNOWN 0xFF

#define WEP40_LEN 5
#define WEP40_HEX_LEN 10
#define WEP104_LEN 13
#define WEP104_HEX_LEN 26

#define PSK_MIN_LEN 8
#define PSK_MAX_LEN 63

#define STA_MIN_SLEEP 100
#define STA_MAX_SLEEP 3000

#define IS_VALID_SSID_LEN(n) ( n > 0 && n <= SCM_WIFI_MAX_SSID_LEN)

#define IS_VALID_WEP_KEY_LEN(n) (n == WEP40_LEN || n == WEP40_HEX_LEN || \
																	n == WEP104_LEN || n == WEP104_HEX_LEN)

#define IS_WEP_KEY_HEX_STR(n) (n == WEP40_HEX_LEN || n == WEP104_HEX_LEN)
#define IS_VALID_PSK_LEN(n) (n >= PSK_MIN_LEN && n <= PSK_MAX_LEN)

#define IS_VALID_CH_NUM(n) (n > 0 && n <= WPA_CHANNEL_MAX_OTHERS)
#define IS_VALID_HIDDEN_NUM(n) (n == 0 || n == 1)

#ifdef CONFIG_API_MONITOR
static bool g_promisc_enabled = false;
#define PROMISC_IS_ENABLED() g_promisc_enabled
#define PROMISC_SET_ENABLED(x) { \
	g_promisc_enabled = x; \
}
#endif

wise_err_t default_evt_handler(void *ctx, system_event_t * event)
{
	return WISE_OK;
}

static int scm_wifi_init (void)
{
	uint8_t *mac_addr = NULL;
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	scm_wifi_get_wlan_mac(&mac_addr, 0);

	if (mac_addr)
		memcpy(cfg.mac, mac_addr, ETH_ALEN);

	if (!wise_event_get_init_flag())
		wise_event_loop_init(default_evt_handler, NULL);

	if (wise_wifi_init(&cfg) != WISE_OK)
		return WISE_FAIL;

	WIFI_SET_INITED(TRUE);

	return WISE_OK;
}

void scm_wifi_event_send(void *event, size_t size)
{
#ifdef CONFIG_SUPPORT_SCDC
	fweh_event_tx(event, sizeof(system_event_t));
#endif
}

struct netif * scm_wifi_get_netif(int idx)
{
	struct netif *netif;
	wise_err_t ret;

	ret = wise_wifi_get_netif(idx, &netif);
	assert(!ret);

	return netif;
}

void scm_wifi_get_wlan_mac(uint8_t **mac_addr, int idx)
{
	struct device *dev = wlandev(0);
	struct ieee80211vap *vap;

	assert(dev);

	vap = wlan_get_vap(dev, idx);

	assert(vap);

	*mac_addr = &vap->iv_myaddr[0];
}

#if 0
static bool isValidIpAddress(char *addr, bool nm_flag)
{
	int num, i, len;
	char *ch;
	char chkaddr[16] = {0};

	/* counting number of quads present in a given IP address */
	int quadsCnt=0;

	memcpy(chkaddr, (char *)addr, 16);

	len = strlen(chkaddr);

	/*  Check if the string is valid */
	if (len<7 || len>15)
		return false;

	ch = strtok(chkaddr, ".");

	while (ch != NULL) {
		quadsCnt++;
		num = i = 0;

		/* Get the current token and convert to an integer value */
		while (ch[i] != '\0') {
			num = num*10;
			num = num+(ch[i]-'0');
			i++;
		}

		if (num < 0 || num >= 256)
			return false;

		if (quadsCnt == 1 && num == 0)
			return false;

		if (!nm_flag && (quadsCnt == 4 && num == 0))
			return false;

		ch = strtok(NULL, ".");
	}

	/*  Check the address string, should be n.n.n.n format */
	if (quadsCnt != 4)
		return false;

	/* Looks like a valid IP address */
	return true;
}
#endif

static void scm_wifi_print_ip(const char* name, ip4_addr_t * addr)
{
	SCM_INFO_LOG(SCM_API_TAG, "%s: %u.%u.%u.%u\n",
				name,
				ip4_addr1(addr),
				ip4_addr2(addr),
				ip4_addr3(addr),
				ip4_addr4(addr));
}

/* get netif's ip, gateway and netmask */
int scm_wifi_get_ip(const char *ifname,
                                        char *ip,
                                        int ip_len,
                                        char *nm,
                                        int nm_len,
                                        char *gw,
                                        int gw_len)
{
	wifi_ip_info_t ip_info = {0};
	wifi_interface_t interface;
    int len;

	interface = (strcmp(ifname, "wlan0") == 0)? WIFI_IF_STA : WIFI_IF_AP;

	if (wise_wifi_get_ip_info(interface, &ip_info)) {
		return WISE_FAIL;
	}

    if (ip && ip_len) {
        len = min(ip_len, IP4ADDR_STRLEN_MAX);
        strncpy(ip, ip4addr_ntoa(&ip_info.ip), len);
    }
    if (nm && nm_len) {
        len = min(nm_len, IP4ADDR_STRLEN_MAX);
        strncpy(nm, ip4addr_ntoa(&ip_info.nm), len);
    }
    if (gw && gw_len) {
        len = min(gw_len, IP4ADDR_STRLEN_MAX);
        strncpy(gw, ip4addr_ntoa(&ip_info.gw), len);
    }

	return WISE_OK;
}

/* set netif's ip, gateway and netmask */
int scm_wifi_set_ip(const char *ifname,
										const char *ip,
										const char *nm,
										const char *gw)
{
	wifi_ip_info_t ip_info = {0};
	wifi_interface_t interface;

	if (!(strcmp(ifname, "wlan0") == 0 || strcmp(ifname, "wlan1") == 0)) {
		SCM_ERR_LOG(SCM_API_TAG,"wrong ifname\n");
		return WISE_FAIL;
	}

	interface = (strcmp(ifname, "wlan0") == 0)? WIFI_IF_STA : WIFI_IF_AP;

	/* use original check in the ip4addr_aton
	 * to allow set IP with base 8, 10, 16
	 */
#if 0
	if (!isValidIpAddress((char *)ip, false)) {
		SCM_ERR_LOG(SCM_API_TAG,"wrong ip address\n");
		return WISE_FAIL;
	}
#endif

	if (ip4addr_aton(ip, &ip_info.ip) == 0) {
		SCM_ERR_LOG(SCM_API_TAG,"wrong IP address\n");
		return WISE_FAIL;
	}

	scm_wifi_print_ip("ip",&ip_info.ip);

	if (nm) {
#if 0
		if (!isValidIpAddress((char *)nm, true)) {
			SCM_ERR_LOG(SCM_API_TAG,"wrong network mask address\n");
			return WISE_FAIL;
		}
#endif

		if (ip4addr_aton(nm, &ip_info.nm) == 0) {
			SCM_ERR_LOG(SCM_API_TAG,"wrong network mask address\n");
			return WISE_FAIL;
		}
		scm_wifi_print_ip("nm",&ip_info.nm);
	}

	if (gw) {
#if 0
		if (!isValidIpAddress((char *)gw, false)) {
			SCM_ERR_LOG(SCM_API_TAG,"wrong gateway address\n");
			return WISE_FAIL;
		}
#endif

		if (ip4addr_aton(gw, &ip_info.gw) == 0) {
			SCM_ERR_LOG(SCM_API_TAG,"wrong gateway address\n");
			return WISE_FAIL;
		}
		scm_wifi_print_ip("gw",&ip_info.gw);
	}

	return wise_wifi_set_ip_info(interface, &ip_info, nm, gw);
}

/* clear netif's ip, gateway and netmask */
int scm_wifi_reset_ip(const char *ifname)
{
	wifi_ip_info_t ip_info = {0};
	wifi_interface_t interface;

/*
	IP4_ADDR(&ip_info.ip, 0, 0, 0, 0);
	IP4_ADDR(&ip_info.nm, 0, 0, 0, 0);
	IP4_ADDR(&ip_info.gw, 0, 0, 0, 0);
*/

	interface = (strcmp(ifname, "wlan0") == 0)? WIFI_IF_STA : WIFI_IF_AP;

	return wise_wifi_set_ip_info(interface, &ip_info, true, true);
}

int scm_wifi_register_event_callback(system_event_cb_t event_cb, void *priv)
{
	wise_event_loop_init(event_cb, priv);

	return WISE_OK;
}

int scm_wifi_unregister_event(void)
{
	wise_event_loop_deinit();

	return WISE_OK;
}

int scm_wifi_system_event_send(system_event_t *evt)
{
	wise_event_send(evt);

	return WISE_OK;
}

int scm_wifi_mode(wifi_mode_t mode, uint8_t wlan_if)
{
	wifi_mode_t old_mode;

	wise_wifi_get_mode(&old_mode, wlan_if);

	if (old_mode == mode)
		return WISE_OK;
	else if (WISE_OK == wise_wifi_stop(wlan_if)) {
		wise_err_t ret = wise_wifi_set_mode(mode, wlan_if);
		return ret;
	} else
		return WISE_FAIL;
}

static bool g_sta_started;

#define STA_IS_STARTED() g_sta_started
#define STA_SET_STARTED(s) { \
	g_sta_started = s; \
}

static int addr_precheck(const unsigned char *addr)
{
	if (is_zero_ether_addr(addr) || is_broadcast_ether_addr(addr)) {
		return WISE_FAIL;
	}
	return WISE_OK;
}

#ifdef CONFIG_API_SCAN
static int scm_wifi_scan_params_parse(const scm_wifi_scan_params *sp, wifi_scan_config_t *wif_scan_config)
{
	unsigned int len;
	char *bssid_str= NULL, *ssid = NULL;

	if ((sp == NULL) || (sp->scan_type == SCM_WIFI_BASIC_SCAN) ||
		(wif_scan_config == NULL)) {
		SCM_ERR_LOG(SCM_API_TAG,"scm_wifi_scan_params_parse: Invalid param!\n");
		return WISE_OK;
	}

	switch (sp->scan_type) {
		case SCM_WIFI_CHANNEL_SCAN:
			if (!IS_VALID_CH_NUM(sp->channel)) {
				SCM_ERR_LOG(SCM_API_TAG,"Invalid channel_num!\n");
				return WISE_FAIL;
			}
			wif_scan_config->channel = sp->channel;
			break;

		case SCM_WIFI_SSID_SCAN:
			wif_scan_config->spec_ssid = true;
			/* fall-through */
		case SCM_WIFI_SSID_PREFIX_SCAN:
			len = strlen(sp->ssid);

			if (!IS_VALID_SSID_LEN(sp->ssid_len) ||
					!IS_VALID_SSID_LEN(len) ||
					(len != sp->ssid_len)) {
				SCM_ERR_LOG(SCM_API_TAG,"invalid scan ssid parameter");
				return WISE_FAIL;
			}
			ssid = zalloc(sp->ssid_len + 1);
			if (ssid) {
				memcpy(ssid, sp->ssid, sp->ssid_len);
				wif_scan_config->ssid = (uint8_t *) ssid;
			}
			else {
				return WISE_FAIL;
			}

			break;

		case SCM_WIFI_BSSID_SCAN:
			if (addr_precheck(sp->bssid) != WISE_OK) {
				SCM_ERR_LOG(SCM_API_TAG,"invalid scan bssid parameter\n");
				return WISE_FAIL;
			}

			bssid_str = zalloc(6*3*sizeof(char));
			if (bssid_str) {
				sprintf(bssid_str, MACSTR, MAC2STR(sp->bssid));
				wif_scan_config->bssid = (uint8_t *) bssid_str;
			}
			else {
				return WISE_FAIL;
			}

			break;
		default:
			SCM_ERR_LOG(SCM_API_TAG,"scm_wifi_sta_scan: Invalid scan_type!\n");
			return WISE_FAIL;
	}

	return WISE_OK;
}

int scm_wifi_sta_scan(void)
{
	wifi_scan_config_t wif_scan_config;
	memset(&wif_scan_config, 0, sizeof(wifi_scan_config_t));

	if (!STA_IS_STARTED()) {
		SCM_ERR_LOG(SCM_API_TAG,"%s FAIL, STA is not started\n", __func__);
		return WISE_FAIL;
	}

	return wise_wifi_scan_start(&wif_scan_config, true, WIFI_IF_STA);
}

int scm_wifi_sta_advance_scan(scm_wifi_scan_params *sp)
{
	wifi_scan_config_t wif_scan_config = {0};
	wise_err_t ret;

	if (!STA_IS_STARTED()) {
		SCM_ERR_LOG(SCM_API_TAG,"%s FAIL, STA is not started\n", __func__);
		return WISE_FAIL;
	}

	if (scm_wifi_scan_params_parse(sp, &wif_scan_config))
		return WISE_FAIL;

	ret = wise_wifi_scan_start(&wif_scan_config, true, WIFI_IF_STA);

	if (wif_scan_config.bssid)
		os_free(wif_scan_config.bssid);

	if (wif_scan_config.ssid)
		os_free(wif_scan_config.ssid);

	return ret;
}

static scm_wifi_auth_mode scm_wifi_auth_translate(wifi_auth_mode_t authmode)
{
	scm_wifi_auth_mode auth = SCM_WIFI_SECURITY_UNKNOWN;

	switch(authmode){

		case WIFI_AUTH_OPEN:
			auth = SCM_WIFI_SECURITY_OPEN;
			break;

		case WIFI_AUTH_WPA_PSK:
			auth = SCM_WIFI_SECURITY_WPAPSK;
			break;

		case WIFI_AUTH_WPA2_PSK:
			auth = SCM_WIFI_SECURITY_WPA2PSK;
			break;

		case WIFI_AUTH_WPA_ENTERPRISE:
			break;

		case WIFI_AUTH_WPA2_ENTERPRISE:
			break;

		case WIFI_AUTH_WPA_WPA2_PSK:
			auth = SCM_WIFI_SECURITY_WPA2PSK;
			break;

		case WIFI_AUTH_WPA_WPA2_ENTERPRISE:
			break;

		case WIFI_AUTH_WPA3_SAE:
			auth = SCM_WIFI_SECURITY_SAE;
			break;

		case WIFI_AUTH_WPA3_ENTERPRISE:
			break;

		default:
			auth = SCM_WIFI_SECURITY_UNKNOWN;
		}

	return auth;
}

static int scm_wifi_scan_result_parse(scm_wifi_ap_info *ap_list, wifi_ap_record_t *ap_rec, uint16_t ap_num)
{
	if (!ap_list || !ap_rec)
		return WISE_FAIL;

	for (int i = 0; i < ap_num; i++)
	{
		memcpy(ap_list[i].ssid, ap_rec[i].ssid, SCM_WIFI_MAX_SSID_LEN + 1);
		memcpy(ap_list[i].bssid, ap_rec[i].bssid, ETH_ALEN);
		ap_list[i].channel = ap_rec[i].primary;
		ap_list[i].auth = scm_wifi_auth_translate(ap_rec[i].authmode);
		ap_list[i].rssi = ap_rec[i].rssi;
	}

	return WISE_OK;
}

int scm_wifi_sta_scan_results(scm_wifi_ap_info *ap_list, uint16_t *ap_num,
										 uint16_t max_ap_num)
{
	uint8_t ap_idx;
	uint16_t qap_num;
	uint16_t ap_total_num = 0;
	wifi_ap_record_t *ap_rec = NULL;

	if (!STA_IS_STARTED()) {
		SCM_ERR_LOG(SCM_API_TAG,"%s FAIL, STA is not started\n", __func__);
		return WISE_FAIL;
	}

	if ((max_ap_num == 0) || (max_ap_num > WIFI_SCAN_AP_LIMIT)) {
		return WISE_FAIL;
	}
	ap_idx = 0;

	wise_wifi_scan_get_ap_num(&ap_total_num);

	if (ap_total_num > max_ap_num)
		ap_total_num = max_ap_num;

	*ap_num = qap_num = ap_total_num;

	ap_rec =
		(wifi_ap_record_t *) zalloc(qap_num * sizeof(wifi_ap_record_t));

	if (*ap_num) {
		if (!ap_rec) {
			SCM_ERR_LOG(SCM_API_TAG,"memory not enough\n");
			return WISE_FAIL;
		}
		wise_wifi_scan_get_ap_records(ap_idx, &qap_num, ap_rec);

#ifdef CONFIG_WPA_SUPPLICANT_NO_SCAN_RES
		wise_wifi_flush_bss();
#endif

		scm_wifi_scan_result_parse(ap_list, ap_rec, *ap_num);

#if 0 /* for debug */
		for (int i = 0; i < qap_num; i++) {
			SCM_DBG_LOG(SCM_API_TAG,"ap[%d] = %s authmode = %s b:%d g:%d n:%d ax:%d \n",
					i + ap_idx, ap_rec[i].ssid, scm_wifi_auth_mode_strings[ap_rec[i].authmode],
					ap_rec[i].phy_11b, ap_rec[i].phy_11g, ap_rec[i].phy_11n,
					ap_rec[i].phy_11ax);
		}
#endif
	}

	if (ap_rec)
		free(ap_rec);

	return WISE_OK;
}
#endif

int scm_wifi_sta_start(char *ifname, int *len)
{
	int ret = WISE_OK;
	const char *ifc = "wlan0";

#ifdef CONFIG_API_MONITOR
	if (PROMISC_IS_ENABLED()) {
		SCM_ERR_LOG(SCM_API_TAG, "Failed to start STA mode due to promiscuous mode is enabled\n");
		return WISE_FAIL;
	}
#endif

	if ((ifname != NULL) && (len != NULL)) {
		*len = strlen(ifc);

		if (*len > WIFI_IFNAME_MAX_SIZE) {
			ret = WISE_FAIL;
			goto fail;
		}

		memcpy(ifname, ifc, *len);
	}

	if (STA_IS_STARTED())
		return WISE_OK;

	if (!WIFI_IS_INITED()) {
		scm_wifi_init();
	}

	if (scm_wifi_mode(WIFI_MODE_STA, WIFI_IF_STA) != WISE_OK) {
		ret = WISE_FAIL;
		goto fail;
	}

	if (wise_wifi_start(WIFI_IF_STA) != WISE_OK) {
		ret = WISE_FAIL;
		goto fail;
	}

	STA_SET_STARTED(TRUE);
	return WISE_OK;

fail:
	SCM_ERR_LOG(SCM_API_TAG,"%s FAIL\n", __func__);
	return ret;
}

int scm_wifi_sta_stop(void)
{
	if (!STA_IS_STARTED())
		return WISE_OK;

	scm_wifi_sta_disconnect();

	if (wise_wifi_stop(WIFI_IF_STA) != WISE_OK)
		return WISE_FAIL;

	if (wise_wifi_set_mode(WIFI_MODE_NULL, WIFI_IF_STA) != WISE_OK)
		return WISE_FAIL;

	STA_SET_STARTED(FALSE);

	return WISE_OK;
}

static int alg_translate(uint8_t wlan_if, scm_wifi_auth_mode auth,
							scm_wifi_pairwise pairwise)
{
	if (auth >= SCM_WIFI_SECURITY_UNKNOWN ||
			pairwise >= SCM_WIFI_PAIRWISE_MAX)
		return SECURITY_UNKNOWN;

	/* SoftAP only support OPEN and WPA2PSK */
	if (auth == SCM_WIFI_SECURITY_OPEN)
		return SECURITY_OPEN;

	if (auth == SCM_WIFI_SECURITY_WPAPSK && wlan_if != WIFI_IF_AP) {
		if (pairwise == SCM_WIFI_PAIRWISE_TKIP)
			return SECURITY_TKIP;
		if (pairwise == SCM_WIFI_PAIRWISE_AES)
			return SECURITY_CCMP;
	}

	if (auth == SCM_WIFI_SECURITY_WPA2PSK) {
		if (pairwise == SCM_WIFI_PAIRWISE_AES)
			return SECURITY_CCMP;
	}

	if (auth == SCM_WIFI_SECURITY_WPA2PSK256) {
		if (pairwise == SCM_WIFI_PAIRWISE_AES)
			return SECURITY_CCMP_256;
	}

	if (auth == SCM_WIFI_SECURITY_SAE && wlan_if != WIFI_IF_AP) {
		if (pairwise == SCM_WIFI_PAIRWISE_AES)
			return SECURITY_SAE;
	}

	return SECURITY_UNKNOWN;
}

static int scm_wifi_check_key_len(char *key, uint16_t alg)
{
	uint8_t len = strlen(key);

	if (!IS_VALID_PSK_LEN(len)) {
		SCM_ERR_LOG(SCM_API_TAG,"ERROR: PSK len error, should be 8 ~ 63\n");
		return WISE_FAIL;
	}

	return WISE_OK;
}

static void scm_wifi_sta_set_sec(wifi_sta_config_t *sta, scm_wifi_auth_mode auth, uint32_t pmf)
{
	switch (auth) {
		case SCM_WIFI_SECURITY_SAE:
			sta->proto = WIFI_PROTO_WPA2;
			sta->pmf_mode = WIFI_PMF_REQUIRED;
			break;
		case SCM_WIFI_SECURITY_WPA2PSK:
		case SCM_WIFI_SECURITY_WPA2PSK256:
			sta->proto = WIFI_PROTO_WPA2;
			sta->pmf_mode = pmf;
			break;
		case SCM_WIFI_SECURITY_WPAPSK:
			sta->proto = WIFI_PROTO_WPA;
			sta->pmf_mode = WIFI_PMF_DISABLE;
			break;
		default:
			break;
	}
}

int scm_wifi_sta_set_config(scm_wifi_assoc_request *req, void *fast_config)
{
	int ret = WISE_FAIL;
	const char *err = NULL;
	wifi_sta_config_t sta = {0};

	if (req == NULL) {
		err = "input param is NULL";
		goto fail;
	}

	if (!STA_IS_STARTED()) {
		err = "sta is not started";
		goto fail;
	}

	if(is_wise_wpas_run(WISE_IF_WIFI_STA) == 0) {
		err = "is_wise_wpas_run() == 0";
		goto fail;
	}

	if (!IS_VALID_SSID_LEN(strlen(req->ssid))) {
		err = "SSID Len";
		goto fail;
	}

	if (req->hidden_ap > 1) {
		err = "hidden_ap value";
		goto fail;
	}

	strncpy((char *) sta.ssid,
			req->ssid,
			FIELD_SIZE(wifi_sta_config_t, ssid));

	sta.alg = alg_translate(WIFI_IF_STA, req->auth, req->pairwise);

	if (sta.alg == SECURITY_UNKNOWN) {
		err ="Non-Supported Security";
		goto fail;
	}

	scm_wifi_sta_set_sec(&sta, req->auth, req->pmf);
	if (sta.alg != 0 && !fast_config) {
		if (scm_wifi_check_key_len(req->key, sta.alg) != WISE_OK) {
			err ="Key Len";
			goto fail;
		}

		strncpy((char *) sta.password, req->key,
						FIELD_SIZE(wifi_sta_config_t, password));

	} else {
		sta.password[0] = '\0';
	}

	if (addr_precheck(req->bssid) == WISE_OK) {
		strncpy((char *) sta.bssid,
				(void *)req->bssid,
				FIELD_SIZE(wifi_sta_config_t, bssid));
		sta.bssid_set = 1;
	} else
		sta.bssid_set = 0;

	if (req->hidden_ap)
		sta.scan_ssid = 1;

	if (wise_wifi_clean_config(WISE_IF_WIFI_STA, WIFI_IF_STA) != WISE_OK)
		return WISE_FAIL;

	if (wise_wifi_set_config(WISE_IF_WIFI_STA,
				(wifi_config_t *)&sta,
				WIFI_IF_STA,
				(wifi_fast_connect_t *) fast_config) !=
		WISE_OK)
		return WISE_FAIL;

	return WISE_OK;

fail:
	SCM_ERR_LOG(SCM_API_TAG,"%s Invalid Input: %s\n", __func__, err);
	return ret;

}

int scm_wifi_sta_connect(void)
{
	int ret;

	if (is_wise_wpas_run(WISE_IF_WIFI_STA) == 0) {
		return WISE_FAIL;
	}

	if (!STA_IS_STARTED()) {
		SCM_ERR_LOG(SCM_API_TAG,"%s FAIL, STA is not started\n", __func__);
		return WISE_FAIL;
	}

	wise_wifi_disconnect(WIFI_IF_STA);
	ret = wise_wifi_connect(WIFI_IF_STA);
	if (ret != WISE_OK) {
		return ret;
	}
	/* re-set wpa_s->disconnected, was clean
	 * by wpas_request_disconnection
	 */
	ret = wise_wifi_reconnect(WIFI_IF_STA);

	return ret;
}

int scm_wifi_sta_disconnect(void)
{
	struct netif *lwip_netif = scm_wifi_get_netif(WISE_IF_WIFI_STA);

	if (is_wise_wpas_run(WISE_IF_WIFI_STA) == 0) {
		return WISE_FAIL;
	}

	if (!STA_IS_STARTED()) {
		SCM_ERR_LOG(SCM_API_TAG,"%s FAIL, STA is not started\n", __func__);
		return WISE_FAIL;
	}

	netifapi_dhcp_stop(lwip_netif);
	scm_wifi_reset_ip("wlan0");
	wise_wifi_disconnect(WISE_IF_WIFI_STA);

	return WISE_OK;
}

int scm_wifi_sta_disconnect_event(uint8_t enable)
{
	char *ifc = "wlan0";

	struct wpa_supplicant *wpa_s;

	if (!STA_IS_STARTED()) {
		SCM_ERR_LOG(SCM_API_TAG,"error: sta is not started\n");
		return WISE_FAIL;
	}

	wpa_s = wise_wpas_get(ifc);

	if (wpa_s == NULL) {
		SCM_ERR_LOG(SCM_API_TAG,"get supplicant info fail. \n");
		return WISE_FAIL;
	}

	osMutexAcquire(wpa_s->bss_lock, osWaitForever);

	wpa_s->disassoc_evt = enable;

	osMutexRelease(wpa_s->bss_lock);

	return WISE_OK;
}

#define UNAVAILABLE_RSSI 255

int scm_wifi_sta_get_ap_rssi(void)
{
	wifi_ap_record_t ap_info = {0};

	if(wise_wifi_sta_get_ap_info(&ap_info, WIFI_IF_STA)) {
		SCM_ERR_LOG(SCM_API_TAG,"get rssi fail \n");
		return UNAVAILABLE_RSSI;
	}

	return ap_info.rssi;
}

int scm_wifi_sta_get_ap_country(char *country)
{
	wifi_ap_record_t ap_info = {0};

	if(wise_wifi_sta_get_ap_info(&ap_info, WIFI_IF_STA)) {
		SCM_ERR_LOG(SCM_API_TAG,"get country_code fail \n");
		return WISE_FAIL;
	}
	memcpy(country, ap_info.country.cc, 2);
	return WISE_OK;
}

#ifdef CONFIG_IEEE80211_MODE_5GHZ
uint8_t scm_wise_freq5g_to_channel(int freq_mhz)
{
	if (freq_mhz >= 5180 && freq_mhz <= 5240) {
		/* UNII-1 */
		return (freq_mhz - 5000) / 5;
	} else if (freq_mhz >= 5260 && freq_mhz <= 5320) {
		/* UNII-2 */
		return (freq_mhz - 5000) / 5;
	} else if (freq_mhz >= 5500 && freq_mhz <= 5700) {
		/* UNII-2e */
		return (freq_mhz - 5000) / 5;
	} else if (freq_mhz >= 5745 && freq_mhz <= 5825) {
		/* UNII-3 */
		return (freq_mhz - 5000) / 5;
	} else {
		return 0;
	}
}
#endif

int scm_wifi_sta_get_connect_info(scm_wifi_status *connect_status)
{
	const char *ifc = "wlan0";
	struct wpa_supplicant *wpa_s;
	struct wpa_bss *bss;

	if (connect_status == NULL) {
		SCM_ERR_LOG(SCM_API_TAG,"input param is NULL.\n");
		return WISE_FAIL;
	}

	if (is_wise_wpas_run(WISE_IF_WIFI_STA) == 0) {
		return WISE_FAIL;
	}

	wpa_s = wise_wpas_get(ifc);

	if (wpa_s == NULL) {
		SCM_ERR_LOG(SCM_API_TAG,"get supplicant info fail. \n");
		return WISE_FAIL;
	}

	if (!STA_IS_STARTED()) {
		return WISE_FAIL;
	}

	osMutexAcquire(wpa_s->bss_lock, osWaitForever);

	bss = wpa_s->current_bss;

	if (bss) {
		memcpy(connect_status->bssid, bss->bssid, ETH_ALEN);
		memcpy(connect_status->ssid, bss->ssid, bss->ssid_len);
		connect_status->ssid[bss->ssid_len] = '\0';
		connect_status->channel = (bss->freq - 2407) / 5;
#ifdef CONFIG_IEEE80211_MODE_5GHZ
		if (bss->freq > 5000) {
			connect_status->channel = scm_wise_freq5g_to_channel(bss->freq);
		}
#endif
		connect_status->status = (wpa_s->wpa_state == WPA_COMPLETED) ? SCM_WIFI_CONNECTED : SCM_WIFI_DISCONNECTED;
	}

	osMutexRelease(wpa_s->bss_lock);

	return WISE_OK;
}

int scm_wifi_sta_get_reconnect_policy(void)
{
	char *ifc = "wlan0";
	struct wpa_supplicant *wpa_s;
	struct wifi_reconnect_set reconnect_para;

	wpa_s = wise_wpas_get(ifc);

	if (wpa_s == NULL) {
		SCM_ERR_LOG(SCM_API_TAG,"get supplicant info fail. \n");
		return WISE_FAIL;
	}

	osMutexAcquire(wpa_s->bss_lock, osWaitForever);
	reconnect_para.enable = wpa_s->reconnect_params.enable;
	reconnect_para.timeout = wpa_s->reconnect_params.timeout;
	reconnect_para.period = wpa_s->reconnect_params.period;
	reconnect_para.max_try_count = wpa_s->reconnect_params.max_try_count;
	osMutexRelease(wpa_s->bss_lock);

	SCM_INFO_LOG(SCM_API_TAG,"enable: %d\n", reconnect_para.enable);
	SCM_INFO_LOG(SCM_API_TAG,"timeout: %u\n", reconnect_para.timeout);
	SCM_INFO_LOG(SCM_API_TAG,"period: %u\n", reconnect_para.period);
	SCM_INFO_LOG(SCM_API_TAG,"count: %u\n", reconnect_para.max_try_count);

	return WISE_OK;
}

int scm_wifi_sta_set_reconnect_policy(int enable, unsigned int timeout,
                                     unsigned int period, unsigned int max_try_count)
{
	char *ifc = "wlan0";
	struct wpa_supplicant *wpa_s;

	if (!STA_IS_STARTED()) {
		SCM_ERR_LOG(SCM_API_TAG,"error: sta is not started\n");
		return WISE_FAIL;
	}

	wpa_s = wise_wpas_get(ifc);

	if ((enable < WPA_FLAG_OFF) || (enable > WPA_FLAG_ON) ||
			(timeout < WIFI_MIN_RECONNECT_TIMEOUT) || (timeout > WIFI_MAX_RECONNECT_TIMEOUT) ||
			(period < WIFI_MIN_RECONNECT_PERIOD) || (period > WIFI_MAX_RECONNECT_PERIOD) ||
			(max_try_count < WIFI_MIN_RECONNECT_TIMES) || (max_try_count > WIFI_MAX_RECONNECT_TIMES)) {
		SCM_ERR_LOG(SCM_API_TAG,"input value error.\n");
		return WISE_FAIL;
	}

	if (wpa_s == NULL) {
		SCM_ERR_LOG(SCM_API_TAG,"get supplicant info fail. \n");
		return WISE_FAIL;
	}

	if (timeout == WIFI_MAX_RECONNECT_TIMEOUT)
		enable = 0;

	osMutexAcquire(wpa_s->bss_lock, osWaitForever);

	wpa_s->reconnect_params.enable = enable;
	wpa_s->reconnect_params.timeout = timeout;
	wpa_s->reconnect_params.period = period;
	wpa_s->reconnect_params.max_try_count = max_try_count;

	/*
	 * if disable the reconnect policy, we disable wpa_supplicant reconnect as well,
	 * and for the case that enable == 1, we will take over the control of reconnect
	 * from wpa_supplicant when our reconnect policy is triggered, e.g., a disconnect
	 * event received
	 */
	if (enable == 0)
		wpa_s->reconnect_params.disable_wpas_retry = true;

	osMutexRelease(wpa_s->bss_lock);

	return WISE_OK;
}

/* For getting/setting country code Using API Testing */
int scm_wifi_sta_get_country_code(char *country_code)
{
	return wise_wifi_get_country_code(country_code);
}


#define SCM_WIFI_COUNTRY_DEFAULT "01"	/* world-wide safe region */
int scm_wifi_sta_set_country_code(char *country_code, bool use_ieee80211d, bool set_ap_mode_enable)
{
	if (!country_code || !country_code[0]) {
		country_code = SCM_WIFI_COUNTRY_DEFAULT;
		use_ieee80211d = true;	/* uses the configured AP's CC */
	}

	return wise_wifi_set_country_code(country_code, use_ieee80211d, set_ap_mode_enable);
}

/* For Setting PS type */
int scm_wifi_sta_set_ps(int mode)
{
	int ret = WISE_FAIL;
	wifi_ps_type_t ps_type;

	switch (mode) {
	case SCM_WIFI_PS_OFF:
		ps_type = WIFI_PS_NONE;
		break;
	case SCM_WIFI_PS_ON:
		ps_type = WIFI_PS_MIN_MODEM;
		break;
	case SCM_WIFI_PS_ON_LESS_BEACONS:
		ps_type = WIFI_PS_MAX_MODEM;
		break;
	default:
		SCM_ERR_LOG(SCM_API_TAG,"error: ps mode is not support: %d\n", mode);
		goto done;
	}

	ret = wise_wifi_set_ps(ps_type);
	if (ret) {
		SCM_ERR_LOG(SCM_API_TAG,"error: ps set mode fail\n");
		goto done;
	}

	ret = WISE_OK;
done:
	return ret;
}

int scm_wifi_sta_set_sleeptime(uint16_t sleeptime)
{
	if ((sleeptime < STA_MIN_SLEEP) || (sleeptime > STA_MAX_SLEEP)) {
		SCM_ERR_LOG(SCM_API_TAG,"station powersave sleep tune must be %d ~ %d.\n", STA_MIN_SLEEP, STA_MAX_SLEEP);
		return WISE_FAIL;
	}

	if (wise_wifi_set_powersavesleep(sleeptime) != WISE_OK) {
			SCM_ERR_LOG(SCM_API_TAG,"set station powersave sleep time fail\n");
			return WISE_FAIL;
		}

	return WISE_OK;
}

int scm_wifi_sta_set_keepalive(scm_wifi_keepalive_param *param)
{
	param->type = WIFI_KEEPALIVE_TYPE_WISE;
	return wise_wifi_set_keepalive(param);
}

int scm_wifi_sta_get_psk(uint8_t *psk, int len) {

	if (!psk || len != WISE_PMK_LEN)
		return WISE_FAIL;

	return wise_wpa_get_psk(WISE_IF_WIFI_STA, psk, len);
}


int scm_wifi_sta_restore_psk(uint8_t *psk) {

	if (!psk)
		return WISE_FAIL;
	/*
	 * it's a example, please get the psk from flash
	 * WPA2_PSK
	 * ssid: Xiaomi_7AB6
	 * password: 12345678
	 */
	uint8_t restore_psk[WISE_PMK_LEN] = {
			0xfa, 0xbe, 0x43, 0x0c, 0xcd, 0xca, 0xdb, 0x88, 0xcb, 0x2a, 0x7f, 0x81, 0x2b, 0x46, 0x3f, 0x67,
			0x00, 0xc0, 0x50, 0x3c, 0xb6, 0x57, 0xef, 0xd6, 0x7a, 0x80, 0x41, 0x40, 0x95, 0x19, 0xfc, 0x83};
	memcpy(psk, restore_psk, WIFI_STA_PSK_LEN);

	return WISE_OK;
}

int scm_wifi_sta_fast_connect(scm_wifi_fast_assoc_request *fast_request)
{
	wifi_fast_connect_t fast_config = {0};

	if (!STA_IS_STARTED()) {
		SCM_ERR_LOG(SCM_API_TAG,"error: sta is not started \n");
		return WISE_FAIL;
	}

	if (fast_request == NULL) {
		SCM_ERR_LOG(SCM_API_TAG,"input param is NULL \n");
		return WISE_FAIL;
	}

	if ((fast_request->req.auth != SCM_WIFI_SECURITY_WPAPSK) &&
		(fast_request->req.auth != SCM_WIFI_SECURITY_WPA2PSK)) {
		SCM_ERR_LOG(SCM_API_TAG,"this auth not support set psk \n");
		return WISE_FAIL;
	}

	memcpy(fast_config.psk, fast_request->psk, WISE_PMK_LEN);
	fast_config.channel = fast_request->channel;
	if (scm_wifi_sta_set_config(&fast_request->req, &fast_config))
		return WISE_FAIL;

	if (scm_wifi_sta_connect())
		return WISE_FAIL;

	return WISE_OK;
}

void scm_wifi_sta_dump_ap_info(scm_wifi_status *connect_status)
{
	if (connect_status) {
		char cc_buf[3];
		char *country = cc_buf;
		memset(cc_buf, 0, sizeof(cc_buf));

		SCM_INFO_LOG(SCM_API_TAG,"AP SSID: %s\n", connect_status->ssid);
		SCM_INFO_LOG(SCM_API_TAG,"AP BSSID: "MACSTR"\n"
				, MAC2STR(connect_status->bssid));
		SCM_INFO_LOG(SCM_API_TAG,"AP CH: %d\n", connect_status->channel);
		SCM_INFO_LOG(SCM_API_TAG,"AP RSSI: %d\n", scm_wifi_sta_get_ap_rssi());
		if (!scm_wifi_sta_get_ap_country(country))
			SCM_INFO_LOG(SCM_API_TAG,"AP Country : %s\n", country);
		SCM_INFO_LOG(SCM_API_TAG,"Status: %s\n", (connect_status->status==SCM_WIFI_CONNECTED) ? "CONNECTED" : "DISCONNECTED");
	}
}

#ifdef CONFIG_API_WATCHER
int scm_wifi_wc_set_keepalive(scm_wifi_keepalive_param *param)
{
	param->type = WIFI_KEEPALIVE_TYPE_WATCHER;
	return wise_wifi_set_keepalive(param);
}

int scm_wifi_wc_set_bcn_loss_chk(uint8_t enable)
{
	return wise_wifi_set_wc_bcn_loss_chk(enable);
}

int scm_wifi_wc_set_port_filter(uint8_t enable)
{
	return wise_wifi_set_wc_port_filter(enable);
}
#endif

int scm_wifi_set_tx_pwr(scm_wifi_tx_power_param *param)
{
	return wise_wifi_set_tx_power(param);
}

int scm_wifi_get_tx_pwr(scm_wifi_tx_power_param *param)
{
	return wise_wifi_get_tx_power(param);
}

int scm_wifi_tx_pwr_mode(uint32_t *mode)
{
	return wise_wifi_set_power_mode(mode);
}

#if defined(CONFIG_SUPPORT_WC_MQTT_KEEPALIVE) || defined(CONFIG_SET_SHARE_MEM_ADDR)
int scm_wifi_set_shared_mem_addr(uint32_t *addr)
{
	return wise_wifi_set_shared_mem_addr(addr);
}
#endif

#ifdef CONFIG_API_SOFTAP

static bool g_sap_configured;
static bool g_sap_started;

#define SAP_IS_CONFIGURED() g_sap_configured
#define SAP_SET_CONFIGURED(s) { \
	g_sap_configured = s; \
}

#define SAP_IS_STARTED() g_sap_started
#define SAP_SET_STARTED(s) { \
	g_sap_started = s; \
}

static wifi_ap_config_t saved_ap_config = {0};
static scm_wifi_softap_config saved_sap = {0};

#define SAVED_WISE_AP_CFG() &saved_ap_config

#define SAVED_SCM_SAP_CFG() &saved_sap
#define SAVED_SCM_SAP_CFG_FIELD(f) (saved_sap.f)

static int scm_wifi_sap_store_cfg(wifi_ap_config_t *ap)
{
	int ret = WISE_OK;

	strncpy(saved_sap.ssid, (char *)ap->ssid, FIELD_SIZE(wifi_ap_config_t, ssid));
	saved_sap.channel_num = ap->channel;
	saved_sap.ssid_hidden = ap->ssid_hidden;
	strncpy(saved_sap.key, (char *)ap->password, FIELD_SIZE(wifi_ap_config_t, password));
	/* currently SAP only support OPEN/WPA2PSK */
	if (ap->alg == SECURITY_CCMP) {
		saved_sap.authmode = SCM_WIFI_SECURITY_WPA2PSK;
		saved_sap.pairwise = SCM_WIFI_PAIRWISE_AES;
	} else if (ap->alg == SECURITY_OPEN) {
		saved_sap.authmode = SCM_WIFI_SECURITY_OPEN;
		saved_sap.pairwise = SCM_WIFI_PAIRWISE_NONE;
	} else {
		SCM_ERR_LOG(SCM_API_TAG,"%s unsupported ap alg %d\n", __func__, ap->alg);
		ret = WISE_FAIL;
	}

	return ret;
}

int scm_wifi_sap_start(char *ifname, int *len)
{
	int ret = WISE_FAIL;
	const char *err = NULL;
	wifi_ap_config_t *ap = SAVED_WISE_AP_CFG();
	const char *ifc = "wlan1";

#ifdef CONFIG_API_MONITOR
	if (PROMISC_IS_ENABLED()) {
		SCM_ERR_LOG(SCM_API_TAG, "Failed to start SAP mode due to promiscuous mode is enabled\n");
		return WISE_FAIL;
	}
#endif

	if ((ifname != NULL) && (len != NULL)) {
		*len = strlen(ifc);

		if (*len > WIFI_IFNAME_MAX_SIZE) {
			err ="ERROR: len > WIFI_IFNAME_MAX_SIZE";
			goto fail;
		}

		memcpy(ifname, ifc, *len);
	}

	if (SAP_IS_STARTED()) {
		SCM_INFO_LOG(SCM_API_TAG,"SAP has already been started, please stop it first if there's cfg info updated\n");
		return WISE_OK;
	}

	if (strcmp((const char *)ap->ssid, "\0") == 0) {
		err ="ERROR: SoftAP not configured";
		goto fail;
	}

	if (!WIFI_IS_INITED()) {
		scm_wifi_init();
	}

	scm_wifi_mode(WIFI_MODE_AP, WIFI_IF_AP);

	if (wise_wifi_clean_config(WISE_IF_WIFI_AP, WIFI_IF_AP) != WISE_OK) {
		goto fail;
	}

	if (SAVED_SCM_SAP_CFG_FIELD(interval)) {
		SCM_DBG_LOG(SCM_API_TAG,"beacon interval: %u\n", SAVED_SCM_SAP_CFG_FIELD(interval));
		if (wise_wifi_set_beacon_interval(SAVED_SCM_SAP_CFG_FIELD(interval))!= WISE_OK) {
			goto fail;
		}
	}
	if (SAVED_SCM_SAP_CFG_FIELD(period)) {
		SCM_DBG_LOG(SCM_API_TAG,"dtim period: %u\n", SAVED_SCM_SAP_CFG_FIELD(period));
		if (wise_wifi_set_dtim_period(SAVED_SCM_SAP_CFG_FIELD(period))!= WISE_OK) {
			goto fail;
		}
	}

	if (wise_wifi_set_config(WISE_IF_WIFI_AP, (wifi_config_t *)ap, WIFI_IF_AP, NULL) != WISE_OK) {
		goto fail;
	}

	if (wise_wifi_start(WISE_IF_WIFI_AP) != WISE_OK) {
		goto fail;
	}

	scm_wifi_sap_store_cfg(ap);

	SAP_SET_STARTED(TRUE);
	return WISE_OK;

fail:
	SCM_ERR_LOG(SCM_API_TAG,"%s FAIL: %s\n", __func__, err);
	return ret;
}

int scm_wifi_sap_stop(void)
{
	if (!SAP_IS_STARTED())
		return WISE_OK;

	scm_wifi_reset_ip("wlan1");

	if (wise_wifi_stop(WISE_IF_WIFI_AP) != WISE_OK) {
			SCM_ERR_LOG(SCM_API_TAG,"scm_wifi_sap_stop FAIL\n");
			return WISE_FAIL;
	}

	if (wise_wifi_set_mode(WIFI_MODE_NULL, WISE_IF_WIFI_AP) != WISE_OK)
		return WISE_FAIL;

	SAP_SET_STARTED(FALSE);
	return WISE_OK;
}

static void scm_wifi_sap_set_sec(wifi_ap_config_t *ap, scm_wifi_auth_mode auth)
{
	/* do not support wpa3 */
	switch (auth) {
		case SCM_WIFI_SECURITY_WPA2PSK:
			ap->proto = WIFI_PROTO_WPA2;
			break;
		case SCM_WIFI_SECURITY_WPAPSK:
			ap->proto = WIFI_PROTO_WPA;
			break;
		default:
			break;
	}
}

int scm_wifi_sap_set_config(scm_wifi_softap_config *sap)
{
	int ret = WISE_FAIL;
	const char *err = NULL;
	wifi_ap_config_t ap = {0};

	if (sap == NULL) {
		err = "input param is NULL";
		goto fail;
	}

	if (!IS_VALID_CH_NUM(sap->channel_num)) {
		err = "Channel Number";
		goto fail;
	}

	if (!IS_VALID_HIDDEN_NUM(sap->ssid_hidden)) {
		err = "SSID Hidden";
		goto fail;
	}

	ap.max_connection = WPA_MAX_NUM_STA;

	ap.ssid_hidden = sap->ssid_hidden;

	ap.channel = sap->channel_num;

	if (!IS_VALID_SSID_LEN(strlen(sap->ssid))) {
		err = "SSID Len";
		goto fail;
	}

	strncpy((char *) ap.ssid, sap->ssid, FIELD_SIZE(wifi_ap_config_t, ssid));

	ap.alg = alg_translate(WIFI_IF_AP, sap->authmode, sap->pairwise);

	if (ap.alg == SECURITY_UNKNOWN) {
		err ="Non-Supported Security";
		goto fail;
	}

	if (ap.alg != 0) {
		if (scm_wifi_check_key_len(sap->key, ap.alg) != WISE_OK) {
			err ="Key Len";
			goto fail;
		}

		strncpy((char *) ap.password, sap->key,
						FIELD_SIZE(wifi_ap_config_t, password));

		scm_wifi_sap_set_sec(&ap, sap->authmode);
	} else {
		ap.password[0] = '\0';
	}

	memcpy(SAVED_WISE_AP_CFG(), &ap, sizeof(wifi_ap_config_t));

	SAP_SET_CONFIGURED(TRUE);

	return WISE_OK;

fail:
	SCM_ERR_LOG(SCM_API_TAG,"%s Invalid Input: %s\n", __func__, err);
	return ret;
}

int scm_wifi_sap_get_config(scm_wifi_softap_config *sap)
{
	if (sap == NULL) {
		SCM_ERR_LOG(SCM_API_TAG,"Invalid Input\n");
		return WISE_FAIL;
	}

	if (!SAP_IS_CONFIGURED() || strcmp((const char *)SAVED_SCM_SAP_CFG_FIELD(ssid), "\0") == 0) {
		SCM_ERR_LOG(SCM_API_TAG,"ERROR: SoftAP not configured yet.\n");
		return WISE_FAIL;
	}

	memcpy(sap, SAVED_SCM_SAP_CFG(), sizeof(scm_wifi_softap_config));

	return WISE_OK;
}

int scm_wifi_sap_set_beacon_interval(uint32_t interval)
{
	if ((interval < WPA_AP_MIN_BEACON) || (interval > WPA_AP_MAX_BEACON)) {
		SCM_ERR_LOG(SCM_API_TAG,"beacon interval must be %d ~ %d.\n", WPA_AP_MIN_BEACON, WPA_AP_MAX_BEACON);
		return WISE_FAIL;
	}

	SAVED_SCM_SAP_CFG_FIELD(interval) = interval;

	SCM_DBG_LOG(SCM_API_TAG,"beacon interval: %lu\n", interval);
	return WISE_OK;
}

int scm_wifi_sap_set_dtim_period(uint8_t period)
{
	if ((period < WPA_AP_MIN_DTIM) || (period > WPA_AP_MAX_DTIM)) {
		SCM_ERR_LOG(SCM_API_TAG,"dtim period must be %d ~ %d.\n", WPA_AP_MIN_DTIM, WPA_AP_MAX_DTIM);
		return WISE_FAIL;
	}

	SAVED_SCM_SAP_CFG_FIELD(period) = period;

	SCM_DBG_LOG(SCM_API_TAG,"dtim period: %u\n", period);
	return WISE_OK;
}

int scm_wifi_sap_get_connected_sta(scm_wifi_ap_sta_info *sta_list, uint8_t *sta_num)
{
	struct device *dev = wlandev(0);
	struct ieee80211vap *vap = wlan_get_vap(dev, WIFI_IF_AP);
	struct ieee80211_node *ni;

	struct wpa_supplicant *wpa_s;
	struct hostapd_data *hapd;
	struct sta_info *sta;

	*sta_num = 0;

	if (!SAP_IS_STARTED())
		return WISE_OK;

	wpa_s = wise_wpas_get("wlan1");

	if ((vap == NULL) || (wpa_s == NULL)) {
		SCM_ERR_LOG(SCM_API_TAG, "vap[%p] or wpa_s[%p] is NULL\n", vap, wpa_s);
		return WISE_FAIL;
	}

	hapd = wpa_s->ap_iface->bss[0];

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		SCM_DBG_LOG(SCM_API_TAG, MACSTR"\n", MAC2STR(sta->addr));

		ni = ieee80211_find_vap_node(&vap->iv_ic->ic_sta, vap, sta->addr);
		if (ni == NULL) {
			/* Synchronizing sta list between hostap and net80211 */
			return WISE_OK;
		}
		memcpy(sta_list[*sta_num].mac, sta->addr, SCM_WIFI_MAC_LEN);

		sta_list[*sta_num].rssi = ni->ni_avgrssi;
		sta_list[*sta_num].rate = ni->ni_txrate;

		ieee80211_free_node(ni);
		*sta_num += 1;
	}

#if 0 //keep for debug if needed
	SCM_DBG_LOG(SCM_API_TAG,"sta num: %d\n", *sta_num);

	for (int i = 0; i < *sta_num; i++) {
		SCM_DBG_LOG(SCM_API_TAG,"STA addr:"MACSTR"\n", MAC2STR(sta_list[i].mac));
		SCM_DBG_LOG(SCM_API_TAG,"STA rssi: %d\n", sta_list[i].rssi);
		SCM_DBG_LOG(SCM_API_TAG,"STA rate: 0x%x\n", sta_list[i].rate);
	}
#endif

	return WISE_OK;
}


int scm_wifi_sap_deauth_sta (const char *txtaddr, unsigned char addr_len)
{
	struct wpa_supplicant *wpa_s;

	if (!SAP_IS_STARTED()) {
		SCM_ERR_LOG(SCM_API_TAG,"%s FAIL, SAP is not started\n", __func__);
		return WISE_FAIL;
	}

	wpa_s = wise_wpas_get("wlan1");

	if (wpa_drv_deauthenticate (wpa_s, (const uint8_t *)txtaddr, WLAN_REASON_PREV_AUTH_NOT_VALID) != WISE_OK)
		return WISE_FAIL;

	if (wise_wifi_deauth(WIFI_IF_AP, txtaddr) != WISE_OK)
		return WISE_FAIL;

	return WISE_OK;
}

#endif

int scm_wifi_set_options(scm_wifi_set_option opt, void *arg)
{
	int ret = WISE_FAIL;

	switch (opt) {
		case SCM_WIFI_STA_SET_KEEPALIVE:
			{
				scm_wifi_keepalive_param *param = (scm_wifi_keepalive_param *)arg;
				ret = scm_wifi_sta_set_keepalive(param);
			}
			break;

		case SCM_WIFI_STA_SET_POWERSAVE:
			{
				scm_wifi_ps_param *param = (scm_wifi_ps_param *)arg;
				ret = scm_wifi_sta_set_ps(param->mode);
				if (ret)
					break;

				if (param->mode != WIFI_PS_NONE && param->interval != 0)
					ret = scm_wifi_sta_set_sleeptime(param->interval);
			}
			break;
		case SCM_WIFI_STA_SET_COUNTRYCODE:
			{
				scm_wifi_country_param *param = (scm_wifi_country_param *)arg;
				ret = scm_wifi_sta_set_country_code(param->country_code,
							param->use_ieee80211d,
							true);
			}
			break;

		case SCM_WIFI_STA_SET_RECONNECT:
			{
				scm_wifi_reconnect_param *param = (scm_wifi_reconnect_param *)arg;
				ret = scm_wifi_sta_set_reconnect_policy(param->enable,
							param->timeout,
							param->period,
							param->max_try_count);
			}
			break;

#ifdef CONFIG_API_WATCHER
		case SCM_WIFI_WC_SET_KEEPALIVE:
			{
				scm_wifi_keepalive_param *param = (scm_wifi_keepalive_param *)arg;
				ret = scm_wifi_wc_set_keepalive(param);
			}
			break;

		case SCM_WIFI_WC_SET_BCN_LOSS_CHK:
			{
				uint8_t *enable = (uint8_t *) arg;
				ret = scm_wifi_wc_set_bcn_loss_chk(*enable);
			}
			break;


		case SCM_WIFI_WC_SET_PORT_FILTER:
			{
				uint8_t *enable = (uint8_t *) arg;
				ret = scm_wifi_wc_set_port_filter(*enable);
			}
			break;
#endif
		case SCM_WIFI_SET_TXPOWER:
		case SCM_WIFI_SET_RESET_TXPOWER:
				ret = scm_wifi_set_tx_pwr((scm_wifi_tx_power_param *) arg);
			break;
		case SCM_WIFI_SET_TXPOWER_MODE:
				ret = scm_wifi_tx_pwr_mode((uint32_t *) arg);
			break;
#if defined(CONFIG_SUPPORT_WC_MQTT_KEEPALIVE) || defined(CONFIG_SET_SHARE_MEM_ADDR)
		case SCM_WIFI_SET_SHARED_MEM_ADDR:
				ret = scm_wifi_set_shared_mem_addr((uint32_t *) arg);
			break;
#endif
		default:
			SCM_ERR_LOG(SCM_API_TAG,"%s FAIL, opt not supported\n", __func__);
			break;
	}

	return ret;
}

int scm_wifi_get_options(scm_wifi_get_option opt, void *buf)
{
	int ret = WISE_FAIL;

	switch (opt) {
		case SCM_WIFI_STA_GET_CONNECT:
			{
				ret = scm_wifi_sta_get_connect_info(buf);
			}
			break;

		case SCM_WIFI_STA_GET_RSSI:
			{
				int *rssi = buf;
				*rssi = scm_wifi_sta_get_ap_rssi();
				ret = WISE_OK;
			}
			break;
		case SCM_WIFI_STA_GET_PSK:
			{
				ret = scm_wifi_sta_get_psk(buf, WISE_PMK_LEN);
			}
			break;

		case SCM_WIFI_STA_GET_COUNTRYCODE:
			{
				ret = scm_wifi_sta_get_country_code(buf);
			}
			break;

		case SCM_WIFI_GET_TXPOWER:
			ret = scm_wifi_get_tx_pwr(buf);
			break;
		default:
			SCM_ERR_LOG(SCM_API_TAG,"%s FAIL, opt not supported\n", __func__);
			break;
	}

	return ret;
}

int scm_wifi_set_channel(char *ifname, uint8_t primary, scm_wifi_2nd_ch_loc secondary)
{
	wifi_interface_t interface;
    wifi_second_chan_t second_ch;

	if (!(strcmp(ifname, "wlan0") == 0 || strcmp(ifname, "wlan1") == 0)) {
		SCM_ERR_LOG(SCM_API_TAG,"wrong ifname\n");
		return WISE_FAIL;
	}

	interface = (strcmp(ifname, "wlan0") == 0)? WIFI_IF_STA : WIFI_IF_AP;

    if (secondary == SCM_WIFI_2ND_CH_ABOVE) {
        second_ch = WIFI_SECOND_CHAN_ABOVE;
    } else if (secondary == SCM_WIFI_2ND_CH_BELOW) {
        second_ch = WIFI_SECOND_CHAN_BELOW;
    } else {
        second_ch = WIFI_SECOND_CHAN_NONE;
    }

	if (wise_wifi_set_channel(interface, primary, second_ch))
		return WISE_FAIL;

    return WISE_OK;
}

int scm_wifi_get_channel(char *ifname, uint8_t *primary, scm_wifi_2nd_ch_loc *secondary)
{
	wifi_interface_t interface;
    wifi_second_chan_t second_ch;

	if (!(strcmp(ifname, "wlan0") == 0 || strcmp(ifname, "wlan1") == 0)) {
		SCM_ERR_LOG(SCM_API_TAG,"wrong ifname\n");
		return WISE_FAIL;
	}

	interface = (strcmp(ifname, "wlan0") == 0)? WIFI_IF_STA : WIFI_IF_AP;

	if (wise_wifi_get_channel(interface, primary, &second_ch))
		return WISE_FAIL;

    if (second_ch == WIFI_SECOND_CHAN_ABOVE) {
        *secondary = SCM_WIFI_2ND_CH_ABOVE;
    } else if (second_ch == WIFI_SECOND_CHAN_BELOW){
        *secondary = SCM_WIFI_2ND_CH_BELOW;
    } else {
        *secondary = SCM_WIFI_2ND_CH_NONE;
    }

    return WISE_OK;
}

#ifdef CONFIG_API_MONITOR

int scm_wifi_80211_tx(const char *ifname, const void *buffer, int len)
{
	wifi_interface_t interface;

	if (!(strcmp(ifname, "wlan0") == 0 || strcmp(ifname, "wlan1") == 0)) {
		SCM_ERR_LOG(SCM_API_TAG,"wrong ifname\n");
		return WISE_FAIL;
	}

	interface = (strcmp(ifname, "wlan0") == 0) ? WIFI_IF_STA : WIFI_IF_AP;

	if (wise_wifi_80211_tx(interface, buffer, len, false)) {
		return WISE_FAIL;
	}

	return WISE_OK;
}

int scm_wifi_set_promiscuous(const char *ifname, bool en)
{
	wifi_interface_t interface;

	if (!(strcmp(ifname, "wlan0") == 0 || strcmp(ifname, "wlan1") == 0)) {
		SCM_ERR_LOG(SCM_API_TAG,"wrong ifname\n");
		return WISE_FAIL;
	}

	if (! (en ^ PROMISC_IS_ENABLED()))
		return WISE_OK;

#ifdef CONFIG_API_SOFTAP
	if (SAP_IS_STARTED()) {
		SCM_ERR_LOG(SCM_API_TAG, "Failed to set promiscuous mode because SAP has already started\n");
		return WISE_FAIL;
	}
#endif

	if (STA_IS_STARTED()) {
		SCM_ERR_LOG(SCM_API_TAG, "Failed to set promiscuous mode because STA has already started\n");
		return WISE_FAIL;
	}

	interface = (strcmp(ifname, "wlan0") == 0) ? WIFI_IF_STA : WIFI_IF_AP;

	if (!en) {
		bool promiscuous_en;

		wise_wifi_get_promiscuous(interface, &promiscuous_en);
		/* if the promisc mode is already disabled on this interface,
		 * and the setting is to disable the promisc mode,
		 * we just return OK
		 */
		if (!promiscuous_en)
			return WISE_OK;
	}

	if (wise_wifi_set_promiscuous(interface, en)) {
		return WISE_FAIL;
	}

	PROMISC_SET_ENABLED(en);

	return WISE_OK;
}

int scm_wifi_get_promiscuous(const char *ifname, bool *en)
{
	wifi_interface_t interface;

	if (!(strcmp(ifname, "wlan0") == 0 || strcmp(ifname, "wlan1") == 0)) {
		SCM_ERR_LOG(SCM_API_TAG,"wrong ifname\n");
		return WISE_FAIL;
	}

	interface = (strcmp(ifname, "wlan0") == 0) ? WIFI_IF_STA : WIFI_IF_AP;

	if (wise_wifi_get_promiscuous(interface, en)) {
		return WISE_FAIL;
	}

    return WISE_OK;
}

#endif



#ifdef __NOT_YET__

int scm_wifi_raw_scan(const char *ifname,
                         scm_wifi_scan_params *custom_scan_param, scm_wifi_scan_no_save_cb cb)
{
  return WISE_OK;
}

int scm_wifi_enable_intrf_mode(const char* ifname, unsigned char enable, unsigned short flag)
{
  return WISE_OK;
}

int scm_wifi_enable_anti_microwave_intrf(unsigned char enable)
{
  return WISE_OK;
}

#endif

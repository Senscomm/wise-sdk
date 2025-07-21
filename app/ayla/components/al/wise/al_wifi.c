/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdio.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ada/err.h>
#include <ayla/log.h>
#include <ayla/timer.h>
#include <ayla/nameval.h>
#include <ayla/wifi_error.h>
#include <ayla/mod_log.h>
#include <ayla/callback.h>

#include <al/al_net_if.h>
#include <al/al_os_mem.h>
#include <al/al_wifi.h>

#include <u-boot/list.h>
#include <dhcps/dhcps.h>

#include <platform/pfm_net_if.h>
#include <platform/pfm_ada_thread.h>

#include <wise_event_loop.h>
#include <wise_wifi_types.h>
#include <wise_wifi.h>
#include <wise_err.h>
#include <scm_wifi.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <cmsis_os.h>

/* TODO: wise api not yet support aync scan */
#define ASYNC_SCAN				0

#define PFM_WIFI_AP_MODE_CHAN	3	/* AP mode channel */
#define PFM_WIFI_STA_NET_NAME   "sta"   /* selected STA profile name */
#define PFM_WIFI_AP_NET_NAME    "ap"    /* AP-mode network name */
#define PFM_WIFI_COUNTRY_DEFAULT "01"	/* ESP32 world-wide safe region */
#define PFM_WIFI_COUNTRY_EUROPE	"EU"
#define PFM_WIFI_COUNTRY_GERMANY "DE"

#define PFM_WIFI_MAX_START_DELAY 8000	/* (ms) max wait for start */
#define PFM_WIFI_MAX_STOP_DELAY	5000	/* (ms) max wait for stop */
#define MAX_SUPPORTED_CLIENT_CONNECTION  10

/*
 * Scan times were determined experimentally to not drop Android phones
 * as AP-mode clients while scanning.  140 ms was too much for a maximum.
 */
#define PFM_WIFI_SCAN_MIN_MS	110	/* min scan time per channel in ms */
#define PFM_WIFI_SCAN_MAX_MS	130	/* max scan time per channel in ms */

#ifndef min
#define min(a, b) ((a < b) ? (a) : (b))
#endif

struct pfm_wifi_state {
	u8 enable_sta;		/* STA interface needed */
	u8 enable_ap;		/* AP interface needed */
	u8 shutdown;		/* non-zero if system shutting down */
	u8 country_eu;		/* non-zero if EU specified as country code */
	u8 connected;		/* non-zero if STA interface connected */
	u8 sta_if_started;
	u8 ap_if_started;
	enum wifi_error join_err;   /* error from last join event */
	void (*scan_cb)(struct al_wifi_scan_result *);
				/* scan_cb is non-NULL if scan in progress */
	int (*event_cb)(enum al_wifi_event, void *arg);
	void *event_arg;
	enum al_wifi_event event;
	osSemaphoreId_t event_sem;
	int32_t event_to_wait;
	struct netif *wise_ap;
	struct netif *wise_sta;
	const char *hostname_sta; /* hostname for station interface */
};

static struct pfm_wifi_state pfm_wifi_state;

static struct callback pfm_wifi_scan_callback;
static struct callback pfm_wifi_event_callback;
#if (ASYNC_SCAN == 0)
static struct callback pfm_wifi_scan_start_callback;
static wifi_scan_config_t scan_config;
#endif

static void pfm_wifi_check_stop(struct pfm_wifi_state *pws);

static void pfm_wifi_log(const char *fmt, ...) ADA_ATTRIB_FORMAT(1, 2);

static void pfm_wifi_log(const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_put_va(MOD_LOG_WIFI, fmt, args);
	ADA_VA_END(args);
}

/*
 * Return description of common WISE errors.
 */
static const char *pfm_wifi_err_str(int err_code)
{
	switch (err_code) {
	case WISE_OK:
		return "none";
	case WISE_ERR_INVALID_ARG:
		return "param";
	case WISE_ERR_NO_MEM:
		return "nomem";
	case WISE_ERR_NOT_FOUND:
		return "Not found";
	case WISE_ERR_INVALID_STATE:
		return "invalid state";
	case WISE_ERR_INVALID_SIZE:
		return "invalid size";
	case WISE_ERR_INVALID_RESPONSE:
		return "invalid response";
	case WISE_ERR_TIMEOUT:
		return "timeout";
	case WISE_ERR_INVALID_CRC:
		return "invalid crc";
	case WISE_ERR_INVALID_VERSION:
		return "invalid version";
	case WISE_ERR_NOT_SUPPORTED:
		return "unsupported";
	case WISE_ERR_WIFI_NOT_STARTED:
		return "not started";
	case WISE_ERR_INVALID_MAC:
		return "invalid mac address";
	case WISE_ERR_WIFI_IF:
		return "WiFi interface error";
	case WISE_ERR_WIFI_NVS:
		return "wifi internal NVS Error";
	case WISE_ERR_WIFI_MAC:
		return "invalid wifi MAC address";
	case WISE_ERR_WIFI_SSID:
		return "bssid_not_in_scan_list";
	case WISE_ERR_WIFI_NOT_STOPPED:
		return "wifi not stopped";
	case WISE_ERR_WIFI_MODE:
		return "wifi mode error";
	case WISE_ERR_WIFI_PASSWORD:
		return "invalid password";
	case WISE_ERR_WIFI_WAKE_FAIL:
		return "WiFi is in sleep state(RF closed) and wakeup fail";
	/* TODO: There is no this error code our API, do we need? */
#ifndef SCM_PORT
	case ESP_ERR_ESP_NETIF_IF_NOT_READY:
		return "netif not ready";
#endif
	default:
		return "unknown err";;
	}
	return "";
}

static void pfm_wifi_log_err(const char *func, int rc)
{
	const char *sign = "";

	if (rc < 0) {
		sign = "-";
		rc = -rc;
	}
	pfm_wifi_log(LOG_ERR "pfm_wifi: %s failed rc %s0x%x %s",
	    func, sign, rc, pfm_wifi_err_str(rc));
}

static enum al_wifi_sec pfm_wifi_sec_import(u32 wmi_sec)
{
	switch (wmi_sec) {
	case WIFI_AUTH_WEP:
		return AL_WIFI_SEC_WEP;
	case WIFI_AUTH_WPA_PSK:
		return AL_WIFI_SEC_WPA;
	case WIFI_AUTH_WPA2_PSK:
	case WIFI_AUTH_WPA_WPA2_PSK:
		return AL_WIFI_SEC_WPA2;
	/*  WIFI_AUTH_WPA3_PSK/WIFI_AUTH_WPA2_WPA3_PSK -> WIFI_AUTH_WPA3_SAE */
	case WIFI_AUTH_WPA3_SAE:
		return AL_WIFI_SEC_WPA3;
	case WIFI_AUTH_OPEN:
		return AL_WIFI_SEC_NONE;
#ifdef CONFIG_WPA2_ENTP
	case WIFI_AUTH_WPA2_ENTERPRISE:
		return AL_WIFI_SEC_NONE;        /* not supported by Ayla */
#endif
	default:
		break;
	}
	return 0;
}

static struct netif * pfm_wifi_get_netif(int idx)
{
	struct netif *netif;
	wise_err_t ret;

	ret = wise_wifi_get_netif(idx, &netif);
	if (ret) {
		return NULL;
	}

	return netif;
}

int al_wifi_powersave_set(enum al_wifi_powersave_mode mode)
{
	wifi_ps_type_t ps_type;
	wise_err_t err;

	switch (mode) {
	case AL_WIFI_PS_OFF:
		ps_type = WIFI_PS_NONE;
		break;
	case AL_WIFI_PS_ON:
		ps_type = WIFI_PS_MIN_MODEM;
		break;
	case AL_WIFI_PS_ON_LESS_BEACONS:
		ps_type = WIFI_PS_MAX_MODEM;
		break;
	default:
		pfm_wifi_log(LOG_ERR "%s: unsupported mode %u", __func__, mode);
		return -1;
	}
	err = wise_wifi_set_ps(ps_type);
	if (err) {
		pfm_wifi_log(LOG_ERR "%s: set_ps err -%#x", __func__, (unsigned int)err);
		return -1;
	}
	return 0;
}

int al_wifi_get_rssi(s16 *rssip)
{
	wifi_ap_record_t ap_info;
	wise_err_t rc;

	rc = wise_wifi_sta_get_ap_info(&ap_info, WIFI_IF_STA);
	if (rc) {
		pfm_wifi_log_err("get_ap_info", rc);
		return -1;
	}
	*rssip = ap_info.rssi;
	return 0;
}

int al_wifi_tx_power_set(u8 tx_power)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;
	wise_err_t err;
	int8_t power;
	uint8_t wifi_if;

	if (pws->ap_if_started) {
		wifi_if = WIFI_IF_AP;
	} else {
		wifi_if = WIFI_IF_STA;
	}

	/*
	 * TODO: How to convert Power dBm utit
	 * ex)convert dBm to ESP's 0.25 dBm units
	 *    power = tx_power * 4;
	 */
	power = tx_power;
	err = wise_wifi_set_max_tx_power(power, wifi_if);
	if (err) {
		pfm_wifi_log_err("set_tx_power", err);
		return -1;
	}
	return 0;
}

int al_wifi_tx_power_get(u8 *tx_powerp)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;
	wise_err_t err;
	int8_t power;
	uint8_t wifi_if;

	if (pws->ap_if_started) {
		wifi_if = WIFI_IF_AP;
	} else {
		wifi_if = WIFI_IF_STA;
	}

	err = wise_wifi_get_max_tx_power(&power, wifi_if);
	if (err) {
		pfm_wifi_log_err("get_tx_power", err);
		return -1;
	}

	/*
	 * TODO: How to convert Power dBm utit
	 * ex)convert dBm to ESP's 0.25 dBm units
	 *    power /= 4;
	 */

	*tx_powerp = (u8)power;
	return 0;
}

void al_wifi_set_event_cb(void *arg,
		int (*event_cb)(enum al_wifi_event, void *arg))
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;

	pws->event_cb = event_cb;
	pws->event_arg = arg;
}

/*
 * Deliver event to ADW.
 */
static void pfm_wifi_event_cb(void *arg)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;
	enum al_wifi_event event = pws->event;
	enum wifi_error err;

	switch (event) {
	case AL_WIFI_EVENT_STA_LOST:
		err = pws->join_err;
		if (err && err != WIFI_ERR_IN_PROGRESS) {
			pws->enable_sta = 0;
			pfm_wifi_check_stop(pws);
		}
		break;
	case AL_WIFI_EVENT_STA_UP:
		/* TODO: ESP dhcp start in default handler */
		netifapi_dhcp_start(pws->wise_sta);
		break;
	default:
		break;
	}
	if (pws->event_cb) {
		pws->event_cb(event, pws->event_arg);
	}
}

static void pfm_wifi_event(enum al_wifi_event event)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;

	pws->event = event;
	pfm_callback_pend(&pfm_wifi_event_callback);
}

static void pfm_wifi_scan_import(struct al_wifi_scan_result *scan,
		wifi_ap_record_t *res)
{
	memset(scan, 0, sizeof(*scan));

	scan->ssid.len = strlen((const char *)res->ssid);
	memcpy(scan->ssid.id, res->ssid, scan->ssid.len);
	memcpy(scan->bssid, res->bssid, sizeof(scan->bssid));
	scan->channel = res->primary;

	scan->type = AL_WIFI_BT_INFRASTRUCTURE;
	scan->rssi = res->rssi;

	scan->wmi_sec = pfm_wifi_sec_import(res->authmode);
}

/*
 * Start Wi-Fi in the appropriate mode for join or scan.
 * Waits for start event.
 */
static int pfm_wifi_start(struct pfm_wifi_state *pws, uint8_t wifi_if)
{
	wise_err_t rc;
	wifi_mode_t old_mode, mode;

	wise_wifi_get_mode(&old_mode, wifi_if);

	if (wifi_if == WIFI_IF_AP) {
		if (pws->ap_if_started) {
			return WIFI_ERR_NONE;
		}
		pws->event_to_wait = SYSTEM_EVENT_AP_START;
		mode = WIFI_MODE_AP;
	} else {
		if (pws->sta_if_started) {
			return WIFI_ERR_NONE;
		}
		pws->event_to_wait = SYSTEM_EVENT_STA_START;
		mode = WIFI_MODE_STA;
	}
	if (mode != old_mode) {
		wise_wifi_stop(wifi_if);
		rc = wise_wifi_set_mode(mode, wifi_if);
		if (rc != WISE_OK) {
			pfm_wifi_log_err("pfm_wifi_sta_start : mode", rc);
			return WIFI_ERR_MEM;
		}
	}

	pfm_wifi_log(LOG_DEBUG2 "pfm_wifi_start");

	rc = wise_wifi_start(wifi_if);
	if (rc != WISE_OK) {
		pfm_wifi_log_err("pfm_wifi_sta_start : start", rc);
		return WIFI_ERR_MEM;
	}

	/*
	 * Wait for SYSTEM_EVENT_STA_START or SYSTEM_EVENT_AP_START
	 */
	if (osSemaphoreAcquire(pws->event_sem, PFM_WIFI_MAX_START_DELAY) != osOK) {
		pfm_wifi_log(LOG_WARN "Failed to get start event.");
		return WIFI_ERR_MEM;
	}

	if (wifi_if == WIFI_IF_AP) {
		pws->ap_if_started = 1;
	} else {
		pws->sta_if_started = 1;
	}

	pfm_wifi_log(LOG_DEBUG2 "pfm_wifi_start done");

	return 0;
}

/*
 * Stop Wi-Fi.
 * Waits for stop event.
 */
static void pfm_wifi_stop(struct pfm_wifi_state *pws, uint8_t wifi_if)
{
	wise_err_t rc;

	pfm_wifi_log(LOG_DEBUG2 "pfm_wifi_stop");

	if (wifi_if == WIFI_IF_AP) {
		if (!pws->ap_if_started) {
			return;
		}

		pws->event_to_wait = SYSTEM_EVENT_AP_STOP;
	} else {
		if (!pws->sta_if_started) {
			return;
		}

		pws->event_to_wait = SYSTEM_EVENT_STA_STOP;
	}

	rc = wise_wifi_stop(wifi_if);

	if (pws->shutdown) {
		return;
	}
	if (rc != WISE_OK) {
		pfm_wifi_log_err("pfm_wifi_stop: stop", rc);
		return;
	}

	rc = wise_wifi_set_mode(WIFI_MODE_NULL, wifi_if);
	if (rc != WISE_OK) {
		pfm_wifi_log_err("pfm_wifi_stop: set_mode", rc);
		return;
	}

	/*
	 * Wait for SYSTEM_EVENT_STA_STOP or SYSTEM_EVENT_AP_STOP
	 */
	if (osSemaphoreAcquire(pws->event_sem, PFM_WIFI_MAX_STOP_DELAY) != osOK) {
		pfm_wifi_log(LOG_WARN "Failed to get stop event.");
		return;
	}

	if (wifi_if == WIFI_IF_AP) {
		pws->ap_if_started = 0;
	} else {
		pws->sta_if_started = 0;
	}

	pfm_wifi_log(LOG_DEBUG2 "pfm_wifi_stop done");
}

/*
 * Stop Wi-Fi if nothing enabled.
 * Waits for stop event.
 */
static void pfm_wifi_check_stop(struct pfm_wifi_state *pws)
{
	if (pws->enable_sta == 0 && !pws->scan_cb) {
		pfm_wifi_stop(pws, WIFI_IF_STA);
	}

	if (pws->enable_ap == 0) {
		pfm_wifi_stop(pws, WIFI_IF_AP);
	}
}

/*
 * Handle scan results.
 */
static void pfm_wifi_scan_cb(void *arg)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;
	wifi_ap_record_t *res = NULL;
	struct al_wifi_scan_result scan;
	int i = 0;
	wise_err_t err;
	uint16_t ap_count;
	void (*scan_cb)(struct al_wifi_scan_result *);

	scan_cb = pws->scan_cb;
	if (!scan_cb) {
		return;		/* ignore repeated scan done events */
	}

	pws->scan_cb = NULL;

	ap_count = 0;
	err = wise_wifi_scan_get_ap_num(&ap_count);
	if (err) {
		pfm_wifi_log_err("scan_get_ap_num", err);
		goto fail;
	}

	pfm_wifi_log(LOG_INFO "scan count %u", ap_count);
	res = al_os_mem_calloc(sizeof(*res) * ap_count);
	if (!res) {
		pfm_wifi_log(LOG_ERR "scan alloc failed");
		goto fail;
	}

	err = wise_wifi_scan_get_ap_records(0, &ap_count, res);
	if (err) {
		pfm_wifi_log_err("get_scan_res", err);
		goto fail;
	}
	for (i = 0; i < ap_count; i++) {
		pfm_wifi_scan_import(&scan, &res[i]);
		scan_cb(&scan);
	}

fail:
	free(res);
	scan_cb(NULL);

#ifdef CONFIG_WPA_SUPPLICANT_NO_SCAN_RES
	wise_wifi_flush_bss();
#endif

	/*
	 * Stop if not associated or in AP mode.
	 */
	pfm_wifi_check_stop(pws);
}

#if (ASYNC_SCAN == 0)
static void pfm_wifi_scan_start_cb(void *arg)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;
	wise_err_t rc;

	/* TODO: not yet prepare asynchronous scan */
	rc = wise_wifi_scan_start(&scan_config, true, WIFI_IF_STA);
	if (rc != WISE_OK) {
		pfm_wifi_log_err("wise_wifi_scan_start", rc);
		pws->scan_cb = NULL;
	}
}
#endif

int al_wifi_scan(struct al_wifi_ssid *spec_ssid,
		void (*callback)(struct al_wifi_scan_result *))
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;
#if (ASYNC_SCAN == 1)
	wifi_scan_config_t scan_config;
	wise_err_t rc;
#endif

	pfm_wifi_log(LOG_DEBUG2 "pfm_wifi_scan");
	if (pws->scan_cb) {
		pfm_wifi_log(LOG_ERR "pfm_wifi_scan already in progress");
		return WIFI_ERR_IN_PROGRESS;
	}
	memset(&scan_config, 0, sizeof(wifi_scan_config_t));

	if (spec_ssid) {
		scan_config.ssid = spec_ssid->id;
		scan_config.spec_ssid = true;
	}
	scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
	scan_config.scan_time.active.min = PFM_WIFI_SCAN_MIN_MS;
	scan_config.scan_time.active.max = PFM_WIFI_SCAN_MAX_MS;

	pws->scan_cb = callback;
	if (pfm_wifi_start(pws, WIFI_IF_STA)) {
		pfm_wifi_log(LOG_ERR "start WIFI STA failed");
		pws->scan_cb = NULL;
		return WIFI_ERR_MEM;
	}

#if (ASYNC_SCAN == 1)
	/*
	 * Start asynchronous scan.
	 */
	rc = wise_wifi_scan_start(&scan_config, false, WIFI_IF_STA);
	if (rc != WISE_OK) {
		pfm_wifi_log_err("wise_wifi_scan_start", rc);
		pws->scan_cb = NULL;
		return WIFI_ERR_MEM;    /* XXX maybe not accurate */
	}
#else
	pfm_callback_pend(&pfm_wifi_scan_start_callback);
#endif

	return 0;
}

/*
 * Start join.
 * Returns non-zero on error and sets pws->join_err.
 */
static int pfm_wifi_join(struct al_wifi_ssid *ssid,
		const struct al_wifi_key *key, enum al_wifi_sec sec,
		struct al_wifi_scan_result *scan)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;
	wifi_config_t wifi_config;
	wise_err_t err;

	memset(&wifi_config, 0, sizeof(wifi_config));
	memcpy(&wifi_config.sta.ssid, ssid->id, ssid->len);
	if (scan) {
		wifi_config.sta.channel = scan->channel;
	}

	/* TODO: Need to comfirm */
	switch (sec) {
	case AL_WIFI_SEC_NONE:
		break;
	case WIFI_AUTH_WEP:
		wifi_config.sta.alg = AUTH_ALG_WEP;
		break;
	case AL_WIFI_SEC_WPA:
		wifi_config.sta.alg = AUTH_ALG_CCMP;
		wifi_config.sta.proto = WIFI_PROTO_WPA;
		break;
	case AL_WIFI_SEC_WPA2:
		wifi_config.sta.alg = AUTH_ALG_CCMP;
		wifi_config.sta.proto = WIFI_PROTO_WPA2;
		break;
	case AL_WIFI_SEC_WPA3:
		wifi_config.sta.alg = AUTH_ALG_SAE;
		wifi_config.sta.proto = WIFI_PROTO_WPA2;
		wifi_config.sta.pmf_mode = WIFI_PMF_REQUIRED;
		break;
	default:
		pfm_wifi_log(LOG_ERR "unknown sercurity %d", sec);
		return -1;
		break;
	}

	if (sec != AL_WIFI_SEC_NONE) {
		memcpy(wifi_config.sta.password, key->key, key->len);
	}

	if (pws->shutdown) {
		return -1;
	}

	/* Disconnect[disable_network] before Connect[enable_network] */
	wise_wifi_disconnect(WIFI_IF_STA);
	pws->enable_sta = 0;
	pfm_wifi_check_stop(pws);

	pws->enable_sta = 1;
	pws->join_err = pfm_wifi_start(pws, WIFI_IF_STA);
	if (pws->join_err) {
		return -1;
	}

	err = wise_wifi_set_config(WISE_IF_WIFI_STA, &wifi_config, WIFI_IF_STA, NULL);
	if (err) {
		pfm_wifi_log_err("wise_wifi_set_config", err);
		pws->join_err = WIFI_ERR_MEM;
		return -1;
	}

	pws->join_err = WIFI_ERR_IN_PROGRESS;

	err = wise_wifi_connect(WIFI_IF_STA);
	if (err) {
		pfm_wifi_log_err("wise_wifi_connect", err);
		return -1;
	}
	return 0;
}

int al_wifi_join(struct al_wifi_ssid *ssid, const struct al_wifi_key *key,
		enum al_wifi_sec sec)
{
	return pfm_wifi_join(ssid, key, sec, NULL);
}

int al_wifi_join_from_scan(struct al_wifi_scan_result *scan,
	const struct al_wifi_key *key)
{
	return pfm_wifi_join(&scan->ssid, key, scan->wmi_sec, scan);
}

void al_wifi_leave(void)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;
	wise_err_t rc;

	pfm_wifi_log(LOG_DEBUG "leave");

	/*
	 * Can be called after failed join when already stopped.
	 */
	if (!pws->sta_if_started) {
		return;
	}

	if (pws->connected) {
		wifi_ip_info_t ip_info = {0};

		/* TODO: ESP calling action_net_stop() will also stop dhcp if it is running */
		netifapi_dhcp_stop(pws->wise_sta);
		wise_wifi_set_ip_info(WIFI_IF_STA, &ip_info, true, true);
	}
	/*
	 * Disconnect even if connection wasn't successful.
	 */
	rc = wise_wifi_disconnect(WIFI_IF_STA);
	if (rc != WISE_OK && !pws->shutdown) {
		pws->event_to_wait = -1;
		pfm_wifi_log_err("wise_wifi_disconnect", rc);
		return;
	}

	pws->enable_sta = 0;
	pfm_wifi_check_stop(pws);
}

enum wifi_error al_wifi_join_status_get(void)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;

	if (pws->shutdown) {
		return WIFI_ERR_IN_PROGRESS;
	}
	return pws->join_err;
}

/*
 * Start AP mode.
 * Only handles open AP mode for now - no Wi-Fi security.
 */
int al_wifi_start_ap(struct al_wifi_ssid *ssid, u32 ip, u32 mask)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;
	wifi_config_t wifi_config;
	wifi_ip_info_t ip_info;
	wise_err_t err;

	/*
	 * Init and stop the DHCP server
	 * The default event handler in components/tcpip_adapter
	 * will bring up DHCPS when the AP starts.
	 */
	netifapi_dhcps_stop(pws->wise_ap);
#ifndef SCM_PORT /* TODO: If dhcps is not started, return error */
	if (err) {
		pfm_wifi_log_err("dhcps_stop", err);
		return -1;
	}
#endif

	err = wise_wifi_set_mode(WIFI_MODE_AP, WIFI_IF_AP);
	if (err) {
		pfm_wifi_log_err("set_mode AP", err);
		return -1;
	}

	/*
	 * assign a static IP to the network interface
	 */
	memset(&ip_info, 0, sizeof(ip_info));
	ip_info.ip.addr = htonl(ip);
	ip_info.gw.addr = htonl(ip);
	ip_info.nm.addr = htonl(mask);
	err = wise_wifi_set_ip_info(WIFI_IF_AP, &ip_info, true, true);
	if (err) {
		pfm_wifi_log_err("set_ip_info AP", err);
		return -1;
	}

	/* start the DHCP server - this actually only re-inits it */
	err = netifapi_dhcps_start(pws->wise_ap);
	if (err) {
		pfm_wifi_log_err("dhcps_start", err);
		return -1;
	}

	pws->enable_ap = 1;

	err = wise_wifi_clean_config(WISE_IF_WIFI_AP, WIFI_IF_AP);
	if (err) {
		pws->enable_ap = 0;
		return -1;
	}

	memset(&wifi_config, 0, sizeof(wifi_config));
	memcpy(wifi_config.ap.ssid, ssid->id, ssid->len);
	wifi_config.ap.ssid_len = ssid->len;
	wifi_config.ap.max_connection = MAX_SUPPORTED_CLIENT_CONNECTION;
	wifi_config.ap.channel = PFM_WIFI_AP_MODE_CHAN;
	wifi_config.ap.alg = WIFI_AUTH_OPEN;

	err = wise_wifi_set_config(WISE_IF_WIFI_AP, &wifi_config, WIFI_IF_AP, NULL);
	if (err) {
		pws->enable_ap = 0;
		return -1;
	}

	if (pfm_wifi_start(pws, WIFI_IF_AP)) {
		pfm_wifi_log(LOG_ERR "failed start AP");
		pws->enable_ap = 0;
		return -1;
	}

	pfm_wifi_log(LOG_INFO "AP channel %u", wifi_config.ap.channel);

	return 0;
}

void al_wifi_stop_ap(void)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;

	pws->enable_ap = 0;

	if (pws->shutdown) {
		goto out;
	}

out:
	pfm_wifi_check_stop(pws);
}

static wise_err_t pfm_wifi_wifi_event(void *ctx, system_event_t *event)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;
	system_event_sta_scan_done_t *scan_ev;
	system_event_sta_disconnected_t *disc_ev;
	const char *msg = NULL;
	const char ap_net[] = "AP: ";
	const char *net = "STA: ";
	wise_err_t rc;
	wifi_mode_t mode;

	if (pws->shutdown) {
		return WISE_FAIL;		/* ignore events while shutting down */
	}

	switch (event->event_id) {
	case SYSTEM_EVENT_WIFI_READY:
		msg = "wifi ready";
		net = "";
		break;
	case SYSTEM_EVENT_SCAN_DONE:
		scan_ev = &event->event_info.scan_done;
		pfm_wifi_log(LOG_DEBUG2
		    "scan event: status %"PRIu32" number %u scan_id %u",
		    (unsigned long int)scan_ev->status, scan_ev->number, scan_ev->scan_id);
		pfm_callback_pend(&pfm_wifi_scan_callback);
		msg = "scan done";
		net = "";
		break;
	case SYSTEM_EVENT_STA_START:
		log_thread_id_set("v");	/* first event - mark event thread */
		msg = "started";
		break;
	case SYSTEM_EVENT_STA_CONNECTED:
		msg = "connected";
		pws->join_err = WIFI_ERR_NONE;
		pws->connected = 1;
		/* It might have happened due to wpa_supplicant reconnection
		 * without upper layer knowing it.
		 * Make sure STA i/f will have the right mode set.
		 */
		if (wise_wifi_get_mode(&mode, WIFI_IF_STA) == WISE_OK
			&& mode != WIFI_MODE_STA) {
			rc = wise_wifi_set_mode(WIFI_MODE_STA, WIFI_IF_STA);
			if (rc != WISE_OK) {
				pfm_wifi_log_err("set_mode %x", rc);
			}
		}
		pfm_wifi_event(AL_WIFI_EVENT_STA_UP);
		break;
	case SYSTEM_EVENT_STA_STOP:
		msg = "stopped";
		break;
	case SYSTEM_EVENT_AP_START:
		net = ap_net;
		msg = "started";
		break;
	case SYSTEM_EVENT_AP_STOP:
		net = ap_net;
		msg = "stopped";
		break;
	case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
		msg = "authentication mode changed";
		break;
	case SYSTEM_EVENT_AP_STACONNECTED:
		net = ap_net;
		msg = "STA connected";
		break;
	case SYSTEM_EVENT_AP_STADISCONNECTED:
		net = ap_net;
		msg = "STA disconnected";
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		msg = "disconnected";
		disc_ev = &event->event_info.disconnected;

		/*
		 * A disconnected event when not connected may indicate a join
		 * failure.
		 */
		if (!pws->connected && pws->join_err != WIFI_ERR_IN_PROGRESS) {
			msg = "extra disconnected - extraneous";
			pfm_wifi_log(LOG_DEBUG2 "wmi event %s reason %u",
			    msg, disc_ev->reason);
			break;
		}

		pfm_wifi_log(LOG_DEBUG2 "wmi event %s reason %u",
		    msg, disc_ev->reason);

		switch (disc_ev->reason) {
		case WIFI_REASON_NO_AP_FOUND:
			pws->join_err = WIFI_ERR_NOT_FOUND;
			msg = "SSID not found";
			break;
		case WIFI_REASON_AUTH_LEAVE:
			pws->join_err = WIFI_ERR_LOS;
			msg = "auth leave";
			break;
		case WIFI_REASON_ASSOC_EXPIRE:
		case WIFI_REASON_ASSOC_FAIL:
		case WIFI_REASON_ASSOC_LEAVE:
		case WIFI_REASON_ASSOC_TOOMANY:
		case WIFI_REASON_DISASSOC_PWRCAP_BAD:
		case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
		case WIFI_REASON_IE_INVALID:
		case WIFI_REASON_MIC_FAILURE:
		case WIFI_REASON_NOT_ASSOCED:
		case WIFI_REASON_UNSPECIFIED:
			pws->join_err = WIFI_ERR_AP_DISC;
			msg = "disassociated";
			break;
		case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
			pws->join_err = WIFI_ERR_WRONG_KEY;
			msg = "incorrect key"; /* most likely bad key */
			break;
		case WIFI_REASON_AKMP_INVALID:
		case WIFI_REASON_ASSOC_NOT_AUTHED:
		case WIFI_REASON_GROUP_CIPHER_INVALID:
		case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
		case WIFI_REASON_HANDSHAKE_TIMEOUT:
		case WIFI_REASON_IE_IN_4WAY_DIFFERS:
		case WIFI_REASON_AUTH_FAIL:
		case WIFI_REASON_NOT_AUTHED:
		case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
			pws->join_err = WIFI_ERR_NOT_AUTH;
			msg = "not authenticated"; /* unusual issues */
			break;
		case WIFI_REASON_BEACON_TIMEOUT:
			pws->join_err = WIFI_ERR_LOS;
			msg = "beacon timeout";
			break;
		default:
			pws->join_err = WIFI_ERR_LOS;
			msg = "unknown";
			break;
		}

		pfm_wifi_event(AL_WIFI_EVENT_STA_LOST);
		pws->connected = 0;
		break;
	case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
		msg = "WPS success";
		break;
	case SYSTEM_EVENT_STA_WPS_ER_FAILED:
		msg = "WPS failed";
		break;
	case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
		msg = "WPS timeout";
		break;
	case SYSTEM_EVENT_STA_WPS_ER_PIN:
		msg = "WPS PIN";
		break;
	case SYSTEM_EVENT_AP_PROBEREQRECVED:
		msg = "recvd probe req";
		net = ap_net;
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		pfm_wifi_log(LOG_DEBUG2 "wmi event STA got IP");
		break;
	default:
		pfm_wifi_log(LOG_DEBUG2 "wmi event %#"PRIx32"", (unsigned long int)event->event_id);
		break;
	}
	if (msg) {
		pfm_wifi_log(LOG_INFO "wmi event %s%s", net, msg);
	}
	if (pws->event_to_wait > 0 && event->event_id == pws->event_to_wait) {
		pws->event_to_wait = -1;
		osSemaphoreRelease(pws->event_sem);
	}

	return WISE_OK;
}

int pfm_wifi_get_mac_addr(int ap, u8 *mac)
{
	(void)ap;

	wise_wifi_get_mac(mac, WIFI_IF_STA);

	return 0;
}

/*
 * Set country code.
 * Note: setting must work even if Wi-Fi is not started yet.
 * This is used as a way to validate the country code for the CLI.
 */
#define SCM_WIFI_COUNTRY_DEFAULT "01"	/* world-wide safe region */
#define SCM_WIFI_COUNTRY_CODE_LEN 3
int al_wifi_country_code_set(const char *country_code)
{
	bool use_ieee80211d = false;
	char cc_buf[SCM_WIFI_COUNTRY_CODE_LEN];		/* Country code string 3 is enough */
	struct pfm_wifi_state *pws = &pfm_wifi_state;

	if (!country_code || !country_code[0]) {
		country_code = SCM_WIFI_COUNTRY_DEFAULT;
		use_ieee80211d = true;	/* uses the configured AP's CC */
	}

	memcpy(cc_buf, country_code, sizeof(cc_buf));

	if (wise_wifi_set_country_code(cc_buf, use_ieee80211d, pws->enable_ap))
		return WISE_FAIL;

	return WISE_OK;
}

/*
 * Get country code for display of current setting.
 */
int al_wifi_country_code_get(char *buf, size_t len)
{
	int ret = WISE_FAIL;
	char *country_code = buf;
	size_t slen;

	if (len < SCM_WIFI_COUNTRY_CODE_LEN)
		goto done;

	ret = wise_wifi_get_country_code(country_code);
	if (ret) {
		goto done;
	}

	slen = strlen(country_code);
	if (slen >= len) {
		goto done;;
	}

done:
	return ret;
}

/*
 * Set the network hostname.
 */
void al_wifi_hostname_set(const char *hostname)
{
#if LWIP_NETIF_HOSTNAME
	struct pfm_wifi_state *pws = &pfm_wifi_state;
	struct netif *netif;

    pws->hostname_sta = hostname;
    netif = pfm_wifi_get_netif(WIFI_IF_STA);

	if (!netif || !hostname || !hostname[0]) {
		return;
	}

	netifapi_netif_set_hostname(netif, hostname);
#endif
}

/*
 * Init and start network layer, without starting Wi-Fi.
 */
static int pfm_wifi_init_net(void)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;

	if (pws->hostname_sta) {
#if LWIP_NETIF_HOSTNAME
		struct netif *netif;

		netif = pfm_wifi_get_netif(WIFI_IF_STA);
		netif_set_hostname(netif, pws->hostname_sta);
#endif
	}

	pws->wise_sta = pfm_wifi_get_netif(WIFI_IF_STA);
	AYLA_ASSERT(pws->wise_sta);
	if (pfm_net_if_attach(AL_NET_IF_STA, pws->wise_sta)) {
		pfm_wifi_log(LOG_ERR "init_net: attach STA failed");
		return -1;
	}

	pws->wise_ap = pfm_wifi_get_netif(WIFI_IF_AP);
	AYLA_ASSERT(pws->wise_ap);
	if (pfm_net_if_attach(AL_NET_IF_AP, pws->wise_ap)) {
		pfm_wifi_log(LOG_ERR "init_net: attach AP failed");
		return -1;
	}

	return 0;
}

#ifndef SCM_PORT /* TODO: We don't have shutdown callback */
/*
 * Shutdown handler.
 * Ignore events after this is called.
 */
static void pfm_wifi_shutdown(void)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;

	pws->shutdown = 1;
	pfm_wifi_log(LOG_DEBUG "wmi: shutdown");
	al_wifi_off();
}
#endif

int al_wifi_init(void)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	uint8_t mac[6]; /* ETH_ALEN */
	wise_err_t rc;

	if (pws->event_sem) {
		return 0;     /* already initialized */
	}

	pws->event_to_wait = -1;
	pws->event_sem = osSemaphoreNew(1, 0, NULL);
	assert(pws->event_sem);

#if (ASYNC_SCAN == 0)
	callback_init(&pfm_wifi_scan_start_callback, pfm_wifi_scan_start_cb, NULL);
#endif
	callback_init(&pfm_wifi_scan_callback, pfm_wifi_scan_cb, NULL);
	callback_init(&pfm_wifi_event_callback, pfm_wifi_event_cb, NULL);

	wise_event_loop_init(pfm_wifi_wifi_event, NULL);

	rc = wise_wifi_get_mac(mac, WIFI_IF_STA);
	if (rc != WISE_OK) {
		pfm_wifi_log_err("wise_wifi_get_mac", rc);
		return -1;
	}

	memcpy(cfg.mac, mac, sizeof(cfg.mac));

	rc = wise_wifi_init(&cfg);
	if (rc != WISE_OK) {
		pfm_wifi_log_err("wise_wifi_init", rc);
		return -1;
	}

	pfm_wifi_init_net();

	al_wifi_powersave_set(AL_WIFI_PS_OFF);

	rc = wise_wifi_set_mode(WIFI_MODE_STA, WIFI_IF_STA);
	if (rc != WISE_OK) {
		pfm_wifi_log_err("set_mode %x", rc);
		return WIFI_ERR_MEM;
	}

	rc = wise_wifi_set_mode(WIFI_MODE_AP, WIFI_IF_AP);
	if (rc != WISE_OK) {
		pfm_wifi_log_err("set_mode %x", rc);
		return WIFI_ERR_MEM;
	}

	return 0;
}

/*
 * Turn Wi-Fi on.
 * There's no power management when not associated with a network, so
 * Wi-Fi will be turned on only later when AP, STA or scan is needed.
 */
int al_wifi_on(void)
{
	return 0;
}

void al_wifi_off(void)
{
	struct pfm_wifi_state *pws = &pfm_wifi_state;

	pws->enable_ap = 0;
	pws->enable_sta = 0;
	pfm_wifi_check_stop(pws);
}

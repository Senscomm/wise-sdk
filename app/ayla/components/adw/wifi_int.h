/*
 * Copyright 2011-2016 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADW_WIFI_INT_H__
#define __AYLA_ADW_WIFI_INT_H__

#include <ada/metrics.h>
#include <ayla/wifi_error.h>
#include <ayla/nameval.h>
#include <ayla/callback.h>
#include <adw/wifi.h>
#include <al/al_wifi.h>

#define WIFI_MAX_SIG		0
#define WIFI_MIN_SIG		(-200)
#define WIFI_SCAN_MIN_LIMIT	8000	/* ms, minimum scan time limit */
#define WIFI_SCAN_AP_WAIT	30000	/* ms inter-scan wait in AP mode */
#define WIFI_SCAN_DEF_IDLE	10000	/* ms inter-scan wait in idle */
#define WIFI_RSSI_CT		3	/* number of RSSI readings to average */
#define WIFI_RSSI_MIN_DELAY	100	/* min delay between samples, in ms */
#define WIFI_AP_MODE_CHAN	3	/* channel to use for AP mode */
#define WIFI_SERVER_ACT_TIME	20000	/* ms age of max active server */
#define WIFI_PREF_TRY_LIMIT	3	/* times to try new preferred profile */
#define WIFI_JOIN_TRY_LIMIT	3	/* times to try join */
#define WIFI_JOIN_KEY_ERR	10	/* count for key errors */
#define WIFI_JOIN_POLL		20	/* ms between join checks */
#define WIFI_JOIN_TIMEOUT	10000	/* ms before join timeout */
#define WIFI_DHCP_POLL		200	/* ms for DHCP client poll */
#define WIFI_DHCP_WAIT		15000	/* ms timeout for DHCP */
#define WIFI_POLL		17000	/* ms time between join polls */
#define WIFI_STOP_AP_TMO	30000	/* ms stop AP mode after OK connect */
#define WIFI_CMD_RSP_TMO	250	/* ms delay for resp to reach the app */

#define WIFI_MIN_KEY_LEN	8	/* minimum legal pre-shared key len */
#define WIFI_MAX_KEY_LEN	64	/* max key length (any type) */
#define WIFI_WPA_KEY_LEN	32	/* length of hex WPA key (bytes) */
#define WIFI_WEP_KEY_LEN	32	/* length of WEP key (bytes) */
#define ADW_COUNTRY_CODE_LEN	3	/* max length of any country code */

#define WIFI_HIST_CT		5	/* connection histories to keep */
#define WIFI_PS_BCN_ITVL	3	/* beacon listen interval in PS mode */

#define WIFI_EVENT_HANDLER_CT	10	/* max number of event handlers */

/* DHCP APIPA IP:169.254.*.* */
#define ADW_WIFI_APIPA	((169U << 24) | (254 << 16))

#define WIFI_MAX_FAILS		3

/*
 * Antenna selection values: defined for Broadcom firmware compatibility.
 * These are the values in the config files.
 */
PREPACKED_ENUM enum adw_wifi_ant {
	WIFI_ANT0 = 0,		/* use antenna 0 */
	WIFI_ANT1 = 1,		/* use antenna 1 */
	WIFI_ANT_DIV = 3,	/* use antenna diversity */
} PACKED_ENUM;

ASSERT_SIZE(enum, adw_wifi_ant, 1);

struct adw_profile {
	u8 enable:1;		/* enabled in config or by successful join */
	u8 hidden:1;		/* network is possibly hidden */
	u8 spec_scan_done:1;	/* specific scan was done */
	u8 join_errs;		/* count of join errors */
	enum conf_token sec;
	struct al_wifi_scan_result *scan;
	struct al_wifi_ssid ssid;
	struct al_wifi_key phrase;
};

ASSERT_SIZE(enum, conf_token, 1);	/* may grow, but should be constant */

#define WIFI_HIST_DNS_SERVERS	2	/* number of DNS servers for history */

/*
 * History of a Wi-Fi connection attempt.
 */
struct adw_wifi_history {
	u8 ssid_len;		/* length of SSID (zero if record empty) */
	u8 ssid_info[2];	/* first and last bytes of SSID */
	enum wifi_error error;	/* error code */
	u8 bssid[6];		/* BSSID, if known */
	int last:1;		/* last attempt before moving on */
	int curr:1;		/* due to most recent wifi_json_connect_post */
	int logged:1;		/* sent to log server */
	u32 time;		/* time since boot when connection was tried */
	u32 ip_addr;		/* IP address assigned by DHCP */
	u32 netmask;		/* netmask assigned by DHCP */
	u32 def_route;		/* default route from DCHP */
	u32 dns_servers[WIFI_HIST_DNS_SERVERS];	/* DNS servers from DHCP */
};

PREPACKED_ENUM enum adw_wifi_conn_state {
		WS_DISABLED = 0,
		WS_ERROR,
		WS_IDLE,
		WS_RESTART,
		WS_SCAN_DONE,
		WS_JOIN,
		WS_DHCP,
		WS_WAIT_CLIENT,
		WS_UP,
		WS_START_AP,
		WS_UP_AP,
} PACKED_ENUM;

PREPACKED_ENUM enum adw_scan_state {
		SS_IDLE = 0,
		SS_SCAN_START,	/* scan requested */
		SS_SCAN,	/* scan in progress */
		SS_DONE,	/* scan completed */
		SS_SCAN_WAIT,	/* wait for a while between scans */
		SS_STOPPED,	/* no scanning */
} PACKED_ENUM;

ASSERT_SIZE(enum, adw_wifi_conn_state, 1);
ASSERT_SIZE(enum, adw_scan_state, 1);

struct adw_state {
	enum adw_wifi_conn_state state;
	enum adw_scan_state scan_state;
	u8	ap_up:1;
	u8	conditional_ap:1;/* don't enable AP mode if no profiles */
	u8	started:1;
	u8	enable:1;
	u8	save_on_ap_connect:1;
	u8	save_on_server_connect:1;
	u8	power_mode:2;
	u8	rejoin:1;
	u8	scan_report:1;
	u8	send_ant:1;	/* antenna selection should be set */
	u8	init_if_done:1;
	u8	fail_cnt:4;
	u8	reset_cnt:4;
	u8	ap_unsupported; /* not bitfield for atomicity */
	enum ada_err client_err; /* client connection status */
	enum adw_wifi_ant ant;	/* antenna selection */
	u8	curr_profile;	/* current profile index */
	u8	pref_profile;	/* connecting profile index + 1 */
	u8	scan_profile;	/* index + 1 for specific scan if needed */
	s8	listen_itvl;    /* override to automatic listen itvl */
	u32	scan_time;	/* time when scan last started, in ms */
	struct al_wifi_ssid scan4; /* SSID for targeted scan */
	s16	rssi_reading[WIFI_RSSI_CT];
	s16	rssi_total;
	u8	rssi_index;	/* next RSSI to fill in */
	u8	hist_curr;	/* current (most recent) history in use */
	struct adw_wifi_history hist[WIFI_HIST_CT];
	u8	mac[6];		/* configured MAC addr */
	u8	tx_power;	/* configured tx power */
	u8	ap_mode_chan;	/* preferred AP-mode channel if non-zero */
	struct adw_profile profile[ADW_WIFI_PROF_CT];
	struct al_wifi_scan_result scan[ADW_WIFI_SCAN_CT];
	const char *err_msg;
	char err_buf[60];
	u32 use_time;		/* last time adw_wifi_stayup() was called */
	enum wifi_error join_err;	/* error from last join event */
	s8 saved_key_prof;	/* profile index for saved key */
	u8 saved_key_len;	/* key length (0 if not valid) */
	u8 saved_key[WIFI_WPA_KEY_LEN];	/* key from saved scan result */
#if defined(AYLA_ESP32_SUPPORT)
	u8 conn_bssid[6];	/* BSSID from last connect attempt */
#endif
	struct adw_wifi_status status; /* status for adw_wifi_status_get() */
	char country_code[ADW_COUNTRY_CODE_LEN + 1];
};

#ifdef AYLA_EXT_WIFI_FW
extern u8 adw_wifi_fw_ok;	/* wifi firmware is usable or unknown */
#else
#define adw_wifi_fw_ok 1
#endif

/*
 * This is what gets persisted about currently joined wifi network. It is
 * enough for us to join this this network directly without a scan after
 * a restart.
 * Compatibility with other config versions is not necessary, this is
 * just a hint to speed up the initial join.
 */
struct adw_wifi_scan_persist {
	u8 wsp_version;		/* version number of this structure */
	u8 wsp_channel;
	struct al_wifi_ssid wsp_ssid;
	u8 wsp_bssid[6];
	u32 wsp_wmi_security;	/* WMI-specific security code */
	u8 wsp_key_len;
	u8 wsp_key[32];		/* precomputed WPA2 or other key */
};
#define WSP_VERSION		3	/* current version */

#define ADW_SIZE_DEBUG
#ifdef ADW_SIZE_DEBUG
extern size_t adw_state_size;
#define ADW_SIZE_INIT size_t adw_state_size = sizeof(adw_state);
#define ADW_SIZE_CHECK							\
	do {								\
		ASSERT(adw_state_size == sizeof(struct adw_state));	\
	} while (0)
#else
#define ADW_SIZE_INIT
#define ADW_SIZE_CHECK do { } while (0)
#endif

extern struct adw_state adw_state;
extern const char * const adw_wifi_errors[];

/*
 * Logging interface for platform code.
 */
void adw_log(const char *fmt, ...) ADA_ATTRIB_FORMAT(1, 2);

const char *adw_format_ssid(const struct al_wifi_ssid *ssid,
		char *buf, size_t len);

struct adw_wifi_history *adw_wifi_hist_new(struct adw_state *,
		struct al_wifi_ssid *, struct al_wifi_scan_result *);
void adw_wifi_hist_clr_curr(struct adw_state *);
enum wifi_error adw_wifi_get_error(void);
void adw_wifi_export_cur_prof(void *);
void adw_wifi_export_profiles(void *arg);
const char *adw_wifi_conf_string(enum conf_token);
int adw_wifi_sec_downgrade(enum conf_token new, enum conf_token old);
struct adw_profile *adw_wifi_prof_lookup(struct adw_state *,
				const struct al_wifi_ssid *);
struct adw_profile *adw_wifi_prof_search(struct adw_state *,
				const struct al_wifi_ssid *);
int adw_wifi_wep_key_convert(const u8 *key, size_t key_len,
				struct al_wifi_key *);
int adw_wifi_wpa_password_convert(const u8 *pwd, size_t pwd_len, u8 *key);
u8 *adw_wifi_mac(struct adw_state *);
enum wifi_error adw_wifi_connect(struct al_wifi_ssid *ssid, const char *key,
    size_t key_len, enum conf_token sec_token, u8 hidden);
void adw_wifi_rejoin(struct adw_state *);
void adw_wifi_scan(struct adw_state *);
struct al_wifi_scan_result *adw_wifi_get_scan(u8 index, u32 *token,
    struct al_wifi_scan_result *scan_ret);
void adw_wifi_commit(int from_ui);
struct server_req;
void adw_wifi_setup_callback(struct adw_state *, struct server_req *);
void adw_wifi_stop_ap_sched(int timo);
u8 adw_wifi_bars(int signal);
void adw_wifi_scan_snapshot_reset(void);
struct server_req;
enum wifi_error adw_wifi_add_prof(struct adw_state *,
    const struct al_wifi_ssid *,
    const char *key, size_t key_len, enum conf_token sec_token,
    u8 hidden);
int adw_wifi_del_prof(struct adw_state *wifi, const struct al_wifi_ssid *);
enum ada_err adw_wifi_start_scan4(u32 min_interval, struct al_wifi_ssid *ssid);
int adw_ssids_match(const struct al_wifi_ssid *, const struct al_wifi_ssid *);
void adw_wifi_save_profiles(void);
#if defined(AYLA_ESP32_SUPPORT)
void adw_wifi_persist_conn_bssid(void);
#endif

/*
 * Get current wifi state
 */
enum adw_wifi_conn_state adw_wifi_get_state(void);

/*
 * Get the average rssi
 */
int adw_wifi_avg_rssi(void);

/*
 * Convert between AL security and Ayla tokens.
 */
enum conf_token adw_wifi_sec_import(enum al_wifi_sec);
enum al_wifi_sec adw_wifi_sec_export(enum conf_token sec);
const char *adw_wifi_sec_string(enum al_wifi_sec wmi_sec);

struct al_wifi_scan_result *adw_wifi_scan_lookup_ssid(struct adw_state *,
				const struct al_wifi_ssid *, u32 wmi_sec);

void adw_wifi_event_post(enum adw_wifi_event_id);

extern const struct conf_entry adw_wifi_conf_entry;
extern const struct conf_entry adw_wifi_ip_conf_entry;

extern struct callback adw_wifi_cbmsg_delayed;

#endif /* __AYLA_ADW_WIFI_INT_H__ */

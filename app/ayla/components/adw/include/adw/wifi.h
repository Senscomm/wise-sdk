/*
 * Copyright 2011-2016 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADW_WIFI_H__
#define __AYLA_ADW_WIFI_H__

#include <ayla/wifi_error.h>
#include <al/al_wifi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADW_WIFI_PROF_CT	11	/* number of profiles */
#define ADW_WIFI_PROF_AP	(ADW_WIFI_PROF_CT - 1) /* profile for AP mode */
#define ADW_WIFI_SCAN_CT	20	/* number of scan results kept */

#define WIFI_IOS_APP_LEN	16

enum adw_wifi_powersave_mode {
	ADW_WIFI_PS_OFF = AL_WIFI_PS_OFF,
	ADW_WIFI_PS_ON = AL_WIFI_PS_ON,
	ADW_WIFI_PS_ON_LESS_BEACONS = AL_WIFI_PS_ON_LESS_BEACONS,
};

enum adw_wifi_event_id {
	ADW_EVID_ENABLE,
	ADW_EVID_DISABLE,
	ADW_EVID_ASSOCIATING,
	ADW_EVID_SETUP_START,
	ADW_EVID_SETUP_STOP,
	ADW_EVID_STA_DOWN,
	ADW_EVID_STA_UP,
	ADW_EVID_STA_DHCP_UP,		/* join succeeded */
	ADW_EVID_AP_START,
	ADW_EVID_AP_DOWN,
	ADW_EVID_AP_UP,
	ADW_EVID_RESTART_FAILED,
	ADW_EVID_SCAN_DONE,
	ADW_EVID_STATUS,		/* new status is available */
};

/*
 * Status of most recent join attempt.
 */
struct adw_wifi_status {
	u8	seq;		/* counter for use in detecting stale status */
	u8	final:1;	/* set if status is final */
	u8	ssid[32];	/* SSID */
	u8	ssid_len;	/* length of SSID */
	enum wifi_error error;	/* error, if any */
};

#ifdef AYLA_WIFI_SUPPORT

extern char adw_wifi_ios_app[WIFI_IOS_APP_LEN];

void adw_wifi_init(void);
void adw_wifi_show_rssi(int argc, char **argv);
void adw_wifi_show(void);
int adw_wifi_fw_open(u16 conf_inode, u32 index);
void adw_wifi_powersave(enum adw_wifi_powersave_mode);
int adw_wifi_configured(void);
void adw_wifi_start_scan(u32 min_interval);
int adw_wifi_scan_result_count(void);

void adw_wifi_cli(int argc, char **argv);
extern const char adw_wifi_cli_help[];

int adw_wifi_join_rx(void *buf, int len);
int adw_wifi_delete_rx(void *buf, int len);
int adw_wifi_in_ap_mode(void);
void adw_wifi_stop(void);
void adw_wifi_stayup(void);
void adw_wifi_show_hist(int to_log);

struct server_req;
void adw_wifi_http_ios_get(struct server_req *);

void adw_wifi_event_register(void (*fn)(enum adw_wifi_event_id, void *arg),
				void *arg);
void adw_wifi_event_deregister(void (*fn)(enum adw_wifi_event_id, void *arg));

/*
 * Returns the saved Wi-Fi status from the last pending notification.
 * This is available after an ADW_EVID_STATUS event.
 *
 * Returns -1 if no status pending.
 */
int adw_wifi_status_get(struct adw_wifi_status *status);

/*
 * Wi-Fi synchronization interfaces.
 */
void adw_lock(void);
void adw_unlock(void);

extern u8 adw_locked;			/* for ASSERTs only */

#else /* no AYLA_WIFI_SUPPORT */

static inline void adw_wifi_init(void) {}
static inline void adw_wifi_powersave(enum adw_wifi_powersave_mode mode) {}
static inline int adw_wifi_configured(void) { return 0; }
static inline void adw_wifi_show_hist(int to_log) {}
static inline void adw_wifi_event_register(void (*fn)(enum adw_wifi_event_id,
			void *arg), void *arg) {}
static inline void adw_wifi_event_deregister(void (*fn)(enum adw_wifi_event_id,
			void *arg)) {}

#endif /* AYLA_WIFI_SUPPORT */

/*
 * Return non-zero if the current Wi-Fi connection was setup by MFi.
 */
int adw_wifi_was_setup_by_mfi(void);

/*
 * Server interfaces.
 */
void adw_wifi_page_init(int enable_redirect);

/*
 * Check Wi-Fi enable status and start Wi-Fi if it is enabled.
 */
void adw_check_wifi_enable_conf(void);

/*
 * Search scan results for specified network, and return RSSI if found.
 * The network must have the specified security or better.
 * RSSI is filled in if found.
 * Returns 0 if found, -1 on error.
 */
enum conf_token;
int adw_wifi_scan_find(const u8 *ssid, u8 ssid_len,
			enum conf_token sec, int *rssi);

/*
 * Start a join to the specified network.
 * Creates a profile for the join.
 * Saving of the profile can be disabled using adw_wifi_save_policy_set().
 * Association success can be determined by an event handler.
 */
enum wifi_error;
enum wifi_error adw_wifi_join_net(const u8 *ssid, u8 ssid_len,
			enum conf_token sec, const char *passphrase);

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_ADW_WIFI_H__ */

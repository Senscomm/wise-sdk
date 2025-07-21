/*
 * Copyright 2011-2014 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <ayla/utypes.h>
#include <ayla/endian.h>
#include <ayla/assert.h>
#include <ayla/clock.h>
#include <ayla/log.h>
#include <ayla/conf_token.h>
#include <ayla/conf.h>
#include <ayla/parse.h>
#include <ayla/tlv.h>
#include <ayla/wifi_status.h>
#include <ayla/callback.h>
#include <ada/err.h>
#include <ayla/mod_log.h>

#include <al/al_net_addr.h>
#include <al/al_net_dns.h>
#include <al/al_net_if.h>
#include <al/al_net_mdnss.h>
#include <al/al_os_lock.h>
#include <al/al_os_mem.h>

#include <adw/wifi.h>
#include <adw/wifi_conf.h>
#include <ayla/timer.h>
#include "wifi_int.h"
#include <ada/client.h>
#include <ada/metrics.h>
#include <ada/dnss.h>
#include <ada/ada_wifi.h>
#include <ada/ada_timer.h>

#define REJOIN_TIMES_MAX 1

static int rejoin_times;
static char adw_wifi_hostname[33];
const char * const adw_wifi_errors[] = WIFI_ERRORS;

struct adw_state adw_state;
ADW_SIZE_INIT				/* for ADW_SIZE_DEBUG */

static struct callback adw_wifi_cbmsg_join;
static struct callback adw_wifi_cbmsg_step;
struct callback adw_wifi_cbmsg_delayed;

static int adw_wifi_start(void);
static void adw_wifi_step_cb(void *);
static void adw_wifi_stop_ap(void *arg);
static void adw_wifi_scan2prof(struct adw_state *wifi, u8 scan_prof);
static int adw_wifi_join_profile(struct adw_state *, struct adw_profile *);
static int adw_wifi_configured_nolock(void);

/*
 * Check station interface for DHCP status.
 * Returns 0 if DHCP address configured.
 */
static int adw_wifi_dhcpc_poll(struct adw_state *, struct adw_wifi_history *);

/*
 * Check AP interface for DHCP status.
 * Returns 0 if DHCP address configured.
 */
static int adw_wifi_dhcps_poll(struct adw_state *, u32);

struct adw_wifi_event_handler {
	void (*handler)(enum adw_wifi_event_id, void *);
	void *arg;
};

static struct adw_wifi_event_handler
    adw_wifi_event_handlers[WIFI_EVENT_HANDLER_CT];

struct adw_wifi_event {
	struct adw_wifi_event *next;
	enum adw_wifi_event_id id;
};
static struct adw_wifi_event *adw_wifi_event_head;
static struct adw_wifi_event *adw_wifi_event_tail;
static struct callback adw_wifi_cbmsg_event;

static struct ada_timer *adw_wifi_rssi_timer;
static struct ada_timer *adw_wifi_scan_timer;
static struct ada_timer *adw_wifi_step_timer;
static struct ada_timer *adw_wifi_join_timer;
static struct ada_timer *adw_wifi_client_timer;
static struct ada_timer *adw_wifi_ap_mode_timer;
static struct al_lock *adw_wifi_lock;
u8 adw_locked;

void adw_lock(void)
{
	al_os_lock_lock(adw_wifi_lock);
	ASSERT(!adw_locked);
	adw_locked = 1;
}

void adw_unlock(void)
{
	ASSERT(adw_locked);
	adw_locked = 0;
	al_os_lock_unlock(adw_wifi_lock);
}

/*
 * Register for event callback.
 * Callback is in the Wi-Fi thread.  Callee must handle any softcalls needed.
 */
void adw_wifi_event_register(void (*handler)(enum adw_wifi_event_id, void *),
				void *arg)
{
	struct adw_wifi_event_handler *hp;

	adw_lock();
	for (hp = adw_wifi_event_handlers;
	    hp < &adw_wifi_event_handlers[WIFI_EVENT_HANDLER_CT]; hp++) {
		if (!hp->handler) {
			hp->handler = handler;
			hp->arg = arg;
			adw_unlock();
			return;
		}
	}
	adw_unlock();
	adw_log(LOG_ERR "%s: limit reached", __func__);
}

/*
 * Deregister for event callback.
 */
void adw_wifi_event_deregister(void (*handler)(enum adw_wifi_event_id, void *))
{
	struct adw_wifi_event_handler *hp;

	adw_lock();
	for (hp = adw_wifi_event_handlers;
	    hp < &adw_wifi_event_handlers[WIFI_EVENT_HANDLER_CT]; hp++) {
		if (hp->handler == handler) {
			hp->handler = NULL;
			break;
		}
	}
	adw_unlock();
}

/*
 * Handle all queued events.
 * The adw_lock is dropped during the event handler.
 */
static void adw_wifi_event_cb(void *wifi_arg)
{
	struct adw_wifi_event_handler *hp;
	struct adw_wifi_event *ev;
	enum adw_wifi_event_id id;
	void (*handler)(enum adw_wifi_event_id, void *);
	void *arg;

	adw_lock();
	while ((ev = adw_wifi_event_head) != NULL) {
		adw_wifi_event_head = ev->next;
		id = ev->id;

		for (hp = adw_wifi_event_handlers;
		    hp < &adw_wifi_event_handlers[WIFI_EVENT_HANDLER_CT];
		    hp++) {
			handler = hp->handler;
			arg = hp->arg;
			adw_unlock();
			if (ev) {
				al_os_mem_free(ev);
				ev = NULL;
			}

			if (handler) {
				handler(id, arg);
			}

			adw_lock();
		}
	}
	adw_wifi_event_head = NULL;
	adw_wifi_event_tail = NULL;
	adw_unlock();
}

/*
 * Post an event callback.
 */
void adw_wifi_event_post(enum adw_wifi_event_id id)
{
	struct adw_wifi_event *ev;

	ev = al_os_mem_calloc(sizeof(*ev));
	if (!ev) {
		adw_log(LOG_ERR "malloc failed for event");
		return;
	}
	ev->id = id;
	if (!adw_wifi_event_head) {
		adw_wifi_event_head = ev;
	}
	if (adw_wifi_event_tail) {
		adw_wifi_event_tail->next = ev;
	}
	adw_wifi_event_tail = ev;
	ada_callback_pend(&adw_wifi_cbmsg_event);
}

void adw_log(const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_put_va(MOD_LOG_WIFI, fmt, args);
	ADA_VA_END(args);
}

static inline int adw_wifi_ether_addr_non_zero(u8 *mac)
{
	return (mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) != 0;
}

static int adw_addr_conflict_check(u32 addr1, u32 mask1, u32 addr2, u32 mask2)
{
	u32 diff = addr1 ^ addr2;

	return !(diff & mask1) || !(diff & mask2);
}

/*
 * Save status and generate a status event.
 */
static void adw_wifi_status_notify(struct adw_state *wifi,
		struct al_wifi_ssid *ssid, enum wifi_error error, u8 final)
{
	struct adw_wifi_status *status = &wifi->status;

	status->final = final;
	memcpy(status->ssid, ssid->id, ssid->len);
	status->ssid_len = ssid->len;
	status->error = error;
	status->seq++;
	if (!status->seq) {
		status->seq++;	/* skip 0 */
	}
	adw_wifi_event_post(ADW_EVID_STATUS);
}

int adw_wifi_status_get(struct adw_wifi_status *statusp)
{
	struct adw_state *wifi = &adw_state;
	struct adw_wifi_status *status = &wifi->status;

	if (!status->seq) {
		return -1;
	}
	adw_lock();
	*statusp = *status;	/* structure copy */
	adw_unlock();
	return 0;
}

/*
 * Get new connection history entry.
 * Called with lock held.
 */
struct adw_wifi_history *adw_wifi_hist_new(struct adw_state *wifi,
    struct al_wifi_ssid *ssid, struct al_wifi_scan_result *scan)
{
	struct adw_wifi_history *hist;

	if (++wifi->hist_curr >= WIFI_HIST_CT) {
		wifi->hist_curr = 0;
	}
	hist = &wifi->hist[wifi->hist_curr];
	memset(hist, 0, sizeof(*hist));
	hist->time = al_clock_get_total_ms();
	hist->ssid_info[0] = ssid->id[0];
	hist->ssid_info[1] = ssid->len < 2 ? '\0' : ssid->id[ssid->len - 1];
	hist->ssid_len = ssid->len;
	if (scan) {
		memcpy(hist->bssid, scan->bssid, sizeof(hist->bssid));
	}
	if (wifi->pref_profile) {
		adw_wifi_hist_clr_curr(wifi);
		hist->curr = 1;
	}
	return hist;
}

void adw_wifi_hist_clr_curr(struct adw_state *wifi)
{
	struct adw_wifi_history *old;

	for (old = wifi->hist; old < &wifi->hist[WIFI_HIST_CT]; old++) {
		old->curr = 0;
	}
}

enum wifi_error adw_wifi_get_error(void)
{
	struct adw_state *wifi = &adw_state;
	struct adw_wifi_history *hist;

	hist = &wifi->hist[wifi->hist_curr];

	return hist->error;
}

/*
 * Convert received signal strength to bar-graph intensity.
 * Any signal should give at least one bar.  Max signal is 5 bars.
 * Lean towards giving 5 bars for a wide range of usable signals.
 */
u8 adw_wifi_bars(int signal)
{
	if (signal == WIFI_MIN_SIG) {
		return 0;
	}
	if (signal < -70) {
		return 1;
	}
	if (signal < -60) {
		return 2;
	}
	if (signal < -50) {
		return 3;
	}
	if (signal < -40) {
		return 4;
	}
	return 5;
}

/*
 * Return non-hyphenated security token string.
 */
const char *adw_wifi_conf_string(enum conf_token token)
{
	if (token == CT_WPA2_Personal) {
		return "WPA2 Personal";
	} else if (token == CT_WPA3_Personal) {
		return "WPA3 Personal";
	}
	return conf_string(token);
}

static int adw_profile_cnt(struct adw_state *wifi)
{
	struct adw_profile *prof;
	int enabled_entries = 0;

	for (prof = wifi->profile;
	     prof < &wifi->profile[ADW_WIFI_PROF_AP]; prof++) {
		if (prof->ssid.len && prof->enable) {
			enabled_entries++;
		}
	}
	return enabled_entries;
}

static void adw_wifi_step_cb_pend(struct adw_state *wifi)
{
	ada_callback_pend(&adw_wifi_cbmsg_step);
}

static void adw_wifi_step_timeout(void *arg)
{
	adw_wifi_step_cb_pend(&adw_state);
}

static void adw_wifi_step_arm_timer(void *wifi_arg)
{
	adw_lock();
	ada_timer_set(adw_wifi_step_timer, WIFI_CMD_RSP_TMO);
	adw_unlock();
}

static void adw_wifi_commit_locked(struct adw_state *wifi)
{
	adw_wifi_scan2prof(wifi, 0);
	if (wifi->state == WS_UP && !wifi->profile[wifi->curr_profile].enable) {
		adw_wifi_rejoin(wifi);
	} else {
		adw_wifi_step_cb_pend(wifi);
	}
}

void adw_wifi_commit(int from_ui)
{
	struct adw_state *wifi = &adw_state;

	adw_lock();
	adw_wifi_commit_locked(wifi);
	adw_unlock();
}

static void adw_wifi_enable_set(int enable)
{
	struct adw_state *wifi = &adw_state;

	adw_lock();
	wifi->enable = enable;
	adw_wifi_commit_locked(wifi);
	adw_unlock();
}

void adw_wifi_enable(void)
{
	adw_wifi_enable_set(1);
}

void adw_wifi_disable(void)
{
	adw_wifi_enable_set(0);
}

int adw_wifi_is_enabled(void)
{
	struct adw_state *wifi = &adw_state;

	return wifi->enable;
}

void adw_check_wifi_enable_conf(void)
{
	struct adw_state *wifi = &adw_state;
	if (wifi->enable) {
		adw_wifi_commit(0);
	}
}

void adw_wifi_force_ap_mode(void)
{
	struct adw_state *wifi = &adw_state;

	adw_lock();
	wifi->pref_profile = ADW_WIFI_PROF_AP + 1;
	wifi->profile[ADW_WIFI_PROF_AP].join_errs = 0;
	adw_wifi_rejoin(wifi);
	adw_unlock();
}

void adw_wifi_unforce_ap_mode(void)
{
	struct adw_state *wifi = &adw_state;

	adw_lock();
	if (wifi->pref_profile == ADW_WIFI_PROF_AP + 1) {
		wifi->pref_profile = 0;
	}
	adw_wifi_rejoin(wifi);
	adw_unlock();
}

static void adw_wifi_clear_pref_profile(struct adw_state *wifi)
{
	if (wifi->pref_profile - 1 != ADW_WIFI_PROF_AP) {
		/*
		 * If AP profile was selected specifically, keep using
		 * until something else is selected, or system restarts.
		 */
		wifi->pref_profile = 0;
	}
}

void adw_wifi_save_profiles(void)
{
	struct adw_state *wifi = &adw_state;
	struct adw_profile *prof;

	adw_lock();
	if (wifi->curr_profile + 1 == wifi->pref_profile) {
		prof = &wifi->profile[wifi->curr_profile];
		prof->enable = 1;
		adw_wifi_clear_pref_profile(wifi);
	}
	adw_unlock();
	conf_persist(CT_wifi, adw_wifi_export_profiles, wifi);
}

/*
 * Log failed history entry.
 */
static void adw_wifi_hist_log(struct adw_state *wifi,
				struct adw_wifi_history *hist)
{
	struct adw_profile *prof;
	char ssid_buf[33];

	if (hist->error <= WIFI_ERR_NONE
	    || hist->error > WIFI_ERR_IN_PROGRESS) {
		return;
	}
	if (!wifi->pref_profile && hist->error == WIFI_ERR_NOT_FOUND) {
		return;
	}
	prof = &wifi->profile[wifi->curr_profile];
	snprintf(wifi->err_buf, sizeof(wifi->err_buf),
	    "Wi-Fi connect to %s: %s",
	    adw_format_ssid(&prof->ssid, ssid_buf, sizeof(ssid_buf)),
	    adw_wifi_errors[hist->error]);
	wifi->err_msg = wifi->err_buf;
	adw_log(LOG_WARN "%s", wifi->err_buf);
}

/*
 * Join attempt failed. Are we giving up on this profile?
 * We only record this for preferred profiles, i.e. tentative profiles.
 */
static int adw_wifi_current_profile_done(struct adw_state *wifi,
					struct adw_wifi_history *hist)
{
	struct adw_profile *prof;

	adw_wifi_hist_log(wifi, hist);
	if (wifi->curr_profile != wifi->pref_profile - 1) {
		return 0;
	}
	prof = &wifi->profile[wifi->curr_profile];
	if (prof->join_errs >= WIFI_PREF_TRY_LIMIT) {
		wifi->pref_profile = 0;
		adw_wifi_event_post(ADW_EVID_STA_DOWN);
		adw_wifi_status_notify(wifi, &prof->ssid, hist->error, 1);
		hist->last = 1;
		return 1;
	}
	adw_wifi_status_notify(wifi, &prof->ssid, hist->error, 0);
	return 0;
}

static void adw_wifi_service_fail(struct adw_state *wifi)
{
	struct adw_wifi_history *hist;
	struct adw_profile *prof;

	if (wifi->state == WS_DHCP || wifi->state == WS_WAIT_CLIENT) {
		hist = &wifi->hist[wifi->hist_curr];
		if (wifi->state == WS_DHCP) {
			/*
			 * TBD: Can't distinguish between no IP and no gateway.
			 */
			hist->error = WIFI_ERR_NO_IP;
		} else {
			hist->error = client_status();
		}
		switch (hist->error) {
		case WIFI_ERR_CLIENT_AUTH:
			/*
			 * Don't rejoin on authentication error.
			 */
			if (wifi->curr_profile == wifi->pref_profile - 1) {
				hist->last = 1;
				adw_wifi_hist_log(wifi, hist);
				prof = &wifi->profile[wifi->curr_profile];
				adw_wifi_status_notify(wifi, &prof->ssid,
				    hist->error, 1);
			}
			break;
		default:
			prof = &wifi->profile[wifi->curr_profile];
			if ((wifi->state == WS_WAIT_CLIENT && prof->enable &&
				client_lanmode_is_enabled())) {
				/*
				 * Don't rejoin if this is a LAN-enabled device
				 * and the profile had successfully reached the
				 * service at some point. This is to allow the
				 * device to have LAN registrations.
				 */
				break;
			}
			prof->join_errs++;
			adw_wifi_current_profile_done(wifi, hist);
			adw_log(LOG_WARN "timeout: timed out waiting for %s",
			    wifi->state == WS_DHCP ? "DHCP" : "ADS");
			adw_wifi_rejoin(wifi);
		}
		METRIC_INCR(M_WIFI_CONNECT_FAILURES, 0, 0);
		METRIC_INCR(M_WIFI_CONNECT_ERRORS, 0, hist->error);
	}
}

/*
 * Timeout waiting for DHCP or client connection to the device service.
 */
static void adw_wifi_client_timeout(void *arg)
{
	struct adw_state *wifi = &adw_state;

	adw_lock();
	adw_wifi_service_fail(wifi);
	adw_unlock();
}

/*
 * Callback when client connection status changes.
 * May be called with the wifi lock held.
 */
static void adw_wifi_client_event(void *arg, enum ada_err err)
{
	struct adw_state *wifi = arg;

	adw_log(LOG_DEBUG "%s: %d", __func__, err);
	wifi->client_err = err;
	adw_wifi_step_cb_pend(wifi);
}

/*
 * Return non-zero if a new security setting is a downgrade.
 */
int adw_wifi_sec_downgrade(enum conf_token new, enum conf_token old)
{
	/* these asserts are evaluated at compile time. */
	ASSERT(CT_none < CT_WEP);
	ASSERT(CT_WEP < CT_WPA);
	ASSERT(CT_WPA < CT_WPA2_Personal);
	ASSERT(CT_WPA2_Personal < CT_WPA3_Personal);

	return new < old;
}

int adw_ssids_match(const struct al_wifi_ssid *a, const struct al_wifi_ssid *b)
{
	return a->len == b->len && !memcmp(a->id, b->id, a->len);
}

/*
 * Lookup profile by SSID.
 * Called with lock held.
 */
struct adw_profile *
adw_wifi_prof_lookup(struct adw_state *wifi, const struct al_wifi_ssid *ssid)
{
	struct adw_profile *prof;

	for (prof = wifi->profile;
	    prof < &wifi->profile[ADW_WIFI_PROF_AP]; prof++) {
		if (adw_ssids_match(&prof->ssid, ssid)) {
			return prof;
		}
	}
	return NULL;
}

struct adw_profile *
adw_wifi_prof_search(struct adw_state *wifi, const struct al_wifi_ssid *ssid)
{
	struct adw_profile *prof;

	prof = adw_wifi_prof_lookup(wifi, ssid);
	if (prof) {
		return prof;
	}
	for (prof = wifi->profile;
	     prof < &wifi->profile[ADW_WIFI_PROF_AP]; prof++) {
		if (prof->ssid.len == 0) {
			return prof;
		}
	}
	return NULL;
}

void adw_wifi_rejoin(struct adw_state *wifi)
{
	wifi->rejoin = 1;
	adw_wifi_step_cb_pend(wifi);
}

static void adw_wifi_rescan(void *arg)
{
	struct adw_state *wifi = &adw_state;

	adw_lock();
	if (wifi->scan_state == SS_SCAN_WAIT) {
		adw_wifi_scan(wifi);
	} else if (wifi->scan_state == SS_SCAN) {
		if (wifi->enable) {
			adw_log(LOG_WARN "scan timeout");
		}
		wifi->scan_state = SS_DONE;
		adw_wifi_scan2prof(wifi, 0);
		adw_wifi_rejoin(wifi);
	}
	adw_unlock();
}

/*
 * Update profile data items to matching scan entries.
 */
static void adw_wifi_scan2prof(struct adw_state *wifi, u8 scan_prof)
{
	struct adw_profile *prof;
	struct al_wifi_scan_result *scan;
	enum conf_token sec;

	if (!scan_prof) {
		for (prof = wifi->profile;
		    prof < &wifi->profile[ADW_WIFI_PROF_AP]; prof++) {
			prof->scan = NULL;
		}
	}
	for (scan = &wifi->scan[ADW_WIFI_SCAN_CT - 1];
	    scan >= wifi->scan; scan--) {
		if (scan->ssid.len == 0) {
			continue;
		}
		prof = adw_wifi_prof_lookup(wifi, &scan->ssid);
		if (!prof) {
			continue;
		}
		sec = adw_wifi_sec_import(scan->wmi_sec);
		if ((sec == CT_none || prof->phrase.len) &&
		    !adw_wifi_sec_downgrade(sec, prof->sec)) {
			prof->scan = scan;
			if (!scan_prof && !prof->spec_scan_done) {
				prof->hidden = 0;
			}
		}
	}
}

static void adw_wifi_scan_callback(struct al_wifi_scan_result *rp)
{
	struct adw_state *wifi = &adw_state;
	struct al_wifi_scan_result *best;
	struct al_wifi_scan_result *scan;
	int i;

	adw_lock();
	if (rp == NULL) {
		wifi->scan_state = SS_DONE;
		adw_wifi_step_cb_pend(wifi);
		ada_timer_cancel(adw_wifi_scan_timer);
		goto out;
	}

	/*
	 * Toss scan results that have empty or all-zeroes SSID.
	 */
	for (i = 0; i < rp->ssid.len; i++) {
		if (rp->ssid.id[i]) {
			break;
		}
	}
	if (i >= rp->ssid.len) {
		goto out;
	}

	if (adw_ssids_match(&rp->ssid, &wifi->scan4)) {
		/*
		 * If doing scan for specific target, make sure there's spot
		 * for it.
		 */
		best = &wifi->scan[0];
	} else {
		best = NULL;
	}

	/*
	 * Replace an entry in the scan list that is empty,
	 * has the same SSID but a weaker signal,
	 * or has a weakest signal that's also weaker than the new result.
	 * But drop the scan result if it is a known SSID but not stronger.
	 * If hearing from multiple base stations with different security or
	 * band, keep both entries.
	 */
	for (scan = wifi->scan; scan < &wifi->scan[ADW_WIFI_SCAN_CT]; scan++) {
		if (scan->ssid.len == 0) {
			if (!best) {
				best = scan;
			}
			break;
		}
		if (!best && rp->rssi > scan->rssi) {
			best = scan;
		}
		if (adw_ssids_match(&rp->ssid, &scan->ssid) &&
		    rp->wmi_sec == scan->wmi_sec) {
			break;
		}
	}
	if (best) {
		/*
		 * Move from scan to best.
		 */
		if (best != scan) {
			if (scan == &wifi->scan[ADW_WIFI_SCAN_CT]) {
				/*
				 * Last item will fall off.
				 */
				scan--;
			}
			memmove(best + 1, best, (scan - best) * sizeof(*scan));
		}
		*best = *rp;
	}

out:
	adw_unlock();
}

/*
 * Select next candidate for joining.
 * Choose the AP with the strongest signal first.
 *
 * Called with lock held.
 */
static struct adw_profile *adw_wifi_select(struct adw_state *wifi)
{
	struct adw_profile *prof;
	struct adw_profile *best = NULL;
#ifdef WIFI_DEBUG
	char ssid_buf[33];
#endif

	if (wifi->pref_profile) {
		prof = &wifi->profile[wifi->pref_profile - 1];
		if (prof->join_errs < WIFI_PREF_TRY_LIMIT) {
			return prof;
		}
		wifi->pref_profile = 0;
	}
	for (prof = wifi->profile; prof < &wifi->profile[ADW_WIFI_PROF_AP];
	    prof++) {
		if (prof->ssid.len == 0 || prof->enable == 0) {
			continue;
		}
#ifdef WIFI_DEBUG
		adw_log(LOG_DEBUG "select: consider prof %u "
		    "ssid %s sec %s signal %d errs %u",
		    prof - wifi->profile,
		    adw_format_ssid(&prof->ssid, ssid_buf, sizeof(ssid_buf)),
		    conf_string(prof->sec), prof->signal,
		    prof->join_errs);
#endif /* WIFI_DEBUG */

		if (prof->join_errs >= WIFI_JOIN_TRY_LIMIT) {
			continue;
		}
		if (!prof->scan) {
			continue;
		}
		if (!best || !best->scan || (prof->scan &&
		    prof->scan->rssi > best->scan->rssi)) {
			best = prof;
		}
	}
	return best;
}

/*
 * We will attempt to heal ourselves by resetting wifi chip if
 * a) there are configured profiles AND
 * b) we've tried all of them 3 times AND
 * c) there has not been activity with our local server recently
 *
 * We will attempt to heal ourselves by resetting whole module if
 * a) we've attempted wifi reset 3 times and that didn't work AND
 * b) there has not been activity with our local server or console recently
 */
static int adw_wifi_health_check(struct adw_state *wifi)
{
	if (conf_setup_mode || conf_mfg_mode) {
		return 0;
	}
	if (++wifi->fail_cnt < WIFI_MAX_FAILS) {
		return 0;
	}
	wifi->fail_cnt = 0;
	return 1;
}

void adw_wifi_stayup(void)
{
	struct adw_state *wifi = &adw_state;

	wifi->fail_cnt = 0;
	wifi->reset_cnt = 0;
	wifi->use_time = al_clock_get_total_ms();
}

void adap_wifi_stayup(void)
{
	adw_wifi_stayup();
}

/*
 * Initialize RSSI samples.
 * Call right after join.
 * Set all samples the same to start with.
 * Called with lock held.
 */
static void adw_wifi_init_rssi(struct adw_state *wifi)
{
	int i;
	s16 rssi;

	if (al_wifi_get_rssi(&rssi)) {
		return;
	}
	for (i = 0; i < WIFI_RSSI_CT; ++i) {
		wifi->rssi_reading[i] = rssi;
	}
	wifi->rssi_total = rssi * WIFI_RSSI_CT;
	wifi->rssi_index = 1;
}

/*
 * Add a periodic RSSI sample to the accumulated samples and total.
 * Called with lock held.
 */
static void adw_wifi_sample_rssi(struct adw_state *wifi)
{
	s16 *entry;
	s16 old;
	s16 rssi;

	if (al_wifi_get_rssi(&rssi)) {
		return;
	}

	METRIC_SAMPLE(M_WIFI_RSSI, 0, rssi);
	entry = &wifi->rssi_reading[wifi->rssi_index];
	wifi->rssi_index = (wifi->rssi_index + 1) % WIFI_RSSI_CT;
	old = *entry;
	wifi->rssi_total += rssi - old;
	*entry = rssi;
#ifdef WIFI_DEBUG
	adw_log(LOG_DEBUG "sample_rssi: RSSI %d avg %d",
	    rssi, wifi->rssi_total / WIFI_RSSI_CT);
#endif /* WIFI_DEBUG */
}

int adw_wifi_avg_rssi(void)
{
	struct adw_state *wifi = &adw_state;
	int total = 0;
	int count = 0;
	int i;

	if (wifi->state >= WS_DHCP && wifi->state <= WS_UP) {
		adw_wifi_sample_rssi(wifi);
	}
	for (i = 0; i < WIFI_RSSI_CT; i++) {
		if (wifi->rssi_reading[i] &&
		    wifi->rssi_reading[i] != WIFI_MIN_SIG) {
			total += wifi->rssi_reading[i];
			count++;
		}
	}
	if (count) {
		return total / count;
	}
	return WIFI_MIN_SIG;
}

static void adw_wifi_rssi_timeout(void *arg)
{
	struct adw_state *wifi = &adw_state;
	u8 tx_power;

	adw_lock();
	adw_wifi_sample_rssi(wifi);
	if (wifi->rssi_index) {
		ada_timer_set(adw_wifi_rssi_timer, WIFI_RSSI_MIN_DELAY);
	} else {
		adw_log(LOG_INFO "RSSI average %d",
		    wifi->rssi_total / WIFI_RSSI_CT);
		if (!al_wifi_tx_power_get(&tx_power)) {
			adw_log(LOG_INFO "tx_power %d", tx_power);
		}
	}
	adw_unlock();
}

/*
 * Show the average RSSI.
 */
void adw_wifi_show_rssi(int argc, char **argv)
{
	struct adw_state *wifi = &adw_state;

	adw_lock();
	if (wifi->started) {
		adw_wifi_init_rssi(wifi);
		ada_timer_set(adw_wifi_rssi_timer, WIFI_RSSI_MIN_DELAY);
	}
	adw_unlock();
}

void adw_wifi_hostname_set(const char *name)
{
	snprintf(adw_wifi_hostname, sizeof(adw_wifi_hostname), "%s", name);
}

/*
 * Check for Wi-Fi Join completion.
 */
static void adw_wifi_check_join(void *arg)
{
	struct adw_state *wifi = arg;
	struct adw_wifi_history *hist;
	struct adw_profile *prof = NULL;
	enum wifi_error err;
	u8 retry_immed = 0;

	adw_lock();
	ada_timer_cancel(adw_wifi_join_timer);
	hist = &wifi->hist[wifi->hist_curr];

	err = al_wifi_join_status_get();
	switch (wifi->state) {
	case WS_JOIN:
		switch (err) {
		case WIFI_ERR_WRONG_KEY:
			/*
			 * The first time a "wrong key" error occurs,
			 * it might really be a 4-way-handshake
			 * timeout, so retry the join right away
			 * without a new scan.
			 */
			prof = &wifi->profile[wifi->curr_profile];
			if (!prof->join_errs) {
				if (rejoin_times) {
					retry_immed = 1;
					rejoin_times--;
				}
			}
			goto fail;
		case WIFI_ERR_NOT_AUTH:
			goto wait;
		case WIFI_ERR_NOT_FOUND:
		case WIFI_ERR_INV_KEY:
			goto fail;
		default:
			err = WIFI_ERR_TIME;

			/*
			 * Workaround for early status.
			 * If it's been less than X seconds, continue waiting,
			 * connect may yet complete.
			 */
wait:
			if (al_clock_get_total_ms() -
			    wifi->hist[wifi->hist_curr].time >=
			    WIFI_JOIN_TIMEOUT) {
fail:
				hist->error = err;
				prof = &wifi->profile[wifi->curr_profile];
				if (err == WIFI_ERR_WRONG_KEY) {
					prof->join_errs += WIFI_JOIN_KEY_ERR;
				} else {
					prof->join_errs++;
				}
				METRIC_INCR(M_WIFI_CONNECT_FAILURES, 0, 0);
				METRIC_INCR(M_WIFI_CONNECT_ERRORS, 0, err);
				adw_wifi_current_profile_done(wifi, hist);

				al_wifi_leave();
				wifi->state = WS_IDLE;

				/*
				 * Profile count is zero if we're trying to
				 * connect to "preferred profile". This means
				 * wifi setup is going on.
				 */
				if (!retry_immed && adw_profile_cnt(wifi)) {
					adw_log(LOG_DEBUG
					   "join failed %d will rescan", err);
					adw_wifi_scan(wifi);
				} else if (retry_immed) {
					adw_log(LOG_DEBUG
					   "join failed %d will retry", err);
					adw_wifi_join_profile(wifi, prof);
				} else {
					wifi->state = WS_SCAN_DONE;
				}
				break;
			}
#ifdef WIFI_DEBUG
			adw_log(LOG_DEBUG "join status %d - "
			    "continue waiting", err);
#endif /* WIFI_DEBUG */
			ada_timer_set(adw_wifi_join_timer, WIFI_JOIN_POLL);
			break;
		case WIFI_ERR_NONE:
			adw_log(LOG_INFO "join succeeded");
			wifi->state = WS_DHCP;
			ada_timer_set(adw_wifi_client_timer,
			    WIFI_DHCP_WAIT);
			ada_timer_set(adw_wifi_join_timer, 1000);
			adw_wifi_event_post(ADW_EVID_STA_UP);
			break;
		}
		adw_wifi_step_cb_pend(wifi);
		break;
	case WS_DHCP:
		adw_wifi_step_cb_pend(wifi);
		/* fallthrough */
	case WS_WAIT_CLIENT:
		if (err != WIFI_ERR_NONE) {
			prof = &wifi->profile[wifi->curr_profile];
			prof->join_errs++;
			METRIC_INCR(M_WIFI_CONNECT_FAILURES, 0, 0);
			METRIC_INCR(M_WIFI_CONNECT_ERRORS, 0, err);
		}
		/* fallthrough */
	case WS_UP:
		/* fallthrough */
	case WS_UP_AP:
		if (err == WIFI_ERR_NONE) {
			adw_wifi_sample_rssi(wifi);
		}
		if (err != WIFI_ERR_NONE) {
			adw_log(LOG_WARN
			    "check_join: wifi down. status %d", err);
			if (err == WIFI_ERR_IN_PROGRESS) {
				break;		/* likely shutting down */
			}
			wifi->rejoin = 1;
			/* TBD: can't tell between WIFI_ERR_LOS and AP_DISC */
			hist->error = WIFI_ERR_LOS;
			if (prof) {
				adw_wifi_current_profile_done(wifi, hist);
			}
			adw_wifi_step_cb_pend(wifi);
		} else {
			ada_timer_set(adw_wifi_join_timer, WIFI_POLL);
		}
		break;
	case WS_DISABLED:
	case WS_IDLE:
	case WS_RESTART:
	case WS_SCAN_DONE:
	case WS_START_AP:
	case WS_ERROR:
		adw_log(LOG_WARN "check_join: unexpected state %x",
		    wifi->state);
		break;
	}
	adw_unlock();
}

static void adw_wifi_join_timeout(void *arg)
{
	adw_wifi_check_join(&adw_state);
}

/*
 * Start AP mode if enabled.
 * Called with lock held, but drops it while starting AP.
 */
static void adw_wifi_start_ap(struct adw_state *wifi)
{
	struct adw_profile *prof = &wifi->profile[ADW_WIFI_PROF_AP];
	int rc;
	char ssid_buf[33];

	if (prof->enable == 0 || prof->ssid.len == 0 || wifi->ap_unsupported) {
		wifi->state = WS_IDLE;
		return;
	}
	ada_timer_cancel(adw_wifi_ap_mode_timer);

	adw_wifi_event_post(ADW_EVID_AP_START);

	if (wifi->ap_up) {
		wifi->state = WS_UP_AP;
		adw_wifi_clear_pref_profile(wifi);
		adw_wifi_event_post(ADW_EVID_AP_UP);
		return;
	}

	adw_log(LOG_INFO "Setting AP mode SSID %s",
	    adw_format_ssid(&prof->ssid, ssid_buf, sizeof(ssid_buf)));

	/*
	 * Drop lock during al_wifi_start_ap().
	 * Re-evaluate state after reacquiring lock.
	 */
	adw_unlock();
	rc = al_wifi_start_ap(&prof->ssid, ADW_WIFI_AP_IP,
	    ADW_WIFI_AP_NETMASK);
	adw_lock();

	if (wifi->state != WS_START_AP) {
		return;		/* state changed while waiting for lock above */
	}
	if (rc) {
		wifi->state = WS_RESTART;	/* Starting AP mode failed */
		return;
	}

	wifi->state = WS_UP_AP;
	wifi->ap_up = 1;
	adw_wifi_clear_pref_profile(wifi);

	adw_wifi_event_post(ADW_EVID_AP_UP);
}

/*
 * Stop AP mode.
 * Called without holding adw_lock since al_wifi may call
 * adw_wifi_scan_callback(), which needs the lock.
 */
static void adw_wifi_stop_ap(void *arg)
{
	struct adw_state *wifi = (struct adw_state *)arg;

	if (!wifi->ap_up) {
		return;
	}
	adw_log(LOG_INFO "stopping AP mode");
	adw_wifi_event_post(ADW_EVID_AP_DOWN);

	al_wifi_stop_ap();
	wifi->ap_up = 0;
}

static void adw_wifi_ap_mode_timeout(void *arg)
{
	adw_wifi_stop_ap(&adw_state);
}

void adw_wifi_ap_mode_not_supported(void)
{
	struct adw_state *wifi = &adw_state;

	wifi->ap_unsupported = 1;
}

/*
 * Timer for stopping AP mode operation.
 */
void adw_wifi_stop_ap_sched(int timo)
{
	ada_timer_set(adw_wifi_ap_mode_timer, timo);
}

/*
 * Convert key string WEP key format.
 * Returns -1 for invalid key.
 */
int adw_wifi_wep_key_convert(const u8 *key, size_t key_len,
			struct al_wifi_key *wep)
{
	size_t len;

	/*
	 * WEP keys are 64 bits (10 hex digits - 40-bit secret)
	 * or 128 bits (26 hex digits, 104-bit secret).
	 * Wiced also supports 16-byte AES-CCM and
	 * 32-byte ALGO_TKIP keys.
	 */
	if (key_len == 13 || key_len == 5) {
		memcpy(wep->key, key, key_len);
		len = key_len;
	} else {
		len = parse_hex(wep->key, sizeof(wep->key),
		    (const char *)key, key_len);
	}
	if (len != 5 && len != 13 && len != 16 && len != 32) {
		return -1;
	}
	wep->len = len;
	return 0;
}

/*
 * Convert WPA key from printable string format to binary.
 * Returns -1 for failure.
 */
int adw_wifi_wpa_password_convert(const u8 *pwd, size_t pwd_len, u8 *key)
{
	if (pwd_len == 2 * WIFI_WPA_KEY_LEN &&
	    (parse_hex(key, WIFI_WPA_KEY_LEN, (const char *)pwd, pwd_len) ==
		WIFI_WPA_KEY_LEN)) {
		return 0;
	}
	return -1;
}

static void adw_wifi_scan_report(struct adw_state *wifi)
{
	struct al_wifi_scan_result *scan;
	int results = 0;

	for (scan = wifi->scan; scan < &wifi->scan[ADW_WIFI_SCAN_CT]; scan++) {
		if (scan->rssi != WIFI_MIN_SIG) {
			results++;
		}
	}
	adw_log(LOG_DEBUG "scan done. %d networks found", results);
}

/*
 * Start join process to associate with the given profile. Returns 0
 * if process is underway.
 */
static int adw_wifi_join_profile(struct adw_state *wifi,
				struct adw_profile *prof)
{
	struct adw_wifi_history *hist;
	struct al_wifi_scan_result *scan;
	enum wifi_error wifi_error;
	int rc;
	char ssid_buf[33];

	wifi->curr_profile = prof - wifi->profile;

	METRIC_INCR(M_WIFI_CONNECT_ATTEMPTS, 0, 0);
	METRIC_OP_BEGIN(M_WIFI_CONNECT_LATENCY, 0);
	/*
	 * Create a connection history record.
	 */
	scan = prof->scan;
	hist = adw_wifi_hist_new(wifi, &prof->ssid, scan);
	if (!scan) {
		wifi_error = WIFI_ERR_NOT_FOUND;
		goto fail;
	}
	al_wifi_hostname_set(adw_wifi_hostname);
	wifi->state = WS_IDLE;
	wifi->client_err = AE_IN_PROGRESS;

	adw_log(LOG_INFO "connecting to SSID %s sec %s signal %d",
	    adw_format_ssid(&prof->ssid, ssid_buf, sizeof(ssid_buf)),
	    conf_string(adw_wifi_sec_import(scan->wmi_sec)),
	    scan->rssi);

	hist->error = WIFI_ERR_IN_PROGRESS;
	wifi->join_err = WIFI_ERR_IN_PROGRESS;
	rc = al_wifi_join_from_scan(scan, &prof->phrase);
	if (rc) {
		wifi_error = al_wifi_join_status_get();
		if (!wifi_error) {
			wifi_error = WIFI_ERR_INV_KEY;
		}
		prof->join_errs = WIFI_JOIN_KEY_ERR;
	} else {
		ada_timer_cancel(adw_wifi_scan_timer);
		hist->error = WIFI_ERR_IN_PROGRESS;
		wifi->scan_state = SS_IDLE;
		wifi->state = WS_JOIN;
		ada_callback_pend(&adw_wifi_cbmsg_join);
		adw_wifi_event_post(ADW_EVID_ASSOCIATING);
		return 0;
	}
fail:
	wifi->join_err = wifi_error;
	hist->error = wifi_error;
	prof->join_errs++;
	METRIC_INCR(M_WIFI_CONNECT_FAILURES, 0, 0);
	METRIC_INCR(M_WIFI_CONNECT_ERRORS, 0, wifi_error);
	adw_wifi_current_profile_done(wifi, hist);
	al_wifi_powersave_set((enum al_wifi_powersave_mode)wifi->power_mode);
	return -1;
}

static int adw_wifi_straight_join(struct adw_state *wifi)
{
	struct adw_profile *prof;

	for (prof = wifi->profile; prof < &wifi->profile[ADW_WIFI_PROF_AP];
	     prof++) {
		if (prof->ssid.id[0] != '\0' && prof->enable && prof->scan) {
			break;
		}
	}
	if (prof == &wifi->profile[ADW_WIFI_PROF_AP]) {
		return -1;
	}
	rejoin_times = REJOIN_TIMES_MAX;
	return adw_wifi_join_profile(wifi, prof);
}

/*
 * Scan done.  Do adw_wifi_join if possible.
 * Find the strongest network in the scan results that has a profile
 * and try to join it.
 * If nothing found, and we've been scanning for more than
 * WIFI_SCAN_DEF_LIMIT seconds, go to AP mode, if enabled.
 *
 * Called with lock held.
 */
static void adw_wifi_scan_done(struct adw_state *wifi)
{
	struct adw_profile *prof;
	u32 delay = WIFI_SCAN_DEF_IDLE;
	int scan_prof;
	int join_errs;

	scan_prof = wifi->scan_profile;
	adw_wifi_scan2prof(wifi, scan_prof);

	/*
	 * If we just finished a specific scan, mark the profile scanned.
	 * If the profile is the (not-yet-enabled) preferred profile,
	 * mark it hidden.
	 */
	wifi->scan_profile = 0;
	if (scan_prof) {
		prof = &wifi->profile[scan_prof - 1];
		prof->spec_scan_done = 1;
		if (scan_prof == wifi->pref_profile && prof->scan &&
		    !prof->enable) {
			prof->hidden = 1;
		}
	}

	/*
	 * If there are hidden networks, setup for specific scan.
	 * wifi->scan_profile is the index + 1 of the one to do next.
	 */
	for (prof = &wifi->profile[scan_prof];
	     scan_prof++ < ADW_WIFI_PROF_AP; prof++) {
		if (((prof->enable && prof->hidden) ||
		    scan_prof == wifi->pref_profile) &&
		    !prof->spec_scan_done && !prof->scan &&
		    prof->ssid.id[0] != '\0') {
			wifi->scan_profile = scan_prof;
			break;
		}
	}
	if (wifi->scan_profile) {
		adw_wifi_scan(wifi);
		return;
	}

	prof = adw_wifi_select(wifi);
	if (!prof) {
		join_errs = 0;
		for (prof = wifi->profile;
		    prof < &wifi->profile[ADW_WIFI_PROF_AP]; prof++) {
			join_errs += prof->join_errs;
			prof->join_errs = 0;
		}
		if (join_errs && adw_wifi_health_check(wifi)) {
			wifi->state = WS_RESTART;
			return;
		} else if (wifi->state != WS_UP_AP &&
		    !(wifi->conditional_ap && adw_wifi_configured_nolock())) {
			wifi->state = WS_START_AP;
		}
		delay = WIFI_SCAN_AP_WAIT;
		goto rescan;
	}
	if (prof == &wifi->profile[ADW_WIFI_PROF_AP]) {
		/*
		 * Preferred profile is AP.
		 */
		if (wifi->state != WS_UP_AP) {
			wifi->state = WS_START_AP;
		}
		wifi->scan_state = SS_STOPPED;
		return;
	}
	if (wifi->state == WS_UP_AP) {
		wifi->rejoin = 1;
		delay = WIFI_SCAN_AP_WAIT;
		goto rescan;
	}

	rejoin_times = REJOIN_TIMES_MAX;
	if (adw_wifi_join_profile(wifi, prof) == 0) {
		return;
	}
rescan:
	wifi->scan_state = SS_SCAN_WAIT;
	ada_timer_cancel(adw_wifi_join_timer);
	ada_timer_set(adw_wifi_scan_timer, delay);
}

static int adw_wifi_server_is_active(struct adw_state *wifi)
{
	return wifi->use_time &&
	    TSTAMP_LT(al_clock_get(NULL), wifi->use_time + WIFI_SCAN_AP_WAIT);
}

/*
 * Start wifi scan.
 *
 * Called with lock held.
 */
void adw_wifi_scan(struct adw_state *wifi)
{
	struct adw_profile *prof;
	struct al_wifi_scan_result *scan;
	int enabled_entries = 0;
	struct al_wifi_ssid *scan4;
	enum wifi_error err;
	int rc;
	char ssid_buf[33];

	if (wifi->scan_state == SS_SCAN) {
		return;
	}

	if (wifi->scan_profile) {
		prof = &wifi->profile[wifi->scan_profile - 1];
		scan4 = &prof->ssid;
	} else if (wifi->scan4.len) {
		scan4 = &wifi->scan4;
	} else {
		scan4 = NULL;
	}
	if (wifi->scan_state != SS_SCAN_START) {
		enabled_entries = adw_profile_cnt(wifi);
		/*
		 * If we're in STA mode, don't do automatic periodic scan.
		 * If we're in AP mode with no profiles, don't do automatic
		 * scans.
		 */
		if ((wifi->state == WS_UP) ||
		    (wifi->state == WS_UP_AP && !enabled_entries)) {
			return;
		}
		/*
		 * If local webserver is active, and we've not explicitly
		 * been told to join a network, don't do scans.
		 */
		if (!wifi->pref_profile && adw_wifi_server_is_active(wifi)) {
			goto rescan;
		}
	}
	if (!scan4) {
		for (scan = wifi->scan; scan < &wifi->scan[ADW_WIFI_SCAN_CT];
		     scan++) {
			scan->rssi = WIFI_MIN_SIG;
			scan->ssid.len = 0;
		}
		for (prof = wifi->profile;
		     prof < &wifi->profile[ADW_WIFI_PROF_AP]; prof++) {
			prof->scan = NULL;
			prof->spec_scan_done = 0;
		}
	}
	adw_unlock();
	rc = al_wifi_scan(scan4, adw_wifi_scan_callback);
	adw_lock();
	if (!rc) {
		adw_log(LOG_DEBUG "scan started %s",
		    scan4 ? adw_format_ssid(scan4, ssid_buf,
		    sizeof(ssid_buf)) : "");
		adw_wifi_scan_snapshot_reset();
		wifi->scan_report = 0;
		if (wifi->scan_state == SS_IDLE) {
			wifi->scan_time = al_clock_get_total_ms();
		}
		wifi->scan_state = SS_SCAN;
	} else {
		err = WIFI_ERR_MEM;		/* error unknown */
		METRIC_INCR(M_WIFI_CONNECT_ERRORS, 0, err);
rescan:
		wifi->scan_state = SS_SCAN_WAIT;
	}
	ada_timer_set(adw_wifi_scan_timer, WIFI_SCAN_MIN_LIMIT);
}

/*
 * Event callback from al_wifi.
 */
static int adw_wifi_al_event(enum al_wifi_event event, void *arg)
{
	struct adw_state *wifi = (struct adw_state *)arg;

	switch (event) {
	case AL_WIFI_EVENT_STA_LOST:
		wifi->join_err = al_wifi_join_status_get();
		break;
	case AL_WIFI_EVENT_STA_UP:
		wifi->join_err = WIFI_ERR_NONE;
		break;
	default:
		break;
	}
	return 0;
}

/*
 * Start Wi-Fi.
 * Called with lock held.
 */
static int adw_wifi_start(void)
{
	struct adw_state *wifi = &adw_state;
	static u8 done;

	adw_log(LOG_DEBUG "wifi start");
	if (al_wifi_on()) {
		return -1;
	}
	al_wifi_country_code_set(wifi->country_code);
	adw_wifi_event_post(ADW_EVID_ENABLE);

	/*
	 * Register for client callbacks.
	 * Do not do this in adw_init() before ADA is initialized.
	 */
	if (!done) {
		ada_client_event_register(adw_wifi_client_event, &adw_state);
		done = 1;
	}
	al_mdns_server_up(NULL, conf_sys_dev_id);
	return 0;
}

/*
 * Stop Wi-Fi.  Turn off chip.
 * Called with lock held.
 */
void adw_wifi_stop(void)
{
	adw_log(LOG_DEBUG "wifi stop");
	al_mdns_server_down(NULL);
	al_wifi_off();
	adw_wifi_event_post(ADW_EVID_DISABLE);
}

/*
 * Step to next state in mon_wifi state machine.
 *
 * Called with lock held.
 * Called only in the ADA thread.
 */
static int adw_wifi_step(struct adw_state *wifi)
{
	struct adw_profile *prof;
	struct adw_wifi_history *hist;
	enum adw_wifi_conn_state state;
	u8 rejoin;
	u8 enabled;
	int save = 0;

	do {
		if (wifi->scan_state == SS_DONE && !wifi->scan_report) {
			adw_wifi_scan_report(wifi);
			wifi->scan4.len = 0;
			wifi->scan_report = 1;
			adw_wifi_event_post(ADW_EVID_SCAN_DONE);
		}
		rejoin = wifi->rejoin;
		wifi->rejoin = 0;
		enabled = wifi->enable && adw_wifi_fw_ok;

		state = wifi->state;
		switch (state) {
		/*
		 * Start scan if enabled and not already running or joined.
		 */
		case WS_DISABLED:
			if (enabled) {
				wifi->state = WS_IDLE;
			} else if (wifi->started) {
				wifi->started = 0;
				adw_wifi_stop();
			}
			break;

		case WS_ERROR:
			return 0;

		case WS_RESTART:
			/*
			 * Stop and start Wi-Fi.
			 * This is used in trying to recover from wedged states.
			 */
			wifi->started = 0;
			adw_wifi_stop_ap(wifi);
			adw_wifi_stop();
			wifi->state = WS_IDLE;
			wifi->scan_state = SS_IDLE;
			if (++wifi->reset_cnt >= WIFI_MAX_FAILS) {
				adw_wifi_event_post(ADW_EVID_RESTART_FAILED);
				wifi->reset_cnt = 0;
			}
			adw_log(LOG_WARN "resetting wifi");
			break;

		case WS_IDLE:
			if (!wifi->started) {
				if (adw_wifi_start()) {
					wifi->state = WS_ERROR;
					return 0;
				}
				wifi->started = 1;
			}
			if (!adw_wifi_fw_ok) {
				wifi->state = WS_IDLE;
				break;
			}

			if (wifi->scan_state == SS_DONE ||
			    (wifi->scan_state == SS_SCAN_WAIT && rejoin)) {
				wifi->state = WS_SCAN_DONE;
				break;
			}
			if (wifi->scan_state == SS_IDLE) {
				if (adw_wifi_straight_join(wifi) < 0) {
					adw_wifi_scan(wifi);
				}
			}
			break;

		case WS_SCAN_DONE:
			if (!enabled) {
				wifi->state = WS_DISABLED;
				wifi->scan_state = SS_IDLE;
				ada_timer_cancel(adw_wifi_scan_timer);
				ada_timer_cancel(adw_wifi_join_timer);
				adw_wifi_stop_ap(wifi);
				break;
			}
			if (wifi->scan_state != SS_SCAN) {
				adw_wifi_scan_done(wifi);
			} else {
				adw_log(LOG_DEBUG "scan not done, wait");
			}
			break;

		case WS_DHCP:
			if (!enabled || rejoin) {
				goto up_state;
			}
			hist = &wifi->hist[wifi->hist_curr];
			if (adw_wifi_dhcpc_poll(wifi, hist)) {
				ada_timer_set(adw_wifi_step_timer,
				    WIFI_DHCP_POLL);
				break;
			}
			ada_timer_cancel(adw_wifi_client_timer);

			/*
			 * The IP stack might not correctly handle two
			 * interfaces with the same link-local IPv4 address.
			 * If it does, the conflict check will return zero.
			 *
			 * See the station and AP IP addresses overlap, and
			 * if so, shut down the AP.
			 */
			if (wifi->ap_up &&
			    adw_addr_conflict_check(hist->ip_addr,
			    hist->netmask,
			    ADW_WIFI_AP_IP, ADW_WIFI_AP_NETMASK)) {
				adw_log(LOG_WARN "STA IP and AP IP conflict");
				adw_wifi_stop_ap(wifi);
			}

			save |= wifi->save_on_ap_connect;

			adw_wifi_event_post(ADW_EVID_STA_DHCP_UP);

			switch (wifi->client_err) {
			case AE_OK:
				save |= wifi->save_on_server_connect;
				hist->error = WIFI_ERR_NONE;
				hist->last = 1;
				wifi->state = WS_UP;	/* client not enabled */
				METRIC_INCR(M_WIFI_CONNECT_SUCCESSES, 0, 0);
				METRIC_OP_END(M_WIFI_CONNECT_LATENCY, 0);
				adw_wifi_stayup();
				conf_persist(CT_wifi, adw_wifi_export_cur_prof,
				    wifi);
				prof = &wifi->profile[wifi->curr_profile];
				adw_wifi_status_notify(wifi, &prof->ssid,
				    hist->error, 1);
				break;
			case AE_IN_PROGRESS:
				ada_timer_set(adw_wifi_client_timer,
				    CLIENT_WAIT);
				wifi->state = WS_WAIT_CLIENT;
				break;
			default:
				break;
			}
			break;

		case WS_WAIT_CLIENT:
			if (!enabled || rejoin) {
				goto up_state;
			}
			if (wifi->client_err == AE_IN_PROGRESS ||
			    wifi->client_err == AE_NOTCONN) {
				adw_log(LOG_DEBUG
				    "client not connected - keep waiting");
				break;
			}
			if (wifi->client_err != AE_OK) {
				adw_log(LOG_DEBUG "wait_client err %d",
				    wifi->client_err);
				adw_wifi_service_fail(wifi);
				break;
			}
#ifdef WIFI_CONCURRENT_AP_STA_MODE	/* platform-specific define */
			adw_wifi_stop_ap_sched(WIFI_STOP_AP_TMO);
#else
			if (adap_wifi_features_get() & AWF_SIMUL_AP_STA) {
				adw_wifi_stop_ap_sched(WIFI_STOP_AP_TMO);
			}
#endif
			save |= wifi->save_on_server_connect;
			hist = &wifi->hist[wifi->hist_curr];
			hist->error = WIFI_ERR_NONE;
			hist->last = 1;
			wifi->state = WS_UP;
			METRIC_INCR(M_WIFI_CONNECT_SUCCESSES, 0, 0);
			METRIC_OP_END(M_WIFI_CONNECT_LATENCY, 0);
			adw_wifi_stayup();
			conf_persist(CT_wifi, adw_wifi_export_cur_prof, wifi);
			prof = &wifi->profile[wifi->curr_profile];
			adw_wifi_status_notify(wifi, &prof->ssid,
			    hist->error, 1);
			break;

		case WS_UP:
up_state:
			ada_timer_cancel(adw_wifi_client_timer);
			if (!enabled || rejoin) {
				adw_wifi_event_post(ADW_EVID_STA_DOWN);
			}
			if (enabled && adw_wifi_dhcpc_poll(wifi, NULL)) {
				wifi->state = WS_DHCP;
				ada_timer_set(adw_wifi_client_timer,
				    WIFI_DHCP_WAIT);
				adw_wifi_event_post(ADW_EVID_STA_DOWN);
			}
			if ((!enabled || rejoin) &&
			    wifi->scan_state != SS_SCAN) {
				ada_timer_cancel(adw_wifi_client_timer);
				al_wifi_leave();
				wifi->state = WS_SCAN_DONE;
			}
			break;

		case WS_START_AP:
			adw_wifi_start_ap(wifi);
			break;

		case WS_UP_AP:
			if (!wifi->ap_up) {
				if (adw_wifi_dhcps_poll(wifi, ADW_WIFI_AP_IP)) {
					ada_timer_set(adw_wifi_step_timer,
					    WIFI_DHCP_POLL);
					break;
				}
				adw_wifi_event_post(ADW_EVID_AP_UP);
				wifi->ap_up = 1;
			}

			if (!enabled || rejoin) {
				wifi->state = WS_SCAN_DONE;
#ifndef WIFI_CONCURRENT_AP_STA_MODE	/* platform-specific define */
				if (adap_wifi_features_get()
				    & AWF_SIMUL_AP_STA) {
					break;
				}
				/*
				 * Drop lock for adw_wifi_stop_ap().
				 * Re-evaluate state after reacquiring lock.
				 */
				adw_unlock();
				adw_wifi_stop_ap(wifi);
				adw_lock();
#endif
				break;
			}
			if (wifi->scan_state == SS_DONE) {
				adw_wifi_scan_done(wifi);
			} else if (wifi->scan_state == SS_IDLE) {
				adw_wifi_scan(wifi);
			}
			break;

		/*
		 * If not joined, wait until called after configuration changes.
		 */
		case WS_JOIN:
			break;
		}
	} while (wifi->state != state || wifi->rejoin);

	if (save) {
		prof = &wifi->profile[wifi->curr_profile];
		prof->join_errs = 0;
		if (wifi->curr_profile + 1 == wifi->pref_profile) {
			prof->enable = 1;
			wifi->pref_profile = 0;
		}
	}
	return save;
}

/*
 * Advance the state machine.
 * Called only in the ADA thread.
 */
static void adw_wifi_step_cb(void *wifi_arg)
{
	struct adw_state *wifi = wifi_arg;
	int save;

	adw_lock();
	save = adw_wifi_step(wifi);
	adw_unlock();
	if (save) {
		adw_log(LOG_DEBUG "step_cb: saving Wi-Fi profiles");
		conf_persist(CT_wifi, adw_wifi_export_profiles, wifi);
	}
}

static struct ada_timer *adw_wifi_timer_create(void (*handler)(void *))
{
	struct ada_timer *atimer;

	atimer = ada_timer_create(handler, NULL);
	ASSERT(atimer);
	return atimer;
}

/*
 * initialize mon_wifi.
 * Called before adw_conf_load(), so wifi isn't enabled yet.
 * Initialize lock.
 * Set defaults.  May be overridden by config.
 */
void adw_wifi_init(void)
{
	struct adw_state *wifi = &adw_state;
	static u8 done;
	int enable_redirect = 1;

	if (done) {
		return;
	}
	done = 1;

	adw_wifi_lock = al_os_lock_create();
	ASSERT(adw_wifi_lock);
	adw_wifi_rssi_timer = adw_wifi_timer_create(adw_wifi_rssi_timeout);
	adw_wifi_scan_timer = adw_wifi_timer_create(adw_wifi_rescan);
	adw_wifi_step_timer = adw_wifi_timer_create(adw_wifi_step_timeout);
	adw_wifi_join_timer = adw_wifi_timer_create(adw_wifi_join_timeout);
	adw_wifi_client_timer = adw_wifi_timer_create(adw_wifi_client_timeout);
	adw_wifi_ap_mode_timer =
	    adw_wifi_timer_create(adw_wifi_ap_mode_timeout);
	wifi->save_on_server_connect = 1;
	callback_init(&adw_wifi_cbmsg_join, adw_wifi_check_join, wifi);
	callback_init(&adw_wifi_cbmsg_step, adw_wifi_step_cb, wifi);
	callback_init(&adw_wifi_cbmsg_delayed,
	    adw_wifi_step_arm_timer, wifi);
	callback_init(&adw_wifi_cbmsg_event, adw_wifi_event_cb, wifi);
	adw_wifi_page_init(enable_redirect);
	al_wifi_init();
	al_wifi_set_event_cb(&adw_state, adw_wifi_al_event);
	conf_table_entry_add(&adw_wifi_conf_entry);
	conf_table_entry_add(&adw_wifi_ip_conf_entry);
}

/*
 * Return pointer to the local MAC address of STA interface.
 */
u8 *adw_wifi_mac(struct adw_state *wifi)
{
	static u8 mac[6];
	struct al_net_if *nif;

	memset(mac, 0, sizeof(mac));
	nif = al_net_if_get(AL_NET_IF_STA);
	if (nif) {
		al_net_if_get_mac_addr(nif, mac);
	}
	return mac;
}

/*
 * Set power savings mode for wifi chip. If we're far enough in bringup,
 * set the mode immediatelly.
 */
void adw_wifi_powersave(enum adw_wifi_powersave_mode new_mode)
{
	struct adw_state *wifi = &adw_state;

	if (new_mode > ADW_WIFI_PS_ON_LESS_BEACONS) {
		ASSERT(0);
		return;
	}
	adw_lock();
	wifi->power_mode = new_mode;
	if (wifi->state >= WS_WAIT_CLIENT) {
		/*
		 * Apply change immediately.
		 */
		al_wifi_powersave_set(new_mode);
	}
	adw_unlock();
}

/*
 * Returns 1 if there is a configured WIFI profile (not AP config).
 */
static int adw_wifi_configured_nolock(void)
{
	struct adw_state *wifi = &adw_state;
	struct adw_profile *prof;
	int rc;

	rc = wifi->pref_profile != 0;
	for (prof = wifi->profile; prof < &wifi->profile[ADW_WIFI_PROF_AP];
	     prof++) {
		if (prof->enable == 1) {
			rc = 1;
			break;
		}
	}
	return rc;
}

int adw_wifi_configured(void)
{
	int rc;

	adw_lock();
	rc = adw_wifi_configured_nolock();
	adw_unlock();

	return rc;
}

int adw_wifi_in_ap_mode(void)
{
	struct adw_state *wifi = &adw_state;

	return wifi->ap_up;
}

int adw_wifi_get_ssid(void *buf, size_t len)
{
	int rc = 0;
	struct adw_state *wifi = &adw_state;
	struct adw_profile *prof;

	memset(buf, 0, len);
	adw_lock();
	switch (wifi->state) {
	case WS_JOIN:
	case WS_DHCP:
	case WS_WAIT_CLIENT:
	case WS_UP:
		prof = &wifi->profile[wifi->curr_profile];
		if (len > prof->ssid.len) {
			len = prof->ssid.len;
		}
		memcpy(buf, prof->ssid.id, len);
		rc = len;
	default:
		break;
	}
	adw_unlock();
	return rc;
}

int adw_scan_result_count(void)
{
	struct adw_state *wifi = &adw_state;
	struct al_wifi_scan_result *scan;
	int count = 0;

	for (scan = wifi->scan; scan < &wifi->scan[ADW_WIFI_SCAN_CT]; scan++) {
		if (scan->ssid.len && scan->type == AL_WIFI_BT_INFRASTRUCTURE) {
			count++;
		}
	}
	return count;
}

/* Required by ADA to allow it to use other Wi-FI stacks. */
enum ada_wifi_features adap_wifi_features_get(void)
{
	enum ada_wifi_features features = 0;

#ifdef WIFI_CONCURRENT_AP_STA_MODE
	features |= AWF_SIMUL_AP_STA;
#endif
	return features;
}

/* Required by ADA to allow it to use other Wi-FI stacks. */
int adap_wifi_in_ap_mode(void)
{
	return adw_wifi_in_ap_mode();
}

/* Required by ADA to allow it to use other Wi-FI stacks. */
int adap_net_get_signal(int *signalp)
{
	s16 rssi;

	if (al_wifi_get_rssi(&rssi)) {
		return -1;
	}
	*signalp = (int)rssi;
	return 0;
}

/*
 * Check station interface for DHCP status.
 */
static int adw_wifi_dhcpc_poll(struct adw_state *wifi,
		struct adw_wifi_history *hist)
{
	int i;
	struct al_net_if *net_if;
	struct al_net_addr *p_addr;
	struct al_net_addr dns_addr;
	u32 ipv4;

	net_if = al_net_if_get(AL_NET_IF_STA);
	if (!net_if) {
		return -1;
	}

	ipv4 = al_net_if_get_ipv4(net_if);
	if (!ipv4) {
		return -1;
	}
	if (ADW_WIFI_APIPA == (0xffff0000 & ipv4)) {
		return -1;
	}
	if (!hist) {
		return 0;
	}
	hist->time = al_clock_get(NULL);
	hist->ip_addr = ipv4;

	ipv4 = al_net_if_get_netmask(net_if);
	hist->netmask = ipv4;

	p_addr = al_net_if_get_default_gw(net_if);
	if (!p_addr) {
		hist->def_route = 0;
	} else {
		hist->def_route = al_net_addr_get_ipv4(p_addr);
#ifdef AYLA_SCM_SUPPORT
        /* al_net_if_get_default_gw has already swapped endian.
         */
        hist->def_route = lwip_htonl(hist->def_route);
#endif
	}

	for (i = 0; i < WIFI_HIST_DNS_SERVERS; i++) {
		if (al_net_dns_server_get(AL_NET_IF_STA, i, &dns_addr)) {
			hist->dns_servers[i] = 0;
			continue;
		}
		hist->dns_servers[i] = al_net_addr_get_ipv4(&dns_addr);
#ifdef AYLA_SCM_SUPPORT
        /* al_net_dns_server_get has already swapped endian.
         */
        hist->dns_servers[i] = lwip_htonl(hist->dns_servers[i]);
#endif
	}
	return 0;
}

/*
 * Check AP interface for DHCP status.
 */
static int adw_wifi_dhcps_poll(struct adw_state *wifi, u32 ip)
{
	struct al_net_if *net_if;
	u32 ipv4;

	net_if = al_net_if_get(AL_NET_IF_AP);
	ASSERT(net_if);

	ipv4 = al_net_if_get_ipv4(net_if);
	if (0 == ipv4 || (u32)-1 == ipv4) {
		return -1;
	}

	if (ipv4 != ip) {
		return 1;
	}

	return 0;
}

/*
 * Return AL security value for conf_token.
 */
enum al_wifi_sec adw_wifi_sec_export(enum conf_token sec)
{
	enum al_wifi_sec wmi_sec;

	switch (sec) {
	case CT_WEP:
		wmi_sec = AL_WIFI_SEC_WEP;
		break;
	case CT_WPA:
		wmi_sec = AL_WIFI_SEC_WPA;
		break;
	case CT_WPA2_Personal:
		wmi_sec = AL_WIFI_SEC_WPA2;
		break;
	case CT_WPA3_Personal:
		wmi_sec = AL_WIFI_SEC_WPA3;
		break;
	case CT_none:
	default:
		wmi_sec = AL_WIFI_SEC_NONE;
		break;
	}
	return wmi_sec;
}

/*
 * Returns conf_token for AL security value, or 0 if not usable.
 */
enum conf_token adw_wifi_sec_import(enum al_wifi_sec wmi_sec)
{
	enum conf_token sec = 0;

	switch (wmi_sec) {
	case AL_WIFI_SEC_WEP:
		sec = CT_WEP;
		break;
	case AL_WIFI_SEC_WPA:
		sec = CT_WPA;
		break;
	case AL_WIFI_SEC_WPA2:
		sec = CT_WPA2_Personal;
		break;
	case AL_WIFI_SEC_WPA3:
		sec = CT_WPA3_Personal;
		break;
	case AL_WIFI_SEC_NONE:
	default:
		sec = CT_none;
		break;
	}
	return sec;
}

/*
 * Returns string for AL security value.
 */
const char *adw_wifi_sec_string(enum al_wifi_sec wmi_sec)
{
	const char *sec;

	switch (wmi_sec) {
	case AL_WIFI_SEC_NONE:
		sec = "None";
		break;
	case AL_WIFI_SEC_WEP:
		sec = "WEP";
		break;
	case AL_WIFI_SEC_WPA:
		sec = "WPA";
		break;
	case AL_WIFI_SEC_WPA2:
		sec = "WPA2 Personal";
		break;
	case AL_WIFI_SEC_WPA3:
		sec = "WPA3 Personal";
		break;
	default:
		sec = "Unknown";
		break;
	}
	return sec;
}

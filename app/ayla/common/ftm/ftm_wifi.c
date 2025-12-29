/*
 * Copyright 2012-2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ada/err.h>
#include <ayla/tlv.h>
#include <ayla/conf_token.h>
#include <ayla/conf.h>
#include <ayla/clock.h>
#include <ayla/log.h>

#include <FreeRTOS.h>
#include <task.h>

#include <compat_if.h>
#include <if_media.h>

#include "wise_event_loop.h"
#include "scm_wifi.h"

#include "ftm.h"

#define SCM_NEED_DHCP_START(event) ((event)->event_info.connected.not_en_dhcp == false)

static bool is_dryrun;
static scm_wifi_scan_params sp;

static void ftm_wifi_connect(void);
static void ftm_wifi_test_done(void);

static wise_err_t ftm_wifi_event_handler(void *ctx, system_event_t *event)
{
    int err;
	switch (event->event_id) {
	case SYSTEM_EVENT_STA_START:
		log_put(LOG_DEBUG "STA_START");
		break;
	case SYSTEM_EVENT_STA_STOP:
		log_put(LOG_DEBUG "STA_STOP");
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
	{
		char ip[IP4ADDR_STRLEN_MAX] = {0,};
		scm_wifi_get_ip("wlan0", ip, sizeof(ip), NULL, 0, NULL, 0);
        if (is_dryrun == false) {
            ftm_wifi_test_done();
        }
        ftm_success();
		log_put(LOG_INFO "WIFI GOT IP: <%s>", ip);
		break;
	}
	case SYSTEM_EVENT_STA_CONNECTED:
		log_put(LOG_INFO "STA_CONNECTED");

		if (SCM_NEED_DHCP_START(event)) {
			scm_wifi_status connect_status;
			netifapi_dhcp_start(scm_wifi_get_netif(WISE_IF_WIFI_STA));
			scm_wifi_sta_get_connect_info(&connect_status);
			scm_wifi_sta_dump_ap_info(&connect_status);
		}
		break;
	case SYSTEM_EVENT_STA_NO_NETWORK:
		if (strcmp(event->ifname, "wlan0") == 0)
			log_put(LOG_WARN "WIFI: No suitable network found");
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		log_put(LOG_INFO "WIFI DISCONNECT: %d", event->event_info.disconnected.reason);
		break;
	case SYSTEM_EVENT_SCAN_DONE:
    {
        uint16_t num;
	    scm_wifi_ap_info *ap_info = NULL;

        ap_info = malloc(sizeof(scm_wifi_ap_info) * 2);
        if (ap_info == NULL) {
		    log_put(LOG_ERR "WIFI: Can't allocate memory");
            break;
        }
	    err = scm_wifi_sta_scan_results(ap_info, &num, 2);
        if (err == WISE_OK && num > 0) {
            log_put(LOG_INFO "SSID: %s, RSSI: %d", ap_info[0].ssid, ap_info[0].rssi);
            if (!strncmp(ap_info[0].ssid, "factory-test-dryrun", SCM_WIFI_MAX_SSID_LEN)) {
                is_dryrun = true;
            } else {
                is_dryrun = false;
            }
            if (ap_info[0].rssi >= -60) {
                scm_wifi_assoc_request req = {0};
	            const char *ssid = ap_info[0].ssid;

                memcpy(req.ssid, ssid, strlen(ssid));
                req.auth = SCM_WIFI_SECURITY_WPA2PSK;
                memcpy(req.key, ssid, strlen(ssid));
                memset(req.bssid, 0, sizeof(req.bssid));
                req.pairwise = SCM_WIFI_PAIRWISE_AES;
                req.hidden_ap = 0;

	            err = scm_wifi_sta_set_config(&req, NULL);
                if (err != WISE_OK) {
		            log_put(LOG_ERR "Error(%d) from scm_wifi_sta_set_config", err);
                }
                err = scm_wifi_sta_connect();
                if (err != WISE_OK) {
		            log_put(LOG_ERR "Error(%d) from scm_wifi_sta_connect", err);
                }
                ftm_error(100, 200, 3, 2000);
            } else {
                err = scm_wifi_sta_advance_scan(&sp);
                if (err != WISE_OK) {
                    log_put(LOG_ERR "[%s] scm_wifi_sta_advance_scan err: %d", __func__, err);
                }
                ftm_error(100, 200, 2, 2000);
            }
        } else {
            log_put(LOG_INFO "SSID factory-test(-dryrun) not found");
            err = scm_wifi_sta_advance_scan(&sp);
            if (err != WISE_OK) {
                log_put(LOG_ERR "[%s] scm_wifi_sta_advance_scan err: %d", __func__, err);
            }
            ftm_error(100, 200, 1, 2000);
        }
        free(ap_info);
		break;
    }
	default:
		break;
	}

	return WISE_OK;
}

static void ftm_wifi_test_done(void)
{
    ftm_wifi = 0;
    al_persist_data_write(AL_PERSIST_FACTORY, "ftm/ftm_wifi", &ftm_wifi, sizeof(ftm_wifi));
}

static void ftm_wifi_connect(void)
{
}

static void ftm_wifi_scan(void)
{
	char ifname[WIFI_IFNAME_MAX_SIZE + 1] = {0};
	int len = sizeof(ifname);
    const char *ssid_prefix = "factory-test"; /* factory-test(-dryrun) */
    int err;

    scm_wifi_unregister_event();
    err = scm_wifi_register_event_callback(ftm_wifi_event_handler, NULL);
    if (err != WISE_OK) {
        log_put(LOG_ERR "[%s] scm_wifi_register_event_callback err: %d", __func__, err);
        return;
    }

	err = scm_wifi_sta_stop();
    if (err != WISE_OK) {
        log_put(LOG_WARN "[%s] scm_wifi_stop err: %d", __func__, err);
    }

	err = scm_wifi_sta_start(ifname, &len);
    if (err != WISE_OK) {
        log_put(LOG_ERR "[%s] scm_wifi_start err: %d", __func__, err);
        return;
    }

	memset(&sp, 0, sizeof(sp));
    sp.scan_type = SCM_WIFI_SSID_PREFIX_SCAN;
    strncpy(sp.ssid, ssid_prefix, SCM_WIFI_MAX_SSID_LEN);
    sp.ssid_len = strlen(ssid_prefix);

    err = scm_wifi_sta_advance_scan(&sp);
    if (err != WISE_OK) {
        log_put(LOG_ERR "[%s] scm_wifi_sta_advance_scan err: %d", __func__, err);
        return;
    }
}

void ftm_wifi_init(void)
{
    if (ftm_wifi == 0) {
        return;
    }

    ftm_register(&ftm_wifi_scan);
}

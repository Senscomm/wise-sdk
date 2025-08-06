/*
 * Copyright 2011-2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wise_err.h"
#include "wise_wifi.h"
#include "wise_wifi_types.h"
#include "wise_system.h"

#include <ayla/utypes.h>
#include <ayla/clock.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/tlv.h>
#include <ayla/conf.h>

#include <ada/libada.h>
#include "conf.h"
#include "conf_wifi.h"
#include "demo.h"

u8 conf_connected;	/* 1 if ever connected to ADS */

/*
 * Reset the module, optionally to the factory configuration.
 * If factory is non-zero, the app should remove any startup configuration
 * and other customer-specific information.  The agent configuration will
 * already have been put back to the factory settings in this case.
 */
void adap_conf_reset(int factory)
{
	conf_log(LOG_INFO "Ayla Agent requested %sreset",
	    factory ? "factory " : "");
	wise_restart();
}

/*
 * Set platform ADA client configuration items.
 */
void client_conf_init(void)
{
	struct ada_conf *cf = &ada_conf;
	static char hw_id[32];
	static u8 mac[6];

	wise_wifi_get_mac(mac, WISE_IF_WIFI_STA);

	snprintf(hw_id, sizeof(hw_id), "mac-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
	    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	cf->mac_addr = mac;
	cf->hw_id = hw_id;

	/*
	 * Load config for individual modules
	 */
	sched_conf_load();
#ifdef AYLA_WIFI_SUPPORT
	adw_conf_load();
#endif /* AYLA_WIFI_SUPPORT */

	cf->enable = 1;
	cf->get_all = 1;

	/*
	 * Allow the region to default (NULL) for most of the world.
	 * DNS based on the OEM-model and OEM-ID will take
	 * care of directing the device to the correct service.
	 * The only exception as of this writing is China, for which you
	 * should set cf->region = "CN".
	 */
}

/*
 * The user-registered flag changed.
 */
void adap_conf_reg_changed(void)
{
	struct ada_conf *cf = &ada_conf;

	log_put(LOG_INFO "%s: user %sregistered",
	    __func__, cf->reg_user ? "" : "un");
}

/*
 * client_conf_sw_build() returns the string to be reported to the cloud as the
 * module image version.
 */
const char *adap_conf_sw_build(void)
{
	return ada_version_build;
}

/*
 * client_conf_sw_version() returns the string reported to LAN clients with the
 * name, version, and more.
 */
const char *adap_conf_sw_version(void)
{
	return ada_version;
}


/*
 * Copyright 2016 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADW_WIFI_CONF_H__
#define __AYLA_ADW_WIFI_CONF_H__

/*
 * AP-mode IP address and mask.
 * Choosing 10.231.227.81/28 as an address unlikely to conflict with STA net.
 */
#define ADW_WIFI_AP_IP	((10U << 24) | (231 << 16) | (227 << 8) | 81)
#define ADW_WIFI_AP_NETMASK 0xffffff00U

/*
 * DHCP lease ranges.
 * These addresses can be used by the platform, but
 * other addresses within the AP network may be used instead.
 */
#define ADW_WIFI_AP_DHCP_START	(ADW_WIFI_AP_IP + 1)
#define ADW_WIFI_AP_DHCP_END	(ADW_WIFI_AP_IP + 13)
#define ADW_WIFI_AP_DHCP_LEASE	300

/*
 * Set save on AP connect and save on server connect defaults.
 */
void adw_wifi_save_policy_set(int ap_connect, int serv_connect);

/*
 * Set conditional AP mode (default is unconditional).
 */
void adw_wifi_ap_conditional_set(int conditional);

/*
 * Set default AP profile name.
 */
void adw_wifi_ap_ssid_set(const char *ssid);

/*
 * Get AP profile ssid.
 */
const u8 *adw_wifi_ap_ssid_get(void);

/*
 * Set iOS app name.
 */
void adw_wifi_ios_setup_app_set(const char *name);

/*
 * Enable or disable Wi-Fi.
 */
void adw_wifi_enable(void);
void adw_wifi_disable(void);

/*
 * Return non-zero if Wi-Fi is enabled.
 */
int adw_wifi_is_enabled(void);

/*
 * Force AP mode even if profile is configured and connected
 */
void adw_wifi_force_ap_mode(void);

/*
 * Don't force AP mode connect to a profile if present
 */
void adw_wifi_unforce_ap_mode(void);

/*
 * Declare that AP mode is not to be supported on this device.
 */
void adw_wifi_ap_mode_not_supported(void);

/*
 * Set hostname used by DHCP on Wi-Fi interfaces.
 */
void adw_wifi_hostname_set(const char *name);

/*
 * Erase profiles from the running configuration.
 * Will disconnect if the currently-connected profile is erased.
 * Does not save the configuration.
 */
int adw_wifi_profile_erase(unsigned int index);	/* erase specified profile */
void adw_wifi_profile_sta_erase(void);		/* erase all station profiles */
void adw_wifi_profile_ap_erase(void);		/* erase the AP profile */

#endif /* __AYLA_ADW_WIFI_CONF_H__ */

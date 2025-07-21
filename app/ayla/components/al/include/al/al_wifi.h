/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_WIFI_H__
#define __AYLA_AL_COMMON_WIFI_H__

#include <al/al_utypes.h>
#include <ayla/wifi_error.h>

/**
 * @file
 * Wifi interfaces
 */

/**
 * The buffer size to hold the ssid.
 */
#define AL_WIFI_SSID_LEN	32

/**
 * The buffer size to hold the encrypted key.
 */
#define AL_WIFI_ENCRYPTED_KEY_LEN	32

/**
 * Wifi security mode.
 */
enum al_wifi_sec {
	AL_WIFI_SEC_NONE,		/**< None */
	AL_WIFI_SEC_WEP,		/**< WEP */
	AL_WIFI_SEC_WPA,		/**< WPA: TKIP_PSK AES_PSK */
	AL_WIFI_SEC_WPA2,		/**< WPA2: TKIP_PSK AES_PSK
					TKIP_AES_MIXED_PSK WPA_WPA2_MIXED */
	AL_WIFI_SEC_WPA3,		/**< WPA3_Personal */
};

/**
 * BSS type
 */
enum al_wifi_bss_type {
	AL_WIFI_BT_UNKNOWN = 0,		/**< Unknown */
	AL_WIFI_BT_INFRASTRUCTURE,	/**< Infrastructure */
	AL_WIFI_BT_AD_HOC,		/**< AD HOC */
};

/**
 * Event notification type.
 */
enum al_wifi_event {
	AL_WIFI_EVENT_STA_LOST,		/**< STA link lost */
	AL_WIFI_EVENT_STA_UP,		/**< STA link up */
};

/**
 * Power saving modes.
 */
enum al_wifi_powersave_mode {
	AL_WIFI_PS_OFF,			/**< do not use power-saving modes */
	AL_WIFI_PS_ON,			/**< use power-saving modes */
	AL_WIFI_PS_ON_LESS_BEACONS,	/**< reduce beacon listen interval */
};

/**
 * Wifi SSID.
 */
struct al_wifi_ssid {
	u8 len;				/**< SSID length. */
	u8 id[AL_WIFI_SSID_LEN];	/**< SSID that may be in binary. */
};

/**
 * Wifi key.
 */
struct al_wifi_key {
	u8 len;				 /**< Key length. */
	u8 key[AL_WIFI_ENCRYPTED_KEY_LEN];/**< Key that is in binary. */
};

/**
 * The structure holds the scan result.
 */
struct al_wifi_scan_result {
	struct al_wifi_ssid ssid;	/**< SSID. */
	u8 bssid[6];			/**< MAC address. */
	u8 channel;			/**< Wifi channel. */
	enum al_wifi_bss_type type;	/**< BSS type. */
	s16 rssi;			/**< RSSI. */
	enum al_wifi_sec wmi_sec;	/**< Wifi security mode. */
};

/**
 * Initialize Wi-Fi subsystem.
 * This does not turn on Wi-Fi, but initializes the network interfaces.
 * It is safe to call this multiple times.
 *
 * \returns 0 on success.
 */
int al_wifi_init(void);

/**
 * Turn on the Wifi.
 *
 * \returns 0 if successfully turn the Wifi on, others on failure.
 */
int al_wifi_on(void);

/**
 * Turn off the Wifi.
 */
void al_wifi_off(void);

/**
 * Start Wifi AP
 *
 * \param ssid is AP's SSID.
 * \param ip is the IP assigned to the AP network interface.
 * \param mask is the network mask assigned to the AP network interface.
 *
 * \returns 0 if successfully start the Wifi AP, others on failure.
 */
int al_wifi_start_ap(struct al_wifi_ssid *ssid, u32 ip, u32 mask);

/**
 * Stop Wifi AP
 */
void al_wifi_stop_ap(void);

/**
 * Associate a Wifi AP.
 *
 * al_wifi_join() clears the interface IP first, set IP to 0.0.0.0, and
 * then, the ADW uses interface IP is not zero to ensure the DHCP client
 * has gotten an IP from DHCP server.
 *
 * \param ssid is AP's SSID.
 * \param key is encrypted password returned by al_wifi_encrypt_key().
 * \param sec is AP's security mode
 *
 * \returns 0 if successfully join a Wifi AP, others on failure.
 */
int al_wifi_join(struct al_wifi_ssid *ssid, const struct al_wifi_key *key,
	enum al_wifi_sec sec);

/**
 * Associate a Wifi AP given the scan result.
 *
 * al_wifi_join_from_scan() clears the interface IP first, set IP to 0.0.0.0,
 * and then, the ADW uses interface IP is not zero to ensure the DHCP client
 * has gotten an IP from DHCP server.
 *
 * \param scan is AP's scan result.
 * \param key is encrypted password returned by al_wifi_encrypt_key().
 *
 * \returns 0 if successfully join a Wifi AP, others on failure.
 */
int al_wifi_join_from_scan(struct al_wifi_scan_result *scan,
	const struct al_wifi_key *key);

/**
 * Dissconnect from the associated Wifi AP.
 */
void al_wifi_leave(void);

/**
 * Scan the Wifi networks.
 *
 * \param ssid is searched SSID, it is NULL to search all AP in the network.
 * \param scan_cb is a callback function to retrieve the scan result.
 *
 * The scan_cb should be called in the ADA thread, if the scanning runs in a
 * driver thread, please sends it back to the ADA thread.
 * The scan_cb is called multi times to report Wifi APs, the last call, the
 * result is NULL to indicate no further data.
 *
 * The argument result of scan_cb presents the a AP.
 *
 *  \returns zero on success, others on failure.
 */
int al_wifi_scan(struct al_wifi_ssid *ssid,
	void (*scan_cb)(struct al_wifi_scan_result *result));

/**
 * Get the received signal strength.
 *
 * \param rssi points a variable to retrieve the RSSI.
 *
 * \returns 0 if successfully get rssi, others on failure.
 */
int al_wifi_get_rssi(s16 *rssi);

/**
 * Encrypt a key for the AP with ssid.
 *
 * ADW needs to persist the key for the future to sign in the AP, it calls
 * this function to encrypt the key, and then, passes it to al_wifi_join()
 * to associate the AP.
 *
 * If the platform don't support to encrypt the key, just puts the original
 * key into the buffer.
 *
 * \param ssid points the SSID.
 * \param key points the key.
 * \param buf points the buffer to retrieve the encrypted key in binary.
 * \param bufsize is the buffer size in bytes, it is
 * AL_WIFI_ENCRYPTED_KEY_LEN at least.
 *
 * \returns the data is put into the buffer in byte, -1 if the platform doesn't
 * support it.
 */
int al_wifi_encrypt_key(const struct al_wifi_ssid *ssid, const char *key,
	u8 *buf, size_t bufsize);

/**
 * Set the event notification callback.
 *
 * \param arg is the parameter is passed to the event callback.
 * \param event_cb is a callback which will called on wifi event happen.
 * The first argument of wifi_event_cb() is the event type, the second is
 * the value set by al_net_stream_set_arg(). This callback will be called
 * in the ADA thread.
 */
void al_wifi_set_event_cb(void *arg, int (*event_cb)(enum al_wifi_event,
	void *arg));

/**
 * Get the status of the current join attempt.
 *
 * \return wifi error code for join attempt. 0 on success.
 */
enum wifi_error al_wifi_join_status_get(void);

/**
 * Set country code.
 *
 * The country code is an standard two-character string establishing
 * valid Wi-Fi parameters to be used.
 *
 * Note: setting must work even if Wi-Fi is not started yet.
 * This is used as a way to validate the country code for the CLI.
 *
 * \param country_code is a two-character string.
 * \returns zero on success, -1 if country code is invalid.
 */
int al_wifi_country_code_set(const char *country_code);

/**
 * Get country code for display of current setting.
 *
 * \param buf buffer to hold the country code.
 * \param len length of buffer.
 * \return zero on success, -1 on error.
 */
int al_wifi_country_code_get(char *buf, size_t len);

/**
 * Set power-saving mode.
 *
 * \param mode power-saving mode.
 * \returns zero on success, -1 if not supported or unable to set the mode.
 */
int al_wifi_powersave_set(enum al_wifi_powersave_mode mode);

/**
 * Set transmit power.
 *
 * The actual transmit power used may be modified by the platform to respect
 * the restrictions of the country-code in use.
 * If the value exceeds the range of the platform the maximum range
 * should be used.
 *
 * \param tx_power the transmit power in units of 1 dBm.
 * \returns zero on success or if unsupported, -1 if out of range or
 * other error.
 */
int al_wifi_tx_power_set(u8 tx_power);

/**
 * Get transmit power setting.
 *
 * \param tx_powerp a pointer to the resulting transmit power setting, in dBm.
 * \returns zero on success, -1 if not supported or on failure.
 */
int al_wifi_tx_power_get(u8 *tx_powerp);

/**
 * Set the DHCP hostname for station interface.
 *
 * \param hostname a string containing the hostname.  This will be
 * referenced and not copied, so it must not change after the call.
 * If the hostname is NULL or empty, it will be ignored and a
 * default or previously set hostname will be used.
 */
void al_wifi_hostname_set(const char *hostname);

#endif /* __AYLA_AL_COMMON_WIFI_H__ */

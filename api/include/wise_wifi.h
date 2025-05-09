/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.
 */
// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * Originated from esp_wifi.h of ESP8266_RTOS_SDK
 * (https://github.com/espressif/ESP8266_RTOS_SDK)
 * and modified to provide wise Wi-Fi API as being ESP8266 style
 */

#ifndef __WISE_WIFI_H__
#define __WISE_WIFI_H__

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/queue.h"
#include "wise_err.h"
#include "wise_wifi_types.h"
#include "wise_event.h"

#ifdef __cplusplus
extern "C" {
#endif

extern wifi_mode_t s_wifi_mode[2];

#define WISE_ERR_WIFI_NOT_INIT    (WISE_ERR_WIFI_BASE + 1)   /*!< WiFi driver was not installed by wise_wifi_init */
#define WISE_ERR_WIFI_NOT_STARTED (WISE_ERR_WIFI_BASE + 2)   /*!< WiFi driver was not started by wise_wifi_start */
#define WISE_ERR_WIFI_NOT_STOPPED (WISE_ERR_WIFI_BASE + 3)   /*!< WiFi driver was not stopped by wise_wifi_stop */
#define WISE_ERR_WIFI_IF          (WISE_ERR_WIFI_BASE + 4)   /*!< WiFi interface error */
#define WISE_ERR_WIFI_MODE        (WISE_ERR_WIFI_BASE + 5)   /*!< WiFi mode error */
#define WISE_ERR_WIFI_STATE       (WISE_ERR_WIFI_BASE + 6)   /*!< WiFi internal state error */
#define WISE_ERR_WIFI_CONN        (WISE_ERR_WIFI_BASE + 7)   /*!< WiFi internal control block of station or soft-AP error */
#define WISE_ERR_WIFI_NVS         (WISE_ERR_WIFI_BASE + 8)   /*!< WiFi internal NVS module error */
#define WISE_ERR_WIFI_MAC         (WISE_ERR_WIFI_BASE + 9)   /*!< MAC address is invalid */
#define WISE_ERR_WIFI_SSID        (WISE_ERR_WIFI_BASE + 10)  /*!< SSID is invalid */
#define WISE_ERR_WIFI_PASSWORD    (WISE_ERR_WIFI_BASE + 11)  /*!< Password is invalid */
#define WISE_ERR_WIFI_TIMEOUT     (WISE_ERR_WIFI_BASE + 12)  /*!< Timeout error */
#define WISE_ERR_WIFI_WAKE_FAIL   (WISE_ERR_WIFI_BASE + 13)  /*!< WiFi is in sleep state(RF closed) and wakeup fail */
#define WISE_ERR_WIFI_WOULD_BLOCK (WISE_ERR_WIFI_BASE + 14)  /*!< The caller would block */
#define WISE_ERR_WIFI_NOT_CONNECT (WISE_ERR_WIFI_BASE + 15)  /*!< Station still in disconnect status */

#define WISE_WIFI_PARAM_USE_NVS  0
#define WISE_PMK_LEN  32

#define	IEEE80211_BINTVAL_DEFAULT 100	/* default beacon interval (TU's) */
#define IEEE80211_LISTENINT_MINSLEEP	IEEE80211_BINTVAL_DEFAULT
#define IEEE80211_LISTENINT_MAXSLEEP	1000	// 1sec

/**
 * @brief WiFi stack configuration parameters passed to wise_wifi_init call.
 */
typedef struct {
	system_event_handler_t	event_handler;          /**< WiFi event handler */
	uint8_t			mac[6];			/**< WiFi I/F mac address */
	int			static_rx_buf_num;      /**< WiFi static RX buffer number */
	int			dynamic_rx_buf_num;     /**< WiFi dynamic RX buffer number */
	int			tx_buf_type;            /**< WiFi TX buffer type */
	int			static_tx_buf_num;      /**< WiFi static TX buffer number */
	int			dynamic_tx_buf_num;     /**< WiFi dynamic TX buffer number */
	int			cache_tx_buf_num;       /**< WiFi TX cache buffer number */
	int			csi_enable;             /**< WiFi channel state information enable flag */
	int			ampdu_rx_enable;        /**< WiFi AMPDU RX feature enable flag */
	int			ampdu_tx_enable;        /**< WiFi AMPDU TX feature enable flag */
	int			amsdu_tx_enable;        /**< WiFi AMSDU TX feature enable flag */
	int			nvs_enable;             /**< WiFi NVS flash enable flag */
	int			nano_enable;            /**< Nano option for printf/scan family enable flag */
	int			tx_ba_win;              /**< WiFi Block Ack TX window size */
	int			rx_ba_win;              /**< WiFi Block Ack RX window size */
	int			beacon_max_len;         /**< WiFi softAP maximum length of the beacon */
	uint64_t	feature_caps;           /**< Enables additional WiFi features and capabilities */
	bool		sta_disconnected_pm;    /**< WiFi Power Management for station at disconnected status */
	int			magic;                  /**< WiFi init magic number, it should be the last field */
} wifi_init_config_t;

typedef struct {
	uint8_t psk[WISE_PMK_LEN];
	int channel;
} wifi_fast_connect_t;


#define WIFI_INIT_CONFIG_MAGIC    0x1F2F3F4F

#define WIFI_INIT_CONFIG_DEFAULT() {\
    .event_handler = &wise_event_send,\
	.mac = {0x64, 0xf9, 0x47, 0xf0, 0x01, 0x20},\
    .static_rx_buf_num = 5,\
	.dynamic_rx_buf_num = 0,\
	.tx_buf_type = 0,\
	.static_tx_buf_num = 6,\
	.dynamic_tx_buf_num = 0,\
	.csi_enable = 0,\
	.ampdu_rx_enable = 0,\
	.ampdu_tx_enable = 0,\
	.nvs_enable = 1,\
	.nano_enable = 0,\
	.tx_ba_win = 0,\
	.rx_ba_win = 0,\
	.magic = WIFI_INIT_CONFIG_MAGIC\
};

/**
  * @brief  Init WiFi
  *         Alloc resource for WiFi driver, such as WiFi control structure, RX/TX buffer,
  *         WiFi NVS structure etc, this WiFi also start WiFi task
  *
  * @attention 1. This API must be called before all other WiFi API can be called
  * @attention 2. Always use WIFI_INIT_CONFIG_DEFAULT macro to init the config to default values, this can
  *               guarantee all the fields got correct value when more fields are added into wifi_init_config_t
  *               in future release. If you want to set your owner initial values, overwrite the default values
  *               which are set by WIFI_INIT_CONFIG_DEFAULT, please be notified that the field 'magic' of
  *               wifi_init_config_t should always be WIFI_INIT_CONFIG_MAGIC!
  *
  * @param  config pointer to WiFi init configuration structure; can point to a temporary variable.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_NO_MEM: out of memory
  *    - others: refer to error code wise_err.h
  */
wise_err_t wise_wifi_init(const wifi_init_config_t *config);


/**
  * @brief  Deinit WiFi
  *         Free all resource allocated in wise_wifi_init and stop WiFi task
  *
  * @attention 1. This API should be called if you want to remove WiFi driver from the system
  *
  * @return WISE_OK: succeed
  */
wise_err_t wise_wifi_deinit(void);

/**
  * @brief     Set the WiFi operating mode
  *
  *            Set the WiFi operating mode as station, soft-AP or station+soft-AP,
  *            The default mode is soft-AP mode.
  *
  * @param     mode  WiFi operating mode
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - others: refer to error code in wise_err.h
  */
wise_err_t wise_wifi_set_mode(wifi_mode_t mode, uint8_t wlan_if);

/**
  * @brief  Get current operating mode of WiFi
  *
  * @param[out]  mode  store current WiFi mode
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  */
wise_err_t wise_wifi_get_mode(wifi_mode_t *mode, uint8_t wlan_if);

/**
  * @brief  Start WiFi according to current configuration
  *         If mode is WIFI_MODE_STA, it create station control block and start station
  *         If mode is WIFI_MODE_AP, it create soft-AP control block and start soft-AP
  *         If mode is WIFI_MODE_APSTA, it create soft-AP and station control block and start soft-AP and station
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - WISE_ERR_NO_MEM: out of memory
  *    - WISE_ERR_WIFI_CONN: WiFi internal error, station or soft-AP control block wrong
  *    - WISE_FAIL: other WiFi internal errors
  */
wise_err_t wise_wifi_start(uint8_t wlan_if);

/**
  * @brief  Stop WiFi
  *         If mode is WIFI_MODE_STA, it stop station and free station control block
  *         If mode is WIFI_MODE_AP, it stop soft-AP and free soft-AP control block
  *         If mode is WIFI_MODE_APSTA, it stop station/soft-AP and free station/soft-AP control block
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  */
wise_err_t wise_wifi_stop(uint8_t wlan_if);

/**
 * @brief  Restore WiFi stack persistent settings to default values
 *
 * This function will reset settings made using the following APIs:
 * - wise_wifi_get_auto_connect,
 * - wise_wifi_set_protocol,
 * - wise_wifi_set_config related
 * - wise_wifi_set_mode
 *
 * @return
 *    - WISE_OK: succeed
 *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
 */
wise_err_t wise_wifi_restore(void);

/**
  * @brief     Reconnect the WiFi station to the AP.
  *
  * @attention 1. This API only impact WIFI_MODE_STA or WIFI_MODE_APSTA mode
  * @attention 2. If the station is disconnected to an AP, call wise_wifi_reconnect to reconnect with sched scan.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: failed
  */
wise_err_t wise_wifi_reconnect(uint8_t wlan_if);

/**
  * @brief     Connect the WiFi station to the AP.
  *
  * @attention 1. This API only impact WIFI_MODE_STA or WIFI_MODE_APSTA mode
  * @attention 2. If the station is connected to an AP, call wise_wifi_disconnect to disconnect.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_NOT_START: WiFi is not started by wise_wifi_start
  *    - WISE_ERR_WIFI_CONN: WiFi internal error, station or soft-AP control block wrong
  *    - WISE_ERR_WIFI_SSID: SSID of AP which station connects is invalid
  */
wise_err_t wise_wifi_connect(uint8_t wlan_if);

/**
  * @brief     Disconnect the WiFi station from the AP.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi was not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_NOT_STARTED: WiFi was not started by wise_wifi_start
  *    - WISE_FAIL: other WiFi internal errors
  */
wise_err_t wise_wifi_disconnect(uint8_t wlan_if);

/**
  * @brief     Currently this API is just an stub API
  *

  * @return
  *    - WISE_OK: succeed
  *    - others: fail
  */
wise_err_t wise_wifi_clear_fast_connect(void);

/**
  * @brief     list STA connected to AP.
  *
  * @attention 1. This API only impact WIFI_MODE_AP or WIFI_MODE_APSTA mode
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: failed
  */
wise_err_t wise_wifi_sta_list(uint8_t wlan_if);

/**
  * @brief     deauthenticate all stations or associated id equals to aid
  *
  * @param     aid  when aid is 0, deauthenticate all stations, otherwise deauthenticate station whose associated id is aid
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_NOT_STARTED: WiFi was not started by wise_wifi_start
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - WISE_ERR_WIFI_MODE: WiFi mode is wrong
  */
wise_err_t wise_wifi_deauth_sta(uint16_t aid);


/**
  * @brief     deauthenticate station by mac address
  *
  * @param     txtaddr deauthenticate station whose txtaddr is matched
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: failed
  */
wise_err_t wise_wifi_deauth(uint8_t wlan_if, const char *txtaddr);

/**
  * @brief     Scan all available APs.
  *
  * @attention If this API is called, the found APs are stored in WiFi driver dynamic allocated memory and the
  *            will be freed in wise_wifi_scan_get_ap_records, so generally, call wise_wifi_scan_get_ap_records to cause
  *            the memory to be freed once the scan is done
  * @attention The values of maximum active scan time and passive scan time per channel are limited to 1500 milliseconds.
  *            Values above 1500ms may cause station to disconnect from AP and are not recommended.
  *
  * @param     config  configuration of scanning
  * @param     block if block is true, this API will block the caller until the scan is done, otherwise
  *                         it will return immediately
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_NOT_STARTED: WiFi was not started by wise_wifi_start
  *    - WISE_ERR_WIFI_TIMEOUT: blocking scan is timeout
  *    - others: refer to error code in wise_err.h
  */
wise_err_t wise_wifi_scan_start(const wifi_scan_config_t *config, bool block, uint8_t wlan_if);

/**
  * @brief     Stop the scan in process
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_NOT_STARTED: WiFi is not started by wise_wifi_start
  */
wise_err_t wise_wifi_scan_stop(void);

/**
  * @brief     Get number of APs found in last scan
  *
  * @param[out] number  store number of APIs found in last scan
  *
  * @attention This API can only be called when the scan is completed, otherwise it may get wrong value.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_NOT_STARTED: WiFi is not started by wise_wifi_start
  *    - WISE_ERR_INVALID_ARG: invalid argument
  */
wise_err_t wise_wifi_scan_get_ap_num(uint16_t *number);

void wise_wifi_flush_bss(void);


/**
  * @brief     Get AP list found in last scan
  *
  * @param         ap_idx As input param, get ap list start from index
  * @param[inout]  number As input param, it stores max AP number ap_records can hold.
  *                As output param, it receives the actual AP number this API returns.
  * @param         ap_records  wifi_ap_record_t array to hold the found APs
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_NOT_STARTED: WiFi is not started by wise_wifi_start
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - WISE_ERR_NO_MEM: out of memory
  */
wise_err_t wise_wifi_scan_get_ap_records(uint16_t ap_idx,uint16_t *number, wifi_ap_record_t *ap_records);

/**
  * @brief     Get information of AP which the station is associated with
  *
  * @param     ap_info  the wifi_ap_record_t to hold AP information
  *            sta can get the connected ap's phy mode info through the struct member
  *            phy_11b，phy_11g，phy_11n，phy_lr in the wifi_ap_record_t struct.
  *            For example, phy_11b = 1 imply that ap support 802.11b mode
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_CONN: The station interface don't initialized
  *    - WISE_ERR_WIFI_NOT_CONNECT: The station is in disconnect status
  */
wise_err_t wise_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info, uint8_t wlan_if);


/**
  * @brief     configure the current country code
  *
  * @param     country  country code
  * @attention Configured country info used for connect, the country info of the AP to which the station is connected is used
  * @attention When this API is called, the PHY init data will switch to the PHY init data type corresponding to the country info.
  * @attention When country code “01” (world safe mode) is set, SoftAP mode won’t contain country IE
  * @attention The default country is “01” (world safe mode) and ieee80211d_enabled is TRUE
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: The station interface don't initialized
  *    - WISE_ERR: parameter error status
  */
wise_err_t wise_wifi_set_country_code(char *country, bool ieee80211d_enabled, bool set_ap_mode_enable);

/**
  * @brief     get country code
  *
  * @param     country  the configured country ISO code
  *            ieee80211d_enabled  802.11d is enabled or not
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR: parameter error status
  */
wise_err_t wise_wifi_get_country_code(char *country);

/**
  * @brief     Set current power save type
  *
  * @attention Default power save type is WIFI_PS_NONE.
  *
  * @param     type  power save type
  *
  * @return    WISE_ERR_NOT_SUPPORTED: not supported yet
  */
wise_err_t wise_wifi_set_ps(wifi_ps_type_t type);

/**
  * @brief     Get current power save type
  *
  * @attention Default power save type is WIFI_PS_NONE.
  *
  * @param[out]  type: store current power save type
  *
  * @return    WISE_ERR_NOT_SUPPORTED: not supported yet
  */
wise_err_t wise_wifi_get_ps(wifi_ps_type_t *type);

/**
  * @brief     Set protocol type of specified interface
  *            The default protocol is (WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N)
  *
  * @attention Currently we only support 802.11b or 802.11bg or 802.11bgn mode
  *
  * @param     ifx  interfaces
  * @param     protocol_bitmap  WiFi protocol bitmap
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_IF: invalid interface
  *    - others: refer to error codes in wise_err.h
  */
wise_err_t wise_wifi_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap);

/**
  * @brief     Get the current protocol bitmap of the specified interface
  *
  * @param     ifx  interface
  * @param[out] protocol_bitmap  store current WiFi protocol bitmap of interface ifx
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_IF: invalid interface
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - others: refer to error codes in wise_err.h
  */
wise_err_t wise_wifi_get_protocol(wifi_interface_t ifx, uint8_t *protocol_bitmap);

/**
  * @brief     Set bandwidth of the specified interface
  *
  * @attention 1. API return false if try to configure an interface that is not enabled
  * @attention 2. WIFI_BW_HT40 is supported only when the interface support 11N
  *
  * @param     ifx  interface to be configured
  * @param     bw  bandwidth
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_IF: invalid interface
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - others: refer to error codes in wise_err.h
  */
wise_err_t wise_wifi_set_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t bw);

/**
  * @brief     Get bandwidth of the specified interface
  *
  * @attention 1. API return false if try to get a interface that is not enable
  *
  * @param     ifx interface to be configured
  * @param[out] bw  store bandwidth of interface ifx
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_IF: invalid interface
  *    - WISE_ERR_INVALID_ARG: invalid argument
  */
wise_err_t wise_wifi_get_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t *bw);

/**
  * @brief     Set primary/secondary channel
  *
  * @attention 1. This is a special API for sniffer
  * @attention 2. This API should be called after wise_wifi_start() or wise_wifi_set_promiscuous()
  *
  * @param     interface  interface
  * @param     primary  for HT20, primary is the channel number, for HT40, primary is the primary channel
  * @param     second   for HT20, second must be NONE, for HT40, second is the second channel
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_IF: invalid interface
  *    - WISE_ERR_INVALID_ARG: invalid argument
  */
wise_err_t wise_wifi_set_channel(wifi_interface_t interface, uint8_t primary, wifi_second_chan_t second);

/**
  * @brief     Get the primary/secondary channel
  *
  * @attention 1. API return false if try to get a interface that is not enable
  *
  * @param     interface  interface
  * @param     primary   store current primary channel
  * @param[out]  second  store current second channel
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  */
wise_err_t wise_wifi_get_channel(wifi_interface_t interface, uint8_t *primary, wifi_second_chan_t *second);

/**
  * @brief     configure country info
  *
  * @attention 1. The default country is {.cc="CN", .schan=1, .nchan=13, policy=WIFI_COUNTRY_POLICY_AUTO}
  * @attention 2. When the country policy is WIFI_COUNTRY_POLICY_AUTO, the country info of the AP to which
  *               the station is connected is used. E.g. if the configured country info is {.cc="USA", .schan=1, .nchan=11}
  *               and the country info of the AP to which the station is connected is {.cc="JP", .schan=1, .nchan=14}
  *               then the country info that will be used is {.cc="JP", .schan=1, .nchan=14}. If the station disconnected
  *               from the AP the country info is set back back to the country info of the station automatically,
  *               {.cc="USA", .schan=1, .nchan=11} in the example.
  * @attention 3. When the country policy is WIFI_COUNTRY_POLICY_MANUAL, always use the configured country info.
  * @attention 4. When the country info is changed because of configuration or because the station connects to a different
  *               external AP, the country IE in probe response/beacon of the soft-AP is changed also.
  * @attention 5. The country configuration is not stored into flash
  * @attention 6. This API doesn't validate the per-country rules, it's up to the user to fill in all fields according to
  *               local regulations.
  *
  * @param     country   the configured country info
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  */
wise_err_t wise_wifi_set_country(const wifi_country_t *country);

/**
  * @brief     get the current country info
  *
  * @param     country  country info
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  */
wise_err_t wise_wifi_get_country(wifi_country_t *country);


/**
  * @brief     Set MAC address of the WiFi station or the soft-AP interface.
  *
  * @attention 1. This API can only be called when the interface is disabled
  * @attention 2. Soft-AP and station have different MAC addresses, do not set them to be the same.
  * @attention 3. The bit 0 of the first byte of the MAC address can not be 1. For example, the MAC address
  *      can set to be "1a:XX:XX:XX:XX:XX", but can not be "15:XX:XX:XX:XX:XX".
  *
  * @param     mac  the MAC address
  * @param     wlan_if  the interface id
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - WISE_ERR_WIFI_IF: invalid interface
  *    - WISE_ERR_WIFI_MAC: invalid mac address
  *    - WISE_ERR_WIFI_MODE: WiFi mode is wrong
  *    - others: refer to error codes in wise_err.h
  */
wise_err_t wise_wifi_set_mac(const uint8_t mac[6], uint8_t wlan_if);

/**
  * @brief     Get mac of specified interface
  *
  * @param[out] mac  store mac of the interface ifx
  * @param      wlan_if interface id
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - WISE_ERR_WIFI_IF: invalid interface
  */
wise_err_t wise_wifi_get_mac(uint8_t mac[6], uint8_t wlan_if);

/**
  * @brief The RX callback function in the promiscuous mode.
  *        Each time a packet is received, the callback function will be called.
  *
  * @param buf  Data received. Type of data in buffer (wifi_promiscuous_pkt_t or wifi_pkt_rx_ctrl_t) indicated by 'type' parameter.
  * @param type  promiscuous packet type.
  *
  */
typedef void (* wifi_promiscuous_cb_t)(void *buf, wifi_promiscuous_pkt_type_t type);
/**
  * @brief Register the RX callback function in the promiscuous mode.
  *
  * Each time a packet is received, the registered callback function will be called.
  *
  * @param     wlan_if  wlan interface idx
  * @param cb  callback
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  */
wise_err_t wise_wifi_set_promiscuous_rx_cb(uint8_t wlan_if, wifi_promiscuous_cb_t cb);

/**
  * @brief     Enable the promiscuous mode.
  *
  * @param     wlan_if  wlan interface idx
  * @param     en  false - disable, true - enable
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  */
wise_err_t wise_wifi_set_promiscuous(uint8_t wlan_if, bool en);

/**
  * @brief     Get the promiscuous mode.
  *
  * @param     wlan_if  wlan interface idx
  * @param[out] en  store the current status of promiscuous mode
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  */
wise_err_t wise_wifi_get_promiscuous(uint8_t wlan_if, bool *en);

/**
  * @brief Enable the promiscuous mode packet type filter.
  *
  * @note The default filter is to filter all packets except WIFI_PKT_MISC
  *
  * @param filter the packet type filtered in promiscuous mode.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  */
wise_err_t wise_wifi_set_promiscuous_filter(const wifi_promiscuous_filter_t *filter);

/**
  * @brief     Get the promiscuous filter.
  *
  * @param[out] filter  store the current status of promiscuous filter
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  */
wise_err_t wise_wifi_get_promiscuous_filter(wifi_promiscuous_filter_t *filter);

/**
  * @brief     Clean the configuration of the STA or AP
  *
  * @attention 1. This API can be called only when specified interface is enabled, otherwise, API fail
  *
  * @param     interface  interface
  * @param     wlan_if  wlan interface idx
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: failed
  *    - WISE_ERR_WIFI_MODE: invalid mode
  */
wise_err_t wise_wifi_clean_config(wifi_interface_t interface, uint8_t wlan_if);

/**
  * @brief     Set the configuration of the STA or AP
  *
  * @attention 1. This API can be called only when specified interface is enabled, otherwise, API fail
  * @attention 2. For station configuration, bssid_set needs to be 0; and it needs to be 1 only when users need to check the MAC address of the AP.
  * @attention 3. HW is limited to only one channel, so when in the soft-AP+station mode, the soft-AP will adjust its channel automatically to be the same as
  *               the channel of the station.
  *
  * @param     interface  interface
  * @param     conf  station or soft-AP configuration
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - WISE_ERR_WIFI_IF: invalid interface
  *    - WISE_ERR_WIFI_MODE: invalid mode
  *    - WISE_ERR_WIFI_PASSWORD: invalid password
  *    - WISE_ERR_WIFI_NVS: WiFi internal NVS error
  *    - others: refer to the error code in wise_err.h
  */
wise_err_t wise_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf, uint8_t wlan_if, wifi_fast_connect_t *fast_config);

/**
  * @brief     Get configuration of specified interface
  *
  * @param     interface  interface
  * @param[out]  conf  station or soft-AP configuration
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - WISE_ERR_WIFI_IF: invalid interface
  */
wise_err_t wise_wifi_get_config(wifi_interface_t interface, wifi_config_t *conf);

/**
  * @brief    Save wpa supplicant configuration
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL:Failed
  */
wise_err_t wise_wifi_save_config(void);

/**
  * @brief     Set the IP configuration of the STA or AP
  *
  * @attention 1. This API can be called only when specified interface is enabled, otherwise, API fail
  * @attention 2. For station configuration, IP info might be automatically changed upon DHCP later.
  * @attention 3. For AP configuration, this should be done before wise_wifi_start().
  *
  * @param     interface  interface
  * @param     ipinfo  IP address, netmask, and gateway
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - WISE_ERR_WIFI_IF: invalid interface
  *    - others: refer to the error code in wise_err.h
  */
wise_err_t wise_wifi_set_ip_info(wifi_interface_t interface, wifi_ip_info_t *ipinfo, bool nm, bool gw);

/**
  * @brief     Get IP configuration of specified interface
  *
  * @param     interface  interface
  * @param[out]  ipinfo  IP address, netmask, and gateway
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - WISE_ERR_WIFI_IF: invalid interface
  */

wise_err_t wise_wifi_get_ip_info(wifi_interface_t interface, wifi_ip_info_t *ipinfo);

/**
  * @brief     Get STAs associated with soft-AP
  *
  * @attention SSC only API
  *
  * @param[out] sta  station list
  *             ap can get the connected sta's phy mode info through the struct member
  *             phy_11b，phy_11g，phy_11n，phy_lr in the wifi_sta_info_t struct.
  *             For example, phy_11b = 1 imply that sta support 802.11b mode
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - WISE_ERR_WIFI_MODE: WiFi mode is wrong
  *    - WISE_ERR_WIFI_CONN: WiFi internal error, the station/soft-AP control block is invalid
  */
wise_err_t wise_wifi_ap_get_sta_list(wifi_sta_list_t *sta);


/**
  * @brief     Set the WiFi API configuration storage type
  *
  * @attention 1. The default value is WIFI_STORAGE_FLASH
  *
  * @param     storage : storage type
  *
  * @return
  *   - WISE_OK: succeed
  *   - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *   - WISE_ERR_INVALID_ARG: invalid argument
  */
wise_err_t wise_wifi_set_storage(wifi_storage_t storage);

/**
  * @brief     Set auto connect
  *            The default value is true
  *
  * @param     en : true - enable auto connect / false - disable auto connect
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_MODE: WiFi internal error, the station/soft-AP control block is invalid
  *    - others: refer to error code in wise_err.h
  */
wise_err_t wise_wifi_set_auto_connect(bool en) __attribute__ ((deprecated));

/**
  * @brief     Get the auto connect flag
  *
  * @param[out] en  store current auto connect configuration
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_INVALID_ARG: invalid argument
  */
wise_err_t wise_wifi_get_auto_connect(bool *en) __attribute__ ((deprecated));

/**
  * @brief     Set 802.11 Vendor-Specific Information Element
  *
  * @param     enable If true, specified IE is enabled. If false, specified IE is removed.
  * @param     type Information Element type. Determines the frame type to associate with the IE.
  * @param     idx  Index to set or clear. Each IE type can be associated with up to two elements (indices 0 & 1).
  * @param     vnd_ie Pointer to vendor specific element data. First 6 bytes should be a header with fields matching vendor_ie_data_t.
  *            If enable is false, this argument is ignored and can be NULL. Data does not need to remain valid after the function returns.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init()
  *    - WISE_ERR_INVALID_ARG: Invalid argument, including if first byte of vnd_ie is not WIFI_VENDOR_IE_ELEMENT_ID (0xDD)
  *      or second byte is an invalid length.
  *    - WISE_ERR_NO_MEM: Out of memory
  */
wise_err_t wise_wifi_set_vendor_ie(bool enable, wifi_vendor_ie_type_t type, wifi_vendor_ie_id_t idx, const void *vnd_ie);

/**
  * @brief     Function signature for received Vendor-Specific Information Element callback.
  * @param     ctx Context argument, as passed to wise_wifi_set_vendor_ie_cb() when registering callback.
  * @param     type Information element type, based on frame type received.
  * @param     sa Source 802.11 address.
  * @param     vnd_ie Pointer to the vendor specific element data received.
  * @param     rssi Received signal strength indication.
  */
typedef void (*wise_vendor_ie_cb_t) (void *ctx, wifi_vendor_ie_type_t type, const uint8_t sa[6], const vendor_ie_data_t *vnd_ie, int rssi);

/**
  * @brief     Register Vendor-Specific Information Element monitoring callback.
  *
  * @param     cb   Callback function
  * @param     ctx  Context argument, passed to callback function.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  */
wise_err_t wise_wifi_set_vendor_ie_cb(wise_vendor_ie_cb_t cb, void *ctx);

/**
  * @brief     Set maximum WiFi transmitting power
  *
  * @attention WiFi transmitting power is divided to six levels in phy init data.
  *            Level0 represents highest transmitting power and level5 represents lowest
  *            transmitting power. Packets of different rates are transmitted in
  *            different powers according to the configuration in phy init data.
  *            This API only sets maximum WiFi transmitting power. If this API is called,
  *            the transmitting power of every packet will be less than or equal to the
  *            value set by this API. If this API is not called, the value of maximum
  *            transmitting power set in phy_init_data.bin or menuconfig (depend on
  *            whether to use phy init data in partition or not) will be used. Default
  *            value is level0. Values passed in power are mapped to transmit power
  *            levels as follows:
  *            - [78, 127]: level0
  *            - [76, 77]: level1
  *            - [74, 75]: level2
  *            - [68, 73]: level3
  *            - [60, 67]: level4
  *            - [52, 59]: level5
  *            - [44, 51]: level5 - 2dBm
  *            - [34, 43]: level5 - 4.5dBm
  *            - [28, 33]: level5 - 6dBm
  *            - [20, 27]: level5 - 8dBm
  *            - [8, 19]: level5 - 11dBm
  *            - [-128, 7]: level5 - 14dBm
  *
  * @param     power  Maximum WiFi transmitting power.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_NOT_START: WiFi is not started by wise_wifi_start
  */
wise_err_t wise_wifi_set_max_tx_power(int8_t power, uint8_t wlan_if);

/**
  * @brief     Get maximum WiFi transmitting power
  *
  * @attention This API gets maximum WiFi transmitting power. Values got
  *            from power are mapped to transmit power levels as follows:
  *            - 78: 19.5dBm
  *            - 76: 19dBm
  *            - 74: 18.5dBm
  *            - 68: 17dBm
  *            - 60: 15dBm
  *            - 52: 13dBm
  *            - 44: 11dBm
  *            - 34: 8.5dBm
  *            - 28: 7dBm
  *            - 20: 5dBm
  *            - 8:  2dBm
  *            - -4: -1dBm
  *
  * @param     power  Maximum WiFi transmitting power.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_NOT_START: WiFi is not started by wise_wifi_start
  *    - WISE_ERR_INVALID_ARG: invalid argument
  */
wise_err_t wise_wifi_get_max_tx_power(int8_t *power, uint8_t wlan_if);

/**
  * @brief     Send raw ieee80211 data
  *
  * @attention Currently only support for sending beacon/probe request/probe response/action
  *            and non-QoS data frame
  *
  * @param     ifx interface if the Wi-Fi mode is Station, the ifx should be WIFI_IF_STA.
  *            If the Wi-Fi mode is SoftAP, the ifx should be WIFI_IF_AP. If the Wi-Fi mode is
  *            Station+SoftAP, the ifx should be WIFI_IF_STA or WIFI_IF_AP.If the ifx is wrong,
  *            the API returns WISE_ERR_WIFI_IF.
  *
  * @param     buffer raw ieee80211 buffer
  *
  * @param     len the length of raw buffer, the len must be <= 1500 Bytes and >= 24 Bytes
  *
  * @param     en_sys_seq indicate whether use the internal sequence number.
  *            If en_sys_seq is false, the sequence in raw buffer is unchanged, otherwise
  *            it will be overwritten by WiFi driver with the system sequence number.
  *            Generally, if esp_wifi_80211_tx is called before the Wi-Fi connection has been set up,
  *            both en_sys_seq==true and en_sys_seq==false are fine.
  *            However, if the API is called after the Wi-Fi connection has been set up, en_sys_seq must be true,
  *            otherwise ESP_ERR_INVALID_ARG is returned.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_IF: invalid interface
  *    - WISE_ERR_INVALID_ARG: invalid argument
  *    - WISE_ERR_NO_MEM: out of memory
  */
wise_err_t wise_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);

/**
  * @brief     Set Beacon Interval of SoftAP
  *
  * @param     beacon_interval  interval of beacon.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: fail
  */
wise_err_t wise_wifi_set_beacon_interval(uint32_t beacon_interval);


/**
  * @brief     Set keepalive mode and interval of wise/watcher
  *
  * @param     priv is scm_wifi_keepalive_param.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: fail
  */
wise_err_t wise_wifi_set_keepalive(void* priv);


/**
  * @brief     Set bcn loss check en
  *
  * @param     enable is 1 to enable feature
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: fail
  */
wise_err_t wise_wifi_set_wc_bcn_loss_chk(uint8_t enable);

/**
  * @brief     Set port filter en
  *
  * @param     enable is 1 to enable feature
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: fail
  */
wise_err_t wise_wifi_set_wc_port_filter(uint8_t enable);

/**
  * @brief     Set tx power by different rate
  *
  * @param     priv is scm_wifi_tx_power_param
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: fail
  */
wise_err_t wise_wifi_set_tx_power(void* priv);

/**
  * @brief     Set tx power adjust reference ap proximate
  *
  * @param     0: proximate mode, 1 disable proximate
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: fail
  */
wise_err_t wise_wifi_set_power_mode(void* priv);

/**
  * @brief     Get tx power table
  *
  * @param     priv is scm_wifi_tx_power_param
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: fail
  */
wise_err_t wise_wifi_get_tx_power(void* priv);

/**
  * @brief     Set Dtim Period of SoftAP
  *
  * @param     dtim_period period of dtim.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: fail
  */
wise_err_t wise_wifi_set_dtim_period(uint8_t dtim_period);

/**
  * @brief     Set mask to enable or disable some WiFi events
  *
  * @attention 1. Mask can be created by logical OR of various WIFI_EVENT_MASK_ constants.
  *               Events which have corresponding bit set in the mask will not be delivered to the system event handler.
  * @attention 2. Default WiFi event mask is WIFI_EVENT_MASK_AP_PROBEREQRECVED.
  * @attention 3. There may be lots of stations sending probe request data around.
  *               Don't unmask this event unless you need to receive probe request data.
  *
  * @param     mask  WiFi event mask.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  */
wise_err_t wise_wifi_set_event_mask(uint32_t mask);

/**
  * @brief     Get mask of WiFi events
  *
  * @param     mask  WiFi event mask.
  *
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_NOT_INIT: WiFi is not initialized by wise_wifi_init
  *    - WISE_ERR_WIFI_ARG: invalid argument
  */
wise_err_t wise_wifi_get_event_mask(uint32_t *mask);

/**
  * @brief     Send user-define 802.11 packets.
  *
  * @attention 1. Packet has to be the whole 802.11 packet, does not include the FCS.
  *               The length of the packet has to be longer than the minimum length
  *               of the header of 802.11 packet which is 24 bytes, and less than 1400 bytes.
  * @attention 2. Duration area is invalid for user, it will be filled in SDK.
  * @attention 3. The rate of sending packet is same as the management packet which
  *               is the same as the system rate of sending packets.
  * @attention 4. Only after the previous packet was sent, entered the sent callback,
  *               the next packet is allowed to send. Otherwise, wifi_send_pkt_freedom
  *               will return fail.
  *
  * @param     uint8 *buf : pointer of packet
  * @param     uint16 len : packet length
  * @param     bool sys_seq : follow the system's 802.11 packets sequence number or not,
  *                           if it is true, the sequence number will be increased 1 every
  *                           time a packet sent.
  *
  * @return    WISE_OK, succeed;
  * @return    WISE_FAIL, fail.
  */
wise_err_t wise_wifi_send_pkt_freedom(uint8_t *buf, int32_t len, bool sys_seq);

/**
  * @brief     Get connection AP's psk
  *
  * @param    wlan_if : WiFi interface.
  * @param    get the *psk
  * @param    len : length of psk
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_MODE: WiFi mode is wrong
  *    - WISE_FAIL: get ssid fail or ssid->psk not available
  */
wise_err_t wise_wpa_get_psk (uint8_t wlan_if, uint8_t *psk, int len);


/**
  * @brief     Set connection AP's psk
  *
  * @param    wlan_if : WiFi event mask.
  * @param    input : *psk
  * @return
  *    - WISE_OK: succeed
  *    - WISE_ERR_WIFI_MODE: WiFi mode is wrong
  *    - WISE_FAIL: get ssid fail
  */
wise_err_t wise_wpa_set_psk (uint8_t wlan_if, uint8_t *psk);

/**
  * @brief     Set station powersave sleep time.
  *
  * @param    dtim_period : The unit is milliseconds.
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: set powersavesleep fail
  */
wise_err_t wise_wifi_set_powersavesleep(uint16_t dtim_period);

/**
  * @brief     Get network interface.
  *
  * @param    wlan_if : WiFi interface.
  * @param    netif : WiFi net network interface.
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: get WiFi interface fail
  */
wise_err_t wise_wifi_get_netif(uint8_t wlan_if, struct netif **netif);

/**
  * @brief     Set shared memory address.
  *
  * @param    addr : address of shared memory
  * @return
  *    - WISE_OK: succeed
  *    - WISE_FAIL: get WiFi interface fail
  */
wise_err_t wise_wifi_set_shared_mem_addr(uint32_t *addr);


#ifdef __cplusplus
}
#endif

#endif /* __WISE_WIFI_H__ */

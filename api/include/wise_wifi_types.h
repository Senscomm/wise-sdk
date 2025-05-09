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
 * Originated from esp_wifi_types.h of ESP8266_RTOS_SDK
 * (https://github.com/espressif/ESP8266_RTOS_SDK)
 * and modified to provide wise Wi-Fi API as being ESP8266 style
 */

#ifndef __WISE_WIFI_TYPES_H__
#define __WISE_WIFI_TYPES_H__

#include <stdint.h>
#include <stdbool.h>

#include "lwip/netif.h"
#include "lwip/netifapi.h"

#include "wise_err.h"
#include "wise_interface.h"
#ifdef CONFIG_SUPPORT_WIFI_REPEATER
#include "repeater/wifi_repeater.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define AUTH_ALG_NONE           0
#define AUTH_ALG_WEP            1
#define AUTH_ALG_TKIP           2
#define AUTH_ALG_CCMP           3
#define AUTH_ALG_PMK            4
#define AUTH_ALG_AES_CMAC       5
#define AUTH_ALG_SAE            6

typedef enum {
    WIFI_MODE_NULL = 0,  /**< null mode */
    WIFI_MODE_STA,       /**< Wi-Fi station mode */
    WIFI_MODE_AP,        /**< Wi-Fi soft-AP mode */
    WIFI_MODE_APSTA,     /**< Wi-Fi station + soft-AP mode */
    WIFI_MODE_MAX
} wifi_mode_t;


typedef enum {
    WIFI_KEEPALIVE_TYPE_WISE = 0,      /** idx of wise keepalive */
    WIFI_KEEPALIVE_TYPE_WATCHER,       /** idx of watcher keepalive */
    WIFI_KEEPALIVE_TYPE_MAX
} wifi_keepalive_type_t;

typedef enum {
    WIFI_KEEPALIVE_MODE_NONE = 0,      /** keepalive mode disable */
    WIFI_KEEPALIVE_MODE_NULL,          /**< Wi-Fi station mode */
    WIFI_KEEPALIVE_MODE_GARP,          /**< Wi-Fi soft-AP mode */
    WIFI_KEEPALIVE_MODE_MAX
} wifi_keepalive_mode_t;

#define WIFI_KEEPALIVE_INTVL_MAX	255

typedef wise_interface_t wifi_interface_t;

#define WIFI_IF_STA WISE_IF_WIFI_STA
#define WIFI_IF_AP  WISE_IF_WIFI_AP

typedef enum {
    WIFI_COUNTRY_POLICY_AUTO,   /**< Country policy is auto, use the country info of AP to which the station is connected */
    WIFI_COUNTRY_POLICY_MANUAL, /**< Country policy is manual, always use the configured country info */
} wifi_country_policy_t;

/** @brief Structure describing WiFi country-based regional restrictions. */
typedef struct {
    char                  cc[3];   /**< country code string */
    uint8_t               schan;   /**< start channel */
    uint8_t               nchan;   /**< total channel number */
    int8_t                max_tx_power;   /**< maximum tx power */
    wifi_country_policy_t policy;  /**< country policy */
} wifi_country_t;

typedef enum {
    WIFI_AUTH_OPEN = 0,         	/**< authenticate mode : open */
    WIFI_AUTH_WEP,              	/**< authenticate mode : WEP */
    WIFI_AUTH_WPA_PSK,          	/**< authenticate mode : WPA_PSK */
    WIFI_AUTH_WPA2_PSK,         	/**< authenticate mode : WPA2_PSK */
    WIFI_AUTH_WPA_ENTERPRISE,   	/**< authenticate mode : WPA_ENTERPRISE */
    WIFI_AUTH_WPA2_ENTERPRISE,  	/**< authenticate mode : WPA2_ENTERPRISE */
    WIFI_AUTH_WPA_WPA2_PSK,     	/**< authenticate mode : WPA_WPA2_PSK */
    WIFI_AUTH_WPA_WPA2_ENTERPRISE, 	/**< authenticate mode : WPA_WPA2_ENTERPRISE */
    WIFI_AUTH_WPA3_SAE,         	/**< authenticate mode : WPA3_SAE */
    WIFI_AUTH_WPA3_ENTERPRISE,  	/**< authenticate mode : WPA3_ENTERPRISE */
    WIFI_AUTH_MAX
} wifi_auth_mode_t;

typedef enum {
    WIFI_REASON_UNSPECIFIED              = 1,
    WIFI_REASON_AUTH_EXPIRE              = 2,
    WIFI_REASON_AUTH_LEAVE               = 3,
    WIFI_REASON_ASSOC_EXPIRE             = 4,
    WIFI_REASON_ASSOC_TOOMANY            = 5,
    WIFI_REASON_NOT_AUTHED               = 6,
    WIFI_REASON_NOT_ASSOCED              = 7,
    WIFI_REASON_ASSOC_LEAVE              = 8,
    WIFI_REASON_ASSOC_NOT_AUTHED         = 9,
    WIFI_REASON_DISASSOC_PWRCAP_BAD      = 10,
    WIFI_REASON_DISASSOC_SUPCHAN_BAD     = 11,
    WIFI_REASON_IE_INVALID               = 13,
    WIFI_REASON_MIC_FAILURE              = 14,
    WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT   = 15,
    WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT = 16,
    WIFI_REASON_IE_IN_4WAY_DIFFERS       = 17,
    WIFI_REASON_GROUP_CIPHER_INVALID     = 18,
    WIFI_REASON_PAIRWISE_CIPHER_INVALID  = 19,
    WIFI_REASON_AKMP_INVALID             = 20,
    WIFI_REASON_UNSUPP_RSN_IE_VERSION    = 21,
    WIFI_REASON_INVALID_RSN_IE_CAP       = 22,
    WIFI_REASON_802_1X_AUTH_FAILED       = 23,
    WIFI_REASON_CIPHER_SUITE_REJECTED    = 24,

    WIFI_REASON_BEACON_TIMEOUT           = 200,
    WIFI_REASON_NO_AP_FOUND              = 201,
    WIFI_REASON_AUTH_FAIL                = 202,
    WIFI_REASON_ASSOC_FAIL               = 203,
    WIFI_REASON_HANDSHAKE_TIMEOUT        = 204,
} wifi_err_reason_t;

typedef enum {
    WIFI_SECOND_CHAN_NONE = 0,  /**< the channel width is HT20 */
    WIFI_SECOND_CHAN_ABOVE,     /**< the channel width is HT40 and the second channel is above the primary channel */
    WIFI_SECOND_CHAN_BELOW,     /**< the channel width is HT40 and the second channel is below the primary channel */
} wifi_second_chan_t;

typedef enum {
    WIFI_SCAN_TYPE_ACTIVE = 0,  /**< active scan */
    WIFI_SCAN_TYPE_PASSIVE,     /**< passive scan */
} wifi_scan_type_t;

/** @brief Range of active scan times per channel */
typedef struct {
    uint32_t min;  /**< minimum active scan time per channel, units: millisecond */
    uint32_t max;  /**< maximum active scan time per channel, units: millisecond, values above 1500ms may
                                          cause station to disconnect from AP and are not recommended.  */
} wifi_active_scan_time_t;

/** @brief Aggregate of active & passive scan time per channel */
typedef union {
    wifi_active_scan_time_t active;  /**< active scan time per channel, units: millisecond. */
    uint32_t passive;                /**< passive scan time per channel, units: millisecond, values above 1500ms may
                                          cause station to disconnect from AP and are not recommended. */
} wifi_scan_time_t;

/** @brief Parameters for an SSID scan. */
typedef struct {
    uint8_t* ssid;               /**< SSID of AP */
    uint8_t* bssid;              /**< MAC address of AP */
    uint8_t channel;             /**< channel, scan the specific channel */
    bool show_hidden;            /**< enable to scan AP whose SSID is hidden */
    bool spec_ssid;              /**< enable to scan Specified SSID */
    wifi_scan_type_t scan_type;  /**< scan type, active or passive */
    wifi_scan_time_t scan_time;  /**< scan time per channel */
} wifi_scan_config_t;

typedef enum {
    WIFI_CIPHER_TYPE_NONE = 0,   /**< the cipher type is none */
    WIFI_CIPHER_TYPE_WEP40,      /**< the cipher type is WEP40 */
    WIFI_CIPHER_TYPE_WEP104,     /**< the cipher type is WEP104 */
    WIFI_CIPHER_TYPE_TKIP,       /**< the cipher type is TKIP */
    WIFI_CIPHER_TYPE_CCMP,       /**< the cipher type is CCMP */
    WIFI_CIPHER_TYPE_TKIP_CCMP,  /**< the cipher type is TKIP and CCMP */
    WIFI_CIPHER_TYPE_UNKNOWN,    /**< the cipher type is unknown */
} wifi_cipher_type_t;

typedef enum {
    WIFI_ANT_ANT0,          /**< WiFi antenna 0 */
    WIFI_ANT_ANT1,          /**< WiFi antenna 1 */
    WIFI_ANT_MAX,           /**< Invalid WiFi antenna */
} wifi_ant_t;

/** @brief Description of a WiFi AP */
typedef struct {
    uint8_t bssid[6];                     /**< MAC address of AP */
    uint8_t ssid[33];                     /**< SSID of AP */
    uint8_t primary;                      /**< channel of AP */
    wifi_second_chan_t second;            /**< secondary channel of AP */
    int8_t  rssi;                         /**< signal strength of AP */
    wifi_auth_mode_t authmode;            /**< authmode of AP */
    wifi_cipher_type_t pairwise_cipher;   /**< pairwise cipher of AP */
    wifi_cipher_type_t group_cipher;      /**< group cipher of AP */
    wifi_ant_t ant;                       /**< antenna used to receive beacon from AP */
    uint32_t phy_11b: 1;                  /**< bit: 0 flag to identify if 11b mode is enabled or not */
    uint32_t phy_11g: 1;                  /**< bit: 1 flag to identify if 11g mode is enabled or not */
    uint32_t phy_11n: 1;                  /**< bit: 2 flag to identify if 11n mode is enabled or not */
    uint32_t phy_11ax: 1;                 /**< bit: 3 flag to identify if 11ax mode is enabled or not */
    uint32_t wps: 1;                      /**< bit: 4 flag to identify if WPS is supported or not */
    uint32_t reserved: 27;                /**< bit: 5..31 reserved */
    wifi_country_t country;               /**< country information of AP */
} wifi_ap_record_t;

typedef enum {
    WIFI_FAST_SCAN = 0,                   /**< Do fast scan, scan will end after find SSID match AP */
    WIFI_ALL_CHANNEL_SCAN,                /**< All channel scan, scan will end after scan all the channel */
} wifi_scan_method_t;

typedef enum {
    WIFI_CONNECT_AP_BY_SIGNAL = 0,        /**< Sort match AP in scan list by RSSI */
    WIFI_CONNECT_AP_BY_SECURITY,          /**< Sort match AP in scan list by security mode */
} wifi_sort_method_t;

/** @brief Structure describing parameters for a WiFi fast scan */
typedef struct {
    int8_t              rssi;             /**< The minimum rssi to accept in the fast scan mode */
    wifi_auth_mode_t    authmode;         /**< The weakest authmode to accept in the fast scan mode */
} wifi_fast_scan_threshold_t;

typedef enum
{
  WIFI_PROTO_DISABLED = 0,
  WIFI_PROTO_WPA,
  WIFI_PROTO_WPA2
} wifi_proto_mode_t;

typedef enum {
    WIFI_PS_NONE,        /**< No power save */
    WIFI_PS_MAX_MODEM,   /**< Maximum modem power saving. In this mode, station close cpu and RF in DTIM period */
    WIFI_PS_MIN_MODEM,   /**< Minimum modem power saving. In this mode, station close RF in DTIM period */
    WIFI_PS_TYPE_MAX,
} wifi_ps_type_t;

#define WIFI_PS_MODEM WIFI_PS_MIN_MODEM /**< @deprecated Use WIFI_PS_MIN_MODEM or WIFI_PS_MAX_MODEM instead */

#define WIFI_PROTOCOL_11B         1
#define WIFI_PROTOCOL_11G         2
#define WIFI_PROTOCOL_11N         4
#define WIFI_PROTOCOL_LR          8	/** NOT SUPPORTED */

#define SSID_MAX_LEN 32
#define PASSWORD_MAX_LEN 64

typedef enum {
    WIFI_BW_HT20 = 1, /* Bandwidth is HT20 */
    WIFI_BW_HT40,     /* Bandwidth is HT40 */
} wifi_bandwidth_t;

enum
{
  WIFI_PMF_DISABLE,
  WIFI_PMF_CAPABLE,
  WIFI_PMF_REQUIRED,
  WIFI_PMF_INVLAD
};


/** @brief Soft-AP configuration settings */
typedef struct {
    uint8_t ssid[SSID_MAX_LEN];           /**< SSID of soft-AP */
    uint8_t password[PASSWORD_MAX_LEN];       /**< Password of soft-AP */
    uint8_t ssid_len;           /**< Length of SSID. If softap_config.ssid_len==0, check the SSID until there is a termination character; otherwise, set the SSID length according to softap_config.ssid_len. */
    uint8_t channel;            /**< Channel of soft-AP */
    uint16_t alg;               /**< Auth mode of soft-AP. Do not support AUTH_WEP and AES in soft-AP mode */
    wifi_proto_mode_t proto;    /**< proto set wpa/wpa2  */
    uint8_t ssid_hidden;        /**< Broadcast SSID or not, default 0, broadcast the SSID */
    uint8_t max_connection;     /**< Max number of stations allowed to connect in, default 4, max 4 */
    uint16_t beacon_interval;   /**< Beacon interval, 100 ~ 60000 ms, default 100 ms */
} wifi_ap_config_t;

/** @brief STA configuration settings */
typedef struct {
    uint8_t ssid[SSID_MAX_LEN];      /**< SSID of target AP*/
    uint8_t password[PASSWORD_MAX_LEN];  /**< password of target AP*/
    uint16_t alg;          /**<IW_ENCODE_ALG 0. OPEN 1. WEP 2. TKIP 3. CCMP 6. SAE CCMP256*/
	wifi_proto_mode_t proto;                /**< proto wpa/wpa2 setting of sta */
	uint32_t pmf_mode;                       /**< pmf mode : 0: disable, 1:capable, 2: required */
    wifi_scan_method_t scan_method;    /**< do all channel scan or fast scan */
    bool bssid_set;        /**< whether set MAC address of target AP or not. Generally, station_config.bssid_set needs to be 0; and it needs to be 1 only when users need to check the MAC address of the AP.*/
    uint8_t bssid[6];     /**< MAC address of target AP*/
    uint8_t channel;       /**< channel of target AP. Set to 1~13 to scan starting from the specified channel before connecting to AP. If the channel of AP is unknown, set it to 0.*/
    uint16_t listen_interval;   /**< Listen interval for the station to receive beacon when WIFI_PS_MAX_MODEM is set. Units: AP beacon intervals. Defaults to 3 if set to 0. */
    wifi_sort_method_t sort_method;    /**< sort the connect AP in the list by rssi or security mode */
    wifi_fast_scan_threshold_t  threshold;     /**< When scan_method is set to WIFI_FAST_SCAN, only APs which have an auth mode that is more secure than the selected auth mode and a signal stronger than the minimum RSSI will be used. */
    uint8_t scan_ssid;            /**<  1: scna hidden ssid, 0: normal scan */
    uint32_t rm_enabled:1;        /**< Whether Radio Measurements are enabled for the connection */
    uint32_t btm_enabled:1;       /**< Whether BSS Transition Management is enabled for the connection */
    uint32_t reserved:30;         /**< Reserved for future feature set */
} wifi_sta_config_t;

/** @brief Configuration data for AP or STA.
 *
 * The usage of this union (for ap or sta configuration) is determined by the accompanying
 * interface argument passed to esp_wifi_set_config() or esp_wifi_get_config()
 *
 */
typedef union {
    wifi_ap_config_t  ap;  /**< configuration of AP */
    wifi_sta_config_t sta; /**< configuration of STA */
} wifi_config_t;

#ifdef CONFIG_LWIP_IPV4
/** @brief IP configuration  */
typedef struct {
    ip4_addr_t ip;
    ip4_addr_t nm;
    ip4_addr_t gw;
} wifi_ip_info_t;
#endif

/** @brief Description of STA associated with AP */
typedef struct {
    uint8_t mac[6];  /**< mac address */
    uint32_t phy_11b: 1;     /**< bit: 0 flag to identify if 11b mode is enabled or not */
    uint32_t phy_11g: 1;     /**< bit: 1 flag to identify if 11g mode is enabled or not */
    uint32_t phy_11n: 1;     /**< bit: 2 flag to identify if 11n mode is enabled or not */
    uint32_t phy_lr: 1;      /**< bit: 3 flag to identify if low rate is enabled or not */
    uint32_t reserved: 28;   /**< bit: 4..31 reserved */
} wifi_sta_info_t;

#define WISE_WIFI_MAX_CONN_NUM  (10)       /**< max number of stations which can connect to soft-AP */

/** @brief List of stations associated with the Soft-AP */
typedef struct {
    wifi_sta_info_t sta[WISE_WIFI_MAX_CONN_NUM]; /**< station list */
    int       num; /**< number of stations in the list (other entries are invalid) */
} wifi_sta_list_t;

typedef enum {
    WIFI_STORAGE_FLASH,  /**< all configuration will store in both memory and flash */
    WIFI_STORAGE_RAM,    /**< all configuration will only store in the memory */
} wifi_storage_t;

/**
  * @brief     Vendor Information Element type
  *
  * Determines the frame type that the IE will be associated with.
  */
typedef enum {
    WIFI_VND_IE_TYPE_BEACON,
    WIFI_VND_IE_TYPE_PROBE_REQ,
    WIFI_VND_IE_TYPE_PROBE_RESP,
    WIFI_VND_IE_TYPE_ASSOC_REQ,
    WIFI_VND_IE_TYPE_ASSOC_RESP,
} wifi_vendor_ie_type_t;

/**
  * @brief     Vendor Information Element index
  *
  * Each IE type can have up to two associated vendor ID elements.
  */
typedef enum {
    WIFI_VND_IE_ID_0,
    WIFI_VND_IE_ID_1,
} wifi_vendor_ie_id_t;

#define WIFI_VENDOR_IE_ELEMENT_ID 0xDD

/**
 * @brief Vendor Information Element header
 *
 * The first bytes of the Information Element will match this header. Payload follows.
 */
typedef struct {
    uint8_t element_id;      /**< Should be set to WIFI_VENDOR_IE_ELEMENT_ID (0xDD) */
    uint8_t length;          /**< Length of all bytes in the element data following this field. Minimum 4. */
    uint8_t vendor_oui[3];   /**< Vendor identifier (OUI). */
    uint8_t vendor_oui_type; /**< Vendor-specific OUI type. */
    uint8_t payload[0];      /**< Payload. Length is equal to value in 'length' field, minus 4. */
} vendor_ie_data_t;

/** @brief Received packet radio metadata header, this is the common header at the beginning of all promiscuous mode RX callback buffers */
typedef struct {
    signed rssi: 8;           /**< signal intensity of packet */
    unsigned rate: 4;         /**< data rate */
    unsigned is_group: 1;
    unsigned : 1;             /**< reserve */
    unsigned sig_mode: 2;     /**< 0:is not 11n packet; 1:is 11n packet */
    unsigned legacy_length: 12;
    unsigned damatch0: 1;
    unsigned damatch1: 1;
    unsigned bssidmatch0: 1;
    unsigned bssidmatch1: 1;
    unsigned mcs: 7;          /**< if is 11n packet, shows the modulation(range from 0 to 76) */
    unsigned cwb: 1;          /**< if is 11n packet, shows if is HT40 packet or not */
    unsigned HT_length: 16;            /**< reserve */
    unsigned smoothing: 1;    /**< reserve */
    unsigned not_sounding: 1; /**< reserve */
    unsigned : 1;             /**< reserve */
    unsigned aggregation: 1;  /**< Aggregation */
    unsigned stbc: 2;         /**< STBC */
    unsigned fec_coding: 1;   /**< Flag is set for 11n packets which are LDPC */
    unsigned sgi: 1;          /**< SGI */
    unsigned rxend_state: 8;
    unsigned ampdu_cnt: 8;    /**< ampdu cnt */
    unsigned channel: 4;      /**< which channel this packet in */
    unsigned : 4;             /**< reserve */
    signed noise_floor: 8;
} wifi_pkt_rx_ctrl_t;

/** @brief Payload passed to 'buf' parameter of promiscuous mode RX callback.
 */
typedef struct {
    wifi_pkt_rx_ctrl_t rx_ctrl; /**< metadata header */
    uint8_t payload[0];       /**< Data or management payload. Length of payload is described by rx_ctrl.sig_len. Type of content determined by packet type argument of callback. */
} wifi_promiscuous_pkt_t;

/**
  * @brief Promiscuous frame type
  *
  * Passed to promiscuous mode RX callback to indicate the type of parameter in the buffer.
  *
  */
typedef enum {
    WIFI_PKT_MGMT,  /**< Management frame, indicates 'buf' argument is wifi_promiscuous_pkt_t */
    WIFI_PKT_CTRL,  /**< Control frame, indicates 'buf' argument is wifi_promiscuous_pkt_t */
    WIFI_PKT_DATA,  /**< Data frame, indicates 'buf' argument is wifi_promiscuous_pkt_t */
    WIFI_PKT_MISC,  /**< Other type, such as MIMO etc. 'buf' argument is wifi_promiscuous_pkt_t but the payload is zero length. */
} wifi_promiscuous_pkt_type_t;


#define WIFI_PROMIS_FILTER_MASK_ALL         (0xFFFFFFFF)  /**< filter all packets */
#define WIFI_PROMIS_FILTER_MASK_MGMT        (1)           /**< filter the packets with type of WIFI_PKT_MGMT */
#define WIFI_PROMIS_FILTER_MASK_CTRL        (1<<1)        /**< filter the packets with type of WIFI_PKT_CTRL */
#define WIFI_PROMIS_FILTER_MASK_DATA        (1<<2)        /**< filter the packets with type of WIFI_PKT_DATA */
#define WIFI_PROMIS_FILTER_MASK_MISC        (1<<3)        /**< filter the packets with type of WIFI_PKT_MISC */

#define WIFI_PROMIS_CTRL_FILTER_MASK_ALL         (0xFF800000)  /**< filter all control packets */
#define WIFI_PROMIS_CTRL_FILTER_MASK_WRAPPER     (1<<23)       /**< filter the control packets with subtype of Control Wrapper */
#define WIFI_PROMIS_CTRL_FILTER_MASK_BAR         (1<<24)       /**< filter the control packets with subtype of Block Ack Request */
#define WIFI_PROMIS_CTRL_FILTER_MASK_BA          (1<<25)       /**< filter the control packets with subtype of Block Ack */
#define WIFI_PROMIS_CTRL_FILTER_MASK_PSPOLL      (1<<26)       /**< filter the control packets with subtype of PS-Poll */
#define WIFI_PROMIS_CTRL_FILTER_MASK_RTS         (1<<27)       /**< filter the control packets with subtype of RTS */
#define WIFI_PROMIS_CTRL_FILTER_MASK_CTS         (1<<28)       /**< filter the control packets with subtype of CTS */
#define WIFI_PROMIS_CTRL_FILTER_MASK_ACK         (1<<29)       /**< filter the control packets with subtype of ACK */
#define WIFI_PROMIS_CTRL_FILTER_MASK_CFEND       (1<<30)       /**< filter the control packets with subtype of CF-END */
#define WIFI_PROMIS_CTRL_FILTER_MASK_CFENDACK    (1<<31)       /**< filter the control packets with subtype of CF-END+CF-ACK */

/** @brief Mask for filtering different packet types in promiscuous mode. */
typedef struct {
    uint32_t filter_mask; /**< OR of one or more filter values WIFI_PROMIS_FILTER_* */
} wifi_promiscuous_filter_t;

#define WIFI_EVENT_MASK_ALL                 (0xFFFFFFFF)  /**< mask all WiFi events */
#define WIFI_EVENT_MASK_NONE                (0)           /**< mask none of the WiFi events */
#define WIFI_EVENT_MASK_AP_PROBEREQRECVED   (BIT(0))      /**< mask SYSTEM_EVENT_AP_PROBEREQRECVED event */

typedef enum {
    TX_STATUS_SUCCESS = 1,
    TX_STATUS_SRC_EXCEED,
    TX_STATUS_LRC_EXCEED,
    TX_STATUS_DISCARD,
} wifi_tx_result_t;

typedef enum {
    PHY_RATE_1_LONG,
    PHY_RATE_2_LONG,
    PHY_RATE_5_LONG,
    PHY_RATE_11_LONG,
    PHY_RATE_RESERVED,
    PHY_RATE_2_SHORT,
    PHY_RATE_5_SHORT,
    PHY_RATE_11_SHORT,
    PHY_RATE_48,
    PHY_RATE_24,
    PHY_RATE_12,
    PHY_RATE_6,
    PHY_RATE_54,
    PHY_RATE_36,
    PHY_RATE_18,
    PHY_RATE_9,
} wifi_tx_rate_t;

typedef struct {
    unsigned wifi_tx_result: 8;
    unsigned wifi_tx_src: 6;
    unsigned wifi_tx_lrc: 6;
    unsigned wifi_tx_rate: 8;
    unsigned unused: 4;
} wifi_tx_status_t;

typedef enum {
	PHY_MODE_NONHT = 0,
	PHY_MODE_HTMF = 1,
	PHY_MODE_HESU = 4,
} wifi_phy_fmt_t;

typedef enum {
	RATE_1 = 2,
	RATE_2 = 4,
	RATE_5_5 = 11,
	RATE_11 = 22,
	RATE_6 = 12,
	RATE_9 = 18,
	RATE_12 = 24,
	RATE_18 = 36,
	RATE_24 = 48,
	RATE_36 = 72,
	RATE_48 = 96,
	RATE_54 = 108,
} wifi_nonht_rate_t;

typedef enum {
	HT_MCS0,
	HT_MCS1,
	HT_MCS2,
	HT_MCS3,
	HT_MCS4,
	HT_MCS5,
	HT_MCS6,
	HT_MCS7,
} wifi_ht_rate_t;

typedef enum {
	HE_MCS0,
	HE_MCS1,
	HE_MCS2,
	HE_MCS3,
	HE_MCS4,
	HE_MCS5,
	HE_MCS6,
	HE_MCS7,
	HE_MCS8,
	HE_MCS9,
} wifi_he_rate_t;


#ifdef __cplusplus
}
#endif

#endif /* __WISE_WIFI_TYPES_H__ */

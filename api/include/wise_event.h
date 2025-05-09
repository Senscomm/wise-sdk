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
 * Originated from esp_event.h of ESP8266_RTOS_SDK
 * (https://github.com/espressif/ESP8266_RTOS_SDK)
 * and modified to provide wise Wi-Fi API as being ESP8266 style
 */

#ifndef __WISE_EVENT_H__
#define __WISE_EVENT_H__

#include <stdint.h>
#include <stdbool.h>

#include "wise_err.h"
#include "wise_wifi_types.h"
#include "lwip/ip_addr.h"
#include "lwip/opt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WISE_EVENT_IPV6 LWIP_IPV6

typedef enum {
    SYSTEM_EVENT_WIFI_READY = 0,           /**< WiFi ready */
    SYSTEM_EVENT_SCAN_DONE,                /**< finish scanning AP */
    SYSTEM_EVENT_STA_START,                /**< station start */
    SYSTEM_EVENT_STA_STOP,                 /**< station stop */
    SYSTEM_EVENT_STA_CONNECTED,            /**< station connected to AP */
    SYSTEM_EVENT_STA_DISCONNECTED,         /**< station disconnected from AP */
    SYSTEM_EVENT_STA_AUTHMODE_CHANGE,      /**< the auth mode of AP connected by station changed */
    SYSTEM_EVENT_STA_GOT_IP,               /**< station got IP from connected AP */
    SYSTEM_EVENT_STA_LOST_IP,              /**< station lost IP and the IP is reset to 0 */
    SYSTEM_EVENT_STA_WPS_ER_SUCCESS,       /**< station wps succeeds in enrollee mode */
    SYSTEM_EVENT_STA_WPS_ER_FAILED,        /**< station wps fails in enrollee mode */
    SYSTEM_EVENT_STA_WPS_ER_TIMEOUT,       /**< station wps timeout in enrollee mode */
    SYSTEM_EVENT_STA_WPS_ER_PIN,           /**< station wps pin code in enrollee mode */
    SYSTEM_EVENT_STA_STATE_CHANGE,         /**< station status changes */
    SYSTEM_EVENT_STA_NO_NETWORK,           /**< station no suitable network found after scan */
    SYSTEM_EVENT_AP_START,                 /**< soft-AP start */
    SYSTEM_EVENT_AP_STOP,                  /**< soft-AP stop */
    SYSTEM_EVENT_AP_STACONNECTED,          /**< a station connected to soft-AP */
    SYSTEM_EVENT_AP_STADISCONNECTED,       /**< a station disconnected from soft-AP */
    SYSTEM_EVENT_AP_STAIPASSIGNED,         /**< soft-AP assign an IP to a connected station */
    SYSTEM_EVENT_AP_PROBEREQRECVED,        /**< Receive probe request packet in soft-AP interface */
    SYSTEM_EVENT_GOT_IP6,                  /**< station or ap or ethernet interface v6IP addr is preferred */
    SYSTEM_EVENT_ETH_START,                /**< ethernet start */
    SYSTEM_EVENT_ETH_STOP,                 /**< ethernet stop */
    SYSTEM_EVENT_ETH_CONNECTED,            /**< ethernet phy link up */
    SYSTEM_EVENT_ETH_DISCONNECTED,         /**< ethernet phy link down */
    SYSTEM_EVENT_ETH_GOT_IP,               /**< ethernet got IP from connected AP */
    SYSTEM_EVENT_SCM_CHANNEL,              /**< senscomm channel: send to host */
    SYSTEM_EVENT_SCM_LINK_UP,              /**< Link state up: send to host */
    SYSTEM_EVENT_REKEY,                    /**< rekey event */
    SYSTEM_EVENT_MAX_RETRY,                /**< max retry event */
    SYSTEM_EVENT_MAX
} system_event_id_t;

/* add this macro define for compatible with old IDF version */
#ifndef SYSTEM_EVENT_AP_STA_GOT_IP6
#define SYSTEM_EVENT_AP_STA_GOT_IP6 SYSTEM_EVENT_GOT_IP6
#endif

typedef enum {
    WPS_FAIL_REASON_NORMAL = 0,            /**< WPS normal fail reason */
    WPS_FAIL_REASON_RECV_M2D,              /**< WPS receive M2D frame */
    WPS_FAIL_REASON_MAX
}system_event_sta_wps_fail_reason_t;

typedef struct {
    uint32_t status;          /**< status of scanning APs */
    uint8_t  number;
    uint8_t  scan_id;
} system_event_sta_scan_done_t;

typedef struct {
    uint8_t ssid[32];         /**< SSID of connected AP */
    uint8_t ssid_len;         /**< SSID length of connected AP */
    uint8_t bssid[6];         /**< BSSID of connected AP*/
    uint8_t channel;          /**< channel of connected AP*/
    wifi_auth_mode_t authmode;
    bool not_en_dhcp;
} system_event_sta_connected_t;

typedef struct {
    uint8_t ssid[32];         /**< SSID of disconnected AP */
    uint8_t ssid_len;         /**< SSID length of disconnected AP */
    uint8_t bssid[6];         /**< BSSID of disconnected AP */
    uint8_t reason;           /**< reason of disconnection */
} system_event_sta_disconnected_t;

typedef struct {
    wifi_auth_mode_t old_mode;         /**< the old auth mode of AP */
    wifi_auth_mode_t new_mode;         /**< the new auth mode of AP */
} system_event_sta_authmode_change_t;

typedef struct {
    const char *str_old_state;
    const char *str_new_state;
} system_event_sta_state_change_t;

typedef struct {
    ip4_addr_t ip;
    ip4_addr_t netmask;
    ip4_addr_t gw;
} ip_info_t;

#ifdef CONFIG_LWIP_IPV6
typedef struct {
    ip6_addr_t ip;
} ip6_info_t;
#else
typedef struct {
    struct {
        uint32_t addr[4];
    } ip;
} ip6_info_t;
#endif

typedef struct {
    ip_info_t ip_info;
    bool ip_changed;
} system_event_sta_got_ip_t;

typedef struct {
    uint8_t pin_code[8];         /**< PIN code of station in enrollee mode */
} system_event_sta_wps_er_pin_t;

typedef struct {
    uint16_t if_index;
    ip6_info_t ip6_info;
} system_event_got_ip6_t;

typedef struct {
    uint8_t mac[6];           /**< MAC address of the station connected to soft-AP */
    uint8_t aid;              /**< the aid that soft-AP gives to the station connected to  */
} system_event_ap_staconnected_t;

typedef struct {
    uint8_t mac[6];           /**< MAC address of the station disconnects to soft-AP */
    uint8_t aid;              /**< the aid that soft-AP gave to the station disconnects to  */
} system_event_ap_stadisconnected_t;

typedef struct {
    int rssi;                 /**< Received probe request signal strength */
    uint8_t mac[6];           /**< MAC address of the station which send probe request */
} system_event_ap_probe_req_rx_t;

typedef struct {
    uint8_t *buf;
    uint32_t buf_len;
} system_event_scm_channel_t;

typedef union {
    system_event_sta_connected_t               connected;          /**< station connected to AP */
    system_event_sta_disconnected_t            disconnected;       /**< station disconnected to AP */
    system_event_sta_scan_done_t               scan_done;          /**< station scan (APs) done */
    system_event_sta_authmode_change_t         auth_change;        /**< the auth mode of AP station connected to changed */
    system_event_sta_got_ip_t                  got_ip;             /**< station got IP, first time got IP or when IP is changed */
    system_event_sta_wps_er_pin_t              sta_er_pin;         /**< station WPS enrollee mode PIN code received */
    system_event_sta_wps_fail_reason_t         sta_er_fail_reason; /**< station WPS enrollee mode failed reason code received */
    system_event_sta_state_change_t            sta_state_change;   /**< station status changes */
    system_event_ap_staconnected_t             sta_connected;      /**< a station connected to soft-AP */
    system_event_ap_stadisconnected_t          sta_disconnected;   /**< a station disconnected to soft-AP */
    system_event_ap_probe_req_rx_t             ap_probereqrecved;  /**< soft-AP receive probe request packet */
    system_event_got_ip6_t                     got_ip6;            /**< station or ap or ethernet ipv6 addr state change to preferred */
    system_event_scm_channel_t                 scm_channel_msg;    /**< senscomm channel for private message handshake */
} system_event_info_t;

typedef struct {
    char ifname[8];                      /**< event interface */
    system_event_id_t     event_id;      /**< event ID */
    system_event_info_t   event_info;    /**< event information */
} system_event_t;

typedef wise_err_t (*system_event_handler_t)(system_event_t *event);

/**
  * @brief  Send a event to event task
  *
  * @attention 1. Other task/modules, such as the TCPIP module, can call this API to send an event to event task
  *
  * @param  system_event_t * event : event
  *
  * @return WISE_OK : succeed
  * @return others : fail
  */
wise_err_t wise_event_send(system_event_t *event);

/**
  * @brief  Default event handler for system events
  *
  * This function performs default handling of system events.
  * When using wise_event_loop APIs, it is called automatically before invoking the user-provided
  * callback function.
  *
  * Applications which implement a custom event loop must call this function
  * as part of event processing.
  *
  * @param  event pointer to event to be handled
  * @return WISE_OK if an event was handled successfully
  */
wise_err_t wise_event_process_default(system_event_t *event);

/**
  * @brief  Install default event handlers for Wi-Fi interfaces (station and AP)
  *
  */
void wise_event_set_default_wifi_handlers();

#ifdef __cplusplus
}
#endif

#endif /* __ESP_EVENT_H__ */

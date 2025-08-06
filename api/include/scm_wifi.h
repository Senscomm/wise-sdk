/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __WIFI_API_H__
#define __WIFI_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#define WPA_FLAG_ON     1
#define WPA_FLAG_OFF    0

#define WPA_MIN_KEY_LEN                 8
#define WPA_MAX_KEY_LEN                 64

#define WPA_MAX_SSID_LEN                32
#define WPA_MAX_ESSID_LEN               WPA_MAX_SSID_LEN

#define WPA_AP_MIN_BEACON               25
#define WPA_AP_MAX_BEACON               1000

#define WPA_AP_MAX_DTIM                 15
#define WPA_AP_MIN_DTIM                 1

#define WPA_MAX_NUM_STA                 CONFIG_SUPPORT_STA_NUM /* only support 1 sta */

#define WPA_24G_CHANNEL_NUMS            14

#define WPA_COUNTRY_CODE_LEN            3
#define WPA_COUNTRY_CODE_USA            "US"
#define WPA_COUNTRY_CODE_JAPAN          "JP"
#define WPA_CHANNEL_MAX_USA             11
#define WPA_CHANNEL_MAX_JAPAN           14
#define WPA_CHANNEL_MAX_OTHERS          13

/**
 * @ingroup scm_wifi_basic
 *
 * default max num of station.
 */
#define WIFI_DEFAULT_MAX_NUM_STA         CONFIG_SUPPORT_STA_NUM


/**
 * @ingroup hi_wifi_basic
 *
 * Length of wpa ssid psk
 */
#define WIFI_STA_PSK_LEN                 32

/**
 * @ingroup scm_wifi_basic
 *
 * max interiface name length.
 */
#define WIFI_IFNAME_MAX_SIZE             16

/**
 * @ingroup scm_wifi_basic
 *
 * The minimum timeout of a single reconnection.
 */
#define WIFI_MIN_RECONNECT_TIMEOUT   5

/**
 * @ingroup scm_wifi_basic
 *
 * The maximum timeout of a single reconnection,
 * representing an infinite number of loop reconnections.
 */
#define WIFI_MAX_RECONNECT_TIMEOUT   65535

/**
 * @ingroup scm_wifi_basic
 *
 * The minimum auto reconnect interval.
 */
#define WIFI_MIN_RECONNECT_PERIOD    1

/**
 * @ingroup scm_wifi_basic
 *
 * The maximum auto reconnect interval.
 */
#define WIFI_MAX_RECONNECT_PERIOD   65535

/**
 * @ingroup scm_wifi_basic
 *
 * The minimum times of auto reconnect.
 */
#define WIFI_MIN_RECONNECT_TIMES    1

/**
 * @ingroup scm_wifi_basic
 *
 * The maximum times of auto reconnect.
 */
#define WIFI_MAX_RECONNECT_TIMES   65535

/**
 * @ingroup scm_wifi_basic
 *
 * max scan number of ap.
 */
#define WIFI_SCAN_AP_LIMIT               32

/**
 * @ingroup scm_wifi_basic
 *
 * Max length of SSID.
 */
#define SCM_WIFI_MAX_SSID_LEN  32

/**
 * @ingroup scm_wifi_basic
 *
 * Length of MAC address.
 */
#define SCM_WIFI_MAC_LEN        6

/**
 * @ingroup scm_wifi_basic
 *
 * Minimum/Maximum length of Key.
 */
#define SCM_WIFI_PSK_MIN_LEN    8
#define SCM_WIFI_PSK_MAX_LEN    63

/**
 * @ingroup scm_wifi_basic
 *
 * String length of bssid, eg. 00:00:00:00:00:00.
 */
#define SCM_WIFI_ADDR_STR_LEN   17

/**
 * @ingroup scm_wifi_basic
 *
 * Return value of invalid channel.
 */
#define SCM_WIFI_INVALID_CHANNEL 0xFF


struct wifi_reconnect_set {
	int enable;
	unsigned int timeout;
	unsigned int period;
	unsigned int max_try_count;
	unsigned int try_count;
	unsigned int try_freq_scan_count;
	int pending_flag;
	struct wpa_ssid *current_ssid;
};

/**
 * @ingroup scm_wifi_basic
 *
 * Scan type enum.
 */
typedef enum {
    SCM_WIFI_BASIC_SCAN,             /* Common and all channel scan. */
    SCM_WIFI_CHANNEL_SCAN,           /* Specified channel scan. */
    SCM_WIFI_SSID_SCAN,              /* Specified SSID scan. */
    SCM_WIFI_SSID_PREFIX_SCAN,       /* Prefix SSID scan. */
    SCM_WIFI_BSSID_SCAN,             /* Specified BSSID scan. */
} scm_wifi_scan_type;

/**
 * @ingroup scm_wifi_basic
 *
 * Authentication type enum.
 */
typedef enum {
    SCM_WIFI_SECURITY_OPEN,                  /* OPEN. */
    SCM_WIFI_SECURITY_WPAPSK,                /* WPA-PSK. */
    SCM_WIFI_SECURITY_WPA2PSK,               /* WPA2-PSK. */
    SCM_WIFI_SECURITY_SAE,                   /* SAE. */
    SCM_WIFI_SECURITY_WPA2PSK256,            /* WPA2-PSK-256. */
    SCM_WIFI_SECURITY_UNKNOWN                /* UNKNOWN. */
} scm_wifi_auth_mode;

/**
 * @ingroup scm_wifi_basic
 *
 * Encryption type enum.
 *
 */
typedef enum {
    SCM_WIFI_PAIRWISE_NONE,                  /* UNKNOWN.  */
    SCM_WIFI_PAIRWISE_AES,                   /* AES. */
    SCM_WIFI_PAIRWISE_TKIP,                  /* TKIP. */
    SCM_WIFI_PAIRWISE_MAX,                   /* MAX. */
} scm_wifi_pairwise;

/**
 * @ingroup scm_wifi_basic
 *
 * options that be set
 *
 */
typedef enum {
    SCM_WIFI_STA_SET_KEEPALIVE,              /* STA set keepalive */
    SCM_WIFI_STA_SET_POWERSAVE,              /* STA set powersave */
    SCM_WIFI_STA_SET_COUNTRYCODE,            /* STA set country code */
    SCM_WIFI_STA_SET_RECONNECT,              /* STA set reconnect policy */

#ifdef CONFIG_API_WATCHER
    SCM_WIFI_WC_SET_KEEPALIVE,               /* WATCHER set keepalive */
    SCM_WIFI_WC_SET_BCN_LOSS_CHK,            /* WATCHER set bcn loss check */
    SCM_WIFI_WC_SET_PORT_FILTER,             /* WATCHER set port filter */
#endif
    SCM_WIFI_SET_TXPOWER,                    /* WiFi set tx power */
    SCM_WIFI_SET_RESET_TXPOWER,              /* WiFi reset tx power */
    SCM_WIFI_SET_TXPOWER_MODE,               /* WiFi proximate mode */
#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
    SCM_WIFI_SET_SHARED_MEM_ADDR,            /* Shared memory address for some info exchange */
#endif
    SCM_WIFI_OPT_SET_MAX,                    /* MAX */
} scm_wifi_set_option;

/**
 * @ingroup scm_wifi_basic
 *
 * options that be get
 *
 */
typedef enum {
    SCM_WIFI_STA_GET_CONNECT,                /* STA get connection info */
    SCM_WIFI_STA_GET_RSSI,                   /* STA get ap rssi */
    SCM_WIFI_STA_GET_PSK,                    /* STA get psk */
    SCM_WIFI_STA_GET_COUNTRYCODE,            /* STA get country code */
    SCM_WIFI_GET_TXPOWER,                    /* WiFi get tx power */
    SCM_WIFI_OPT_GET_MAX,                    /* MAX */
} scm_wifi_get_option;

/**
 * @ingroup scm_wifi_basic
 *
 * Struct of connect parameters.
 */
typedef struct {
    char ssid[SCM_WIFI_MAX_SSID_LEN + 1];    /* SSID. */
    unsigned int hidden_ap;                  /* Ap is hidden AP */
    scm_wifi_auth_mode auth;                 /* Authentication mode. */
    char key[SCM_WIFI_PSK_MAX_LEN + 1];      /* Secret key. */
    unsigned char bssid[SCM_WIFI_MAC_LEN];   /* BSSID. */
    scm_wifi_pairwise pairwise;              /* Encryption type. */
    uint8_t pmf;                             /* pmf mode : 0: disable, 1:capable, 2: required */
} scm_wifi_assoc_request;

/**
 * @ingroup scm_wifi_basic
 *
 * Struct of connect parameters.
 */
typedef struct {
    scm_wifi_assoc_request req;              /* Association request */
    unsigned char channel;                   /* AP Channel number  */
    unsigned char psk[WIFI_STA_PSK_LEN];     /* PSK. */
    unsigned int resv;
} scm_wifi_fast_assoc_request;

/**
 * @ingroup scm_wifi_basic
 *
 * parameters of scan.
 */
typedef struct {
    char ssid[SCM_WIFI_MAX_SSID_LEN + 1];    /* SSID. */
    unsigned char bssid[SCM_WIFI_MAC_LEN];   /* BSSID. */
    unsigned char ssid_len;                  /* SSID length. */
    unsigned char channel;                   /* Channel number. */
    scm_wifi_scan_type scan_type;            /* Scan type. */
} scm_wifi_scan_params;

/**
 * @ingroup scm_wifi_basic
 *
 * Type of connect's status.
 */
typedef enum {
    SCM_WIFI_DISCONNECTED,                   /* Disconnected. */
    SCM_WIFI_CONNECTED,                      /* Connected. */
} scm_wifi_conn_status;

/**
 * @ingroup scm_wifi_basic
 *
 * Type of powersave mode.
 */
typedef enum {
    SCM_WIFI_PS_OFF,                         /**< do not use power-saving modes */
    SCM_WIFI_PS_ON,                          /**< use power-saving modes */
    SCM_WIFI_PS_ON_LESS_BEACONS,             /**< reduce beacon listen interval */
} scm_wifi_ps_mode;

/**
 * @ingroup scm_wifi_basic
 *
 * Status of sta's connection.
 */
typedef struct {
    char ssid[SCM_WIFI_MAX_SSID_LEN + 1];    /* SSID. */
    unsigned char bssid[SCM_WIFI_MAC_LEN];   /* BSSID. */
    int channel;                             /* Channel number. */
    scm_wifi_conn_status status;             /* Connect status. */
} scm_wifi_status;

/**
 * @ingroup scm_wifi_basic
 *
 * Struct of scan result.
 */
typedef struct {
    char ssid[SCM_WIFI_MAX_SSID_LEN + 1];    /* SSID. */
    unsigned char bssid[SCM_WIFI_MAC_LEN];   /* BSSID. */
    unsigned int channel;                    /* Channel number. */
    scm_wifi_auth_mode auth;                 /* Authentication type. */
    int rssi;                                /* Signal Strength. */
    unsigned char resv1 : 1;                 /* Reserved. */
    unsigned char resv2 : 1;                 /* Reserved. */
    unsigned char resv3 : 1;                 /* Reserved. */
    unsigned char resv4 : 1;                 /* Reserved. */
    unsigned char resv5 : 1;                 /* Reserved. */
} scm_wifi_ap_info;

/**
 * @ingroup scm_wifi_basic
 *
 * Struct of softap's basic config.
 *
 */
typedef struct {
    char ssid[SCM_WIFI_MAX_SSID_LEN + 1];     /* SSID. */
    char key[SCM_WIFI_PSK_MAX_LEN + 1];       /* Secret key. */
    unsigned char channel_num;                /* Channel number. */
    int ssid_hidden;                          /* Hidden ssid. */
    scm_wifi_auth_mode authmode;              /* Authentication mode. */
    scm_wifi_pairwise pairwise;               /* Encryption type. */
    uint16_t interval;                        /* Beacon interval */
    uint8_t period;                           /* DTIM period */
} scm_wifi_softap_config;

/**
 * @ingroup scm_wifi_basic
 *
 * information of softap's user.
 *
 */
typedef struct {
    unsigned char   mac[SCM_WIFI_MAC_LEN];     /* MAC address. */
    int             rssi;                      /* rssi. */
    int             rate;                      /* data rate code. */
} scm_wifi_ap_sta_info;

/**
 * @ingroup scm_wifi_basic
 *
 * parameter of reconnect policy.
 *
 */
typedef struct {
    int enable;                                /* on/off */
    unsigned int timeout;                      /* timeout value */
    unsigned int period;                       /* retry period */
    unsigned int max_try_count;                /* max try count */
} scm_wifi_reconnect_param;



/**
 * @ingroup scm_wifi_basic
 *
 * parameter of country code.
 *
 */
typedef struct {
    char *country_code;                        /* country code */
    bool use_ieee80211d;                       /* forced mode */
} scm_wifi_country_param;

/**
 * @ingroup scm_wifi_basic
 *
 * parameter of powersave.
 *
 */
typedef struct {
    int mode;                                  /* mode */
    int interval;                              /* interval */
} scm_wifi_ps_param;

/**
 * @ingroup scm_wifi_basic
 *
 * parameter of keepalive.
 *
 */
typedef struct {
    uint8_t type;                                  /* wise/watcher */
    uint8_t mode;                                  /* off/null/garp */
    uint8_t interval;                              /* interval */
} scm_wifi_keepalive_param;

/**
 * @ingroup scm_wifi_basic
 *
 * parameter of tx power.
 *
 */
typedef struct {
    wifi_phy_fmt_t fmt;                            /* phy mode */
    uint8_t mcs_rate;                              /* rate */
    uint8_t power;                                 /* power value */
    uint8_t reset;                                 /* reset */
} scm_wifi_tx_power_param;

#define SCM_MAX_RATE_TABLE 12

struct scm_tx_pwr_table_entry
{
	int rate;
	uint8_t tx_pwr_index; /* dBm */
	uint8_t tx_pwr_index_max; /* dBm */
};

typedef struct {
    uint32_t fmt;                                                        /* phy mode */
    struct scm_tx_pwr_table_entry pwr_table[SCM_MAX_RATE_TABLE];         /* power table */
    int size;                                                            /* size of power table */
} scm_wifi_tx_pwr_table;


/**
 * @ingroup scm_wifi_basic
 *
 * Secondary channel location for HT40.
 */
typedef enum {
    SCM_WIFI_2ND_CH_NONE = 0,               /* HT20. */
    SCM_WIFI_2ND_CH_ABOVE,                  /* HT40+ */
    SCM_WIFI_2ND_CH_BELOW,                  /* HT40- */
} scm_wifi_2nd_ch_loc;

int scm_wifi_register_event_callback(system_event_cb_t event_cb, void *priv);

int scm_wifi_system_event_send(system_event_t *evt);

int scm_wifi_unregister_event(void);

void scm_wifi_event_send(void *event, size_t size);

void scm_wifi_get_wlan_mac(uint8_t **mac_addr, int idx);

struct netif * scm_wifi_get_netif(int idx);

int scm_wifi_get_ip(const char *ifname, char *ip, int ip_len,
                    char *nm, int nm_len, char *gw, int gw_len);

int scm_wifi_set_ip(const char *ifname, const char *ip,
                    const char *nm, const char *gw);

int scm_wifi_reset_ip(const char *ifname);

int scm_wifi_sta_start(char *ifname, int *len);

int scm_wifi_sta_stop(void);

int scm_wifi_sta_set_reconnect_policy(int enable, unsigned int timeout,
                                     unsigned int period, unsigned int max_try_count);

int scm_wifi_sta_get_country_code(char *country_code);
int scm_wifi_sta_set_country_code(char *country_code, bool use_ieee80211d, bool set_ap_mode_enable);
int scm_wifi_sta_set_ps(int mode);
int scm_wifi_sta_set_sleeptime(uint16_t sleeptime);

#ifdef CONFIG_API_SCAN

int scm_wifi_sta_scan(void);

int scm_wifi_sta_advance_scan(scm_wifi_scan_params *sp);

int scm_wifi_sta_scan_results(scm_wifi_ap_info *ap_list, uint16_t *ap_num, uint16_t max_ap_num);

#endif

int scm_wifi_sta_set_config(scm_wifi_assoc_request *req, void *fast_config);

int scm_wifi_sta_connect(void);

int scm_wifi_sta_disconnect(void);

int scm_wifi_sta_disconnect_event(uint8_t enable);

int scm_wifi_sta_get_ap_rssi(void);

int scm_wifi_sta_get_connect_info(scm_wifi_status *connect_status);

int scm_wifi_sta_restore_psk(uint8_t *psk);

int scm_wifi_sta_fast_connect(scm_wifi_fast_assoc_request *fast_request);

void scm_wifi_sta_dump_ap_info(scm_wifi_status *connect_status);

int scm_wifi_sta_get_psk(uint8_t *psk, int len);

int scm_wifi_get_mode(wifi_mode_t *mode, uint8_t wlan_if);

int scm_wifi_get_config(uint8_t wlan_if, wifi_config_t *cfg);

int scm_wifi_clear_config(uint8_t wlan_if);

int scm_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info);

#ifdef CONFIG_API_SOFTAP

int scm_wifi_sap_get_state(bool *en, bool *started, bool *configured);

int scm_wifi_sap_set_state(bool *started, bool *configured);

int scm_wifi_sap_start(char *ifname, int *len);

int scm_wifi_sap_stop(void);

int scm_wifi_sap_set_config(scm_wifi_softap_config *sap);

int scm_wifi_sap_get_config(scm_wifi_softap_config *sap);

int scm_wifi_sap_set_beacon_interval(uint32_t interval);

int scm_wifi_sap_set_dtim_period(uint8_t period);

int scm_wifi_sap_get_connected_sta(scm_wifi_ap_sta_info *sta_list, uint8_t *sta_num);

int scm_wifi_sap_deauth_sta (const char *txtaddr, unsigned char addr_len);

#endif

int scm_wifi_set_options(scm_wifi_set_option opt, void *arg);

int scm_wifi_get_options(scm_wifi_get_option opt, void *buf);

int scm_wifi_set_channel(char *ifname, uint8_t primary, scm_wifi_2nd_ch_loc secondary);

int scm_wifi_get_channel(char *ifname, uint8_t *primary, scm_wifi_2nd_ch_loc *secondary);

int scm_wifi_dhcps_stop(void);

int scm_wifi_dhcps_start(void);

int scm_wifi_dhcp_stop(void);

int scm_wifi_dhcp_start(void);


#ifdef CONFIG_API_MONITOR

int scm_wifi_80211_tx(const char *ifname, const void *buffer, int len);
int scm_wifi_set_promiscuous(const char *ifname, bool en);
int scm_wifi_get_promiscuous(const char *ifname, bool *en);

#endif

#ifdef __NOT_YET__

int scm_wifi_raw_scan(const char *ifname,
                         scm_wifi_scan_params *custom_scan_param, scm_wifi_scan_no_save_cb cb);

int scm_wifi_enable_intrf_mode(const char* ifname, unsigned char enable, unsigned short flag);

int scm_wifi_enable_anti_microwave_intrf(unsigned char enable);

#endif

#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
int scm_wifi_set_shared_mem_addr(uint32_t *addr);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_API_H__ */

/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_ADA_H__
#define __AYLA_ADA_ADA_H__

#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/callback.h>
#include <ayla/conf.h>
#include <ayla/log.h>
#include <ayla/tlv.h>
#include <ayla/timer.h>
#include <ayla/ipaddr_fmt.h>
#include <al/al_clock.h>
#include <al/al_net_addr.h>

#include <ada/ada_conf.h>
#include <ada/batch.h>
#include <ada/client.h>
#include <ada/client_ota.h>
#include <ada/dnss.h>
#include <ada/err.h>
#include <ada/prop.h>
#include <ada/prop_mgr.h>
#include <ada/sched.h>
#include <ada/sprop.h>
#include <ada/task_label.h>
#include <ada/ada_wifi.h>
#include <ada/prop_mgr.h>

#ifdef ADA_BUILD_WIFI_SUPPORT
#include <al/al_wifi.h>
#include <adw/wifi.h>
#include <adw/wifi_conf.h>
#endif /* ifdef ADA_BUILD_WIFI_SUPPORT */

/**
 * @file
 * ADA Application Interfaces
 *
 * The file contains ADA APIs for applications
 */

/*--------------------- ADA main interfaces   -----------------*/

#ifdef ADA_BUILD_FINAL
/**
 * Finalize the ADA client environment.
 */
void ada_final(void);
#endif

/**
 * The ADA main loop.
 */
void ada_main_loop(void);

/*--------------------- ADA client interfaces   -----------------*/

/**
 * Start up the ADA client agent.
 */
int ada_client_up(void);

/**
 * Shut down the ADA client agent.
 */
void ada_client_down(void);

/**
 * Start registration window.
 */
void ada_client_reg_window_start(void);

/**
 * Register for callback when ADS reachability changes, or a new
 * connection attempt fails.
 * This callback is made inside the client thread, and must not block.
 * Multiple callbacks may be registered, and they'll all be called.
 * Callbacks may not be unregistered for now.

 * \param fn is the callback to be registered to the client.
 * \param arg is argument of the callback.
 */
void ada_client_event_register(void (*fn)(void *arg, enum ada_err), void *arg);

/**
 * Set the template version for the device.
 *
 * \param ver is the string of template version. It should remain valid
 * after the call.
 */
void ada_client_set_oem_version(const char *ver);

/**
 * Start up httpd server.
 *
 * Call it on network interface available.
 */
void ada_server_up(void);

/**
 * Shut down httpd server.
 */
void ada_server_down(void);

/*--------------------- ADA adap Interfaces   -----------------*/

/**
 * Get signal strength from network layer, usually Wi-Fi.
 *
 * \param signal is address for the signal to be stored.
 *
 * \returns 0 on success, -1 if not supported.
 */
int adap_net_get_signal(int *signal);

/**
 * Enumeration for adap_net_get_conn_info
 */
enum ada_net_info {
	ANI_SSID,
	ANI_BSSID,
	ANI_USER_ID,
	ANI_MCC,
	ANI_MNC,
	ANI_APN,
	ANI_BASE_STATION,
	ANI_IMSI,
	ANI_IMEI
};

/**
 * Get connection info item for the primary network interface
 *
 * \param item is the enum value for the desired item.
 * \param buf is a pointer to result buffer.
 * \param len is the length of the buffer.
 *
 * \return length of string if supported, -1 if item not supported or error.
 */
int adap_net_get_conn_info(enum ada_net_info item, char *buf, size_t len);

/**
 * Execute a cli command, placing as much of its output as will fit into
 * the provided output buffer, then call the callback function with the
 * provided argument. Application should implement the function.
 *
 * \param cmd UTF8 string containing a CLI command.
 * \param outbuf Buffer to hold as much UTF8 CLI output as will fit. The output
 *    is always terminated with a null character including when truncated.
 * \param outlen Size of the output buffer.
 * \param callback Function that will be called after the command has been
 *    executed. The callback may be called before or after this function
 *    returns. May be NULL if no callback is required.
 * \param cb_arg Argument that will be passed to the callback function.
 *
 * \returns AE_ERR if this feature is not supported
 */
enum ada_err adap_cli_exec(const char *cmd, char *outbuf, size_t outlen,
    void (*callback)(void *arg), void *cb_arg);

/*----------------- ADA interfaces for CLI ---------------*/

/**
 * Information of client LAN registration. It is used by CLI.
 */
struct ada_client_lan_info {
	u8 enable;	/**< flag of LAN enabled */
	u8 auto_echo;	/**< flag of auto echo */
};

/**
 * Get client LAN state. It is used by CLI.
 *
 * \param li pointer to struct ada_client_lan_info.
 */
void ada_client_get_lan_info(struct ada_client_lan_info *li);

/**
 * Information of client LAN registration. It is used by CLI.
 */
struct ada_client_lan_reg {
	u8 id;				/**< ID of LAN registration */
	const char *uri;		/**< uri */
	struct al_net_addr host_addr;	/**< host IP address */
	u16 host_port;			/**< flag of auto echo */
};

/**
 * Get client LAN registration information. It is used by CLI.
 *
 * \param idx index of LAN registration information.
 *
 * \param lr pointer to struct ada_client_lan_reg.
 *
 * \returns 0 on success.
 */
int ada_client_get_lan_reg(int idx, struct ada_client_lan_reg *lr);

/**
 * Set client test connect. It is used by CLI.
 *
 * \param test_connect value to be set.
 */
void ada_client_set_test_connect(u8 test_connect);

#ifndef ADA_CLIENT_COAP
/**
 * Information of client. It is used by CLI.
 */
struct ada_client_info {
	const char *host;	/**< host of struct http_client */
	u64 connect_time;	/**< members in struct client_state */
	u8 poll_ads:1;		/**< flag of polling ADS */
	u8 np_started:1;	/**< flag of np started */
	u8 np_up:1;		/**< flag of np up */
	u8 np_up_once:1;	/**< flag of np just up once */
	u8 np_any_event:1;	/**< flag of any np event */
	u8 np_event:1;		/**< flag of np evwnt */
	u8 test_connect:1;	/**< Indicates ADS should not consider
				 * the DSN activated. */
	u16 conf_port;		/**< Configured port from host app,
				 * if non-zero. */
};

/**
 * Get client status information. It is used by CLI.
 *
 * \param info pointer to struct ada_client_info.
 */
void ada_client_get_info(struct ada_client_info *info);
#endif

/**
 * Display client connection state and http client state. It can be called
 * both by CLI and APP.
 */
void ada_client_state_log(void);

/**
 * Calculate device signature based on hardware information
 *
 * \param oem_id id of OEM, can be found on dashboard
 * \param hw_id serial number of hardware device given by OEM
 * \param key_id private key id assgined by Ayla cloud
 * \param prv_key pointer to prviate key buffer
 * \param key_len length of private key
 * \param sig pointer to store generated signature
 * \param sig_len length of give signature buffer, must equal to
 *    CLIENT_CONF_RSA_MOD_SIZE
 */
int ada_client_cal_signature(const char *oem_id, const char *hw_id,
   const char *key_id, const char *prv_key, size_t key_len,
   char *sig, size_t sig_len);
#endif /* __AYLA_ADA_ADA_H__ */

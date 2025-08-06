/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

#ifdef AYLA_BLUETOOTH_SUPPORT

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <ayla/utypes.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <sys/types.h>
#include <ayla/timer.h>
#include <ayla/conf.h>
#include <al/al_random.h>
#include <al/al_os_mem.h>
#include <ada/err.h>
#include <ada/client.h>
#include <adw/wifi.h>
#include <adb/adb.h>
#include <al/al_bt.h>
#include <adb/adb_ayla_svc.h>
#include <adb/adb_conn_svc.h>
#include <ada/local_control.h>
#include <adb/adb_mbox_svc.h>
#include <ada/onboard_msg.h>
#include <adb/adb_wifi_cfg_svc.h>
#include <cmsis_os.h>

#include "nimble/nimble_port.h"
#define H_BLE_HS_LOG_	/* prevent conflict with LOG_* defines */
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define MS_TO_TICKS(ms) ((uint32_t)(((uint32_t)(ms) * osKernelGetTickFreq()) / 1000))

#define TICK_PERIOD_MS ((uint32_t) 1000 / osKernelGetTickFreq())

/* configuration can be changed from nimble host syscfg.h */
#define NIMBLE_MAX_CONNECTIONS 	MYNEWT_VAL(BLE_MAX_CONNECTIONS)

/* configuration can be changed from nimble host syscfg.h */
#define BT_NIMBLE_MAX_BONDS		MYNEWT_VAL(BLE_STORE_MAX_BONDS)

/* configuration can be changed freely here */
#define NIMBLE_NVS_PERSIST		MYNEWT_VAL(BLE_STORE_CONFIG_PERSIST)

/*
 * TODO: This is patched by esp to nimble host
 * If it is needed, we also apply our sdk
 * #define __PATCH__
 */

#define BT_MAX_SERVICES	10

#define BT_WIFI_TIMEOUT	(5 * 60 * 1000)	/* ms timeout for wifi adverts */
#define BT_WIFI_ONBOARD_POLL 4000	/* ms polling period for wifi config */

/*
 * BT device name definitions
 *
 * The BT device name may be defined through configuration. The name
 * is defined by the following format string:
 *
 * <prefix>%<digits>[X]
 *
 * where:
 *
 * <prefix> - any ASCII string that doesn't include '%'
 * <digits> - even number of hex digits of BT device address to append
 * X - optional, to specify upper case hex, defaults to lower case
 */
#define BT_NAME_DEFAULT_FORMAT	"Ayla%4X"
#define BT_NAME_SIZE_MAX	12		/* max characters in name */

/*
 * BT passkey defines
 *
 * Passkeys are 6 digits that may include leading zeros.
 *
 * Configuration is as follows:
 *
 * 0 - 999999 - fixed passkey defined by config
 * -1 - no passkey auth
 * -2 - random passkey
 * all other configured values or no configured value - no passkey auth
 */
#define BT_PASSKEY_CHARS	6		/* number of char in passkey */
#define BT_PASSKEY_NONE		-1		/* no passkey */
#define BT_PASSKEY_MIN		-2		/* random passkey */
#define BT_PASSKEY_MAX		999999

#define BT_RESET_CONF		"bt/reset"	/* reset BT NV storage */
#define BT_NAME_KEY_CONF	"bt/hostname"	/* name formatting string */
#define BT_PASSKEY_KEY_CONF	"bt/key"	/* passkey */
#define BT_WIFI_TIME_CONF	"bt/wifi/time"	/* timeout for onboarding */
#define BT_WIFI_PB_CONF		"bt/wifi/user"	/* push-button onboarding */

struct adb_conn {
	u8 active:1;
	u16 handle;
	u16 mtu;
	void *ctxt;	/* pointer to application specific context */
};

static struct adb_conn adb_connections[NIMBLE_MAX_CONNECTIONS];
static const struct adb_attr *services_table[BT_MAX_SERVICES];
static osSemaphoreId_t al_adb_semaphore;
static osSemaphoreId_t al_bt_mutex;
u8 al_bt_locked;

/*
 * Flags indicating which functions currently require Bluetooth to stay up
 */
static u8 al_bt_keep_up_flags =	/* flags indicating if BT needs to stay up */
		0
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
		| BIT(AL_BT_FUNC_LOCAL_CONTROL)
#endif
		;

static u8 al_bt_inited;		/* set if BT has been inited */
static u8 al_bt_init_complete;	/* set when init has fully completed */
static u8 al_bt_deinited;	/* set if BT has been permanently shutdown */
static u8 al_bt_stopped;	/* set if BT task has returned */
static volatile u8 al_bt_controller_syncd; /* set when controller is sync'd */
static int al_bt_connections;
static void (*al_bt_passkey_cb)(u32 passkey);
static u8 al_bt_conf_was_reset = 1; /* cleared if config wasn't reset */
static s32 al_bt_configured_passkey = BT_PASSKEY_NONE;
static unsigned int al_bt_wifi_timeout = BT_WIFI_TIMEOUT;
static u8 al_bt_wifi_holdoff;	/* set if pushbutton provisioning mode */
static u8 al_bt_wifi_provisioning;
static osTimerId_t al_bt_timer;
static osTimerId_t al_bt_pairing_timer;

static int al_bt_gap_event_handler(struct ble_gap_event *event, void *arg);
static struct adb_conn *al_bt_connection_find(u16 handle, u8 *index);
static int al_bt_conn_mtu_update(u16 handle, u16 mtu);
static void al_bt_wifi_provision_start(void);
static void al_bt_wifi_provision_stop(void *arg);
static void al_bt_advertise(void);

static void (*al_bt_timer_func)(void *arg) = al_bt_wifi_provision_stop;

static osThreadId_t host_task_h;

/*
 * Entry point for nimble host task
 */
static void al_bt_host_task(void *param)
{
	log_thread_id_set("b");
	adb_log(LOG_DEBUG "host task started");
	/* This function will return only when nimble_port_stop is executed */
	nimble_port_run();
	adb_log(LOG_DEBUG "host task exiting.  stack headroom %u",
	    osThreadGetStackSpace(host_task_h));
	al_bt_stopped = 1;
	al_bt_controller_syncd = 0;

	/*
	 * Suspend here waiting to be deleted.
	 */
	adb_log(LOG_INFO "ble is terminated\n");
	for (;;) {
		osThreadSuspend(host_task_h);
	}
}

/*
 * Log connection descriptor info
 */
static void al_bt_print_conn_desc(struct ble_gap_conn_desc *desc)
{
	char addr_str[20];

	adb_snprint_addr(desc->our_ota_addr.val, addr_str,
	    sizeof(addr_str));
	adb_log(LOG_DEBUG "handle %d our_ota_addr type %d addr %s",
	    desc->conn_handle, desc->our_ota_addr.type, addr_str);
	adb_snprint_addr(desc->our_id_addr.val, addr_str,
	    sizeof(addr_str));
	adb_log(LOG_DEBUG "our_id_addr type %d addr %s",
	    desc->our_id_addr.type, addr_str);
	adb_snprint_addr(desc->peer_ota_addr.val, addr_str,
	    sizeof(addr_str));
	adb_log(LOG_DEBUG "peer_ota_addr type %d addr %s",
	    desc->peer_ota_addr.type, addr_str);
	adb_snprint_addr(desc->peer_id_addr.val, addr_str,
	    sizeof(addr_str));
	adb_log(LOG_DEBUG "peer_id_addr type %d addr %s",
	    desc->peer_id_addr.type, addr_str);
	adb_log(LOG_DEBUG
	    "conn itvl %d latency %d timeout %d "
	    "enc %d auth %d bond %d",
	    desc->conn_itvl, desc->conn_latency,
	    desc->supervision_timeout, desc->sec_state.encrypted,
	    desc->sec_state.authenticated, desc->sec_state.bonded);
}

/*
 * Delete the oldest bond for a peer that is not currently connected and
 * is not the first bond entry. The first entry is preserved because it is
 * presumably the owner's mobile, which was used to onboard this device.
 */
static int al_bt_delete_old_peer(const ble_addr_t *peer_addr)
{
	ble_addr_t peer_id_addrs[BT_NIMBLE_MAX_BONDS];
	int num_peers = 0;
	int i;
	int rc;

	rc = ble_store_util_bonded_peers(peer_id_addrs, &num_peers,
	    ARRAY_LEN(peer_id_addrs));
	if (rc || !num_peers) {
		return BLE_HS_ENOENT;
	}

	/*
	 * Don't delete the first entry, which presumably is the peer
	 * that onboard this device.
	 */
	for (i = 1; i < num_peers; i++) {
		if (peer_addr &&
		    !ble_addr_cmp(peer_addr, &peer_id_addrs[i])) {
			continue;
		}
		rc = ble_gap_conn_find_by_addr(&peer_id_addrs[i], NULL);
		if (rc) {
			rc = ble_gap_unpair(&peer_id_addrs[i]);
			adb_log(LOG_DEBUG "deleted peer entry %d", i);
			return 0;
		}
	}
	return BLE_HS_ENOENT;
}

static void al_bt_store_counts_log(void)
{
	int our_count = 0;
	int peer_count = 0;
	int cccd_count = 0;

	ble_store_util_count(BLE_STORE_OBJ_TYPE_OUR_SEC,
	    &our_count);
	ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC,
	    &peer_count);
	ble_store_util_count(BLE_STORE_OBJ_TYPE_CCCD,
	    &cccd_count);
	adb_log(LOG_DEBUG "stored records: ours %d peers %d cccds %d",
	    our_count, peer_count, cccd_count);
}

void al_bt_deinit(void)
{
	if (al_bt_deinited) {
		return;
	}
	al_bt_deinited = 1;

	adb_log(LOG_DEBUG "Bluetooth shut down");
}

int al_bt_enabled(void)
{
	if (al_bt_deinited || al_bt_stopped || !al_bt_inited) {
		return 0;
	}
	return 1;
}

/*
 * Callback to permanently shutdown the nimble host thread and free Bluetooth
 * memory. This frees a large amount of memory to the heap. Bluetooth
 * cannot be restarted after this. A restart is required to start
 * Bluetooth again.
 * The stack is shallow here, don't do too much.
 */
static void al_bt_shutdown_cb(void *arg)
{
	if (al_bt_deinited) {
		return;
	}
	if (!al_bt_stopped) {
		nimble_port_stop();
		while (!al_bt_stopped) {
			osDelay(1);
		}

	}
    /* we don't have to use nimble_port_deinit */
	al_bt_deinit();
	osTimerDelete(al_bt_timer);
	if (al_bt_wifi_provisioning) {
		adb_event_notify(ADB_EV_WIFI_PROVISION_STOP);
		al_bt_wifi_provisioning = 0;
	}
}

static void al_bt_timer_cb(void *arg)
{
	ASSERT(al_bt_timer_func);
	al_bt_timer_func(arg);
}

/*
 * Shutdown bluetooth and recover memory. Shutdown can't
 * be done by this thread. Use a timer to do it on the timer thread.
 */
static void al_bt_stop(void)
{
	if (al_bt_deinited) {
		return;
	}

	al_bt_timer_func = al_bt_shutdown_cb;
	while (1) {
		if (osTimerStart(al_bt_timer, 1) != osErrorResource) {
			break;
		}
	}
}

void al_bt_keep_up_set(enum al_bt_function func_id)
{
	al_bt_keep_up_flags |= BIT(func_id);
}

void al_bt_keep_up_clear(enum al_bt_function func_id)
{
	al_bt_keep_up_flags &= ~BIT(func_id);
	if (!al_bt_keep_up_flags && !al_bt_deinited) {
		adb_log(LOG_DEBUG "Bluetooth not needed, shutting down");
		al_bt_stop();
	}
}

static void al_bt_wifi_timeout_start(void)
{
	al_bt_timer_func = al_bt_wifi_provision_stop;

	while (1) {
		if (osTimerStart(al_bt_timer, MS_TO_TICKS(al_bt_wifi_timeout)) != osErrorResource) {
			break;
		}
	}
}

/*
 * Set default timeout for BLE Wi-Fi setup.
 * This may be called before or after al_bt_init().
 */
void al_bt_wifi_timeout_set(unsigned int time_ms)
{
	if (!time_ms) {
		al_bt_wifi_holdoff = 1;
		return;
	}
	al_bt_wifi_holdoff = 0;
	al_bt_wifi_timeout = time_ms;
	if (!al_bt_timer) {
		return;			/* not initialized */
	}
	if (al_bt_wifi_provisioning) {
		al_bt_wifi_timeout_start();	/* running, restart timer */
		return;
	}
	al_bt_wifi_provision_start();
}

static int al_bt_conf_read_long(char *name, long *valp)
{
	char buf[20];
	char *errptr;
	long val;
	int len;

	len = conf_persist_get(name, buf, sizeof(buf));
	if (len <= 0 || len >= sizeof(buf)) {
		return -1;
	}
	buf[len] = '\0';
	val = strtol(buf, &errptr, 10);
	if (errptr == buf || *errptr) {
		adb_log(LOG_ERR "adb_conf: %s: invalid value \"%s\"",
		    name, buf);
		return -1;
	}
	*valp = val;
	return 0;
}

static void al_bt_wifi_provision_conf_read(void)
{
	static u8 done;
	long val;

	if (done) {
		return;
	}
	done = 1;

	/*
	 * Read configuration for provisioning/onboarding time in seconds.
	 */
	if (!al_bt_conf_read_long(BT_WIFI_TIME_CONF, &val)) {
		if (val <= 0 || val > MAX_U32 / 1000) {
			adb_log(LOG_ERR "adb_conf: %s: invalid value %ld",
			    BT_WIFI_TIME_CONF, val);
		} else {
			al_bt_wifi_timeout = (u32)(val * 1000);
		}
	}

	/*
	 * Read configuration for pushbutton mode.
	 */
	if (!al_bt_conf_read_long(BT_WIFI_PB_CONF, &val)) {
		if (val < 0 || val > 1) {
			adb_log(LOG_ERR "adb_conf: %s: invalid value %ld",
			    BT_WIFI_PB_CONF, val);
		} else {
			al_bt_wifi_holdoff = val;
		}
	}
}

/*
 * Start provisioning.
 */
static void al_bt_wifi_provision_start(void)
{
	if (adw_wifi_configured()) {
		return;
	}
	al_bt_keep_up_set(AL_BT_FUNC_PROVISION);
	al_bt_wifi_provision_conf_read();
	if (al_bt_wifi_holdoff) {
		adb_pairing_mode_set(ADB_PM_DISABLED, 0);
		return;		/* do not advertise yet */
	}
	if (al_bt_configured_passkey == BT_PASSKEY_NONE) {
		adb_pairing_mode_set(ADB_PM_NO_PASSKEY, 0);
	} else if (al_bt_configured_passkey >= 0) {
		adb_pairing_mode_set(ADB_PM_CONFIGURED_PASSKEY, 0);
	} else {
		adb_pairing_mode_set(ADB_PM_RANDOM_PASSKEY, 0);
	}
	al_bt_advertise();
}

/*
 * Stop provisioning.
 */
static void al_bt_wifi_provision_stop(void *arg)
{
	if (!al_bt_wifi_provisioning) {
		return;
	}
	al_bt_wifi_provisioning = 0;
	osTimerStop(al_bt_timer);
	adb_pairing_mode_set(ADB_PM_DISABLED, 0);
	adb_event_notify(ADB_EV_WIFI_PROVISION_STOP);
	ble_gap_adv_stop();		/* caller may restart advertisements */
}

/*
 * Start BLE advertisements
 */
static void al_bt_advertise(void)
{
	int rc;
	struct ble_hs_adv_fields adv_fields;
	struct ble_gap_adv_params adv_params;
	const char *name;
	const char *connectability;

	if (al_bt_deinited || al_bt_stopped || !al_bt_controller_syncd) {
		/*
		 * Bluetooth has been permanently shutdown or HCI
		 * interface is not sync'd.
		 */
		return;
	}

	if (!adw_wifi_configured() && (al_bt_connections > 0)) {
		/* limit to one connection during Wi-Fi provisioning */
		return;
	}

	if (!adw_wifi_configured()) {
		al_bt_wifi_provisioning = 1;
		adb_event_notify(ADB_EV_WIFI_PROVISION_START);
		al_bt_wifi_timeout_start();
#ifdef AYLA_TEST_SERVICE_SUPPORT
		adb_mbox_svc_callbacks_set(om_session_alloc, om_session_down,
		    om_rx);
#endif
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
	} else if (al_bt_wifi_provisioning) {
		al_bt_wifi_provision_stop(NULL);
		adb_log(LOG_INFO "Wi-Fi configured, stop advertising");
		if (!al_bt_connections) {
			al_bt_keep_up_clear(AL_BT_FUNC_PROVISION);
		}
		adb_mbox_svc_callbacks_set(lctrl_session_alloc,
		    lctrl_session_down, lctrl_msg_rx);
#endif
	} else {
		if (!al_bt_connections) {
			al_bt_keep_up_clear(AL_BT_FUNC_PROVISION);
		}
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
		adb_mbox_svc_callbacks_set(lctrl_session_alloc,
		    lctrl_session_down, lctrl_msg_rx);
#else
		adb_log(LOG_INFO "Wi-Fi configured, not advertising");
#ifdef AYLA_TEST_SERVICE_SUPPORT
		adb_mbox_svc_callbacks_set(NULL, NULL, NULL);
#endif
		return;
#endif
	}
	/*
	 * Stop advertising if its active so we can restart with any changes
	 * to fields and parameters.
	 */
	ble_gap_adv_stop();

	memset(&adv_fields, 0, sizeof(adv_fields));

	adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

	/*
	 * Advertise Ayla ID only if Wi-Fi is configured and the service
	 * data is available (requires device has LAN IP key).
	 *
	 * Don't advertise the Ayla ID if the device is not configured
	 * even if it has an Ayla ID, which can happen if the LAN IP key has
	 * leaked into the factory config on developer's devices.
	 */
	if (adw_wifi_configured()) {
		if (!client_lctrl_is_enabled()) {
			/*
			 * Device is configured but local control is disabled.
			 * Don't advertise.
			 */
			adb_log(LOG_DEBUG
			    "local control disabled, not advertising");
			/*
			 * If the LAN config was received from the service and
			 * local control is disabled, we don't need to keep
			 * Bluetooth up. Otherwise, keep Bluetooth up until we
			 * receive config from the service and know whether
			 * Bluetooth will be needed or not.
			 */
			if (client_lanconf_recvd()) {
				al_bt_keep_up_clear(AL_BT_FUNC_LOCAL_CONTROL);
			}
			return;
		}
		/*
		 * Advertise local control service data, pointer should be
		 * const but isn't.
		 */
		adv_fields.svc_data_uuid16 = (u8 *)adb_ayla_svc_data_get(
		    &adv_fields.svc_data_uuid16_len);
	}

	/*
	 * If service data containing resolvable random id is available,
	 * advertise that and no name. This helps make the device less
	 * trackable.
	 *
	 * If the service data isn't available (device not onboarded),
	 * advertise the name.
	 */
	if (!adv_fields.svc_data_uuid16_len) {
		name = ble_svc_gap_device_name();
		adb_log(LOG_DEBUG "gap device name %s", name);
		adv_fields.name = (uint8_t *)name;
		adv_fields.name_len = strlen(name);
		adv_fields.name_is_complete = 1;
	}

	/* advertise UUID for Ayla generic service */
	adv_fields.uuids16 = (ble_uuid16_t *)adb_ayla_svc_uuid_get();
	adv_fields.num_uuids16 = 1;
	adv_fields.uuids16_is_complete = 1;

	rc = ble_gap_adv_set_fields(&adv_fields);
	if (rc != 0) {
		adb_log(LOG_ERR "error setting advertisement fields %d", rc);
		if (rc == BLE_HS_ENOTSYNCED) {
			al_bt_controller_syncd = 0;
		}
		return;
	}

	memset(&adv_params, 0, sizeof adv_params);
	if (al_bt_connections < NIMBLE_MAX_CONNECTIONS) {
		connectability = "connectable";
		adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
	} else {
		connectability = "not connectable";
		adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
	}
	adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

	adb_log(LOG_INFO "starting advertisements, %s", connectability);
#ifdef __PATCH__
	adb_log(LOG_DEBUG2
	    "%s io_cap %d mitm %d no_pair %d bond %d okey %d tkey %d",
	    __func__, ble_hs_cfg.sm_io_cap, ble_hs_cfg.sm_mitm,
	    ble_hs_cfg.sm_no_pairing, ble_hs_cfg.sm_bonding,
	    ble_hs_cfg.sm_our_key_dist, ble_hs_cfg.sm_their_key_dist);
#else
	adb_log(LOG_DEBUG2
	    "%s io_cap %d mitm %d bond %d okey %d tkey %d",
	    __func__, ble_hs_cfg.sm_io_cap, ble_hs_cfg.sm_mitm,
	    ble_hs_cfg.sm_bonding,
	    ble_hs_cfg.sm_our_key_dist, ble_hs_cfg.sm_their_key_dist);
#endif
	rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
	    &adv_params, al_bt_gap_event_handler, NULL);
	if (rc != 0) {
		adb_log(LOG_ERR "error enabling advertisement %d", rc);
		return;
	}
}

/*
 * Handle nimble host reset events
 */
static void al_bt_on_reset(int reason)
{
	adb_log(LOG_INFO "%s reason %d", __func__, reason);
}

/*
 * Handle nimble host sync events
 */
static void al_bt_on_sync(void)
{
	int rc;
	uint8_t addr_val[6] = { 0 };
	char addr_str[20];
	uint8_t own_addr_type;

	adb_log(LOG_DEBUG "controller sync'd");

	al_bt_controller_syncd = 1;

	rc = ble_hs_util_ensure_addr(0);
	if (rc) {
		adb_log(LOG_ERR "ensure addr error %d", rc);
		return;
	}

	/* Figure out address to use while advertising (no privacy for now) */
	rc = ble_hs_id_infer_auto(0, &own_addr_type);
	if (rc) {
		adb_log(LOG_ERR "error determining address type %d", rc);
		return;
	}

	rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
	if (rc) {
		adb_log(LOG_DEBUG "unable to read device address");
		return;
	}

	adb_snprint_addr(addr_val, addr_str, sizeof(addr_str));
	adb_log(LOG_DEBUG "device address: %s", addr_str);

	if (al_bt_init_complete) {
		al_bt_advertise();
	}
}

/*
 * Set callback to be called when passkey needs to be displayed
 */
void al_bt_passkey_callback_set(void (*callback)(u32 passkey))
{
	al_bt_passkey_cb = callback;
}

static void al_bt_display_passkey(u16 conn_handle)
{
	struct ble_gap_conn_desc desc;
	struct ble_sm_io pk;
	u32 passkey;
	int rc;

	rc = ble_gap_conn_find(conn_handle, &desc);
	if (rc) {
		return;
	}

	switch (adb_pairing_mode_get()) {
	case ADB_PM_CONFIGURED_PASSKEY:
		if (al_bt_configured_passkey < 0) {
			adb_log(LOG_ERR
			    "configured passkey not available, using random");
			goto random;
		}
		passkey = al_bt_configured_passkey;
		break;
	case ADB_PM_AYLA_PASSKEY:
		passkey = adb_ayla_svc_passkey_get();
		if (passkey > ADB_PASSKEY_MAX) {
			/* likely LAN IP key not yet available */
			adb_log(LOG_ERR
			    "Ayla passkey not available, using random");
			goto random;
		}
		break;
	case ADB_PM_RANDOM_PASSKEY:
random:
		al_random_fill(&passkey, sizeof(passkey));
		break;
	default:
		/* not in a passkey mode */
		return;
	}

	/* limit to maximum value */
	pk.passkey = passkey % (ADB_PASSKEY_MAX + 1);
	pk.action = BLE_SM_IOACT_DISP;

	rc = ble_sm_inject_io(conn_handle, &pk);
	ASSERT(!rc);

	if (al_bt_passkey_cb) {
		al_bt_passkey_cb(pk.passkey);
	} else {
		adb_log(LOG_INFO "passkey %06d", pk.passkey);
	}
}

/*
 * Handle GAP events
 */
static int al_bt_gap_event_handler(struct ble_gap_event *event, void *arg)
{
	struct ble_gap_conn_desc desc;
	u8 mask;
	int rc;
	const struct adb_attr *attr;
	struct adb_chr_info *chr_info;

	switch (event->type) {
	case BLE_GAP_EVENT_CONNECT:
		/* A new connection was established or a connection attempt
		 * failed. */
		adb_log(LOG_DEBUG "connection %s status %d ",
		    event->connect.status == 0 ? "established" : "failed",
		    event->connect.status);
		if (event->connect.status == 0) {
			al_bt_connections++;
			rc = ble_gap_conn_find(event->connect.conn_handle,
			    &desc);
			ASSERT(!rc);
			al_bt_print_conn_desc(&desc);
			rc = al_bt_connection_add(event->connect.conn_handle);
			if (rc) {
				adb_log(LOG_ERR "bt conn add err %d", rc);
			}
			adb_conn_event_notify(ADB_CONN_UP,
			    event->connect.conn_handle);
		}
		al_bt_advertise();
		break;

	case BLE_GAP_EVENT_DISCONNECT:
		adb_log(LOG_DEBUG "disconnect reason %d",
		    event->disconnect.reason);
		adb_conn_event_notify(ADB_CONN_DOWN,
		    event->disconnect.conn.conn_handle);
		rc = al_bt_connection_delete(
		    event->disconnect.conn.conn_handle);
		if (rc) {
			adb_log(LOG_ERR "bt conn delete err %d", rc);
		}
		al_bt_connections--;
		al_bt_print_conn_desc(&event->disconnect.conn);
		if (event->disconnect.reason == BLE_HS_ETIMEOUT_HCI) {
			al_bt_controller_syncd = 0;
			break;
		}
		al_bt_advertise();
		break;

	case BLE_GAP_EVENT_CONN_UPDATE:
		/* The central has updated the connection parameters. */
		adb_log(LOG_DEBUG "connection updated status %d",
		    event->conn_update.status);
		rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
		ASSERT(!rc);
		al_bt_print_conn_desc(&desc);
		break;

	case BLE_GAP_EVENT_CONN_UPDATE_REQ:
		adb_log(LOG_DEBUG "connection update request");
		return 0;

	case BLE_GAP_EVENT_ADV_COMPLETE:
		adb_log(LOG_DEBUG "advertise complete reason %d",
		    event->adv_complete.reason);
		if (event->adv_complete.reason == BLE_HS_ETIMEOUT_HCI) {
			al_bt_controller_syncd = 0;
			break;
		}
		al_bt_advertise();
		break;

	case BLE_GAP_EVENT_ENC_CHANGE:
		if (!event->enc_change.status) {
			adb_log(LOG_DEBUG "encryption change success");
		} else {
			adb_log(LOG_ERR "authentication failure, status %d",
			    event->enc_change.status);
		}
		rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
		ASSERT(!rc);
		al_bt_print_conn_desc(&desc);
#ifndef AYLA_LOCAL_CONTROL_SUPPORT
		if (!event->enc_change.status && desc.sec_state.encrypted) {
			al_bt_wifi_timeout_start();
		}
#endif
		break;

	case BLE_GAP_EVENT_PASSKEY_ACTION:
		adb_log(LOG_DEBUG "passkey action event %d",
		    event->passkey.params.action);
		if (adb_pairing_mode_get() == ADB_PM_DISABLED) {
			adb_log(LOG_WARN
			    "attempt to pair while pairing disabled");
			break;
		}
		if (event->passkey.params.action != BLE_SM_IOACT_DISP) {
			adb_log(LOG_ERR "unsupported passkey action");
			break;
		}

		al_bt_display_passkey(event->passkey.conn_handle);
		break;

	case BLE_GAP_EVENT_SUBSCRIBE:
		adb_log(LOG_DEBUG
		    "subscribe event conn_handle %d attr_handle %d "
		    "reason %d prevn %d curn %d previ %d curi %d",
		    event->subscribe.conn_handle,
		    event->subscribe.attr_handle,
		    event->subscribe.reason,
		    event->subscribe.prev_notify,
		    event->subscribe.cur_notify,
		    event->subscribe.prev_indicate,
		    event->subscribe.cur_indicate);
		mask = al_bt_connection_mask(event->subscribe.conn_handle);
		if (!mask) {
			adb_log(LOG_ERR "subscribe connection not found");
			break;
		}
		attr = al_bt_find_attr_by_handle(event->subscribe.attr_handle);
		if (!attr || attr->type != ADB_ATTR_CHR) {
			/*
			 * Attributes that are outside of ADB, not known or not
			 * characteristics.
			 */
			break;
		}
		chr_info = attr->info;
		if (event->subscribe.cur_notify) {
			chr_info->notify_mask |= mask;
		} else {
			chr_info->notify_mask &= ~mask;
		}
		if (event->subscribe.cur_indicate) {
			chr_info->indicate_mask |= mask;
		} else {
			chr_info->indicate_mask &= ~mask;
		}
		if (attr->subscribe_cb) {
			attr->subscribe_cb(event->subscribe.conn_handle,
			    event->subscribe.cur_notify,
			    event->subscribe.cur_indicate);
		}
		break;

	case BLE_GAP_EVENT_MTU:
		adb_log(LOG_DEBUG
		    "mtu update event conn_handle %d cid %d mtu %d",
		    event->mtu.conn_handle,
		    event->mtu.channel_id,
		    event->mtu.value);
		rc = al_bt_conn_mtu_update(event->mtu.conn_handle,
		    event->mtu.value);
		if (rc) {
			adb_log(LOG_ERR "mtu update err %d", rc);
		}
		break;

	case BLE_GAP_EVENT_IDENTITY_RESOLVED:
		adb_log(LOG_DEBUG "identity resolved");
		rc = ble_gap_conn_find(event->identity_resolved.conn_handle,
		    &desc);
		ASSERT(!rc);
		al_bt_print_conn_desc(&desc);
		break;

	case BLE_GAP_EVENT_REPEAT_PAIRING:
		adb_log(LOG_DEBUG "repeat pairing conn_handle %d",
		    event->repeat_pairing.conn_handle);
		rc = ble_gap_conn_find(event->repeat_pairing.conn_handle,
		    &desc);
		ASSERT(!rc);
		if (adb_pairing_mode_get() == ADB_PM_DISABLED) {
			adb_log(LOG_WARN
			    "attempted re-pair while pairing disabled");
			return BLE_GAP_REPEAT_PAIRING_IGNORE;
		}
		ble_store_util_delete_peer(&desc.peer_id_addr);
		return BLE_GAP_REPEAT_PAIRING_RETRY;

	case BLE_GAP_EVENT_NOTIFY_RX:
		adb_log(LOG_DEBUG2
		    "notification rx event attr_handle %d indication %d "
		    "len %d",
		    event->notify_rx.attr_handle,
		    event->notify_rx.indication,
		    OS_MBUF_PKTLEN(event->notify_rx.om));
		break;

	case BLE_GAP_EVENT_NOTIFY_TX:
		adb_log(LOG_DEBUG2
		    "notification tx event status %d attr_handle %d "
		    "indication %d",
		    event->notify_tx.status,
		    event->notify_tx.attr_handle,
		    event->notify_tx.indication);
		break;

	default:
		adb_log(LOG_DEBUG "%s unhandled event %d", __func__,
		    event->type);
		break;
	}

	return 0;
}

/*
 * Handle GATT registration events
 */
static void al_bt_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt,
    void *arg)
{
	char buf[BLE_UUID_STR_LEN];
	const struct adb_attr *service;
	struct adb_service_info *service_info;

	switch (ctxt->op) {
	case BLE_GATT_REGISTER_OP_SVC:
		adb_log(LOG_DEBUG "registered svc %s handle %d",
		    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
		    ctxt->svc.handle);
		/* only services using al_bt APIs will be found */
		service = al_bt_find_service_by_arg(ctxt->svc.svc_def);
		if (service) {
			service_info = service->info;
			service_info->handle = ctxt->svc.handle;
		}
		break;

	case BLE_GATT_REGISTER_OP_CHR:
		adb_log(LOG_DEBUG "registered chr %s "
		    "def_handle %d val_handle %d",
		    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
		ctxt->chr.def_handle, ctxt->chr.val_handle);
		break;

	case BLE_GATT_REGISTER_OP_DSC:
		adb_log(LOG_DEBUG "registered desc %s handle %d",
		    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
		    ctxt->dsc.handle);
		break;

	default:
		adb_log(LOG_DEBUG "%s unhandled op %d", __func__, ctxt->op);
		break;
	}
}

static void al_bt_wifi_event_handler(enum adw_wifi_event_id id, void *arg)
{
	if (id == ADW_EVID_STA_DOWN) {
		/*
		 * STA connection failed. Restart BLE advertising,
		 * if appropriate
		 */
		al_bt_advertise();
	}
}

static void al_bt_client_event_handler(void *arg, enum ada_err err)
{
	if (err) {
		return;		/* client is down */
	}
	al_bt_advertise();	/* client is up */
}

static void al_bt_pairing_cb(TimerHandle_t timer)
{
	adb_pairing_mode_set(ADB_PM_DISABLED, 0);
}

int al_bt_pairing_mode_set(enum adb_pairing_mode mode, u16 duration)
{
	u8 io_cap = BLE_SM_IO_CAP_DISP_ONLY;
	u8 mitm = 1;

	if (!al_bt_enabled()) {
		return -1;
	}

	osTimerStop(al_bt_pairing_timer);

	switch (mode) {
	case ADB_PM_DISABLED:
		ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
		ble_hs_cfg.sm_mitm = 1;
#ifdef __PATCH__
		ble_hs_cfg.sm_no_pairing = 1;	/* no pairing */
#endif
		ble_hs_cfg.sm_bonding = 0;	/* no bonding */
		ble_hs_cfg.sm_our_key_dist = 0;
		ble_hs_cfg.sm_their_key_dist = 0;
		duration = 0;	/* disabled until re-enabled */
		goto done;
	case ADB_PM_NO_PASSKEY:
		io_cap = BLE_SM_IO_CAP_NO_IO;
		mitm = 0;
		break;
	case ADB_PM_CONFIGURED_PASSKEY:
	case ADB_PM_RANDOM_PASSKEY:
	case ADB_PM_AYLA_PASSKEY:
		break;
	default:
		ASSERT_NOTREACHED();
	}

	ble_hs_cfg.sm_io_cap = io_cap;
	ble_hs_cfg.sm_mitm = mitm;
#ifdef __PATCH__
	ble_hs_cfg.sm_no_pairing = 0;	/* pairing enabled */
#endif

#ifdef NIMBLE_NVS_PERSIST
	ble_hs_cfg.sm_bonding = 1;	/* bonding enabled */
#else
	ble_hs_cfg.sm_bonding = 0;	/* no bonding */
#endif
	ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ID |
        BLE_SM_PAIR_KEY_DIST_ENC;
	ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ID |
        BLE_SM_PAIR_KEY_DIST_ENC;
done:
	if (duration) {
		while (1) {
			if (osTimerStart(al_bt_pairing_timer, MS_TO_TICKS(duration * 1000)) != osErrorResource) {
				break;
			}
		}
		adb_log(LOG_DEBUG "pairing mode set to %s for %u seconds",
		    adb_pairing_mode_name_get(), duration);
	} else {
		adb_log(LOG_DEBUG "pairing mode set to %s indefinitely",
		    adb_pairing_mode_name_get());
	}
	return 0;
}

int
al_bt_store_event_handler(struct ble_store_status_event *event, void *arg)
{
	switch (event->event_code) {
	case BLE_STORE_EVENT_OVERFLOW:
		switch (event->overflow.obj_type) {
		case BLE_STORE_OBJ_TYPE_OUR_SEC:
		case BLE_STORE_OBJ_TYPE_PEER_SEC:
			return al_bt_delete_old_peer(NULL);
		case BLE_STORE_OBJ_TYPE_CCCD:
			/* Delete peer records for a peer other than current. */
			return al_bt_delete_old_peer
				(&event->overflow.value->cccd.peer_addr);

		default:
			return BLE_HS_EUNKNOWN;
		}

	case BLE_STORE_EVENT_FULL:
		/*
		 * Just proceed with the operation, Now.
		 * we'll delete a record when the overflow occurs.
		 */
		return 0;

	default:
		return BLE_HS_EUNKNOWN;
	}
}

#ifdef AYLA_LOCAL_CONTROL_SUPPORT
static void al_bt_lanip_update_handler(void)
{
	if (!client_lctrl_is_enabled()) {
		al_bt_keep_up_clear(AL_BT_FUNC_LOCAL_CONTROL);
		return;
	}
	al_bt_keep_up_set(AL_BT_FUNC_LOCAL_CONTROL);
	if (al_bt_deinited) {
		adb_log(LOG_WARN "reset required to restart Bluetooth");
		return;
	}

	/*
	 * Update advertised random ID based on new LAN IP key
	 */
	adb_ayla_svc_random_id_update();
	al_bt_advertise();
}
#endif

#if 0 /* removed in 3.2.3 */
static void al_bt_conf_reset(void)
{
	if (!al_bt_deinited) {
		ble_store_clear();
	}
}
#endif

/*
 * Initialize and start the Bluetooth subsystem
 */
void al_bt_init(void (register_cb)(void))
{
	u8 addr[6];
	char device_name[BT_NAME_SIZE_MAX + 1];
	char passkey_str[BT_PASSKEY_CHARS + 1];
	int len;
	s32 value;
	int i;
	unsigned long bytes;
	char *ptr;
	char *hex_format = "%02x";
	ble_addr_t peer_addrs[BT_NIMBLE_MAX_BONDS];
	ble_addr_t *peer;
	int num_peers = 0;
	char addr_str[20];
	int sync_wait_count = 0;
	osTimerAttr_t timer_attr;
	osThreadAttr_t thread_attr;

	if (al_bt_deinited || al_bt_inited) {
		return;
	}

	al_bt_inited = 1;

#ifndef AYLA_LOCAL_CONTROL_SUPPORT
	if (adw_wifi_configured()) {
		adb_log(LOG_INFO "Wi-Fi configured, Bluetooth disabled");
		al_bt_deinit();
		return;
	}
#endif

	memset(&timer_attr, 0, sizeof(timer_attr));
	timer_attr.name = "btAL";
	al_bt_timer = osTimerNew(al_bt_timer_cb, osTimerOnce, NULL, &timer_attr);
	ASSERT(al_bt_timer);

	memset(&timer_attr, 0, sizeof(timer_attr));
	timer_attr.name = "btPairing";
	al_bt_pairing_timer = osTimerNew(al_bt_timer_cb, osTimerOnce, NULL, &timer_attr);
	ASSERT(al_bt_pairing_timer);

	/*
	 * nimble_port_init() already initialized by the soc initialization
	 * our initialization include both host and controller
	 */

	/* Initialize the NimBLE host configuration. */
	ble_hs_cfg.reset_cb = al_bt_on_reset;
	ble_hs_cfg.sync_cb = al_bt_on_sync;
	ble_hs_cfg.gatts_register_cb = al_bt_gatt_register_cb;
	ble_hs_cfg.store_status_cb = al_bt_store_event_handler;

	/*
	 * Read passkey from config.
	 */
	len = conf_persist_get(BT_PASSKEY_KEY_CONF, passkey_str,
	    sizeof(passkey_str) - 1);
	if (len > 0) {
		passkey_str[len] = '\0';
		value = strtol(passkey_str, &ptr, 10);
		if (ptr != passkey_str && value >= BT_PASSKEY_MIN &&
		    value <= BT_PASSKEY_MAX) {
			al_bt_configured_passkey = value;
		}
	}

	/* Initialize nimble BLE subsystems */
	ble_svc_gap_init();
	ble_svc_gatt_init();
	ble_store_config_init();

	if (log_mod_sev_is_enabled(MOD_LOG_BT, LOG_SEV_DEBUG)) {
		al_bt_store_counts_log();
		ble_store_util_bonded_peers(peer_addrs, &num_peers,
			ARRAY_LEN(peer_addrs));
		for (i = 0, peer = peer_addrs; i < num_peers; peer++, i++) {
			adb_snprint_addr(peer->val, addr_str,
			    sizeof(addr_str));
			adb_log(LOG_DEBUG "bond[%d]: %s", i, addr_str);
		}
	}

	al_adb_semaphore = osSemaphoreNew(1, 0, NULL);
	ASSERT(al_adb_semaphore);
	al_bt_mutex = osSemaphoreNew(1, 0, NULL);
	osSemaphoreRelease(al_bt_mutex);

	if (register_cb) {
		register_cb();
	}

	memset(&thread_attr, 0, sizeof(thread_attr));
	thread_attr.name = "ble_host";
	thread_attr.priority = osPriorityNormal;
	thread_attr.stack_size = CONFIG_DEFAULT_STACK_SIZE;
	/* only the run host task as controller task is initialized internally */
	host_task_h = osThreadNew(al_bt_host_task, NULL, &thread_attr);
	ASSERT(host_task_h != NULL);

	/*
	 * Wait for the controller to sync before issuing
	 * subsequent ble_ calls.
	 */
	while (!al_bt_controller_syncd) {
		osDelay(100 / TICK_PERIOD_MS);
		ASSERT(++sync_wait_count < 50);
	}

	/*
	 * Reset the BT NV storage if the conf item isn't present to suppress
	 * it. After a factory reset, the conf item is cleared
	 * such that the was-reset flag will be set to 1 (compiled in value)
	 * after it reset. It is done this way because the Nimble NV APIs
	 * can't be called after a BT shutdown, so they are called during
	 * init when BT is up.
	 */
	conf_persist_get(BT_RESET_CONF, &al_bt_conf_was_reset,
	    sizeof(al_bt_conf_was_reset));
	if (al_bt_conf_was_reset) {
		/*
		 * Suppress reset if the system config wasn't reset so the
		 * BT conf doesn't get reset after an OTA from a version
		 * that didn't handle BT reset this way.
		 */
#if 0 /* removed in 3.2.3 */
		if (conf_was_reset) {
			al_bt_conf_reset();
		}
#endif
		al_bt_conf_was_reset = 0;
		if (!mfg_or_setup_mode_active()) {
			conf_persist_set(BT_RESET_CONF, &al_bt_conf_was_reset,
			    sizeof(al_bt_conf_was_reset));
		} else {
			conf_persist_set(BT_RESET_CONF, NULL, 0);
		}
	}

	/* generate and set device name */
	len = conf_persist_get(BT_NAME_KEY_CONF, device_name,
	    sizeof(device_name) - 1);
	if (len <= 0) {
		/* use default */
		strncpy(device_name, BT_NAME_DEFAULT_FORMAT,
		    sizeof(device_name));
	} else {
		device_name[len] = '\0';
	}
	ptr = strchr(device_name, '%');
	if (ptr) {
		len = ptr - device_name;
		*ptr++ = '\0';
		bytes = strtoul(ptr, &ptr, 10) / 2;
		if (bytes > 6) {
			bytes = 0;
		}
		if (ptr && *ptr == 'X') {
			hex_format = "%02X";
		}
		ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, addr, NULL);
		i = (int)bytes - 1;
		while (i >= 0 && len < sizeof(device_name) - 1) {
			snprintf(&device_name[len], sizeof(device_name) - len,
			    hex_format, addr[i]);
			--i;
			len += 2;
		}
	}
	ble_svc_gap_device_name_set(device_name);

	/* register for Wi-Fi events to restart advertising when needed */
	adw_wifi_event_register(al_bt_wifi_event_handler, NULL);
	ada_client_event_register(al_bt_client_event_handler, NULL);

#ifdef AYLA_LOCAL_CONTROL_SUPPORT
	/* register for lanip update events */
	ada_client_lanip_cb_register(al_bt_lanip_update_handler);
#endif

	/* set the initial pairing mode */
	if (!adw_wifi_configured()) {
		al_bt_wifi_provision_start();
	} else {
		/* no pairing until application enables it */
		adb_pairing_mode_set(ADB_PM_DISABLED, 0);
		al_bt_advertise();
	}
	al_bt_init_complete = 1;
}

static int al_bt_uuid_cmp(const ble_uuid_any_t *uuid1,
    const ble_uuid_any_t *uuid2)
{
	if (uuid1->u.type != uuid2->u.type) {
		return -1;
	}

	switch (uuid1->u.type) {
	case BLE_UUID_TYPE_16:
		return uuid1->u16.value == uuid2->u16.value;
	case BLE_UUID_TYPE_128:
		return memcmp(&uuid1->u128.value, &uuid2->u128.value, 16);
	default:
		return -1;
	}
}

const struct adb_attr *al_bt_find_attr_by_uuid(const struct adb_attr *parent,
    const void *uuid)
{
	const struct adb_attr *attr = parent + 1;
	const ble_uuid_any_t *ble_uuid = uuid;

	while (attr->type && (attr->type != parent->type)) {
		if (!al_bt_uuid_cmp(attr->uuid, ble_uuid)) {
			return attr;
		}
		attr++;
	}
	return NULL;
}

const struct adb_attr *al_bt_find_attr_by_handle(u16 handle)
{
	const struct adb_attr **services = services_table;
	const struct adb_attr *attr;
	struct adb_service_info *service_info;
	struct adb_chr_info *chr_info;

	while (*services) {
		attr = *services;
		while (attr->type) {
			switch (attr->type) {
			case ADB_ATTR_SERVICE:
				service_info = attr->info;
				if (service_info->handle == handle) {
					return attr;
				}
				break;
			case ADB_ATTR_CHR:
				chr_info = attr->info;
				if (chr_info->handle == handle) {
					return attr;
				}
				break;
			default:
				break;
			}
			attr++;
		}
		services++;
	}
	return NULL;
}

static int al_bt_add_service(const struct adb_attr *service)
{
	int i;
	const struct adb_attr **services = services_table;

	/* last entry must always be left NULL */
	for (i = 0; i < ARRAY_LEN(services_table) - 1; i++) {
		if (!*services) {
			break;
		}
		services++;
	}

	if (*services) {
		adb_log(LOG_ERR "service table full");
		return -1;
	}
	*services = service;

	return 0;
}

const struct adb_attr *al_bt_find_service_by_arg(const void *pfm_arg)
{
	const struct adb_attr **services = services_table;
	const struct adb_attr *service;
	struct adb_service_info *service_info;

	while (*services) {
		service = *services;
		service_info = service->info;
		if (pfm_arg == service_info->pfm_arg) {
			return service;
		}
		services++;
	}

	return NULL;
}

void al_bt_clear_notify_indicate(u8 conn_mask)
{
	const struct adb_attr **services = services_table;
	const struct adb_attr *attr;
	struct adb_chr_info *chr_info;

	conn_mask = ~conn_mask;
	while (*services) {
		attr = *services;
		while (attr->type) {
			if (attr->type == ADB_ATTR_CHR) {
				chr_info = attr->info;
				chr_info->notify_mask &= conn_mask;
				chr_info->indicate_mask &= conn_mask;
			}
			attr++;
		}
		services++;
	}
}

static struct adb_conn *al_bt_connection_find(u16 handle, u8 *index)
{
	struct adb_conn *adb_conn;
	u8 i = 0;

	ASSERT(al_bt_locked);
	for (adb_conn = adb_connections; adb_conn < ARRAY_END(adb_connections);
	    adb_conn++) {
		if (adb_conn->active && adb_conn->handle == handle) {
			if (index) {
				*index = i;
			}
			return adb_conn;
		}
		i++;
	}
	return NULL;
}

int al_bt_connection_add(u16 handle)
{
	struct adb_conn *conn;

	for (conn = adb_connections; conn < ARRAY_END(adb_connections);
	    conn++) {
		if (!conn->active) {
			conn->active = 1;
			conn->handle = handle;
			conn->mtu = ADB_GATT_MTU_DEFAULT;
			return 0;
		}
	}
	return -1;
}

int al_bt_connection_delete(u16 handle)
{
	u8 index;
	struct adb_conn *adb_conn;

	al_bt_lock();
	adb_conn = al_bt_connection_find(handle, &index);
	if (!adb_conn) {
		al_bt_unlock();
		return -1;
	}
	adb_conn->active = 0;
	adb_conn->ctxt = NULL;
	al_bt_unlock();
	al_bt_clear_notify_indicate(1 << index);
	return 0;
}

int al_bt_connection_terminate(u16 handle)
{
	int rc;

	rc = ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM);
	if (rc) {
		adb_log(LOG_ERR "conn terminate err 0x%x", rc);
		return -1;
	}
	return 0;
}

u8 al_bt_connection_mask(u16 handle)
{
	u8 index;
	struct adb_conn *adb_conn;
	u8 mask = 0;

	al_bt_lock();
	adb_conn = al_bt_connection_find(handle, &index);
	if (adb_conn) {
		mask = 1 << index;
	}
	al_bt_unlock();
	return mask;
}

int al_bt_connection_next(u16 *handle, u16 *cookie)
{
	struct adb_conn *adb_conn;
	int rc = -1;
	int index = *cookie;

	while (index < ARRAY_LEN(adb_connections)) {
		adb_conn = &adb_connections[index++];
		if (adb_conn->active) {
			*handle = adb_conn->handle;
			rc = 0;
			break;
		}
	}

	*cookie = index;
	return rc;
}

int al_bt_connection_ctxt_set(u16 handle, void *ctxt)
{
	struct adb_conn *adb_conn;
	int rc = -1;

	al_bt_lock();
	adb_conn = al_bt_connection_find(handle, NULL);
	if (adb_conn) {
		adb_conn->ctxt = ctxt;
		rc = 0;
	}
	al_bt_unlock();
	return rc;
}

void *al_bt_connection_ctxt_get(u16 handle)
{
	struct adb_conn *adb_conn;
	void *ctxt = NULL;

	al_bt_lock();
	adb_conn = al_bt_connection_find(handle, NULL);
	if (adb_conn) {
		ctxt = adb_conn->ctxt;
	}
	al_bt_unlock();
	return ctxt;
}

static int al_bt_conn_mtu_update(u16 handle, u16 mtu)
{
	struct adb_conn *adb_conn;
	int rc = -1;

	al_bt_lock();
	adb_conn = al_bt_connection_find(handle, NULL);
	if (adb_conn) {
		adb_conn->mtu = mtu;
		rc = 0;
	}
	al_bt_unlock();
	return rc;
}

int al_bt_conn_mtu_get(u16 handle, u16 *mtu)
{
	struct adb_conn *adb_conn;
	int rc = -1;

	al_bt_lock();
	adb_conn = al_bt_connection_find(handle, NULL);
	if (adb_conn) {
		*mtu = adb_conn->mtu;
		rc = 0;
	}
	al_bt_unlock();
	return rc;
}

static int al_bt_notify1(const struct adb_attr *bt_chr, u8 *buf, u16 length,
    u16 conn, u8 indicate)
{
	struct adb_chr_info *chr_info = bt_chr->info;
	int rc;
	struct os_mbuf *om;
	char *type_str;
	char *prefix;

	if (al_bt_deinited) {
		return -1;
	}
	if (indicate) {
		type_str = "indicate";
		prefix = "bt_in: ";
	} else {
		type_str = "notify";
		prefix = "bt_nf: ";
	}
	om = ble_hs_mbuf_from_flat(buf, length);
	if (!om) {
		adb_log(LOG_ERR "%s chr %s mbuf alloc failure", type_str,
		    bt_chr->name);
		return -1;
	}
	adb_log(LOG_DEBUG "%s %s conn %d", type_str, bt_chr->name, conn);
	adb_dump_log(prefix, buf, length, bt_chr->read_redact_cb);
	if (indicate) {
		rc = ble_gattc_indicate_custom(conn, chr_info->handle, om);
	} else {
		rc = ble_gattc_notify_custom(conn, chr_info->handle, om);
	}
	if (rc) {
		adb_log(LOG_ERR "%s %s error %d", type_str, bt_chr->name,
		    rc);
	}
	return rc;
}

int al_bt_notify_conn(u16 handle, const struct adb_attr *bt_chr, u8 *buf,
    u16 length)
{
	struct adb_chr_info *chr_info = bt_chr->info;
	u8 index;
	u8 mask;
	u16 pdu_length = length + ADB_NOTIFY_OVERHEAD;
	u16 mtu;
	struct adb_conn *adb_conn;

	al_bt_lock();
	adb_conn = al_bt_connection_find(handle, &index);
	if (!adb_conn) {
		al_bt_unlock();
		return -1;
	}
	mtu = adb_conn->mtu;
	al_bt_unlock();

	if (pdu_length > mtu) {
		adb_log(LOG_WARN
		    "chr %s handle %d pdu_length %d exceeds MTU %d conn %d",
		    bt_chr->name, chr_info->handle, pdu_length, mtu, handle);
		return -1;
	}
	mask = 1 << index;
	if (chr_info->notify_mask & mask) {
		al_bt_notify1(bt_chr, buf, length, handle, 0);
	}
	if (chr_info->indicate_mask & mask) {
		al_bt_notify1(bt_chr, buf, length, handle, 1);
	}
	return 0;
}

int al_bt_notify(const struct adb_attr *bt_chr, u8 *buf, u16 length)
{
	struct adb_conn *adb_conn = adb_connections;
	struct adb_chr_info *chr_info = bt_chr->info;
	u8 notify_mask = chr_info->notify_mask;
	u8 indicate_mask = chr_info->indicate_mask;
	u8 mask = notify_mask | indicate_mask;
	u16 pdu_length = length + ADB_NOTIFY_OVERHEAD;

	if (!mask) {
		return 0;
	}
	while (mask && adb_conn < ARRAY_END(adb_connections)) {
		if (adb_conn->active) {
			if (pdu_length > adb_conn->mtu) {
				adb_log(LOG_WARN
				    "chr %s handle %d pdu_length %d "
				    "exceeds MTU %d conn %d",
				    bt_chr->name, chr_info->handle, pdu_length,
				    adb_conn->mtu, adb_conn->handle);
			} else {
				if (notify_mask & 0x1) {
					al_bt_notify1(bt_chr, buf, length,
					    adb_conn->handle, 0);
				}
				if (indicate_mask & 0x1) {
					al_bt_notify1(bt_chr, buf, length,
					    adb_conn->handle, 1);
				}
			}
		}
		notify_mask >>= 1;
		indicate_mask >>= 1;
		mask >>= 1;
		adb_conn++;
	}
	return 0;
}

void al_bt_wait(void)
{
	osSemaphoreAcquire(al_adb_semaphore, osWaitForever);
}

void al_bt_wakeup(void)
{
	osSemaphoreRelease(al_adb_semaphore);
}

void al_bt_lock(void)
{
	osSemaphoreAcquire(al_bt_mutex, osWaitForever);
	ASSERT(!al_bt_locked);
	al_bt_locked = 1;
}

void al_bt_unlock(void)
{
	ASSERT(al_bt_locked);
	al_bt_locked = 0;
	osSemaphoreRelease(al_bt_mutex);
}

static int al_ble_store_clean(void)
{
    /* XXX: what to do? */
	return 0;
}

/*
 * No longer needed, but kept for source compatibility.
 * The entire startup configuration will be erased during factory reset.
 */
void al_bt_conf_factory_reset(void)
{
	adb_log(LOG_INFO "%s: Delete BLE store data", __func__);
	al_ble_store_clean();
}

static int al_bt_gatt_chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	const struct adb_attr *bt_chr = arg;
	int rc;
	u8 *buf;
	u16 length = ADB_GATT_ATT_MAX_LEN;

	buf = al_os_mem_alloc(length);
	if (!buf) {
		return BLE_ATT_ERR_INSUFFICIENT_RES;
	}
	switch (ctxt->op) {
	case BLE_GATT_ACCESS_OP_READ_CHR:
		adb_log(LOG_DEBUG "read %s conn %d ", bt_chr->name,
		    conn_handle);
		if (!bt_chr->read_cb) {
			adb_log(LOG_ERR "no read_cb for %s", bt_chr->name);
			rc = BLE_ATT_ERR_UNLIKELY;
			break;
		}
		rc = bt_chr->read_cb(conn_handle, bt_chr, buf, &length);
		if (rc) {
			break;
		}
		adb_dump_log("bt_rd: ", buf, length, bt_chr->read_redact_cb);
		rc = os_mbuf_append(ctxt->om, buf, length);
		/* convert to BLE error numbers */
		rc = rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
		break;
	case BLE_GATT_ACCESS_OP_WRITE_CHR:
		adb_log(LOG_DEBUG "write %s conn %d", bt_chr->name,
		    conn_handle);
		rc = ble_hs_mbuf_to_flat(ctxt->om, buf, length, &length);
		if (rc) {
			break;
		}
		adb_dump_log("bt_wr: ", buf, length, bt_chr->write_redact_cb);
		if (!bt_chr->write_cb) {
			adb_log(LOG_ERR "no write_cb for %s", bt_chr->name);
			rc = BLE_ATT_ERR_UNLIKELY;
			break;
		}
		rc = bt_chr->write_cb(conn_handle, bt_chr, buf, length);
		break;
	default:
		adb_log(LOG_DEBUG "%s unhandled op %d", __func__, ctxt->op);
		rc = BLE_ATT_ERR_UNLIKELY;
		break;
	}

	al_os_mem_free(buf);
	if (rc < 0 || rc > BLE_ATT_ERR_INSUFFICIENT_RES) {
		rc = BLE_ATT_ERR_UNLIKELY;
	}
	return rc;
}

int al_bt_register_service(const struct adb_attr *service)
{
	struct ble_gatt_svc_def *ble_service;
	struct ble_gatt_chr_def *ble_chr;
	struct adb_service_info *service_info = service->info;
	const struct adb_attr *attr = service + 1;
	struct adb_chr_info *chr_info;
	u16 chr_count = 0;
	u16 desc_count = 0;

	if (al_bt_add_service(service)) {
		return -1;
	}

	while (attr->type) {
		switch (attr->type) {
		case ADB_ATTR_CHR:
			chr_count++;
			break;
		case ADB_ATTR_DESC:
			desc_count++;	/* 1 handle per descriptor */
			break;
		default:
			break;		/* should never happen */
		}
		attr++;
	}

	adb_log(LOG_DEBUG "registering service %s chrs %u descs %u",
	    service->name, chr_count, desc_count);
	ble_service = al_os_mem_calloc(2 * sizeof(struct ble_gatt_svc_def));
	ASSERT(ble_service);
	service_info->pfm_arg = ble_service;
	ble_service->type = service_info->is_primary;
	ble_service->uuid = (ble_uuid_t *)service->uuid;

	ble_chr = al_os_mem_calloc((chr_count + 1) *
	    sizeof(struct ble_gatt_chr_def));
	ASSERT(ble_chr);
	ble_service->characteristics = ble_chr;

	attr = service + 1;
	while (attr->type) {
		if (attr->type == ADB_ATTR_CHR) {
			chr_info = attr->info;
			chr_info->pfm_arg = ble_chr;
			ble_chr->uuid = (ble_uuid_t *)attr->uuid;
			ble_chr->access_cb = al_bt_gatt_chr_access_cb;
			ble_chr->arg = (void *)attr, /* drop const */
			ble_chr->flags = (ble_gatt_chr_flags)attr->access;
			ble_chr->val_handle = &chr_info->handle;
			ble_chr++;
		}
		attr++;
	}

	int rc = ble_gatts_count_cfg(ble_service);
	ASSERT(!rc);

	rc = ble_gatts_add_svcs(ble_service);
	ASSERT(!rc);

	return 0;
}

#endif /* AYLA_BLUETOOTH_SUPPORT */

/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 */

#ifdef AYLA_BLUETOOTH_SUPPORT

#include <stddef.h>

#include <ayla/utypes.h>
#include <ayla/timer.h>
#include <ayla/log.h>
#include <ada/libada.h>
#include <adb/adb.h>
#include <adb/al_bt.h>
#include <adb/adb_ayla_svc.h>
#include <adb/adb_conn_svc.h>
#include <ada/generic_session.h>
#include <adb/adb_mbox_svc.h>
#include <adb/adb_wifi_cfg_svc.h>

#include "demo.h"

#ifdef AYLA_LOCAL_CONTROL_SUPPORT_WLT
#include <string.h>
#include <ayla/callback.h>
#include "client_timer.h"

#define WLT_BT_NAME_DEFAULT_FORMAT	"HYD00-01WALLWW %2X"

/* WLT BLE GATT Service */
static const AL_BT_UUID16(wlt_svc_uuid, 0xFFF0);
static const AL_BT_UUID16(wlt_inbox_uuid, 0xFFF3);
static const AL_BT_UUID16(wlt_outbox_uuid, 0xFFF4);

static struct adb_service_info wlt_svc = {.is_primary = 1,};
static struct adb_chr_info wlt_inbox_info;
static struct adb_chr_info wlt_outbox_info;

static const struct adb_attr wlt_svc_table[] = {
	ADB_SERVICE("wlt_svc", &wlt_svc_uuid, &wlt_svc),
	/* dynamic means need indicate or notify maybe.... */
	ADB_CHR("wlt_inbox", &wlt_inbox_uuid, AL_BT_AF_READ | AL_BT_AF_WRITE_NR ,NULL, adb_mbox_svc_write_cb, &wlt_inbox_info),
	ADB_CHR_SUB("wlt_outbox", &wlt_outbox_uuid, AL_BT_AF_NOTIFY , NULL, NULL, adb_mbox_svc_subscribe_cb, &wlt_outbox_info),
	ADB_SERVICE_END()
};

/* Only support one session */
static struct generic_session wlt_bt_lctrl_session;
static struct callback wlt_bt_lctrl_cb;

static void wlt_bt_lctrl_ev_hdlr(void *arg)
{
	struct generic_session *gs = &wlt_bt_lctrl_session;

    /* TODO: Implement WLT private protocols */
    printf("[TODO]: Implement private protocols!!!\n");
	al_os_mem_free(gs->rx_buffer);
	gs->rx_buffer = NULL;
	gs->rx_length = 0;
}

static struct generic_session *wlt_bt_lctrl_session_alloc(void)
{
    struct generic_session *gs = &wlt_bt_lctrl_session;

	if (gs->active) {
		return NULL;
	}
	gs->active = 1;
	gs->fragment = 1;

	return gs;
}

static void wlt_bt_lctrl_session_down(struct generic_session *gs)
{
    ASSERT(gs == &wlt_bt_lctrl_session);
	generic_session_close(gs);
}

static enum ada_err wlt_bt_lctrl_rx(struct generic_session *gs, const u8 *buf, u16 length)
{
    if (!gs->active) {
		return AE_INVAL_STATE;
	}
	if (gs->rx_buffer) {
		return AE_BUSY;
	}
	gs->rx_buffer = al_os_mem_alloc(length);
	if (!gs->rx_buffer) {
		return AE_ALLOC;
	}
	memcpy(gs->rx_buffer, buf, length);
	gs->rx_length = length;

	client_callback_pend(&wlt_bt_lctrl_cb);

	return AE_OK;
}

static void wlt_bt_lctrl_init(void)
{
	callback_init(&wlt_bt_lctrl_cb, wlt_bt_lctrl_ev_hdlr, NULL);
}
#endif

static int demo_bt_provisioning;

int demo_bt_is_provisioning(void)
{
	return demo_bt_provisioning;
}

static void demo_bt_adb_event_handler(enum adb_event event)
{
	log_put(LOG_INFO "%s: event %d\n", __func__, event);

	switch (event) {
	case ADB_EV_WIFI_PROVISION_START:
		demo_bt_provisioning = 1;
		break;
	case ADB_EV_WIFI_PROVISION_STOP:
		demo_bt_provisioning = 0;
		break;
	default:
		break;
	}
}

void demo_bt_register_services_cb(void)
{
	adb_ayla_svc_identify_cb_set(demo_identify_cb);
	adb_ayla_svc_register(NULL);
	adb_conn_svc_register(NULL);
#if defined(AYLA_LOCAL_CONTROL_SUPPORT) || defined(AYLA_TEST_SERVICE_SUPPORT)
#ifdef AYLA_LOCAL_CONTROL_SUPPORT_WLT
    const struct adb_attr *svc_ptr = wlt_svc_table;
	adb_mbox_svc_register(&svc_ptr);
#else
	adb_mbox_svc_register(NULL);
#endif
#endif
	adb_wifi_cfg_svc_register(NULL);
}

/*
 * Initialize and start the Bluetooth demo
 */
void demo_bt_init(void)
{
#ifdef AYLA_LOCAL_CONTROL_SUPPORT_WLT
    u8 feat_flags = 0 | BIT(AL_BT_FEAT_EXTRA_LCTRL) | BIT(AL_BT_FEAT_BT_NAME_FMT) 
                    | BIT(AL_BT_FEAT_KEEP_BT_ADV) | BIT(AL_BT_FEAT_KEEP_NAME_IN_ADV);

    wlt_bt_lctrl_init();

    adb_mbox_svc_callbacks_set(wlt_bt_lctrl_session_alloc, wlt_bt_lctrl_session_down,
        wlt_bt_lctrl_rx);

    al_bt_set_features(feat_flags, WLT_BT_NAME_DEFAULT_FORMAT);
#endif
	adb_event_register(demo_bt_adb_event_handler);
	al_bt_init(demo_bt_register_services_cb);
}
#endif

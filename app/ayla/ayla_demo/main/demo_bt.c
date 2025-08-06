/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 */

#ifdef AYLA_BLUETOOTH_SUPPORT

#include <stddef.h>

#include <ayla/utypes.h>
#include <ayla/timer.h>
#include <ada/libada.h>
#include <adb/adb.h>
#include <adb/al_bt.h>
#include <adb/adb_ayla_svc.h>
#include <adb/adb_conn_svc.h>
#include <ada/generic_session.h>
#include <adb/adb_mbox_svc.h>
#include <adb/adb_wifi_cfg_svc.h>

#include "demo.h"

static int demo_bt_provisioning;

int demo_bt_is_provisioning(void)
{
	return demo_bt_provisioning;
}

static void demo_bt_adb_event_handler(enum adb_event event)
{
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
	adb_mbox_svc_register(NULL);
#endif
	adb_wifi_cfg_svc_register(NULL);
}

/*
 * Initialize and start the Bluetooth demo
 */
void demo_bt_init(void)
{
	adb_event_register(demo_bt_adb_event_handler);
	al_bt_init(demo_bt_register_services_cb);
}
#endif

/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

#ifdef AYLA_BLUETOOTH_SUPPORT

#include <stddef.h>
#include <string.h>

#include <ayla/log.h>
#include <sys/types.h>
#include <adb/adb.h>
#include <al/al_bt.h>
#include <adb/adb_ayla_svc.h>
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "services/gap/ble_svc_gap.h"

const char al_bt_cli_help[] =
    "bt [show]";

static int al_bt_store_iter_print(int obj_type, union ble_store_value *val,
    void *arg)
{
	struct ble_store_value_sec *sec;
	struct ble_store_value_cccd *cccd;
	char addr_str[20];
	int *count = arg;

	switch (obj_type) {
	case BLE_STORE_OBJ_TYPE_OUR_SEC:
	case BLE_STORE_OBJ_TYPE_PEER_SEC:
		sec = &val->sec;
		adb_snprint_addr(sec->peer_addr.val, addr_str,
		    sizeof(addr_str));
		printcli(
		    "%d: %s key-size %u ltk %u irk %u csrk %u auth %u sc %u",
		    *count, addr_str, sec->key_size, sec->ltk_present,
		    sec->irk_present, sec->csrk_present, sec->authenticated,
		    sec->sc);
		break;
	case BLE_STORE_OBJ_TYPE_CCCD:
		cccd = &val->cccd;
		adb_snprint_addr(cccd->peer_addr.val, addr_str,
		    sizeof(addr_str));
		printcli("%d: %s handle %d flags 0x%04x changed %d",
		    *count, addr_str, cccd->chr_val_handle, cccd->flags,
		    cccd->value_changed);
		break;
	default:
		break;
	}
	++(*count);

	return 0;
}

static void al_bt_show_store(void)
{
	int index;

	printcli("\n--- records in persisted storage ---");
	printcli("Our security:");
	index = 0;
	ble_store_iterate(BLE_STORE_OBJ_TYPE_OUR_SEC,
	    al_bt_store_iter_print, &index);

	printcli("\nPeer security:");
	index = 0;
	ble_store_iterate(BLE_STORE_OBJ_TYPE_PEER_SEC,
	    al_bt_store_iter_print, &index);

	printcli("\nCCCD:");
	index = 0;
	ble_store_iterate(BLE_STORE_OBJ_TYPE_CCCD,
	    al_bt_store_iter_print, &index);
}

static void al_bt_show_connections(void)
{
	const char *pstring;
	int index = 0;
	u8 none_found = 1;
	u16 cookie = 0;
	u16 handle;
	int rc;
	struct ble_gap_conn_desc desc;
	char addr_str[20];
	uint8_t addr_val[6] = { 0 };
	uint8_t own_addr_type;

	pstring = "unknown";
	rc = ble_hs_id_infer_auto(0, &own_addr_type);
	if (!rc) {
		rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
		if (!rc) {
			adb_snprint_addr(addr_val, addr_str, sizeof(addr_str));
			pstring = addr_str;
		}
	}
	printcli("Device address: %s", pstring);
	pstring = ble_svc_gap_device_name();
	printcli("Device name: %s", pstring ? pstring : "n/a");
	pstring = adb_ayla_svc_ayla_id_get();
	printcli("Ayla id: %s", pstring ? pstring : "n/a");
	printcli("BLE advertisements: %s",
	    ble_gap_adv_active() ? "active" : "inactive");
	printcli("Pairing mode: %s", adb_pairing_mode_name_get());

	printcli("\nActive connections:");
	while (1) {
		rc = al_bt_connection_next(&handle, &cookie);
		if (rc) {
			break;
		}
		none_found = 0;
		if (ble_gap_conn_find(handle, &desc)) {
			printcli("%d: handle %u desc not found", index,
			    handle);
		} else {
			adb_snprint_addr(desc.peer_id_addr.val, addr_str,
			    sizeof(addr_str));
			printcli("%d: handle %u peer %s", index, handle,
			    addr_str);
		}
		index++;
	}
	if (none_found) {
		printcli("none");
	}
}

static void al_bt_show(void)
{
	if (!al_bt_enabled()) {
		printcli("Bluetooth not enabled");
		return;
	}

	al_bt_show_connections();
	al_bt_show_store();
}

/*
 * 'conf' CLI
 * Show the configuration.
 */
wise_err_t al_bt_cli(int argc, char **argv)
{
	if (argc == 1 || (argc == 2 && !strcmp(argv[1], "show"))) {
		al_bt_show();
		return 0;
	}
	printcli("usage: %s", al_bt_cli_help);
	return 0;
}
#endif

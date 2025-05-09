/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "services/gatt/ble_svc_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "btshell.h"

enum attr_handles {
	HANDLE_GAPS_DEVICE_NAME,
	HANDLE_GAPS_APPEARANCE,
	HANDLE_GAPS_PPCP,
	HANDLE_GATTS_SERVICE_CHANGED,
	HANDLE_GATTS_B004,
	HANDLE_MAX,
};

uint16_t char_handles[HANDLE_MAX];

static char *name = "Test Database";
static uint16_t appearance = 17;
static uint16_t ppcp[4] = {
   htole16(100),
   htole16(200),
   htole16(0),
   htole16(2000),
};

struct small_db_data {
	uint8_t svr_gatt_chr2;
};

struct small_db_data small_db = {
	.svr_gatt_chr2 = 0x04,
};

static int
gatt_svr_access(uint16_t conn_handle, uint16_t attr_handle,
				   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	uint16_t uuid16;
	int rc;

	uuid16 = ble_uuid_u16(ctxt->chr->uuid);
	assert(uuid16 != 0);

	switch (uuid16) {
	case BLE_SVC_GAP_CHR_UUID16_DEVICE_NAME:
		rc = os_mbuf_append(ctxt->om, name, strlen(name));
		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

	case BLE_SVC_GAP_CHR_UUID16_APPEARANCE:
		rc = os_mbuf_append(ctxt->om, &appearance, sizeof(appearance));
		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

	case BLE_SVC_GAP_CHR_UUID16_PERIPH_PREF_CONN_PARAMS:
		assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
		rc = os_mbuf_append(ctxt->om, &ppcp, sizeof(ppcp));
		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

	case BLE_SVC_GATT_CHR_SERVICE_CHANGED_UUID16:
		return 0;

	case 0xB004:
		rc = os_mbuf_append(ctxt->om, &small_db.svr_gatt_chr2, sizeof(small_db.svr_gatt_chr2));
		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
	default:
		assert(0);
		return BLE_ATT_ERR_UNLIKELY;
	}
}

static const struct ble_gatt_svc_def gatt_svr_svc_gap[] = {
	{
		/*** Service: GAP. */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.start_handle = 1,
		.uuid = BLE_UUID16_DECLARE(BLE_SVC_GAP_UUID16),
		.characteristics = (struct ble_gatt_chr_def[]) { {
			/*** Characteristic: Device Name. */
			.uuid = BLE_UUID16_DECLARE(BLE_SVC_GAP_CHR_UUID16_DEVICE_NAME),
			.access_cb = gatt_svr_access,
			.val_handle = &char_handles[HANDLE_GAPS_DEVICE_NAME],
			.flags = BLE_GATT_CHR_F_READ,
		}, {
			/*** Characteristic: Appearance. */
			.uuid = BLE_UUID16_DECLARE(BLE_SVC_GAP_CHR_UUID16_APPEARANCE),
			.access_cb = gatt_svr_access,
			.val_handle = &char_handles[HANDLE_GAPS_DEVICE_NAME],
			.flags = BLE_GATT_CHR_F_READ,
		}, {
			/*** Characteristic: Peripheral Preferred Connection Parameters. */
			.uuid = BLE_UUID16_DECLARE(BLE_SVC_GAP_CHR_UUID16_PERIPH_PREF_CONN_PARAMS),
			.access_cb = gatt_svr_access,
			.val_handle = &char_handles[HANDLE_GAPS_PPCP],
			.flags = BLE_GATT_CHR_F_READ,
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

static const struct ble_gatt_svc_def gatt_svr_svc_gatt[] = {
	{
		/*** Service: GATT */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.start_handle = 0x0010,
		.uuid = BLE_UUID16_DECLARE(BLE_GATT_SVC_UUID16),
		.characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = BLE_UUID16_DECLARE(BLE_SVC_GATT_CHR_SERVICE_CHANGED_UUID16),
			.access_cb = gatt_svr_access,
			.val_handle = &char_handles[HANDLE_GATTS_SERVICE_CHANGED],
			.flags = BLE_GATT_CHR_F_INDICATE,
		}, {
			/*** Characteristic: Peripheral Preferred Connection Parameters. */
			.uuid = BLE_UUID16_DECLARE(0xB004),
			.access_cb = gatt_svr_access,
			.val_handle = &char_handles[HANDLE_GATTS_B004],
			.flags = BLE_GATT_CHR_F_READ,
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

int
gatt_svr_init_small_db(void)
{
	int rc;

	rc = ble_gatts_reset();
	if (rc != 0) {
		return rc;
	}

	rc = ble_gatts_count_cfg(gatt_svr_svc_gap);
	if (rc != 0) {
		return rc;
	}

	rc = ble_gatts_add_svcs(gatt_svr_svc_gap);
	if (rc != 0) {
		return rc;
	}

	rc = ble_gatts_count_cfg(gatt_svr_svc_gatt);
	if (rc != 0) {
		return rc;
	}

	rc = ble_gatts_add_svcs(gatt_svr_svc_gatt);
	if (rc != 0) {
		return rc;
	}

	rc = ble_gatts_start();
	if (rc != 0) {
		return rc;
	}

	return 0;
}

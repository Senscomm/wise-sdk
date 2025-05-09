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
#include <stdio.h>
#include <string.h>

#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "blehogp.h"
#include "blehogp_svc_dis.h"
#include "blehogp_svc_bas.h"
#include "blehogp_svc_hids.h"
#include "blehogp_svc_scps.h"

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            printf("service " "uuid16 %s handle=%d (%04X)\n",
                ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                ctxt->svc.handle, ctxt->svc.handle);
            break;

        case BLE_GATT_REGISTER_OP_CHR:
            printf("characteristic "
                "uuid16 %s arg %d def_handle=%d (%04X) val_handle=%d (%04X)\n",
                ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                (int)ctxt->chr.chr_def->arg,
                ctxt->chr.def_handle, ctxt->chr.def_handle,
                ctxt->chr.val_handle, ctxt->chr.val_handle);
            break;

        case BLE_GATT_REGISTER_OP_DSC:
            printf("descriptor " "uuid16 %s arg %d handle=%d (%04X)\n",
                ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                (int)ctxt->dsc.dsc_def->arg,
                ctxt->dsc.handle, ctxt->dsc.handle);
            break;
    }
}

int
gatt_svr_init(void)
{
    int rc = 0;

	/* include the default gap service */
    ble_svc_gap_init();
	/* include the default gatt service */
    ble_svc_gatt_init();
	/* include the custom device information service */
	blehogp_svc_dis_init();
	/* include the custom battery service */
	blehogp_svc_bas_init();
	/* include the custom hid service */
	blehogp_svc_hids_init();
	/* include the custom scan parameter service */
	blehogp_svc_scps_init();

	blehogp_svc_bas_battery_level_set(80);

	rc = ble_gatts_start();
	if (rc != 0)
	{
		printf("gatts start error\n");
		return rc;
	}

    return rc;
}

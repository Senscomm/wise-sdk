/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * scpstributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software scpstributed under the License is scpstributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include "sysinit/sysinit.h"
#include "host/ble_hs.h"
#include "blehogp.h"
#include "blehogp_svc_scps.h"

enum attr_handles {
    HANDLE_SCPS_SCAN_INTERVAL_WINDOW,
    HANDLE_SCPS_SCAN_REFRESH,
    HANDLE_SCPS_COUNT,
};

static uint16_t scps_char_handles[HANDLE_SCPS_COUNT];

static struct blehogp_svc_scps_data blehogp_svc_scps_data;

static int
blehogp_svc_scps_access(uint16_t conn_handle, uint16_t attr_handle,
                   struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def blehogp_svc_scps_defs[] = {
    { /*** Service: Device Information Service (DIS). */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_SCPS_UUID16),
        .characteristics = (struct ble_gatt_chr_def[]) { {
        /*** Characteristic: Model Number String */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_SCPS_CHR_UUID16_SCAN_INTERVAL_WINDOW),
            .access_cb = blehogp_svc_scps_access,
            .val_handle = &scps_char_handles[HANDLE_SCPS_SCAN_INTERVAL_WINDOW],
            .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        }, {
        /*** Characteristic: Serial Number String */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_SCPS_CHR_UUID16_SCAN_REFRESH),
            .access_cb = blehogp_svc_scps_access,
            .val_handle = &scps_char_handles[HANDLE_SCPS_SCAN_REFRESH],
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            0, /* No more characteristics in this service */
        }, }
    },

    {
        0, /* No more services. */
    },
};

/**
 * Simple read access callback for the device information service
 * characteristic.
 */
static int
blehogp_svc_scps_access(uint16_t conn_handle, uint16_t attr_handle,
                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
    int rc = BLE_ATT_ERR_UNLIKELY;

    printf("%s: UUID %04X attr %04X arg %d op %d\n",
         __FUNCTION__, uuid16, attr_handle, (int)arg, ctxt->op);

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR &&
        uuid16 == BLE_SVC_SCPS_CHR_UUID16_SCAN_INTERVAL_WINDOW) {
        int max_len = sizeof(blehogp_svc_scps_data);
        uint16_t len;
        rc = ble_hs_mbuf_to_flat(ctxt->om, &blehogp_svc_scps_data, max_len, &len);

        printf("SCPS new interval/window = %04x/%04x\n",
               blehogp_svc_scps_data.interval,
               blehogp_svc_scps_data.window);
    }

    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

/**
 * Initialize the DIS package.
 */
void
blehogp_svc_scps_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = ble_gatts_count_cfg(blehogp_svc_scps_defs);
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = ble_gatts_add_svcs(blehogp_svc_scps_defs);
    SYSINIT_PANIC_ASSERT(rc == 0);
}


#ifdef CONFIG_CMDLINE

#include "cli.h"

extern uint16_t g_conn_handle;

int
do_ble_send_scan_refresh(int argc, char *argv[])
{
    struct os_mbuf *om;
    int rc;
    uint8_t buf = BLE_SVC_SCPS_SCAN_REFRESH_VALUE;

    om = ble_hs_mbuf_from_flat(&buf, 1);
    if (!om) {
        return -1;
    }

    rc = ble_gatts_notify_custom(g_conn_handle, scps_char_handles[HANDLE_SCPS_SCAN_REFRESH], om);
    if (rc) {
        return -1;
    }

    printf("SCPS sent notification for scan refresh\n");

    return 0;
}

HOGP_CMD(scanrefresh, do_ble_send_scan_refresh, "ble send scan refresh notification",
    "scanrefresh"
);

#endif

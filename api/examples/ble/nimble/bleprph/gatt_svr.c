/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "bleprph.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/**
 * The vendor specific security test service consists of two characteristics:
 *     o random-number-generator: generates a random 32-bit number each time
 *       it is read.  This characteristic can only be read over an encrypted
 *       connection.
 *     o static-value: a single-byte characteristic that can always be read,
 *       but can only be written over an encrypted connection.
 */

/* 59462f12-9543-9999-12c8-58b459a2712d */

static const ble_uuid128_t gatt_svr_svc_sec_test_uuid =
BLE_UUID128_INIT(0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12,
				 0x99, 0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59);

/* 5c3a659e-897e-45e1-b016-007107c96df6 */

static const ble_uuid128_t gatt_svr_chr_sec_test_rand_uuid =
BLE_UUID128_INIT(0xf6, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
				 0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

/* 5c3a659e-897e-45e1-b016-007107c96df7 */

static const ble_uuid128_t gatt_svr_chr_sec_test_static_uuid =
BLE_UUID128_INIT(0xf7, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
				 0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

/* 5c3a659e-897e-45e1-b016-007107c96df8 */

static const ble_uuid128_t gatt_svr_chr_ntf_test_static_uuid =
BLE_UUID128_INIT(0xf8, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
				 0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

static uint8_t gatt_svr_sec_test_static_val;

static int
gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
		struct ble_gatt_access_ctxt *ctxt,
		void *arg);


static const struct ble_gatt_svc_def gatt_svr_svcs[] =
{
	{
		/*** Service: Security test. */

		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = &gatt_svr_svc_sec_test_uuid.u,
		.characteristics = (struct ble_gatt_chr_def[])
		{
			{
				/*** Characteristic: Random number generator. */

				.uuid = &gatt_svr_chr_sec_test_rand_uuid.u,
				.access_cb = gatt_svr_chr_access_sec_test,
				.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
			},

			{
				/*** Characteristic: Static value. */

				.uuid = &gatt_svr_chr_sec_test_static_uuid.u,
				.access_cb = gatt_svr_chr_access_sec_test,
				.flags = BLE_GATT_CHR_F_READ |
					BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
			},

			{
				/*** Characteristic: Static value. */

				.uuid = &gatt_svr_chr_ntf_test_static_uuid.u,
				.access_cb = gatt_svr_chr_access_sec_test,
				.flags = BLE_GATT_CHR_F_NOTIFY,
			},

			{
				/* No more characteristics in this service. */

				0,
			}
		},
	},

	{
		0, /* No more services. */
	},
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int
gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
		void *dst, uint16_t *len)
{
	uint16_t om_len;
	int rc;

	om_len = OS_MBUF_PKTLEN(om);
	if (om_len < min_len || om_len > max_len)
	{
		return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
	}

	rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
	if (rc != 0)
	{
		return BLE_ATT_ERR_UNLIKELY;
	}

	return 0;
}

static int
gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
		struct ble_gatt_access_ctxt *ctxt,
		void *arg)
{
	const ble_uuid_t *uuid;
	int static_number;
	int rc;

	uuid = ctxt->chr->uuid;

	/* Determine which characteristic is being accessed by examining its
	 * 128-bit UUID.
	 */

	if (ble_uuid_cmp(uuid, &gatt_svr_chr_sec_test_rand_uuid.u) == 0)
	{
		assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);

		static_number = 0x04030201;
		rc = os_mbuf_append(ctxt->om, &static_number, sizeof static_number);
		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
	}

	if (ble_uuid_cmp(uuid, &gatt_svr_chr_sec_test_static_uuid.u) == 0)
	{
		switch (ctxt->op)
		{
			case BLE_GATT_ACCESS_OP_READ_CHR:
				rc = os_mbuf_append(ctxt->om, &gatt_svr_sec_test_static_val,
						sizeof gatt_svr_sec_test_static_val);
				return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

			case BLE_GATT_ACCESS_OP_WRITE_CHR:
				rc = gatt_svr_chr_write(ctxt->om,
						sizeof gatt_svr_sec_test_static_val,
						sizeof gatt_svr_sec_test_static_val,
						&gatt_svr_sec_test_static_val, NULL);

				return rc;

			default:
				assert(0);
				return BLE_ATT_ERR_UNLIKELY;
		}
	}

	/* Unknown characteristic; the nimble stack should not have called this
	 * function.
	 */

	assert(0);
	return BLE_ATT_ERR_UNLIKELY;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
	char buf[BLE_UUID_STR_LEN];

	switch (ctxt->op)
	{
		case BLE_GATT_REGISTER_OP_SVC:
			MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
					ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
					ctxt->svc.handle);
			break;

		case BLE_GATT_REGISTER_OP_CHR:
			MODLOG_DFLT(DEBUG, "registering characteristic %s with "
					"def_handle=%d val_handle=%d\n",
					ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
					ctxt->chr.def_handle,
					ctxt->chr.val_handle);
			break;

		case BLE_GATT_REGISTER_OP_DSC:
			MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
					ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
					ctxt->dsc.handle);
			break;

		default:
			assert(0);
			break;
	}
}

int
gatt_svr_init(void)
{
	int rc;

	rc = ble_gatts_reset();
	if (rc != 0)
	{
		return rc;
	}

	rc = ble_gatts_count_cfg(gatt_svr_svcs);
	if (rc != 0)
	{
		return rc;
	}

	rc = ble_gatts_add_svcs(gatt_svr_svcs);
	if (rc != 0)
	{
		return rc;
	}

	rc = ble_gatts_start();
	if (rc != 0)
	{
		return rc;
	}

	return 0;
}

#include "hal/cmsis/cmsis_os2.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "cli.h"

int
do_ble_ntf(int argc, char *argv[])
{
	uint16_t conn = 1;
	uint16_t handle;
	uint8_t max = 255;
	uint8_t buf[256];
	struct os_mbuf *om;
	int i;
	int rc;

	if (argc == 2) {
		conn = atoi(argv[1]);
	}
	if (argc == 3) {
		max = atoi(argv[2]);
	}

	rc = ble_gatts_find_chr(
		(ble_uuid_t *)&gatt_svr_svc_sec_test_uuid,
		(ble_uuid_t *)&gatt_svr_chr_ntf_test_static_uuid,
		NULL,
		&handle);
	if (rc) {
		return CMD_RET_FAILURE;
	}

	for (i = 0; i < 256; i++) {
		buf[i] = (uint8_t)i;
	}

	for (i = 1; i < max; i++) {
		osDelay(pdMS_TO_TICKS(300));
		printf("blentf conn=%d len=%d\n", conn, i);
		om = ble_hs_mbuf_from_flat(buf, i);
		if (!om) {
			printf("blentf om failed\n");
			break;
		}
		rc = ble_gatts_notify_custom(conn, handle, om);
		if (rc) {
			printf("blentf err, rc=%d\n", rc);
			break;
		}
	}

	return CMD_RET_SUCCESS;
}

CMD(blentf, do_ble_ntf, "ble notification test", "blentf conn max_len");


/*
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"

#include "blecomp.h"

static const ble_uuid16_t gatt_svr_comp_test_uuid =
BLE_UUID16_INIT(BLECOMP_SVC_TEST_UUID16);

static const ble_uuid16_t gatt_chr1_comp_test_uuid =
BLE_UUID16_INIT(BLECOMP_CHR1_TEST_UUID16);

static const ble_uuid16_t gatt_chr2_comp_test_uuid =
BLE_UUID16_INIT(BLECOMP_CHR2_TEST_UUID16);

static const ble_uuid16_t gatt_chr3_comp_test_uuid =
BLE_UUID16_INIT(BLECOMP_CHR3_TEST_UUID16);

static char char1_name[] = "Char_1_Short_WR";
static char char2_name[] = "Char_2_Long_WR";
static char char3_name[] = "Char_3_Short_Notify";

static uint8_t gatt_char1_short_val[4] = {0x11, 0x22, 0x33, 0x44};
static uint16_t gatt_short_len = 4;
static uint8_t gatt_char2_long_val[256];
static uint16_t gatt_long_len = 256;
static uint8_t gatt_char3_short_val[2] = {0xAA, 0xBB};

static int
gatt_svr_chr_access_comp_test(uint16_t conn_handle, uint16_t attr_handle,
		struct ble_gatt_access_ctxt *ctxt,
		void *arg);

static int
gatt_svr_desc_access_comp_test(uint16_t conn_handle, uint16_t attr_handle,
		struct ble_gatt_access_ctxt *ctxt,
		void *arg);


static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
	{
		/*** Service: Compatibility test. */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = &gatt_svr_comp_test_uuid.u,
		.characteristics = (struct ble_gatt_chr_def[]) { {
			/*** Characteristic: short read write . */
			.uuid = &gatt_chr1_comp_test_uuid.u,
			.access_cb = gatt_svr_chr_access_comp_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_READ_ENC,
			.descriptors = (struct ble_gatt_dsc_def[]) { {
				.uuid = BLE_UUID16_DECLARE(BLECOMP_USER_DESC_UUID16),
				.access_cb = gatt_svr_desc_access_comp_test,
				.att_flags = BLE_ATT_F_READ,
				.arg = char1_name,
			}, {
				0
			} }
		}, {
			/*** Characteristic: long read write. */
			.uuid = &gatt_chr2_comp_test_uuid.u,
			.access_cb = gatt_svr_chr_access_comp_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.descriptors = (struct ble_gatt_dsc_def[]) { {
				.uuid = BLE_UUID16_DECLARE(BLECOMP_USER_DESC_UUID16),
				.access_cb = gatt_svr_desc_access_comp_test,
				.att_flags = BLE_ATT_F_READ,
				.arg = char2_name,
			}, {
				0
			} }

		}, {
			/*** Characteristic: short notification. */
			.uuid = &gatt_chr3_comp_test_uuid.u,
			.access_cb = gatt_svr_chr_access_comp_test,
			.flags = BLE_GATT_CHR_F_NOTIFY,
			.descriptors = (struct ble_gatt_dsc_def[]) { {
				.uuid = BLE_UUID16_DECLARE(BLECOMP_USER_DESC_UUID16),
				.access_cb = gatt_svr_desc_access_comp_test,
				.att_flags = BLE_ATT_F_READ,
				.arg = char3_name,
			}, {
				0
			} }
		} , {
			0, /* No more characteristics in this service. */
		} },

	},

	{
		0, /* No more services. */
	},
};

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
gatt_svr_chr_access_comp_test(uint16_t conn_handle, uint16_t attr_handle,
		struct ble_gatt_access_ctxt *ctxt,
		void *arg)
{
    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
	int rc = 0;

	switch (uuid16) {
		case BLECOMP_CHR1_TEST_UUID16:
			if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
				rc = os_mbuf_append(ctxt->om, gatt_char1_short_val,
						gatt_short_len);
				if (rc == 0) {
					printf("(2) ***** read char_1 *****\n");
				}
			} else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
				rc = gatt_svr_chr_write(ctxt->om, 0,
						sizeof(gatt_char1_short_val),
						gatt_char1_short_val,
						&gatt_short_len);
				if (rc == 0) {
					printf("(3) ***** short write success *****\n");
				}
			}
			break;

		case BLECOMP_CHR2_TEST_UUID16:
			if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
				rc = os_mbuf_append(ctxt->om, gatt_char2_long_val,
						gatt_long_len);
				if (rc == 0) {
					printf("(5) ***** read char_2 *****\n");
				}
			} else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
				rc = gatt_svr_chr_write(ctxt->om, 0,
						sizeof(gatt_char2_long_val),
						gatt_char2_long_val,
						&gatt_long_len);
				if (rc == 0) {
					printf("(4) ***** long write success *****\n");
				}
			}
			break;

		case BLECOMP_CHR3_TEST_UUID16:
		default:
			rc = BLE_ATT_ERR_UNLIKELY;
			break;
	}

	return rc;
}

static int
gatt_svr_desc_access_comp_test(uint16_t conn_handle, uint16_t attr_handle,
		struct ble_gatt_access_ctxt *ctxt,
		void *arg)
{
    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
	int rc;

	if (uuid16 == BLECOMP_USER_DESC_UUID16) {
		char *chr_name = arg;

		rc = os_mbuf_append(ctxt->om, chr_name, strlen(chr_name));
	} else {
		rc = BLE_ATT_ERR_UNLIKELY;
	}

	return rc;
}

int
gatt_svr_notify(uint16_t conn_handle)
{
	struct os_mbuf *om;
	uint16_t handle;
	int rc;

	rc = ble_gatts_find_chr(
		(ble_uuid_t *)&gatt_svr_comp_test_uuid,
		(ble_uuid_t *)&gatt_chr3_comp_test_uuid,
		NULL,
		&handle);
	if (rc) {
		printf("find chr handle err, rc=%d\n", rc);
		return rc;
	}


	om = ble_hs_mbuf_from_flat(gatt_char3_short_val, 2);
	if(!om) {
		printf("No resource\n");
		return rc;
	}

	rc = ble_gatts_notify_custom(conn_handle, handle, om);
	if (rc) {
		printf("notify err, rc=%d\n", rc);
		return rc;
	}

	printf("(6) ***** send notify AA BB *****\n");

	return 0;
}

void
gatt_svt_val_clear(void)
{
    gatt_char1_short_val[0] = 0x11;
    gatt_char1_short_val[1] = 0x22;
    gatt_char1_short_val[2] = 0x33;
    gatt_char1_short_val[3] = 0x44;
    gatt_short_len = 4;

    memset(gatt_char2_long_val, 0, sizeof(gatt_char2_long_val));
    gatt_long_len = 256;
}

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
	int rc;

	rc = ble_gatts_reset();
	if (rc != 0) {
		return rc;
	}

	rc = ble_gatts_count_cfg(gatt_svr_svcs);
	if (rc != 0) {
		return rc;
	}

	rc = ble_gatts_add_svcs(gatt_svr_svcs);
	if (rc != 0) {
		return rc;
	}

	rc = ble_gatts_start();
	if (rc != 0) {
		return rc;
	}

	return 0;
}

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

#include "console/console.h"
#include "host/ble_uuid.h"
#include "host/ble_gap.h"

#include "btshell.h"

#include "../misc.h"

int
svc_is_empty(const struct btshell_svc *svc)
{
    return svc->svc.end_handle <= svc->svc.start_handle;
}

uint16_t
chr_end_handle(const struct btshell_svc *svc, const struct btshell_chr *chr)
{
    const struct btshell_chr *next_chr;

    next_chr = SLIST_NEXT(chr, next);
    if (next_chr != NULL) {
        return next_chr->chr.def_handle - 1;
    } else {
        return svc->svc.end_handle;
    }
}

int
chr_is_empty(const struct btshell_svc *svc, const struct btshell_chr *chr)
{
    return chr_end_handle(svc, chr) <= chr->chr.val_handle;
}

static void
print_dsc(struct btshell_dsc *dsc)
{
    console_printf("            dsc_handle=%d (0x%04x) uuid=",
                   dsc->dsc.handle,
                   dsc->dsc.handle);
    print_uuid(&dsc->dsc.uuid.u);
    console_printf("\n");
}

static void
print_chr(struct btshell_chr *chr)
{
    struct btshell_dsc *dsc;

    console_printf("        def_handle=%d (0x%04x) val_handle=%d (0x%04x) properties=0x%02x "
                   "uuid=",
                   chr->chr.def_handle,
                   chr->chr.def_handle,
                   chr->chr.val_handle,
                   chr->chr.val_handle,
                   chr->chr.properties);
    print_uuid(&chr->chr.uuid.u);
    console_printf("\n");

    SLIST_FOREACH(dsc, &chr->dscs, next) {
        print_dsc(dsc);
    }
}

void
print_svc(struct btshell_svc *svc)
{
    struct btshell_chr *chr;

    console_printf("    start=%d (0x%04x) end=%d (0x%04x) uuid=",
                   svc->svc.start_handle,
                   svc->svc.start_handle,
                   svc->svc.end_handle,
                   svc->svc.end_handle);
    print_uuid(&svc->svc.uuid.u);
    console_printf("\n");

    SLIST_FOREACH(chr, &svc->chrs, next) {
        print_chr(chr);
    }
}

#ifdef CONFIG_NIMBLE_PTS

#include <string.h>
#include <stdio.h>

#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_l2cap.h"
#include "host/ble_sm.h"


struct dict {
    uint16_t value;
    const char *str;
};

#define NAME_STR(code)          { code, #code }


static const struct dict ble_hs_err_str[] = {
	{0, "BLE_HS_SUCCESS"},
	NAME_STR(BLE_HS_EAGAIN),
	NAME_STR(BLE_HS_EALREADY),
	NAME_STR(BLE_HS_EINVAL),
	NAME_STR(BLE_HS_EMSGSIZE),
	NAME_STR(BLE_HS_ENOENT),
	NAME_STR(BLE_HS_ENOMEM),
	NAME_STR(BLE_HS_ENOTCONN),
	NAME_STR(BLE_HS_ENOTSUP),
	NAME_STR(BLE_HS_EAPP),
	NAME_STR(BLE_HS_EBADDATA),
	NAME_STR(BLE_HS_EOS),
	NAME_STR(BLE_HS_ECONTROLLER),
	NAME_STR(BLE_HS_ETIMEOUT),
	NAME_STR(BLE_HS_EDONE),
	NAME_STR(BLE_HS_EBUSY),
	NAME_STR(BLE_HS_EREJECT),
	NAME_STR(BLE_HS_EUNKNOWN),
	NAME_STR(BLE_HS_EROLE),
	NAME_STR(BLE_HS_ETIMEOUT_HCI),
	NAME_STR(BLE_HS_ENOMEM_EVT),
	NAME_STR(BLE_HS_ENOADDR),
	NAME_STR(BLE_HS_ENOTSYNCED),
	NAME_STR(BLE_HS_EAUTHEN),
	NAME_STR(BLE_HS_EAUTHOR),
	NAME_STR(BLE_HS_EENCRYPT),
	NAME_STR(BLE_HS_EENCRYPT_KEY_SZ),
	NAME_STR(BLE_HS_ESTORE_CAP),
	NAME_STR(BLE_HS_ESTORE_FAIL),
	NAME_STR(BLE_HS_EPREEMPTED),
	NAME_STR(BLE_HS_EDISABLED),
	NAME_STR(BLE_HS_ESTALLED),
};

static const struct dict ble_hci_err_str[] = {
	NAME_STR(BLE_ERR_SUCCESS),
	NAME_STR(BLE_ERR_UNKNOWN_HCI_CMD),
	NAME_STR(BLE_ERR_UNK_CONN_ID),
	NAME_STR(BLE_ERR_HW_FAIL),
	NAME_STR(BLE_ERR_PAGE_TMO),
	NAME_STR(BLE_ERR_AUTH_FAIL),
	NAME_STR(BLE_ERR_PINKEY_MISSING),
	NAME_STR(BLE_ERR_MEM_CAPACITY),
	NAME_STR(BLE_ERR_CONN_SPVN_TMO),
	NAME_STR(BLE_ERR_CONN_LIMIT),
	NAME_STR(BLE_ERR_SYNCH_CONN_LIMIT),
	NAME_STR(BLE_ERR_ACL_CONN_EXISTS),
	NAME_STR(BLE_ERR_CMD_DISALLOWED),
	NAME_STR(BLE_ERR_CONN_REJ_RESOURCES),
	NAME_STR(BLE_ERR_CONN_REJ_SECURITY),
	NAME_STR(BLE_ERR_CONN_REJ_BD_ADDR),
	NAME_STR(BLE_ERR_CONN_ACCEPT_TMO),
	NAME_STR(BLE_ERR_UNSUPPORTED),
	NAME_STR(BLE_ERR_INV_HCI_CMD_PARMS),
	NAME_STR(BLE_ERR_REM_USER_CONN_TERM),
	NAME_STR(BLE_ERR_RD_CONN_TERM_RESRCS),
	NAME_STR(BLE_ERR_RD_CONN_TERM_PWROFF),
	NAME_STR(BLE_ERR_CONN_TERM_LOCAL),
	NAME_STR(BLE_ERR_REPEATED_ATTEMPTS),
	NAME_STR(BLE_ERR_NO_PAIRING),
	NAME_STR(BLE_ERR_UNK_LMP),
	NAME_STR(BLE_ERR_UNSUPP_REM_FEATURE),
	NAME_STR(BLE_ERR_SCO_OFFSET),
	NAME_STR(BLE_ERR_SCO_ITVL),
	NAME_STR(BLE_ERR_SCO_AIR_MODE),
	NAME_STR(BLE_ERR_INV_LMP_LL_PARM),
	NAME_STR(BLE_ERR_UNSPECIFIED),
	NAME_STR(BLE_ERR_UNSUPP_LMP_LL_PARM),
	NAME_STR(BLE_ERR_NO_ROLE_CHANGE),
	NAME_STR(BLE_ERR_LMP_LL_RSP_TMO),
	NAME_STR(BLE_ERR_LMP_COLLISION),
	NAME_STR(BLE_ERR_LMP_PDU),
	NAME_STR(BLE_ERR_ENCRYPTION_MODE),
	NAME_STR(BLE_ERR_LINK_KEY_CHANGE),
	NAME_STR(BLE_ERR_UNSUPP_QOS),
	NAME_STR(BLE_ERR_INSTANT_PASSED),
	NAME_STR(BLE_ERR_UNIT_KEY_PAIRING),
	NAME_STR(BLE_ERR_DIFF_TRANS_COLL),
	/* NAME_STR(BLE_ERR_RESERVED), */
	NAME_STR(BLE_ERR_QOS_PARM),
	NAME_STR(BLE_ERR_QOS_REJECTED),
	NAME_STR(BLE_ERR_CHAN_CLASS),
	NAME_STR(BLE_ERR_INSUFFICIENT_SEC),
	NAME_STR(BLE_ERR_PARM_OUT_OF_RANGE),
	/* NAME_STR(BLE_ERR_RESERVED), */
	NAME_STR(BLE_ERR_PENDING_ROLE_SW),
	/* NAME_STR(BLE_ERR_RESERVED), */
	NAME_STR(BLE_ERR_RESERVED_SLOT),
	NAME_STR(BLE_ERR_ROLE_SW_FAIL),
	NAME_STR(BLE_ERR_INQ_RSP_TOO_BIG),
	NAME_STR(BLE_ERR_SEC_SIMPLE_PAIR),
	NAME_STR(BLE_ERR_HOST_BUSY_PAIR),
	NAME_STR(BLE_ERR_CONN_REJ_CHANNEL),
	NAME_STR(BLE_ERR_CTLR_BUSY),
	NAME_STR(BLE_ERR_CONN_PARMS),
	NAME_STR(BLE_ERR_DIR_ADV_TMO),
	NAME_STR(BLE_ERR_CONN_TERM_MIC),
	NAME_STR(BLE_ERR_CONN_ESTABLISHMENT),
	NAME_STR(BLE_ERR_MAC_CONN_FAIL),
	NAME_STR(BLE_ERR_COARSE_CLK_ADJ),
	NAME_STR(BLE_ERR_TYPE0_SUBMAP_NDEF),
	NAME_STR(BLE_ERR_UNK_ADV_INDENT),
	NAME_STR(BLE_ERR_LIMIT_REACHED),
	NAME_STR(BLE_ERR_OPERATION_CANCELLED),
	NAME_STR(BLE_ERR_PACKET_TOO_LONG),
};

static const struct dict ble_l2cap_err_str[] = {
    NAME_STR(BLE_L2CAP_SIG_ERR_CMD_NOT_UNDERSTOOD),
    NAME_STR(BLE_L2CAP_SIG_ERR_MTU_EXCEEDED),
    NAME_STR(BLE_L2CAP_SIG_ERR_INVALID_CID),
};

static const struct dict ble_l2coc_err_str[] = {
    NAME_STR(BLE_L2CAP_COC_ERR_CONNECTION_SUCCESS),
    NAME_STR(BLE_L2CAP_COC_ERR_UNKNOWN_LE_PSM),
    NAME_STR(BLE_L2CAP_COC_ERR_NO_RESOURCES),
    NAME_STR(BLE_L2CAP_COC_ERR_INSUFFICIENT_AUTHEN),
    NAME_STR(BLE_L2CAP_COC_ERR_INSUFFICIENT_AUTHOR),
    NAME_STR(BLE_L2CAP_COC_ERR_INSUFFICIENT_KEY_SZ),
    NAME_STR(BLE_L2CAP_COC_ERR_INSUFFICIENT_ENC),
    NAME_STR(BLE_L2CAP_COC_ERR_INVALID_SOURCE_CID),
    NAME_STR(BLE_L2CAP_COC_ERR_SOURCE_CID_ALREADY_USED),
    NAME_STR(BLE_L2CAP_COC_ERR_UNACCEPTABLE_PARAMETERS),
    NAME_STR(BLE_L2CAP_COC_ERR_INVALID_PARAMETERS),
};

static const struct dict ble_att_err_str[] = {
    NAME_STR(BLE_ATT_ERR_INVALID_HANDLE),
    NAME_STR(BLE_ATT_ERR_READ_NOT_PERMITTED),
    NAME_STR(BLE_ATT_ERR_WRITE_NOT_PERMITTED),
    NAME_STR(BLE_ATT_ERR_INVALID_PDU),
    NAME_STR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN),
    NAME_STR(BLE_ATT_ERR_REQ_NOT_SUPPORTED),
    NAME_STR(BLE_ATT_ERR_INVALID_OFFSET),
    NAME_STR(BLE_ATT_ERR_INSUFFICIENT_AUTHOR),
    NAME_STR(BLE_ATT_ERR_PREPARE_QUEUE_FULL),
    NAME_STR(BLE_ATT_ERR_ATTR_NOT_FOUND),
    NAME_STR(BLE_ATT_ERR_ATTR_NOT_LONG),
    NAME_STR(BLE_ATT_ERR_INSUFFICIENT_KEY_SZ),
    NAME_STR(BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN),
    NAME_STR(BLE_ATT_ERR_UNLIKELY),
    NAME_STR(BLE_ATT_ERR_INSUFFICIENT_ENC),
    NAME_STR(BLE_ATT_ERR_UNSUPPORTED_GROUP),
    NAME_STR(BLE_ATT_ERR_INSUFFICIENT_RES),

	{0x80, "BLE_ATT_ERR_APPLICATION"},
};

static const struct dict ble_smp_err_str[] = {
    NAME_STR(BLE_SM_ERR_PASSKEY),
    NAME_STR(BLE_SM_ERR_OOB),
    NAME_STR(BLE_SM_ERR_AUTHREQ),
    NAME_STR(BLE_SM_ERR_CONFIRM_MISMATCH),
    NAME_STR(BLE_SM_ERR_PAIR_NOT_SUPP),
    NAME_STR(BLE_SM_ERR_ENC_KEY_SZ),
    NAME_STR(BLE_SM_ERR_CMD_NOT_SUPP),
    NAME_STR(BLE_SM_ERR_UNSPECIFIED),
    NAME_STR(BLE_SM_ERR_REPEATED),
    NAME_STR(BLE_SM_ERR_INVAL),
    NAME_STR(BLE_SM_ERR_DHKEY),
    NAME_STR(BLE_SM_ERR_NUMCMP),
    NAME_STR(BLE_SM_ERR_ALREADY),
    NAME_STR(BLE_SM_ERR_CROSS_TRANS),
    NAME_STR(BLE_SM_ERR_KEY_REJECTED),
    NAME_STR(BLE_SM_ERR_MAX_PLUS_1),
};

static const char *value_to_str(uint32_t value, const struct dict *array, int array_size, int offset)
{
    const struct dict *it;
    int i;

    for (i = 0; i < array_size; i++) {
        it = &array[i];
        if (it->value == value) {
            return &it->str[offset];
        }
    }

    return "Unknown";
}

const char *btshell_hs_err_str(uint8_t err)
{
    return value_to_str(err, ble_hs_err_str, sizeof(ble_hs_err_str) / sizeof(struct dict), 4);
}

const char *btshell_hci_err_str(uint8_t err)
{
    return value_to_str(err, ble_hci_err_str, sizeof(ble_hci_err_str) / sizeof(struct dict), 4);
}

const char *btshell_l2cap_err_str(uint16_t err)
{
    return value_to_str(err, ble_l2cap_err_str, sizeof(ble_l2cap_err_str) / sizeof(struct dict), 4);
}

const char *btshell_l2coc_err_str(uint16_t err)
{
    return value_to_str(err, ble_l2coc_err_str, sizeof(ble_l2coc_err_str) / sizeof(struct dict), 4);
}

const char *btshell_att_err_str(uint8_t err)
{
    return value_to_str(err, ble_att_err_str, sizeof(ble_att_err_str) / sizeof(struct dict), 4);
}

const char *btshell_sm_err_str(uint8_t err)
{
    return value_to_str(err, ble_smp_err_str, sizeof(ble_smp_err_str) / sizeof(struct dict), 4);
}

const char *btshell_err_str(uint16_t hs_err, uint16_t *layer_err)
{
	uint16_t err_layer = hs_err & 0xff00;
	uint16_t err_code = hs_err & 0xff;
	const char *err_str;

	switch (err_layer) {
		case 0:
			err_str = btshell_hs_err_str(err_code);
			err_code = 0;
		break;
		case BLE_HS_ERR_ATT_BASE:
			err_str = btshell_att_err_str(err_code);
		break;
		case BLE_HS_ERR_HCI_BASE:
			err_str = btshell_hci_err_str(err_code);
		break;
		case BLE_HS_ERR_L2C_BASE:
			err_str = btshell_l2cap_err_str(err_code);
		break;
		case BLE_HS_ERR_SM_US_BASE:
		case BLE_HS_ERR_SM_PEER_BASE:
			err_str = btshell_sm_err_str(err_code);
			err_str = btshell_sm_err_str(err_code);
		break;
		case BLE_HS_ERR_HW_BASE:
			err_str = "HARDWARE_ERROR";
		break;
		default:
		break;
	}

	if (layer_err) {
		*layer_err = err_code;
	}

	return err_str;
}

uint16_t btshell_hs_err_to_coc_err(int hs_err)
{
	uint16_t coc_err;

	switch (hs_err) {
	    case BLE_HS_ENOTSUP:
	        coc_err = BLE_L2CAP_COC_ERR_UNKNOWN_LE_PSM; break;
	    case BLE_HS_ENOMEM:
	        coc_err = BLE_L2CAP_COC_ERR_NO_RESOURCES; break;
	    case BLE_HS_EAUTHEN:
	        coc_err = BLE_L2CAP_COC_ERR_INSUFFICIENT_AUTHEN; break;
	    case BLE_HS_EAUTHOR:
	        coc_err = BLE_L2CAP_COC_ERR_INSUFFICIENT_AUTHOR; break;
	    case BLE_HS_EENCRYPT:
	        coc_err = BLE_L2CAP_COC_ERR_INSUFFICIENT_ENC; break;
	    case BLE_HS_EENCRYPT_KEY_SZ:
	        coc_err = BLE_L2CAP_COC_ERR_INSUFFICIENT_KEY_SZ; break;
	    case BLE_HS_EINVAL:
	        coc_err = BLE_L2CAP_COC_ERR_UNACCEPTABLE_PARAMETERS; break;
		case BLE_HS_EREJECT:
			coc_err = BLE_L2CAP_COC_ERR_INVALID_SOURCE_CID; break;
		case BLE_HS_EALREADY:
			coc_err = BLE_L2CAP_COC_ERR_SOURCE_CID_ALREADY_USED; break;
		default:
			coc_err = BLE_L2CAP_COC_ERR_NO_RESOURCES;
	}
	return coc_err;
}

#endif

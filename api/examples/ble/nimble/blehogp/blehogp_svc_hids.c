/**
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

#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "blehogp.h"
#include "blehogp_svc_hids.h"
#include "blehogp_svc_bas.h"
#include "hid_codes.h"

enum attr_handles {
    HANDLE_HID_PROTO_MODE,
    HANDLE_HID_INFORMATION,
    HANDLE_HID_CONTROL_POINT,
    HANDLE_HID_REPORT_MAP,
    HANDLE_HID_REPORT_KEYBOARD,
    HANDLE_HID_REPORT_KEYBOARD_LED,
    HANDLE_HID_REPORT_KEYBOARD_FEAT,
    HANDLE_HID_REPORT_MOUSE,
    HANDLE_HID_REPORT_CONSUMER,
    HANDLE_HID_COUNT,
};

enum report_id {
    HID_REPORT_KEYBOARD = 1,
    HID_REPORT_MOUSE    = 2,
    HID_REPORT_CONSUMER = 3,
};

struct report_reference_table {
    int id;
    uint8_t report_ref[HID_REPORT_REF_LEN];
} __attribute__((packed));


static uint16_t hids_char_handles[HANDLE_HID_COUNT];
static const uint8_t hid_info[HID_INFORMATION_LEN] = {
    0x01, 0x01, /* bcd */
    0x00,       /* country */
    0x00,       /* flag */
};
static uint8_t hid_proto_mode = HID_PROTOCOL_MODE_REPORT;
static const struct report_reference_table hid_report_ref_data[] = {
    { .id = HANDLE_HID_REPORT_KEYBOARD,      .report_ref = { HID_REPORT_KEYBOARD, HID_REPORT_TYPE_INPUT   }},
    { .id = HANDLE_HID_REPORT_KEYBOARD_LED,  .report_ref = { HID_REPORT_KEYBOARD, HID_REPORT_TYPE_OUTPUT  }},
    { .id = HANDLE_HID_REPORT_KEYBOARD_FEAT, .report_ref = { HID_REPORT_KEYBOARD, HID_REPORT_TYPE_FEATURE }},
    { .id = HANDLE_HID_REPORT_MOUSE,         .report_ref = { HID_REPORT_MOUSE,    HID_REPORT_TYPE_INPUT   }},
    { .id = HANDLE_HID_REPORT_CONSUMER,      .report_ref = { HID_REPORT_CONSUMER, HID_REPORT_TYPE_INPUT   }},
};
static const size_t hid_report_ref_data_count = sizeof(hid_report_ref_data)/sizeof(hid_report_ref_data[0]);
static uint8_t hid_report_kb_in[8];
static uint8_t hid_report_kb_out[1];
static uint8_t hid_report_kb_feat[2];
static uint8_t hid_report_mouse[4];
static uint8_t hid_report_consumer[1];

static const uint8_t hid_report_map[] = {
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x06, // USAGE (Keyboard)
    0xA1, 0x01, // COLLECTION (Application)
    0x85, 0x01, //   REPORT_ID (1)

    0x05, 0x07, //   USAGE_PAGE (Key Codes)
    0x19, 0xe0, //   USAGE_MINIMUM (224)
    0x29, 0xe7, //   USAGE_MAXIMUM (231)
    0x15, 0x00, //   LOGICAL_MINIMUM (0)
    0x25, 0x01, //   LOGICAL_MAXIMUM (1)
    0x75, 0x01, //   REPORT_SIZE (1)
    0x95, 0x08, //   REPORT_COUNT (8)
    0x81, 0x02, //   INPUT (Data, Var, Abs)

    0x95, 0x01, //   REPORT_COUNT (1)
    0x75, 0x08, //   REPORT_SIZE (8)
    0x81, 0x01, //   INPUT (Cnst) reserved byte(1)

    0x95, 0x05, //   REPORT_COUNT (5)
    0x75, 0x01, //   REPORT_SIZE (1)
    0x05, 0x08, //   USAGE_PAGE (LEDs)
    0x19, 0x01, //   USAGE_MINIMUM (1)
    0x29, 0x05, //   USAGE_MAXIMUM (5)
    0x91, 0x02, //   OUTPUT (Data, Var, Abs)
    0x95, 0x01, //   REPORT_COUNT (1)
    0x75, 0x03, //   REPORT_SIZE (3)
    0x91, 0x01, //   OUTPUT (Cnst)

    0x95, 0x06, //   REPORT_COUNT (6)
    0x75, 0x08, //   REPORT_SIZE (8)
    0x15, 0x00, //   LOGICAL_MINIMUM (0)
    0x25, 0x65, //   LOGICAL_MAXIMUM (101)
    0x05, 0x07, //   USAGE_PAGE (Key codes)
    0x19, 0x00, //   USAGE_MINIMUM (0)
    0x29, 0x65, //   USAGE_MAXIMUM (101)
    0x81, 0x00, //   INPUT (Data, Array) Key array(6 bytes)

    0x09, 0x05, // USAGE (VENDOR DEFINED)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00, // LOGICAL_MAXIMUM (255)
    0x75, 0x08, // REPORT_SIZE (8 bit)
    0x95, 0x02, // REPORT_COUNT (2)
    0xB1, 0x02, // FEATURE (Data, Var, Abs)

    0xC0,       // END_COLLECTION

    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x02, // USAGE (Mouse)
    0xA1, 0x01, // COLLECTION (Application)
    0x85, 0x02, //   REPORT_ID (2)
    0x09, 0x01, //   USAGE (Pointer)
    0xA1, 0x00, //   COLLECTION (Physical)

    0x05, 0x09, //     USAGE_PAGE (Buttons - left, right, middle)
    0x19, 0x01, //     USAGE_MINIMUM (1)
    0x29, 0x03, //     USAGE_MAXIMUM (3)
    0x15, 0x00, //     LOGICAL_MINIMUM (0)
    0x25, 0x01, //     LOGICAL_MAXIMUM (1)
    0x75, 0x01, //     REPORT_SIZE (1)
    0x95, 0x03, //     REPORT_COUNT (3)
    0x81, 0x02, //     INPUT (Data, Var, Abs)

    0x75, 0x05, //     REPORT_SIZE (5)
    0x95, 0x01, //     REPORT_COUNT (1)
    0x81, 0x01, //     INPUT (Cnst)

    0x05, 0x01, //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30, //     USAGE (X)
    0x09, 0x31, //     USAGE (Y)
    0x09, 0x38, //     USAGE (Wheel)
    0x15, 0x81, //     LOGICAL_MINIMUM (-127)
    0x25, 0x7F, //     LOGICAL_MAXIMUM (127)
    0x75, 0x08, //     REPORT_SIZE (8)
    0x95, 0x03, //     REPORT_COUNT (3)
    0x81, 0x06, //     INPUT (Data,Var,Rel)

    0xC0,       //   END_COLLECTION
    0xC0,       // END_COLLECTION

    0x05, 0x0C, // USAGE_PAGE (CONSUMER)
    0x09, 0x01, // USAGE (CONSUMER CONTROL)
    0xA1, 0x01, // COLLECTION (APPLICATION)
    0x85, 0x03, //   REPORT_ID (3)

    0x09, 0xE9, //   USAGE (VOLUME INCREMENT)
    0x09, 0xEA, //   USAGE (VOLUME DECREMENT)
    0x15, 0x00, //   LOGCIAL_MINIMUM (0)
    0x25, 0x01, //   LOGICAL_MAXIMUM (1)
    0x75, 0x01, //   REPORT_SIZE (1)
    0x95, 0x02, //   REPORT_COUNT (2)
    0x81, 0x02, //   INPUT (Data, Var, Abs)
    0x75, 0x06, //   REPORT_SIZE (6)
    0x95, 0x01, //   REPORT_COUNT (1)
    0x81, 0x01, //   INPUT (Cnst)

    0XC0,       // END_COLLECTION

    0x05, 0x0C, // USAGE_PAGE (CONSUMER)
    0x09, 0x01, // USAGE (CONSUMER CONTROL)
    0xA1, 0x01, // COLLECTION (APPLICATION)
    0x85, 0xF0, //   REPORT_ID (240)

    0x05, 0x01, //   USAGE_PAGE (Generic Desktop)
    0x09, 0x06, //   USAGE (Keyboard)
    0xA1, 0x02, //   COLLECTION (Logical)
    0x05, 0x06, //     USAGE_PAGE (Generic Device Control)
    0x09, 0x20, //     USAGE (Battery Strength)
    0x15, 0x00, //     LOGICAL_MINIMUM (0)
    0x25, 0x64, //     LOGICAL_MAXIMUM (100)
    0x75, 0x08, //     REPORT_SIZE (8)
    0x95, 0x01, //     REPORT_COUNT (1)
    0x81, 0x02, //     INPUT (Data, Var, Abs)
    0xC0,       //   END_COLLECTION

    0xC0,       // END_COLLECTION
};

static uint16_t hid_external_report_ref = BLE_SVC_BAS_UUID16;

int
blehogp_svc_hids_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);
int
blehogp_svc_hids_report_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);

extern const struct ble_gatt_svc_def ble_svc_bas_defs[];

static const struct ble_gatt_svc_def *inc_svcs[] = {
    &ble_svc_bas_defs[0],
    NULL,
};

static const struct ble_gatt_svc_def blehogp_svc_hids_defs[] = {
    { /*** HID Service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_HIDS_UUID16),
        .includes = inc_svcs,
        .characteristics = (struct ble_gatt_chr_def[]) { {
        /*** Protocol Mode Characteristic */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HIDS_CHR_UUID16_PROTO_MODE),
            .access_cb = blehogp_svc_hids_access,
            .val_handle = &hids_char_handles[HANDLE_HID_PROTO_MODE],
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC |
                BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC,
        }, {
        /*** HID INFO characteristic */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HIDS_CHR_UUID16_INFORMATION),
            .access_cb = blehogp_svc_hids_access,
            .val_handle = &hids_char_handles[HANDLE_HID_INFORMATION],
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
        }, {
        /*** HID Control Point */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HIDS_CHR_UUID16_CONTROL_POINT),
            .access_cb = blehogp_svc_hids_access,
            .val_handle = &hids_char_handles[HANDLE_HID_CONTROL_POINT],
            .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC,
        }, {
        /*** Report Map */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HIDS_CHR_UUID16_REPORT_MAP),
            .access_cb = blehogp_svc_hids_access,
            .val_handle = &hids_char_handles[HANDLE_HID_REPORT_MAP],
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                /* External Report Reference Descriptor */
                .uuid = BLE_UUID16_DECLARE(BLE_SVC_CHR_DSC_UUID16_EXTERNAL_REPORT_REF),
                .att_flags = BLE_ATT_F_READ,
                .access_cb = blehogp_svc_hids_access,
            }, {
                0, /* No more descriptors in this characteristic. */
            } },
        }, {
        /*** Keyboard hid report */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HIDS_CHR_UUID16_REPORT),
            .access_cb = blehogp_svc_hids_report_access,
            .arg = (void *)HANDLE_HID_REPORT_KEYBOARD,
            .val_handle = &hids_char_handles[HANDLE_HID_REPORT_KEYBOARD],
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ_ENC |
                BLE_GATT_CHR_F_WRITE_ENC,
            .min_key_size = HID_DEFAULT_MIN_KEY_SIZE,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                /* Report Reference Descriptor */
                .uuid = BLE_UUID16_DECLARE(BLE_SVC_CHR_DSC_UUID16_REPORT_REF),
                .att_flags = BLE_ATT_F_READ,
                .access_cb = blehogp_svc_hids_report_access,
                .arg = (void *)HANDLE_HID_REPORT_KEYBOARD,
                .min_key_size = HID_DEFAULT_MIN_KEY_SIZE,
            }, {
                0, /* No more descriptors in this characteristic. */
            } },
        }, {
        /*** Keyboard led hid report */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HIDS_CHR_UUID16_REPORT),
            .access_cb = blehogp_svc_hids_report_access,
            .arg = (void *)HANDLE_HID_REPORT_KEYBOARD_LED,
            .val_handle = &hids_char_handles[HANDLE_HID_REPORT_KEYBOARD_LED],
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP |
                BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ_ENC |
                BLE_GATT_CHR_F_WRITE_ENC,
            .min_key_size = HID_DEFAULT_MIN_KEY_SIZE,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                /* Report Reference Descriptor */
                .uuid = BLE_UUID16_DECLARE(BLE_SVC_CHR_DSC_UUID16_REPORT_REF),
                .att_flags = BLE_ATT_F_READ,
                .access_cb = blehogp_svc_hids_report_access,
                .arg = (void *)HANDLE_HID_REPORT_KEYBOARD_LED,
                .min_key_size = HID_DEFAULT_MIN_KEY_SIZE,
            }, {
                0, /* No more descriptors in this characteristic. */
            } },
        }, {
        /*** Keyboard feature hid report */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HIDS_CHR_UUID16_REPORT),
            .access_cb = blehogp_svc_hids_report_access,
            .arg = (void *)HANDLE_HID_REPORT_KEYBOARD_FEAT,
            .val_handle = &hids_char_handles[HANDLE_HID_REPORT_KEYBOARD_FEAT],
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE_ENC,
            .min_key_size = HID_DEFAULT_MIN_KEY_SIZE,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                /* Report Reference Descriptor */
                .uuid = BLE_UUID16_DECLARE(BLE_SVC_CHR_DSC_UUID16_REPORT_REF),
                .att_flags = BLE_ATT_F_READ,
                .access_cb = blehogp_svc_hids_report_access,
                .arg = (void *)HANDLE_HID_REPORT_KEYBOARD_FEAT,
                .min_key_size = HID_DEFAULT_MIN_KEY_SIZE,
            }, {
                0, /* No more descriptors in this characteristic. */
            } },
        }, {
        /*** Mouse hid report */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HIDS_CHR_UUID16_REPORT),
            .access_cb = blehogp_svc_hids_report_access,
            .arg = (void *)HANDLE_HID_REPORT_MOUSE,
            .val_handle = &hids_char_handles[HANDLE_HID_REPORT_MOUSE],
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ_ENC |
                BLE_GATT_CHR_F_WRITE_ENC,
            .min_key_size = HID_DEFAULT_MIN_KEY_SIZE,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                /* Report Reference Descriptor */
                .uuid = BLE_UUID16_DECLARE(BLE_SVC_CHR_DSC_UUID16_REPORT_REF),
                .att_flags = BLE_ATT_F_READ,
                .access_cb = blehogp_svc_hids_report_access,
                .arg = (void *)HANDLE_HID_REPORT_MOUSE,
                .min_key_size = HID_DEFAULT_MIN_KEY_SIZE,
            }, {
                0, /* No more descriptors in this characteristic. */
            } },
        }, {
        /*** consumer hid report */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HIDS_CHR_UUID16_REPORT),
            .access_cb = blehogp_svc_hids_report_access,
            .arg = (void *)HANDLE_HID_REPORT_CONSUMER,
            .val_handle = &hids_char_handles[HANDLE_HID_REPORT_CONSUMER],
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ_ENC |
                BLE_GATT_CHR_F_WRITE_ENC,
            .min_key_size = HID_DEFAULT_MIN_KEY_SIZE,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                /* Report Reference Descriptor */
                .uuid = BLE_UUID16_DECLARE(BLE_SVC_CHR_DSC_UUID16_REPORT_REF),
                .att_flags = BLE_ATT_F_READ,
                .access_cb = blehogp_svc_hids_report_access,
                .arg = (void *)HANDLE_HID_REPORT_CONSUMER,
                .min_key_size = HID_DEFAULT_MIN_KEY_SIZE,
            }, {
                0, /* No more descriptors in this characteristic. */
            } },
        }, {
            0, /* No more characteristics in this service */
        }, }
    },

    {
        0, /* No more services. */
    },
};

int
gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

int
gatt_svr_chr_notify(uint16_t conn_handle, uint16_t value_handle, uint8_t *buf, uint8_t len)
{
    struct os_mbuf *om;
    int rc;

    om = ble_hs_mbuf_from_flat(buf, len);
    if (!om) {
        return -1;
    }

    rc = ble_gatts_notify_custom(conn_handle, value_handle, om);
    if (rc) {
        return -1;
    }

    return 0;
}

int
blehogp_svc_hids_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
    int rc;

    printf("%s: UUID %04X attr %04X arg %d op %d\n", __FUNCTION__,
        uuid16, attr_handle, (int)arg, ctxt->op);

    switch (uuid16) {

    case BLE_SVC_HIDS_CHR_UUID16_INFORMATION:
        if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
            printf("invalid op %d\n", ctxt->op);
            break;
        }
        rc = os_mbuf_append(ctxt->om, hid_info,
                            HID_INFORMATION_LEN);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case BLE_SVC_HIDS_CHR_UUID16_CONTROL_POINT: {
        if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
            printf("invalid op %d\n", ctxt->op);
            break;
        }

        uint8_t new_suspend_state;

        rc = gatt_svr_chr_write(ctxt->om, 1, 1, &new_suspend_state, NULL);
        if (!rc) {
            printf("HID_CONTROL_POINT received new suspend state: %d\n",
                (int)new_suspend_state);
        }
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    case BLE_SVC_HIDS_CHR_UUID16_REPORT_MAP: {
        if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
            printf("invalid op %d\n", ctxt->op);
            break;
        }

        rc = os_mbuf_append(ctxt->om, hid_report_map, sizeof(hid_report_map));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    case BLE_SVC_CHR_DSC_UUID16_EXTERNAL_REPORT_REF: {
        if (ctxt->op != BLE_GATT_ACCESS_OP_READ_DSC) {
            printf("invalid op %d\n", ctxt->op);
            break;
        }

        rc = os_mbuf_append(ctxt->om, &hid_external_report_ref, sizeof(hid_external_report_ref));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    case BLE_SVC_HIDS_CHR_UUID16_PROTO_MODE: {

        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {

            rc = os_mbuf_append(ctxt->om, &hid_proto_mode,
                                sizeof(hid_proto_mode));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {

            uint8_t new_protocol_mode;

            rc = gatt_svr_chr_write(ctxt->om, 1, sizeof(new_protocol_mode),
                &new_protocol_mode, NULL);
            if (!rc) {
                hid_proto_mode = new_protocol_mode;

                printf("HID_PROTO_MODE received new protocol mode: %d\n",
                    (int)new_protocol_mode);
            }

            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        } else {
            printf("invalid op %d\n", ctxt->op);
        }
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    default:
        printf("invalid UUID %02X\n", uuid16);
        break;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/* Report access function for all reports */

int
blehogp_svc_hids_report_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
    int handle_num = (int) arg;
    int rc = BLE_ATT_ERR_UNLIKELY;

    printf("%s: UUID %04X attr %04X arg %d op %d\n",
         __FUNCTION__, uuid16, attr_handle, (int)arg, ctxt->op);

    do {
        // Report reference descriptors
        if (uuid16 == BLE_SVC_CHR_DSC_UUID16_REPORT_REF) {
            if (ctxt->op != BLE_GATT_ACCESS_OP_READ_DSC) {
                printf("invalid op %d\n", ctxt->op);
                break;
            }
            int rpt_ind = -1;

            for (int i = 0; i < hid_report_ref_data_count; ++i) {
                if ((int)hid_report_ref_data[i].id == handle_num) {
                    rpt_ind = i;
                    break;
                }
            }
            if (rpt_ind != -1) {
                rc = os_mbuf_append(ctxt->om,
                    (uint8_t *)hid_report_ref_data[rpt_ind].report_ref,
                    HID_REPORT_REF_LEN);
                if (rc) {
                    rc = BLE_ATT_ERR_INSUFFICIENT_RES;
                }
            } else {
                rc = BLE_ATT_ERR_UNLIKELY;
            }

            break;
        }

        /* reports read */
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR &&
                (uuid16 == BLE_SVC_HIDS_CHR_UUID16_REPORT)) {
            uint8_t *report;
            uint8_t report_len;

            if (handle_num == HANDLE_HID_REPORT_KEYBOARD) {
                report = hid_report_kb_in;
                report_len = sizeof(hid_report_kb_in);
            } else if (handle_num == HANDLE_HID_REPORT_KEYBOARD_LED) {
                report = hid_report_kb_out;
                report_len = sizeof(hid_report_kb_out);
            } else if (handle_num == HANDLE_HID_REPORT_KEYBOARD_FEAT) {
                report = hid_report_kb_feat;
                report_len = sizeof(hid_report_kb_feat);
            } else if (handle_num == HANDLE_HID_REPORT_MOUSE) {
                report = hid_report_mouse;
                report_len = sizeof(hid_report_mouse);
            } else if (handle_num == HANDLE_HID_REPORT_CONSUMER) {
                report = hid_report_consumer;
                report_len = sizeof(hid_report_consumer);
            }
            rc = os_mbuf_append(ctxt->om, report, report_len);
            if (rc) {
                rc = BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            break;
        }

        /* reports write */
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR &&
                (uuid16 == BLE_SVC_HIDS_CHR_UUID16_REPORT)) {

            if (handle_num == HANDLE_HID_REPORT_KEYBOARD_LED) {
                gatt_svr_chr_write(ctxt->om, 1, 1, &hid_report_kb_out, NULL);

                if (hid_report_kb_out[0] & (1 << 0)) {
                    printf("NumLock ON\n");
                } else {
                    printf("NumLock OFF\n");
                }

                if (hid_report_kb_out[0] & (1 << 1)) {
                    printf("CapsLock ON\n");
                } else {
                    printf("CapsLock OFF\n");
                }

            } else if (handle_num == HANDLE_HID_REPORT_KEYBOARD_FEAT) {
                printf("Keyboard feature write\n");
            }

            rc = 0;

            break;
        }

    } while (0);

    return rc;
}


/**
 * Initialize the DIS package.
 */
void
blehogp_svc_hids_init(void)
{
    int rc;

    memset(&hids_char_handles, 0, sizeof(hids_char_handles[0]) * HANDLE_HID_COUNT);
    memset(hid_report_kb_in, 0, sizeof(hid_report_kb_in));
    memset(hid_report_kb_out, 0, sizeof(hid_report_kb_out));
    memset(hid_report_kb_feat, 0, sizeof(hid_report_kb_feat));
    memset(hid_report_mouse, 0, sizeof(hid_report_mouse));
    memset(hid_report_consumer, 0, sizeof(hid_report_consumer));

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = ble_gatts_count_cfg(blehogp_svc_hids_defs);
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = ble_gatts_add_svcs(blehogp_svc_hids_defs);
    SYSINIT_PANIC_ASSERT(rc == 0);
}

#ifdef CONFIG_CMDLINE

#include "cli.h"

extern uint16_t g_conn_handle;

int
do_ble_send_key(int argc, char *argv[])
{
    uint16_t value_handle;
    char key;
    uint8_t modifier = 0;
    uint8_t keycode = 0;
    int rc;

    if (argc != 2) {
        return CMD_RET_USAGE;
    }

    value_handle = hids_char_handles[HANDLE_HID_REPORT_KEYBOARD];

    if (strlen(argv[1]) > 1 && !strcmp(argv[1], "num")) {
        keycode = HID_KEY_NUM_LOCK;
    } else if (strlen(argv[1]) > 1 && !strcmp(argv[1], "caps")) {
        keycode = HID_KEY_CAPS_LOCK;
    } else if (strlen(argv[1]) == 1) {
        key = argv[1][0];
        if (key >= 'a' && key <= 'z') {
            keycode = HID_KEY_A + key - 'a' ;
        } else if (key >= 'A' && key <= 'Z') {
            keycode = HID_KEY_A + key - 'A' ;
            modifier = 1 << 1; /* left shift */
        } else if (key >= '0' && key <= '9') {
            if (key == 0) {
                keycode = HID_KEY_0;
            } else {
                keycode = HID_KEY_1 + (key - '1');
            }
        } else {
            return CMD_RET_USAGE;
        }
    } else {
        return CMD_RET_USAGE;
    }

    printf("blekey conn=0x%x, attr=0x%04x, modifier=%02x, keycode=0x%02x\n",
            g_conn_handle, value_handle, modifier, keycode);

    /* key press */
    hid_report_kb_in[0] = modifier;
    hid_report_kb_in[2] = keycode;
    rc = gatt_svr_chr_notify(g_conn_handle, value_handle,
            hid_report_kb_in, sizeof(hid_report_kb_in));
    if (rc) {
        printf("failed to send notification\n");
        return CMD_RET_FAILURE;
    }

    /* key release */
    hid_report_kb_in[0] = 0;
    hid_report_kb_in[2] = 0;
    rc = gatt_svr_chr_notify(g_conn_handle, value_handle,
            hid_report_kb_in, sizeof(hid_report_kb_in));
    if (rc) {
        printf("failed to send notification\n");
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

HOGP_CMD(key, do_ble_send_key, "ble send key",
    "key [character (a-z)|(A-Z)|(0~9)|(num)|(caps)]"
);

int
do_ble_send_mouse_pointer(int argc, char *argv[])
{
    uint16_t value_handle;
    int rc;

    if (argc != 3) {
        return CMD_RET_USAGE;
    }

    value_handle = hids_char_handles[HANDLE_HID_REPORT_MOUSE],
    hid_report_mouse[0] = 0;
    hid_report_mouse[1] = atoi(argv[1]);
    hid_report_mouse[2] = atoi(argv[2]);
    hid_report_mouse[3] = 0;

    printf("blemouse conn=0x%x, attr=0x%04x, x=%d, y=%d\n",
            g_conn_handle, value_handle, hid_report_mouse[1], hid_report_mouse[2]);

    rc = gatt_svr_chr_notify(g_conn_handle, value_handle, hid_report_mouse, sizeof(hid_report_mouse));
    if (rc) {
        printf("failed to send notification\n");
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

HOGP_CMD(point, do_ble_send_mouse_pointer, "ble send mouse pointer",
    "point x y"
);

int
do_ble_send_mouse_button(int argc, char *argv[])
{
    uint16_t value_handle;
    int rc;

    if (argc != 2) {
        return CMD_RET_USAGE;
    }

    value_handle = hids_char_handles[HANDLE_HID_REPORT_MOUSE];
    if (strcmp(argv[1], "left") == 0) {
        hid_report_mouse[0] = 1 << 0;
    } else if (strcmp(argv[1], "right") == 0) {
        hid_report_mouse[0] = 1 << 1;
    } else if (strcmp(argv[1], "middle") == 0) {
        hid_report_mouse[0] = 1 << 2;
    } else {
        return CMD_RET_USAGE;
    }
    hid_report_mouse[1] = 0;
    hid_report_mouse[2] = 0;

    printf("blemouse conn=0x%x, attr=0x%04x, button=%d\n",
            g_conn_handle, value_handle, hid_report_mouse[0]);

    rc = gatt_svr_chr_notify(g_conn_handle, value_handle, hid_report_mouse, sizeof(hid_report_mouse));
    if (rc) {
        printf("failed to send notification\n");
        return CMD_RET_FAILURE;
    }

    hid_report_mouse[0] = 0;
    rc = gatt_svr_chr_notify(g_conn_handle, value_handle, hid_report_mouse, sizeof(hid_report_mouse));
    if (rc) {
        printf("failed to send notification\n");
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

HOGP_CMD(button, do_ble_send_mouse_button, "ble send mouse pointer",
    "button [left|middle|right]"
);

int
do_ble_send_vol(int argc, char *argv[])
{
    struct os_mbuf *om;
    uint16_t value_handle;
    char key;
    uint8_t keycode = 0;
    int rc;

    if (argc != 2) {
        return CMD_RET_USAGE;
    }

    value_handle = hids_char_handles[HANDLE_HID_REPORT_CONSUMER],

    key = argv[1][0];
    if (key == '+') {
        keycode = 1 << 0;
    } else if (key == '-') {
        keycode = 1 << 1;
    } else {
        return CMD_RET_USAGE;
    }

    printf("blevol conn=0x%x, attr=0x%04x, volume=0x%02x\n",
            g_conn_handle, value_handle, keycode);

    /* key press */
    hid_report_consumer[0] = keycode;
    rc = gatt_svr_chr_notify(g_conn_handle, value_handle, hid_report_consumer, sizeof(hid_report_consumer));
    if (rc) {
        printf("failed to send notification\n");
        return CMD_RET_FAILURE;
    }

    /* key release */
    hid_report_consumer[0] = 0;
    rc = gatt_svr_chr_notify(g_conn_handle, value_handle, hid_report_consumer, sizeof(hid_report_consumer));
    if (rc) {
        printf("failed to send notification\n");
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

HOGP_CMD(vol, do_ble_send_vol, "ble send keycode",
    "vol [+|-]"
);

#endif

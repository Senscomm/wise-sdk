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

#ifndef H_BLEHOGP_SVC_HIDS_
#define H_BLEHOGP_SVC_HIDS_

#define BLE_SVC_HIDS_UUID16                     0x1812

#define BLE_SVC_HIDS_CHR_UUID16_INFORMATION     0x2A4A
#define BLE_SVC_HIDS_CHR_UUID16_REPORT_MAP      0x2A4B
#define BLE_SVC_HIDS_CHR_UUID16_CONTROL_POINT   0x2A4C
#define BLE_SVC_HIDS_CHR_UUID16_REPORT          0x2A4D
#define BLE_SVC_HIDS_CHR_UUID16_PROTO_MODE      0x2A4E
#define BLE_SVC_HIDS_CHR_UUID16_BT_KB_INPUT     0x2A22
#define BLE_SVC_HIDS_CHR_UUID16_BT_KB_OUTPUT    0x2A32
#define BLE_SVC_HIDS_CHR_UUID16_BT_MOUSE_INPUT  0x2A33


#define HID_DEFAULT_MIN_KEY_SIZE        0


/* Attribute value lengths */
#define HID_PROTOCOL_MODE_LEN           1
#define HID_INFORMATION_LEN             4
#define HID_REPORT_REF_LEN              2
#define HID_EXT_REPORT_REF_LEN          2

// HID Report types
#define HID_REPORT_TYPE_INPUT           1
#define HID_REPORT_TYPE_OUTPUT          2
#define HID_REPORT_TYPE_FEATURE         3

#define HID_CC_RPT_MUTE                 1
#define HID_CC_RPT_POWER                2
#define HID_CC_RPT_LAST                 3
#define HID_CC_RPT_ASSIGN_SEL           4
#define HID_CC_RPT_PLAY                 5
#define HID_CC_RPT_PAUSE                6
#define HID_CC_RPT_RECORD               7
#define HID_CC_RPT_FAST_FWD             8
#define HID_CC_RPT_REWIND               9
#define HID_CC_RPT_SCAN_NEXT_TRK        10
#define HID_CC_RPT_SCAN_PREV_TRK        11
#define HID_CC_RPT_STOP                 12

#define HID_CC_RPT_CHANNEL_UP           0x01
#define HID_CC_RPT_CHANNEL_DOWN         0x03
#define HID_CC_RPT_VOLUME_UP            0x40
#define HID_CC_RPT_VOLUME_DOWN          0x80

// Keyboard report size
#define HIDD_LE_REPORT_KB_IN_SIZE       (8)

// Mouse report size
#define HIDD_LE_REPORT_MOUSE_SIZE       (4)

// LEDS report size
#define HIDD_LE_REPORT_KB_OUT_SIZE      (1)

// Consumer control report size
#define HIDD_LE_REPORT_CC_SIZE          (2)

// battery level data size
#define HIDD_LE_BATTERY_LEVEL_SIZE      (1)

// feature data size
#define HIDD_LE_REPORT_FEATURE          (6)

/* HID information flags */
#define HID_FLAGS_REMOTE_WAKE           0x01      // RemoteWake
#define HID_FLAGS_NORMALLY_CONNECTABLE  0x02      // NormallyConnectable

/* Control point commands */
#define HID_CMD_SUSPEND                 0x00      // Suspend
#define HID_CMD_EXIT_SUSPEND            0x01      // Exit Suspend

/* HID protocol mode values */
#define HID_PROTOCOL_MODE_BOOT          0x00      // Boot Protocol Mode
#define HID_PROTOCOL_MODE_REPORT        0x01      // Report Protocol Mode


void blehogp_svc_hids_init(void);

#endif

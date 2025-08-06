/*
 * Copyright 2011 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PROTO_MCU_H__
#define __AYLA_PROTO_MCU_H__

#include <ayla/utypes.h>

#define MAX_SERIAL_TX_PBUFS	6

/*
 * Ayla cmd structure:
 */
PREPACKED struct ayla_cmd {
	u8	protocol;	/* protocol: see below */
	u8	opcode;		/* opcode: see below */
	be16	req_id;		/* request identifier */
} PACKED;

/*
 * Command opcodes
 */
enum ayla_cmd_op {
	/*
	 * Control ops.
	 */
	ACMD_NOP = 0x00,	/* no-op */
	ACMD_RESP = 0x01,	/* response message */
	ACMD_GET_CONF = 0x02,	/* get configuration item */
	ACMD_SET_CONF = 0x03,	/* set configuration item */
	ACMD_SAVE_CONF = 0x04,	/* save running configuration for startup */
	ACMD_GET_STAT = 0x05,	/* get status variable */
	ACMD_NAK = 0x06,	/* error return */
	ACMD_LOAD_STARTUP = 0x07, /* load startup configuration and reset */
	ACMD_LOAD_FACTORY = 0x08, /* load factory configuration and reset */
	ACMD_OTA_STAT = 0x09,	/* status regarding OTA firmware updates */
	ACMD_OTA_CMD = 0x0a,	/* update module with available OTA */
	ACMD_COMMIT = 0x0b,	/* commit configuration change */
	ACMD_LOG = 0x0c,	/* logging operations */
	ACMD_MCU_OTA = 0x0d,	/* MCU firmware update report/start */
	ACMD_MCU_OTA_LOAD = 0x0e, /* MCU firmware image part */
	ACMD_MCU_OTA_STAT = 0x0f, /* report status of MCU fw update */
	ACMD_MCU_OTA_BOOT = 0x10, /* boot to a different image */
	ACMD_CONF_UPDATE = 0x11, /* config update */
	ACMD_WIFI_JOIN = 0x12,  /* configure and try to join a wifi network */
	ACMD_WIFI_DELETE = 0x13, /* leave and forget a wifi network */
	ACMD_HOST_RESET = 0x14,	/* MCU should reset as soon as possible */
	ACMD_WIFI_ONBOARD = 0x15, /* start Wi-Fi onboarding */
};

enum ayla_data_op {

	/*
	 * Ayla Data Service operations
	 */
	AD_SEND_TLV_V1 = 0x01,	/* send TLVs to data service */
	AD_REQ_TLV = 0x02,	/* request TLVs from data service */
	AD_RECV_TLV = 0x03,	/* TLV values from data service */
	__AD_RESVD_ACK = 0x04,	/* obsolete - Acknowledgement */
	AD_NAK = 0x05,		/* Negative Acknowledgement */
	AD_SEND_PROP = 0x06,	/* request to send property */
	AD_SEND_PROP_RESP = 0x07, /* response to send-property request */
	AD_SEND_NEXT_PROP = 0x08, /* request to send next property */
	AD_SEND_TLV =	0x09,	/* send TLV to data service */
	AD_DP_REQ =	0x0a,	/* Request data point value */
	AD_DP_RESP =	0x0b,	/* Response for data point request */
	AD_DP_CREATE =	0x0c,	/* Create a new data point */
	AD_DP_FETCHED =	0x0d,	/* Indicate current data point fetched */
	AD_DP_STOP =	0x0e,	/* Stop current data point transfer */
	AD_DP_SEND =	0x0f,	/* Send data point value */
	/* AD_DP_STATUS =	0x10,	 get status of data point send */
	AD_CONNECT =	0x11,	/* connectivity status info */
	AD_ECHO_FAIL =	0x12,	/* ADS echo failure for a prop */
	AD_LISTEN_ENB =	0x13,	/* Start accepting cmd/props from ADS */
	AD_ERROR =	0x14,	/*
				 * General error. Not tied to a specific
				 * request from MCU.
				 */
	AD_CONFIRM =	0x15,	/* Confirmation of successful dp post/put */
	AD_PROP_NOTIFY = 0x16,	/* Indication of pending prop update in cloud */
	AD_EVENT =	0x17,	/* Send event notification(s) */
	AD_BATCH_SEND = 0x18,	/* Send batch datapoint */
	AD_BATCH_STATUS = 0x19,	/* Response for batch datapoints */
};

#define ASPI_LEN_MAX	384	/* max length, arbitrary for debugging */

/*
 * Error numbers for NAKs.
 */
#define AERR_TIMEOUT	0x01	/* timeout with data service */
#define AERR_LEN_ERR	0x02	/* TLV extends past end of received buffer */
#define AERR_UNK_TYPE	0x03	/* unknown TLV type */
#define AERR_UNK_VAR	0x04	/* unknown config/status variable */
#define AERR_INVAL_TLV	0x05	/* invalid TLV sequence (e.g. not conf) */
#define AERR_INVAL_OP	0x06	/* invalid opcode */
#define AERR_INVAL_DP	0x07	/* invalid data point location */
#define AERR_INVAL_OFF	0x08	/* invalid offset */
#define AERR_INVAL_REQ	0x09	/* invalid request sequence */
#define AERR_INVAL_NAME 0x0a	/* invalid property name */
#define AERR_CONN_ERR	0x0b	/* connection to the ADS fail */
#define AERR_ADS_BUSY	0x0c	/* ADS busy */
#define AERR_INTERNAL	0x0d	/* internal error */
#define AERR_CHECKSUM	0x0e	/* checksum error */
#define AERR_ALREADY	0x0f	/* already done */
#define AERR_BOOT	0x10	/* MCU did not boot to new image */
#define AERR_OVERFLOW	0x11	/*
				 * MCU took too long to recv data response from
				 * the request it made to the module. It needs
				 * to re-do the request.
				 */
#define AERR_BAD_VAL	0x12	/* bad value */
#define AERR_PROP_LEN	0x13	/* property value too long */
#define AERR_UNEXP_OP	0x14	/* unexpected operation */
#define AERR_DPMETA	0x15	/* datapoint metadata format error */
#define AERR_INVAL_ACK	0x16	/* ack info in incorrect format */
#define AERR_UNK_PROP	0x17	/* property not present on device per ADS  */
#define AERR_INVAL_FMT	0x18	/* wrong format (e.g., not JSON, if required) */
#define AERR_BUSY_RETRY	0x19	/* ADS too busy; retry requested */
#define AERR_RETRY	0x1a	/* retry requested */
#define AERR_TIME_UNK	0x1b	/* time setting required (used in MCU only) */
#define AERR_PARTIAL	0x1c	/* partial success (e.g., of batch, MCU only) */
#define AERR_UNK_STAT	0x1d	/* status unknown (used in MCU only) */
#define AERR_TLV_MISSING 0x1e	/* required TLV not found */

/*
 * MCU Feature Mask Definitions
 * Features Supported by the MCU (Used in ATLV_FEATURES TLV)
 *
 * Note that MCU_LAN_SUPPORT and MCU_OTA_SUPPORT bits are no longer
 * used by the module but are reserved for that purpose.
 */
#define _MCU_LAN_SUPPORT	(1 << 0) /* MCU supports LAN-mode */
#define _MCU_OTA_SUPPORT	(1 << 1) /* MCU supports host OTA upgragdes */
#define MCU_TIME_SUBSCRIPTION	(1 << 2) /* MCU wants time-related updates */
#define MCU_DATAPOINT_CONFIRM	(1 << 3) /* MCU wants confirms on dp posts */

#endif /* __AYLA_PROTO_MCU_H__ */

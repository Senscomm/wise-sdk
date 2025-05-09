/*
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2022 Senscomm Semiconductor Co., Ltd.
 *
 */

// SPDX-License-Identifier: ISC
/*
 * Copyright (c) 2012 Broadcom Corporation
 */

#ifndef FWEH_H_
#define FWEH_H_

#include <hal/types.h>
#include <freebsd/ethernet.h>

#define __be16	u16
#define __be32	u32
#define __be64	u64

/* list of firmware events */
#define SNCMF_FWEH_EVENT_ENUM_DEFLIST \
	SNCMF_ENUM_DEF(SET_SSID, 0) \
	SNCMF_ENUM_DEF(JOIN, 1) \
	SNCMF_ENUM_DEF(START, 2) \
	SNCMF_ENUM_DEF(AUTH, 3) \
	SNCMF_ENUM_DEF(AUTH_IND, 4) \
	SNCMF_ENUM_DEF(DEAUTH, 5) \
	SNCMF_ENUM_DEF(DEAUTH_IND, 6) \
	SNCMF_ENUM_DEF(ASSOC, 7) \
	SNCMF_ENUM_DEF(ASSOC_IND, 8) \
	SNCMF_ENUM_DEF(REASSOC, 9) \
	SNCMF_ENUM_DEF(REASSOC_IND, 10) \
	SNCMF_ENUM_DEF(DISASSOC, 11) \
	SNCMF_ENUM_DEF(DISASSOC_IND, 12) \
	SNCMF_ENUM_DEF(QUIET_START, 13) \
	SNCMF_ENUM_DEF(QUIET_END, 14) \
	SNCMF_ENUM_DEF(BEACON_RX, 15) \
	SNCMF_ENUM_DEF(LINK, 16) \
	SNCMF_ENUM_DEF(MIC_ERROR, 17) \
	SNCMF_ENUM_DEF(NDIS_LINK, 18) \
	SNCMF_ENUM_DEF(ROAM, 19) \
	SNCMF_ENUM_DEF(TXFAIL, 20) \
	SNCMF_ENUM_DEF(PMKID_CACHE, 21) \
	SNCMF_ENUM_DEF(RETROGRADE_TSF, 22) \
	SNCMF_ENUM_DEF(PRUNE, 23) \
	SNCMF_ENUM_DEF(AUTOAUTH, 24) \
	SNCMF_ENUM_DEF(EAPOL_MSG, 25) \
	SNCMF_ENUM_DEF(SCAN_COMPLETE, 26) \
	SNCMF_ENUM_DEF(ADDTS_IND, 27) \
	SNCMF_ENUM_DEF(DELTS_IND, 28) \
	SNCMF_ENUM_DEF(BCNSENT_IND, 29) \
	SNCMF_ENUM_DEF(BCNRX_MSG, 30) \
	SNCMF_ENUM_DEF(BCNLOST_MSG, 31) \
	SNCMF_ENUM_DEF(ROAM_PREP, 32) \
	SNCMF_ENUM_DEF(PFN_NET_FOUND, 33) \
	SNCMF_ENUM_DEF(PFN_NET_LOST, 34) \
	SNCMF_ENUM_DEF(RESET_COMPLETE, 35) \
	SNCMF_ENUM_DEF(JOIN_START, 36) \
	SNCMF_ENUM_DEF(ROAM_START, 37) \
	SNCMF_ENUM_DEF(ASSOC_START, 38) \
	SNCMF_ENUM_DEF(IBSS_ASSOC, 39) \
	SNCMF_ENUM_DEF(RADIO, 40) \
	SNCMF_ENUM_DEF(PSM_WATCHDOG, 41) \
	SNCMF_ENUM_DEF(PROBREQ_MSG, 44) \
	SNCMF_ENUM_DEF(SCAN_CONFIRM_IND, 45) \
	SNCMF_ENUM_DEF(PSK_SUP, 46) \
	SNCMF_ENUM_DEF(COUNTRY_CODE_CHANGED, 47) \
	SNCMF_ENUM_DEF(EXCEEDED_MEDIUM_TIME, 48) \
	SNCMF_ENUM_DEF(ICV_ERROR, 49) \
	SNCMF_ENUM_DEF(UNICAST_DECODE_ERROR, 50) \
	SNCMF_ENUM_DEF(MULTICAST_DECODE_ERROR, 51) \
	SNCMF_ENUM_DEF(TRACE, 52) \
	SNCMF_ENUM_DEF(IF, 54) \
	SNCMF_ENUM_DEF(P2P_DISC_LISTEN_COMPLETE, 55) \
	SNCMF_ENUM_DEF(RSSI, 56) \
	SNCMF_ENUM_DEF(EXTLOG_MSG, 58) \
	SNCMF_ENUM_DEF(ACTION_FRAME, 59) \
	SNCMF_ENUM_DEF(ACTION_FRAME_COMPLETE, 60) \
	SNCMF_ENUM_DEF(PRE_ASSOC_IND, 61) \
	SNCMF_ENUM_DEF(PRE_REASSOC_IND, 62) \
	SNCMF_ENUM_DEF(CHANNEL_ADOPTED, 63) \
	SNCMF_ENUM_DEF(AP_STARTED, 64) \
	SNCMF_ENUM_DEF(DFS_AP_STOP, 65) \
	SNCMF_ENUM_DEF(DFS_AP_RESUME, 66) \
	SNCMF_ENUM_DEF(ESCAN_RESULT, 69) \
	SNCMF_ENUM_DEF(ACTION_FRAME_OFF_CHAN_COMPLETE, 70) \
	SNCMF_ENUM_DEF(PROBERESP_MSG, 71) \
	SNCMF_ENUM_DEF(P2P_PROBEREQ_MSG, 72) \
	SNCMF_ENUM_DEF(DCS_REQUEST, 73) \
	SNCMF_ENUM_DEF(FIFO_CREDIT_MAP, 74) \
	SNCMF_ENUM_DEF(ACTION_FRAME_RX, 75) \
	SNCMF_ENUM_DEF(TDLS_PEER_EVENT, 92) \
	SNCMF_ENUM_DEF(MLME_MGMT_RX, 93) \
	SNCMF_ENUM_DEF(MLME_MGMT_TX, 94) \
	SNCMF_ENUM_DEF(GOT_IP, 95) \
	SNCMF_ENUM_DEF(SNCM_CHANNEL, 126) \
	SNCMF_ENUM_DEF(BCMC_CREDIT_SUPPORT, 127)

#define SNCMF_ENUM_DEF(id, val) \
	SNCMF_E_##id = (val),

/* firmware event codes sent by the dongle */
enum sncmf_fweh_event_code {
	SNCMF_FWEH_EVENT_ENUM_DEFLIST
	/* this determines event mask length which must match
	 * minimum length check in device firmware so it is
	 * hard-coded here.
	 */
	SNCMF_E_LAST = 139
};
#undef SNCMF_ENUM_DEF

#define SNCMF_EVENTING_MASK_LEN		DIV_ROUND_UP(SNCMF_E_LAST, 8)
#define SNCMF_MGMT_TXCB_LEN			16

/* flags field values in struct sncmf_event_msg */
#define SNCMF_EVENT_MSG_LINK		0x01
#define SNCMF_EVENT_MSG_FLUSHTXQ	0x02
#define SNCMF_EVENT_MSG_GROUP		0x04

/* status field values in struct sncmf_event_msg */
#define SNCMF_E_STATUS_SUCCESS			0
#define SNCMF_E_STATUS_FAIL			1
#define SNCMF_E_STATUS_TIMEOUT			2
#define SNCMF_E_STATUS_NO_NETWORKS		3
#define SNCMF_E_STATUS_ABORT			4
#define SNCMF_E_STATUS_NO_ACK			5
#define SNCMF_E_STATUS_UNSOLICITED		6
#define SNCMF_E_STATUS_ATTEMPT			7
#define SNCMF_E_STATUS_PARTIAL			8
#define SNCMF_E_STATUS_NEWSCAN			9
#define SNCMF_E_STATUS_NEWASSOC			10
#define SNCMF_E_STATUS_11HQUIET			11
#define SNCMF_E_STATUS_SUPPRESS			12
#define SNCMF_E_STATUS_NOCHANS			13
#define SNCMF_E_STATUS_CS_ABORT			15
#define SNCMF_E_STATUS_ERROR			16

/* status field values for PSK_SUP event */
#define SNCMF_E_STATUS_FWSUP_WAIT_M1		4
#define SNCMF_E_STATUS_FWSUP_PREP_M2		5
#define SNCMF_E_STATUS_FWSUP_COMPLETED		6
#define SNCMF_E_STATUS_FWSUP_TIMEOUT		7
#define SNCMF_E_STATUS_FWSUP_WAIT_M3		8
#define SNCMF_E_STATUS_FWSUP_PREP_M4		9
#define SNCMF_E_STATUS_FWSUP_WAIT_G1		10
#define SNCMF_E_STATUS_FWSUP_PREP_G2		11

/* reason field values in struct sncmf_event_msg */
#define SNCMF_E_REASON_INITIAL_ASSOC		0
#define SNCMF_E_REASON_LOW_RSSI			1
#define SNCMF_E_REASON_DEAUTH			2
#define SNCMF_E_REASON_DISASSOC			3
#define SNCMF_E_REASON_BCNS_LOST		4
#define SNCMF_E_REASON_MINTXRATE		9
#define SNCMF_E_REASON_TXFAIL			10

#define SNCMF_E_REASON_LINK_BSSCFG_DIS		4
#define SNCMF_E_REASON_FAST_ROAM_FAILED		5
#define SNCMF_E_REASON_DIRECTED_ROAM		6
#define SNCMF_E_REASON_TSPEC_REJECTED		7
#define SNCMF_E_REASON_BETTER_AP		8

#define SNCMF_E_REASON_TDLS_PEER_DISCOVERED	0
#define SNCMF_E_REASON_TDLS_PEER_CONNECTED	1
#define SNCMF_E_REASON_TDLS_PEER_DISCONNECTED	2

/* reason field values for PSK_SUP event */
#define SNCMF_E_REASON_FWSUP_OTHER		0
#define SNCMF_E_REASON_FWSUP_DECRYPT_KEY_DATA	1
#define SNCMF_E_REASON_FWSUP_BAD_UCAST_WEP128	2
#define SNCMF_E_REASON_FWSUP_BAD_UCAST_WEP40	3
#define SNCMF_E_REASON_FWSUP_UNSUP_KEY_LEN	4
#define SNCMF_E_REASON_FWSUP_PW_KEY_CIPHER	5
#define SNCMF_E_REASON_FWSUP_MSG3_TOO_MANY_IE	6
#define SNCMF_E_REASON_FWSUP_MSG3_IE_MISMATCH	7
#define SNCMF_E_REASON_FWSUP_NO_INSTALL_FLAG	8
#define SNCMF_E_REASON_FWSUP_MSG3_NO_GTK	9
#define SNCMF_E_REASON_FWSUP_GRP_KEY_CIPHER	10
#define SNCMF_E_REASON_FWSUP_GRP_MSG1_NO_GTK	11
#define SNCMF_E_REASON_FWSUP_GTK_DECRYPT_FAIL	12
#define SNCMF_E_REASON_FWSUP_SEND_FAIL		13
#define SNCMF_E_REASON_FWSUP_DEAUTH		14
#define SNCMF_E_REASON_FWSUP_WPA_PSK_TMO	15
#define SNCMF_E_REASON_FWSUP_WPA_PSK_M1_TMO	16
#define SNCMF_E_REASON_FWSUP_WPA_PSK_M3_TMO	17

/* action field values for sncmf_ifevent */
#define SNCMF_E_IF_ADD				1
#define SNCMF_E_IF_DEL				2
#define SNCMF_E_IF_CHANGE			3

/* flag field values for sncmf_ifevent */
#define SNCMF_E_IF_FLAG_NOIF			1

/* role field values for sncmf_ifevent */
#define SNCMF_E_IF_ROLE_STA			0
#define SNCMF_E_IF_ROLE_AP			1
#define SNCMF_E_IF_ROLE_WDS			2
#define SNCMF_E_IF_ROLE_P2P_GO			3
#define SNCMF_E_IF_ROLE_P2P_CLIENT		4

/**
 * definitions for event packet validation.
 */
#define SNCM_OUI				"\x64\xf9\x47"
#define SCMILCP_SCM_SUBTYPE_EVENT		1
#define SCMILCP_SUBTYPE_VENDOR_LONG		32769

/**
 * struct sncm_ethhdr - senscomm specific ether header.
 *
 * @subtype: subtype for this packet.
 * @length: length of appended data.
 * @version: version indication.
 * @oui: OUI of this packet.
 * @usr_subtype: subtype for this OUI.
 */
struct sncm_ethhdr {
	__be16 subtype;
	__be16 length;
	u8 version;
	u8 oui[3];
	__be16 usr_subtype;
} __packed;

struct sncmf_event_msg_be {
	__be16 version;
	__be16 flags;
	__be32 event_type;
	__be32 status;
	__be32 reason;
	__be32 auth_type;
	__be32 datalen;
	u8 addr[ETHER_ADDR_LEN];
	char ifname[IFNAMSIZ];
	u8 ifidx;
	u8 bsscfgidx;
} __packed;

/**
 * struct sncmf_event - contents of senscomm event packet.
 *
 * @eth: standard ether header.
 * @hdr: senscomm specific ether header.
 * @msg: common part of the actual event message.
 */
struct sncmf_event {
	struct ether_header eth;
	struct sncm_ethhdr hdr;
	struct sncmf_event_msg_be msg;
} __packed;

/**
 * struct sncmf_event_msg - firmware event message.
 *
 * @version: version information.
 * @flags: event flags.
 * @event_code: firmware event code.
 * @status: status information.
 * @reason: reason code.
 * @auth_type: authentication type.
 * @datalen: length of event data buffer.
 * @addr: ether address.
 * @ifname: interface name.
 * @ifidx: interface index.
 * @bsscfgidx: bsscfg index.
 */
struct sncmf_event_msg {
	u16 version;
	u16 flags;
	u32 event_code;
	u32 status;
	u32 reason;
	s32 auth_type;
	u32 datalen;
	u8 addr[ETHER_ADDR_LEN];
	char ifname[IFNAMSIZ];
	u8 ifidx;
	u8 bsscfgidx;
};

struct sncmf_if_event {
	u8 ifidx;
	u8 action;
	u8 flags;
	u8 bsscfgidx;
	u8 role;
};

void fweh_init(void);
int fweh_set_event_mask(u8 ifidx, u8 *mask);
int fweh_set_mgt_txcb(u8 ifidx, u8 *subtype);
int fweh_send_event(struct ieee80211vap *vap, u32 code, u16 flags,
		u32 status, u32 reason, u32 auth_type,
		u8 *data, u32 datalen);
int fweh_send_if_event(u8 ifidx, struct sncmf_if_event *evt,
		struct ieee80211vap *vap);

void fweh_send_fws_credit(uint32_t *fifo_credit, int len);

#if defined(CONFIG_AT_OVER_SCDC) || defined(CONFIG_SCDC_SUPPORT_SCM_CHANNEL)
void fweh_event_tx(void *event, size_t size);
#endif

#ifdef CONFIG_SCDC_SUPPORT_SCM_CHANNEL
int fweh_sncm_channel_tx(char *buf, int len);
#endif

#endif /* FWEH_H_ */

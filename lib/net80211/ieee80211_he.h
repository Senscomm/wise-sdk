/*-
 * Copyright (c) 2016 Adrian Chadd <adrian@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _NET80211_IEEE80211_HE_H_
#define _NET80211_IEEE80211_HE_H_

#define	IEEE80211_PLUSHTC_LEN						4
#define	IEEE80211_PLUSHTC_CID_MASK					0x0000003c
#define	IEEE80211_PLUSHTC_CID_SHIFT					2
#define	IEEE80211_PLUSHTC_CINFO_MASK				0xffffffc0
#define	IEEE80211_PLUSHTC_CINFO_SHIFT				6
#define	IEEE80211_PLUSHTC_A_CONTROL_SHIFT			4
#define	IEEE80211_PLUSHTC_VARIANT_SHIFT				2
#define	IEEE80211_PLUSHTC_HE_VARIANT				3

enum htc_he_ctl {
	HTC_HE_TRS = 0,
	HTC_HE_OM = 1,
	HTC_HE_HLA = 2,
	HTC_HE_BSR = 3,
	HTC_HE_UPH = 4,
	HTC_HE_BQR = 5,
	HTC_HE_CAS = 6,
	HTC_HE_Rsvd = 7,
	HTC_HE_ONES = 26,
};

#define	IEEE80211_OM_RX_NSS_MASK					0x0000007
#define	IEEE80211_OM_RX_NSS_SHIFT					0
#define	IEEE80211_OM_CH_WIDTH_MASK					0x0000018
#define	IEEE80211_OM_CH_WIDTH_SHIFT					3
#define	IEEE80211_OM_UL_MU_DIS_MASK					0x0000020
#define	IEEE80211_OM_UL_MU_DIS_SHIFT				5
#define	IEEE80211_OM_TX_NSTS_MASK					0x00001c0
#define	IEEE80211_OM_TX_NSTS_SHIFT					6
#define	IEEE80211_OM_ER_SU_DIS_MASK					0x0000200
#define	IEEE80211_OM_ER_SU_DIS_SHIFT				9
#define	IEEE80211_OM_DL_MU_MIMO_RR_MASK				0x0000400
#define	IEEE80211_OM_DL_MU_MIMO_RR_SHIFT			10
#define	IEEE80211_OM_UL_MU_DATA_DIS_MASK			0x0000800
#define	IEEE80211_OM_UL_MU_DATA_DIS_SHIFT			11

#define	IEEE80211_HLA_UNS_MFB_MASK					0x0000001
#define	IEEE80211_HLA_UNS_MFB_SHIFT					0
#define	IEEE80211_HLA_MRQ_MASK						0x0000002
#define	IEEE80211_HLA_MRQ_SHIFT						1
#define	IEEE80211_HLA_NSS_MASK						0x000001c
#define	IEEE80211_HLA_NSS_SHIFT						2
#define	IEEE80211_HLA_HE_MCS_MASK					0x00001e0
#define	IEEE80211_HLA_HE_MCS_SHIFT					5
#define	IEEE80211_HLA_DCM_MASK						0x0000200
#define	IEEE80211_HLA_DCM_SHIFT						9
#define	IEEE80211_HLA_RU_ALLOC_MASK					0x003fc00
#define	IEEE80211_HLA_RU_ALLOC_SHIFT				10
#define	IEEE80211_HLA_BW_MASK						0x00c0000
#define	IEEE80211_HLA_BW_SHIFT						18
#define	IEEE80211_HLA_MSI_PPP_MASK					0x0700000
#define	IEEE80211_HLA_MSI_PPP_SHIFT					20
#define	IEEE80211_HLA_TX_BF_MASK					0x0800000
#define	IEEE80211_HLA_TX_BF_SHIFT					23
#define	IEEE80211_HLA_UL_TB_MFB_MASK				0x1000000
#define	IEEE80211_HLA_UL_TB_MFB_SHIFT				24
#define	IEEE80211_HLA_RSVD_MASK						0x2000000
#define	IEEE80211_HLA_RSVD_SHIFT					25

#ifdef CONFIG_SUPPORT_TWT

#define IEEE80211_TWT_TRIGGER_BY_SUPPLICANT 0
#define IEEE80211_TWT_TRIGGER_BY_AUTO 		1

#define IEEE80211_GET_TWT_STATE(_vap) (_vap)->iv_he_twt_info.twt_state

enum twt_timer_type {
	WAKE_TIMER		= 1,
	DOZE_TIMER		= 2,
};

int _ieee80211_twt_set_action(struct ieee80211vap *vap, u8 twt_action, const struct ieee80211_he_twt_params *temp_twt_params);
extern int (*ieee80211_twt_set_action)(struct ieee80211vap *vap, u8 twt_action, const struct ieee80211_he_twt_params *temp_twt_params);

void _ieee80211_set_twt_state(struct ieee80211vap *vap, enum ieee80211_twt_state new_state);
extern void (*ieee80211_set_twt_state)(struct ieee80211vap * vap, enum ieee80211_twt_state new_state);
#endif

void _ieee80211_he_attach(struct ieee80211com *);
extern void (*ieee80211_he_attach)(struct ieee80211com * ic);

void ieee80211_he_detach(struct ieee80211com *);
void _ieee80211_he_vattach(struct ieee80211vap *);
extern void (*ieee80211_he_vattach)(struct ieee80211vap * vap);

void _ieee80211_he_vdetach(struct ieee80211vap *);
extern void (*ieee80211_he_vdetach)(struct ieee80211vap * vap);

void ieee80211_he_announce(struct ieee80211com *);

void _ieee80211_he_node_init(struct ieee80211_node *);
extern void (*ieee80211_he_node_init)(struct ieee80211_node * ni);

void _ieee80211_he_node_cleanup(struct ieee80211_node *);
extern void (*ieee80211_he_node_cleanup)(struct ieee80211_node * ni);

void _ieee80211_parse_heop(struct ieee80211_node *, const uint8_t *);
extern void (*ieee80211_parse_heop)(struct ieee80211_node * ni, const uint8_t * ie);

void _ieee80211_parse_hecap(struct ieee80211_node *, const uint8_t *);
extern void (*ieee80211_parse_hecap)(struct ieee80211_node * ni, const uint8_t * ie);

void ieee80211_parse_uora(struct ieee80211_node *, const uint8_t *);
void ieee80211_parse_muedca(struct ieee80211_node *, const uint8_t *);
void _ieee80211_parse_srparam(struct ieee80211_node *, const uint8_t *);
extern void (*ieee80211_parse_srparam)(struct ieee80211_node *, const uint8_t *);

void ieee80211_parse_ndpparam(struct ieee80211_node *, const uint8_t *);
void ieee80211_parse_bsscolor(struct ieee80211_node *, const uint8_t *);
void ieee80211_parse_qtpsetup(struct ieee80211_node *, const uint8_t *);
void ieee80211_parse_essrpt(struct ieee80211_node *, const uint8_t *);
void ieee80211_parse_ops(struct ieee80211_node *, const uint8_t *);
void ieee80211_parse_hebssload(struct ieee80211_node *, const uint8_t *);
void ieee80211_parse_multibssid(struct ieee80211_node *, const uint8_t *);
void ieee80211_parse_knownbssid(struct ieee80211_node *, const uint8_t *);
void ieee80211_parse_shortssid(struct ieee80211_node *, const uint8_t *);
void ieee80211_parse_he6gcap(struct ieee80211_node *, const uint8_t *);
void ieee80211_parse_ulmupwrcap(struct ieee80211_node *, const uint8_t *);

int _ieee80211_he_updateparams(struct ieee80211_node *, const uint8_t *, const uint8_t *);
extern int (*ieee80211_he_updateparams)(struct ieee80211_node *, const uint8_t *, const uint8_t *);

void _ieee80211_setup_he_rates(struct ieee80211_node *, const uint8_t *, const uint8_t *);
extern void (*ieee80211_setup_he_rates)(struct ieee80211_node *, const uint8_t *, const uint8_t *);

void _ieee80211_he_timeout(struct ieee80211com *ic);
extern void (*ieee80211_he_timeout)(struct ieee80211com * ic);

void _ieee80211_he_node_join(struct ieee80211_node *ni);
extern void (*ieee80211_he_node_join)(struct ieee80211_node * ni);

void _ieee80211_he_node_leave(struct ieee80211_node *ni);
extern void (*ieee80211_he_node_leave)(struct ieee80211_node * ni);

uint8_t *_ieee80211_add_hecap(uint8_t * frm, struct ieee80211_node *);
extern uint8_t *(*ieee80211_add_hecap) (uint8_t * frm, struct ieee80211_node * ni);

uint8_t *_ieee80211_add_hecap_vap(uint8_t * frm, struct ieee80211vap *);
extern uint8_t *(*ieee80211_add_hecap_vap) (uint8_t * frm, struct ieee80211vap * vap);

uint8_t *_ieee80211_add_heinfo(uint8_t * frm, struct ieee80211_node *);
extern uint8_t *(*ieee80211_add_heinfo) (uint8_t * frm, struct ieee80211_node * ni);

void _ieee80211_he_update_cap(struct ieee80211_node *, const uint8_t *);
extern void (*ieee80211_he_update_cap)(struct ieee80211_node * ni, const uint8_t * hecap_ie);

struct ieee80211_channel *
	ieee80211_he_adjust_channel(struct ieee80211com *,
	    struct ieee80211_channel *, int);

void _ieee80211_he_get_hecap_ie(struct ieee80211_node *ni, struct ieee80211_ie_hecap *, int);
extern void (*ieee80211_he_get_hecap_ie)(struct ieee80211_node *ni, struct ieee80211_ie_hecap *, int);

void ieee80211_he_get_heinfo_ie(struct ieee80211_node *ni, struct ieee80211_ie_he_operation *, int);

void ieee80211_parse_ecsa(struct ieee80211_node *, const uint8_t *);
void ieee80211_parse_opmode_notif(struct ieee80211_node *, const uint8_t *);
void _ieee80211_init_supherates(struct ieee80211com *ic);
extern void (*ieee80211_init_supherates)(struct ieee80211com * ic);

uint8_t *ieee80211_add_ecsa(uint8_t * frm, struct ieee80211_node *ni);

#ifdef CONFIG_SUPPORT_MU_EDCA
void _ieee80211_he_update_mu_edca(struct ieee80211_node *, uint8_t *);
extern void (*ieee80211_he_update_mu_edca)(struct ieee80211_node * ni, uint8_t * ie);
#endif
#ifdef CONFIG_SUPPORT_UORA
void _ieee80211_he_update_uora(struct ieee80211_node *, uint8_t *, uint8_t);
extern void (*ieee80211_he_update_uora)(struct ieee80211_node * ni, uint8_t * ie, uint8_t csa);

void _ieee80211_he_init_uora(struct ieee80211_node *);
extern void (*ieee80211_he_init_uora)(struct ieee80211_node * ni);
#endif
#ifdef CONFIG_SUPPORT_SR
void _ieee80211_he_update_sr(struct ieee80211_node *, uint8_t *);
extern void (*ieee80211_he_update_sr)(struct ieee80211_node * ni, uint8_t * ie);
#endif

#ifdef CONFIG_SUPPORT_BSS_COLOR_CA
void ieee80211_he_bss_color_ca(struct ieee80211_node *, uint8_t *);
#endif

void _ieee80211_he_update_bss_color(struct ieee80211_node *, bool reinit);
extern void (*ieee80211_he_update_bss_color)(struct ieee80211_node * ni, bool reinit);

uint8_t _ieee80211_update_buffer_status(struct ieee80211vap *vap, struct ieee80211_frame *wh, int diff);
extern uint8_t (*ieee80211_update_buffer_status)(struct ieee80211vap *vap, struct ieee80211_frame *wh, int diff);

uint32_t _ieee80211_build_htc_bsr(struct ieee80211vap *vap);
extern uint32_t (*ieee80211_build_htc_bsr) (struct ieee80211vap * vap);

uint16_t _ieee80211_build_qc_with_queue_size(uint8_t tid, uint32_t queue_size);
extern uint16_t (*ieee80211_build_qc_with_queue_size) (uint8_t tid, uint32_t queue_size);

uint32_t _ieee80211_get_plushtc(const struct ieee80211_frame *wh);
extern uint32_t (*ieee80211_get_plushtc)(const struct ieee80211_frame *wh);

void _ieee80211_process_plushtc(struct ieee80211_node *ni, struct ieee80211_frame *wh);
extern void (*ieee80211_process_plushtc)(struct ieee80211_node * ni, struct ieee80211_frame * wh);

void _ieee80211_put_plushtc(const struct ieee80211_frame *wh, uint32_t plushtc);
extern void (*ieee80211_put_plushtc)(const struct ieee80211_frame * wh, uint32_t plushtc);

void ieee80211_he_send_plushtc_om(struct ieee80211_node *ni);

#endif	/* _NET80211_IEEE80211_HE_H_ */

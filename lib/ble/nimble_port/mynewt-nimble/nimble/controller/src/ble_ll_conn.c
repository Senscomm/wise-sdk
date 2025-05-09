#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "syscfg/syscfg.h"
#include "os/os.h"

#include "controller/ble_ll.h"
#include "nimble/ble.h"
#include "controller/ble_ll_conn.h"
#include "controller/ble_hw.h"

#include "hal/compiler.h"
#include "hal/rom.h"

extern struct ble_ll_conn_sm g_ble_ll_conn_sm[MYNEWT_VAL(BLE_MAX_CONNECTIONS)];

void
_ble_ll_conn_module_deinit(void)
{
	struct ble_ll_conn_sm *connsm;
	uint16_t i;

	connsm = &g_ble_ll_conn_sm[0];
	for (i = 0; i < MYNEWT_VAL(BLE_MAX_CONNECTIONS); ++i) {
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_PING)
		ble_npl_callout_deinit(&connsm->auth_pyld_timer);
#endif
		ble_npl_callout_deinit(&connsm->ctrl_proc_rsp_timer);

		++connsm;
	}
}
#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_ll_conn_module_deinit, &ble_ll_conn_module_deinit, &_ble_ll_conn_module_deinit);
#else
__func_tab__ void (*ble_ll_conn_module_deinit)(void) = _ble_ll_conn_module_deinit;
#endif

#ifdef CONFIG_LINK_TO_ROM
#include <hal/console.h>

extern uint16_t (*ble_ll_conn_adjust_pyld_len)(struct ble_ll_conn_sm *connsm, uint16_t pyld_len);

static uint16_t
patch_ble_ll_conn_adjust_pyld_len(struct ble_ll_conn_sm *connsm, uint16_t pyld_len)
{
	uint16_t max_pyld_len;
	uint16_t ret;
	uint8_t phy_mode;

	if (connsm->phy_tx_transition) {
		phy_mode = ble_ll_phy_to_phy_mode(connsm->phy_tx_transition,
				connsm->phy_data.pref_opts);
	} else {
		phy_mode = connsm->phy_data.tx_phy_mode;
	}

	max_pyld_len = ble_ll_pdu_max_tx_octets_get(connsm->eff_max_tx_time,
			phy_mode);


	ret = pyld_len;

	if (CONN_F_ENCRYPTED(connsm)) {
		max_pyld_len += BLE_LL_DATA_MIC_LEN;
	}

	if (CONN_F_ENCRYPTED(connsm)) {
		if (ret > connsm->eff_max_tx_octets + BLE_LL_DATA_MIC_LEN) {
			ret = connsm->eff_max_tx_octets + BLE_LL_DATA_MIC_LEN;
		}
	} else {
		if (ret > connsm->eff_max_tx_octets) {
			ret = connsm->eff_max_tx_octets;
		}
	}


	if (ret > max_pyld_len) {
		ret = max_pyld_len;
	}

	return ret;
}
PATCH(ble_ll_conn_adjust_pyld_len, &ble_ll_conn_adjust_pyld_len, &patch_ble_ll_conn_adjust_pyld_len);

extern uint8_t raw_data[256];
extern uint16_t (*ble_ll_con_pkt_enc)(struct ble_ll_conn_sm *connsm, struct os_mbuf *om);
uint8_t g_enc_buf[256];

static void
patch_ble_ll_con_pkt_enc(struct ble_ll_conn_sm *connsm, struct os_mbuf *om)
{
	struct ble_mbuf_hdr *ble_hdr;
	struct ble_encryption_block *ecb;
	uint16_t extend_len;
	uint16_t adjust_len;
	uint8_t llid;
	int length;
	int ret;

	ecb = &connsm->enc_data.enc_block;

	memcpy(ecb->nonce, (const uint8_t *)&connsm->enc_data.tx_pkt_cntr, 4);

	if (CONN_IS_CENTRAL(connsm)) {
		ecb->nonce[4] = 0x80;
	} else {
		ecb->nonce[4] = 0x00;
	}

	length = OS_MBUF_PKTHDR(om)->omp_len;

	ble_hdr = BLE_MBUF_HDR_PTR(om);
	llid = ble_hdr->txinfo.hdr_byte & BLE_LL_DATA_HDR_LLID_MASK;

	extend_len = length + BLE_LL_DATA_MIC_LEN;
	adjust_len = ble_ll_conn_adjust_pyld_len(connsm, extend_len);

	if (adjust_len == extend_len) {
		os_mbuf_copydata(om, 0, length, raw_data);

		ble_hw_ccm_encryption(ecb, g_enc_buf, adjust_len, raw_data, length, llid);

		os_mbuf_extend(om, BLE_LL_DATA_MIC_LEN);
		ret = os_mbuf_copyinto(om, 0, g_enc_buf, adjust_len);
		assert(!ret);

		connsm->enc_data.tx_pkt_cntr++;
	} else {
		uint16_t src_oft = 0;
		uint16_t dst_oft = 0;

		os_mbuf_copydata(om, 0, length, raw_data);

		while (length > 0) {
			ble_hw_ccm_encryption(ecb, g_enc_buf, adjust_len, &raw_data[src_oft],
					adjust_len - BLE_LL_DATA_MIC_LEN, llid);

			os_mbuf_extend(om, BLE_LL_DATA_MIC_LEN);
			ret = os_mbuf_copyinto(om, dst_oft, g_enc_buf, adjust_len);
			assert(!ret);

			dst_oft += adjust_len;
			src_oft += (adjust_len - BLE_LL_DATA_MIC_LEN);
			length -= (adjust_len - BLE_LL_DATA_MIC_LEN);

			connsm->enc_data.tx_pkt_cntr++;

			if (length > 0) {
				llid = BLE_LL_LLID_DATA_FRAG;
				extend_len = length + BLE_LL_DATA_MIC_LEN;
				adjust_len = ble_ll_conn_adjust_pyld_len(connsm, extend_len);
				memcpy(ecb->nonce, (const uint8_t *)&connsm->enc_data.tx_pkt_cntr, 4);
			}
		}
	}

	length = OS_MBUF_PKTHDR(om)->omp_len;
}
PATCH(ble_ll_con_pkt_enc, &ble_ll_con_pkt_enc, &patch_ble_ll_con_pkt_enc);

extern void (*ble_ll_conn_rx_data_pdu)(struct os_mbuf *rxpdu, struct ble_mbuf_hdr *hdr);
extern struct ble_ll_conn_sm * ble_ll_conn_find_by_handle(uint16_t handle);
extern void ble_ll_conn_chk_csm_flags(struct ble_ll_conn_sm *connsm);
extern void ble_ll_conn_timeout(struct ble_ll_conn_sm *connsm, uint8_t ble_err);
extern uint8_t ble_ll_ctrl_phy_tx_transition_get(uint8_t phy_mask);
extern void ble_ll_conn_auth_pyld_timer_start(struct ble_ll_conn_sm *connsm);

static int
ble_ll_conn_is_empty_pdu(uint8_t *rxbuf)
{
	int rc;
	uint8_t llid;

	llid = rxbuf[0] & BLE_LL_DATA_HDR_LLID_MASK;
	if ((llid == BLE_LL_LLID_DATA_FRAG) && (rxbuf[1] == 0)) {
		rc = 1;
	} else {
		rc = 0;
	}
	return rc;
}

void
patch_ble_ll_conn_rx_data_pdu(struct os_mbuf *rxpdu, struct ble_mbuf_hdr *hdr)
{
    uint8_t hdr_byte;
    uint8_t rxd_sn;
    uint8_t *rxbuf;
    uint8_t llid;
    uint16_t acl_len;
    uint16_t acl_hdr;
    struct ble_ll_conn_sm *connsm;

    /* Packets with invalid CRC are not sent to LL */
    BLE_LL_ASSERT(BLE_MBUF_HDR_CRC_OK(hdr));

    /* XXX: there is a chance that the connection was thrown away and
       re-used before processing packets here. Fix this. */
    /* We better have a connection state machine */
    connsm = ble_ll_conn_find_by_handle(hdr->rxinfo.handle);
    if (!connsm) {
       STATS_INC(ble_ll_conn_stats, no_conn_sm);
       goto conn_rx_data_pdu_end;
    }

    /* Check state machine */
    ble_ll_conn_chk_csm_flags(connsm);

    /* Validate rx data pdu */
    rxbuf = rxpdu->om_data;
    hdr_byte = rxbuf[0];
    acl_len = rxbuf[1];
    llid = hdr_byte & BLE_LL_DATA_HDR_LLID_MASK;

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_ENHANCED_CONN_UPDATE)
    connsm->last_pdu_event = connsm->event_cntr;
#endif

    /*
     * Discard the received PDU if the sequence number is the same
     * as the last received sequence number
     */
    rxd_sn = hdr_byte & BLE_LL_DATA_HDR_SN_MASK;
    if (rxd_sn == connsm->last_rxd_sn) {
       STATS_INC(ble_ll_conn_stats, data_pdu_rx_dup);
       goto conn_rx_data_pdu_end;
    }

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_ENCRYPTION)
#if (SCM2010)
    if (hdr->rxinfo.need_dec && !ble_ll_conn_is_empty_pdu(rxbuf)) {
        struct ble_encryption_block *ecb;
        uint32_t start;
        int ret;

        ecb = &connsm->enc_data.enc_block;

        memcpy(ecb->nonce, (const uint8_t *)&hdr->rxinfo.rx_pkt_cntr, 4);

        if (CONN_IS_CENTRAL(connsm)) {
            ecb->nonce[4] = 0x00;
        } else {
            ecb->nonce[4] = 0x80;
        }

#if (USE_HW_CRYPTO == 1)
        ret = ble_hw_ccm_decryption(ecb, &rxbuf[2], 256, &rxbuf[2], acl_len, llid);
#else
        ret = ble_hw_ccm_decryption(ecb, ccm_out, 256, &rxbuf[2], acl_len, llid);
#endif

        acl_len -= 4;

        if (ret == 0) {
            hdr->rxinfo.flags |= BLE_MBUF_HDR_F_MIC_FAILURE;
        } else {
#if (USE_HW_CRYPTO == 0)
            memcpy(&rxbuf[2], ccm_out, acl_len);
#endif
        }

        rxbuf[1] = acl_len;
        rxpdu->om_len -= 4;
        OS_MBUF_PKTHDR(rxpdu)->omp_len -= 4;
    }
#endif

    if (BLE_MBUF_HDR_MIC_FAILURE(hdr)) {
        STATS_INC(ble_ll_conn_stats, mic_failures);
        ble_ll_conn_timeout(connsm, BLE_ERR_CONN_TERM_MIC);
        goto conn_rx_data_pdu_end;
    }
#endif

#if (SCM2010)
#if (BLE_LL_BT5_PHY_SUPPORTED == 1)
    if (BLE_LL_LLID_IS_CTRL(hdr_byte) &&
            (connsm->conn_role == BLE_LL_CONN_ROLE_PERIPHERAL) &&
            (rxbuf[2] == BLE_LL_CTRL_PHY_UPDATE_IND)) {
            connsm->phy_tx_transition =
            ble_ll_ctrl_phy_tx_transition_get(rxbuf[3]);
    }
#endif

    if (BLE_LL_LLID_IS_CTRL(hdr_byte) &&
            (rxbuf[2] == BLE_LL_CTRL_TERMINATE_IND) &&
            (acl_len == (1 + BLE_LL_CTRL_TERMINATE_IND_LEN))) {
        connsm->csmflags.cfbit.terminate_ind_rxd = 1;
		if (!CONN_IS_CENTRAL(connsm)) {
			connsm->csmflags.cfbit.terminate_ind_rxd_acked = 1;
		}
        connsm->rxd_disconnect_reason = rxbuf[3];
    }
#endif

    /*
     * Check that the LLID and payload length are reasonable.
     * Empty payload is only allowed for LLID == 01b.
     *  */
    if ((llid == 0) || ((acl_len == 0) && (llid != BLE_LL_LLID_DATA_FRAG))) {
        STATS_INC(ble_ll_conn_stats, rx_bad_llid);
        goto conn_rx_data_pdu_end;
    }

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_ENCRYPTION)
    /* Check if PDU is allowed when encryption is started. If not,
     * terminate connection.
     *
     * Reference: Core 5.0, Vol 6, Part B, 5.1.3.1
     */
    if ((connsm->enc_data.enc_state > CONN_ENC_S_PAUSE_ENC_RSP_WAIT &&
         CONN_IS_CENTRAL(connsm)) ||
        (connsm->enc_data.enc_state >= CONN_ENC_S_ENC_RSP_TO_BE_SENT &&
         CONN_IS_PERIPHERAL(connsm))) {
        if (!ble_ll_ctrl_enc_allowed_pdu_rx(rxpdu)) {
            ble_ll_conn_timeout(connsm, BLE_ERR_CONN_TERM_MIC);
            goto conn_rx_data_pdu_end;
        }
    }
#endif

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_PING)
    /*
     * Reset authenticated payload timeout if valid MIC. NOTE: we dont
     * check the MIC failure bit as that would have terminated the
     * connection
     */
    if ((connsm->enc_data.enc_state == CONN_ENC_S_ENCRYPTED) &&
        CONN_F_LE_PING_SUPP(connsm) && (acl_len != 0)) {
        ble_ll_conn_auth_pyld_timer_start(connsm);
    }
#endif

    /* Update RSSI */
    connsm->conn_rssi = hdr->rxinfo.rssi;

    /*
     * If we are a peripheral, we can only start to use peripheral latency
     * once we have received a NESN of 1 from the central
     */
#if MYNEWT_VAL(BLE_LL_ROLE_PERIPHERAL)
    if (connsm->conn_role == BLE_LL_CONN_ROLE_PERIPHERAL) {
        if (hdr_byte & BLE_LL_DATA_HDR_NESN_MASK) {
            connsm->csmflags.cfbit.allow_periph_latency = 1;
        }
    }
#endif

    /* Update last rxd sn */
    connsm->last_rxd_sn = rxd_sn;

    /* No need to do anything if empty pdu */
    if ((llid == BLE_LL_LLID_DATA_FRAG) && (acl_len == 0)) {
        goto conn_rx_data_pdu_end;
    }

    if (llid == BLE_LL_LLID_CTRL) {
        /* Process control frame */
        STATS_INC(ble_ll_conn_stats, rx_ctrl_pdus);
        if (ble_ll_ctrl_rx_pdu(connsm, rxpdu)) {
            STATS_INC(ble_ll_conn_stats, rx_malformed_ctrl_pdus);
        }
    } else {
        /* Count # of received l2cap frames and byes */
        STATS_INC(ble_ll_conn_stats, rx_l2cap_pdus);
        STATS_INCN(ble_ll_conn_stats, rx_l2cap_bytes, acl_len);

        /* NOTE: there should be at least two bytes available */
        BLE_LL_ASSERT(OS_MBUF_LEADINGSPACE(rxpdu) >= 2);
        os_mbuf_prepend(rxpdu, 2);
        rxbuf = rxpdu->om_data;

        acl_hdr = (llid << 12) | connsm->conn_handle;
        put_le16(rxbuf, acl_hdr);
        put_le16(rxbuf + 2, acl_len);
        ble_transport_to_hs_acl(rxpdu);
    }

    /* NOTE: we dont free the mbuf since we handed it off! */
    return;

    /* Free buffer */
conn_rx_data_pdu_end:
#if MYNEWT_VAL(BLE_TRANSPORT_INT_FLOW_CTL)
    if (hdr->rxinfo.flags & BLE_MBUF_HDR_F_CONN_CREDIT_INT) {
        ble_transport_int_flow_ctl_put();
    }
#endif

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_CTRL_TO_HOST_FLOW_CONTROL)
    /* Need to give credit back if we allocated one for this PDU */
    if (hdr->rxinfo.flags & BLE_MBUF_HDR_F_CONN_CREDIT) {
        ble_ll_conn_cth_flow_free_credit(connsm, 1);
    }
#endif

    os_mbuf_free_chain(rxpdu);
}
PATCH(ble_ll_conn_rx_data_pdu, &ble_ll_conn_rx_data_pdu, &patch_ble_ll_conn_rx_data_pdu);
#endif

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
#ifndef __SCM2020_VAR_H__
#define __SCM2020_VAR_H__

#include <hal/types.h>
#include <hal/timer.h>
#include <hal/rf.h>
#include <hal/pm.h>

#include <net80211/ieee80211_var.h>

#define MAC_IMEM_OFFSET		(0x00000000)
#define MAC_DMEM_OFFSET		(0x00004000)
#define MAC_SHM_OFFSET		(0x00004760)
#define MAC_FIFO_OFFSET		(0x00006000)
#define MAC_REG_OFFSET		(0x00008000)
#define MAC_SYS_OFFSET      (0x00009000)
#define MAC_TIM_OFFSET		(0x00009100)
#define MAC_DMA_OFFSET		(0x00009200)
#define MAC_TXE_OFFSET		(0x00009300)
#define MAC_RXE_OFFSET		(0x00009400)

#define DMEM_CTS_RESP_CNT   (0x00000290)
#define DMEM_RTS_NAVB_CNT   (0x00000294) /* RXE_MPDU_INFO.rts == 0, i.e., NAV Busy */
#define DMEM_RTS_NCTS_CNT   (0x00000298) /* RXE_CTRL_INFO.need_cts == 0 */

#define RXE_BD_SHM_ADDR				0x10
#define RXE_BA_FIFO_ADDR			0x14
#define RXE_VEC_FIFO_ADDR			0x18
#define RXE_MPDU_FIFO_CFG			0x30
#define RXE_MPDU_FIFO_ADDR			0x34

/* NB: for concurrent mode, SC_NR_TXQ should be 10 */
#ifdef CONFIG_SUPPORT_DUAL_VIF
#define SC_NR_TXQ 	10
#else
#define SC_NR_TXQ 	5
#endif

#define TXQ_PER_VAP 	5
#define LOG2(x)	({ int __n, __k = 0; for (__n = 1; __n < x; __n = __n * 2) __k++; __k;})
/* LOG2 with floor */
#define LOG2_FL(x)	({ int __n, __k = 0; for (__n = 1; __n <= x; __n = __n * 2) __k++; __k - 1;})

#ifdef CONFIG_SUPPORT_MU_EDCA
#ifdef CONFIG_SUPPORT_SOFTAP_MU_EDCA
#define SC_MU_EDCA_NR_TXQ SC_NR_TXQ
#else
#define SC_MU_EDCA_NR_TXQ TXQ_PER_VAP
#endif
#endif

/**
 * Privacy
 */

#define SC_CIPHER_WEP40		0
#define SC_CIPHER_WEP104	1
#define SC_CIPHER_TKIP		2
#define SC_CIPHER_CCMP		4
#define SC_CIPHER_CCMP256	5
#define SC_CIPHER_GCMP		6
#define SC_CIPHER_GCMP256	7
#define SC_CIPHER_NONE		8
#define SC_CIPHER_UNKNOWN 	9

#define scm2020_cipher_is_valid(cipher)	\
	((cipher) >= SC_CIPHER_WEP40 	\
	 && (cipher) < SC_CIPHER_NONE)

#define SC_KEY_CMD_ADD		1
#define SC_KEY_CMD_DEL_BY_TAG		2
#define SC_KEY_CMD_DELALL	3
#define SC_KEY_CMD_READ_BY_INDEX		4
#define SC_KEY_CMD_DEL_BY_INDEX		5
#define SC_KEY_CMD_DONE		0

#define ifq_len(ifq)		    ({ int __len = _IF_QLEN(ifq); __len; })
#define ifq_full(ifq)		    ({ bool __full = _IF_QFULL(ifq); __full; })
#define ifq_empty(ifq)		    ({ bool __empty = IFQ_IS_EMPTY(ifq); __empty; })
#define ifq_lock(ifq)           do {IF_LOCK(ifq);} while (0);
#define ifq_unlock(ifq)         do {IF_UNLOCK(ifq);} while (0);
#define ifq_dequeue(ifq)	    ({ struct mbuf *__m; IFQ_DEQUEUE(ifq, __m); __m; })
#define ifq_dequeue_nolock(ifq)	({ struct mbuf *__m; IFQ_DEQUEUE_NOLOCK(ifq, __m); __m; })
#define ifq_peek(ifq)		    ({ struct mbuf *__m; IFQ_POLL_NOLOCK(ifq, __m); __m; })
#define ifq_enqueue(ifq, m)	    ({ int __ret; IFQ_ENQUEUE(ifq, m, __ret); __ret; })
#define ifq_enqueue_nolock(ifq, m)	    ({ int __ret; IFQ_ENQUEUE_NOLOCK(ifq, m, __ret); __ret; })
#define ifq_foreach_mbuf(m, ifq) \
	for ((m) = ifq_peek((ifq)); (m) != NULL; (m) = (m)->m_nextpkt)

#define ifq_suspend(txq)    ((txq)->txq_status & SCM2020_TXQ_SUSPEND)


/* MPDU table entry */
struct mentry {
	/* MT0 */
	u32 pbd_l;	/* pointer to buffer descriptor (lower 32 bits) */
	/* MT1 */
	u32 pbd_h;	/* pointer to buffer descriptor (higher 32 bits) */

	/* MT2 */
	bf( 0, 11, aid);
	bf(12, 15, tid);
	bf(16, 27, sn);
	bf(28, 28, noack);
	bf(29, 29, eof);
	bf(30, 30, sent);
	bf(31, 31, ack);

	/* MT3 */
	bf( 0, 13, len);
	bf(14, 14, more);
	bf(15, 15, noenc);
	bf(16, 20, num);
	bf(21, 22, prot);
	bf(23, 23, ts_new);
	bf(24, 30, ts_off);
	bf(31, 31, htc);

	/* MT4 */
	bf( 0, 15, prot_dur);
	bf(16, 25, spacing);
	bf(26, 31, retry);

	/* MT5 */
	bf(0, 3, to_retry);
	bf(4, 31, reserve);

	/* MT6 */
	u32 sw0; /* wise: m */
	/* MT7 */
	u32 sw1;
};

/* Buffer descriptor */
struct bdesc {
	u32 pdata_l;	/* pointer to data buffer (lower 32 bits) */
	u32 pdata_h;	/* pointer to data buffer (higher 32 bits) */
	u32 len;
};

struct sc_vap;

#define NENTRY	(32) /* max. # of MPDUs in MPDU table */
#define NBUFFER	(32) /* max. # of buffers per MPDU */

/**
 * struct sc_tx_queue_params - transmit queue configuration
 *
 * @aifsn: arbitration interframe space [0..255]
 * @cw_min: minimum contention window [a value of the form
 *	2^n-1 in the range 1..32767]
 * @cw_max: maximum contention window [like @cw_min]
 */
struct sc_tx_queue_params {
	u8 aifsn;
	u16 cw_min;
	u16 cw_max;
};

#define SCM2020_TXQ_DISABLE 	0x0
#define SCM2020_TXQ_ENABLE		BIT(0)
#define SCM2020_TXQ_SUSPEND		BIT(1)
#define SCM2020_TXQ_DROP		BIT(2)

struct sc_tx_queue_ext {
	struct mbuf *txq_m;
};

struct sc_tx_queue {
	int hwqueue;

	struct ieee80211vap *vap;
	struct ieee80211com *ic;
	int ac;
	u32 cw;
	u32 txop_limit;
	u32 txop_start; /* tsf at backoff done */
	u16  txq_status;
	struct sc_tx_queue_params edca;

	struct ifqueue ifq;	      /* input from net80211 */
	struct ifqueue queue;	  /* enqueued */
	struct ifqueue sched;	  /* scheduled and not yet done */
	struct ifqueue retry;	  /* to be retransmitted */

	struct task txstart_task; /* tx task */

	volatile struct txdesc *desc;
	struct mentry *mtable;

#ifdef CONFIG_MAC_TX_TIMEOUT
	osTimerId_t txq_timer;
#endif

#ifdef CONFIG_SUPPORT_MU_EDCA
#define SC_FLG_CALLOUT_RUNNING		0x01
#define SC_FLG_MU_EDCA_CONFIGURED	0x02
	u32	mu_edca_status;	/* during timer running, if aifsn == 0,
		the EDCAF corresponding to that AC shall be suspended until the
		MU_EDCA_Timer[AC] reaches 0 or is reset to 0 */
	u32 mu_aifsn;
	u32 mu_cwmin;
	u32 mu_cwmax;
	u32 mu_edca_timeval;	/* unit of TU */
	osTimerId_t	txq_edca_timer;
	struct sc_tx_queue_params b_edca;
#endif
	struct sc_tx_queue_ext	*txq_ext;
	/* NOTE: don't add more element in the end please add to txq_ext*/
};

struct sc_rx_desc {
	int index;
	u8 state;
	struct mbuf *m;
	/* for macext rx callback */
	void (*func)(struct ieee80211_node*, void*, int);
	void *context;
};

struct sc_rx_ampdu_context {
	u8 tid;
};

#define VIF0	0x0
#define VIF1	0x1
#define VIF_MAX 0x2
#define VIF_ANY 0xFF

#define VIF0_EN BIT(VIF0)
#define VIF1_EN BIT(VIF1)
#define DUAL_VIF_EN (BIT(VIF0) | BIT(VIF1))

#ifdef CONFIG_SUPPORT_MCC
#define MCC_SCAN_ON_GOING BIT(2) /* BIT(2) indicate mcc scan status */
#define MCC_SET_SCAN_ONGOING(_sc) (((_sc)->sc_mcc_cfg.vif_status) |= (MCC_SCAN_ON_GOING))
#define MCC_CLR_SCAN_ONGOING(_sc) (((_sc)->sc_mcc_cfg.vif_status) &= ~(MCC_SCAN_ON_GOING))
#define MCC_IS_SCAN_ONGOING(_sc) ((((_sc)->sc_mcc_cfg.vif_status) & (MCC_SCAN_ON_GOING)) == MCC_SCAN_ON_GOING)

struct channel_status {
	int ch_owner;
	enum ieee80211_ch_req_module req_reason;
};

struct scm2020_mcc_cfg {
	osMutexId_t         mcc_queue_mutex; /* Lock for mcc_task_queue */
	struct channel_status ch_status;
	osTimerId_t	ch_acquire_timer;
	uint64_t		mcc_timeout;
	struct task 	mcc_timeout_task;
	struct list_head       mcc_task_queue;
	int 				mcc_task_num;
	uint8_t				vif_status;
};

#endif

struct sc_bcn_counter {
	u32 miss;
	u32 total;
};

struct sc_key {
	struct ieee80211_key *pkey;
	struct ieee80211_key *gkey;
};

struct sc_vap {
	struct ieee80211vap vap;
	int index;
	bool in_use;
	int gkeyix[IEEE80211_WEP_NKID];
	int (*key_alloc)(struct ieee80211vap *, struct ieee80211_key *,
			ieee80211_keyix *, ieee80211_keyix *);
	struct sc_tx_queue *txq[TXQ_PER_VAP];

	/*
	 * Warning: above elements are used in rom
	 * Please don't change sequence
	 */
	u32 supported_opmodes;

	/* Support fixed rate, bandwidth in data frames */
	bool fixed_rate;
	struct ieee80211_bpf_params fixed_bpf_params;

	/* Power management */
	u64 ntbtt;
	u16 next_beacon_sn;
	u32 beacon, tot_beacon, beacon_miss;
	u32 beacon_rssi_map; /* '1': high RSSI */
	struct sc_bcn_counter bcn_counter;
	int ps;
	u64 doze_start;
	u32 ts_sleep;
	u32 tot_doze_time;
	u64 dtim_tsf;	/* for WC sleep duration cal, save dtim rx tsf */

	int (*newstate)(struct ieee80211vap*, enum ieee80211_state, int);
	int (*input)(struct ieee80211_node *, struct mbuf *,
		     struct ieee80211_rx_stats *, int, int);
	void (*recv_mgmt)(struct ieee80211_node *, struct mbuf *,
			  int, struct ieee80211_rx_stats *,
			  int, int);
	struct sc_key sc_keys[2];
};

#define RESET_BMISS_CONF(vap) do {\
    vap->bmiss_win = 0;        \
    vap->bmiss_credit = 0;     \
    vap->bmiss_thres = 0;      \
} while (0)

#define RESET_BMISS_STATS(tvap) do {\
    tvap->beacon_miss = 0;      \
    tvap->tot_beacon = 0;       \
    tvap->beacon = 0;           \
    tvap->next_beacon_sn = 0;   \
} while (0)

#define RESET_BMISS(vap, tvap)   {RESET_BMISS_CONF(vap); RESET_BMISS_STATS(tvap);}


struct sc_ver {
	u32 macver;
	u32 macgit;
	u32 phyver;
	u32 phygit;
};

#define prodc(ver)	((ver & 0xFFFF0000) >> 16)
#define version(ver)	((ver & 0x0000FFF0) >> 4)
#define is_chip(ver)	((ver & 0x0000000F) == 0xC ? 1 : 0)
#define is_scm2020(ver) (prodc(ver) == 0xB0BA)
#define is_scm2010(ver) (prodc(ver) == 0xDCAF)

enum rate_type {
    RATE_PHY_NONHT,
    RATE_PHY_HTMF,
    RATE_PHY_VHT,
    RATE_PHY_HESU,
    RATE_TYPE_MAX,
};

enum rate_idx {
    MCS_0,
    MCS_1,
    MCS_2,
    MCS_3,
    MCS_4,
    MCS_5,
    MCS_6,
    MCS_7,
    MCS_8,
    MCS_9,
    MCS_10,
    MCS_11,
    MCS_MAX
};

typedef enum {
	MAC_DISABLE = 0,
	MAC_0_EN = 1,
	MAC_1_EN = 2,
	MAC_EN_NO_OPERATION = 3,
}scm2020_reg_mac_en;

typedef enum {
	BSS_DISABLE = 0,
	BSS_0_EN = 1,
	BSS_1_EN = 2,
	BSS_0_1_EN = 3,
	BSS_EN_NO_OPERATION = 4,
}scm2020_reg_bss_en;

/* Table 17-8 */
enum cbw {
	SC_PHY_CBW20 		= 0,
	SC_PHY_CBW40 		= 1,
	SC_PHY_CBW80 		= 2,
	SC_PHY_CBW160 		= 3,
	SC_PHY_CBW80PLUS80 	= 3,
	SC_PHY_CBW_NOT_PRESENT	= 4,
};

enum ppdu {
	SC_PHY_NONHT 		= 0,
	SC_PHY_HTMF  		= 1,
	SC_PHY_HTGF  		= 2,
	SC_PHY_VHT   		= 3,
	SC_PHY_HESU  		= 4,
	SC_PHY_HEER  		= 5,
	SC_PHY_HEMU  		= 6,
	SC_PHY_HETB  		= 7,
	SC_PHY_MAX  		= 8,
};

enum fec_coding {
	SC_PHY_BCC		= 0,
	SC_PHY_LDPC		= 1,
};

enum gi_type {
	SC_PHY_LONG_GI 		= 1,
	SC_PHY_SHORT_GI 	= 0,
	SC_PHY_GI_0u4s 		= 0, /* 0.4us (HE?)*/
	SC_PHY_GI_0u8s 		= 1, /* 0.8us (HE) */
	SC_PHY_GI_1u6s 		= 2, /* 1.6us (HE) */
	SC_PHY_GI_3u2s 		= 3, /* 3.2us (HE) */
};

enum preamble_type {
	SC_PHY_LONGPREAMBLE	= 0,
	SC_PHY_SHORTPREAMBLE	= 1,
};

enum ltf_type {
	SC_PHY_1x 		= 0,
	SC_PHY_2x 		= 1,
	SC_PHY_4x 		= 2,
};

enum spatial_reuse {
	SC_PHY_PSR_DISALLOW	= 0,
	SC_PHY_PSR_AND_NON_SRG_OBSS_PD_PROHIBITED	= 15,
};

/* Table 9-321d Constellation index */
enum mod_type {
	SC_PHY_MOD_BPSK 	= 0,
	SC_PHY_MOD_QPSK 	= 1,
	SC_PHY_MOD_16QAM	= 2,
	SC_PHY_MOD_64QAM 	= 3,
	SC_PHY_MOD_256QAM 	= 4,
	SC_PHY_MOD_1024QAM 	= 5,
	SC_PHY_MOD_RESERVED	= 6,
	SC_PHY_MOD_NONE 	= 7,
};

#define IEEE80211_HE_TXOP_DURATION_UNSPECIFIED		0x7F
#define IEEE80211_HE_TXOP_DURATION_GRANULARITY_128	0x40

#define SC_FMT_MCS(fmt, mcs) 	((fmt) << 16 | (mcs))
#define SC_PHY_FMT(x)		((x >> 16) & 0xf)
#define SC_PHY_MCS(x)		((x) & 0xf)

struct tx_vector {
	/* word 1 (  0 -  31) */
	bf( 0,  6, mcs);
	bf( 7,  7, preamble_type);
	bf( 8, 19, l_length);
	bf(20, 22, format);
	bf(23, 23, fec_coding);
	bf(24, 31, txpwr_level_index);

	/* word 2 ( 32 -  63) */
	bf( 0, 15, length);
	bf(16, 31, spatial_reuse);

	/* word 3 ( 64 -  95) */
	bf( 0, 19, apep_length);
	bf(20, 22, ch_bandwidth);
	bf(23, 23, stbc);
	bf(24, 31, ru_allocation);

	/* word 4 ( 96 - 127) */
	bf( 0,  8, he_sig_a2_reserved);
	bf( 9, 15, txop_duration);
	bf(16, 24, partial_aid);
	bf(25, 25, rate_auto_fallback); /* wise software define*/
	bf(26, 27, sw_encrypt);
	bf(28, 28, dyn_bandwidth_in_non_ht);
	bf(29, 30, ch_bandwidth_in_non_ht);
	bf(31, 31, midamble_periodicity);

	/* word 5 (128 - 159) */
	bf( 0,  7, rate_idx); /* wise software define, 8 bits should be enough */
	bf( 8, 15, rsvd2);
	bf(16, 21, group_id);
	bf(22, 23, gi_type);
	bf(24, 31, rsvd3); /* wise: current channel id */

	/* word 6 (160 - 191) */
	bf( 0,  6, scrambler_initial_value);
	bf( 7,  7, ldpc_extra_symbol);
	bf( 8, 13, bss_color);
	bf(14, 15, he_ltf_type);
	bf(16, 18, total_num_sts);
	bf(19, 21, num_he_ltf);
	bf(22, 23, nominal_packet_padding);
	bf(24, 26, default_pe_duration);
	bf(27, 29, starting_sts_num);
	bf(30, 30, smoothing);
	bf(31, 31, aggregation);

	/* word 7 (192 - 223) */
	bf( 0,  0, dcm);
	bf( 1,  1, num_sts);
	bf( 2,  2, doppler);
	bf( 3,  3, he_ltf_mode);
	bf( 4,  4, txop_ps_not_allowed);
	bf( 5,  5, trigger_method);
	bf( 6,  6, uplink_flag);
	bf( 7,  7, trigger_responding);
	bf( 8, 12, he_tb_data_symbols);
	bf(13, 14, he_tb_pre_fec_factor);
	bf(15, 15, feedback_status);
	bf(16, 16, he_tb_pe_disambiguity);
	bf(17, 31, rsvd4);
};

struct tx_info {
	bf( 0,  1, prot);
	bf( 2,  2, noack);
	bf( 3,  3, aggr);
	bf( 4,  6, retry);
	bf( 7, 14, rate);
	bf(15, 24, spacing);
	bf(25, 25, sent);
	bf(26, 26, acked);
	bf(27, 27, ok);
	bf(28, 31, fail);

	u16	prot_dur;
	u16	txtime;
	u16	aid;
	u16	mpdu_len;
	bool add_htc;
};

struct hw_tx_hdr {
	struct tx_vector 	tv;
	struct ieee80211_frame 	mh[0]; 	/* ieee80211 frame */
};

struct drv_tx_param {
	struct tx_info		ti;
	struct mbuf 		*bds;
};

#define hw(m) ((mtod((m), struct hw_tx_hdr *)))
#define ti(m) ({struct tx_info *__ti = NULL; struct drv_tx_param *__param;		\
		if ((__param = ieee80211_get_drv_params_ptr(m))) __ti = &__param->ti;	\
		__ti;})
#define tv(m) (&(hw(m)->tv))
#define hw_hdr_size(m0, m)	((m == m0) ? sizeof(struct hw_tx_hdr) : 0)
#define is_cck(tv)		(tv->format == SC_PHY_NONHT && tv->mcs < 4)
#define is_vht(tv)		(tv->format == SC_PHY_VHT	||\
				 tv->format == SC_PHY_HESU 	||\
				 tv->format == SC_PHY_HEER 	||\
				 tv->format == SC_PHY_HEMU 	||\
				 tv->format == SC_PHY_HETB)
#define is_he(tv)		(is_vht(tv) && tv->format != SC_PHY_VHT)
#define is_he_tb(tv)	(tv->format == SC_PHY_HETB)

struct rx_vector {
	/* word 1 (  0 -  31) */
	bf( 0,  6, mcs);
	bf( 7,  7, preamble_type);
	bf( 8, 19, l_length);
	bf(20, 22, format);
	bf(23, 23, fec_coding);
	bf(24, 31, rssi_legacy);

	/* word 2 ( 32 -  63) */
	bf( 0, 15, length);
	bf(16, 19, spatial_reuse);
	bf(20, 22, mcs_sig_b);
	bf(23, 23, dcm_sig_b);
	bf(24, 31, rssi);

	/* word 3 ( 64 -  95) */
	bf( 0, 19, psdu_length);
	bf(20, 22, ch_bandwidth);
	bf(23, 23, stbc);
	bf(24, 30, txop_duration);
	bf(31, 31, dcm);

	/* word 4 ( 96 - 127) */
	bf( 0,  7, ru_allocation);
	bf( 8, 15, rsvd1);
	bf(16, 23, td_best_sym_epwr_db);
	bf(24, 31, td_best_sym_dpwr_db);

	/* word 5 (128 - 159) */
	bf( 0,  7, td_worst_sym_epwr_db);
	bf( 8, 15, td_worst_sym_dpwr_db);
	bf(16, 23, avg_epwr_db);
	bf(24, 31, avg_dpwr_db);

	/* word 6 (160 - 191) */
	bf( 0, 10, sta_id_list);
	bf(11, 13, pe_duration);
	bf(14, 15, gi_type);
	bf(16, 24, partial_aid);
	bf(25, 27, num_sts);
	bf(28, 28, dyn_bandwidth_in_non_ht);
	bf(29, 30, ch_bandwidth_in_non_ht);
	bf(31, 31, doppler);

	/* word 7 (192 - 223) */
	bf( 0,  6, scrambler_initial_value);
	bf( 7,  7, uplink_flag);
	bf( 8, 13, bss_color);
	bf(14, 15, he_ltf_type);
	bf(16, 21, group_id);
	bf(22, 23, user_position);
	bf(24, 24, beam_change);
	bf(25, 25, beamformed);
	bf(26, 26, txop_ps_not_allowed);
	bf(27, 27, non_ht_modulation);
	bf(28, 28, aggregation);
	bf(29, 29, sounding);
	bf(30, 30, smoothing);
	bf(31, 31, lsigvalid);

	/* word 8 (224 - 255) */
	bf( 0,  3, spatial_reuse2);
	bf( 4,  7, spatial_reuse3);
	bf( 8, 11, spatial_reuse4);
	bf(12, 23, cfo_est);
	bf(24, 31, rcpi);
};

struct rx_info {
	/* word 1 */
	bf(0,  13, len);
	bf(14, 31, rsvd1);

	/* word 2 */
	bf( 0,  0, phy);
	bf( 1,  1, ok);
	bf( 2,  2, err_crc);
	bf( 3,  3, err_len);
	bf( 4,  4, err_key);
	bf( 5,  5, err_mic);
	bf( 6,  6, err_icv);
	bf( 7,  7, clear);
	bf( 8, 10, cipher);
	bf(11, 11, ampdu);
	bf(12, 12, eof);
	bf(13, 13, rsvd2);
	bf(14, 15, policy);
	bf(16, 19, tid);
	bf(20, 20, tail);
	bf(21, 31, rsvd3);

	/* word 3 */
	u32 tsf;
};

struct hw_rx_hdr {
	struct rx_vector rv;
	struct rx_info   ri;
	struct ieee80211_frame mh[0];
};

struct sc_softc {
	struct ifnet *ifp;	/* should be the first one */
	struct ieee80211com *ic;
	struct device *dev;
	struct device *rf;
	struct clk *mac_clk;
	struct clk *phy_tx_clk;
	struct clk *phy_rx_clk;
	struct sc_tx_queue txq[SC_NR_TXQ];
	volatile struct scm_mac_shmem_map *shmem;
	struct {
		struct sc_rx_desc *desc;
		u32 n_rx_desc;
		struct sc_rx_desc *head; /* next read pointer */
		struct task handler_task;
		struct task refill_task;

		u32 map[howmany(128, 32)]; /* bitmap of descriptors that need refilling */
		bool no_refill; /* macext tx only */
		struct mbuf *m0;

		struct sc_rx_ampdu_context ampdu_ctx[WME_NUM_TID];
		void *sc_rx_ext;
	} rx;

	struct {
		const struct reg_set *phy_init;
		u32 n_phy_init;
		const struct reg_set *phy_mon_init;
		u32 n_phy_mon_init;
		u32 tx_lut[64];
		u32 rx_lut[2][95];
		/* save power-on default values */
		u8 phy_11b_cs_rssi_thr[2];
		u8 phy_cs_xcr_minrssi[2];
		u8 phy_cs_acr_minrssi[2];
		void *sc_phy_ext;
	} phy;

	struct sc_vap *tvap;
	int n_vap;
	struct sc_vap *cvap;
	struct taskqueue *sc_tq; /* fast task queue */

#ifdef CONFIG_SCM2020_STATS
	struct {
		struct {
			u32 err_mic;
			u32 err_icv;
			u32 err_key;
			u32 err_len;
			u32 err_delim;
			u32 err_addr;
			u32 err_crc;
			u32 err_seq;
			u32 discard;
			u32 multi;
			u32 other;
			u32 deliver;
			u32 lowest;
		} rx;
		struct {
			u32 total;
			u32 qfull;  	/* discarded packets due to tx queue full */
			u32 enqueue;	/* enqueued packets in tx queue */
			u32 aggr;
			u32 ampdu;
			u32 ack;	/* ack received */
			u32 sent;	/* sent out to PHY */
			u32 missing; 	/* not sent for some reason */
			/* fail reason */
			u32 ack_tmo;	/* ack/ba Timeout */
			u32 cts_tmo;	/* cts Timeout */
			u32 ack_err;	/* ack/ba crc fail, address match fail */
			u32 cts_err;	/* cts crc fail, address match fail */
			u32 cca_err;	/* CCA detected during PIFS (40/80) */
			u32 int_col;	/* internal collision */
			u32 sifs_to;	/* SIFS timeout */
			u32 win_los;	/* Win lost */
			u32 tx_to;	/* TX timeout occurred specified by REG_TX_TO_CFG */
			u32 phy_err; 	/* TX error indication from PHY to MAC */
			u32 phy_abt; 	/* TX error due to PHY abort signal (PHY SW reset) */
			u32 cox_abt;	/* TX abort due to coexistence */
			u32 amsdu;
			u32 retry;
		} tx[SC_NR_TXQ];
		u32 rate[RATE_TYPE_MAX][MCS_MAX];
	} stats;

#define tx_rate_stat(type, idx) 	(sc->stats.rate[type][idx])
#define tx_rate_inc(type, idx) \
	do { txq2sc(txq)->stats.rate[type][idx]++; } while(0)

#define tx_stat(i, what) 	(sc->stats.tx[i].what)
#define tx_inc(what) \
	do { txq2sc(txq)->stats.tx[txq->hwqueue].what++; } while(0)
#define tx_add(what, val) \
	do { txq2sc(txq)->stats.tx[txq->hwqueue].what += val; } while(0)
#define rx_stat(what)  		(sc->stats.rx.what)
#define rx_stat_set(what, v)	sc->stats.rx.what = v
#define rx_inc(what) \
	do { sc->stats.rx.what++; } while(0)
#else
#define tx_rate_stat(type, idx)	(0)
#define tx_rate_inc(type, idx)
#define tx_stat(i, what)	(0)
#define tx_inc(what)
#define rx_stat(what) 		(0)
#define rx_stat_set(what, v)
#define rx_inc(what)

#endif /* CONFIG_SCM2020_STATS */

	int (*ampdu_rx_start)(struct ieee80211_node *,
			struct ieee80211_rx_ampdu *,
			int baparamset, int batimeout, int baseqctl);
	void (*ampdu_rx_stop)(struct ieee80211_node *, struct ieee80211_rx_ampdu *);
	bool debug_mode;
	struct sc_ver ver;
	/*
	 *  Above elements are used in rom
	*/

	int cbw;
	struct {
		struct ifqueue done; /* completed and to be reclaimed */
		struct task reclaim_task;
	} tx;
	/* Power management */
	int doze;
	struct callout doze_timer;

	bool dump_tx;
	bool dump_rx;
	bool dump_ba;
	bool enable_dm;
#ifdef CONFIG_SUPPORT_TWT
	struct task twt_timeout_task;
#endif
#ifdef CONFIG_SUPPORT_MCC
	struct scm2020_mcc_cfg sc_mcc_cfg;
#endif
	/*
	 * sc_mcc_cfg is used in rom
	 */

	/* Backup for monitor mode */
	struct {
		struct hw_rx_hdr rxh;
	} mon;

#ifdef CONFIG_SCM2020_STATS_EXT

#define stsext_name(a) #a
#define stsext(a) (sc->stats_ext.a)

    /* More statistics */
    struct {
#ifdef CONFIG_DEBUG_RTS_CTS_SEQ_RX
        u32 rts_rcvd;
        u32 rts_navb;
        u32 rts_ncts;
        u32 cts_sent;
#endif
    } stats_ext;

#endif

	int cs_thr;
	struct task txq_drop_task;
	struct task ps_chk_bcn_miss_task;
	void (*tv_set_txpwr_index)(struct tx_vector *tv, struct ieee80211_node *ni, u32 hwrate);

	struct {
		struct {
			int fcal_tx_offset;
			int fcal_rssi_offset;
		} phy;
		struct {
			int cfo_ofs;
		} rf;
	} cal;

	void (*load_cal)(struct sc_softc *sc);
};

struct tx_pwr_table_entry
{
	int rate;
	u8 tx_pwr_index; /* dBm */
	u8 tx_pwr_index_max; /* dBm */
};

struct scm2020_pwr_table {
	struct tx_pwr_table_entry *table;
	int size;
};

#ifdef CONFIG_SCM2010_TX_TIMEOUT_DEBUG
#include "scm2020_shmem.h"

#define TXTO_DBG_MAX 8
#define TXTO_DBG_AMPDU_MAX 16
struct scm2020_mpdu_tx_dbg_hdr {
	volatile struct mentry me;
	uint8_t bd_num;
	struct bdesc bd[4];
	struct tx_vector	tv;
};

struct scm2020_ampdu_tx_dbg_hdr {
	struct scm2020_mpdu_tx_dbg_hdr ampdu_hdr[TXTO_DBG_AMPDU_MAX];
	int ampdu_num;
	volatile struct txdesc desc;
	u32 txq_cmd;
	u32 txq_txop;
	u32 txq_state;
	u32 txq_timer;
};

struct scm2020_tx_dbg_info {
	u8 hwqueue;
	u32 kick_time;
	u32 timeout_time;
	bool updating;
	struct scm2020_ampdu_tx_dbg_hdr kick_hdr;
	struct scm2020_ampdu_tx_dbg_hdr timeout_hdr;
};

struct	tx_timeout_dbg {
	struct	scm2020_tx_dbg_info tx_dbg_info[TXTO_DBG_MAX];
	int q_idx;
	struct	mtx ifq_mtx;
};

extern struct tx_timeout_dbg g_tx_timeout_dbg;

/* consider memory we only debug for sta or sap */
#define SCM2010_TX_TIMEOUT_DEBUG_TXQ_NUM 10
extern struct scm2020_tx_dbg_info g_tx_timeout_pkts[SCM2010_TX_TIMEOUT_DEBUG_TXQ_NUM];
#endif /* CONFIG_SCM2010_TX_TIMEOUT_DEBUG */

#define PRIMARY_WLANID		0
#define wlan_inst(sc)		dev_id(sc->dev)
#define wlan_other_inst(sc)	(wlan_inst(sc) ? 0 : 1)
#define wlan_is_primary(sc)	(wlan_inst(sc) == PRIMARY_WLANID)

#define RX_DESC_MACEX_RX	BIT(3)	/* used for macext rx stats */
#define RX_DESC_HW_READY	BIT(2)  /* already emptied by SW */
#define RX_DESC_SW_READY 	BIT(1)  /* ready to be emptied by SW */
#define RX_DESC_HW_OWNED 	BIT(0) 	/* ready to be written by HW */

#define rx_desc_change_state(sc, desc, newstate)	do {				\
	desc->state &= ~(RX_DESC_HW_OWNED | RX_DESC_SW_READY | RX_DESC_HW_READY); 	\
	desc->state |= newstate;							\
	sc->shmem->rx[desc->index].pbuffer_h = desc->state;				\
} while (0);

static inline struct sc_rx_desc *rx_desc_next(struct sc_softc *sc,
		struct sc_rx_desc *desc)
{
	struct sc_rx_desc *next = desc + 1;
	struct sc_rx_desc *first = &sc->rx.desc[0];

	if (next - first == sc->rx.n_rx_desc)
		return first;

	return next;
}

/**
 * PHY
 */
#define PHY_HE_DIR_DL		0x1
#define PHY_HE_DIR_UL		0x2
#define PHY_HE_DIR_DL_UL 	(PHY_HE_DIR_DL | PHY_HE_DIR_UL)

/**
 * Primitives
 */

#define vap2tvap(vap) 		(container_of(vap, struct sc_vap, vap))

/* Register access macros */

#define M_OFT_N(x, n) \
	(REG_##x + ((REG_##x##_SPACING) * (n)))

#define REG_VIF_MAC0_L_SPACING 8
#define REG_VIF_MAC0_H_SPACING 8
#define REG_VIF_BSS0_L_SPACING 8
#define REG_VIF_BSS0_H_SPACING 8
#define REG_SEC_TEMP_KEY0_SPACING 4
#define REG_SEC_TEMP_KEY4_SPACING 4
#define REG_SEC_TEMP_KEY6_SPACING 4

/* register bit field */
#define RBFM(r, f) 		(r##_##f##_MASK) 	/* mask */
#define RBFS(r, f) 		(r##_##f##_SHIFT) 	/* shift */
#define RBFSET(v, r, f, x) 	((v) |= ((x) << RBFS(r, f)) & RBFM(r, f))
#define RBFCLR(v, r, f) 	((v) &= ~RBFM(r, f))
#define RBFMOD(v, r, f, x) 	{RBFCLR(v, r, f); RBFSET(v, r, f, x);}
#define RBFGET(v, r, f) 	(((v) & RBFM(r, f)) >> RBFS(r, f))
#define RBFZERO(v) 		((v) = 0)

#define RBFM_P(f)		(f##_MASK) 	/* mask */
#define RBFS_P(f) 		(f##_SHIFT) 	/* shift */
#define RBFSET_P(v, f, x) 	((v) |= ((x) << RBFS_P(f)) & RBFM_P(f))
#define RBFCLR_P(v, f) 		((v) &= ~RBFM_P(f))
#define RBFMOD_P(v, f, x) 	{RBFCLR_P(v, f); RBFSET_P(v, f, x);}
#define RBFGET_P(v, f) 		(((v) & RBFM_P(f)) >> RBFS_P(f))

#define bfzero 			RBFZERO

#define bfor_m  		RBFSET
#define bfclr_m  		RBFCLR
#define bfmod_m  		RBFMOD
#define bfget_m  		RBFGET

#define bfset_p  		RBFSET_P
#define bfclr_p  		RBFCLR_P
#define bfmod_p  		RBFMOD_P
#define bfget_p  		RBFGET_P

#define RX_PTR(sc, p)		((p) & ((sc->rx.n_rx_desc << 1) - 1))
#define RX_PTR2IDX(sc, p)	((p) & (sc->rx.n_rx_desc - 1))

#define rx_ptr_READY(sc)	({u32 __v; u16 __ptr;				\
			__v = mac_readl(sc->dev,  REG_RX_BUF_PTR0);		\
	       		__ptr = RX_PTR(sc, bfget_m(__v, RX_BUF_PTR0, READY_PTR)); __ptr;})
#define rx_ptr_READ(sc)		({u32 __v; u16 __ptr;				\
			__v = mac_readl(sc->dev,  REG_RX_BUF_PTR0);		\
	       		__ptr = RX_PTR(sc, bfget_m(__v, RX_BUF_PTR0, READ_PTR)); __ptr;})
#define rx_ptr_WRITE(sc)	({u32 __v; u16 __ptr;				\
			__v = mac_readl(sc->dev,  REG_RX_BUF_PTR1);		\
	       		__ptr = RX_PTR(sc, bfget_m(__v, RX_BUF_PTR1, WRITE_PTR)); __ptr;})
#define advance_rx_ptr_READY(sc)	do {					\
			u32 v; u16 ptr;						\
			v = mac_readl(sc->dev, REG_RX_BUF_PTR0);		\
			ptr = rx_ptr_READY(sc);					\
			ptr = RX_PTR(sc, ptr + 1);					\
			bfmod_m(v, RX_BUF_PTR0, READY_PTR, ptr);		\
			mac_writel(v, sc->dev, REG_RX_BUF_PTR0);		\
} while (0);
#define advance_rx_ptr_READ(sc)	do {						\
			u32 v; u16 ptr;						\
			v = mac_readl(sc->dev, REG_RX_BUF_PTR0);		\
			ptr = rx_ptr_READ(sc);					\
			ptr = RX_PTR(sc, ptr + 1);					\
			bfmod_m(v, RX_BUF_PTR0, READ_PTR, ptr);			\
			mac_writel(v, sc->dev, REG_RX_BUF_PTR0);		\
} while (0);
#define is_room_READY(sc)	({bool __room; u16 __wp, __rdp, __wi, __rdi;	\
			__wp = rx_ptr_WRITE(sc);__wi = RX_PTR2IDX(sc, __wp);	\
			__rdp = rx_ptr_READY(sc);__rdi = RX_PTR2IDX(sc, __rdp);	\
			__room = !(__wp != __rdp && __wi == __rdi); __room;})
#define is_room_READ(sc)	({bool __room; u16 __wp, __rp;			\
			__wp = rx_ptr_WRITE(sc);				\
			__rp = rx_ptr_READ(sc);					\
			__room = __wp != __rp; __room;})

#define init_ptr0(sc, which, val)		do {				\
			u32 v;							\
			v = mac_readl(sc->dev, REG_RX_BUF_PTR0);		\
			bfclr_m(v, RX_BUF_PTR0, which##_PTR);	\
			mac_writel(v, sc->dev, REG_RX_BUF_PTR0);		\
} while(0);

#define init_ptr1(sc, which, val)		do {				\
			u32 v;							\
			v = mac_readl(sc->dev, REG_RX_BUF_PTR1);		\
			bfclr_m(v, RX_BUF_PTR1, which##_PTR);	\
			mac_writel(v, sc->dev, REG_RX_BUF_PTR1);		\
} while(0);

#define read_ptr(sc, which)	rx_ptr_##which(sc)
#define read_index(sc, which)	RX_PTR2IDX(sc, read_ptr(sc, which))
#define advance_ptr(sc, which)	advance_rx_ptr_##which(sc)
#define is_room(sc, which)	is_room_##which(sc)
#define is_readable(sc, idx)	({bool __good = (idx >= read_ptr(sc, READ)) &&	\
					(idx < read_ptr(sc, WRITE)); __good;})	\

#define SCM2020_GET_HE_TB_LEN(sc, vif) mac_readl((sc)->dev, REG_VIF_BASIC_TRIG_PSDU_LEN(vif))

#define AP_ALIVE(vap) \
		(vap)->iv_swbmiss_count = 1; \
		(vap)->iv_bmiss_count = 0;

#define LINK_ALIVE(vap) \
	 AP_ALIVE(vap) \
	(vap)->iv_ic->ic_lastdata = ticks;

static inline int rx_desc_num_empty_slot(struct sc_softc *sc)
{
	int i, empty = 0;

	for (i = 0; i < sc->rx.n_rx_desc; i++) {
		if (isset(sc->rx.map, i))
			empty++;
	}

	return empty;
}

static inline int rx_desc_num_filled_slot(struct sc_softc *sc)
{
	return sc->rx.n_rx_desc - rx_desc_num_empty_slot(sc);
}

struct device *_wlandev(int inst);
extern struct device *(*wlandev)(int inst);

static inline struct sc_softc *wlansc(int inst)
{
	struct device *dev = wlandev(inst);

	return dev ? dev->driver_data : NULL;
}

static inline struct device *vap2dev(struct ieee80211vap *vap)
{
	struct sc_softc *sc = (struct sc_softc *)vap->iv_ic->ic_softc;
	return sc->dev;
}

static inline struct device *ic2dev(struct ieee80211com *ic)
{
	struct sc_softc *sc = (struct sc_softc *)ic->ic_softc;
	return sc->dev;
}

static inline struct sc_softc *txq2sc(struct sc_tx_queue *txq)
{
	struct sc_softc *sc = (struct sc_softc *)txq->ic->ic_softc;
	return sc;
}

/* VHT */
/*
 * ap has different meanings depending on the direction
 * for Tx : ap == true means the recipient is an AP
 * for Rx : ap == true means this station is an AP
 */
static inline u16 partial_aid(u16 aid, const u8 bssid[], bool ap)
{
	u16 v = 0;

	if (ap) {
		v = bssid[5] << 1 | (bssid[4] & 0x80 >> 7);
	} else if (aid > 0) {
		/* (bssid[44:47] ^ bssid[40:43]) * 2^5 */
		v = (((bssid[5] >> 4) ^ (bssid[5])) & 0xf) << 5;
		v += (aid & 0x3ff);
	}
	v &= 0x1ff; /* Take the 9 least significant bits */

	return v;
}

static inline u16 scm2020_checksum(const void *buf, size_t len)
{
	size_t i;
	u32 sum = 0;
	const u16 *pos = buf;

	for (i = 0; i < len / 2; i++)
		sum += *pos++;

	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return sum ^ 0xffff;
}

static inline int scm2020_rx_mic_error(struct hw_rx_hdr *hw)
{
	u32 *rxi = (u32 *) &hw->ri;

	/* No other errors, except for MIC */
	return ((rxi[1] & 0x0000007C) == (1 << 5));
}

/* TX */
extern void scm2020_init_tx_queue(struct sc_softc *sc);
struct sc_tx_queue *scm2020_get_txq(struct ieee80211vap *vap, int ac);
extern int (*scm2020_txop_kick)(struct sc_tx_queue *txq);

/* RX */
int rx_desc_num_hw_owned(struct sc_softc *, int *);
extern void (*scm2020_rx_desc_reinit)(struct sc_softc *sc, bool tx,
		void (*func)(struct ieee80211_node*, void*, int),
		void *context);

/* Etc */
extern void scm2020_init(void *priv);
extern void scm2020_stop(void *arg);
extern struct ieee80211vap *
scm2020_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ],
		int unit, enum ieee80211_opmode opmode, int flags,
		const u8 bssid[IEEE80211_ADDR_LEN],
		const u8 macaddr[IEEE80211_ADDR_LEN]);
extern void scm2020_vap_delete(struct ieee80211vap *vap);
extern void scm2020_newassoc(struct ieee80211_node *ni, int isnew);
extern void (*scm2020_vif_config)(struct sc_softc *sc,int vif, bool en);
extern int (*scm2020_key_cmd_common)(struct ieee80211vap *vap,
		struct ieee80211_key *key, int action);
extern void (*scm2020_set_sta_wep_key)(bool add, struct ieee80211_node *ni);
extern struct mbuf *(*scm2020_hw_encap)(struct mbuf *m0,
		struct tx_vector *tv,
		struct ieee80211_node *ni,
		struct ieee80211_key *key,
		const struct ieee80211_bpf_params *params);
bool scm2020_hw_is_promisc(struct sc_softc *sc);
void scm2020_hw_set_promisc(struct sc_softc *sc, bool on);
extern u64 (*scm2020_get_tsf)(struct ieee80211com *ic, struct ieee80211_node *ni);
extern u32 (*scm2020_get_ack_duration)(struct ieee80211_node *ni,
		u32 len, u16 rate, u8 ftype, bool ismcast);
extern void scm2020_mcc_init(struct ieee80211com *ic);
#ifdef CONFIG_SUPPORT_VHT
extern void scm2020_update_mu_group(struct ieee80211_node *ni,
		struct ieee80211_action_groupid_mgmt *gidm);
#endif
extern bool scm2020_ap_proximate(struct ieee80211_node *ni);

/* PHY */
int scm2020_fmtmcs2rate(int fmt, int mcs);
int scm2020_fmtmcs2dot11rate(int fmt, int mcs);
u32 scm2020_rate2fmtmcs(u16 rate);
int scm2020_update_tx_pwr(u8 fmt,u8 mcs_rate, u8 pwr);
struct scm2020_pwr_table* get_pwr_rate_table(enum ppdu fmt);
int scm2020_reset_tx_pwr(u8 fmt);


extern void (*scm2020_phy_init)(struct sc_softc *sc);
extern void (*scm2020_phy_set_channel)(struct sc_softc *sc,
		u32 ch_freq, u8 bw, u8 pri);
#ifdef CONFIG_SUPPORT_VHT
void scm2020_phy_vht_mu_group(struct sc_softc *sc,
		u8 vif, u32* msa, u32* upa);
void scm2020_phy_vht_gid_paid_filter(struct sc_softc *sc,
		int vif, bool enable, u16 aid, u8* addr);
#endif

extern void (*scm2020_phy_auto_mode)(struct sc_softc *sc);
extern void (*scm2020_phy_tone)(struct sc_softc *sc, u8 tone);
extern void (*scm2020_phy_multi)(struct sc_softc *sc, u8 tone);
#if defined(CONFIG_WLAN_HW_TL7118)
extern void (*scm2020_phy_dump)(struct sc_softc *sc, u32 loop, u32 trig, u8 dec);
#else
extern void (*scm2020_phy_dump)(struct sc_softc *sc, u32 loop);
#endif
extern void (*scm2020_phy_dump2)(struct sc_softc *sc,
		u32 lang, u8 iter, u8 trig, u8 mode, u8 deci);
extern void (*scm2020_phy_tx_mode)(struct sc_softc *sc);
extern void (*scm2020_phy_rx_mode)(struct sc_softc *sc);
extern void (*scm2020_phy_loopback)(struct sc_softc *sc, bool on);
extern void (*scm2020_phy_apply_cal)(struct sc_softc *sc, u8 bd_no);
extern void scm2020_phy_fcal_value_update(struct sc_softc *sc, int tx_ofs, int rssi_ofs);
extern void scm2020_phy_getrssi(struct sc_softc *sc);
extern void scm2020_phy_cfo_ofs(struct sc_softc *sc, int ofs);

/* RF */
extern void (*scm2020_rf_init)(struct sc_softc *sc);
extern bool (*scm2020_rf_set_channel)(struct sc_softc *sc,
		u32 ch_freq, u8 bw);
extern void (*scm2020_rf_power_on)(struct sc_softc *sc);
extern void (*scm2020_rf_power_off)(struct sc_softc *sc);
extern void (*scm2020_rf_get_cal_data)(struct sc_softc *sc, u8 bd_no, struct rf_cal_data *data);
extern bool (*ieee80211_amsdu_allow)(struct mbuf *m);

void scm2020_clock_gate(struct sc_softc *sc, bool on);
void scm2020_filter_mode_config(u8 mode);
u32 cturn_cal(struct sc_softc *sc);

/**
 * PM
 */
#if defined(CONFIG_WLAN_HW_SCM2010) || defined(CONFIG_WLAN_HW_TL7118)
#define scm2020_pm_stay()   pm_stay(PM_DEVICE_WIFI);
#define scm2020_pm_relax()  pm_relax(PM_DEVICE_WIFI);
#else
#define scm2020_pm_stay()
#define scm2020_pm_relax()
#endif
void scm2020_pm_set_wakeup_abstime(u64 wakeup_time);

void scm2020_ps_save_info_flag(void);
void scm2020_ps_leave_info_flag(void);
/**
 * Debugging
 */
void print_raw(void *, int, bool);
void print_hw_tx_hdr(struct hw_tx_hdr *);
void print_hw_rx_hdr(struct hw_rx_hdr *);
void scm2020_rx_desc_dump(struct sc_softc *sc);
void print_binary(char *str, u32 val);
void ieee80211_dump_frame(struct ieee80211com *ic, struct mbuf *m0, bool kernel);
void scm2020_dm(struct mbuf *m);
void _scm2020_dump_tx(struct sc_tx_queue *txq, bool raw_output);
void scm2020_dump_rx(struct mbuf *m, bool raw_output);
void scm2020_dump_rx_ba_frame(struct mbuf *m0);

void tst_send(void *arg, int pending);

#ifdef CONFIG_SCM2020_CLI_PHY
void scm2020_phy_status_avg(struct sc_softc *, bool, struct hw_rx_hdr *);
#else
#define scm2020_phy_status_avg(sc, mine, hdr)
#endif

void scm2020_txq_drop(void *data, int pending);
void scm2020_pm_start(struct ifnet *ifp);
void scm2020_comp_cfo(struct sc_softc *sc, int freq, struct ieee80211_rx_stats *rxs);
void scm2020_start_tx(struct ifnet *ifp);

/**
 * Cross-refs
 */
extern struct mbuf *(*rx_desc_refill_buffer)(struct sc_softc *sc,
					struct sc_rx_desc *desc);
extern void (*rx_desc_refill)(void *data, int pending);
extern void (*rx_done)(void *data, int pending);
extern void (*tx_reclaim)(void *data, int pending);
extern void (*twt_timeout_task)(void *arg, int pending);
extern void (*scm2020_start)(struct ifnet *ifp);
extern int 	(*scm2020_transmit)(struct ieee80211com *ic, struct mbuf *m);
extern int 	(*scm2020_raw_xmit)(struct ieee80211_node *ni, struct mbuf *m,
			const struct ieee80211_bpf_params *params);
extern void (*scm2020_update_slot)(struct ieee80211com *ic);
extern void (*scm2020_update_promisc)(struct ieee80211com *ic);
extern int (*scm2020_ampdu_rx_start)(struct ieee80211_node *ni,
			struct ieee80211_rx_ampdu *rap, int baparamset,
			int batimeout, int baseqctl);
extern void	(*scm2020_ampdu_rx_stop)(struct ieee80211_node *ni,
			struct ieee80211_rx_ampdu *rap);

extern void	(*scm2020_scan_start)(struct ieee80211com *ic, struct ieee80211vap *vap);
extern void	(*scm2020_scan_end)(struct ieee80211com *ic, struct ieee80211vap *vap);

extern void	(*scm2020_scan_set_mandatory_hwrates)(struct ieee80211com *ic,
			struct ieee80211vap *vap, struct ieee80211_channel *chan);
extern void	(*scm2020_set_channel)(struct ieee80211com *ic);

#ifdef CONFIG_SOC_SCM2010
extern void (*scm2020_mu_edca_timeout)(void *arg);
extern int (*scm2020_update_bss_color)(struct ieee80211_node *ni, bool reinit);
extern int (*scm2020_update_muedca)(struct ieee80211_node *ni);
extern int (*scm2020_update_uora)(struct ieee80211_node *ni, bool reinit);
extern int (*scm2020_update_sr)(struct ieee80211_node *ni);
extern int (*scm2020_set_sta_aid)(struct ieee80211_node *ni, bool add);
#ifdef CONFIG_SUPPORT_HE
extern void (*scm2020_phy_he_bss_color_filter)(struct sc_softc *sc, int vif, bool enable, u8 bss_color);
extern void (*scm2020_phy_he_dir_filter)(struct sc_softc *sc, int vif, u8 dir);
extern void (*scm2020_phy_he_sta_id_filter)(struct sc_softc *sc, int vif, u16 aid, u8 bssidx);
#endif
#elif CONFIG_WLAN_HW_TL7118
void	scm2020_mu_edca_timeout(void *arg);
int scm2020_update_bss_color(struct ieee80211_node *ni, bool reinit);
int scm2020_update_muedca(struct ieee80211_node *ni);
int scm2020_update_uora(struct ieee80211_node *ni, bool reinit);
int scm2020_update_sr(struct ieee80211_node *ni);
int scm2020_set_sta_aid(struct ieee80211_node *ni, bool add);
#ifdef CONFIG_SUPPORT_HE
void scm2020_phy_he_bss_color_filter(struct sc_softc *sc,
		int vif, bool enable, u8 bss_color);
void scm2020_phy_he_dir_filter(struct sc_softc *sc,
		int vif, u8 dir);
void scm2020_phy_he_sta_id_filter(struct sc_softc *sc,
		int vif, u16 aid, u8 bssidx);
#endif
#endif




extern void	(*scm2020_set_twt_timer)(struct ieee80211com *ic, struct ieee80211_node *ni,
			u8 timertype, u32 value, bool disable_timer);
extern void	(*scm2020_hmac_int_mask)(struct sc_softc *sc, bool on);
extern void	(*scm2020_hmac_enable)(struct sc_softc *sc, bool on);
extern u32 	(*scm2020_txop_schedule_ampdu)(struct sc_tx_queue *txq, u32 t_prot,
			struct ieee80211_node *ni,u32 *pt_to_protect, u16 *prate, u32 sifs);
extern u32 	(*scm2020_txop_schedule_mpdu)(struct sc_tx_queue *txq, u32 t_prot,
			u32 *pt_to_protect, u16 *prate);

extern int  (*ieee80211_amsdu_inc_subframes)(struct mbuf *m);
extern int 	(*ieee80211_amsdu_prepare_head)(struct mbuf *m, u_int8_t max_subframes);

extern bool (*ieee80211_amsdu_aggregate)(struct sc_tx_queue *txq, struct mbuf *m);
extern void (*scm2020_tx_timeout)(void *arg);
#ifndef CONFIG_MAC_TX_TIMEOUT
extern void (*scm2020_tx_complete)(struct sc_tx_queue *txq);
#else
extern void	(*scm2020_tx_complete)(struct sc_tx_queue *txq, bool timeout);
extern osTimerId_t txq_timeout_timer;
extern u32 txq_timestamp[SC_NR_TXQ];
#endif
extern void	(*scm2020_txq_flush)(struct ieee80211vap *vap);
extern void	(*scm2020_rx_init)(struct sc_softc *sc);
extern void	(*scm2020_rx_desc_deinit)(struct sc_softc *sc);
extern void	(*scm2020_rx_desc_init)(struct sc_softc *sc,
			void (*func)(struct ieee80211_node*, void*, int), void *context);
extern void	(*scm2020_handle_rx_done)(struct sc_softc *sc);
extern void	(*scm2020_set_bssid)(struct device *dev, u32 vif,
			const u8 *assoc_bssid, const u8 *trans_bssid);
extern void	(*scm2020_set_bssid_mask)(struct device *dev, u32 vif, u8 bssid_indicator);
extern void	(*scm2020_config_rx_filter)(struct ieee80211vap *vap,
			scm2020_reg_mac_en mac_enable, scm2020_reg_bss_en bss_enable, bool bfe_en);

extern void	(*scm2020_set_tsf)(struct ieee80211_node *ni,
			struct device *dev, struct ieee80211vap *vap, int vif,
			const struct ieee80211_rx_stats *rxs);
extern void	(*scm2020_phy_set_cs_threshold)(struct sc_softc *sc, int rssi);
extern void	(*scm2020_phy_fixup)(struct sc_softc *sc);
extern void (*scm2020_fill_rx_stats)(struct ieee80211com *ic,
		struct ieee80211_rx_stats *rxs, struct hw_rx_hdr *hw, bool is_data);
extern void	(*scm2020_recv_mgmt)(struct ieee80211_node *ni, struct mbuf *m0,
			int subtype, struct ieee80211_rx_stats *rxs, int rssi, int nf);
extern void	(*scm2020_sta_pwrsave)(struct ieee80211_node *ni, void *arg, int status);
extern int scm2020_set_options(struct ieee80211vap *vap, enum ieee80211_set_opt opt,
			int val, int len, void *priv);
extern int scm2020_get_options(struct ieee80211vap *vap, enum ieee80211_get_opt opt,
	const struct ieee80211req *ireq);

#ifdef CONFIG_SCM2010_WEP_WORKAROUND
extern void (*scm2020_set_sta_wep_key)(bool add, struct ieee80211_node *ni);
#endif /* CONFIG_SCM2010_WEP_WORKAROUND */

extern uint8_t g_sc_nr_max_amsdu;
extern uint8_t g_sc_nr_bds_hdr;
extern uint8_t g_sc_bds_adj_offset;
extern uint8_t g_sc_bds_hdr_sz;
extern uint8_t g_sc_txkeep_intvl;
extern uint32_t g_wc_txkeep_intvl;
extern uint8_t g_wc_txkeep_mode;
extern uint8_t g_sc_monitor_bmiss;
extern uint8_t g_sc_bmiss_thresold;
extern uint8_t g_sc_bmiss_dyn_win_en;
extern int8_t  g_sc_ap_proximate_rssi_thr;
extern uint8_t g_sc_ap_proximate_tx_pwr;
extern uint8_t g_sc_ap_proximate_mode_dis;

#endif /* __SCM2020_VAR_H__ */

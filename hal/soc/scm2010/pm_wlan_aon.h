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

#ifndef _PM_WLAN_AON_H_
#define _PM_WLAN_AON_H_

#define _ATTR_ALWAYS_INLINE		__attribute__ ((always_inline))

/* VIF1 Backup and Restore Enable Feature */
#define CONFIG_PS_PM_VAP_SUPPORT

/* RF Power current reduction Feature in DTIM Parser waiting */
#define CONFIG_PM_RF_CURRENT_REDUCTION

/* SOC FIB V1 Wakeup delay both Wise and Watcher */
#define SCM2010_SOC_FIB_AWAKE_DELAY				6500	//7000

/* Consider DTIM parser HW procssing time */
#define SCM2010_DTIM_EARLY_AWAKE_TIME			0

/* Hibernation early wakeup margin */
#define SCM2010_SOC_HI_AWAKE_DELAY 				2000

#define	IEEE80211_ADDR_LEN  6

#define BCNLOSS_FOREVER 0xff

#define WAON_RTC_CAL_MODE_TSF 0
#define WAON_RTC_CAL_MODE_TIMER_SNAP 1

enum full_boot_reason {
    FULL_BOOT_REASON_NONE           = 0,
	FULL_BOOT_REASON_WIFI,
	FULL_BOOT_REASON_UART,
	FULL_BOOT_REASON_GPIO,
	FULL_BOOT_REASON_NEXT_WAKEUP,
	FULL_BOOT_REASON_MARGIN,
	FULL_BOOT_REASON_WAKEUP_SRC,
	FULL_BOOT_REASON_WAKEUP_TYPE,
	FULL_BOOT_REASON_UNEXPECTED,
	FULL_BOOT_REASON_MAX,
};

#define WC_MAX_TX_BUF_SIZE 124

#define HW_TX_HDR_LEN sizeof(struct hw_tx_hdr)

/* arp response uses wise buffer */
#define WAON_TX_EXT_BUF_NUM 1

enum waon_tx_pkt_idx {
	WAON_TX_PKT_PPOL_IDX = 0, /* PS Poll */
	WAON_TX_PKT_NULL_IDX = 1, /* QoS NULL for Keepalive */
	WAON_TX_PKT_GARP_IDX = 1, /* GARP Reply */
#if defined(CONFIG_SUPPORT_WC_MQTT_KEEPALIVE) || defined(CONFIG_WATCHER_MQTT_KEEPALIVE)
	WAON_TX_PKT_MQTT_IDX = 2, /* MQTT PING for keep alive */
	WAON_TX_PKT_MQTT_ACK_IDX = 2,
#endif
#ifdef WATCHER_HP
	WAON_TX_PKT_UDPA_IDX = 2, /* UDP Ack for hole punch */
	WAON_TX_PKT_TCPA_IDX = 3, /* TCP Ack for hole punch */
#endif
	WAON_TX_PKT_IDX_NUM
};

enum waon_tx_pkt_type {
	WAON_TX_PKT_PPOL, /* PS Poll */
	WAON_TX_PKT_NULL, /* QoS NULL for Keepalive */
	WAON_TX_PKT_GARP, /* GARP Reply */
	WAON_TX_PKT_MQTT, /* MQTT PING for keep alive */
	WAON_TX_PKT_MQTT_ACK,
#ifdef WATCHER_HP
	WAON_TX_PKT_UDPA, /* UDP Ack for hole punch */
	WAON_TX_PKT_TCPA,  /* TCP Ack for hole punch */
#endif
	/*
	 * Following definition is not really allocated buffer so please don't move it up
	 */
	WAON_TX_PKT_ARPR, /* ARP Response */
	WAON_TX_PKT_NUM
};

enum waon_wifi_boot_type {
	WIFI_BOOT_UC,
	WIFI_BOOT_TX_PPOL_FAIL,
	WIFI_BOOT_TX_ARPR_FAIL,
	WIFI_BOOT_BCN_LOSS,
	WIFI_BOOT_TX_MQTT_FAIL,
	WIFI_BOOT_TYPE_MAX,
};

#define WIFI_BOOT_SUBTYPE_MAX 4

#define WAON_HW_BAK_SIZE    (48)

struct waon_hw_ctx {
	u32 address; /* 0 indicates the end */
	u32 value;
};

struct waon_counter {
	u16 bcn_rx;				/* beacon rx count in watcher */
	u16 bcn_loss;			/* beacon loss count in watcher */
	u16 bcn_loss_fcs_nok;	/* beacon loss with FCS nok */
	u16 bcmc_rx;			/* BC/MC rx count in Watcher */
	u16 uc_rx;				/* UC rx count in Watcher */
	u16 ap_ka;				/* Keepalive from AP count in Watcher */
	u32 wc_wk_time_us;		/* wait rx total time */
	u16 level;				/* window level */
	u16 count[10];			/* window level count */
	u16 window[10];			/* window level count */
	int driftplus;			/* ext drift */
	int driftminus;			/* early drift */
};

struct waon_ctx {
	u64 dtim_peroid;				/* DTIM period[us] for PM: DTIM interval * beacon interval */
	u8  ni_dtim_period;				/* current beacon rx waiting time */
	u8  sched_tx;					/* sched tx, refer to t2b(waon_tx_pkt_type) */
	u32 itvl_keepalive;				/* keepalive interval */
	u32 itvl_udpa;					/* udpa interval */
	u32 itvl_tcpa;					/* tcpa interval */

	u8  port_filter			: 1;	/* port filter en */
	u8  ignore_bcmc			: 1;	/* (test only) ignore BC/MC frames */
	u8  ignore_uc			: 1;	/* (test only) ignore UC frames */
	u8  log_flag			: 1;	/* watcher log flag */
	u8  wakeup_by_rx		: 1;	/* not do full rx desc restore in the Wise */
	u8  non_trans_node		: 1;	/* ni_macaddr is a nontransmitted BSS profile */
	u8  rf_reduced			: 1;	/* rf reduction set flag in watcher */
#ifdef WATCHER_CHECK_PHY_COUNTERS
	u8  phy_cs_sat_cnt_nok  : 1;	/* PHY CS SAT counter being stuck */
	u8  phy_cs_stf_cnt_nok  : 1;	/* PHY CS STF counter being stuck */
	u8  phy_cs_b11_cnt_nok  : 1;	/* PHY CS B11 counter being stuck */
	u8  phy_fcs_ok_cnt_nok  : 1;	/* PHY FCS OK counter being stuck */
#endif
	u8  ni_bssid_indicator;			/* 2^bssid_indicator is the maximum number of APs in set */
	u8  ni_bssid_index;				/* index in the multiple BSS set */
#ifdef WATCHER_GPIO_DBG
	u16 gpio_dbg_cnt;				/* gpio dbg consecutive count */
#endif
	u8  bcn_loss_chk		: 1;	/* beacon lost last check */
	u8  wifi_active_state	: 1;	/* in fullbooting, set as active state */
	u8  uc_poll_proc		: 1;	/* polling UC packet(s) */
	u8  macdtim_int_cnt		: 1;	/* for DTIM Interrupt checking flag */
	u8  beacon_int_cnt		: 1;	/* for Beacon Interrupt checking flag */
	u8  txdone				: 1;	/* for tx done interrupt checking flag */
	u8  rxdone				: 1;	/* for RX BUF interrupt checking flag */
	u8  rx_bcmc_end			: 1;	/* for RX BUF interrupt BC/MC checking flag */
	u16 nonqos_seqno;				/* IEEE80211_NONQOS_TID seqno */
	u32 mcubin_addr;				/* Firmware loading start addr */
	u32 mcubin_size;				/* Firmware size */
	u32 last_mbuf;					/* for restoring last mbuf */
	u8 rtc_cal_mode;				/* Last beacon dtim interrupt time */
	u32 wakeup_leadtime;			/* Manual wakeup lead time by user cmd */
	u16 beacon_rx_count; 			/* current beacon rx success count by DTIM Parser HW */
	u16 beacon_loss_count;			/* current beacon rx fail count by DTIM Parser HW */
	u8  uc_rx_timeout;				/* UC Timeout value by PS-Poll tx */
	u8  bcnloss_threshold_time;		/* Full wakeup by Consecutive BCN Loss value */
	u16 chanfreq;
	u16 icfreq;
	u32 icflag;
	u16 ni_associd;
	u8  ni_bssid[IEEE80211_ADDR_LEN];
	u8  iv_myaddr[IEEE80211_ADDR_LEN];
	u8  tx_bssid[IEEE80211_ADDR_LEN];
	u32 ni_intval;
	u32 ic_headroom;		/* Already rounded up */
	u32 mbufsz;
	u32 mbufdsz;
	u32 rx_rd_ptr;
	u8  n_rx_desc;			/* sc->rx.n_rx_desc */
	u8  n_rx_desc_org;		/* sc->rx.n_rx_desc */
	u32 arp_tx_buf;
	u32 early_wk_time;
	u32 ext_wk_time;
	struct waon_counter counter;
	u8  fix_early_base;
	u8  fix_ext_base;
	u8  fix_early_adj;
	u8  fix_ext_adj;
	int wk_offset_us;
	u8  adv_lp;
	u8  bcn_on_time;
	u32 last_data_time;
	u32 rtc;
	u32 rtc_cnt_down_us;
	u32 tsf;
	u32 tsf_diff;
	u32 rtc_remaining_us;
	u8  wc_wk_cont;
	u32 wc_wk_avg_time_us;
	u32 wc_wk_time_us;
#if defined(CONFIG_SUPPORT_WC_MQTT_KEEPALIVE) || defined(CONFIG_WATCHER_MQTT_KEEPALIVE)
	u16 mqtt_pingreq_ipsum;
	u32 mqtt_prev_acknum;
	u32 mqtt_sent_len;	/* TCP payload len that watcher has been sent */
	u8  mqtt_pingrxfail;
#endif
};

struct waon_arpheader {
	/* arp header & data */
	u8	hardware_type[2];
	u8	protocol_type[2];
	u8	hardware_size;
	u8	protocol_size;
	u8	opcode[2];
	u8	sender_mac[IEEE80211_ADDR_LEN];
	u8	sender_ip[4];
	u8	target_mac[IEEE80211_ADDR_LEN];
	u8	target_ip[4];
	u8	padding[18];	/* It should over 64 byte in 802.3, so added padding */
};
/**
 * GARP packet frame information for watcher working
 */
struct waon_arpframe {
	/* llc snap header */
	u8 hdr_llc_snap[6];
	u8 hdr_ether_type[2];

	/* arp header & data */
	struct waon_arpheader arp_hdr;
};

struct waon_ipheader {
#if BYTE_ORDER == BIG_ENDIAN
	uint8_t			ip_v:4;
	uint8_t			ip_hl:4;
#elif BYTE_ORDER == LITTLE_ENDIAN
	uint8_t			ip_hl:4;
	uint8_t			ip_v:4;
#endif
	uint8_t			ip_tos;
	uint16_t		ip_len;
	uint16_t		ip_id;
	int16_t			ip_off;
	uint8_t			ip_ttl;
	uint8_t			ip_p;
	uint16_t		ip_sum;
	uint32_t		ip_src;
	uint32_t		ip_dst;
} __packed;

struct waon_tcpheader {
	uint16_t	th_sport;		/* source port */
	uint16_t	th_dport;		/* destination port */
	uint32_t	th_seq;			/* sequence number */
	uint32_t	th_ack;			/* acknowledgement number */
#if BYTE_ORDER == LITTLE_ENDIAN
	uint8_t	th_x2:4,		/* (unused) */
		    th_off:4;		/* data offset */
#endif
#if BYTE_ORDER == BIG_ENDIAN
	uint8_t	th_off:4,		/* data offset */
		th_x2:4;		/* (unused) */
#endif
	uint8_t	th_flags;

	uint16_t	th_win;			/* window */
	uint16_t	th_sum;			/* checksum */
	uint16_t	th_urp;			/* urgent pointer */
} __packed;

struct waon_tcpackframe {
	/* llc snap header */
	u8 hdr_llc_snap[6];
	u8 hdr_ether_type[2];

	/* IP header */
	struct waon_ipheader ip_hdr;
	/* TCP header */
	struct waon_tcpheader tcp_hdr;
};

/* Key structure used in watcher */
#define	IEEE80211_WEP_NKID		4       /* number of key ids */
#define	IEEE80211_KEYBUF_SIZE	16
#define	IEEE80211_MICBUF_SIZE	(8+8)   /* space for both tx+rx keys */

struct waon_ieee80211_key {
	u8 	wk_keylen;
	u8 	cipher 			: 3; 	/* for bf(16, 18, cipher)[key_cmd] */
	u8 	wk_keyix 		: 2; 	/* h/w key index, bf(8, 9, keyid);[key_cmd] */
	u8 	devkey 			: 1;
	u8 	group 			: 1;
	u8 	wk_spp_amsdu	: 1;
	u8 	wk_key[IEEE80211_KEYBUF_SIZE + IEEE80211_MICBUF_SIZE];	/* 16 + 16 */
#define	wk_txmic	wk_key+IEEE80211_KEYBUF_SIZE+0  /* XXX can't () right */
#define	wk_rxmic	wk_key+IEEE80211_KEYBUF_SIZE+8  /* XXX can't () right */
	/* In STA mode, wk_macaddr is same with BSSID */
	u8 	hw_key_idx;
};

/* Security structure used in watcher */
struct waon_security_info {
	u8 	sec_mode 		: 1;
	u8 	txcipher 		: 2;
	u8 	txkeyid;
	u8 	cipherheader;  		/* size of privacy header (bytes) */
	u8 	ciphertrailer; 		/* size of privacy trailer + mic trailer (bytes) */
	u64	txkeytsc; 			/* key transmit sequence counter */
	struct waon_ieee80211_key wc_key[IEEE80211_WEP_NKID];
	struct waon_ieee80211_key wc_ucastkey;
};

/* This is TX packet between wise and watcher */
/* struct tx_vector(4x7:28)+struct ieee80211_frame(over 24)	+ data(over 16) */
struct waon_pkt_hdr {
	u8  len;
	u8  data[WC_MAX_TX_BUF_SIZE];			/* PS-Poll, ARP reply, TCP Ack, Max 128 */
};

struct waon_tx_wmm {
	u32 txop_limit; 		/* IEEE80211_TXOP_TO_US(wmep->wmep_txopLimit) */
	u32 cw;					/* txq->edca.cw_min */
	u8  aifsn;				/* txq->edca.aifsn */
};

struct waon_hw_tx_desc {
	struct bdesc bd;
	struct mentry me;
};

struct waon_tx_pkt_ctx {
	u32 tx_time;			/* transmitting time (periodic time), if tx time is started, else tx is done, clear */
	struct waon_pkt_hdr pkt;
};

enum {
	FILTER_TCP = 0,
	FILTER_UDP,
	FILTER_MC,
	FILTER_MAX,
};

/* Watcher max mc filter */
#define CONFIG_WAON_ACCEPT_MAX_MC 4
/* Watcher max tcp port filter */
#define CONFIG_WAON_ACCEPT_MAX_FILTER 8

struct waon_mc_info {
	u32 mc_addr;
	u32 rtc_time;
};

struct waon_accept_info {
	union {
		u16 port_num;
		u32 mc_addr;
	};
	u32 rtc_time;
};

struct waon_filter_info {
    u8 tot_reg;
    u8 reserved;
    struct waon_accept_info accepted[CONFIG_WAON_ACCEPT_MAX_FILTER];
};

/* for getting ip and default gw information */
struct waon_net_info {
	u32 net_my_ip;
	u8  net_default_mac[IEEE80211_ADDR_LEN];
};

/* Watcher ARP Cache info */
struct waon_arp_info {
	u8 sender_mac[IEEE80211_ADDR_LEN];
	u8 sender_ip[4];
};

#define CONFIG_WAON_MAX_ARP_CACHE 	5
struct waon_arp_cache {
	u8 tot_arp_cache;
	u8 reserved;
	struct waon_arp_info arp_info[CONFIG_WAON_MAX_ARP_CACHE];
	u8 arp_hdr[64];
};

struct waon {
	struct waon_ctx             ctx;
	struct waon_hw_ctx          hw_ctx[WAON_HW_BAK_SIZE];
	struct waon_net_info        net_info;
	struct waon_security_info   sec_info;
	struct waon_filter_info     filter_info[3]; // 0: tcp_filter, 1: udp_filter, 2: mc_filter
#ifdef CONFIG_WATCHER_ARP_CACHE
	struct waon_arp_cache       arp_cache;
#endif
	struct waon_tx_wmm          tx_wmm;
	struct waon_tx_pkt_ctx      tx_pkt_ctx[WAON_TX_PKT_IDX_NUM];
};

extern volatile struct waon waon;

#define waon_p(x) (&(waon.x))

#define WAON_LOG_FLAG (waon_p(ctx)->log_flag)

#define waon_get_tot_reg(_f) waon_p(filter_info[_f])->tot_reg

#define waon_set_tot_reg(_f, _n) \
do { \
	waon_p(filter_info[_f])->tot_reg = _n; \
} while (0);


#define waon_get_rtc_time(_f, _n) waon_p(filter_info[_f])->accepted[_n].rtc_time

#define waon_set_rtc_time(_f, _n, _t) \
do { \
	waon_p(filter_info[_f])->accepted[_n].rtc_time = _t; \
} while (0)


//#define waon_get_filter_size(_f) (sizeof(waon_p(filter_info[_f])->accepted)/sizeof(waon_p(filter_info[_f])->accepted[0]))

#define waon_get_accept_mc_addr(_f, _n) waon_p(filter_info[_f])->accepted[_n].mc_addr

#define waon_set_accept_mc_addr(_f, _n, _a) \
do { \
	waon_p(filter_info[_f])->accepted[_n].mc_addr = _a; \
	if (_a) \
		waon_p(filter_info[_f])->tot_reg++; \
} while (0)

#define waon_get_accept_port(_f, _n) waon_p(filter_info[_f])->accepted[_n].port_num

#define waon_set_accept_port(_f, _n, _a) \
do { \
	waon_p(filter_info[_f])->accepted[_n].port_num = _a; \
	if (_a) \
		waon_p(filter_info[_f])->tot_reg++; \
	else if (!_a && waon_p(filter_info[_f])->tot_reg > 0) /* if 0, clearing */ \
		waon_p(filter_info[_f])->tot_reg--; \
} while (0)


#define waon_get_net_ipaddr() waon_p(net_info)->net_my_ip

#define waon_set_net_ipaddr(_i) \
do { \
	if (waon_p(net_info)->net_my_ip != _i) \
		waon_p(net_info)->net_my_ip = _i; \
} while (0)

/* Kind of TX Packet */
#define t2b(x)					(1 << x)

#define	WC_PS_POLL_PKT	t2b(WAON_TX_PKT_PPOL)	/* ps poll packet */
#define	WC_NULL_PKT		t2b(WAON_TX_PKT_NULL)	/* null packet */
#define	WC_GARP_PKT		t2b(WAON_TX_PKT_GARP)	/* GARP packet */
#define	WC_ARP_RESP_PKT	t2b(WAON_TX_PKT_ARPR)	/* ARP Response packet */
#ifdef WATCHER_HP
#define	WC_UDP_HOLE_PKT	t2b(WAON_TX_PKT_UDPA)	/* UDP Hole Punch packet */
#define	WC_TCP_HOLE_PKT	t2b(WAON_TX_PKT_TCPA)	/* TCP Hole Punch packet */
#endif
#define	WC_MQTT_PKT		t2b(WAON_TX_PKT_MQTT)	/* MQTT PING Req packet */
#define	WC_MQTT_ACK_PKT	t2b(WAON_TX_PKT_MQTT_ACK)

extern const u8 tx_pkt_ctx_idx[];

#define ID(x)				tx_pkt_ctx_idx[x]
#define WC_TX_PKT_TIME(x)	waon_p(tx_pkt_ctx[ID(x)])->tx_time

#define WC_TX_PKT(x)		&(waon_p(tx_pkt_ctx[ID(x)])->pkt)
#define WC_TX_PKT_ARP()		&(((struct waon_tx_pkt_ctx *) (waon_p(ctx)->arp_tx_buf))->pkt);

#define waon_clear_tx_time() \
do { \
	WC_TX_PKT_TIME(WAON_TX_PKT_NULL) = 0; \
	WC_TX_PKT_TIME(WAON_TX_PKT_GARP) = 0; \
} while (0)


#define waon_clear_arp_cache() \
do { \
	waon_p(arp_cache)->tot_arp_cache = 0; \
} while (0)



#define SHORT_SLEEP_TIME_US 500 * 1000
enum {
	MODE_SHORT_SLEEP, /* sleep duration < 500ms */
	MODE_LONG_SLEEP, /* sleep duration > 500ms */
	NUM_MODES
};

#define SCM2010_EARLY_F_W_BASE_US 10000
#define SCM2010_EXT_F_W_BASE_US   50000
#define SCM2010_EARLY_W_BASE_US 0
#define SCM2010_EXT_W_BASE_US   7000
#define SCM2010_ADV_EARLY_W_BASE_US 0

#define EARLY_WK_ADJ_UNIT_US 500
#define EXT_WK_ADJ_UNIT_US 1000

#define EARLY_BASE_DEFAUL 0xFF
#define EXT_BASE_DEFAUL 0xFF
#define EARLY_ADJ_DEFAUL 0xFF
#define EXT_ADJ_DEFAUL 0xFF

static inline void scm2010_doze_window_reset(void) {
	if(waon_p(ctx)->fix_early_base >= 0 && waon_p(ctx)->fix_early_base < EARLY_BASE_DEFAUL)
		waon_p(ctx)->early_wk_time = waon_p(ctx)->fix_early_base * EARLY_WK_ADJ_UNIT_US;
	else
		waon_p(ctx)->early_wk_time = SCM2010_EARLY_F_W_BASE_US;

	if(waon_p(ctx)->fix_ext_base >= 0 && waon_p(ctx)->fix_ext_base < EXT_BASE_DEFAUL)
		waon_p(ctx)->ext_wk_time = waon_p(ctx)->fix_ext_base * EXT_WK_ADJ_UNIT_US;
	else
		waon_p(ctx)->ext_wk_time = SCM2010_EXT_F_W_BASE_US;
}

static inline void scm2010_dtmparse_window_reset_early(void) {
	if(waon_p(ctx)->fix_early_base >= 0 && waon_p(ctx)->fix_early_base < EARLY_BASE_DEFAUL)
			waon_p(ctx)->early_wk_time = waon_p(ctx)->fix_early_base * EARLY_WK_ADJ_UNIT_US;
	else
		waon_p(ctx)->early_wk_time = (waon_p(ctx)->adv_lp ? SCM2010_ADV_EARLY_W_BASE_US : SCM2010_EARLY_W_BASE_US);
}

static inline void scm2010_dtmparse_window_reset_ext(void) {
	if(waon_p(ctx)->fix_ext_base >= 0 && waon_p(ctx)->fix_ext_base < EXT_BASE_DEFAUL)
		waon_p(ctx)->ext_wk_time = waon_p(ctx)->fix_ext_base * EXT_WK_ADJ_UNIT_US;
	else
		waon_p(ctx)->ext_wk_time = SCM2010_EXT_W_BASE_US;
}

static inline void scm2010_dtmparse_window_reset(void) {

	waon_p(ctx)->counter.level = 0;

	scm2010_dtmparse_window_reset_early();

	scm2010_dtmparse_window_reset_ext();
}

#endif /* _PM_WLAN_AON */

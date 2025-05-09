/*
 * Copyright 2022-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
#include <stdio.h>
#include <math.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <soc.h>
#include <hal/arm/asm/barriers.h>
#include <hal/kernel.h>
#include <hal/console.h>
#include <hal/wlan.h>
#include <hal/kmem.h>
#include "compat_param.h"

#include "mbuf.h"
#include "kernel.h"
#include "atomic.h"
#include "systm.h"
#include "malloc.h"
#include "mutex.h"


#include "compat_if.h"
#include "if_arp.h"
#include "if_llc.h"
#include "ethernet.h"
#include "if_dl.h"
#include "if_media.h"
#include "compat_if_types.h"
#include "if_ether.h"
#include "lwip-glue.h"

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_sta.h>


#include "scm2020_var.h"
#include "scm2020_regs.h"
#include "scm2020_shmem.h"
#include "scm2020_pm.h"

#include <hal/unaligned.h>
#include <hal/compiler.h>
#include <hal/systimer.h>

#include "pm_rram.h"
#include "pm_wlan_aon.h"
#include <hal/pm.h>
#include "FreeRTOS_tick_config.h"
#include "lwip/prot/etharp.h"

#include <hal/rtc.h>

#include "lwip/prot/ip.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/ip6.h"
#include "lwip/prot/udp.h"
#include "lwip/prot/tcp.h"
#include "lwip/tcp.h"


/**************************************************************************
 * Section Definitions
 **************************************************************************/

__section(".waon") volatile struct waon waon;


/**************************************************************************
 * Macro Definitions
 **************************************************************************/


/* TICKLESS PM WIFI Debugging log */
/*
#define CONFIG_TICKLESS_PM_WIFI_LOG
*/
/*
#define DEBUG_TBTT
*/

#ifdef CONFIG_TICKLESS_PM_WIFI_LOG
#define pm_wifi_log printk
#else
#define pm_wifi_log(args...)
#endif

#ifdef CONFIG_SUPPORT_DUAL_VIF
#define SCM2020_WC_ARP_TX_AC	8
#else
#define SCM2020_WC_ARP_TX_AC 	3
#endif

#define MACFW_RETRY_CNT         (10)

#define SCM2020_PM_IEEE80211_HDR(pkt) (pkt->data + HW_TX_HDR_LEN)

/*
 * WiFi legacy PS
 * Doze --> Wakeup working Primitive
 */
#define SCM2020_DOZE_AWAKE_DELAY 800	/* PM_EARLY_WAKEUP_TIME 1800us */
#define SCM2020_BCN_RX_DEF_TIME_DELAY	2500 /* Default Time from BCN RX to BSS Update */
#define SCM2020_BCN_MIN_DURATION	 (80 << 10) /* DTIM 1 - (HW restoring time + First early wakeup + RX diff) */
#define SCM2020_BCN_MAX_DURATION	 (3000 << 10) /* DTIM 30 (3s) */

/* UDP OR MC Simulation Test */
//#define CONFIG_PS_FILTER_SIMUL_TEST */
//#define PM_SOC_WAKEUP_TEST

/**
 * After Deep sleep,
 * MAC/PHY Register should be restored to the state before deep sleep.
 * 1. HMAC initialize scm2020_hmac_init() include Enable firmware
 * 2. Phy Reg Init scm2020_phy_init()
 * 3. RX Buffer Desc Reinit scm2020_rx_desc_reinit()
 * 4. mac addr assign
 */
#define USEC_PER_MSEC	1000L

/* WATCHER Filter Timestamp feature */
#ifdef CONFIG_N22_ONLY
#define CONFIG_WATCHER_FILTER_TIMESTAMP
#endif

#define VERIFY_WLAN_MAC_REGS 0
#define VERIFY_WLAN_PHY_REGS 0

#if VERIFY_WLAN_MAC_REGS
#define MAC_REG_SIZE (4*1024)
#endif /* VERIFY_WLAN_MAC_REGS */

#if VERIFY_WLAN_PHY_REGS == 1
enum {
	PHY_REG_SEG_BASIC_1,
	PHY_REG_SEG_BASIC_2,
	PHY_REG_SEG_TXBE,
	PHY_REG_SEG_RXBE,
	PHY_REG_SEG_TRX_FE,
	PHY_REG_SEG_11B,
	PHY_REG_SEG_RXTD,
	PHY_REG_SEG_TRX_MID,
	PHY_REG_SEG_RFI,
	PHY_REG_SEG_RX_LUT,
	PHY_REG_SEG_TX_LUT,
	PHY_REG_SEG_NUM
};

#define PHY_BASIC_1_REG_START	(PHY_BASE_START)
#define PHY_BASIC_1_REG_SIZE	(0x0fc)	/* 0xf0f20000 - 0xf0f200fc */
#define PHY_BASIC_2_REG_START	(PHY_BASE_START + 0x01f0)
#define PHY_BASIC_2_REG_SIZE	(0x210)	/* 0xf0f201f0 - 0xf0f20400 */
#define PHY_TXBE_REG_START		(PHY_BASE_START + 0x1000)
#define PHY_TXBE_REG_SIZE		(0x01c)	/* 0xf0f21000 - 0xf0f2101c */
#define PHY_RXBE_REG_START		(PHY_BASE_START + 0x1400)
#define PHY_RXBE_REG_SIZE		(0x020)	/* 0xf0f21400 - 0xf0f21420 */
#define PHY_TRX_FE_REG_START	(PHY_BASE_START + 0x1800)
#define PHY_TRX_FE_REG_SIZE		(0x00c)	/* 0xf0f21800 - 0xf0f2180c */
#define PHY_11B_REG_START		(PHY_BASE_START + 0x2800)
#define PHY_11B_REG_SIZE		(0x108)	/* 0xf0f22800 - 0xf0f22908 */
#define PHY_RXTD_REG_START		(PHY_BASE_START + 0x3000)
#define PHY_RXTD_REG_SIZE		(0x204)	/* 0xf0f23000 - 0xf0f23204 */
#define PHY_TRX_MID_REG_START	(PHY_BASE_START + 0x3800)
#define PHY_TRX_MID_REG_SIZE	(0x054)	/* 0xf0f23800 - 0xf0f23854 */
#define PHY_RFI_REG_START		(PHY_BASE_START + 0x8000)
#define PHY_RFI_REG_SIZE		(0x7cc) /* 0xf0f28000 - 0xf0f287cc */
#define PHY_RX_LUT_REG_START	(PHY_BASE_START + 0x8000 + 0x800)
#define PHY_RX_LUT_REG_SIZE		(0x800)	/* 0xf0f28800 - 0xf0f29000 */
#define PHY_TX_LUT_REG_START	(PHY_BASE_START + 0x8000 + 0x1000)
#define PHY_TX_LUT_REG_SIZE		(0xc00)	/* 0xf0f29000 - 0xf0f29c00 */

#endif /* VERIFY_WLAN_PHY_REGS */

#define IPv4BUF    ((const struct ip_hdr *)mtodo(m, ETHER_HDR_LEN))
#define IPv6BUF    ((const struct ip6_hdr *)mtodo(m, ETHER_HDR_LEN))

/* ETHER HDR 14 + IPv6_HDR 40 */
#define IPv6_HDRLEN 54
#define UDPIPv4BUF(iphdr) ((const struct udp_hdr *)mtodo(m, (ETHER_HDR_LEN + (iphdr))))
#define UDPIPv6BUF ((const struct udp_hdr *)mtodo(m, IPv6_HDRLEN))
#define TCPIPv4BUF(iphdr) ((const struct tcp_hdr *)mtodo(m, (ETHER_HDR_LEN + (iphdr))))
#define TCPv4BUF(iphdr) ((const uint8_t *)mtodo(m, (ETHER_HDR_LEN + (iphdr) + TCP_HLEN)))

#define DHCPC_SERVER_PORT       67
#define DHCPC_CLIENT_PORT       68

#ifdef CONFIG_SCM2010_DHCPC_RECV_TIMEOUT_MS
#define DHCP_RECV_TIMEOUT_MS (CONFIG_SCM2010_DHCPC_RECV_TIMEOUT_MS)
#else
#define DHCP_RECV_TIMEOUT_MS 3000
#endif

#define DEV_OUR_SUBNET_MASK 0xffffff00 /* Fix me lator */
#define TCP_ACK_PSH_FLAG (TCP_ACK | TCP_PSH)
#define TCP_ACK_RST_FLAG (TCP_ACK | TCP_RST)
#define TCP_ACK_FIN_FLAG (TCP_ACK | TCP_FIN)


#define swap_ipaddr(x) \
			((((x) & 0xff000000) >> 24) | \
			 (((x) & 0x00ff0000) >>  8) | \
			 (((x) & 0x0000ff00) <<  8) | \
			 (((x) & 0x000000ff) << 24))

#define swap_portnum(x) \
			((((x) & 0xff00) >> 8) | \
			 (((x) & 0x00ff) << 8))


#define ETHER_IS_FILTER_MC(addr) ((addr)[0] == 0x01 && (addr)[1] == 0x00 && (addr)[2] == 0x5e)
		/* ARP Request or ARP Reply */
#define ETHER_IS_ARP_REQ_OR_REP(temp) (temp[1] == 0x01 && temp[2] == 0x08 && (temp[7] == 1 || temp[7] == 2))
		/* Multicast leave message */
#define ETHER_IS_MULTICAST_LEAVE(x)	(x == 0x020000e0)
#define ETHER_IS_MULTICAST_ADDR(x) 	((x & 0x000000F0) == 0x000000E0)

		/* get the sender ip address of ARP header */
#define getulongip(temp) (((temp[14] << 24) & 0xff000000) | ((temp[15] << 16) & 0x00ff0000) | \
			 ((temp[16] << 8) & 0x0000ff00) | (temp[17] & 0x000000ff))

#define TCPUDP_CHKSUM_SWAP_BYTES(w) (((w) & 0xff) << 8) | (((w) & 0xff00) >> 8)

#define IS_SCM2020_MAC_TX_IDLE(sc, txq)\
	(bfget_m(mac_readl(sc->dev, REG_TX_QUEN_STATE(txq->hwqueue)), TX_QUEN_STATE0, TX_QUEUE_STATE) != 3)


#define mbuf_datasize(m) ({ int __sz; __sz = (m->m_flags & M_EXT) ? 	\
		m->m_ext.ext_size : MHLEN; __sz; })
#define mbuf_databuf(m) ({ char *__buf; __buf = (m->m_flags & M_EXT) ? 	\
		m->m_ext.ext_buf : m->m_pktdat; __buf; })

#define is_gtk(k)  							\
	(k->wk_macaddr == NULL 						\
	|| IEEE80211_ADDR_EQ(k->wk_macaddr, "\xff\xff\xff\xff\xff\xff")	\
	|| k->wk_flags & IEEE80211_KEY_GROUP)

/**************************************************************************
 * Type Definitions
 **************************************************************************/
struct wlan_restore_counter {
	u64 fullwakeup;
	u64 wakeupsrc_rtc;
	u64 wakeupsrc_ext;
	u64 wakeup_reasons[FULL_BOOT_REASON_MAX];
	u64 wakeup_types_wifi[WIFI_BOOT_TYPE_MAX][WIFI_BOOT_SUBTYPE_MAX];
};

struct wlan_restore_ctx {
	struct ieee80211vap *vap;
	struct sc_softc *sc;
	struct device *dtim_timer;	/* dtim pm timer instead OS timer */
#ifdef CONFIG_WATCHER_FILTER_TIMESTAMP
	struct device *rtc_dev;	/* device for getting rtc time */
#endif
	struct systimer wifi_pm_timer;
	u8  suspended;
#ifdef CONFIG_PS_PM_VAP_SUPPORT
	u32 reg_vif1_uora_cfg1;
	u32 reg_vif1_cfg1;
	u32 reg_vif1_cfg0;
	u32 reg_vif1_cfg_info;
	u32 reg_vif1_rx_filter0;
	u32 reg_vif1_rx_filter1;
	u32 reg_vif1_bfm0;
	u32 reg_vif1_bfm1;
	u32 reg_vif1_min_mpdu_spacing;
	u32 reg_vif1_macaddr_l;
	u32 reg_vif1_macaddr_h;
#endif
	struct wlan_restore_counter counter;
};

struct arp_cache_frame {
	/* ethernet hdr */
	struct ether_header eh;

	/* arp header & data */
	struct waon_arpheader arp_hdr;
} __packed;

/* TCP Pseudo Header Structure */
struct tcpudp_pseudo_hdr {
	u32 src_addr;
	u32 	dst_addr;
	u8		reserved;
	u8		protocol;
	u16 	length;
};

/* TCP Hole punch packet */
struct watcher_tcphole_frame {
	/* 802.11 header */
	struct ieee80211_frame mac_hdr;

	/* llc snap header */
	u8 	hdr_llc_snap[6];
	u16	hdr_ether_type;

	/* IP header & TCP Header data */
	struct ip_hdr ip_hdr;
	struct tcp_hdr tcp_hdr;
} __packed;

/* UDP Hole punch packet */
struct watcher_udphole_frame {
	/* 802.11 header */
	struct ieee80211_frame mac_hdr;

	/* llc snap header */
	u8 	hdr_llc_snap[6];
	u16	hdr_ether_type;

	/* IP header & UDP Header data */
	struct ip_hdr ip_hdr;
	struct udp_hdr udp_hdr;
} __packed;

#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
/* MQTT keep PING REQ packet */
struct watcher_mqttping_frame {
	/* llc snap header */
	u8 	hdr_llc_snap[6];
	u16	hdr_ether_type;

	/* IP header & TCP Header data */
	struct ip_hdr ip_hdr;
	struct tcp_hdr tcp_hdr;

	/* MQTT PING Req packet */
	u8 mqtt_pkt_type;
	u8 mqtt_remain_len;
} __packed;
#endif

#if VERIFY_WLAN_PHY_REGS == 1

struct phy_reg_seg {
	u32 addr;
	u32 size; /* size of the segment in bytes */
};
#endif /* VERIFY_WLAN_PHY_REGS */

/**************************************************************************
 * Global Variables
 **************************************************************************/

static struct wlan_pm_ops scm2020_wlan_pm_ops;

const u8 tx_pkt_ctx_idx[] =
	{
		WAON_TX_PKT_PPOL_IDX,
		WAON_TX_PKT_NULL_IDX,
		WAON_TX_PKT_GARP_IDX,
#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
		WAON_TX_PKT_MQTT_IDX,
		WAON_TX_PKT_MQTT_ACK_IDX,
#endif
#ifdef WATCHER_HP
		WAON_TX_PKT_UDPA_IDX,
		WAON_TX_PKT_TCPA_IDX
#endif
	};

extern const u8  mcu_bin[];
extern const u32 mcu_bin_size;


/* MAC address matching the 802.2 LLC header */
static const u_char llc_hdr_mac[ETHER_ADDR_LEN] =
{ LLC_SNAP_LSAP, LLC_SNAP_LSAP, LLC_UI, 0, 0, 0 };

/* ARP Cache restore timer from Watcher */
struct callout	arp_cache_timer;

/* WATCHER Automatically running flag */
__dram__ static bool watcher_save_info_flag = false;

/* Watcher Play flag, Initialize starting is false */
__dram__ static bool scm2020_watcher_onoff_flag = false;

/* Mac/Phy REG restoring Context */
__dram__ static struct wlan_restore_ctx *wlan_pm_ctx = NULL;

static u32 watcher_first_delay_ms_time = 0;

static const u32 waon_hw_bak_addr[WAON_HW_BAK_SIZE] = {
	/*
	 * MAC Registers
	 */
    MAC_BASE_START + MAC_REG_OFFSET + REG_DEV_CFG,
    MAC_BASE_START + MAC_REG_OFFSET + REG_DEV_OPT1,
    MAC_BASE_START + MAC_REG_OFFSET + REG_DEV_OPT2,
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_UORA_CFG1(0),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_CFG1(0),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_CFG0(0),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_CFG_INFO(0),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_AID(0),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_BFM0(0),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_BFM1(0),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_MIN_MPDU_SPACING(0),
    MAC_BASE_START + MAC_REG_OFFSET + REG_TX_TO_CFG,
    MAC_BASE_START + MAC_REG_OFFSET + REG_TX_TO_11B_CFG,
    MAC_BASE_START + MAC_REG_OFFSET + REG_RX_TO_CFG,
    MAC_BASE_START + MAC_REG_OFFSET + REG_RX_TO_11B_CFG,
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_RX_FILTER0(0),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_RX_FILTER1(0),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_QNULL_CTRL_AC(0, 0),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_QNULL_CTRL_AC(0, 1),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_QNULL_CTRL_AC(0, 2),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_QNULL_CTRL_AC(0, 3),
    MAC_BASE_START + MAC_REG_OFFSET + REG_VIF_TX_PWR(0),
	/* Will be modified by MAC FW */
    MAC_BASE_START + MAC_REG_OFFSET + REG_DEV_FORCE_CLR,
    MAC_BASE_START + MAC_RXE_OFFSET + RXE_BD_SHM_ADDR,
    MAC_BASE_START + MAC_RXE_OFFSET + RXE_BA_FIFO_ADDR,
    MAC_BASE_START + MAC_RXE_OFFSET + RXE_VEC_FIFO_ADDR,
    MAC_BASE_START + MAC_RXE_OFFSET + RXE_MPDU_FIFO_CFG,
    MAC_BASE_START + MAC_RXE_OFFSET + RXE_MPDU_FIFO_ADDR,
	/*
	 * PHY Registers
	 */
    PHY_BASE_START + PHY_MDM_OFFSET + FW_HE_CONFIG(1, 0),
    PHY_BASE_START + PHY_MDM_OFFSET + FW_HE_CONFIG(2, 0),
    PHY_BASE_START + PHY_MDM_OFFSET + PHY_11B_CS_PARAM,
    PHY_BASE_START + PHY_MDM_OFFSET + PHY_CS_CONFIG_1,
    PHY_BASE_START + PHY_MDM_OFFSET + PHY_CS_CONFIG_2,
    PHY_BASE_START + PHY_MDM_OFFSET + PHY_FDSCL_CFG2,
    PHY_BASE_START + PHY_MDM_OFFSET + PHY_FDSCL_CFG3,
    PHY_BASE_START + PHY_MDM_OFFSET + PHY_FDSCL_CFG4,
    PHY_BASE_START + PHY_RFI_OFFSET + FW_TXLUT_MANSET_TXGAIN_OUT,
    PHY_BASE_START + PHY_RFI_OFFSET + FW_RFI_TX_IQ_MIS_COMP,
    PHY_BASE_START + PHY_RFI_OFFSET + FW_RFI_TX_LO_LEAK_MAN,
    PHY_BASE_START + PHY_RFI_OFFSET + FW_RFI_TX_LO_LEAK_PARA,
    PHY_BASE_START + PHY_RFI_OFFSET + FW_RFI_RX_IQ_MIS_COMP,
    PHY_BASE_START + PHY_RFI_OFFSET + FW_RFI_RXDC_HPF_CFG,
    0, /* end delimiter */
};

#if VERIFY_WLAN_MAC_REGS

static volatile u32 scm2020_wlan_mac_regs_comp[MAC_REG_SIZE];

#endif /* VERIFY_WLAN_MAC_REGS */

#if VERIFY_WLAN_PHY_REGS

struct phy_reg_seg scm2020_phy_reg[PHY_REG_SEG_NUM] = {
	[PHY_REG_SEG_BASIC_1] =
	{ PHY_BASIC_1_REG_START, PHY_BASIC_1_REG_SIZE },
	[PHY_REG_SEG_BASIC_2] =
	{ PHY_BASIC_2_REG_START, PHY_BASIC_2_REG_SIZE },
	[PHY_REG_SEG_TXBE] = { PHY_TXBE_REG_START, PHY_TXBE_REG_SIZE },
	[PHY_REG_SEG_RXBE] = { PHY_RXBE_REG_START, PHY_RXBE_REG_SIZE },
	[PHY_REG_SEG_TRX_FE] = { PHY_TRX_FE_REG_START, PHY_TRX_FE_REG_SIZE },
	[PHY_REG_SEG_11B] = { PHY_11B_REG_START, PHY_11B_REG_SIZE },
	[PHY_REG_SEG_RXTD] = { PHY_RXTD_REG_START, PHY_RXTD_REG_SIZE },
	[PHY_REG_SEG_TRX_MID] =
	{ PHY_TRX_MID_REG_START, PHY_TRX_MID_REG_SIZE },
	[PHY_REG_SEG_RFI] = { PHY_RFI_REG_START, PHY_RFI_REG_SIZE },
	[PHY_REG_SEG_RX_LUT] = { PHY_RX_LUT_REG_START, PHY_RX_LUT_REG_SIZE },
	[PHY_REG_SEG_TX_LUT] = { PHY_TX_LUT_REG_START, PHY_TX_LUT_REG_SIZE },
};

static volatile u32 *scm2020_wlan_phy_regs_comp[PHY_REG_SEG_NUM];

#endif /* VERIFY_WLAN_PHY_REGS */

/**************************************************************************
 * Function Prototypes
 **************************************************************************/

static void scm2020_pm_init(struct sc_softc *sc);

/* Change PIT Timer as SYSTIMER of PM */
static void scm2020_dtim_awake(void *);
static void scm2020_ps_pkt_info_init(void);
static void scm2020_ps_update_wmm(struct ieee80211vap *vap);
static void scm2020_ps_save_pkt(struct ieee80211vap *vap);
static void scm2020_ps_save_sec_key(struct ieee80211vap *vap);
#ifndef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
static void scm2020_ps_null_save_pkt(struct ieee80211vap *vap);
#endif

/* XXX: ugly, but don't want to create redundancy! */
extern void (*rx_desc_attach_buffer)(struct sc_softc * sc, struct sc_rx_desc * desc, struct mbuf * m);
extern struct mbuf *(*rx_desc_detach_buffer) (struct sc_softc * sc, struct sc_rx_desc * desc);
extern int construct_mbuf(struct mbuf *memoryBuffer, short type, int flags);
extern void memp_reset_region(memp_t type, u16 reg_num);
extern void scm2020_pm_fill_hw_tx_hdr(u8 * data, u32 len, struct ieee80211_node *ni, struct ieee80211_key *key);


/**************************************************************************
 * Function Implementation
 **************************************************************************/

#define IS_FILTER_MC(x) (x == FILTER_MC)

/* get tcp & udp & mc accept filter size*/
static u8 scm2020_ps_get_filter_size(int filter)
{
	if (IS_FILTER_MC(filter))
		return CONFIG_WAON_ACCEPT_MAX_MC;

	return CONFIG_WAON_ACCEPT_MAX_FILTER;
}

static u32 scm2020_ps_get_filter(int filter, int index)
{
	if(IS_FILTER_MC(filter))
		return waon_get_accept_mc_addr(filter, index);

	return waon_get_accept_port(filter, index);
}

static void scm2020_ps_set_tot_reg(int filter, u8 num)
{
	waon_set_tot_reg(filter, num);
}


static void scm2020_ps_set_filter(int filter, int n, u32 accept_filter)
{

	if (IS_FILTER_MC(filter))
		waon_set_accept_mc_addr(filter, n, accept_filter);
	else
		waon_set_accept_port(filter, n, accept_filter);
}

#ifdef CONFIG_WATCHER_FILTER_TIMESTAMP

static u32 scm2020_ps_get_rtc_time(int filter, int index)
{
	return waon_get_rtc_time(filter, index);
}

static u32 scm2020_ps_get_32khz_count(void)
{
	uint32_t cur_rtc_value = 0;

	if (wlan_pm_ctx->rtc_dev != NULL)
		rtc_get_32khz_count(wlan_pm_ctx->rtc_dev, &cur_rtc_value);

	return cur_rtc_value;
}


static void scm2020_ps_set_rtc_time(int filter, int index, u32 rtc_time)
{
	waon_set_rtc_time(filter, index, rtc_time);
}

static int scm2020_ps_replace_filter(int filter)
{
	u32 oldest_timestamp = 0;
	u32 cur_timestamp = 0;
	int oldest_index = 0;
	int max_index = scm2020_ps_get_filter_size(filter);

	/* search oldest filter by comparing timestamp */
	for (int search_index = 0; search_index < max_index; search_index++) {
		cur_timestamp = scm2020_ps_get_rtc_time(filter, search_index);
		if (!oldest_timestamp) {
			oldest_timestamp = cur_timestamp;
			oldest_index = search_index;
		} else if (oldest_timestamp > cur_timestamp) {
			oldest_timestamp = cur_timestamp;
			oldest_index = search_index;
		}
	}
	/* delete oldest multicast filter */
	scm2020_ps_set_filter(filter, oldest_index, 0);
	pm_wifi_log("Filter %d is full and search oldest filter(%d)\n", filter, oldest_index);

	return oldest_index;
}
#else
static u32 scm2020_ps_get_32khz_count(void) { return 0; }
static void scm2020_ps_set_rtc_time(int filter, int index, u32 rtc_time) { }
static int scm2020_ps_replace_filter(int filter)
{
	pm_wifi_log("Reg filter %d is full\n", filter);
	/* Fix me lator, if reg port is full, how about delete first port */
	return -1;
}
#endif

__maybe_unused
__ilm_wlan_tx__ static bool scm2020_ps_chk_filter(int filter , unsigned long filternumber)
{
	u8 index = 0;

	unsigned short reg_filter;

	u8 max_index = scm2020_ps_get_filter_size(filter);

	for (index = 0; index < max_index; index++) {
		reg_filter = scm2020_ps_get_filter(filter, index);

		if (reg_filter == filternumber) {
			/* matching port filter */
			return true;
		}
	}
	return false;
}

/* reg tcp & udp & mc filter */
static void scm2020_ps_reg_filter(int filter , unsigned long filternumber)
{
	u8 index;
	u8 max_index = scm2020_ps_get_filter_size(filter);

	unsigned long reg_filter;

	u32 cur_rtc_value = scm2020_ps_get_32khz_count();

	for (index = 0; index < max_index; index++) {
		reg_filter = scm2020_ps_get_filter(filter, index);
		if (reg_filter == 0)
			break;
		if (reg_filter == filternumber) {
			scm2020_ps_set_rtc_time(filter, index, cur_rtc_value);
			/* port already registered */
			return;
		}
	}

	if (index == max_index) {
		index = scm2020_ps_replace_filter(filter);

		if (index < 0)
			return;
	}

	pm_wifi_log("Register Filter[%d] (%d)\n", index, filternumber);

	/* filter set */
	scm2020_ps_set_filter(filter, index, filternumber);
	scm2020_ps_set_rtc_time(filter, index, cur_rtc_value);

	return;
}

/* unreg tcp & udp & mc filter */
__maybe_unused
static void scm2020_ps_clr_filter(int filter , unsigned long filternumber)
{
	unsigned long reg_filter;

	u8 max_index = scm2020_ps_get_filter_size(filter);

	for (u8 index = 0; index < max_index; index++) {
		reg_filter = scm2020_ps_get_filter(filter, index);
		if (reg_filter == filternumber) {
			/* clear filter */
			pm_wifi_log("Clear Filter[%d] (%d)\n", index, filternumber);
			scm2020_ps_set_filter(filter, index, 0);
			return;
		}
	}

	pm_wifi_log("Match Filter is not exist (%d)\n", filternumber);
	return;
}

/* reset tcp & udp & mc filter */
static void scm2020_ps_reset_filter(int filter)
{
	u8 max_index = scm2020_ps_get_filter_size(filter);

	for (u8 index = 0; index < max_index; index++) {
		scm2020_ps_set_filter(filter, index, 0);
	}

	scm2020_ps_set_tot_reg(filter, 0);

	return;
}

/* Init & Clear Multicast filter */
static void scm2020_ps_filter_init(void)
{
	scm2020_ps_reset_filter(FILTER_MC);
	scm2020_ps_reset_filter(FILTER_TCP);
	scm2020_ps_reset_filter(FILTER_UDP);
}

/* Init shared ip address with Watcher */
static void scm2020_ps_set_net_ip(u32 addr)
{
	waon_set_net_ipaddr(addr);
}

u32 scm2020_ps_get_net_ip(void)
{
	return waon_get_net_ipaddr();
}

static bool scm2020_watcher_make_txpkt(void)
{
	struct sc_softc *sc;
	struct ieee80211vap *vap;
	struct ieee80211vap *c_vap = NULL;

	sc = wlansc(0);

	/* Check WiFi mode & connection status */
	TAILQ_FOREACH(vap, &sc->ic->ic_vaps, iv_next) {
		if (vap->iv_opmode == IEEE80211_M_STA && vap->iv_state >= IEEE80211_S_RUN) {
			c_vap = vap;
			break;
		}
	}
	if (c_vap == NULL) {
		printf("[PM WiFi] vap is null!!!\n");
		return false;
	}

	scm2020_ps_update_wmm(vap);

	scm2020_ps_save_pkt(c_vap);

	scm2020_ps_save_sec_key(vap);
#ifndef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
	scm2020_ps_null_save_pkt(c_vap);
#endif

#ifdef CONFIG_PS_FILTER_SIMUL_TEST /* multicast test */
	scm2020_ps_reg_filter(FILTER_MC, 0xE0000181);

	/* udp sample test */
	scm2020_ps_reg_filter(FILTER_UDP, 4567);
#endif

	/* clear the tx of Watcher */
	waon_clear_tx_time();
	watcher_save_info_flag = false;

	return true;
}

__ilm_wlan__ static int scm2020_tx_checkbusy(struct ieee80211vap *vap, struct sc_softc *sc)
{
	/* check if all AC BUCKET is idle. */
	int ac = 0;

	/* include HWQ_HI */
	for (ac = 0; ac < WME_NUM_AC + 1; ac++) {
		struct sc_tx_queue *txq = scm2020_get_txq(vap, ac);

		if (ifq_empty(&txq->sched) && IS_SCM2020_MAC_TX_IDLE(sc, txq)) {
			/* check additional queue */
			if (ifq_empty(&txq->retry) && ifq_empty(&txq->queue)
					&& !ifq_suspend(txq)) {
				continue;
			}
		}
		return -EBUSY;
	}

	return 0;
}

__ilm_wlan__ static bool scm2020_ps_check_pm_master(void)
{
	/* Except WiFi , other PM Modules is ready */
	if (pm_status() & ~(1 << PM_DEVICE_WIFI))
		return false;
	else
		return true;
}

static void scm2020_ps_mac_chk_int(struct sc_softc *sc)
{
	struct device *dev = sc->dev;

	/* RXE interrupt should be 0 while MAC FW not running */
	assert(!readl(dev->base[0] + MAC_RXE_OFFSET + 0x00));

	/* DMA interrupt should be 0 while MAC FW not running */
	assert(!readl(dev->base[0] + MAC_DMA_OFFSET + 0x00));
}

/**
 * Prepare RX Buffer Desc for sleep
 * do without rx_desc_detach_buffer() & m_free()
 */

static void
scm2020_rx_mbuf_restore(struct sc_softc *sc, struct sc_rx_desc *desc,
					bool reset)
{
	struct ieee80211com *ic = sc->ic;
	int hdrm;
	int size;
	volatile struct rxdesc *rxd;
	struct mbuf *m;

	m = desc->m;

	m->m_next = NULL;
	m->m_nextpkt = NULL;
	m->m_len = 0;
	m->m_flags = M_PKTHDR;
	m->m_type = MT_DATA;

	m->m_data = m->m_pktdat;
	memset(&m->m_pkthdr, 0, sizeof(m->m_pkthdr));
	SLIST_INIT(&m->m_pkthdr.tags);

	m->m_flags &= ~M_TEST_EXT;
	m->m_pkthdr.rcvif = NULL;

	hdrm = roundup(ic->ic_headroom, sizeof(u32));
	size = mbuf_datasize(m) - hdrm;
	m->m_data = mbuf_databuf(m) + hdrm;
	m->m_len = size;


	rxd = &sc->shmem->rx[desc->index];
	rxd->pbuffer_l = mtod(m, u32);
	rxd->pbuffer_h = 0;
	rxd->size = m->m_len;

	rxd->len = 0;
	rxd->frag = 0;

	//printk("mbuf is resotoring %p, len %d\n", m, m->m_len);

	dmb();
}

static struct mbuf *scm2020_rx_desc_prepare(struct sc_softc *sc, bool reset, bool wakeup)
{
	struct sc_rx_desc *desc;
	volatile struct rxdesc *rxd;
	struct mbuf *m = NULL;
	int i;
	u32 v;

	/* After wakeup, case of reset(clearing READY of RX desc) */
	if (reset) {
		init_ptr1(sc, WRITE, 0);
		init_ptr0(sc, READY, 0);
	}

	for (i = 0; i < sc->rx.n_rx_desc; i++) {
		desc = &sc->rx.desc[i];
		if (desc->m) {

			/* In Hibernation mode, after wakeup, SRAM is not maintained, so need workaround
			 */
			if (wakeup && pm_wakeup_is_himode()) {

				/* If packet is exist and reset from Watcher, rx mbuf is not restored */
				if (reset) {
					scm2020_rx_mbuf_restore(sc, desc, reset);
					advance_ptr(sc, READY);
				}
				continue;
			} else {
				if (reset) {
					advance_ptr(sc, READY);
				}
				continue;
			}
		}
		desc->index = i;
		rxd = &sc->shmem->rx[desc->index];
		m = m_gethdr(M_NOWAIT, MT_DATA);
		if (m) {
			rx_desc_attach_buffer(sc, desc, m);
		} else {
			assert(desc->index > 0);
			/* Void the corresponding entry in rxd
			 * so that HMAC won't use it.
			 */
			rxd->pbuffer_l = 0;
			rxd->pbuffer_h = 0;
			rxd->size = 0;
			rxd->len = 0;
			rxd->frag = 0;
			dmb();
		}
	}

	v = mac_readl(sc->dev, REG_RX_BUF_CFG);
	bfmod_m(v, RX_BUF_CFG, BUF_NUM, LOG2(sc->rx.n_rx_desc));
	mac_writel(v, sc->dev, REG_RX_BUF_CFG);

	if (reset) {
		sc->rx.head = &sc->rx.desc[0];
		init_ptr0(sc, READ, 0);
	} else {
		/* Restore READ pointer by what watcher saved. */
		init_ptr0(sc, READ, waon_p(ctx)->rx_rd_ptr);
		sc->rx.head = &sc->rx.desc[RX_PTR2IDX(sc, read_ptr(sc, READ))];
		pm_wifi_log("rx descriptor restores: read=0x%x, ready=0x%x, write=0x%x, map=0x%08x:0x%08x\n",
			read_ptr(sc, READ), read_ptr(sc, READY), read_ptr(sc, WRITE),
			sc->rx.map[0], sc->rx.map[1]);
	}

	if (!wakeup) {
		desc = &sc->rx.desc[sc->rx.n_rx_desc - 1];
		return desc->m;
	}

	return NULL;
}

/**
 * Reserves space of 'size' bytes for PM to utilize as its back-up storage
 * by allocating contiguous mbufs from MBUF_CACHE memory pool.
 */
__maybe_unused
static u8 *scm2020_reserve_backup_storage(struct sc_softc *sc, u32 size)
{
	struct ieee80211com *ic = sc->ic;
	memp_t type = MEMP_MBUF_CACHE;
	u16 blksz, nblk;
	u16 remained;
	u32 reserved;
	struct mbuf *m = NULL;
	struct mbuf *last_m = NULL;
	u8 *wanted;

	blksz = memp_size(type);
	nblk = memp_num(type);

	/* Check sanity by assertion.
	 * We may not check this based on build-time configuration
	 * because some other low power modes may still be available.
	 */

	assert(nblk * blksz > size);

	/* XXX: We will just give up if any mbuf is alive outside of rx descriptor
	 * simply because reclaiming all of them will cost too much.
	 */
	if (memp_available(type) + rx_desc_num_filled_slot(sc) != nblk) {
		pm_wifi_log
		("mbufs are alive outside of rx descriptor. We give up.\n");
		return NULL;
	}

	/*
	 * Disable RX process and DMA as well
	 * to protect rx descriptor.
	 */

	scm2020_hmac_enable(sc, false);

	waon_p(ctx)->n_rx_desc_org = sc->rx.n_rx_desc;
	waon_p(ctx)->ic_headroom = roundup(ic->ic_headroom, sizeof(u32));
	waon_p(ctx)->mbufsz = sizeof(struct mbuf);
	waon_p(ctx)->mbufdsz = MHLEN;

	/* Empty the rx descriptor.
	 */
	scm2020_rx_desc_deinit(sc);

	/* (Re-)Initialize memory pool so that all mbufs will be
	 * contiguous as they will be allocated.
	 */

	memp_reset(type);

	/* Allocate, or reserve, mbufs from the pool until we satisfy our need.
	 * We will take the address of the last mbuf as a start of our back-up storage
	 * because we know allocated elements from an initialized memp pool will be
	 * located at contiguously decreasing addresses.
	 */
	for (reserved = 0; reserved < size; reserved += blksz) {
		m = memp_malloc(type);
		assert(m);
	}

	/*
	 * This last one corresponds to the start of the whole reserved space
	 */

	wanted = (u8 *) m;

	/*
	 * Calculate a new size of a rx descriptor ring based on what we are left with.
	 */

	remained = nblk - (reserved / blksz);
	sc->rx.n_rx_desc = 1 << LOG2_FL(remained);

	/* Fill the rx descriptor with remaining mbufs.
	 */

	last_m = scm2020_rx_desc_prepare(sc, true, false);

	waon_p(ctx)->last_mbuf = (u32) last_m;
	waon_p(ctx)->n_rx_desc = sc->rx.n_rx_desc;

	pm_wifi_log("mbuf reserve address %p\n", waon_p(ctx)->last_mbuf);


	/*
	 * XXX: Don't need to re-enable Rx operation and DMA?
	 */

	return wanted;
}

/**
 * Reclaim the PM back-up storage and reinstall all the reclaimed mbufs back to
 * the rx descriptor ring.
 */
static void scm2020_reclaim_backup_storage(struct sc_softc *sc, void *start, s32 size)
{
	memp_t type = MEMP_MBUF_CACHE;
	u16 blksz, nblk;
	u8 *p;

	if (!start)
		return;

	blksz = memp_size(type);
	nblk = memp_num(type);

	/*
	 *XXX: Be cautious about the order of reclaiming memory regions.
	 *     1 -> 2 -> 3
	 */

	/*
	 * 1. Reset variable remainder at top.
	 */
	if (pm_wakeup_is_himode()) {        /* Up to DS, SRAM will be retained. Don't waste time. */
		int n = nblk - waon_p(ctx)->n_rx_desc - pm_reserved_buf_get_size() / blksz;
		assert(n >= 0);
		memp_reset_region(MEMP_MBUF_CACHE, n);
		memp_reset(MEMP_MBUF_CHUNK);
#ifdef CONFIG_MEMP_NUM_MBUF_DYNA_EXT
		memp_reset(MEMP_MBUF_EXT_NODE);
#endif
	}

	/*
	 * 2. Return all the reserved mbufs to the memory pool.
	 */

	for (p = start; size > 0; size -= blksz, p += blksz) {
		memp_free(type, p);
	}

	/*
	 * Revert the size of a rx descriptor ring.
	 */

	sc->rx.n_rx_desc = waon_p(ctx)->n_rx_desc_org;

	/*
	 * Fill the rx descriptor ring to its full capacity with active descriptors intact.
	 */

	pm_wifi_log("pm_resume buf address %p rx_desc cnt %d\n", start, sc->rx.n_rx_desc);

	/*
	 * 3. Set up a new Rx descriptor.
	 */
	scm2020_rx_desc_prepare(sc, waon_p(ctx)->wakeup_by_rx != 1, true);
	waon_p(ctx)->wakeup_by_rx = 0;
}

__ilm_wlan__ static inline void scm2020_phy_init_restore(struct sc_softc *sc)
{
	/* RFIF control signal assignment */
#define RFI_CTRLSIG_SHDN	RFI_CTRLSIG_CFG(0)
#define RFI_CTRLSIG_2G_PA_EN	RFI_CTRLSIG_CFG(1)
#define RFI_CTRLSIG_5G_PA_EN	RFI_CTRLSIG_CFG(2)
#define RFI_CTRLSIG_EXT_SW_RX	RFI_CTRLSIG_CFG(3)
#define RFI_CTRLSIG_EXT_SW_TX	RFI_CTRLSIG_CFG(4)
#define RFI_CTRLSIG_RF_RX_EN	RFI_CTRLSIG_CFG(5)
#define RFI_CTRLSIG_RF_TX_EN	RFI_CTRLSIG_CFG(6)
#define RFI_CTRLSIG_ADC		RFI_CTRLSIG_CFG(7)
#define RFI_CTRLSIG_DAC		RFI_CTRLSIG_CFG(8)
#define RFI_CTRLSIG_RX_EN	RFI_CTRLSIG_CFG(12)
#define RFI_CTRLSIG_TX_EN	RFI_CTRLSIG_CFG(13)
#define RFI_CTRLSIG_RX_CLK	RFI_CTRLSIG_CFG(14)
#define RFI_CTRLSIG_TX_CLK	RFI_CTRLSIG_CFG(15)

	/* PHY Control mode */

	phy_writel_mimo(0x00000000, sc, PHY_RFITF, FW_RFI_EN);


	/* SHDN: - ctrl[0], always on */
	phy_writel_mimo(RFCTRL(1, HIGH, ALWAYS, TXEN, 0, 0), sc, PHY_RFITF, RFI_CTRLSIG_SHDN);
	/* 2G PA En:1 - ctrl[1], during TXVALID */
	phy_writel_mimo(RFCTRL(1, HIGH, TXVALID, TXVALID, 0, 0), sc, PHY_RFITF, RFI_CTRLSIG_2G_PA_EN);
	/* 5G PA En - ctrl[2], always off */
	phy_writel_mimo(RFCTRL(1, HIGH, TXVALID, ALWAYS, 0, 0), sc, PHY_RFITF, RFI_CTRLSIG_5G_PA_EN);
	/* ExtSwitch Rx - ctrl[3], during RXEN */
	phy_writel_mimo(RFCTRL(1, HIGH, RXEN, RXEN, 0, 0), sc, PHY_RFITF, RFI_CTRLSIG_EXT_SW_RX);
	/* ExtSwitch Tx - ctrl[4], during TXEN */
	phy_writel_mimo(RFCTRL(1, HIGH, TXEN, TXEN, 0, 0), sc, PHY_RFITF, RFI_CTRLSIG_EXT_SW_TX);
	/* RF Rx Enable - ctrl[5], during RXEN */
	phy_writel_mimo(RFCTRL(1, HIGH, RXEN, RXEN, 0, 0), sc, PHY_RFITF, RFI_CTRLSIG_RF_RX_EN);
	/* RF Tx Enable - ctrl[6], during TXEN */
	phy_writel_mimo(RFCTRL(1, HIGH, TXEN, TXEN, 0, 0), sc, PHY_RFITF, RFI_CTRLSIG_RF_TX_EN);
	/* ADC control - ctrl[7], always on */
	phy_writel_mimo(RFCTRL(1, HIGH, ALWAYS, RXEN, 0, 0), sc, PHY_RFITF, RFI_CTRLSIG_ADC);
	/* DAC control - ctrl[8], during TXEN */
	phy_writel_mimo(RFCTRL(1, HIGH, TXEN, TXEN, 1, 5), sc, PHY_RFITF, RFI_CTRLSIG_DAC);
	/* RX_EN (digital I/Q) - ctrl[12], during RXEN */
	phy_writel_mimo(RFCTRL(1, HIGH, RXEN, RXEN, 0, 0), sc, PHY_RFITF, RFI_CTRLSIG_RX_EN);
	/* TX_EN (digital I/Q) - ctrl[13], during TXEN */
	phy_writel_mimo(RFCTRL(1, HIGH, TXEN, TXEN, 0, 0), sc, PHY_RFITF, RFI_CTRLSIG_TX_EN);
	/* RX_CLK  - ctrl[14], always (in FPGA) */
	phy_writel_mimo(RFCTRL(1, HIGH, ALWAYS, TXEN, 0, 0), sc, PHY_RFITF, RFI_CTRLSIG_RX_CLK);
	/* TX_CLK  - ctrl[15], always on (in FGPA) */
	phy_writel_mimo(RFCTRL(1, HIGH, ALWAYS, TXEN, 0, 0), sc, PHY_RFITF, RFI_CTRLSIG_TX_CLK);

	/* Tx DAC interface ON */
	phy_writel_mimo(0x00000001, sc, PHY_RFITF, FW_RFI_TXDAC_INTF);

	/* Rx ADC interface ON */
	phy_writel_mimo(0x00000001, sc, PHY_RFITF, FW_RFI_RXADC_INTF);

	/* TX digital manual gain */
	phy_writel_mimo(0x000000A8, sc, PHY_RFITF, PHY_RFI_TX_DIG_GAIN_MAN);
	/* RX digital manual gain disable */
	phy_writel_mimo(0x00000000, sc, PHY_RFITF, PHY_RFI_RX_DIG_GAIN_MAN);
#if 0
	/* Rx HPF duration */
	phy_writel_mimo(0x00000808, sc, PHY_RFITF, FW_RXHPF_CFG1);
	/* Rx HPF BW selection */
	phy_writel_mimo(0x00000000, sc, PHY_RFITF, FW_RXHPF_CFG2);
#endif
	/* PHY power en */
	/* phy_writel_mimo(0x00000001, sc, PHY_MODEM, FW_PWR_EN); */
	/* AGC LUT index range */
	/* phy_writel_mimo(0x00005E5A, sc, PHY_MODEM, PHY_AGC_INDEX_CFG); */

	/* Power low TH change */
	phy_writel_mimo(0x001100FE, sc, PHY_MODEM, PHY_AGC_PWRMSR_CFG);
	/* Init power range */
	phy_writel_mimo(0x01A2012C, sc, PHY_MODEM, PHY_AGC_INITPWR_RANGE);
	/* Target power */
	phy_writel_mimo(0x015E01A2, sc, PHY_MODEM, PHY_AGC_TARGET_PWR);

	/* Gain adj wait time */
	phy_writel_mimo(0x00280020, sc, PHY_MODEM, PHY_AGC_WAIT_CNT2);
	/* Gain step control */
	phy_writel_mimo(0x18181818, sc, PHY_MODEM, PHY_AGC_GDOWN_CFG);

	/* AGC debug config default setting */
	phy_writel_mimo(0x00007F11, sc, PHY_MODEM, PHY_AGC_TEST_OPTION);

	/* Default value update for All version */
	/* Initial fine/coarse gain adj wait counter */
	phy_writel_mimo(0x00280020, sc, PHY_MODEM, PHY_AGC_WAIT_CNT1);
	/* SW reset */
	phy_writel_mimo(0x00000001, sc, PHY_MODEM, FW_PHY_SW_RESET);

	/* Temp Config */
	/* Don't Stop reg */
	phy_writel_mimo(0xFFFFFFFF, sc, PHY_MODEM, PHY_DONT_BE_STUCK);
	/* Turn off (GID, PAID) filtering */
	phy_writel_mimo(0x00000000, sc, PHY_MODEM, FW_VHT_CONFIG(0, 0));
	/* Turn off (GID, PAID) filtering */
	phy_writel_mimo(0x00000000, sc, PHY_MODEM, FW_VHT_CONFIG(0, 1));

	/* RSSI offset */
	phy_writel_mimo(0x000000F4, sc, PHY_MODEM, PHY_RSSI_OFFSET);
	/* SAT det config */
	phy_writel_mimo(0x000C0708, sc, PHY_RFITF, PHY_RFI_SATDET_CFG);
	/* HPF config when Init gain change */
	phy_writel_mimo(0x000C0000, sc, PHY_RFITF, FW_RXHPF_CFG2);
	/* HPF config when Init gain change */
	phy_writel_mimo(0x00280808, sc, PHY_RFITF, FW_RXHPF_CFG1);
	/* 11B min sen config */
	phy_writel_mimo(0x0003E802, sc, PHY_MODEM, PHY_11B_DET);
}

#if 0

#define DUMP_TYPE	0       /* 0: OFDM, 1:11b */

__ilm_wlan__ static inline void scm2020_phy_dump_stop(struct sc_softc *sc)
{
	phy_writel_mimo(0x00000000, sc, PHY_RFITF, PHY_RFI_MEMDP_RUN);
}

__ilm_wlan__ static inline void scm2020_phy_dump_start(struct sc_softc *sc)
{
	if (phy_readl(sc->dev, PHY_RFITF, PHY_RFI_MEMDP_RUN) == 0) {
		phy_writel_mimo(0x00000011, sc, PHY_RFITF, FW_RFI_LUT_INTFV2_CFG);
		phy_writel_mimo(0x00000000, sc, PHY_RFITF, PHY_RFI_MEMDP_RUN);
#if DUMP_TYPE == 0
		phy_writel_mimo(0x0000ffff, sc, PHY_MODEM, PHY_DUMP_SIG_SEL);
#else
		phy_writel_mimo(0x0000efff, sc, PHY_MODEM, PHY_DUMP_SIG_SEL);
#endif
		phy_writel_mimo(0x00000021, sc, PHY_RFITF, PHY_RFI_MEMDP_CFG0);
		phy_writel_mimo(0x00000010, sc, PHY_RFITF, PHY_RFI_MEMDP_CFG1);
		phy_writel_mimo(0x00000001, sc, PHY_RFITF, PHY_RFI_MEMDP_RUN);
	}
}

#endif

__ilm_wlan__ static inline void scm2020_phy_off(struct sc_softc *sc)
{
	u32 v, t = ktime();

	/* Wait until PHY Rx state becomes IDLE. */
	do {
		v = phy_readl(sc->dev, PHY_MODEM, PHY_MON_59);
		if (v == 2)
			break;
	}
	while (abs(ktime() - t) < ms_to_tick(2));

	phy_writel_mimo(0, sc, PHY_MODEM, FW_PWR_EN);
}

__ilm_wlan__ static void scm2020_ps_load_mac_fw(struct sc_softc *sc)
{
#ifdef CONFIG_USE_SYSDMA_FOR_MACFW_DN
#define DMAC1_SRC_ADDR_REG		(0xf1600048)
#define DMAC1_DST_ADDR_REG		(0xf1600050)
#define DMAC1_STATUS_REG		(0xf1600030)
#define DMAC1_CTRL_REG			(0xf1600040)
#define DMAC1_TRANS_SIZE_REG	(0xf1600044)

#define DMA_CTRL_DATA			(0x27480001)
#define DMA_COMPLETE			(0x10000)
#endif

	u32 v, t;
	int retry = MACFW_RETRY_CNT;

	/*
	 * | Low power mode  | WLAN HW | MAC IMEM RETENTION |
	 * |   Hibernation   |    PG   |          N         |
	 * | Deep Sleep(a|b) |    PG   |          Y         |
	 * |      Others     |    CG   |          Y         |
	 */

	v = mac_readl(sc->dev, REG_MCU_CFG);
	if (bfget_m(v, MCU_CFG, RSTB)) {
		/* Everything is up and running.
		 * It would be unnecessary and potentially dangerous
		 * to reset MAC MCU while the whole Rx chain is ACTIVE
		 * and MAC FW is already running.
		 */
		return;
	}

	scm2020_ps_mac_chk_int(sc);

reset_mcu:

	/* Assert a reset to MAC MCU. */
	bfclr_m(v, MCU_CFG, RSTB);
	mac_writel(v, sc->dev, REG_MCU_CFG);

	/* Clear DONE bit to see
	 * if MAC FW will have been reinitialized
	 * successfully.
	 */
	v = mac_readl(sc->dev, REG_MCU_STATUS);
	bfclr_m(v, MCU_STATUS, DONE);
	mac_writel(v, sc->dev, REG_MCU_STATUS);

	/* XXX: In case of wake-up from DS, i.e.,
	 *      pm_wakeup_from_hib() returns false,
	 *      HMAC has been reset but IMEM is intact.
	 *      There is no need to load HMAC FW in that case.
	 */

	if (pm_wakeup_is_himode()) {
		/* In HI, Download and run MAC binary */
		struct device *dev = sc->dev;
		u8 *imem;
		int i __maybe_unused;

		imem = dev->base[0] + MAC_IMEM_OFFSET;

		pm_wifi_log("In HI, Firmware download(%d).\n", mcu_bin_size);

#ifdef CONFIG_USE_SYSDMA_FOR_MACFW_DN
		writel((u32)mcu_bin, DMAC1_SRC_ADDR_REG);
		writel((u32)imem, DMAC1_DST_ADDR_REG);
		writel((u32)(mcu_bin_size / 4), DMAC1_TRANS_SIZE_REG);
		writel(DMA_CTRL_DATA, DMAC1_CTRL_REG);
		while (1) {
			if ((readl(DMAC1_CTRL_REG) & 0x1) == 0)
				break;
		}
#else
		for (i = 0; i < mcu_bin_size; i++)
			imem[i] = mcu_bin[i];
#endif
	} else {
		udelay(2);
	}

	/* Deassert a reset to run MCU.
	 */
	v = mac_readl(sc->dev, REG_MCU_CFG);
	bfmod_m(v, MCU_CFG, RSTB, 1);
	mac_writel(v, sc->dev, REG_MCU_CFG);

	/* Confirm MAC FW is running okay.
	 */
	t = ktime();
	do {
		v = mac_readl(sc->dev, REG_MCU_STATUS);
		if (bfget_m(v, MCU_STATUS, DONE))
			break;
	}
	while (abs(ktime() - t) < ms_to_tick(2));

	if (!bfget_m(v, MCU_STATUS, DONE)) {
		if (--retry > 0) {
			goto reset_mcu;
		} else {
			assert(bfget_m(v, MCU_STATUS, DONE));
		}
	} else {
		int num_try = MACFW_RETRY_CNT - retry + 1;

		if (num_try > 1)
			printk("MCU runs okay after %d %s.\n", num_try, num_try > 1 ? "tries" : "try");
	}
}

__ilm_wlan__ static void scm2020_pm_restore_init(struct sc_softc *sc, struct ieee80211vap *vap)
{
	struct sc_vap *tvap = vap2tvap(vap);
	int vif = tvap->index;
	u32 v;
	struct device *dev = sc->dev;
	int i;

	/* hmac Int mask */
	bfzero(v);
	mac_writel(v, dev, REG_INTR_ENABLE);

	scm2020_hmac_enable(sc, false);

	/* Restore HW context. */

	for (i = 0; i < WAON_HW_BAK_SIZE; i++) {
		if (waon_p(hw_ctx[i])->address)
			writel(waon_p(hw_ctx[i])->value, waon_p(hw_ctx[i])->address);
	}

	/* RX_CFG_PRO: promisc mode off */
	mac_writel(0, sc->dev, REG_RX_CFG);

#ifdef CONFIG_PS_PM_VAP_SUPPORT
	mac_writel(wlan_pm_ctx->reg_vif1_uora_cfg1, sc->dev, REG_VIF_UORA_CFG1(1));
	mac_writel(wlan_pm_ctx->reg_vif1_cfg1, sc->dev, REG_VIF_CFG1(1));
	mac_writel(wlan_pm_ctx->reg_vif1_cfg0, sc->dev, REG_VIF_CFG0(1));
	mac_writel(wlan_pm_ctx->reg_vif1_cfg_info, sc->dev, REG_VIF_CFG_INFO(1));
	mac_writel(wlan_pm_ctx->reg_vif1_rx_filter0, sc->dev, REG_VIF_RX_FILTER0(1));
	mac_writel(wlan_pm_ctx->reg_vif1_rx_filter1, sc->dev, REG_VIF_RX_FILTER1(1));
	mac_writel(wlan_pm_ctx->reg_vif1_bfm0, sc->dev, REG_VIF_BFM0(1));
	mac_writel(wlan_pm_ctx->reg_vif1_bfm1, sc->dev, REG_VIF_BFM1(1));
	mac_writel(wlan_pm_ctx->reg_vif1_min_mpdu_spacing, sc->dev, REG_VIF_MIN_MPDU_SPACING(1));
	/* VIF1 MAC Addr restore */
	mac_writel(wlan_pm_ctx->reg_vif1_macaddr_l, sc->dev, REG_VIF_MAC0_L(1));
	mac_writel(wlan_pm_ctx->reg_vif1_macaddr_h, sc->dev, REG_VIF_MAC0_H(1));
#endif

	scm2020_ps_load_mac_fw(sc);

	scm2020_phy_init(sc);

#if 0
	scm2020_phy_dump_start(sc);
#endif

	/* RX Buffer descriptor restore */
	scm2020_reclaim_backup_storage(sc, (void *)pm_reserved_buf_get_addr(), pm_reserved_buf_get_size());

	/* If not HI, restore the txq ptable */
	if (!pm_wakeup_is_himode() && pm_reserved_buf_get_addr()) {
		int i;
		struct sc_tx_queue *txq;

		for (i = 0; i < ARRAY_SIZE(sc->txq); i++) {
			txq = &sc->txq[i];
			txq->desc->ptable_l = (u32)txq->mtable;
			txq->desc->ptable_h = 0;
		}
	}

	/* 4. mac addr assign */
	/* MAC0_L#vif, MAC0_H#vif */
	v = get_unaligned_le32(vap->iv_myaddr);
	mac_writel(v, sc->dev, REG_VIF_MAC0_L(vif));
	v = get_unaligned_le16(vap->iv_myaddr + 4);
	mac_writel(v, sc->dev, REG_VIF_MAC0_H(vif));

	return;
}

/**
 * After Deep sleep,
 * MAC/PHY Register should be restored to the connection state before working.
 * similar to scm2020_newassoc()
 */
__ilm_wlan__ static void scm2020_pm_restore_run(struct sc_softc *sc, struct ieee80211vap *vap)
{
	struct ieee80211_node *ni = vap->iv_bss;
	struct sc_vap *tvap = vap2tvap(vap);
	int vif = tvap->index;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_key *k;

	scm2020_set_channel(ic);

	/* BSSID0: associated BSSID & BSSID Mask */
	scm2020_set_bssid(sc->dev, vif, ni->ni_macaddr, ni->ni_transmitter_bssid);
	scm2020_set_bssid_mask(sc->dev, vif, ni->ni_bssid_indicator);

	/* HT related assign */
#ifdef CONFIG_SUPPORT_HE
	if (ni->ni_flags & IEEE80211_NODE_HE) {
		ni->ni_bss_colorinfo_changed = 1;
		scm2020_update_bss_color(ni, false);
		scm2020_phy_he_dir_filter(sc, vif, PHY_HE_DIR_DL);
#ifdef CONFIG_SUPPORT_MBSSID
		scm2020_phy_he_sta_id_filter(sc, vif, ni->ni_associd, ni->ni_bssid_index);
#else
		scm2020_phy_he_sta_id_filter(sc, vif, ni->ni_associd, 0);
#endif

	}
#endif /* CONFIG_SUPPORT_HE */

	/* update sr(supported rate) */
#ifdef CONFIG_SUPPORT_SR
	if (vap->iv_opmode == IEEE80211_M_STA)
		scm2020_update_sr(ni);
#endif

	k = ieee80211_crypto_getucastkey(vap, ni);
	/* Restore the security key and reassign key of device */
	if (k != NULL) {
		int i;

		/* Restore IV/key sequence counter */
		if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_WEP) {
			((struct wep_ctx *)k->wk_private)->wc_iv = waon_p(sec_info)->txkeytsc;
		} else {
			k->wk_keytsc = waon_p(sec_info)->txkeytsc;
		}
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			k = &vap->iv_nw_keys[i];

			if (k->wk_flags & IEEE80211_KEY_DEVKEY)
				vap->iv_key_set(vap, k);
		}

		k = &ni->ni_ucastkey;
		if (k->wk_flags & IEEE80211_KEY_DEVKEY)
			vap->iv_key_set(vap, k);
	}


	/* After RX Desc restoring, enable BUF_CFG EN operation */
	scm2020_hmac_enable(sc, true);
}

#if defined(CONFIG_SCM2020_WLAN_PM) && defined(CONFIG_SUPPORT_WC_MQTT_KEEPALIVE)

static void scm2020_ps_make_mqtt_ping(struct mbuf *m, unsigned char hdr_len);

__ilm_wlan_tx__ void scm2020_check_pm_packet(struct mbuf *m)
{
	/* net80211 didn't do encapsulation because we said so. */
	const struct ether_header *eh = mtod(m, struct ether_header *);

	if (eh->ether_type == htons(ETHERTYPE_IP)) {
		const struct ip_hdr *ipv4 = IPv4BUF;
		uint16_t iphdr_hlen;

		iphdr_hlen = IPH_HL_BYTES(ipv4);
		/* Parsing UDP Packet, for registering UDP filter and Hole Punch packet */
		if (ipv4->_proto == IP_PROTO_TCP) {
			const struct tcp_hdr *tcp = TCPIPv4BUF(iphdr_hlen);
			const uint8_t *payload = TCPv4BUF(iphdr_hlen);

			if ((*payload == 0xC0) && (*(payload + 1) == 0x0) && (TCPH_FLAGS(tcp) & TCP_PSH)) {
				scm2020_ps_make_mqtt_ping(m, iphdr_hlen + TCP_HLEN);
			}

			/* If TCP, no additional checking, just return */
			return;
		}
	}
	return;
}
#endif

__ilm_wlan__ static void scm2020_dtim_pit_timer(struct ieee80211vap *vap , u64 dtim_peroid_us)
{
	u32 cur_tick;

	wlan_pm_ctx->vap = vap;

	systimer_stop(wlan_pm_ctx->dtim_timer, &wlan_pm_ctx->wifi_pm_timer);

	cur_tick = systimer_get_counter(wlan_pm_ctx->dtim_timer);
	systimer_start_at(wlan_pm_ctx->dtim_timer, &wlan_pm_ctx->wifi_pm_timer,
					cur_tick + SYSTIMER_USECS_TO_TICKS(dtim_peroid_us));
}

static void scm2020_ps_update_wmm(struct ieee80211vap *vap)
{
	struct sc_tx_queue *txq = scm2020_get_txq(vap, 0);

	waon_p(tx_wmm)->txop_limit = txq->txop_limit;

	waon_p(tx_wmm)->cw = txq->cw;
	waon_p(tx_wmm)->aifsn = txq->edca.aifsn;
}

/**
 * scm2020_ps_save_pkt()
 * put PS-Poll packet information for watcher working
 */
static void scm2020_ps_save_pkt(struct ieee80211vap *vap)
{
	struct ieee80211_node *ni;
	volatile struct waon_pkt_hdr *pkt;
	volatile struct ieee80211_frame_pspoll *pspoll;
	u16 *aid;

	ni = vap->iv_bss;

	/* For only one packet, using mbuf is waisted memory */
	pkt = WC_TX_PKT(WAON_TX_PKT_PPOL); /* in AON Retention mem */

	/*
	 * The ps poll frame
	 * hw_tx_hdr, ps poll frame, fcs
	 */
	pspoll = (struct ieee80211_frame_pspoll *) SCM2020_PM_IEEE80211_HDR(pkt);

	/* fill the ps poll frame */
	pspoll->i_fc[0] = IEEE80211_FC0_VERSION_0 |
		IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_PS_POLL;
	pspoll->i_fc[1] = IEEE80211_FC1_ORDER | IEEE80211_FC1_PWR_MGT;

	/* AID Assign */
	aid = (u16 *)&pspoll->i_aid[0];
	*aid = ni->ni_associd;

	/* BSSID Assign */
	IEEE80211_ADDR_COPY((u8 *)pspoll->i_bssid, ni->ni_bssid);
	/* Target(STA) Address Assign */
	IEEE80211_ADDR_COPY((u8 *)pspoll->i_ta, vap->iv_myaddr);

	pkt->len = HW_TX_HDR_LEN + sizeof(struct ieee80211_frame_pspoll);

	scm2020_pm_fill_hw_tx_hdr((u8 *)pkt->data, pkt->len, ni, NULL);
	return;
}

#ifndef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
/**
 * scm2020_ps_null_save_pkt()
 * put keepalive Null packet information for watcher working
 */
static void scm2020_ps_null_save_pkt(struct ieee80211vap *vap)
{
	struct ieee80211_node *ni;
	volatile struct waon_pkt_hdr *pkt;
	volatile struct ieee80211_frame *nullframe;

	ni = vap->iv_bss;

	/* For only one packet, using mbuf is waisted memory */
	pkt = WC_TX_PKT(WAON_TX_PKT_NULL); /* in AON Retention mem */

	/*
	 * hw_tx_hdr, null frame, fcs
	 */
	nullframe = (struct ieee80211_frame *)SCM2020_PM_IEEE80211_HDR(pkt);

	/* fill the NULL frame */
	nullframe->i_fc[0] = IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_NODATA;
	nullframe->i_fc[1] = IEEE80211_FC1_DIR_TODS | IEEE80211_FC1_PWR_MGT;

	/* BSSID Assign */
	IEEE80211_ADDR_COPY((u8 *)nullframe->i_addr1, ni->ni_bssid);
	/* MY MAC Assign */
	IEEE80211_ADDR_COPY((u8 *)nullframe->i_addr2, vap->iv_myaddr);
	/* DST MAC Assign */
	IEEE80211_ADDR_COPY((u8 *)nullframe->i_addr3, ni->ni_macaddr);

	/* Update seqno in the Watcher */

	pkt->len = HW_TX_HDR_LEN + sizeof(struct ieee80211_frame);

	/* clear the tx time */
	WC_TX_PKT_TIME(WAON_TX_PKT_NULL) = 0;

	scm2020_pm_fill_hw_tx_hdr((u8 *)pkt->data, pkt->len, ni, NULL);

	return;
}
#endif

/* Initialize Packet structure(GARP, UDP/TCP Home punch) */
/* Let's clear just IP address range */
static void scm2020_ps_pkt_info_init(void)
{
	volatile struct waon_pkt_hdr *pkt;

#ifdef CONFIG_PM_WATCHER_INFO_ALL_CLEAR
	/* Retention memory clearing */
	memset ((char *)SCM2010_PM_WAON_BASE_ADDR , 0 , 4*1024);
#endif
	/* my ip address is cleared */
	scm2020_ps_set_net_ip(0);

	/* GARP Packet is clearing */
	pkt = WC_TX_PKT(WAON_TX_PKT_GARP);
	pkt->len = 0;

#ifdef WATCHER_HP
	/* TCP/UDP Hole punch packet tx is clearing */
	pkt = WC_TX_PKT(WAON_TX_PKT_UDPA);
	pkt->len = 0;

	pkt = WC_TX_PKT(WAON_TX_PKT_TCPA);
	pkt->len = 0;
#endif

#ifdef CONFIG_WATCHER_ARP_CACHE
	/* ARP Cache init & timer init */
	waon_p(arp_cache)->tot_arp_cache = 0;
	callout_init(&arp_cache_timer);
#endif
	return;
}

#ifdef CONFIG_WATCHER_ARP_CACHE
void scm2020_ps_cache_arp(struct ieee80211vap *vap, u32 ipaddr)
{
	/* get the ARP Data memory from Retention */
	volatile struct arp_cache_frame *cache_arp = (struct arp_cache_frame *)waon_p(arp_cache)->arp_hdr;

	/* Fill BASIC ARP information except Source information */

	/* Target is mine, so MY MAC Assign */
	IEEE80211_ADDR_COPY((u8 *)cache_arp->eh.ether_dhost, vap->iv_myaddr);

	cache_arp->eh.ether_type = htons(ETHERTYPE_ARP);

	be16enc((u8 *)cache_arp->arp_hdr.hardware_type, 0x0001);	//ARP_HW_ETHER
	be16enc((u8 *)cache_arp->arp_hdr.protocol_type, 0x0800);	//ARP_PROT_IPv4
	cache_arp->arp_hdr.hardware_size = IEEE80211_ADDR_LEN;
	cache_arp->arp_hdr.protocol_size = 4;
	be16enc((u8 *)cache_arp->arp_hdr.opcode, 0x0002); //ARP_OP_REPLY

	IEEE80211_ADDR_COPY((u8 *)cache_arp->arp_hdr.target_mac, vap->iv_myaddr);

	/* Target is mine, so MY IP Assign */
	be32enc((u8 *)cache_arp->arp_hdr.target_ip, ipaddr);
	/* Minimum 802.3 packet size is 64, so 18 byte should be padded */
	memset ((u8 *)cache_arp->arp_hdr.padding, 0, 18);
}
#endif

/**
 * scm2020_ps_garp_save_pkt()
 * Generate GARP Packet, it also can be processed ARP response packet in Watcher
 */
void scm2020_ps_garp_save_pkt(struct ieee80211vap *vap)
{
	struct ieee80211_node *ni = vap->iv_bss;
	volatile struct waon_pkt_hdr *pkt;
	volatile struct ieee80211_frame *wh;
	volatile struct waon_arpframe *garp;
	u32 ipaddr = 0;
	u8 assign_ip[4];
	struct ieee80211_key *key = ieee80211_crypto_getucastkey(vap, ni);

	/* If ip address is not assigned, do not transmit GARP */
	ipaddr = waon_get_net_ipaddr();
	if (!ipaddr) {
		/* "GARP Packet : failed by IP address(0) */
		return;
	}

	/* get the GARP packet buffer in AON Retention mem */
	pkt = WC_TX_PKT(WAON_TX_PKT_GARP); /* in AON Retention mem */

	/*
	 * hw_tx_hdr, 802.11 header, cipher header, garp frame, fcs
	 */
	wh = (struct ieee80211_frame *)SCM2020_PM_IEEE80211_HDR(pkt);
	garp = (struct waon_arpframe *)&wh[1];

	if (key != NULL) {
		garp = (struct waon_arpframe *)((u8 *)garp + key->wk_cipher->ic_header);
	}

	/* Already made it and IP address is same , return */
	be32enc(assign_ip, ipaddr);

	/* fill the GARP frame */
	wh->i_fc[0] = IEEE80211_FC0_TYPE_DATA;
	wh->i_fc[1] = IEEE80211_FC1_DIR_TODS | IEEE80211_FC1_PWR_MGT;
	if (key != NULL)
		wh->i_fc[1] |= IEEE80211_FC1_PROTECTED;

	/* BSSID Assign */
	IEEE80211_ADDR_COPY((u8 *)wh->i_addr1, ni->ni_bssid);
	/* MY MAC Assign */
	IEEE80211_ADDR_COPY((u8 *)wh->i_addr2, vap->iv_myaddr);

	/* Normal : DST MAC Assign Broadcast or Default gw */
	/* Broadcast Mac address can be applied(adopted) */
	IEEE80211_ADDR_COPY((u8 *)wh->i_addr3, etherbroadcastaddr);

	/* Update seqno in the Watcher */

	IEEE80211_ADDR_COPY((u8 *)garp->hdr_llc_snap, llc_hdr_mac);

	garp->hdr_ether_type[0] = 0x08;
	garp->hdr_ether_type[1] = 0x06;

	be16enc((u8 *)garp->arp_hdr.hardware_type, 0x0001);	//ARP_HW_ETHER
	be16enc((u8 *)garp->arp_hdr.protocol_type, 0x0800);	//ARP_PROT_IPv4
	garp->arp_hdr.hardware_size = IEEE80211_ADDR_LEN;
	garp->arp_hdr.protocol_size = 4;
	be16enc((u8 *)garp->arp_hdr.opcode, 0x0002);	//ARP_OP_REPLY

	IEEE80211_ADDR_COPY((u8 *)garp->arp_hdr.sender_mac, vap->iv_myaddr);

	be32enc((u8 *)garp->arp_hdr.sender_ip, ipaddr);

	/* Target MAC can be ff:ff:ff:ff:ff:ff, it's up to device OS */
	/* IEEE80211_ADDR_COPY((u8 *)garp->target_mac, "\00\00\00\00\00\00"); */
	IEEE80211_ADDR_COPY((u8 *)garp->arp_hdr.target_mac, etherbroadcastaddr);

	be32enc((u8 *)garp->arp_hdr.target_ip, ipaddr);

	pkt->len = HW_TX_HDR_LEN + sizeof(struct ieee80211_frame);
	pkt->len += sizeof(struct waon_arpframe);

	if (key != NULL) {
		pkt->len += key->wk_cipher->ic_header;
	}

	/* clear the tx time */
	WC_TX_PKT_TIME(WAON_TX_PKT_GARP) = 0;

	scm2020_pm_fill_hw_tx_hdr((u8 *)pkt->data, pkt->len, ni, key);
#ifdef CONFIG_WATCHER_ARP_CACHE
	scm2020_ps_cache_arp(vap, ipaddr);
#endif
	pm_wifi_log("GARP Packet is generated with ipaddr(0x%08x)\n", ipaddr);
	return;
}

static int scm2020_ps_cipher_type(const struct ieee80211_key *key)
{
	if (key == NULL)
		return SC_CIPHER_NONE;

	switch (key->wk_cipher->ic_cipher) {
		case IEEE80211_CIPHER_WEP:
			return key->wk_keylen == 40/NBBY ? SC_CIPHER_WEP40: SC_CIPHER_WEP104;
		case IEEE80211_CIPHER_TKIP:
		case IEEE80211_CIPHER_TKIPMIC:
			return SC_CIPHER_TKIP;
		case IEEE80211_CIPHER_AES_CCM:
			return SC_CIPHER_CCMP;
		case IEEE80211_CIPHER_AES_CCM_256:
			return SC_CIPHER_CCMP256;
		case IEEE80211_CIPHER_AES_GCM:
			return SC_CIPHER_GCMP;
		case IEEE80211_CIPHER_AES_GCM_256:
			return SC_CIPHER_GCMP256;
		default:
			return SC_CIPHER_UNKNOWN; /* Not supported by hw */
	}
}

static void scm2020_ps_set_sec_key(struct ieee80211_key *k, volatile struct waon_ieee80211_key *wc_key )
{
	wc_key->cipher = scm2020_ps_cipher_type(k);
	wc_key->group = is_gtk(k);
	wc_key->wk_keylen = k->wk_keylen;
	wc_key->wk_keyix = k->wk_keyix;
	memcpy((u8 *)wc_key->wk_key, k->wk_key, IEEE80211_KEYBUF_SIZE + IEEE80211_MICBUF_SIZE);
	wc_key->wk_spp_amsdu = k->wk_spp_amsdu;
	wc_key->devkey = 1;
	/* Deep Sleep[wlan_suspend] will clear the HW key */
	wc_key->hw_key_idx = -1;

	return;
}

/**
 * scm2020_ps_save_sec_key() - put sec key information for watcher working
 */
static void scm2020_ps_save_sec_key(struct ieee80211vap *vap)
{
	struct ieee80211_node *ni;
	struct ieee80211_key *k;

	ni = vap->iv_bss;

	k = ieee80211_crypto_getucastkey(vap, ni);
	/* Restore the security key and reassign key of device */
	if (k != NULL) {
		int i;
		volatile struct waon_ieee80211_key *wc_key;

		waon_p(sec_info)->sec_mode = 1;

		waon_p(sec_info)->txcipher = k->wk_cipher->ic_cipher;
		waon_p(sec_info)->txkeyid = ieee80211_crypto_get_keyid(vap, k) << 6;;
		/* txkeytsc will be saved every PS suspend */
		waon_p(sec_info)->cipherheader = k->wk_cipher->ic_header;
		waon_p(sec_info)->ciphertrailer = k->wk_cipher->ic_trailer;
		waon_p(sec_info)->ciphertrailer += k->wk_cipher->ic_miclen;

		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			k = &vap->iv_nw_keys[i];
			wc_key = &waon_p(sec_info)->wc_key[i];

			if (k->wk_flags & IEEE80211_KEY_DEVKEY) {
				scm2020_ps_set_sec_key(k, wc_key);
			} else
				wc_key->devkey = 0;
		}

		k = &ni->ni_ucastkey;
		wc_key = &waon_p(sec_info)->wc_ucastkey;
		if (k->wk_flags & IEEE80211_KEY_DEVKEY) {
			scm2020_ps_set_sec_key(k, wc_key);
		} else
			wc_key->devkey = 0;
		pm_wifi_log("PS: saved sec key!!!\n");
	} else {
		waon_p(sec_info)->ciphertrailer = 0;
		waon_p(sec_info)->sec_mode = 0;
	}

	return;
}

#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
/* make MQTT PING Request packet */
static void scm2020_ps_make_mqtt_ping(struct mbuf *m, unsigned char hdr_len)
{
	uint32_t seqno, ackno;
	struct ieee80211_node *ni;
	struct ieee80211vap *vap;
	volatile struct waon_pkt_hdr *pkt;
	volatile struct ieee80211_frame *wh;
	volatile struct watcher_mqttping_frame *mqtt_ping;
	uint8_t *iphdr_start;
	struct ether_header *eh = mtod(m, struct ether_header *);
	struct ip_hdr *iphdr = mtodo(m, ETHER_HDR_LEN);
	struct ieee80211_key *key;

	ni = (struct ieee80211_node *) m->m_pkthdr.rcvif;
	if (!ni)
		return;

	vap = ni->ni_vap;

	key = ieee80211_crypto_getucastkey(vap, ni);
	/* get the MQTT PING packet buffer in AON Retention mem */
	pkt = WC_TX_PKT(WAON_TX_PKT_MQTT); /* in AON Retention mem */

	/*
	 * hw_tx_hdr, 802.11 header, cipher header, garp frame, fcs
	 */
	wh = (struct ieee80211_frame *)SCM2020_PM_IEEE80211_HDR(pkt);
	mqtt_ping = (struct watcher_mqttping_frame *)&wh[1];

	if (key != NULL) {
		mqtt_ping = (struct watcher_mqttping_frame *)((u8 *)mqtt_ping + key->wk_cipher->ic_header);
	}

	/* fill the MQTT PING mac header frame */
	wh->i_fc[0] = IEEE80211_FC0_TYPE_DATA;
	wh->i_fc[1] = IEEE80211_FC1_DIR_TODS | IEEE80211_FC1_PWR_MGT;
	if (key != NULL)
		wh->i_fc[1] |= IEEE80211_FC1_PROTECTED;

	/* BSSID Assign */
	IEEE80211_ADDR_COPY((u8 *)wh->i_addr1, ni->ni_bssid);
	/* MY MAC Assign */
	IEEE80211_ADDR_COPY((u8 *)wh->i_addr2, eh->ether_shost);

	/* Normal : DST MAC Assign Broadcast or Default gw */
	IEEE80211_ADDR_COPY((u8 *)wh->i_addr3, eh->ether_dhost);

	IEEE80211_ADDR_COPY((u8 *)mqtt_ping->hdr_llc_snap, llc_hdr_mac);

	mqtt_ping->hdr_ether_type = eh->ether_type;

	/* IP & TCP Header copy */
	iphdr_start = (uint8_t *)&mqtt_ping->ip_hdr;
	memcpy(iphdr_start, iphdr, hdr_len);

	seqno = ntohl(mqtt_ping->tcp_hdr.seqno);
	ackno = ntohl(mqtt_ping->tcp_hdr.ackno);
	seqno += 2;
	ackno += 2;
	mqtt_ping->tcp_hdr.seqno = htonl(seqno);
	mqtt_ping->tcp_hdr.ackno = htonl(ackno);

	waon_p(ctx)->mqtt_pingreq_ipsum = mqtt_ping->ip_hdr._chksum;

	/* MQTT PING Request type */
	mqtt_ping->mqtt_pkt_type = 0xC0;
	mqtt_ping->mqtt_remain_len = 0;

	pkt->len = HW_TX_HDR_LEN + sizeof(struct ieee80211_frame);
	pkt->len += sizeof(struct watcher_mqttping_frame);
	if (key != NULL) {
		pkt->len += key->wk_cipher->ic_header;
	}

	/* clear the tx time */
	WC_TX_PKT_TIME(WAON_TX_PKT_MQTT) = 0;

	scm2020_pm_fill_hw_tx_hdr((u8 *)pkt->data, pkt->len, ni, key);

	return;
}

static void scm2020_ps_update_mqtt_ping(struct ieee80211vap *vap)
{
	struct ieee80211_node *ni = vap->iv_bss;
	volatile struct waon_pkt_hdr *pkt;
	volatile struct ieee80211_frame *wh;
	volatile struct watcher_mqttping_frame *mqtt_ping;
	struct ieee80211_key *key;
	struct tcp_pcb *mqtt_pcb = (struct tcp_pcb *)vap->shared_mem;

	key = ieee80211_crypto_getucastkey(vap, ni);
	/* get the MQTT PING packet buffer in AON Retention mem */
	pkt = WC_TX_PKT(WAON_TX_PKT_MQTT); /* in AON Retention mem */

	/*
	 * hw_tx_hdr, 802.11 header, cipher header, garp frame, fcs
	 */
	wh = (struct ieee80211_frame *)SCM2020_PM_IEEE80211_HDR(pkt);
	mqtt_ping = (struct watcher_mqttping_frame *)&wh[1];

	if (key != NULL) {
		mqtt_ping = (struct watcher_mqttping_frame *)((u8 *)mqtt_ping + key->wk_cipher->ic_header);
	}

	/* update the seqno and ackno according to PCB info,
	* since there may be other TCP pkts which are not MQTT PING
	*/
	mqtt_ping->tcp_hdr.seqno = htonl(mqtt_pcb->snd_nxt);
	mqtt_ping->tcp_hdr.ackno = htonl(mqtt_pcb->rcv_nxt);
	mqtt_ping->tcp_hdr.src = htons(mqtt_pcb->local_port);
	waon_p(ctx)->mqtt_pingrxfail = 0;
}
#endif

#ifdef CONFIG_PM_HOLE_PUNCH
/* TCP Header checksum Primitive */
static u16 tcpudp_pseudo_hdr_checksum(u16 *pseudo_header, u16 *ptr, int n)
{
	long sum;
	unsigned short odd;
	unsigned short answer;
	int i;

	/* Pseudo header checksum */
	sum = 0;
	for (i = 0; i < 6; i++)
		sum += *pseudo_header++;

	while (n > 1) {
		sum += *ptr++;
		n -= 2;
	}

	if (n == 1) {
		odd = 0;
		*((unsigned char *) &odd) = *(unsigned char *) ptr;
		sum += odd;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum = sum + (sum >> 16);
	answer = (unsigned short) ~sum;

	return answer;
}

/* make udp hole punch packet */
static void scm2020_ps_make_udp_hole(struct mbuf *m, unsigned long destip, unsigned short srcport, unsigned char hdr_len)
{
	struct ieee80211_node *ni;
	struct ieee80211vap *vap;
	volatile struct waon_pkt_hdr *pkt;
	volatile struct watcher_udphole_frame *udp_hole;
	u32 ipaddr = 0;
	u16 portnum = 0;
	char *iphdr_start;
	struct ether_header *eh = mtod(m, struct ether_header *);
	struct ip_hdr *iphdr = mtodo(m, ETHER_HDR_LEN);
	volatile struct udp_hdr *udphdr;
	struct ieee80211_key *key;

	ni = (struct ieee80211_node *) m->m_pkthdr.rcvif;
	if (!ni)
		return;

	vap = ni->ni_vap;

	key = ieee80211_crypto_getucastkey(vap, ni);
	/* get the UDP Hole packet buffer in AON Retention mem */
	pkt = WC_TX_PKT(WAON_TX_PKT_UDPA); /* in AON Retention mem */

	udp_hole = (struct watcher_udphole_frame *)SCM2020_PM_IEEE80211_HDR(pkt);

	if (key != NULL) {
		udp_hole = (struct watcher_udphole_frame *)((u8 *)udp_hole + key->wk_cipher->ic_header);
	}

	/* Already made it and IP address is same , return */
	ipaddr = udp_hole->ip_hdr.dest.addr;
	portnum = udp_hole->udp_hdr.src;
	if (pkt->len != 0 && ipaddr == destip && portnum == srcport) {
		/* UDP Hole punch Packet already generated */

		pm_wifi_log("UDP Hole punch is already registered(0x%08x:0x%04x)\n", ipaddr , portnum);
		WC_TX_PKT_TIME(WAON_TX_PKT_UDPA) = 0;
		/* don't need to update UDP Hole punch packet */
		return;
	}

	/* fill the UDP Hole mac header frame */
	udp_hole->mac_hdr.i_fc[0] = IEEE80211_FC0_TYPE_DATA;
	udp_hole->mac_hdr.i_fc[1] = IEEE80211_FC1_DIR_TODS | IEEE80211_FC1_PWR_MGT;

	if (key != NULL)
		udp_hole->mac_hdr.i_fc[1] |= IEEE80211_FC1_PROTECTED;

	/* BSSID Assign */
	IEEE80211_ADDR_COPY((u8 *)udp_hole->mac_hdr.i_addr1, ni->ni_bssid);
	/* MY MAC Assign */
	IEEE80211_ADDR_COPY((u8 *)udp_hole->mac_hdr.i_addr2, eh->ether_shost);

	/* Normal : DST MAC Assign Broadcast or Default gw */
	/* Broadcast Mac address can be applied(adopted) */
	IEEE80211_ADDR_COPY((u8 *)udp_hole->mac_hdr.i_addr3, eh->ether_dhost);

	IEEE80211_ADDR_COPY((u8 *)udp_hole->hdr_llc_snap, llc_hdr_mac);

	udp_hole->hdr_ether_type = eh->ether_type;

	/* IP & UDP Header copy */
	iphdr_start = (char *)(SCM2020_PM_IEEE80211_HDR(pkt) + sizeof(struct ieee80211_frame) + 8);
	memcpy (iphdr_start, (char *)(iphdr), hdr_len);

	/* If UDP Data is exist, clearing for NO Data */
	udphdr = (struct udp_hdr *)&udp_hole->udp_hdr;
	if (udphdr->len > 8) {
		struct tcpudp_pseudo_hdr udp_pseudo;
		u16 udp_checksum;
		char *psedohdr;
		char *pudphdr;

		/* Starting UDP Header checksum calculation */

		/* UDP pseudo header form checksum */
		udp_pseudo.src_addr = udp_hole->ip_hdr.src.addr;
		udp_pseudo.dst_addr = udp_hole->ip_hdr.dest.addr;
		udp_pseudo.reserved = 0;
		udp_pseudo.protocol = 17;

		psedohdr = (char *)&udp_pseudo;

		/* udp length assignment & swapping. length is just udp header */
		psedohdr[10] = 0;
		psedohdr[11] = sizeof(struct udp_hdr);

		/* 6. TCP Checksum calculation */
		udp_hole->udp_hdr.chksum = 0;
		udp_hole->udp_hdr.len = TCPUDP_CHKSUM_SWAP_BYTES(UDP_HLEN);
		pudphdr = (char *)&udp_hole->udp_hdr;
		udp_checksum = tcpudp_pseudo_hdr_checksum((u16 *) &udp_pseudo, (u16 *)pudphdr, UDP_HLEN);
		udp_hole->udp_hdr.chksum = TCPUDP_CHKSUM_SWAP_BYTES(udp_checksum);
	}

	pkt->len = HW_TX_HDR_LEN + sizeof(struct watcher_udphole_frame);

	if (key != NULL) {
		pkt->len += key->wk_cipher->ic_header;
	}

	/* clear the tx time */
	WC_TX_PKT_TIME(WAON_TX_PKT_UDPA) = 0;

	scm2020_pm_fill_hw_tx_hdr((u8 *)pkt->data, pkt->len, ni, key);

	pm_wifi_log("Make New UDP Hole punch (0x%08x:0x%04x)\n", destip , srcport);
	return;
}

/* clearing udp hole punch packet */
static void scm2020_ps_clear_tcp_hole(unsigned long destip, unsigned short srcport)
{
	volatile struct waon_pkt_hdr *pkt;
	volatile struct watcher_tcphole_frame *tcp_hole;
	u32 ipaddr = 0;
	u16 portnum = 0;

	/* get the TCP Hole packet buffer in AON Retention mem */
	pkt = WC_TX_PKT(WAON_TX_PKT_TCPA); /* in AON Retention mem */

	tcp_hole = (struct watcher_tcphole_frame *)SCM2020_PM_IEEE80211_HDR(pkt);

	/* Already made it and IP address is same , return */
	ipaddr = tcp_hole->ip_hdr.dest.addr;
	portnum = tcp_hole->tcp_hdr.src;
	if (ipaddr == destip && portnum == srcport) {
		/* let's clearing */
		WC_TX_PKT_TIME(WAON_TX_PKT_TCPA) = 0;
		pkt->len = 0;
		/* clearing TCP registered port */
		scm2020_ps_clr_filter(FILTER_TCP, ((srcport & 0xff00) >> 8) | ((srcport & 0x00ff) << 8));

		pm_wifi_log("TCP Hole punch is deleted(0x%08x:0x%04x)\n", ipaddr , portnum);
		return;
	}
	pm_wifi_log("TCP Hole punch is not matched(0x%08x:0x%04x)\n", ipaddr , portnum);
	return;
}

/* make tcp hole punch packet */
static void scm2020_ps_make_tcp_hole(struct mbuf *m, unsigned long destip, unsigned short srcport, unsigned char hdr_len)
{
	struct ieee80211_node *ni;
	struct ieee80211vap *vap;
	volatile struct waon_pkt_hdr *pkt;
	volatile struct watcher_tcphole_frame *tcp_hole;
	u32 ipaddr = 0;
	u16 portnum = 0;
	char *iphdr_start;
	struct ether_header *eh = mtod(m, struct ether_header *);
	struct ip_hdr *iphdr = mtodo(m, ETHER_HDR_LEN);
	struct ieee80211_key *key;

	ni = (struct ieee80211_node *) m->m_pkthdr.rcvif;
	if (!ni)
		return;

	vap = ni->ni_vap;

	key = ieee80211_crypto_getucastkey(vap, ni);
	/* get the TCP Hole packet buffer in AON Retention mem */
	pkt = WC_TX_PKT(WAON_TX_PKT_TCPA); /* in AON Retention mem */

	tcp_hole = (struct watcher_tcphole_frame *)SCM2020_PM_IEEE80211_HDR(pkt);

	if (key != NULL) {
		tcp_hole = (struct watcher_tcphole_frame *)((u8 *)tcp_hole + key->wk_cipher->ic_header);
	}

	/* Check if tcp port is registered */
	if (!scm2020_ps_chk_filter(FILTER_TCP, ((srcport & 0xff00) >> 8) | ((srcport & 0x00ff) << 8))) {
		pm_wifi_log("TCP Port is not registered(0x%04x)\n", srcport);
		return;
	}

	/* Already made it and IP address is same , return */
	ipaddr = tcp_hole->ip_hdr.dest.addr;
	portnum = tcp_hole->tcp_hdr.src;
	if (pkt->len != 0 && ipaddr == destip && portnum == srcport) {
		/* TCP Hole punch Packet already generated */
		pm_wifi_log("TCP Hole punch is already registered(0x%08x:0x%04x)\n", ipaddr , portnum);

		/* TCP ACK is copied to Retention */
		goto tcp_ack_copy;
	}
	pm_wifi_log("Make First TCP Hole punch (0x%08x:0x%04x)\n", destip , srcport);

	/* fill the TCP Hole mac header frame */
	tcp_hole->mac_hdr.i_fc[0] = IEEE80211_FC0_TYPE_DATA;
	tcp_hole->mac_hdr.i_fc[1] = IEEE80211_FC1_DIR_TODS | IEEE80211_FC1_PWR_MGT;
	if (key != NULL)
		tcp_hole->mac_hdr.i_fc[1] |= IEEE80211_FC1_PROTECTED;

	/* BSSID Assign */
	IEEE80211_ADDR_COPY((u8 *)tcp_hole->mac_hdr.i_addr1, ni->ni_bssid);
	/* MY MAC Assign */
	IEEE80211_ADDR_COPY((u8 *)tcp_hole->mac_hdr.i_addr2, eh->ether_shost);

	/* Normal : DST MAC Assign Broadcast or Default gw */
	IEEE80211_ADDR_COPY((u8 *)tcp_hole->mac_hdr.i_addr3, eh->ether_dhost);

	IEEE80211_ADDR_COPY((u8 *)tcp_hole->hdr_llc_snap, llc_hdr_mac);

	tcp_hole->hdr_ether_type = eh->ether_type;

tcp_ack_copy:

	/* IP & TCP Header copy */
	iphdr_start = (char *)(SCM2020_PM_IEEE80211_HDR(pkt) + sizeof(struct ieee80211_frame) + 8);
	memcpy (iphdr_start, (char *)(iphdr), hdr_len);

	/* Important!!! TTL has to be minimized */
	/** Will use TCP Last Packet(TCP ACK) with a little TTL value(2~3) that is just enough to cross their own NATs.
	 * This TCP ACK will be dropped in the middle of the network before reaching to its destination.
	 * Hole punch packet focus on default gw NAT Table
	 */
	tcp_hole->ip_hdr._ttl = 2;

	pkt->len = HW_TX_HDR_LEN + sizeof(struct watcher_tcphole_frame);
	if (key != NULL) {
		pkt->len += key->wk_cipher->ic_header;
	}

	/* clear the tx time */
	WC_TX_PKT_TIME(WAON_TX_PKT_TCPA) = 0;

	scm2020_pm_fill_hw_tx_hdr((u8 *)pkt->data, pkt->len, ni, key);

	return;
}

/* Update TCP Hole punch packet about SEQ Number */
static void scm2020_ps_update_tcp_hole(struct mbuf *m, unsigned long destip,
	unsigned short srcport, unsigned long seqno, unsigned char hdr_len)
{
	struct ieee80211_node *ni;
	struct ieee80211vap *vap;
	volatile struct waon_pkt_hdr *pkt;
	volatile struct watcher_tcphole_frame *tcp_hole;
	u32 ipaddr = 0;
	u16 portnum = 0;
	u16 payload_len = 0;
	struct ieee80211_key *key;

	ni = (struct ieee80211_node *) m->m_pkthdr.rcvif;
	if (!ni)
		return;

	vap = ni->ni_vap;

	key = ieee80211_crypto_getucastkey(vap, ni);
	/* get the TCP Hole packet buffer in AON Retention mem */
	pkt = WC_TX_PKT(WAON_TX_PKT_TCPA);

	tcp_hole = (struct watcher_tcphole_frame *)SCM2020_PM_IEEE80211_HDR(pkt);
	if (key != NULL) {
		tcp_hole = (struct watcher_tcphole_frame *)((u8 *)tcp_hole + key->wk_cipher->ic_header);
	}

	/* Already made it and IP address is same , return */
	ipaddr = tcp_hole->ip_hdr.dest.addr;
	portnum = tcp_hole->tcp_hdr.src;

	/* TCP Hole punch is enabled and session is matched */
	if (pkt->len != 0 && (ipaddr == destip && portnum == srcport)) {
		u32 tcp_seq_number;
		struct tcpudp_pseudo_hdr tcp_pseudo;
		u16 tcp_checksum;
		volatile u8 *tcphdr;
		char *psedohdr;

		/* TCP Hole punch Packet already generated */
		tcphdr = (u8 *) &tcp_hole->tcp_hdr;
		pm_wifi_log("current seq number %ld\n", lwip_ntohl(tcp_hole->tcp_hdr.seqno));

		/* 1. get the payload length except header (MAC, LLC ) */
		//payload_len = m->m_pkthdr.len - ieee80211_hdrspace(vap->iv_ic, mtod(m, void *));

		/* 2. get the real payload length except 802.3, IP, TCP */
		payload_len = m->m_len - (ETHER_HDR_LEN + hdr_len);

		/* If PSH + ACK has no payload, seq number is same, so return */
		if (payload_len <= 0) {
			pm_wifi_log("TCP Hole punch update No Payload\n");
			return;
		}
		pm_wifi_log("TCP Hole punch update(m->m_len %d, hdr_len %d, Payload len : %d)\n", m->m_len, hdr_len, payload_len);

		/* 3. Get the seq number of current tcp packet */
		tcp_seq_number = lwip_ntohl(tcp_hole->tcp_hdr.seqno);

		tcp_seq_number += payload_len;

		/* 4. Add seq number of TCP hole punch */
		tcp_hole->tcp_hdr.seqno = lwip_htonl(tcp_seq_number);

		pm_wifi_log("Update seq number %ld\n", lwip_ntohl(tcp_hole->tcp_hdr.seqno));
		/* Checksum should be updated */

		tcphdr = (u8 *)&tcp_hole->tcp_hdr;

		/* Starting TCP Header checksum calculation */

		/* 5. TCP pseudo header form checksum */
		tcp_pseudo.src_addr = tcp_hole->ip_hdr.src.addr;
		tcp_pseudo.dst_addr = tcp_hole->ip_hdr.dest.addr;
		tcp_pseudo.reserved = 0;
		tcp_pseudo.protocol = 6;

		psedohdr = (char *) &tcp_pseudo;

		/* tcp length assignment & swapping. length is just tcp header */
		psedohdr[10] = 0;
		psedohdr[11] = TCP_HLEN;

		/* 6. TCP Checksum calculation */
		tcp_hole->tcp_hdr.chksum = 0;
		tcp_checksum = tcpudp_pseudo_hdr_checksum((u16 *) &tcp_pseudo, (u16 *)tcphdr, TCP_HLEN);
		tcp_hole->tcp_hdr.chksum = TCPUDP_CHKSUM_SWAP_BYTES(tcp_checksum);

		pkt->len = HW_TX_HDR_LEN + sizeof(struct watcher_tcphole_frame);
		if (key != NULL) {
			pkt->len += key->wk_cipher->ic_header;
		}

		/* clear the tx time & enable tx flag */
		WC_TX_PKT_TIME(WAON_TX_PKT_TCPA) = 0;

		scm2020_pm_fill_hw_tx_hdr((u8 *)pkt->data, pkt->len, ni, key);
	}
	return;
}
#endif

#ifdef CONFIG_WATCHER_ARP_CACHE
__maybe_unused
static void scm2020_ps_arp_restore(void *data)
{
	struct ieee80211vap *vap = data;
	struct mbuf *m;
	volatile struct arp_cache_frame *cache_arp;
	int index;
	struct ifnet *ifp = vap->iv_ifp;

	cache_arp = (struct arp_cache_frame *)waon_p(arp_cache)->arp_hdr;

	/* Fill ARP information */
	for (index = 0; index < waon_p(arp_cache)->tot_arp_cache; index++) {
		/* fill the remained ARP information */
		/* SRC MAC Assign */
		IEEE80211_ADDR_COPY((u8 *)cache_arp->eh.ether_shost, (u8 *)waon_p(arp_cache)->arp_info[index].sender_mac);
		IEEE80211_ADDR_COPY((u8 *)cache_arp->arp_hdr.sender_mac, (u8 *)waon_p(arp_cache)->arp_info[index].sender_mac);
		/* SRC IP Assign */
		memcpy((u8 *)cache_arp->arp_hdr.sender_ip, (u8 *)waon_p(arp_cache)->arp_info[index].sender_ip, 4);

		m = m_gethdr(M_NOWAIT, MT_DATA);
		if (!m) {
			waon_p(arp_cache)->tot_arp_cache = 0;
			return;
		}

		memcpy(m->m_data, (u8 *)cache_arp, 64);
		m->m_len = m->m_pkthdr.len = 64;
		m->m_pkthdr.rcvif = ifp;

		ifp->if_input(ifp, m);

		pm_wifi_log("ARP Cache info(%d) Generated\n", index);
	}
	waon_p(arp_cache)->tot_arp_cache = 0;
}
#endif

void scm2020_watcher_test_set_delay(u32 delay_ms_time)
{
	printf("delay ms %d\n", delay_ms_time);
	watcher_first_delay_ms_time = delay_ms_time;
}

/**
 * scm2020_ps_awake() - wake the interface up to receive a beacon
 *
 */
void scm2020_ps_awake(void *data)
{
	struct ieee80211vap *vap = data;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = vap->iv_bss;
	struct sc_softc *sc = (struct sc_softc *)ic->ic_softc;
	struct sc_vap *tvap = vap2tvap(vap);
	u64 tsf;
	u32 sleeptime;

	if (sc->doze == 0)
		return;

	/** Important !!!
	 * Before doze timer(ps_doze) wakeup,
	 * by other OS Timer ps_awake is called for state transition
	 * dtim parsing related timer should be stopped
	 */
	systimer_stop(wlan_pm_ctx->dtim_timer, &wlan_pm_ctx->wifi_pm_timer);

	sc->doze = 0;

	scm2020_pm_stay();

	tsf = scm2020_get_tsf(ic, ni);

	sleeptime = pm_get_residual();

	if (sleeptime) {
		tvap->tot_doze_time += sleeptime;
#ifdef DEBUG_TBTT
		pm_wifi_log( "PS: beacon(TS=%llu) after doze@%d\n", tsf, sleeptime);
#endif
	} else {
		/* If not Tickless_idle, return tsf difference */
		tvap->tot_doze_time += (int) (tsf - tvap->doze_start);
	}

 /* XXX: No need to reset statistics cause this will not happen
  *      in case of real low power modes.
  */
#if 0
    RESET_BMISS_STATS(tvap);
#endif
	taskqueue_enqueue_fast(sc->sc_tq, &sc->ps_chk_bcn_miss_task);


}

#define USEC_TO_MTIME(u)         ((u) * (CONFIG_XTAL_CLOCK_HZ / CONFIG_MTIME_CLK_DIV / 1000000))
/**
 * scm2020_ps_doze() - put an interface into doze mode
 */
__ilm_wlan__ void scm2020_ps_doze(struct ieee80211vap *vap, u64 tsf_gap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct sc_vap *tvap = vap2tvap(vap);
	struct ieee80211vap *vp;
	struct sc_softc *sc = (struct sc_softc *)ic->ic_softc;
	struct ieee80211_node *ni = vap->iv_bss;
	int zzz = 0;
	u64 tsf;
	u64 dtim_peroid_us;
	u32 sleep_clk_acc;
	int n_beacon;
	int bi = ni->ni_intval << 10;       /* 1 TU = 1024 microsecond */

	/* If there are more than one active interface, we do not put
	 * @ic into doze mode
	 */

	/* set sc->doze = 0 will abort processing the previous scm2020_ps_awake */
	TAILQ_FOREACH(vp, &ic->ic_vaps, iv_next) {
		if (vp != vap && vp->iv_state > IEEE80211_S_INIT)
			return;
	}

	if (sc->doze == 1)
		return;

	/* If NULL function with PM is not acked, do not enter doze mode */
	if (tvap->ps == 0) {

#if 0 /* Let's remove below, it can be took time to enter PM without this one */
		/* If PM APP is enabled and still Null is not acked, we have to change as RUN state
		 */
		if (scm2020_ps_check_pm_master()) {
			if (vap->iv_state == IEEE80211_S_SLEEP) {
				/* Change into Run state */
				ieee80211_new_state_locked(vap, IEEE80211_S_RUN, 0);
				vap->iv_sta_ps(vap, 0);
			}
			pm_wifi_log("[PM WiFi] ps_doze(%d), pm(%d)\n", vap->iv_state, scm2020_ps_check_pm_master());
		}
#endif
		goto out;
	}

	/* let's apply the synchronize TSF */
	tsf = scm2020_get_tsf(ic, ni);

	/* get the next wakeup time by Mtime(10M) unit */
	dtim_peroid_us = tvap->ntbtt - tsf;
	if (dtim_peroid_us <= SCM2020_DOZE_AWAKE_DELAY) {
		goto out;
	}
#if 0 /* Already included in local tsf calculation, so remove it */
	/* bss_update & recv_mgmt() delay */
	dtim_peroid_us -= SCM2020_DOZE_AWAKE_DELAY;
#endif
	/* Check Minimum or Maximum sleep duration */
	if (dtim_peroid_us < SCM2020_BCN_MIN_DURATION || dtim_peroid_us > SCM2020_BCN_MAX_DURATION) {
		goto out;
	}

	tvap->doze_start = tsf;

	/* get & set the OS Idle Time, use adjusting local tsf */
	zzz = (int) (dtim_peroid_us) / 1000;

	/* TX Q is busy */
	if (scm2020_tx_checkbusy(vap, sc)) {
		/* State should be continued */
		pm_wifi_log("PS: txq is busy\n");
		goto out;
	}

	/* If forcely disable entering deep sleep or PM APP is not enabled */
	if (!scm2020_watcher_onoff_flag || !scm2020_ps_check_pm_master()) {
		/* Just do PS Function */
		callout_reset(&sc->doze_timer, pdMS_TO_TICKS(zzz), scm2020_ps_awake, vap);
		goto SLEEP;
	}

	if (watcher_save_info_flag) {
		wlan_pm_ctx->vap = vap;
		scm2020_ps_update_wmm(vap);
		scm2020_ps_save_pkt(vap);
		scm2020_ps_save_sec_key(vap);
#ifndef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
		scm2020_ps_null_save_pkt(vap);
#endif
		/* scm2020_ps_garp_save_pkt(vap); */
		watcher_save_info_flag = false;
	}

	/* All of condition and status is normal for PM */
	/* clear the tx timer of Watcher */
	waon_clear_tx_time();
#ifdef CONFIG_WATCHER_ARP_CACHE
	waon_clear_arp_cache();
#endif
#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
	scm2020_ps_update_mqtt_ping(vap);
#endif

	/* Wathcer has it own tx keeplive, disable this */
	if (g_sc_txkeep_intvl)
		osTimerStop(vap->tx_keeplive_timer);

	/*
	 * NB: The priority of ksofttimerd should be higher than PI_NET
	 * Otherwise, the callouts undergo severe delay
	 */

	/* Even if we use listen interval, let's try
	 * to wake up at DTIMs.
	 * So, if if lintval is, e.g., 1300, and dtim_period
	 * is 3, i.e., DTIM3, lintval will be adjusted to 1200.
	 */
	SCM2020_CALCULATE_DTIM_INTERVAL(ic, ni, n_beacon);

	waon_p(ctx)->ni_dtim_period = ni->ni_dtim_period;
	waon_p(ctx)->dtim_peroid = n_beacon * bi;

#if 0
	printk("[%s, %d]dtim: (%d, %d), n_beacon: %d\n", __func__, __LINE__, ni->ni_dtim_count, ni->ni_dtim_period, n_beacon);
#endif

#ifdef DEBUG_TBTT
	pm_wifi_log("PS: ntbtt %llu, dtim_tsf %llu, duration(%llu)\n", tvap->ntbtt, tvap->dtim_tsf, waon_p(ctx)->dtim_peroid);
#endif

	/* Clear beacon dtim interrupt time */
	//waon_p(ctx)->last_dtim_mtime = 0;

	/* clear the active state change flag */
	waon_p(ctx)->wifi_active_state = 0;

	scm2010_doze_window_reset();

	/* apply time difference between "beacon rx" and "set doze" */
	/* If duration is over 1TU, first tsf timegap has big value, can not use it */
	if (ic->ic_lintval != IEEE80211_BINTVAL_DEFAULT) {
		dtim_peroid_us -= SCM2020_BCN_RX_DEF_TIME_DELAY;     // need extra process time
	}

	sleep_clk_acc = (1500 * (dtim_peroid_us / 1000)) / 1000;
	dtim_peroid_us -= sleep_clk_acc;

	if ((dtim_peroid_us > SCM2020_BCN_RX_DEF_TIME_DELAY) && (dtim_peroid_us < waon_p(ctx)->dtim_peroid))
		scm2020_dtim_pit_timer(vap, dtim_peroid_us);
	else /* If beacon gap is larger and sleep is shorter, do set as next wakeup time */
		scm2020_dtim_pit_timer(vap, waon_p(ctx)->dtim_peroid);

	scm2020_pm_relax();

	pm_wifi_log("PS: ntbtt %llu, duration(%llu), sleep(%llu)\n", tvap->ntbtt, waon_p(ctx)->dtim_peroid, dtim_peroid_us);

SLEEP:
	pm_set_residual(zzz);

	sc->doze = 1;

#ifdef CONFIG_MAC_TX_TIMEOUT
	osTimerStop(txq_timeout_timer);
#endif

out:
	callout_stop(&vap->iv_swbmiss);
}

__ilm_wlan__ static void scm2020_dtim_awake(void *arg)
{
	scm2020_ps_awake((void *)wlan_pm_ctx->vap);
}

static void scm2020_ps_reduce_rf_current(bool lower)
{
#ifdef CONFIG_PM_RF_CURRENT_REDUCTION
	if (lower == (waon_p(ctx)->rf_reduced == 1))
		return;

	if (lower) {
		writel(0x00000318, RF_BASE_START + 0x3c);       //RX_RSSIPGA_EN = 1->0
		writel(0x00000300, RF_BASE_START + 0x40);       //RX_RSSIADC_EN = 1->0
		writel(0x00008300, RF_BASE_START + 0x48c);      //RG_RX_LNA_IBIAS_WIFI = 6->0, Misen loss 2dB, RSSI report error 5dB add
		writel(0x0000227f, RF_BASE_START + 0x490);      //RG_RX_LPF_LDO_R1 = 3->7

		/*XXX: will not enable this optimization because it is observed
		 *     to incur some instability.
		 */
#if 0
		writel(0x0000344a, RF_BASE_START + 0x49c);      //RG_SX_XTAL_IBIAS = 4->1
#endif
		//writel(0x0000ca06, RF_BASE_START + 0x480);      //RG_LDO09_SX_VOUT = 2->2
		writel(0x00004845, RF_BASE_START + 0x488);      //RG_RX_ADC_LDO_R1 = 3->1
		/* XXX: Ignore LDO_PREP during transitioning. */
		writel(0x00005002, RF_BASE_START + 0x018);      //B_WIFI_TRX_SWTICH_SEL = 1->0
	} else {
		writel(0x00006318, RF_BASE_START + 0x3c);
		writel(0x00000318, RF_BASE_START + 0x40);
		writel(0x00008360, RF_BASE_START + 0x48c);
		writel(0x00002267, RF_BASE_START + 0x490);
#if 0
		writel(0x0000d44a, RF_BASE_START + 0x49c);
#endif
		//writel(0x0000ca06, RF_BASE_START + 0x480);
		writel(0x0000484d, RF_BASE_START + 0x488);
		/* XXX: Honor LDO_PREP again. */
		writel(0x00005202, RF_BASE_START + 0x018);      //B_WIFI_TRX_SWTICH_SEL = 0->1
	}

	waon_p(ctx)->rf_reduced = (lower ? 1 : 0);
#endif
}

static void scm2020_ps_deliver_pkts(struct sc_softc *sc)
{
	int readidx;
	struct sc_rx_desc *desc;
	int ret;

	if (!waon_p(ctx)->wakeup_by_rx)
		return;

	readidx = RX_PTR2IDX(sc, read_ptr(sc, READ));
	desc = &sc->rx.desc[readidx];
	while (1) {
		rx_desc_change_state(sc, desc, RX_DESC_SW_READY);
		advance_ptr(sc, READ);
		if (is_room(sc, READ)) {
			desc = rx_desc_next(sc, desc);
		} else
			break;
	};

	/*
	 * It is a bug that taskqueue_enqueue_fast will return -1
	 * when it is supposed to return 0.
	 * So, we need Specifically check if it returns 1 to see if
	 * we need to reschedule.
	 *
	 * Since this is the only where we check the return value,
	 * I would not bother to patch taskqueue_thread_enqueue.
	 */
	if ((ret = taskqueue_enqueue_fast(sc->sc_tq, &sc->rx.handler_task)) < 0) {
		printk("[%s, %d] Error: %d\n", __func__, __LINE__, ret);
	}
}


#ifndef CONFIG_IPC
__ilm_wlan__
#endif
int scm2020_wlan_ps_resume(struct device *dev)
{
	struct sc_softc *sc = dev->driver_data;
	struct ieee80211vap *vap;
	struct ieee80211vap *c_vap;
	struct sc_vap *tvap __maybe_unused;
	u32 v;


	if (!wlan_pm_ctx->suspended)
		return 0;

	if ((sc->ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		pm_wifi_log("[PM WiFi] vap is not up\n");
		return -EPERM;
	}

	/* 0. check WiFi mode & connection status */
	c_vap = NULL;
	TAILQ_FOREACH(vap, &sc->ic->ic_vaps, iv_next) {
		if (vap->iv_opmode == IEEE80211_M_STA && vap->iv_state >= IEEE80211_S_RUN) {
			c_vap = vap;
	        tvap = vap2tvap(vap);
			break;
		}
	}
	if (c_vap == NULL) {
		pm_wifi_log("[PM WiFi] vap is not assigned\n");
		return -EPERM;
	}

#if 0
	/* Assert if PHY has gone abnormal during wather.
	 */
	assert(waon_p(ctx)->phy_cs_sat_cnt_nok == 0);
	assert(waon_p(ctx)->phy_cs_stf_cnt_nok == 0);
	assert(waon_p(ctx)->phy_cs_b11_cnt_nok == 0);
	assert(waon_p(ctx)->phy_fcs_ok_cnt_nok == 0);
#endif

	/* Rollback in RF Current reduction flag in sleep */
	scm2020_ps_reduce_rf_current(false);

	/* 1 normal initialized Mac/Phy reg restore */
	scm2020_pm_restore_init(sc, c_vap);

	/* 2. If connection status, assoc related MAC/PHY reg restore */
	scm2020_pm_restore_run(sc, c_vap);

	vap->iv_bss->ni_txseqs[IEEE80211_NONQOS_TID] = waon_p(ctx)->nonqos_seqno;

	/* In HI, after wakeup tx queue should be initialized */
	if (pm_wakeup_is_himode() && pm_reserved_buf_get_addr()) {
		extern void scm2020_reset_tx_queue(struct sc_softc *sc);

		pm_wifi_log("In HI, TX queue restoring\n");
		scm2020_reset_tx_queue(sc);
	}

	/* Take care of packets received and passed by watcher */
	scm2020_ps_deliver_pkts(sc);

#if 0
	/* If os/net change as active, do it */
	if (waon_p(ctx)->wifi_active_state) {

		/* After wakeup, check if legacy PS work well  */
		/* Without below, can not detect AP disconnection */
		if (vap->iv_state == IEEE80211_S_SLEEP) {
			/* Change into Run state */
			ieee80211_new_state_locked(vap, IEEE80211_S_RUN, 0);
			pm_wifi_log("[PM WiFi] WiFi state as Active\n");
		}

		waon_p(ctx)->wifi_active_state = 0;
	}
#endif

	wlan_pm_ctx->counter.fullwakeup++;
#if 1 /* after checking more, do enable */
	{
#define WAKEUP_SRC_GPIO_BIT (1 << 0)
#define WAKEUP_SRC_RTC_BIT (1 << 1)
		u16 pm_wakeup_evt = 0, pm_wakeup_reason = 0;

		pm_wakeup_evt = pm_wakeup_get_event();
		/* RTC wakeup --> WAKEUP_EVENT_RECORD(19) is 0 */
		if (pm_wakeup_evt == 0) {
			wlan_pm_ctx->counter.wakeupsrc_rtc++;
		}
		/* RTC wakeup */
		if (pm_wakeup_evt & WAKEUP_SRC_GPIO_BIT) {
			wlan_pm_ctx->counter.wakeupsrc_ext++;
		}

		pm_wakeup_reason = pm_wakeup_get_reason();
		wlan_pm_ctx->counter.wakeup_reasons[pm_wakeup_reason]++;

		if (pm_wakeup_reason == FULL_BOOT_REASON_WIFI) {
			u16 wakeup_type = pm_wakeup_get_type();
			u16 wakeup_subtype = pm_wakeup_get_subtype();
			//printk("t:%d st: %d\n", wakeup_type, wakeup_subtype);

			if ((wakeup_type < WIFI_BOOT_TYPE_MAX) &&
				(wakeup_subtype < WIFI_BOOT_SUBTYPE_MAX)) {
				wlan_pm_ctx->counter.wakeup_types_wifi[wakeup_type][wakeup_subtype]++;
			}
		}
	}
#endif

	/* already active state return , do not check sc doze */
	/* MAC/RF Resume & RF will be controlled by RF Driver */

	/* enable all assigned interrupt */
	v = mac_readl(sc->dev, REG_INTR_ENABLE);
	bfmod_m(v, INTR_ENABLE, RX_BUF, 1);

	bfmod_m(v, INTR_ENABLE, TX_DONE, RBFM(INTR_ENABLE, TX_DONE));
#ifdef CONFIG_SUPPORT_HE
	bfmod_m(v, INTR_ENABLE, BASIC_TRIG_QNULL_VIF, 0x3);
#endif
	mac_writel(v, sc->dev, REG_INTR_ENABLE);

	/* full wakeup from watcher */
	/* set state as active */
	sc->doze = 0;

	scm2020_pm_stay()

	/* Restart bmiss statistics */
	RESET_BMISS_STATS(tvap);
	/* Restart bmiss check */
	taskqueue_enqueue_fast(sc->sc_tq, &sc->ps_chk_bcn_miss_task);

	pm_wifi_log("[PM WiFi] Wlan mac/phy reg restored state:%x\n", c_vap->iv_state);

#if VERIFY_WLAN_MAC_REGS
	for (int i = 0; i < MAC_REG_SIZE / sizeof(u32); i++) {
		if (scm2020_wlan_mac_regs_comp[i] != readl(MAC_BASE_START + MAC_REG_OFFSET + i * sizeof(u32))) {
			printk("[MAC] %8p differs: 0x%08x vs. 0x%08x\n",
				(MAC_BASE_START + MAC_REG_OFFSET + i * sizeof(u32)),
				scm2020_wlan_mac_regs_comp[i],
				readl(MAC_BASE_START + MAC_REG_OFFSET + i * sizeof(u32)));
		}
	}
#endif

#if VERIFY_WLAN_PHY_REGS
	for (int i = 0; i < PHY_REG_SEG_NUM; i++) {
		for (int j = 0; j < scm2020_phy_reg[i].size / sizeof(u32); j++) {
			if (scm2020_wlan_phy_regs_comp[i][j] != readl(scm2020_phy_reg[i].addr + j * sizeof(u32))) {
				printk("[PHY] %8p differs: 0x%08x vs. 0x%08x\n",
					(scm2020_phy_reg[i].addr + j * sizeof(u32)),
					scm2020_wlan_phy_regs_comp[i][j],
					readl(scm2020_phy_reg[i].addr + j * sizeof(u32)));
			}
		}
	}
#endif

#if 0                           /* Simulate RF malfunction after wake-up. */
	{
		uint16_t freq;
		struct ieee80211_channel *c = ieee80211_get_current_channel(sc->ic);

		freq = c->ic_freq;
		c->ic_freq = 2412;      /* whatever wrong freq. */
		scm2020_set_channel(sc->ic);
		c->ic_freq = freq;
	}
#endif

	wlan_pm_ctx->suspended = 0;

	return 0;
}


#ifndef CONFIG_IPC
__ilm_wlan__
#endif
int scm2020_wlan_ps_suspend(struct device *dev, u32 * duration)
{
	struct sc_softc *sc = dev->driver_data;
	struct ieee80211vap *vap;
	struct ieee80211vap *c_vap;
	struct ieee80211_node *ni;
	struct ieee80211_channel *ch;
	u8 *reserve_addr;
	struct ieee80211_key *k;
	int i;

	if ((sc->ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		return -EPERM;
	}

	/* 0. check WiFi mode & connection status */
	c_vap = NULL;
	TAILQ_FOREACH(vap, &sc->ic->ic_vaps, iv_next) {
		if (vap->iv_opmode == IEEE80211_M_STA && vap->iv_state >= IEEE80211_S_RUN) {
			c_vap = vap;
			break;
		}
	}
	if (c_vap == NULL) {
		pm_wifi_log("[PM WiFi] vap is null\n");
		return -EPERM;
	}

	/* WiFi sleep state checking */
	if (!sc->doze) {
		pm_wifi_log("[PM WiFi] not doze state\n");
		return -EPERM;
	}

	/* For backup storage of PM, reserve mbuf */
	reserve_addr = scm2020_reserve_backup_storage(sc, pm_reserved_buf_get_size());
	pm_reserved_buf_set_addr((u32)reserve_addr);
	if (reserve_addr) {
		pm_wifi_log("reserved buf addr: %p\n", reserve_addr);
	} else {
		return -EBUSY;
	}

	/* XXX: We can't just simply return any error from this pointer
	 *      cause current PM will not give us a chance
	 *      to make things right back in scm2020_wlan_resume.
	 */

	ni = c_vap->iv_bss;

	/* Backup HW context. */
	for (i = 0; i < WAON_HW_BAK_SIZE; i++) {
		if (waon_p(hw_ctx[i])->address)
			waon_p(hw_ctx[i])->value = readl(waon_p(hw_ctx[i])->address);
		else
			break;
	}

#ifdef CONFIG_PS_PM_VAP_SUPPORT
	wlan_pm_ctx->reg_vif1_uora_cfg1 = mac_readl(sc->dev, REG_VIF_UORA_CFG1(1));
	wlan_pm_ctx->reg_vif1_cfg1 = mac_readl(sc->dev, REG_VIF_CFG1(1));
	wlan_pm_ctx->reg_vif1_cfg0 = mac_readl(sc->dev, REG_VIF_CFG0(1));
	wlan_pm_ctx->reg_vif1_cfg_info = mac_readl(sc->dev, REG_VIF_CFG_INFO(1));
	wlan_pm_ctx->reg_vif1_rx_filter0 = mac_readl(sc->dev, REG_VIF_RX_FILTER0(1));
	wlan_pm_ctx->reg_vif1_rx_filter1 = mac_readl(sc->dev, REG_VIF_RX_FILTER1(1));
	wlan_pm_ctx->reg_vif1_bfm0 = mac_readl(sc->dev, REG_VIF_BFM0(1));
	wlan_pm_ctx->reg_vif1_bfm1 = mac_readl(sc->dev, REG_VIF_BFM1(1));
	wlan_pm_ctx->reg_vif1_min_mpdu_spacing = mac_readl(sc->dev, REG_VIF_MIN_MPDU_SPACING(1));
	/* VIF1 MAC Addr assign */
	wlan_pm_ctx->reg_vif1_macaddr_l = mac_readl(sc->dev, REG_VIF_MAC0_L(1));
	wlan_pm_ctx->reg_vif1_macaddr_h = mac_readl(sc->dev, REG_VIF_MAC0_H(1));
#endif

	/* for my mac addr restoring */
	IEEE80211_ADDR_COPY((u8 *)waon_p(ctx)->iv_myaddr, vap->iv_myaddr);

	/* Save DTIM Parser related MAC REG */
	ch = ieee80211_get_current_channel(c_vap->iv_ic);

	waon_p(ctx)->chanfreq = ieee80211_get_channel_center_freq1(ch);

	waon_p(ctx)->icflag = ch->ic_flags;
	waon_p(ctx)->icfreq = ch->ic_freq;

	if (ni->ni_flags & IEEE80211_NODE_NON_TRANS) {
		waon_p(ctx)->non_trans_node = 1;
		waon_p(ctx)->ni_bssid_indicator = ni->ni_bssid_indicator;
		waon_p(ctx)->ni_bssid_index = ni->ni_bssid_index;
		IEEE80211_ADDR_COPY((u8 *)waon_p(ctx)->tx_bssid, ni->ni_transmitter_bssid);
	} else {
		waon_p(ctx)->non_trans_node = 0;
		waon_p(ctx)->ni_bssid_indicator = 0;
		waon_p(ctx)->ni_bssid_index = 0;
		memset((u8 *)waon_p(ctx)->tx_bssid, 0, IEEE80211_ADDR_LEN);
	}
	IEEE80211_ADDR_COPY((u8 *)waon_p(ctx)->ni_bssid, ni->ni_macaddr);
	waon_p(ctx)->ni_associd = ni->ni_associd;
	waon_p(ctx)->nonqos_seqno = (ni->ni_txseqs[IEEE80211_NONQOS_TID]++) & (IEEE80211_SEQ_RANGE - 1);

	/* Key is saved every PS suspend */
	k = ieee80211_crypto_getucastkey(c_vap, ni);
	/* Restore the security key and reassign key of device */
	if (k != NULL) {
		if (waon_p(sec_info)->txcipher == IEEE80211_CIPHER_WEP) {
			waon_p(sec_info)->txkeytsc = ((struct wep_ctx *)k->wk_private)->wc_iv;
		} else {
			waon_p(sec_info)->txkeytsc = k->wk_keytsc;
		}
	}

	waon_p(ctx)->ni_intval = ni->ni_intval;

	/* Flush the last word written */
#define MAC_SHM_RX_LAST_BUF_OFFSET  0x00001098
	/* For flush the last cache word.
	 * write dummy value to Last RX desc(MAC_SHM_RX_LAST_BUF_SIZE) in mac rx retention mem
	 * (by writing an extra dummy word in other dummy addr. anyway), before going to deep sleep mode
	 */
	mac_writel(0xffffffff, sc->dev, MAC_SHM_OFFSET + MAC_SHM_RX_LAST_BUF_OFFSET);

	waon_p(ctx)->mcubin_addr = (u32)&mcu_bin;
	waon_p(ctx)->mcubin_size = mcu_bin_size;
	pm_wifi_log("wise mcubin_start(0x%08x) , size(0x%08x)\n", &mcu_bin, mcu_bin_size);

	scm2020_phy_off(sc);

	/* RF Current reduction flag in sleep */
	/* Expected SNR 15dB, because the HT MCS4 is 100% receivable. */
	scm2020_ps_reduce_rf_current(true);

	/* MAC suspend & RF will be controlled by RF Driver */

	pm_wifi_log("[PM WiFi] wlan suspend\n");

#if VERIFY_WLAN_MAC_REGS
	for (int i = 0; i < MAC_REG_SIZE / sizeof(u32); i++) {
		scm2020_wlan_mac_regs_comp[i] = readl(MAC_BASE_START + MAC_REG_OFFSET + i * sizeof(u32));
	}
#endif

#if VERIFY_WLAN_PHY_REGS
	uint32_t flags;

	local_irq_save(flags);
	for (int i = 0; i < PHY_REG_SEG_NUM; i++) {
		if (!scm2020_wlan_phy_regs_comp[i]) {
			scm2020_wlan_phy_regs_comp[i] = (u32 *)zalloc(scm2020_phy_reg[i].size);
		}
		for (int j = 0; j < scm2020_phy_reg[i].size / sizeof(u32); j++) {
			scm2020_wlan_phy_regs_comp[i][j] = readl(scm2020_phy_reg[i].addr + j * sizeof(u32));
		}
	}
	local_irq_restore(flags);
#endif

	wlan_pm_ctx->suspended = 1;

#if 0
	scm2020_phy_dump_stop(sc);
#endif

	return 0;

}

__iram__ void scm2020_chk_bcn_miss(void *data, int pending)
{
	struct sc_softc *sc = (struct sc_softc *) data;
	struct sc_vap *tvap;
	struct ieee80211com *ic;
	struct ieee80211vap *vap;
	struct ieee80211_node *ni;
	int bmiss_win;
#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
	struct tcp_pcb *mqtt_pcb;
#endif

	tvap = &sc->tvap[0];
	vap = &tvap->vap;
	ic = vap->iv_ic;
	ni = vap->iv_bss;

	/*
	 * Run beacon miss timer during the SLEEP state
	 *
	 * NB: we reused vap->iv_swbmiss, but with different callout function.
	 */
	if(vap->iv_opmode == IEEE80211_M_STA && ni->ni_associd != 0 && vap->iv_bss != NULL) {
		if (vap->iv_state == IEEE80211_S_RUN  || vap->iv_state == IEEE80211_S_SLEEP) {

			IEEE80211_LOCK(ic);
			bmiss_win = ieee80211_bmiss_calc_win_thres(vap);
			IEEE80211_UNLOCK(ic);

			vap->iv_swbmiss_period =
			IEEE80211_TU_TO_TICKS(2 * bmiss_win * ni->ni_intval);
			callout_reset(&vap->iv_swbmiss, vap->iv_swbmiss_period, ieee80211_swbmiss, vap);
#ifdef DEBUG_TBTT
			pm_wifi_log( "PS: awake (swbmiss_period %d)\n", vap->iv_swbmiss_period);
#endif
			if (g_sc_txkeep_intvl && !osTimerIsRunning(vap->tx_keeplive_timer))
				osTimerStart(vap->tx_keeplive_timer, msecs_to_ticks(g_sc_txkeep_intvl * 1000));
#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
			mqtt_pcb = (struct tcp_pcb *)vap->shared_mem;
			if (waon_p(ctx)->mqtt_sent_len) {
				mqtt_pcb->snd_lbb += waon_p(ctx)->mqtt_sent_len;
				mqtt_pcb->snd_nxt += waon_p(ctx)->mqtt_sent_len;
				mqtt_pcb->snd_wl1 += waon_p(ctx)->mqtt_sent_len;
				mqtt_pcb->snd_wl2 += waon_p(ctx)->mqtt_sent_len;
				mqtt_pcb->rcv_nxt += waon_p(ctx)->mqtt_sent_len;
				mqtt_pcb->lastack += waon_p(ctx)->mqtt_sent_len;
				waon_p(ctx)->mqtt_sent_len = 0;
			}
			if (pm_wakeup_get_type() == WIFI_BOOT_TX_MQTT_FAIL) {
				extern int mqtt_disconnect();
				pm_wakeup_rest_type();
				mqtt_disconnect();
			}
#endif
		}
	}
	/* Resume txq timeout timer unconditionally.
	 */
#ifdef CONFIG_MAC_TX_TIMEOUT
	osTimerStart(txq_timeout_timer, msecs_to_ticks(CONFIG_MAC_TX_TIMEOUT_VAL / 2));
#endif

	if (waon_p(ctx)->wifi_active_state) {
		ieee80211_send_nulldata(ieee80211_ref_node(ni));
		waon_p(ctx)->wifi_active_state = 0;
	}


}

typedef enum {
    WC_KEEPALIVE_MODE_NONE = 0,      /** keepalive mode disable */
    WC_KEEPALIVE_MODE_NULL,          /**keepalive by NULL frame */
    WC_KEEPALIVE_MODE_GARP,          /**keepalive by GARP frame */
    WC_KEEPALIVE_MODE_MQTT,          /**keepalive by MQTT frame */
    WC_KEEPALIVE_MODE_MAX
} watcher_keepalive_mode_t;

static void scm2020_watcher_set_keepalive(u8 mode, u32 intvl)
{
	waon_p(ctx)->sched_tx &= ~(WC_NULL_PKT|WC_GARP_PKT);

	if (mode == WC_KEEPALIVE_MODE_NULL)
		waon_p(ctx)->sched_tx |= WC_NULL_PKT;
	else if (mode == WC_KEEPALIVE_MODE_GARP)
		waon_p(ctx)->sched_tx |= WC_GARP_PKT;
	else if (mode == WC_KEEPALIVE_MODE_MQTT)
		waon_p(ctx)->sched_tx |= WC_MQTT_PKT;

	if (intvl != 0)
		waon_p(ctx)->itvl_keepalive = (intvl * 1000); /* sec */
	else
		waon_p(ctx)->itvl_keepalive = WC_KEEPALIVE_PERIODIC_TX_TIME;
}

static void scm2020_watcher_set_bcn_loss_chk(u8 enable)
{
	waon_p(ctx)->bcn_loss_chk = enable;
}

static void scm2020_watcher_set_port_filter(u8 enable)
{
	waon_p(ctx)->port_filter = enable;
}

static void scm2020_pm_waon_init(struct sc_softc *sc)
{
	for (int i = 0; i < WAON_HW_BAK_SIZE; i++) {
		waon_p(hw_ctx[i])->address = waon_hw_bak_addr[i];
		waon_p(hw_ctx[i])->value = 0;
	}

	memset((u8 *)waon_p(ctx), 0, sizeof(*waon_p(ctx)));

	waon_p(ctx)->port_filter = WC_PORT_FILTER;

	waon_p(ctx)->sched_tx |= WC_KEEPALIVE_NULL;
	waon_p(ctx)->sched_tx |= WC_KEEPALIVE_GARP;
	waon_p(ctx)->sched_tx |= WC_KEEPALIVE_MQTT;
	waon_p(ctx)->sched_tx |= WC_TX_UDPHOLE;
	waon_p(ctx)->sched_tx |= WC_TX_TCPHOLE;

	waon_p(ctx)->itvl_keepalive = WC_KEEPALIVE_PERIODIC_TX_TIME;
	waon_p(ctx)->itvl_udpa = WC_UDPHOLE_PERIODIC_TX_TIME;
	waon_p(ctx)->itvl_tcpa = WC_TCPHOLE_PERIODIC_TX_TIME;

	waon_p(ctx)->bcn_loss_chk = WC_BCN_LOSS_LAST_CHECK;

	/* mtable can be reuse for wc arp tx_buffer*/
	waon_p(ctx)->arp_tx_buf = (u32) sc->txq[SCM2020_WC_ARP_TX_AC].mtable;

	//waon_p(ctx)->log_flag = 0;  /* enabled by pm PM_FEATURE_WC_LOG */
	memset((void *) &waon_p(ctx)->counter, 0, sizeof(waon_p(ctx)->counter));

	waon_p(ctx)->bcnloss_threshold_time = WC_BCN_LOSS_TIME_S;
	waon_p(ctx)->fix_early_base = EARLY_BASE_DEFAUL;
	waon_p(ctx)->fix_ext_base = EXT_BASE_DEFAUL;
	waon_p(ctx)->fix_early_adj = EARLY_ADJ_DEFAUL;
	waon_p(ctx)->fix_ext_adj = EXT_ADJ_DEFAUL;
	waon_p(ctx)->rtc_cal_mode = WAON_RTC_CAL_MODE_TSF;


}

static void scm2020_pm_wlan_restore_ctx_init(struct sc_softc *sc)
{
	wlan_pm_ctx = zalloc(sizeof(*wlan_pm_ctx));
	if (!wlan_pm_ctx) {
		printk("No memory for wlan PM ctx!!!\n");
	}

	wlan_pm_ctx->sc = sc;
	memset(&wlan_pm_ctx->counter, 0, sizeof(wlan_pm_ctx->counter));
	wlan_pm_ctx->vap = NULL;

	/* Change PIT Timer as SYSTIMER of PM */
	wlan_pm_ctx->dtim_timer = device_get_by_name("systimer");
	if (wlan_pm_ctx->dtim_timer == NULL)
		printk("PM DTIM Timer failed!!!\n");
	{
		void *pit_isr_func;

		pit_isr_func = scm2020_dtim_awake;

#ifdef	PM_SOC_WAKEUP_TEST
		systimer_set_cmp_cb(wlan_pm_ctx->dtim_timer, &wlan_pm_ctx->wifi_pm_timer,
				SYSTIMER_TYPE_WAKEUP_FULL, pit_isr_func, NULL);
#else
		systimer_set_cmp_cb(wlan_pm_ctx->dtim_timer, &wlan_pm_ctx->wifi_pm_timer,
				SYSTIMER_TYPE_WAKEUP_IDLE, pit_isr_func, NULL);
#endif

	}

#ifdef CONFIG_WATCHER_FILTER_TIMESTAMP
	wlan_pm_ctx->rtc_dev = device_get_by_name("atcrtc");
	if (wlan_pm_ctx->rtc_dev == NULL)
		printk("PM RTC Timer failed!!!\n");
#endif
}

static void scm2020_pm_init(struct sc_softc *sc)
{
	scm2020_pm_wlan_restore_ctx_init(sc);

	scm2020_pm_waon_init(sc);

	/* watcher multicast filter table init */
	scm2020_ps_filter_init();

	scm2020_ps_pkt_info_init();

	/* If want to start PM with WiFi automatically, uncomment this */
	scm2020_watcher_onoff_flag = CONFIG_SCM2010_PM_WIFI_AUTO_RUN;

	printk("Wlan PM ctx init done\n");
}

/**
 * scm2020_dhcp_pmstay() - DHCP PM stay active callback
 * @ni: peer
 * @arg: not used
 * @status: transmission status (OK if 0)
 *
 * This function sets pm_staytimeout for DHCP recv timeout
 * only if the transmission success.
 */

#ifndef CONFIG_LWIP_TIMERS_ONDEMAND
/* If DHCP ONDEMAND timer is enabled and working, will be controlled DHCP Processing
 */
static void scm2020_dhcp_pmstay(struct ieee80211_node *ni, void *arg, int status)
{
	if (status == 0) {
		/*
		 * Need the stay timeout instead of pm_stay and pm_relax for
		 * 1. The app does the dhcp request but no reply [no reply can do pm_relax]
		 * 2. The app ifconfig the static IP
		 */
		pm_staytimeout(DHCP_RECV_TIMEOUT_MS);
	}
}
#endif

static void scm2020_ps_make_garp(struct ieee80211_node *ni, void *arg, int status)
{
	if (status == 0) {
		scm2020_ps_garp_save_pkt(ni->ni_vap);
	}
}

__maybe_unused static bool scm2020_ps_target_our_network(unsigned long destip)
{
	u32 netmask = DEV_OUR_SUBNET_MASK;  // network ip subnet mask
	u32 netstart;
	u32 netend;
	u32 ipaddr = 0;

	/* If ip address is not assigned, do not transmit GARP */
	ipaddr = scm2020_ps_get_net_ip();
	if (!ipaddr) {
		/* "GARP Packet : failed by IP address(0) */
		return false;
	}

	netstart = ipaddr & netmask;
	netend = (netstart | ~netmask);

	if ((destip >= netstart) && destip <= netend)
		return true;
	return false;
}

/* In local network (same subnet), for demo, enable this feature */
/* This feature should be removed , nat using packets(tcp/udp) is used for Hole punch */
/* #define CONFIG_PM_HOLE_PUNCH_DEMO_TEST */

/* check Packet destination is not our network */
/* PM Packet Paring for Watcher working */
__ilm_wlan_tx__ void scm2020_tx_ps_packet(struct ifnet *ifp)
{
	struct mbuf *m;
	struct ifqueue *snd_queue = (struct ifqueue *) &ifp->if_snd;

	/* After the running state,
	 * there should be no high priority task/interrupt to interrupt the data path.
	 */
	m = snd_queue->ifq_tail;
	if (!m)
		return;

	const struct udp_hdr *udp = NULL;

	/* net80211 didn't do encapsulation because we said so. */
	const struct ether_header *eh = mtod(m, struct ether_header *);

	if (eh->ether_type == htons(ETHERTYPE_IP)) {
		const struct ip_hdr *ipv4 = IPv4BUF;
		u16 iphdr_hlen;

		/* Parsing multicast packet, for registering Multicast filter */
		if (!IEEE80211_IS_BROADCAST(eh->ether_dhost) && IEEE80211_IS_MULTICAST(eh->ether_dhost)) {
			if (ETHER_IS_FILTER_MC(eh->ether_dhost)) {
				unsigned long destipaddr;

				destipaddr = ipv4->dest.addr;

				/* get the ip addr and register multicast ip addr & let's register multicast leave address(224.0.0.2) */
				if (ETHER_IS_MULTICAST_ADDR(destipaddr) && !ETHER_IS_MULTICAST_LEAVE(destipaddr)) {
					scm2020_ps_reg_filter(FILTER_MC, swap_ipaddr(destipaddr));
				}
			}
			/* If Not BC and just MC, no additional checking, just return */
			return;
		}
		iphdr_hlen = IPH_HL_BYTES(ipv4);
		/* Parsing UDP Packet, for registering UDP filter and Hole Punch packet */
		if (ipv4->_proto == IP_PROTO_UDP) {
			udp = UDPIPv4BUF(iphdr_hlen);

			if (ipv4->dest.addr != IPADDR_BROADCAST && !ETHER_IS_MULTICAST_ADDR(ipv4->dest.addr)) {
				/* Parsing UDP Packet, for registering UDP filter */
				scm2020_ps_reg_filter(FILTER_UDP, swap_portnum(udp->src));

#ifdef CONFIG_PM_HOLE_PUNCH
#ifndef CONFIG_PM_HOLE_PUNCH_DEMO_TEST
				/* dest ip address is not our network */
				if (scm2020_ps_target_our_network(ipv4->dest.addr))
#endif
					/* saved to reserved area of retention mem. (Just UDP header & No data) */
					scm2020_ps_make_udp_hole(m, ipv4->dest.addr, udp->src, iphdr_hlen + sizeof(struct udp_hdr));
#endif
				/* If UDP and Not BC/MC, no additional checking, just return */
				return;
			}
			/* Continue; for checking DHCP Discover and Request */
		}
		/* Parsing TCP Packet, for registering TCP filter and Hole Punch packet */
		else if (ipv4->_proto == IP_PROTO_TCP) {
			bool can_be_reg_port = false;
			u16 iphdr_hlen = IPH_HL_BYTES(ipv4);
			const struct tcp_hdr *tcp = TCPIPv4BUF(iphdr_hlen);

			/* TCP is a protocol for communication between exactly two endpoints */
			/* So, don't need to check BC/MC */
#ifdef CONFIG_PM_HOLE_PUNCH
			u8 tcp_flags;

#ifndef CONFIG_PM_HOLE_PUNCH_DEMO_TEST
			/* dest ip address is not our network */
			if (scm2020_ps_target_our_network(ipv4->dest.addr)) {
				/* saved to reserved area of retention mem. (Just TCP header & No data) */
#endif
				tcp_flags = TCPH_FLAGS(tcp);

				/* If not TCP Ack or not FIN, pass */
				if ((tcp_flags & TCP_ACK) == 0 || (tcp_flags & TCP_FIN) == 0)
					return;

				/* Only TCP Ack will be copied */
				//else if ((tcp_flags & TCP_ACK) == TCP_ACK && (tcp_flags & TCP_FIN) != TCP_FIN)         {
				if (tcp_flags == TCP_ACK) {
					scm2020_ps_make_tcp_hole(m, ipv4->dest.addr, tcp->src, iphdr_hlen + sizeof(struct tcp_hdr));
					can_be_reg_port = true;
				} else if ((tcp_flags & TCP_ACK_PSH_FLAG) == TCP_ACK_PSH_FLAG) {
					/* We have to add seq number calculation */
					/* Fix me lator */
					scm2020_ps_update_tcp_hole(m, ipv4->dest.addr, tcp->src, tcp->seqno,
									iphdr_hlen + sizeof(struct tcp_hdr));
					can_be_reg_port = true;
				}
				/* TCP Close case */
				else if (tcp_flags & TCP_FIN) {
#ifndef CONFIG_PM_HOLE_PUNCH_DEMO_TEST
					/* We have to add removing TCP hole punch packet */
					scm2020_ps_clear_tcp_hole(ipv4->dest.addr, tcp->src);
#endif
					can_be_reg_port = false;
				}
				/* TCP Reset case */
				else if ((tcp_flags & TCP_ACK_RST_FLAG) == TCP_ACK_RST_FLAG) {
#ifndef CONFIG_PM_HOLE_PUNCH_DEMO_TEST
					/* We have to add removing TCP hole punch packet */
					scm2020_ps_clear_tcp_hole(ipv4->dest.addr, tcp->src);
#endif
					can_be_reg_port = false;
				}
#ifndef CONFIG_PM_HOLE_PUNCH_DEMO_TEST
			}
#endif
#else
			can_be_reg_port = true;
#endif
			/* Get the TCP source port, for registering TCP filter */
			if (can_be_reg_port)
				scm2020_ps_reg_filter(FILTER_TCP, swap_portnum(tcp->src));

			/* If TCP, no additional checking, just return */
			return;
		}
	} else if (eh->ether_type == htons(ETHERTYPE_ARP)) {
		char *tempptr;

		/* for getting my ip address */

		tempptr = (char *) (ETHER_HDR_LEN + m->m_data);
		/* hw type & protocol type & op code (Req or Rep) */
		if (ETHER_IS_ARP_REQ_OR_REP(tempptr)) {

			/* get and register the sender ip address of ARP */
			scm2020_ps_set_net_ip(getulongip(tempptr));

			ieee80211_add_callback(m, scm2020_ps_make_garp, (void *)m);
		}
		/* If ARP, no additional checking, just return */
		return;
	}
#if LWIP_IPV6
	else if (eh->ether_type == htons(ETHERTYPE_IPV6)) {
		const struct ip6_hdr *ipv6 = IPv6BUF;

		if ((ipv6->_nexth == IP_PROTO_UDP) && ip6_addr_ismulticast(&ipv6->dest))
			udp = UDPIPv6BUF;
	}
#endif
	/* Check DHCP Discover or Request */
#ifndef CONFIG_LWIP_TIMERS_ONDEMAND
	if ((udp != NULL) && (udp->src == htons(DHCPC_CLIENT_PORT)) && (udp->dest == htons(DHCPC_SERVER_PORT))) {
		ieee80211_add_callback(m, scm2020_dhcp_pmstay, m);
	}
#endif

	return;
}

void scm2020_ps_save_info_flag(void)
{
	watcher_save_info_flag = true;
}


/**
 * scm2020_sta_ps_rx() - handle received data frames for PS
 *
 * @ni: peer node, which corresponds to the associated AP
 * @mh: rx frame
 */
void
scm2020_sta_ps_rx(struct ieee80211_node *ni, struct ieee80211_frame *mh)
{

	struct ieee80211vap *vap = ni->ni_vap;

	/* PM Bit should be set */
	if (!(vap->iv_flags & IEEE80211_F_PMGTON))
		return;

	/* Only Station mode */
	if (vap->iv_opmode != IEEE80211_M_STA)
		return;

	/* It should be same BSS */
	if (vap->iv_bss != ni) {
		printk("%s: vap->iv_bss != ni\n", __func__);
		return;
	}

	/* Power Management should be supported and current is Sleep state */
	if (!((ni->ni_flags & IEEE80211_NODE_PWR_MGT) && (vap->iv_state == IEEE80211_S_SLEEP) && (vap2tvap(vap)->ps))) {
		return;
	}

	/* will process only Data */
	if ((mh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_DATA)
		return;

	/*
	 * If (1) the current beacon is DTIM, (2) TIM
	 * does not include me, and (3) @mh
	 * is the unicast frame from the serving AP (not
	 * necessarily toward me), we can enter doze mode.
	 */
	if (ni->ni_dtim_count != 0 || !ni->ni_dtim_mcast)
		return;

	/* Source is not BSSID , discard it */
	if (!IEEE80211_ADDR_EQ(mh->i_addr2, ni->ni_bssid)) {
		//printk("%s: OBSS\n", __func__);
		return;
	}

	if (!IEEE80211_IS_MULTICAST(mh->i_addr1))
		return;

	if (mh->i_fc[1] & IEEE80211_FC1_MORE_DATA)
		return;

	/*
	 * We just received the last BC/MC data frame from the
	 * serviing AP in DTIM which means there is no buffered BU.
	 * Now we can enter doze mode
	 */
#ifdef CONFIG_NET80211_PS_POLL
	{
		struct ieee80211com *ic = vap->iv_ic;

		if (!ic->ic_ps_poll_flag)
			scm2020_sta_doze(vap, 0);
	}
#endif
	return;
}

static void
_scm2020_sta_pwrsave(struct ieee80211_node *ni, void *arg, int status);

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(scm2020_sta_pwrsave, &scm2020_sta_pwrsave, &_scm2020_sta_pwrsave);
#else
__func_tab__ void
(*scm2020_sta_pwrsave)(struct ieee80211_node * ni, void *arg, int status) = _scm2020_sta_pwrsave;
#endif

/**
 * scm2020_sta_pwrsave() - (QOS-)NULL frame tx completion callback
 * @ni: peer
 * @arg: not used
 * @status: transmission status (OK if 0)
 *
 * This function sets the ps field of vap that talks to @ni if the
 * transmission of the (QOS-)NULL frame that it sent to notify the
 * power management status to the associated AP.
 *
 * The net80211 changes the vap state into S_SLEEP or S_RUN without
 * seeing the result of NULL frame transmission. This causes
 * inconsistency between the stack and the driver, and between the AP and STA
 * before that frame is actually acknowledged, or if the transmission fails.
 *
 * tvap ps == 1 indicates the vap is actually granted for power save mode, and
 * can start doze/awake alternation.
 */
static void
_scm2020_sta_pwrsave(struct ieee80211_node *ni, void *arg, int status)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct mbuf *m = (struct mbuf *)arg;
	struct hw_tx_hdr *hw = hw(m);
	struct ieee80211_frame *wh = hw->mh;
	struct tx_info *ti = ti(m);
	int pm;
	u32 ni_flags;

#ifdef CONFIG_SUPPORT_TWT
	struct ieee80211com *ic = ni->ni_ic;
	struct sc_softc *sc = (struct sc_softc *)ic->ic_softc;
	struct device *dev = sc->dev;
	struct sc_vap *tvap = vap2tvap(vap);
	int vif = tvap->index;
	u32 v;
#endif

	/* NB: In the tx_reclaim, it always sets status=0
	 * TIM notify -> NULL frame[notify power management status to AP]
	 * If tx fails, AP keeps sending TIM notify and then send deauth
	 */
	status = !(ti->sent && ti->acked);
	pm = (wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) ? 1 : 0;
	if (status == 0) {
		vap2tvap(vap)->ps = pm;
		IEEE80211_NOTE(vap,
					IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
					ni, "%s mode granted",
					pm ? "PS" : "AM");

#ifdef CONFIG_SUPPORT_TWT
		if (vap->iv_he_twt_info.i_twt_params.twt_trigger) {
			v = mac_readl(dev, REG_VIF_MIN_MPDU_SPACING(vif));
			if (pm)
				bfclr_m(v, VIF0_MIN_MPDU_SPACING, TRIG_EN);
			else
				bfmod_m(v, VIF0_MIN_MPDU_SPACING, TRIG_EN, 0x1);

			mac_writel(v, dev, REG_VIF_MIN_MPDU_SPACING(vif));
		}
#endif
		AP_ALIVE(vap);
	} else if (vap->iv_state == IEEE80211_S_RUN || vap->iv_state == IEEE80211_S_SLEEP) {
		/* (QOS-)Null frame transmission failed. We simply send again */
		/* XXX but indefinitely? */
		/* here we should use ic lock to protect the process as other threads do, otherwise all the protection would be out of work */
		IEEE80211_LOCK(ic);
		ni_flags = ni->ni_flags;
		if (pm)
			ni->ni_flags |= IEEE80211_NODE_PWR_MGT;
		else
			ni->ni_flags &= ~IEEE80211_NODE_PWR_MGT;
		ieee80211_send_nulldata(ieee80211_ref_node(ni));
		ni->ni_flags = ni_flags;
		IEEE80211_UNLOCK(ic);
	}

	/* Make sure being in sync with AP */
	if (pm)
		vap2tvap(vap)->ps = (status == 0);
}

void scm2020_watcher_tsf_snap(u32 tsf)
{
	waon_p(ctx)->tsf = tsf;
	waon_p(ctx)->rtc = SYS_TIMER_RTC_COUNT;
}

struct wlan_pm_ops *get_wlan_pm_ops(void)
{
	return &scm2020_wlan_pm_ops;
}


void scm2020_wlan_pm_attach(struct sc_softc *sc)
{
	scm2020_wlan_pm_ops.init = scm2020_pm_init;
	scm2020_wlan_pm_ops.sta_rx_ps_frame = scm2020_sta_ps_rx;
	scm2020_wlan_pm_ops.sta_ps_handler = _scm2020_sta_pwrsave;
	scm2020_wlan_pm_ops.sta_ps_bmiss_handler = scm2020_chk_bcn_miss;
	scm2020_wlan_pm_ops.sta_tx_ps_pkts = scm2020_tx_ps_packet;
	scm2020_wlan_pm_ops.sta_update_ps_info = scm2020_ps_save_info_flag;
	scm2020_wlan_pm_ops.sta_doze = scm2020_ps_doze;
	scm2020_wlan_pm_ops.sta_awake = scm2020_ps_awake;
	scm2020_wlan_pm_ops.wlan_resume = scm2020_wlan_ps_resume;
	scm2020_wlan_pm_ops.wlan_suspend = scm2020_wlan_ps_suspend;
	scm2020_wlan_pm_ops.wc_keepalive = scm2020_watcher_set_keepalive;
	scm2020_wlan_pm_ops.wc_bcn_loss_chk = scm2020_watcher_set_bcn_loss_chk;
	scm2020_wlan_pm_ops.wc_port_filter = scm2020_watcher_set_port_filter;
	scm2020_wlan_pm_ops.wc_tsf_snap = scm2020_watcher_tsf_snap;

	scm2020_wlan_pm_init(sc);
}

#if defined(CONFIG_CMDLINE)

#include <cli.h>

/* watcher on 0/1 */
int scm2020_watcher_on(int argc, char *argv[])
{
	if (argc != 2) {
		printf("\nUsage: watcher on [0/1]\n0 (disable), 1 (enable) \n");
		return CMD_RET_USAGE;
	}

	int onoff = atoi(argv[1]);

	if (onoff) {
		if (scm2020_watcher_make_txpkt()) {
			/* In case of connection, enter sleep and flag enable */
			scm2020_watcher_onoff_flag = true;
		}
	} else {
		printf("\nWatcher Run Off\n");
		scm2020_watcher_onoff_flag = false;
	}

	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_cnt_status(int clear)
{
	int bcn_loss_rate = 0;

	if (waon_p(ctx)->counter.bcn_rx || waon_p(ctx)->counter.bcn_loss)
		bcn_loss_rate = (100 * waon_p(ctx)->counter.bcn_loss) /
		(waon_p(ctx)->counter.bcn_rx + waon_p(ctx)->counter.bcn_loss);

	printf("\nWatcher PM Status\n");

	if ((waon_p(ctx)->counter.bcn_rx - waon_p(ctx)->counter.uc_rx > 0))
		printf("WC wait bcn avg : %d us\n", (waon_p(ctx)->counter.wc_wk_time_us / (waon_p(ctx)->counter.bcn_rx - waon_p(ctx)->counter.uc_rx)));
	printf("WC RX   count : %d\n", waon_p(ctx)->counter.bcn_rx);
	printf("WC Loss count : %d (%d%%)\n", waon_p(ctx)->counter.bcn_loss, bcn_loss_rate);
	printf("WC Loss FCS nok : %d\n", waon_p(ctx)->counter.bcn_loss_fcs_nok);

	printf("WC BC/MC Count: %d\n", waon_p(ctx)->counter.bcmc_rx);
	printf("WC UC    Count: %d\n", waon_p(ctx)->counter.uc_rx);
	printf("WC AP KA Count: %d\n", waon_p(ctx)->counter.ap_ka);
	printf("Full WK  Count: %llu\n", wlan_pm_ctx->counter.fullwakeup);
	printf("Full WK By RTC: %llu\n", wlan_pm_ctx->counter.wakeupsrc_rtc);
	printf("Full WK By EXT: %llu\n", wlan_pm_ctx->counter.wakeupsrc_ext);

	printf("drift plus: %d\n", waon_p(ctx)->counter.driftplus);
	printf("drift minus: %d\n", waon_p(ctx)->counter.driftminus);

	for (int i = 0; i < 10; i++) {
		if (waon_p(ctx)->counter.count[i]) {
			printf("         Window LV[%d]: %d, %d\n", i+1, waon_p(ctx)->counter.count[i], waon_p(ctx)->counter.window[i]);
		}
	}

	for (int i = 0; i < ARRAY_SIZE(wlan_pm_ctx->counter.wakeup_reasons); i++) {
		if (wlan_pm_ctx->counter.wakeup_reasons[i]) {
			printf("         FB[%d]: %llu\n", i, wlan_pm_ctx->counter.wakeup_reasons[i]);
		}
	}

	for (int i = 0; i < WIFI_BOOT_TYPE_MAX; i++) {
		for (int j = 0; j < WIFI_BOOT_SUBTYPE_MAX; j++) {
			if (wlan_pm_ctx->counter.wakeup_types_wifi[i][j]) {
				printf("         FT[%d][%d]: %llu\n", i, j, wlan_pm_ctx->counter.wakeup_types_wifi[i][j]);
			}
		}
	}

	if (clear) {
		memset((void *) &waon_p(ctx)->counter, 0, sizeof(waon_p(ctx)->counter));
		memset(&wlan_pm_ctx->counter, 0, sizeof(wlan_pm_ctx->counter));
	}
	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_filter_status(void)
{
	int index;
	unsigned long mc_addr;
	unsigned short port;

	printf("\nWatcher Reg Multicast Addr :: %d count\n", waon_get_tot_reg(FILTER_MC));
	if (waon_get_tot_reg(FILTER_MC)) {
		for (index = 0; index < scm2020_ps_get_filter_size(FILTER_MC); index++) {
			mc_addr = scm2020_ps_get_filter(FILTER_MC, index);
			if (mc_addr != 0) {
				printf("%d mc_addr(0x%08x)\n", index, mc_addr);
			}
		}
	}

	printf("\nWatcher Reg TCP Port list :: %d count\n", waon_get_tot_reg(FILTER_TCP));
	if (waon_get_tot_reg(FILTER_TCP)) {
		for (index = 0; index < scm2020_ps_get_filter_size(FILTER_TCP); index++) {
			port = scm2020_ps_get_filter(FILTER_TCP, index);
			if (port != 0)
				printf("%d TCP Port (0x%04x)\n", index, port);
		}
	}

	printf("\nWatcher Reg UDP Port list :: %d count\n", waon_get_tot_reg(FILTER_UDP));
	if (waon_get_tot_reg(FILTER_UDP)) {
		for (index = 0; index < scm2020_ps_get_filter_size(FILTER_UDP); index++) {
			port = scm2020_ps_get_filter(FILTER_UDP, index);
			if (port != 0)
				printf("%d UDP Port (0x%04x)\n", index, port);
		}
	}

	return CMD_RET_SUCCESS;
}

static int scm2020_bcn_status(int clear)
{
	struct sc_softc *sc = wlan_pm_ctx->sc;
	struct sc_vap *tvap = sc->tvap;
	u64 temp;
	u32 percentage;

	temp = (u64)tvap->bcn_counter.miss * 100;
	percentage = (u32)(temp / tvap->bcn_counter.total);

	if (clear)
		memset(&tvap->bcn_counter, 0, sizeof(tvap->bcn_counter));

	printf("miss:%d  total_b:%d %d%% \n", tvap->bcn_counter.miss, tvap->bcn_counter.total,
		percentage);

	return CMD_RET_SUCCESS;
}

/* watcher status rx/filter */
static int scm2020_watcher_status(int argc, char *argv[])
{

	int clear = 0;

	if (argc < 2 || argc > 3) {
		return CMD_RET_USAGE;
	}

	if (argc == 3){
		if (!strcmp(argv[2], "c")) {
			clear = 1;
		}
	}
	if (!strcmp(argv[1], "pm")) {
		return scm2020_watcher_cnt_status(clear);
	} else if (!strcmp(argv[1], "filter")) {
		return scm2020_watcher_filter_status();
	} else if (!strcmp(argv[1], "bcn")) {
		return scm2020_bcn_status(clear);
	}else {
		printf("\nnot support %s\n", argv[1]);
		return CMD_RET_UNHANDLED;
	}


	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_set_wtime(int lead_time)
{
	if (lead_time > 10000) {
		printf("Wakeup Leadtime(%dus) should be under 10ms\n", lead_time);
		return CMD_RET_FAILURE;
	}
	printf("\nWatcher set Wakeup Leadtime as %dus\n", lead_time);
	waon_p(ctx)->wakeup_leadtime = lead_time;

	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_set_uctimeout(int uc_timeout_time)
{
	if (uc_timeout_time < 10 || uc_timeout_time > 50) {
		printf("UC Timeout(%dus) should have rage (10 ~ 50)ms\n", uc_timeout_time);
		return CMD_RET_FAILURE;
	}

	printf("\nWatcher UC Timeout as %dus\n", uc_timeout_time);
	waon_p(ctx)->uc_rx_timeout = uc_timeout_time;

	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_set_bcnloss(int bcnloss_time)
{
	if (bcnloss_time != 0 && (bcnloss_time < 1 || bcnloss_time > 10)) {
		printf("BCN loss threshold time (%d) should have rage (2 ~ 10)\n", bcnloss_time);
		return CMD_RET_FAILURE;
	}

	printf("\nWatcher BCN Loss threshold time as %d\n", bcnloss_time);
	if (!bcnloss_time) {
		waon_p(ctx)->bcnloss_threshold_time = BCNLOSS_FOREVER;
		printf("\nSet 0 is Skip BCN Loss in watcher\n");
	}
	else
		waon_p(ctx)->bcnloss_threshold_time = bcnloss_time;

	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_set_ignore_bcmc(int ignore_bcmc)
{
	waon_p(ctx)->ignore_bcmc = ignore_bcmc;
	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_set_ignore_uc(int ignore_uc)
{
	waon_p(ctx)->ignore_uc = ignore_uc;
	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_set_early_base(int early_base)
{
	if(early_base >= 0 && early_base <= EARLY_BASE_DEFAUL)
		waon_p(ctx)->fix_early_base = early_base;
	else
		return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_set_ext_base(int ext_base)
{
	if(ext_base >= 0 && ext_base <= EXT_BASE_DEFAUL)
		waon_p(ctx)->fix_ext_base = ext_base;
	else
		return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_set_early_adj(int early_adj)
{
	if(early_adj >= 0 && early_adj <= EARLY_ADJ_DEFAUL)
		waon_p(ctx)->fix_early_adj = early_adj;
	else
		return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_set_ext_adj(int ext_adj)
{
	if(ext_adj >= 0 && ext_adj <= EXT_ADJ_DEFAUL)
		waon_p(ctx)->fix_ext_adj = ext_adj;
	else
		return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_set_wk_offset(int offset)
{
	if(offset >= -10000 && offset <= 10000)
		waon_p(ctx)->wk_offset_us = offset;
	else
		return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_set_advlp(int en)
{

	waon_p(ctx)->adv_lp = en;

	return CMD_RET_SUCCESS;
}

/* watcher set wtime/uctimeout/bcnloss/ignore_bcmc */
int scm2020_watcher_set_parameter(int argc, char *argv[])
{
	if (argc != 3) {
		printf("\nUsage: watcher set"
				OR "wtime (0 ~ 10000)"
				OR "uctimeout (10 ~ 50)"
				OR "bcnloss (5 ~ 50)"
				OR "ignore_bcmc (0/1) \n"
				OR "early_base 0~254, 255 to clean (unit 500 us)\n"
				OR "ext_base 0~254, 255 to clean (unit 1 ms)\n"
				OR "early_adj 0~254, 255 to clean (unit 500)\n"
				OR "ext_adj 0~254, 255 to clean (unit 1 ms)\n"
				OR "wk_offset -10000~10000, max -10~+10 ms offset for wake up time shift"
				OR "advlp (0/1)\n");
		return CMD_RET_USAGE;
	}

	if (!strcmp(argv[1], "wtime")) /* get beacon rx status */
		return scm2020_watcher_set_wtime(atoi(argv[2]));
	else if (!strcmp(argv[1], "uctimeout"))
		return scm2020_watcher_set_uctimeout(atoi(argv[2]));
	else if (!strcmp(argv[1], "bcnloss"))
		return scm2020_watcher_set_bcnloss(atoi(argv[2]));
	else if (!strcmp(argv[1], "ignore_bcmc"))
		return scm2020_watcher_set_ignore_bcmc(atoi(argv[2]));
	else if (!strcmp(argv[1], "ignore_uc"))
		return scm2020_watcher_set_ignore_uc(atoi(argv[2]));
	else if (!strcmp(argv[1], "early_base"))
		return scm2020_watcher_set_early_base(atoi(argv[2]));
	else if (!strcmp(argv[1], "ext_base"))
		return scm2020_watcher_set_ext_base(atoi(argv[2]));
	else if (!strcmp(argv[1], "early_adj"))
		return scm2020_watcher_set_early_adj(atoi(argv[2]));
	else if (!strcmp(argv[1], "ext_adj"))
		return scm2020_watcher_set_ext_adj(atoi(argv[2]));
	else if (!strcmp(argv[1], "wk_offset"))
		return scm2020_watcher_set_wk_offset(atoi(argv[2]));
	else if (!strcmp(argv[1], "advlp"))
		return scm2020_watcher_set_advlp(atoi(argv[2]));
	else {
		printf("\nnot support %s\n", argv[1]);
		return CMD_RET_UNHANDLED;
	}

	return CMD_RET_SUCCESS;
}

#ifdef WATCHER_HP
static int scm2020_watcher_hp_tcp(u8 enable, u8 intvl)
{
	if (enable)
		waon_p(ctx)->sched_tx |= WC_TCP_HOLE_PKT;
	else
		waon_p(ctx)->sched_tx &= ~(WC_TCP_HOLE_PKT);

	if (intvl != 0)
		waon_p(ctx)->itvl_tcpa = (intvl * MINUTE); /* min */
	else
		waon_p(ctx)->itvl_tcpa = WC_TCPHOLE_PERIODIC_TX_TIME;

	return CMD_RET_SUCCESS;
}

static int scm2020_watcher_hp_udp(u8 enable, u8 intvl)
{
	if (enable)
		waon_p(ctx)->sched_tx |= WC_UDP_HOLE_PKT;
	else
		waon_p(ctx)->sched_tx &= ~(WC_UDP_HOLE_PKT);

	if (intvl != 0)
		waon_p(ctx)->itvl_udpa = (intvl * MINUTE); /* min */
	else
		waon_p(ctx)->itvl_udpa = WC_UDPHOLE_PERIODIC_TX_TIME;

	return CMD_RET_SUCCESS;
}

/* watcher set tcp/udp hole punch parameter */
int scm2020_watcher_holepunch_parameter(int argc, char *argv[])
{
	u8 enable;
	u8 intvl = 0;

	if (argc < 3) {
		printf("\nUsage: watcher hp <tcp/udp> <0/1> [interval]"
				OR "interval: (1 ~ 3 mins)\n");
		return CMD_RET_USAGE;
	}

	enable = atoi(argv[2]);

	if (argc == 4)
		intvl = atoi(argv[3]);

	if (!strcmp(argv[1], "tcp")) /* set tcp hole punch */
		return scm2020_watcher_hp_tcp(enable, intvl);

	if (!strcmp(argv[1], "udp")) /* set udp hole punch */
		return scm2020_watcher_hp_udp(enable, intvl);

	return CMD_RET_SUCCESS;
}
#endif

static const struct cli_cmd watcher_cmd[] = {
	CMDENTRY(on, scm2020_watcher_on, "", ""),
	CMDENTRY(status, scm2020_watcher_status, "", ""),
	CMDENTRY(set, scm2020_watcher_set_parameter, "", ""),
#ifdef WATCHER_HP
	CMDENTRY(hp, scm2020_watcher_holepunch_parameter, "", ""),
#endif
};

static int do_watcher(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], watcher_cmd, ARRAY_SIZE(watcher_cmd));
	if (cmd == NULL) {
		return CMD_RET_USAGE;
	}

	return cmd->handler(argc, argv);
}

CMD(watcher, do_watcher,
	"CLI commands for WIFI PM",
	"\n watcher on <0 / 1>"
	OR "watcher status <pm / filter / bcn>"
	OR "watcher set <wtime / uctimeout / bcnloss / ignore_bcmc / ignore_uc / advlp>"
	OR "watcher set <early_base / ext_base / early_adj / ext_adj> (unit: early 500 us, ext 1000 us)"
	OR "watcher hp <tcp/udp> <0/1> [interval]");
#endif

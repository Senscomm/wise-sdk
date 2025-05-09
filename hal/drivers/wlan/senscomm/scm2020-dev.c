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
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <soc.h>
#include <wlan.h>
#include <hal/arm/asm/barriers.h>
#include <hal/kernel.h>
#include <hal/console.h>
#include <hal/wlan.h>
#include "hal/efuse.h"
#include <hal/kmem.h>
#include <hal/dma.h>
#include "compat_param.h"

#include "mbuf.h"
#include "kernel.h"
#include "atomic.h"
#include "systm.h"
#include "malloc.h"
#include "mutex.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include "compat_if.h"
#include "if_arp.h"
#include "if_llc.h"
#include "ethernet.h"
#include "if_dl.h"
#include "if_media.h"
#include "compat_if_types.h"
#include "if_ether.h"
#include "lwip-glue.h"
#include "route.h"

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_ratectl.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_proto_wise.h>
#ifdef CONFIG_SUPPORT_HE
#include <net80211/ieee80211_he.h>
#endif

#include "scm2020_var.h"
#include "scm2020_regs.h"
#include "scm2020_shmem.h"
#include "scm2020_pm.h"

#include <hal/device.h>
#include <hal/timer.h>
#include <hal/unaligned.h>
#include <hal/compiler.h>
#include <hal/bitops.h>
#include <hal/irq.h>
#include <hal/rf.h>
#include <stdlib.h>

#include "sys/ioctl.h"
#include "vfs.h"

/* Sanity checks */
#if (CONFIG_MEMP_NUM_MBUF_CACHE <= (1 << CONFIG_RX_BUF_NUM_LOG2))
#error "CONFIG_MEMP_NUM_MBUF_CACHE shall be greater than 2^(CONFIG_RX_BUF_NUM_LOG2)"
#endif

/* XXX: MAC_TX_TIMEOUT can be disabled at runtime
 *      to prevent unnecessary timer operations
 *      from occurring in IRQ handlers.
 *
 *      We can't just disable it by Kconfig
 *      because the ROM library has already defined it.
 */

#ifdef CONFIG_SCM2010_DISABLE_TX_TIMEOUT
#define DISABLE_MAC_TX_TIMEOUT
#else
#undef DISABLE_MAC_TX_TIMEOUT
#endif
//#define VERIFY_DTIM_PARSER

//#define __fib_v1__

extern struct ieee80211_regdomain regdomain;
extern void (*scm2020_enable_mu_edca)(struct sc_tx_queue *txq);
extern void netif_ifattach(struct ifnet *ifp);
extern void netif_ifdetach(struct ifnet *ifp);

int if_transmit(struct ifnet *ifp, struct mbuf *m);

extern const u8  mcu_bin[];
extern const u32 mcu_bin_size;

__dram__ __dconst__
static  struct ieee80211_ic_ht_cap scm2020_ht_cap[] = {
	{
		.opmode= IEEE80211_M_STA,
		.ic_htcaps = (IEEE80211_HTC_HT 		|
#ifdef CONFIG_SUPPORT_AMPDU_TX
				IEEE80211_HTC_AMPDU 		|
#endif
				IEEE80211_HTCAP_SHORTGI20 	|
#ifdef CONFIG_SUPPORT_CHWIDTH40
				IEEE80211_HTCAP_CHWIDTH40 	|
				IEEE80211_HTCAP_DSSSCCK40	|
				IEEE80211_HTCAP_SHORTGI40 	|
#endif
				IEEE80211_HTCAP_SMPS_OFF	|
#ifdef CONFIG_SUPPORT_STBC
				IEEE80211_HTCAP_TXSTBC 		|
				IEEE80211_HTCAP_RXSTBC_2STREAM	|
#endif
#ifdef CONFIG_SUPPORT_LDPC
				IEEE80211_HTCAP_LDPC 		|
				IEEE80211_HTC_TXLDPC		|
#endif
				0),
		.ic_htextcaps = 0,	/* HT extended capabilities */
	},
	{
		.opmode= IEEE80211_M_HOSTAP,
		.ic_htcaps = (IEEE80211_HTC_HT 		|
#ifdef CONFIG_SUPPORT_AMPDU_TX
				IEEE80211_HTC_AMPDU 		|
#endif
				IEEE80211_HTCAP_SHORTGI20 	|
				IEEE80211_HTCAP_SMPS_OFF	|
				0),
		.ic_htextcaps = 0,	/* HT extended capabilities */
	},
};

#ifdef CONFIG_SUPPORT_HE

__dram__ __dconst__
static struct ieee80211_ic_he_cap scm2020_he_cap[] = {
	{
		.opmode = IEEE80211_M_STA,
		.ic_hemaccaps = (
			(IEEE80211_HEMACCAP_SUPP_HTC_HE) |
			(IEEE80211_HEMACCAP_SUPP_TWT_REQ) |
			((8-1) << IEEE80211_HEMACCAP_SUPP_MULTI_TID_AGGR_RX_S) |
			(IEEE80211_HEMACCAP_SUPP_TRS) |
			(IEEE80211_HEMACCAP_SUPP_BSR) |
			(IEEE80211_HEMACCAP_SUPP_MU_CASCADING) |
			(IEEE80211_HEMACCAP_SUPP_OMI_CONTROL) |
#ifdef CONFIG_SUPPORT_UORA
			(IEEE80211_HEMACCAP_SUPP_OFDMA_RA) |              /* STA: support, AP: not support */
#endif
			(IEEE80211_HEMACCAP_BSRP_BQRP_AMPDU_AGGR) |
			(IEEE80211_HEMACCAP_SUPP_BQR) |
			((2ULL-1) << IEEE80211_HEMACCAP_SUPP_MULTI_TID_AGGR_TX_S) |
			(IEEE80211_HEMACCAP_SUPP_HT_VHT_TRIG_FRAME_RX) |
			0),
		.ic_hephycapsl = (0                                     |
#ifdef CONFIG_WLAN_HW_SCM2020
			(IEEE80211_HEPHYCAP_SUPP_CHAN_WIDTH_40MHZ_IN_2GHZ) |
			(IEEE80211_HEPHYCAP_SUPP_CHAN_WIDTH_40_80MHZ_IN_5GHZ) |
			(IEEE80211_HEPHYCAP_SUPP_CHAN_WIDTH_242RU_IN_2GHZ) |
			(IEEE80211_HEPHYCAP_SUPP_CHAN_WIDTH_242RU_IN_5GHZ) |
			(IEEE80211_HEPHYCAP_PUNC_PREAMBLE_RX_80MHZ_P20MHZ) |
			(IEEE80211_HEPHYCAP_PUNC_PREAMBLE_RX_80MHZ_P40MHZ) |
#endif
#ifdef CONFIG_SUPPORT_LDPC
			(IEEE80211_HEPHYCAP_LDPC_IN_PAYLOAD) |
#endif
			(IEEE80211_HEPHYCAP_MIDAMBLE_RX_TX_MAX_NSTS) |
			(IEEE80211_HEPHYCAP_NDP_4x_LTF_AND_3_2US) |
#ifdef CONFIG_SUPPORT_STBC
			(IEEE80211_HEPHYCAP_STBC_TX_UNDER_80MHZ) |
			(IEEE80211_HEPHYCAP_STBC_RX_UNDER_80MHZ) |
#endif
			(IEEE80211_HEPHYCAP_DOPPLER_TX) |
			(IEEE80211_HEPHYCAP_DOPPLER_RX) |
			(IEEE80211_HEPHYCAP_UL_MU_FULL_MU_MIMO) |
			(IEEE80211_HEPHYCAP_UL_MU_PARTIAL_MU_MIMO) |
			(3 << IEEE80211_HEPHYCAP_DCM_MAX_CONST_TX_S) |
			(3 << IEEE80211_HEPHYCAP_DCM_MAX_CONST_RX_S) |
#ifndef CONFIG_NO_BEAMFORMEE_FOR_PS
			(IEEE80211_HEPHYCAP_SU_BEAMFORMEE) |
			(IEEE80211_HEPHYCAP_BEAMFORMEE_MIN_STS_UNDER_80MHZ) |
#endif
			(IEEE80211_HEPHYCAP_TRIG_SU_BEAMFORMER_FB) |
			(IEEE80211_HEPHYCAP_TRIG_MU_BEAMFORMER_FB) |
			(IEEE80211_HEPHYCAP_TRIG_CQI_FB) |
			(IEEE80211_HEPHYCAP_PARTIAL_BW_EXT_RANGE) |
			(IEEE80211_HEPHYCAP_PARTIAL_BANDWIDTH_DL_MUMIMO) |
			(IEEE80211_HEPHYCAP_PPE_THRESHOLD_PRESENT) |
			(IEEE80211_HEPHYCAP_HE_SU_MU_PPDU_4XLTF_AND_08_US_GI) |
			0),
		.ic_hephycapsh = (
			(IEEE80211_HEPHYCAP_HE_ER_SU_PPDU_4XLTF_AND_08_US_GI) |
#ifdef CONFIG_WLAN_HW_SCM2020
			(IEEE80211_HEPHYCAP_20MHZ_IN_40MHZ_HE_PPDU_IN_2G) |
#endif
			(IEEE80211_HEPHYCAP_HE_ER_SU_1XLTF_AND_08_US_GI) |
			(IEEE80211_HEPHYCAP_MIDAMBLE_RX_TX_2X_AND_1XLTF) |
#ifdef CONFIG_WLAN_HW_SCM2010
			(IEEE80211_HEPHYCAP_DCM_MAX_RU_20MHZ) |
#else
			(IEEE80211_HEPHYCAP_DCM_MAX_RU_80MHZ) |
#endif
#ifndef CONFIG_WLAN_HW_SCM2010
			(IEEE80211_HEPHYCAP_LONGER_THAN_16_SIGB_OFDM_SYM) |
#endif
#ifndef CONFIG_WLAN_HW_SCM2010
			(IEEE80211_HEPHYCAP_TX_1024_QAM_LESS_THAN_242_TONE_RU) |
			(IEEE80211_HEPHYCAP_RX_1024_QAM_LESS_THAN_242_TONE_RU) |
#endif
			(IEEE80211_HEPHYCAP_RX_FULL_BW_SU_USING_MU_WITH_COMP_SIGB) |
			(IEEE80211_HEPHYCAP_RX_FULL_BW_SU_USING_MU_WITH_NON_COMP_SIGB) |
			(IEEE80211_HEPHYCAP_NOMIMAL_PKT_PADDING_16US) |
			0),
		.ic_bss_colorinfo = 0,
	},
	{
		.opmode = IEEE80211_M_HOSTAP,
		.ic_hemaccaps = (
			(IEEE80211_HEMACCAP_SUPP_HTC_HE) |
			//(IEEE80211_HEMACCAP_SUPP_TRS) |
			//(IEEE80211_HEMACCAP_SUPP_BSR) |
			//(IEEE80211_HEMACCAP_SUPP_MU_CASCADING) |
			(IEEE80211_HEMACCAP_SUPP_OMI_CONTROL) |
			0),
		.ic_hephycapsl = (0                                     |
#ifdef CONFIG_SUPPORT_LDPC
			(IEEE80211_HEPHYCAP_LDPC_IN_PAYLOAD) |
#endif
			(IEEE80211_HEPHYCAP_MIDAMBLE_RX_TX_MAX_NSTS) |
#ifdef CONFIG_SUPPORT_STBC
			(IEEE80211_HEPHYCAP_STBC_TX_UNDER_80MHZ) |
			(IEEE80211_HEPHYCAP_STBC_RX_UNDER_80MHZ) |
#endif
			(IEEE80211_HEPHYCAP_DOPPLER_TX) |
			(IEEE80211_HEPHYCAP_DOPPLER_RX) |
			(IEEE80211_HEPHYCAP_PARTIAL_BW_EXT_RANGE) |
			(IEEE80211_HEPHYCAP_PPE_THRESHOLD_PRESENT) |
			(IEEE80211_HEPHYCAP_HE_SU_MU_PPDU_4XLTF_AND_08_US_GI) |
			0),
		.ic_hephycapsh = (
			(IEEE80211_HEPHYCAP_HE_ER_SU_PPDU_4XLTF_AND_08_US_GI) |
			(IEEE80211_HEPHYCAP_HE_ER_SU_1XLTF_AND_08_US_GI) |
			(IEEE80211_HEPHYCAP_MIDAMBLE_RX_TX_2X_AND_1XLTF) |
			(IEEE80211_HEPHYCAP_NOMIMAL_PKT_PADDING_16US) |
			0),
		.ic_bss_colorinfo = (CONFIG_APMODE_BSS_COLOR |
			IEEE80211_HEOPPARAM_BSS_COLOR_DISABLED
			),
	},
};

#endif

/*
#define ECO_TX_PWR_TBL
*/

/*  Tx power table
 *
 *  Ref R    M	  R	 MCS	tx_pwr_index (after ECO)
 *  1           				  22(22)
 *  2							  22(22)
 *  5.5							  22(22)
 *  11                            22(22)
 *  6	  BPSK, 1/2	   0		  20(20)
 *  9	  BPSK, 3/4    0(?)		  20(20)
 *  12	  QPSK, 1/2    1		  20(20)
 *  18	  QPSK, 3/4    2          20(20)
 *  24	16-QAM, 1/2    3          20(20)
 *  36	16-QAM, 3/4    4          19(20)
 *  48	64-QAM, 1/2    5          18(19)
 *  48	64-QAM, 2/3    5          18(19)
 *  54	64-QAM,	3/4    6          16(17)
 *  54	64-QAM,	5/6    7          14(16)
 *  54 256-QAM,	3/4    8          14(16)
 *  54 256-QAM,	5/6    8          14(16)
 */

#define SCM2020_DEFAULT_TX_PWR 16

__dram__ static struct tx_pwr_table_entry scm2020_tx_pwr_table_11bg[] = {
	/* 11B */
	{  2, 22, 22},
	{  4, 22, 22},
	{ 11, 22, 22},
	{ 22, 22, 22},
	{ 12, 20, 20},
	{ 18, 20, 20},
	{ 24, 20, 20},
	{ 36, 20, 20},
	{ 48, 20, 20},
#ifdef ECO_TX_PWR_TBL
	{ 72, 20, 20},
	{ 96, 19, 19},
	{108, 17, 17}
#else
	{ 72, 19, 19},
	{ 96, 18, 18},
	{108, 16, 16},
#endif
};

__dram__ static struct tx_pwr_table_entry scm2020_tx_pwr_table_11n[] = {
	{ MCS_0, 20, 20},
	{ MCS_1, 20, 20},
	{ MCS_2, 20, 20},
	{ MCS_3, 20, 20},
#ifdef ECO_TX_PWR_TBL
	{ MCS_4, 20, 20},
	{ MCS_5, 19, 19},
	{ MCS_6, 17, 17},
	{ MCS_7, 17, 17},
#else
	{ MCS_4, 19, 19},
	{ MCS_5, 18, 18},
	{ MCS_6, 16, 16},
	{ MCS_7, 16, 16},
#endif
};

__dram__ static struct tx_pwr_table_entry scm2020_tx_pwr_table_11ax[] = {
	{ MCS_0, 20, 20},
	{ MCS_1, 20, 20},
	{ MCS_2, 20, 20},
	{ MCS_3, 20, 20},
#ifdef ECO_TX_PWR_TBL
	{ MCS_4, 20, 20},
	{ MCS_5, 19, 19},
	{ MCS_6, 17, 17},
	{ MCS_7, 17, 17},
	{ MCS_8, 17, 17},
	{ MCS_9, 17, 17},
#else
	{ MCS_4, 19, 19},
	{ MCS_5, 18, 18},
	{ MCS_6, 16, 16},
	{ MCS_7, 16, 16},
	{ MCS_8, 16, 16},
	{ MCS_9, 16, 16},
#endif
};

#define PWR_TABLE_11BG_SIZE sizeof(scm2020_tx_pwr_table_11bg) / sizeof(struct tx_pwr_table_entry)
#define PWR_TABLE_11N_SIZE sizeof(scm2020_tx_pwr_table_11n) / sizeof(struct tx_pwr_table_entry)
#define PWR_TABLE_11AX_SIZE sizeof(scm2020_tx_pwr_table_11ax) / sizeof(struct tx_pwr_table_entry)

__dram__ static struct scm2020_pwr_table pwr_tables[SC_PHY_MAX] =  {
	[SC_PHY_NONHT] = { scm2020_tx_pwr_table_11bg, PWR_TABLE_11BG_SIZE },
	[SC_PHY_HTMF] = { scm2020_tx_pwr_table_11n, PWR_TABLE_11N_SIZE },
	[SC_PHY_HESU] = { scm2020_tx_pwr_table_11ax, PWR_TABLE_11AX_SIZE },
};

__ilm_wlan_tx__ struct scm2020_pwr_table* get_pwr_rate_table(enum ppdu fmt) {
	if (fmt >= SC_PHY_MAX) {
		return NULL;
	}
	return &pwr_tables[fmt];
}

static int set_pwr(struct scm2020_pwr_table *pwr_table, u8 fmt, u8 mcs_rate, u8 pwr) {
	u32 rate = mcs_rate;

	if (fmt == SC_PHY_NONHT)
		rate = scm2020_rate2fmtmcs(mcs_rate);

	if (rate >= pwr_table->size || rate < 0) {
		return -1;
	}

	if (pwr > pwr_table->table[rate].tx_pwr_index_max) {
		return -1;
	}

	pwr_table->table[rate].tx_pwr_index = pwr;
	return 0;
}

__ilm_wlan_tx__ static u8 get_pwr(struct scm2020_pwr_table *pwr_table, u8 mcs) {
	if (mcs >= pwr_table->size) {
		return SCM2020_DEFAULT_TX_PWR;
	}
	return pwr_table->table[mcs].tx_pwr_index;
}

__iram__ int scm2020_update_tx_pwr(u8 fmt,u8 mcs_rate, u8 pwr) {
	struct scm2020_pwr_table *pwr_table = get_pwr_rate_table(fmt);

	if (!pwr_table || !pwr_table->table)
		return -1;

	return set_pwr(pwr_table, fmt, mcs_rate, pwr);
}

__iram__ int scm2020_reset_tx_pwr(u8 fmt) {
	int i = 0;
	struct scm2020_pwr_table *fmt_table = get_pwr_rate_table(fmt);

	if (fmt_table && fmt_table->table) {
		for (i = 0; i < fmt_table->size; i++) {
			fmt_table->table[i].tx_pwr_index = fmt_table->table[i].tx_pwr_index_max;
		}
		return 0;
	}

	return -1;
}

__ilm_wlan_tx__ static u8 scm2020_rate2txpwr(u32 hwrate)
{
	u8 pwr = SCM2020_DEFAULT_TX_PWR; /* Appropriate Min as default? */
	int fmt = SC_PHY_FMT(hwrate);
	int mcs_rate = SC_PHY_MCS(hwrate);
	struct scm2020_pwr_table *pwr_table = get_pwr_rate_table(fmt);

	if (pwr_table->table) {
		pwr = get_pwr(pwr_table, mcs_rate);
	}

	return pwr;
}

__ilm_wlan__ bool scm2020_ap_proximate(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct sc_vap *tvap = vap2tvap(vap);
	int i, high = 0;
	u32 bcn_rssi_map = tvap->beacon_rssi_map;

	if (g_sc_ap_proximate_mode_dis)
		return 0;

	for (i = 31; i >= 0; i--) {
		/* NB: have recent samples weigh */
		if (isset(&bcn_rssi_map, i)) {
			high++;
		}
	}

	/* NB: need to be better than heuristics. */
	return high > 10;
}

__ilm_wlan_tx__ void scm2020_tv_set_tx_pwr(struct tx_vector *tv,
		struct ieee80211_node *ni, u32 hwrate)
{
	int8_t max_tx_pwr_limit = ni->ni_txpower;

	/* XXX: if AP is, thought to be, too close,
	 *      fall back to some reasonable fixed gain
	 *      instead of using higher Tx power
	 *      which might saturate AP's ADC circuit.
	 */
	if (scm2020_ap_proximate(ni)) {
		tv->txpwr_level_index = g_sc_ap_proximate_tx_pwr;
	} else {
		tv->txpwr_level_index = scm2020_rate2txpwr(hwrate);
	}

	if (tv->txpwr_level_index > max_tx_pwr_limit)
		tv->txpwr_level_index = max_tx_pwr_limit;
}

void scm2020_load_cal(struct sc_softc *sc)
{
	int fd;
	int ret;
	struct efuse_rw_data e_rw_dat;
	u32 val;
	u8 bank;	/* 0: invalid, 1: B1, 3: B2 */
	u8 bs;
	int tx_pwr_cal = 8;
	int rssi_cal = 8;
	int xtal_cal = 0;

	fd = open("/dev/efuse", 0, 0);
	if (fd < 0) {
		printk("Can't open /dev/efuse: %s\n", strerror(errno));
		goto save;
	}

	e_rw_dat.row = 21;
	e_rw_dat.val = &val;
	ret = ioctl(fd, IOCTL_EFUSE_READ_ROW, &e_rw_dat);
	if (ret < 0) {
		printk("ioctl error: %s\n", strerror(errno));
		goto save;
	}

	bank = bf_get(val, 0, 1);
	if (bank & 0x2) { /* bank 2 valid: 1 << 1 */
		bs = 18;
	} else if (bank & 0x1) { /* bank 1 valid: 1 << 0 */
		bs = 4;
	} else {
		goto save;
	}

	val = val >> bs;
	tx_pwr_cal = (int)bf_get(val, 0, 3);
	rssi_cal = (int)bf_get(val, 4, 7);
	xtal_cal = (int)bf_get(val, 8, 13);
	if ((u8)xtal_cal & 0x20) {
		xtal_cal = -32 + ((u8)xtal_cal & 0x1f);
	}

save:
	if (fd >= 0) {
		close(fd);
	}

	sc->cal.phy.fcal_tx_offset = tx_pwr_cal;
	sc->cal.phy.fcal_rssi_offset = rssi_cal;
	sc->cal.rf.cfo_ofs = xtal_cal;
	rf_cfo_ofs(sc->rf, sc->cal.rf.cfo_ofs);
}

#define DMAC_RESET				(0xf1600020)
#define DMAC1_SRC_ADDR_REG		(0xf1600048)
#define DMAC1_DST_ADDR_REG		(0xf1600050)
#define DMAC1_STATUS_REG		(0xf1600030)
#define DMAC1_CTRL_REG			(0xf1600040)
#define DMAC1_TRANS_SIZE_REG	(0xf1600044)

#define DMA_CTRL_DATA			(0x27480003)
#define DMA_COMPLETE			(0x10000)

__iram__ static void scm2020_hmac_run(struct sc_softc *sc)
{
	struct device *dev = sc->dev;
	u8 *imem;
	u32 v, t;
	int i __maybe_unused;

	imem = dev->base[0] + MAC_IMEM_OFFSET;

	/*
	 * Run MCU
	 * (1) Download firmware binary into IMEM
	 * (2) Write 1 to REG_MCU_CFG.RSTB fields
	 * (3) Wait until REG_MCU_STATUS.DONE bit becomes 1
	 */

	t = ktime();

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

	printk("%d usec elapsed in downloading MAC FW.\n", tick_to_us(ktime() - t));

	v = mac_readl(sc->dev, REG_MCU_CFG);
	bfmod_m(v, MCU_CFG, RSTB, 1);
	mac_writel(v, sc->dev, REG_MCU_CFG);

	t = ktime();
	do {
		v = mac_readl(sc->dev, REG_MCU_STATUS);
	} while (!bfget_m(v, MCU_STATUS, DONE) && abs(ktime() - t) < ms_to_tick(1000));

	assert(bfget_m(v, MCU_STATUS, DONE));
}

__iram__ static void scm2020_hmac_init(struct sc_softc *sc)
{
	struct device *dev = sc->dev;
	u32 v;
	u32 flags;

	sc->shmem = (struct scm_mac_shmem_map *)(dev->base[0] + MAC_SHM_OFFSET);

	local_irq_save(flags);

	/*
	 * Initialize Registers
	 */
	/* RX_CFG_PRO: promisc mode off */
	v = mac_readl(sc->dev, REG_RX_CFG);
	bfclr_m(v, RX_CFG, PRO);
	mac_writel(v, sc->dev, REG_RX_CFG);

#ifdef CONFIG_ENABLE_TX_TO
	/* TX_TO_CFG: HW tx timeout mechanism */
	v = mac_readl(sc->dev, REG_TX_TO_CFG);
	bfmod_m(v, TX_TO_CFG, TO_VALUE, 0x1fff); /* max */
	bfmod_m(v, TX_TO_CFG, TO_EN, 1);
	mac_writel(v, sc->dev, REG_TX_TO_CFG);
	/* TX_TO_11B_CFG: tx timeout 16ms for CCK rate */
	v = mac_readl(sc->dev, REG_TX_TO_11B_CFG);
	bfmod_m(v, TX_TO_11B_CFG, TO_11B_VALUE, 0x3fff);
	mac_writel(v, sc->dev, REG_TX_TO_11B_CFG);
#else
	v = mac_readl(sc->dev, REG_TX_TO_CFG);
	bfclr_m(v, TX_TO_CFG, TO_VALUE);
	bfclr_m(v, TX_TO_CFG, TO_EN);
	mac_writel(v, sc->dev, REG_TX_TO_CFG);
#endif

#ifdef CONFIG_ENABLE_RX_TO
	/* RX_TO_CFG: HW rx timeout mechanism */
	v = mac_readl(sc->dev, REG_RX_TO_CFG);
	/* 0x1c00 is about 7ms which is not enough when peer tx cck date
	 * and it will cause rx timeout, changes to ~11ms
	 * hw will separate ofdm and cck configuration later*/
	bfmod_m(v, RX_TO_CFG, RX_TO_VALUE, 0x2c00);
	bfmod_m(v, RX_TO_CFG, RX_TO_PPDU_END_EN, 1);
	mac_writel(v, sc->dev, REG_RX_TO_CFG);
	/* RX_TO_11B_CFG: rx timeout for CCK rate */
	v = mac_readl(sc->dev, REG_RX_TO_11B_CFG);
	bfmod_m(v, TX_TO_11B_CFG, TO_11B_VALUE, 0x8000);
	mac_writel(v, sc->dev, REG_RX_TO_11B_CFG);
#else
	v = mac_readl(sc->dev, REG_TX_TO_CFG);
	bfclr_m(v, RX_TO_CFG, RX_TO_VALUE);
	mac_writel(v, sc->dev, REG_RX_TO_CFG);
#endif

	/* DEV_OPT2: TXBUSY_PHY_EN, RXBUSY_CHK_EN */
	v = mac_readl(sc->dev, REG_DEV_OPT2);
	bfmod_m(v, DEV_OPT2, SIFS_LOST_STOP_TX_EN, 1);
	bfmod_m(v, DEV_OPT2, AID_MATCH_BASIC_TRIG_END_EN, 1);
	bfmod_m(v, DEV_OPT2, RXBUSY_CHK_EN, 1);
	bfmod_m(v, DEV_OPT2, CONT_TX_TERMI_FSMIDLE_EN, 1);
	bfmod_m(v, DEV_OPT2, EDCA_TXVEC_PROT_EN, 1);
#ifdef CONFIG_ENABLE_SIFS_LOST_TX_EN
	bfmod_m(v, DEV_OPT2, SIFS_LOST_SEND_TX_EN, 1);
#endif
	mac_writel(v, sc->dev, REG_DEV_OPT2);

#ifdef CONFIG_WLAN_HW_SCM2020
	mac_writel(0x11F, sc->dev, 0x1114);

#ifdef CONFIG_ERRATA_TX_TIMING_GAP
		/**
		 * timing gap should be within 15.6us ~ 16.4us in responding he_tb
		 **/
	mac_writel(0x11F, sc->dev, 0x1114);
#else
	mac_writel(0x0, sc->dev, 0x1114);
#endif
#endif

	scm2020_hmac_run(sc);

	local_irq_restore(flags);
}

/* PHY */

__dram__ __dconst__ static struct reg_set phy_mon_init[] = {
#ifdef CONFIG_WLAN_HW_SCM2020
	RSET(PHY_MODEM, PHY_TXBE_MON_CNT_RST,	0x00000001), /* TXBE MONITOR CNT reset */
	RSET(PHY_MODEM, PHY_TXBE_MON_CNT_EN,	0x00000001), /* TXBE MONITOR CNT enable */
#endif
	RSET(PHY_MODEM, PHY_MON_CNT_RST,	0x00000001), /* MON REG reset */
};

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

__dram__ __dconst__ static struct reg_set phy_init[] = {
	/* PHY Control mode */
	RSET(PHY_RFITF, FW_RFI_EN,		0x00000000),
	/* SHDN: - ctrl[0], always on */
	RSET(PHY_RFITF, RFI_CTRLSIG_SHDN,	RFCTRL(1, HIGH, ALWAYS, TXEN, 0, 0)),
	/* 2G PA En:1 - ctrl[1], during TXVALID */
	RSET(PHY_RFITF, RFI_CTRLSIG_2G_PA_EN,	RFCTRL(1, HIGH, TXVALID, TXVALID, 0, 0)),
	/* 5G PA En - ctrl[2], always off */
	RSET(PHY_RFITF, RFI_CTRLSIG_5G_PA_EN,   RFCTRL(1, HIGH, TXVALID, ALWAYS, 0, 0)),
	/* ExtSwitch Rx - ctrl[3], during RXEN */
	RSET(PHY_RFITF, RFI_CTRLSIG_EXT_SW_RX,	RFCTRL(1, HIGH, RXEN, RXEN, 0, 0)),
	/* ExtSwitch Tx - ctrl[4], during TXEN */
	RSET(PHY_RFITF, RFI_CTRLSIG_EXT_SW_TX,	RFCTRL(1, HIGH, TXEN, TXEN, 0, 0)),
	/* RF Rx Enable - ctrl[5], during RXEN */
	RSET(PHY_RFITF, RFI_CTRLSIG_RF_RX_EN, 	RFCTRL(1, HIGH, RXEN, RXEN, 0, 0)),
	/* RF Tx Enable - ctrl[6], during TXEN */
	RSET(PHY_RFITF, RFI_CTRLSIG_RF_TX_EN, 	RFCTRL(1, HIGH, TXEN, TXEN, 0, 0)),
	/* ADC control - ctrl[7], always on */
	RSET(PHY_RFITF, RFI_CTRLSIG_ADC, 	RFCTRL(1, HIGH, ALWAYS, RXEN, 0, 0)),
	/* DAC control - ctrl[8], during TXEN */
	RSET(PHY_RFITF, RFI_CTRLSIG_DAC, 	RFCTRL(1, HIGH, TXEN, TXEN, 1, 5)),
	/* RX_EN (digital I/Q) - ctrl[12], during RXEN */
	RSET(PHY_RFITF, RFI_CTRLSIG_RX_EN, 	RFCTRL(1, HIGH, RXEN, RXEN, 0, 0)),
	/* TX_EN (digital I/Q) - ctrl[13], during TXEN */
	RSET(PHY_RFITF, RFI_CTRLSIG_TX_EN, 	RFCTRL(1, HIGH, TXEN, TXEN, 0, 0)),
	/* RX_CLK  - ctrl[14], always (in FPGA) */
	RSET(PHY_RFITF, RFI_CTRLSIG_RX_CLK, 	RFCTRL(1, HIGH, ALWAYS, TXEN, 0, 0)),
	/* TX_CLK  - ctrl[15], always on (in FGPA) */
	RSET(PHY_RFITF, RFI_CTRLSIG_TX_CLK, 	RFCTRL(1, HIGH, ALWAYS, TXEN, 0, 0)),

	/* Tx DAC interface ON */
	RSET(PHY_RFITF, FW_RFI_TXDAC_INTF,	0x00000001),
	/* Rx ADC interface ON */
	RSET(PHY_RFITF, FW_RFI_RXADC_INTF, 	0x00000001),
	/* TX digital manual gain */
	RSET(PHY_RFITF, PHY_RFI_TX_DIG_GAIN_MAN,0x000000A8),
	/* RX digital manual gain */
	/* RSET(PHY_RFITF, PHY_RFI_RX_DIG_GAIN_MAN,0x000000A8),*/
	/* RX digital manual gain disable */
	RSET(PHY_RFITF, PHY_RFI_RX_DIG_GAIN_MAN,0x00000000),
	/* Rx HPF duration */
	/* RSET(PHY_RFITF, FW_RXHPF_CFG1, 0x00040004), */
	RSET(PHY_RFITF, FW_RXHPF_CFG1, 		0x00000808),
	/* Rx HPF BW selection */
	RSET(PHY_RFITF, FW_RXHPF_CFG2, 		0x00000000),
	/* PHY power en */
	RSET(PHY_MODEM, FW_PWR_EN, 		0x00000001),
	/* AGC LUT index range */
	RSET(PHY_MODEM, PHY_AGC_INDEX_CFG, 	0x00005E5A),
	/* Power low TH change */
	RSET(PHY_MODEM, PHY_AGC_PWRMSR_CFG,	0x001100FE),
	/* Init power range */
	RSET(PHY_MODEM, PHY_AGC_INITPWR_RANGE, 	0x01A2012C),
	/* Target power */
	RSET(PHY_MODEM, PHY_AGC_TARGET_PWR, 	0x015E01A2),
	/* Gain adj wait time */
	RSET(PHY_MODEM, PHY_AGC_WAIT_CNT2, 	0x00280020),
	/* Gain step control */
	RSET(PHY_MODEM, PHY_AGC_GDOWN_CFG, 	0x18181818),
	/* AGC debug config default setting */
	RSET(PHY_MODEM, PHY_AGC_TEST_OPTION, 	0x00007F11),
	/* Default value update for All version*/
	/* Initial fine/coarse gain adj wait counter */
	RSET(PHY_MODEM, PHY_AGC_WAIT_CNT1,      0x00280020),
	/* SW reset */
	RSET(PHY_MODEM, FW_PHY_SW_RESET, 	0x00000001),
	/* Temp Config */
	/* Don't Stop reg */
	RSET(PHY_MODEM, PHY_DONT_BE_STUCK, 	0xFFFFFFFF),
	/* Turn off (GID, PAID) filtering */
	RSET(PHY_MODEM, FW_VHT_CONFIG(0, 0), 	0x00000000),
	/* Turn off (GID, PAID) filtering */
	RSET(PHY_MODEM, FW_VHT_CONFIG(0, 1), 	0x00000000),
	/* RSSI offset */
	RSET(PHY_MODEM, PHY_RSSI_OFFSET, 	0x000000F4),
	/* SAT det config */
	RSET(PHY_RFITF, PHY_RFI_SATDET_CFG, 	0x000C0708),
	/* HPF config when Init gain change */
	RSET(PHY_RFITF, FW_RXHPF_CFG2, 		0x000C0000),
	/* HPF config when Init gain change */
	RSET(PHY_RFITF, FW_RXHPF_CFG1, 		0x00280808),
	/* 11B min sen config */
	RSET(PHY_MODEM, PHY_11B_DET, 		0x0003E802),
};

__dram__ __dconst__
static u32 tx_lut[64] = {
#if defined(CONFIG_WLAN_HW_SCM2010) && defined(CONFIG_RF_XRC564)
#if 0
	0x0000001C,    //MAX-41 , degital gain typo fix.
	0x0000001E,    //MAX-40
	0x00000020,    //MAX-39
	0x00000022,    //MAX-38
	0x00000024,    //MAX-37
	0x00000026,    //MAX-36
	0x00000028,    //MAX-35
	0x0000002A,    //MAX-34
	0x0000002C,    //MAX-33
	0x0000002E,    //MAX-32
	0x00000030,    //MAX-31
	0x00000130,    //MAX-30
	0x00000230,    //MAX-29
	0x00000330,    //MAX-28
	0x00000430,    //MAX-27
	0x00000530,    //MAX-26
	0x00000630,    //MAX-25
	0x00000730,    //MAX-24
	0x00000830,    //MAX-23
	0x00000930,    //MAX-22
	0x00000A30,    //MAX-21
	0x00000B30,    //MAX-20
	0x00000C30,    //MAX-19
	0x00000D30,    //MAX-18
	0x00000E30,    //MAX-17
	0x00000F30,    //MAX-16
	0x00001030,    //MAX-15
	0x00001130,    //MAX-14
	0x00001230,    //MAX-13
	0x00001330,    //MAX-12
	0x00001430,    //MAX-11
	0x00001530,    //MAX-10
	0x00001630,    //MAX-9
	0x00001730,    //MAX-8
	0x00001830,    //MAX-7
	0x00001930,    //MAX-6
	0x00001A30,    //MAX-5
	0x00001B30,    //MAX-4
	0x00001C30,    //MAX-3
	0x00001D30,    //MAX-2
	0x00001E30,    //MAX-1
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30,    //MAX
	0x00001F30     //MAX
#else	// 22.5dBm ~ -10dBm TX Gain table changed from 1dB to 0.5dB edit by Tommy 2023.11.23
	0x0025, 0x0026, 0x0125, 0x0126, 0x0225, 0x0226, 0x0325, 0x0326, 0x0425, 0x0426,
	0x0525, 0x0526, 0x0626, 0x0627, 0x0726, 0x0727, 0x0826, 0x0827, 0x0926, 0x0927,
	0x0A26, 0x0A27, 0x0B26, 0x0B28, 0x0C27, 0x0C28, 0x0D27, 0x0D28, 0x0E27, 0x0E28,
	0x0F27, 0x0F28, 0x1027, 0x1028, 0x1127, 0x1128, 0x1228, 0x1229, 0x1328, 0x1329,
	0x1428, 0x1429, 0x1528, 0x1529, 0x1628, 0x1629, 0x1728, 0x1729, 0x1829, 0x182A,
	0x1929, 0x192A, 0x1A29, 0x1A2A, 0x1B29, 0x1B2A, 0x1C29, 0x1C2A, 0x1D29, 0x1D2A,
	0x1E29, 0x1E2A, 0x1F28, 0x1F29,
#endif
#else
	0x00000028,    //MAX-30
	0x00000128,    //MAX-30
	0x00000228,    //MAX-30
	0x00000328,    //MAX-30
	0x00000428,    //MAX-29
	0x00000528,    //MAX-29
	0x00000628,    //MAX-28
	0x00000728,    //MAX-28
	0x00000828,    //MAX-27
	0x00000928,    //MAX-27
	0x00000A28,    //MAX-26
	0x00000B28,    //MAX-26
	0x00000C28,    //MAX-25
	0x00000D28,    //MAX-25
	0x00000E28,    //MAX-24
	0x00000F28,    //MAX-24
	0x00001028,    //MAX-23
	0x00001128,    //MAX-23
	0x00001228,    //MAX-22
	0x00001328,    //MAX-22
	0x00001428,    //MAX-21
	0x00001528,    //MAX-21
	0x00001628,    //MAX-20
	0x00001728,    //MAX-20
	0x00001828,    //MAX-19
	0x00001928,    //MAX-19
	0x00001A28,    //MAX-18
	0x00001B28,    //MAX-18
	0x00001C28,    //MAX-17
	0x00001D28,    //MAX-17
	0x00001E28,    //MAX-16
	0x00001F28,    //MAX-16
	0x00002028,    //MAX-15
	0x00002128,    //MAX-15
	0x00002228,    //MAX-14
	0x00002328,    //MAX-14
	0x00002428,    //MAX-13
	0x00002528,    //MAX-13
	0x00002628,    //MAX-12
	0x00002728,    //MAX-12
	0x00002828,    //MAX-11
	0x00002928,    //MAX-11
	0x00002A28,    //MAX-10
	0x00002B28,    //MAX-10
	0x00002C28,    //MAX-9
	0x00002D28,    //MAX-9
	0x00002E28,    //MAX-8
	0x00002F28,    //MAX-8
	0x00003028,    //MAX-7.5
	0x00003128,    //MAX-7
	0x00003228,    //MAX-6.5
	0x00003328,    //MAX-6
	0x00003428,    //MAX-5.5
	0x00003528,    //MAX-5
	0x00003628,    //MAX-4.5
	0x00003728,    //MAX-4
	0x00003828,    //MAX-3.5
	0x00003928,    //MAX-3
	0x00003A28,    //MAX-2.5
	0x00003B28,    //MAX-2
	0x00003C28,    //MAX-1.5
	0x00003D28,    //MAX-1
	0x00003E28,    //MAX-0.5
	0x00003F28     //MAX
#endif
};

__dram__ __dconst__
static u32 rx_lut[2][95] = {
	/* 2.4 GHz band */
{
	0x00002122, 0x00002220, 0x00002222, 0x00002320,
	0x00002322, 0x00002420, 0x00002422, 0x00002520,
	0x00002522, 0x00002620, 0x00002622, 0x00002720,
	0x00002722, 0x00002820, 0x00002822, 0x00002920,
	0x00002922, 0x00002A20, 0x00002A22, 0x00002B20,
	0x00002B22, 0x00002C20, 0x00002C22, 0x00002D20,
	0x00002D22, 0x00002E20, 0x00002E22, 0x00002F20,
	0x00002F22, 0x00004822, 0x00004920, 0x00004922,
	0x00004A20, 0x00004A22, 0x00004B20, 0x00004B22,
	0x00004C20, 0x00004C22, 0x00004D20, 0x00004D22,
	0x00004E20, 0x00004E22, 0x00004F20, 0x00004F22,
	0x00005020, 0x00005022, 0x00005120, 0x00006923,
	0x00006A21, 0x00006A23, 0x00006B21, 0x00006B23,
	0x00006C21, 0x00006C23, 0x00006D21, 0x00006D23,
	0x00006E21, 0x00006E23, 0x00006F21, 0x00006F23,
	0x00007021, 0x00007023, 0x00007121, 0x00007123,
	0x00007221, 0x00007223, 0x00007321, 0x00007323,
	0x00007421, 0x00007423, 0x00007521, 0x00007523,
	0x00007621, 0x00007623, 0x00007721, 0x00007723,
	0x00007821, 0x00007823, 0x00007921, 0x00007923,
	0x00007A21, 0x00007A23, 0x00007B21, 0x00007B23,
	0x00007C21, 0x00007C23, 0x00007D21, 0x00007D23,
	0x00007E21, 0x00007E23, 0x00007F21, 0x00007F23,
	0x00007F25, 0x00007F27, 0x00007F29
},
	/* 5 GHz band */
{
	0x00002122, 0x00002220, 0x00002222, 0x00002320,
	0x00002322, 0x00002420, 0x00002422, 0x00002520,
	0x00002522, 0x00002620, 0x00002622, 0x00002720,
	0x00002722, 0x00002820, 0x00002822, 0x00002920,
	0x00002922, 0x00002A20, 0x00002A22, 0x00002B20,
	0x00002B22, 0x00002C20, 0x00002C22, 0x00002D20,
	0x00002D22, 0x00002E20, 0x00002E22, 0x00002F20,
	0x00002F22, 0x00004722, 0x00004820, 0x00004822,
	0x00004920, 0x00004922, 0x00004A20, 0x00004A22,
	0x00004B20, 0x00004B22, 0x00004C20, 0x00004C22,
	0x00004D20, 0x00004D22, 0x00004E20, 0x00004E22,
	0x00004F20, 0x00004F22, 0x00005020, 0x00006721,
	0x00006723, 0x00006821, 0x00006823, 0x00006921,
	0x00006923, 0x00006A21, 0x00006A23, 0x00006B21,
	0x00006B23, 0x00006C21, 0x00006C23, 0x00006D21,
	0x00006D23, 0x00006E21, 0x00006E23, 0x00006F21,
	0x00006F23, 0x00007021, 0x00007023, 0x00007121,
	0x00007123, 0x00007221, 0x00007223, 0x00007321,
	0x00007323, 0x00007421, 0x00007423, 0x00007521,
	0x00007523, 0x00007621, 0x00007623, 0x00007721,
	0x00007723, 0x00007821, 0x00007823, 0x00007921,
	0x00007923, 0x00007A21, 0x00007A23, 0x00007B21,
	0x00007B23, 0x00007C21, 0x00007C23, 0x00007D21,
	0x00007D23, 0x00007E21, 0x00007E23
}
};

__dram__ __dconst__
static u32 rx_lut_r2[2][95] = {
	/* 2.4 GHz band */
	{
#if defined(CONFIG_WLAN_HW_SCM2010) && defined(CONFIG_RF_XRC564)
#ifdef __fib_v1__
		0x0028, 0x002A, 0x0128, 0x012A, 0x0228, 0x022A, 0x0328, 0x032A, 0x0428, 0x042A,
		0x0528, 0x052A, 0x0628, 0x062A, 0x0728, 0x072A, 0x0828, 0x082A, 0x0928, 0x092A,
		0x0A28, 0x0A2A, 0x1528, 0x152A, 0x1628, 0x162A, 0x1728, 0x172A, 0x1828, 0x182A,
		0x1928, 0x192A, 0x1A28, 0x1A2A, 0x2328, 0x232A, 0x2428, 0x242A, 0x2528, 0x252A,
		0x2628, 0x262A, 0x2728, 0x272A, 0x2828, 0x282A, 0x2928, 0x292A, 0x2A28, 0x2A2A,
		0x352A, 0x352C, 0x362A, 0x362C, 0x372A, 0x372C, 0x382A, 0x382C, 0x392A, 0x392C,
		0x3A2A, 0x3A2C, 0x7529, 0x752B, 0x7629, 0x762B, 0x7729, 0x772B, 0x7829, 0x782B,
		0x7929, 0x792B, 0x7A29, 0x7A2B, 0x7A2D, 0x7A2F, 0x7A31, 0x7A33, 0x7A35, 0x7A37,
		0x7A39, 0x7A3B, 0x7A3D, 0x7A3F, 0x7A41, 0x7A43, 0x7A45, 0x7A47, 0x7A49, 0x7A4B,
		0x7A4D, 0x7A4F, 0x7A51, 0x7A53, 0x7A55,
#else //A1 RX gain table
		// Improved performance of ACS and NACS edit by Tommy 2023.11.23
		0x0020, 0x0022, 0x0120, 0x0122, 0x0220, 0x0222, 0x0320, 0x0322, 0x0420, 0x0422,
		0x0520, 0x0522, 0x0620, 0x0622, 0x0720, 0x0722, 0x0820, 0x0822, 0x0920, 0x0922,
		0x0A20, 0x0A22, 0x1523, 0x1525, 0x1623, 0x1625, 0x1723, 0x1725, 0x1823, 0x1825,
		0x1923, 0x1925, 0x1A23, 0x1A25, 0x2327, 0x2329, 0x2427, 0x2429, 0x2527, 0x2529,
		0x2627, 0x2629, 0x2727, 0x2729, 0x2827, 0x2829, 0x2927, 0x2929, 0x2A27, 0x2A29,
#if 0	// for NACS test
		0x2A2B, 0x2A2D, 0x2A2F, 0x2A31, 0x2A33, 0x2A35, 0x2A37, 0x2A39, 0x2A3B, 0x392C,
#else	// Roll-back old GT for high MCS performance
		0x352A, 0x352C, 0x362A, 0x362C, 0x372A, 0x372C, 0x382A, 0x382C, 0x392A, 0x392C,
#endif
		0x3A2A, 0x3A2C, 0x7529, 0x752B, 0x7629, 0x762B, 0x7729, 0x772B, 0x7829, 0x782B,
		0x7929, 0x792B, 0x7A29, 0x7A2B, 0x7A2D, 0x7A2F, 0x7A31, 0x7A33, 0x7A35, 0x7A37,
		0x7A39, 0x7A3B, 0x7A3D, 0x7A3F, 0x7A41, 0x7A43, 0x7A45, 0x7A47, 0x7A49, 0x7A4B,
		0x7A4D, 0x7A4F, 0x7A51, 0x7A53, 0x7A55,
#endif
#else
		0x00002026, 0x00002028, 0x00002126, 0x00002128,
		0x00002226, 0x00002228, 0x00002326, 0x00002328,
		0x00002426, 0x00002428, 0x00002526, 0x00002528,
		0x00002626, 0x00002628, 0x00002726, 0x00002728,
		0x00002826, 0x00002828, 0x00002926, 0x00002928,
		0x00002A26, 0x00002A28, 0x00002B26, 0x00002B28,
		0x00004428, 0x00004526, 0x00004528, 0x00004626,
		0x00004628, 0x00004726, 0x00004728, 0x00004826,
		0x00004828, 0x00004926, 0x00004928, 0x00004A26,
		0x00004A28, 0x00004B26, 0x00004B28, 0x00004C26,
		0x00004C28, 0x00006527, 0x00006529, 0x00006627,
		0x00006629, 0x00006727, 0x00006729, 0x00006827,
		0x00006829, 0x00006927, 0x00006929, 0x00006A27,
		0x00006A29, 0x00006B27, 0x00006B29, 0x00006C27,
		0x00006C29, 0x00006D27, 0x00006D29, 0x00006E27,
		0x00006E29, 0x00006F27, 0x00006F29, 0x00007027,
		0x00007029, 0x00007127, 0x00007129, 0x00007227,
		0x00007229, 0x00007327, 0x00007329, 0x00007427,
		0x00007429, 0x00007527, 0x00007529, 0x00007627,
		0x00007629, 0x00007727, 0x00007729, 0x00007827,
		0x00007829, 0x00007927, 0x00007929, 0x00007A27,
		0x00007A29, 0x00007B27, 0x00007B29, 0x00007C27,
		0x00007C29, 0x00007D27, 0x00007D29, 0x00007E27,
		0x00007E29, 0x00007F27, 0x00007F29
#endif
	},
	/* 5 GHz band */
	{
		0x00002026, 0x00002028, 0x00002126, 0x00002128,
		0x00002226, 0x00002228, 0x00002326, 0x00002328,
		0x00002426, 0x00002428, 0x00002526, 0x00002528,
		0x00002626, 0x00002628, 0x00002726, 0x00002728,
		0x00002826, 0x00002828, 0x00002926, 0x00002928,
		0x00002A26, 0x00002A28, 0x00002B26, 0x00002B28,
		0x00002C26, 0x00004426, 0x00004428, 0x00004526,
		0x00004528, 0x00004626, 0x00004628, 0x00004726,
		0x00004728, 0x00004826, 0x00004828, 0x00004926,
		0x00004928, 0x00004A26, 0x00004A28, 0x00004B26,
		0x00004B28, 0x00004C26, 0x00004C28, 0x00004D26,
		0x00006427, 0x00006429, 0x00006527, 0x00006529,
		0x00006627, 0x00006629, 0x00006727, 0x00006729,
		0x00006827, 0x00006829, 0x00006927, 0x00006929,
		0x00006A27, 0x00006A29, 0x00006B27, 0x00006B29,
		0x00006C27, 0x00006C29, 0x00006D27, 0x00006D29,
		0x00006E27, 0x00006E29, 0x00006F27, 0x00006F29,
		0x00007027, 0x00007029, 0x00007127, 0x00007129,
		0x00007227, 0x00007229, 0x00007327, 0x00007329,
		0x00007427, 0x00007429, 0x00007527, 0x00007529,
		0x00007627, 0x00007629, 0x00007727, 0x00007729,
		0x00007827, 0x00007829, 0x00007927, 0x00007929,
		0x00007A27, 0x00007A29, 0x00007B27, 0x00007B29,
		0x00007C27, 0x00007C29, 0x00007D27
	}
};

__dram__ struct sc_vap scm2020_vap[] = {
	[0] = {
		.index = 0,
		.supported_opmodes = BIT(IEEE80211_M_STA),
	},
#if defined(CONFIG_SUPPORT_DUAL_INSTANCE) && defined(CONFIG_SUPPORT_DUAL_VIF)
#error Only support CONFIG_SUPPORT_DUAL_INSTANCE or CONFIG_SUPPORT_DUAL_VIF
#elif defined(CONFIG_SUPPORT_DUAL_INSTANCE)
	[1] = {
		.index = 1,
		.supported_opmodes = BIT(IEEE80211_M_STA),
	},
#elif defined(CONFIG_SUPPORT_DUAL_VIF)
	[1] = {
		.index = 1,
		.supported_opmodes = BIT(IEEE80211_M_STA),
	},
#endif
};

#ifdef CONFIG_SUPPORT_HE

typedef struct {
    u8 data[IEEE80211_PPE_THRESHOLD_MAX_LEN];
    int len;
} ppe_data_t;

/**
 *  Content of PPE Thresholds field for different n_ss
 * */
static const ppe_data_t ppe_thres[] = {

#if defined(CONFIG_WLAN_HW_SCM2010) || !defined(CONFIG_SUPPORT_CHWIDTH40)

	/*  ----------   1-NSS   ---------------
	 * BIT(n)         <--------          BIT(0)
	 * [000]       [[111000]]      [0001] [000]
	 *   PAD          PPE Info       RU-MASK NSS
	 *	     PPET8=None PPET16=0  242  1
	 **/
	{
		.data = {0x08, 0x1c},
		.len = 2,
	},

	/*  ----------   2-NSS   ---------------
	 * BIT(n)         <--------          BIT(0)
	 *  [00000]  [[111000][111000]]  [0001] [001]
	 *  PAD	        PPE Info         RU-MASK NSS
	 *	      PPET8=None PPET16=0  242  2
	 */
	{
		.data = {0x09, 0x1c, 0x07},
		.len = 3,
	},

#elif defined(CONFIG_SUPPORT_CHWIDTH80)

	/*  ----------   1-NSS   ---------------
	 * BIT(n)            <--------          BIT(0)
	 *  [0000000]   [[111000] . [111000]] [0111]   [000]
	 *    PAD	    PPE Info          RU-MASK   NSS
	 *	         PPET8=None PPET16=0  242/484/996 1
	 **/
	{
		.data = {0x38, 0x1c, 0xc7, 0x01},
		.len = 4,
	},

	/*  ----------   2-NSS   ---------------
	 * BIT(n)               <--------           BIT(0)
	 *[00000]   [[111000] .... [111000]] [0111]   [001]
	 *   PAD	   PPE Info          RU-MASK   NSS
	 *	     PPET8=None PPET16=0      242/484/996 2
	 **/
	{
		.data = {0x39, 0x1c, 0xc7, 0x71, 0x1c, 0x07},
		.len = 6,
	},

#elif defined(CONFIG_SUPPORT_CHWIDTH40)

	/*  ----------   1-NSS   ---------------
	 * BIT(n)         <--------          BIT(0)
	 * [00000]   [[111000] [111000]] [0011] [000]
	 *   PAD          PPE Info       RU-MASK NSS
	 *	    PPET8=None PPET16=0  242/484  1
	 **/
	{
		.data = {0x18, 0x1c, 0x07},
		.len = 3,
	},

	/*  ----------   2-NSS   ---------------
	 * BIT(n)         <--------          BIT(0)
	 *  [0]   [[111000] .. [111000]] [0011] [001]
	 *  PAD	        PPE Info         RU-MASK NSS
	 *	      PPET8=None PPET16=0  242/484  2
	 */
	{
		.data = {0x19, 0x1c, 0xc7, 0x71},
		.len = 4,
	},
#endif

};

#endif

#ifdef CONFIG_MAC_TX_TIMEOUT
#include <limits.h>
osTimerId_t txq_timeout_timer = NULL;
u32 txq_timestamp[SC_NR_TXQ] = {0};


#ifdef CONFIG_SCM2010_TX_TIMEOUT_DEBUG

struct scm2020_tx_dbg_info g_tx_timeout_pkts[SCM2010_TX_TIMEOUT_DEBUG_TXQ_NUM] = {0};

struct tx_timeout_dbg g_tx_timeout_dbg;

void scm2020_init_tx_dbg(void) {
	mtx_init(&g_tx_timeout_dbg.ifq_mtx, NULL, NULL, MTX_DEF);
	g_tx_timeout_dbg.q_idx = 0;
}

void dump_bd(char *prefix, struct bdesc *bd0, int bd_num) {
	int i = 0;
	struct bdesc *bd = bd0;

	for (i = 0; i < bd_num; i++) {
		printf("%s bd[%d]: len = %02d, pdata_h = 0x%x, pdata_l = 0x%x\n",
			prefix,
			i,
			bd->len,
			bd->pdata_h,
			bd->pdata_l);
		bd++;
	}
}

void dump_tv(char *prefix, struct tx_vector *tv) {
	/* Word 1 */
	printf("%s tv: mcs = 0x%x, preamble_type = 0x%x, l_length = 0x%x, format = 0x%x, fec_coding = 0x%x, txpwr_level_index = 0x%x\n",
		prefix,
		tv->mcs,
		tv->preamble_type,
		tv->l_length,
		tv->format,
		tv->fec_coding,
		tv->txpwr_level_index);

	/* Word 2 */
	printf("%s tv: length = 0x%x, spatial_reuse = 0x%x\n",
			prefix,
			tv->length,
			tv->spatial_reuse);

	/* Word 3 */
	printf("%s tv: apep_length = 0x%x, ch_bandwidth = 0x%x, stbc = 0x%x, ru_allocation = 0x%x\n",
		prefix,
		tv->apep_length,
		tv->ch_bandwidth,
		tv->stbc,
		tv->ru_allocation);

	/* Word 4 */
	printf("%s tv: he_sig_a2_reserved = 0x%x, txop_duration = 0x%x, partial_aid = 0x%x, rate_auto_fallback = 0x%x, dyn_bandwidth_in_non_ht = 0x%x, ch_bandwidth_in_non_ht = 0x%x, midamble_periodicity = 0x%x\n",
		prefix,
		tv->he_sig_a2_reserved,
		tv->txop_duration,
		tv->partial_aid,
		tv->rate_auto_fallback,
		tv->dyn_bandwidth_in_non_ht,
		tv->ch_bandwidth_in_non_ht,
		tv->midamble_periodicity);

	/* Word 5 */
	printf("%s tv: rate_idx = 0x%x, group_id = 0x%x, gi_type = 0x%x\n",
		prefix,
		tv->rate_idx,
		tv->group_id,
		tv->gi_type);

	/* Word 6 */
	printf("%s tv: scrambler_initial_value = 0x%x, ldpc_extra_symbol = 0x%x, bss_color = 0x%x, he_ltf_type = 0x%x, total_num_sts = 0x%x, num_he_ltf = 0x%x, nominal_packet_padding = 0x%x, default_pe_duration = 0x%x, starting_sts_num = 0x%x, smoothing = 0x%x, aggregation = 0x%x\n",
		prefix,
		tv->scrambler_initial_value,
		tv->ldpc_extra_symbol,
		tv->bss_color,
		tv->he_ltf_type,
		tv->total_num_sts,
		tv->num_he_ltf,
		tv->nominal_packet_padding,
		tv->default_pe_duration,
		tv->starting_sts_num,
		tv->smoothing,
		tv->aggregation);

	/* Word 7 */
	printf("%s tv: dcm = 0x%x, num_sts = 0x%x, doppler = 0x%x, he_ltf_mode = 0x%x, txop_ps_not_allowed = 0x%x, trigger_method = 0x%x, uplink_flag = 0x%x, trigger_responding = 0x%x, he_tb_data_symbols = 0x%x, he_tb_pre_fec_factor = 0x%x, feedback_status = 0x%x, he_tb_pe_disambiguity = 0x%x\n",
		prefix,
		tv->dcm,
		tv->num_sts,
		tv->doppler,
		tv->he_ltf_mode,
		tv->txop_ps_not_allowed,
		tv->trigger_method,
		tv->uplink_flag,
		tv->trigger_responding,
		tv->he_tb_data_symbols,
		tv->he_tb_pre_fec_factor,
		tv->feedback_status,
		tv->he_tb_pe_disambiguity);
	}


void dump_me(char *prefix, volatile struct mentry *me) {
	/* Printing the structure members */
	printf("%s me: pbd_l = 0x%08x, pbd_h = 0x%08x\n", prefix, me->pbd_l, me->pbd_h);

	/* MT2 fields */
	printf("%s me: aid = 0x%03x, tid = 0x%x, sn = 0x%03x, noack = 0x%x, eof = 0x%x, sent = 0x%x, ack = 0x%x\n",
		prefix,
		me->aid,
		me->tid,
		me->sn,
		me->noack,
		me->eof,
		me->sent,
		me->ack);

	/* MT3 fields */
	printf("%s me: len = 0x%03x, more = 0x%x, noenc = 0x%x, num = 0x%x, prot = 0x%x, ts_new = 0x%x, ts_off = 0x%x, htc = 0x%x\n",
		prefix,
		me->len,
		me->more,
		me->noenc,
		me->num,
		me->prot,
		me->ts_new,
		me->ts_off,
		me->htc);

	/* MT4 fields */
	printf("%s me: prot_dur = 0x%04x, spacing = 0x%x, retry = 0x%x\n",
		prefix,
		me->prot_dur,
		me->spacing,
		me->retry);

	/* MT5 fields */
	printf("%s me: to_retry = 0x%x, reserve = 0x%x\n",
		prefix,
		me->to_retry,
		me->reserve);

	/* MT6 and MT7 */
	printf("%s me: sw0 = 0x%08x, sw1 = 0x%08x\n",
		prefix,
		me->sw0,
		me->sw1);
}
#include <cli.h>

static int do_tx_timeout_dbg(int argc, char *argv[])
{
	int i = 0, j = 0;
	struct scm2020_tx_dbg_info *tx_dbg_info;
	struct scm2020_ampdu_tx_dbg_hdr *kick_hdr;
	struct scm2020_ampdu_tx_dbg_hdr *tt_hdr;
	struct scm2020_mpdu_tx_dbg_hdr *kick_mpdu, *tt_kick_mpdu;

	for (i = 0; i < g_tx_timeout_dbg.q_idx; i++) {
		tx_dbg_info = &g_tx_timeout_dbg.tx_dbg_info[i];
		kick_hdr = &tx_dbg_info->kick_hdr;
		tt_hdr = &tx_dbg_info->timeout_hdr;
		printf("============== no. %d A-MPDU on hwq[%d]===================\n", i, tx_dbg_info->hwqueue);
		printf("kick_time:%d timeout_time:%d diff:%dus \n", tx_dbg_info->kick_time,
			tx_dbg_info->timeout_time, tick_to_us(  abs(tx_dbg_info->timeout_time - tx_dbg_info->kick_time)));
		printf("txq_cmd[0x%x][0x%x] \n", kick_hdr->txq_cmd, tt_hdr->txq_cmd);
		printf("txq_state[0x%x][0x%x] \n", kick_hdr->txq_state, tt_hdr->txq_state);
		printf("txq_timer[0x%x][0x%x] \n", kick_hdr->txq_timer, tt_hdr->txq_timer);
		printf("txq_txop[0x%x][0x%x] \n", kick_hdr->txq_txop, tt_hdr->txq_txop);
		printf("ampdu_num[0x%x][0x%x] \n", kick_hdr->ampdu_num, tt_hdr->ampdu_num);
		for (j = 0; j < kick_hdr->ampdu_num; j++) {
			printf("---------------- MPDU [%d]\n", j);
			kick_mpdu = &kick_hdr->ampdu_hdr[j];
			tt_kick_mpdu = &tt_hdr->ampdu_hdr[j];
			dump_me("kick", &kick_mpdu->me);
			dump_me("TT  ", &tt_kick_mpdu->me);
			dump_bd("kick", kick_mpdu->bd, kick_mpdu->bd_num);
			dump_bd("TT  ", tt_kick_mpdu->bd, tt_kick_mpdu->bd_num);
			dump_tv("kick", &kick_mpdu->tv);
			dump_tv("TT  ", &tt_kick_mpdu->tv);
		}

	}
	return 0;
}

CMD(txto_dbg, do_tx_timeout_dbg,
    "dump tx timeout infomations",
    "txto_dbg"
);

#endif /* CONFIG_SCM2010_TX_TIMEOUT_DEBUG */

/*
 * The stuck case:
 * The sched is not empty > TIMEOUT without any txcomplete IRQ.
 */
void scm2020_tx_peridtimeout(void *arg)
{
	u8 i;
	u32 flags;
	u32 timenow, cp_timetxirq;
	bool empty;
	struct sc_softc *sc = (struct sc_softc *)arg;

	timenow = ktime();
	local_irq_save(flags);

	for (i = 0; i < SC_NR_TXQ; i++) {
		empty = ifq_empty(&(sc->txq[i].sched));
		cp_timetxirq  = txq_timestamp[i];


		if (empty) {
			continue;
		}
		/* Two cases:
		 * 1. timer counter normal:   0 timetxirq timenow max
		 * 2. timer counter overflow: 0 timenow timetxirq max*/
		if (((cp_timetxirq <= timenow) &&
			((timenow - cp_timetxirq) >= ms_to_tick(CONFIG_MAC_TX_TIMEOUT_VAL))) ||
			((cp_timetxirq > timenow) &&
			((UINT_MAX - cp_timetxirq + timenow) >= ms_to_tick(CONFIG_MAC_TX_TIMEOUT_VAL)))) {
			scm2020_tx_timeout(&sc->txq[i]);
		}
	}
	local_irq_restore(flags);
}
#endif

#ifdef VERIFY_DTIM_PARSER

struct bcn_tim_info {
    bool aid0;
    bool unic;
    bool bcmc;

    struct ieee80211_frame *wh;
    u16 sn;
    int len;
	bool dtim;
};

static struct bcn_tim_info dtimp_tim, bcn_tim;

static int rx_mpdu_len(struct mbuf *m0)
{
	struct hw_rx_hdr *hw = mtod(m0, struct hw_rx_hdr *);
	/* if receive encrypt frame, length will lack 4 byte*/
	if(hw->ri.err_key)
		hw->ri.len += 4;
	return hw->ri.len + sizeof(*hw);
}

static inline u16 ieee80211_get_seqctrl(struct ieee80211_frame *mh)
{
	u16 seqctrl;

	if (IEEE80211_HAS_SEQ(mh->i_fc[0] & IEEE80211_FC0_TYPE_MASK,
                mh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK))
		seqctrl = get_unaligned_le16(mh->i_seq);
	else
		seqctrl = 0;

	return seqctrl;
}

/* Return true if the last Beacon's TIM IE has been parsed. */

static bool scm2020_chk_bcn_tim(struct sc_softc *sc, struct sc_rx_desc *desc,
        struct bcn_tim_info *btim)
{
	struct ieee80211vap *vap = &scm2020_vap[0].vap;
    struct ieee80211_node *ni = vap->iv_bss;
    struct mbuf *m;
    struct hw_rx_hdr *hw;
    struct ieee80211_frame *mh;
	int totlen = 0;
    u8 *frm, *efrm;
    int tim_ucast = 0, tim_mcast = 0;
    u8 frag;
    u8 type, subtype;

    if (vap->iv_state < IEEE80211_S_RUN || !ni || !ni->ni_associd)
        return false;

    frag = sc->shmem->rx[desc->index].frag;
    if (frag != 0x3) /* Ignore fragmented ones for now. */
        return false;

    m = desc->m;
    hw = mtod(m, struct hw_rx_hdr *);
    mh = hw->mh;

    type = mh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = mh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

    if (type != IEEE80211_FC0_TYPE_MGT || subtype != IEEE80211_FC0_SUBTYPE_BEACON) {
        return false;
    }

    totlen = rx_mpdu_len(m);
    frm = (u8 *)&mh[1];
    efrm = mtod(m, u8 *) + totlen;
	frm += (8 + 2 + 2);
    while (efrm - frm > 1) {
        if (*frm == IEEE80211_ELEMID_TIM) {
            struct ieee80211_tim_ie *tim = (struct ieee80211_tim_ie *)frm;
            int aid = IEEE80211_AID(ni->ni_associd);
            int ix = aid / NBBY;
            int min = tim->tim_bitctl & ~1;
            int max = tim->tim_len + min - 4;

            if (min <= ix && ix <= max
                && isset(tim->tim_bitmap - min, aid)) {
                tim_ucast = 1;
            }
            /*
             * Do a separate notification
             * for the multicast bit being set.
             */
            if (tim->tim_bitctl & 1) {
                tim_mcast = 1;
            }
            break;
        }
        frm += frm[1] + 2;
    }

    /* btim->aid0 ? */
    btim->unic = tim_ucast ? true : false;
    btim->bcmc = tim_mcast ? true : false;

    btim->wh = mh;
    btim->len = hw->ri.len;
    btim->sn = bf_get(ieee80211_get_seqctrl(mh), 4, 15);

    return true;
}

static int _isprint(unsigned char c)
{
	if (c < 32)
		return 0;

	if (c >= 127)
		return 0;

	return 1;
}

static void _hexdump(void *buffer, size_t sz)
{
	unsigned char *start, *end, *buf = (unsigned char *)buffer;
	char *ptr, line[128];
	int cursor;

	start = (unsigned char *)(((portPOINTER_SIZE_TYPE) buf) & ~15);
	end = (unsigned char *)((((portPOINTER_SIZE_TYPE) buf) + sz + 15) & ~15);

	while (start < end) {

		ptr = line;
		ptr += sprintf(ptr, "0x%08lx: ", (unsigned long) start);

		for (cursor = 0; cursor < 16; cursor++) {
			if ((start + cursor < buf) || (start + cursor >= buf + sz))
				ptr += sprintf(ptr, "..");
			else
				ptr += sprintf(ptr, "%02x", *(start + cursor));

			if ((cursor & 1))
				*ptr++ = ' ';
			if ((cursor & 3) == 3)
				*ptr++ = ' ';
		}
		ptr += sprintf(ptr, "  ");

		/* ascii */
		for (cursor = 0; cursor < 16; cursor++) {
			if ((start + cursor < buf) || (start + cursor >= buf + sz))
				ptr += sprintf(ptr, ".");
			else
				ptr += sprintf(ptr, "%c", _isprint(start[cursor]) ? start[cursor] : ' ');
		}
		ptr += sprintf(ptr, "\n");
		printk("%s", line);
		start += 16;
	}
}

#endif

/**
 * scm2020_mac_isr() - interrupt handler
 *
 */
__OPT_O1__ __ilm_wlan__ static int
scm2020_mac_isr(int irq, void *data)
{
	struct sc_softc *sc = data;
	struct device *dev = sc->dev;
	u32 irqs, bf;
	int hwq;
	int vif = 0;
	struct ieee80211vap *vap;

	irqs = mac_readl(dev, REG_INTR_STATUS);
	mac_writel(irqs, dev, REG_INTR_CLEAR);

#ifdef VERIFY_DTIM_PARSER
    /* Only for VIF0 for now. */
	if (bfget_m(irqs, INTR_STATUS, RX_DTIM)) {
        struct ieee80211_node *ni;
        u32 v;
		vap = &scm2020_vap[0].vap;
        ni = vap->iv_bss;

        if (vap->iv_state >= IEEE80211_S_RUN && ni && ni->ni_associd) {
            printk("[%08lu] dtim.int\n", tick_to_us(ktime()));
            v = mac_readl(sc->dev, REG_RX_DTIM_INFO);
            dtimp_tim.aid0 = bfget_m(v, RX_DTIM_INFO, RX_DTIM_AID0_BITMAP_HIT) ? true : false;
            dtimp_tim.unic = bfget_m(v, RX_DTIM_INFO, RX_DTIM_BITMAP_HIT) ? true : false;
            dtimp_tim.bcmc = bfget_m(v, RX_DTIM_INFO, RX_TRAFFIC_INDICATOR) ? true : false;
			dtimp_tim.dtim = bfget_m(v, RX_DTIM_INFO, RX_IS_DTIM) ? true : false;
        } else {
            memset(&dtimp_tim, 0, sizeof(dtimp_tim));
        }
    }
#elif defined(CONFIG_SCM2020_CLI_DTIM_PARSER) || defined(CONFIG_SCM2020_PM_DTIM_PARSER)
	/* In case of DTIM PARSER, trigger the test function */
	extern void set_dtim_parser_int_inc(struct sc_softc *sc);
	if (bfget_m(irqs, INTR_STATUS, RX_DTIM)) {
		set_dtim_parser_int_inc(sc);
#ifdef CONFIG_SCM2020_PM_DTIM_PARSER
		return 0;
#endif
	}
#endif

	if ((bf = bfget_m(irqs, INTR_STATUS, BASIC_TRIG_QNULL_VIF)) > 0) {
		for(vif = 0; vif < sc->n_vap; vif++) {
			if (bf & (1 << vif)) {
				vap = &scm2020_vap[vif].vap;
				if (SCM2020_GET_HE_TB_LEN(sc, vif) > 0)
					vap->he_tb_length = SCM2020_GET_HE_TB_LEN(sc, vif);
			}
		}

	}
#define max_hwq	(ARRAY_SIZE(sc->txq) - 1)
	/* TX */
	if ((bf = bfget_m(irqs, INTR_STATUS, TX_DONE)) != 0) {
		for (hwq = max_hwq; hwq >= 0; hwq--) {
			if (bf & (1 << hwq)) {
#ifndef CONFIG_MAC_TX_TIMEOUT
				scm2020_tx_complete(&sc->txq[hwq]);
#else
				txq_timestamp[hwq] = ktime();
				scm2020_tx_complete(&sc->txq[hwq], false);
#endif
			}
		}
	}
#undef max_hwq

	/* RX */
	if (bfget_m(irqs, INTR_STATUS, RX_BUF)) {
		int readidx = RX_PTR2IDX(sc, read_ptr(sc, READ));
		struct sc_rx_desc *desc = &sc->rx.desc[readidx];
		/* READ_PTR == WRITE_PTR can happen by manipulating REG_RX_BUF_CFG.EN */
		if (read_ptr(sc, READ) == read_ptr(sc, WRITE)) {
			printk("[%s, %d] Spurious interrupt (case 1) ignored\n", __func__, __LINE__);
			return 0; /* spurious */
		}
#ifdef VERIFY_DTIM_PARSER
#include <mem.h>
        {
            if (scm2020_chk_bcn_tim(sc, desc, &bcn_tim)) {
                printk("[%08lu] bcn.int\n", tick_to_us(ktime()));
                if (dtimp_tim.dtim
						&& (dtimp_tim.unic != bcn_tim.unic
							|| dtimp_tim.bcmc != bcn_tim.bcmc)) {
                    printk("DTIM parser is wrong(%04d). bcmc:%d, %d uc:%d, %d\n",
                            bcn_tim.sn,
                            dtimp_tim.bcmc, bcn_tim.bcmc,
                            dtimp_tim.unic, bcn_tim.unic);
                    _hexdump(bcn_tim.wh, bcn_tim.len);
                }
				/* Be ready for the next. */
				dtimp_tim.dtim = false;
            }
      }
#endif
		while (1) {
			rx_desc_change_state(sc, desc, RX_DESC_SW_READY);
			advance_ptr(sc, READ);
			if (is_room(sc, READ)) {
				desc = rx_desc_next(sc, desc);
			} else
				break;
		};

#ifdef CONFIG_SCM2020_CLI_DTIM_PARSER
	{
		extern void check_dtim_parser_start(struct mbuf *m);
		extern bool macdtim_interrupt_run_flag;

		/* let's do start the DTIM Parser Interrupt Test */
		if (macdtim_interrupt_run_flag) {
			check_dtim_parser_start(desc->m);
		}
	}
#endif
		/*
		 * It is a bug that taskqueue_enqueue_fast will return -1
		 * when it is supposed to return 0.
		 * So, we need Specifically check if it returns 1 to see if
		 * we need to reschedule.
		 *
		 * Since this is the only where we check the return value,
		 * I would not bother to patch taskqueue_thread_enqueue.
		 */
		if (taskqueue_enqueue_fast(sc->sc_tq, &sc->rx.handler_task) > 0)
			portYIELD_FROM_ISR(true);

	}

#ifdef CONFIG_SUPPORT_TWT
	if ((bfget_m(irqs, INTR_STATUS, TSF0) & (WAKE_TIMER | DOZE_TIMER))) {
		taskqueue_enqueue_fast(sc->sc_tq, &sc->twt_timeout_task);
		if ((bfget_m(irqs, INTR_STATUS, TSF0) == (WAKE_TIMER | DOZE_TIMER)))
			printk("[%s, %d] Spurious: wake and doze INT should not happen in the same time\n", __func__, __LINE__);
	}
#endif

	return 0;
}

__iram__ void
scm2020_init(void *priv)
{
	struct sc_softc *sc = priv;
	struct ifnet *ifp = sc->ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct device *dev = sc->dev;
	int ret = 0;
	u8 i __maybe_unused;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

#ifdef DEBUG
	if_printf(ifp, "starting MAC controller\n");
#endif

	scm2020_hmac_enable(sc, true);
	scm2020_rx_init(sc);
	scm2020_hmac_int_mask(sc, true);
#ifdef VERIFY_DTIM_PARSER
    {
        u32 v;

        v = mac_readl(sc->dev, REG_INTR_ENABLE);
		bfmod_m(v, INTR_ENABLE, RX_DTIM, 1);
        mac_writel(v, sc->dev, REG_INTR_ENABLE);

        v = mac_readl(sc->dev, REG_RX_DTIM_CFG);
		bfmod_m(v, RX_DTIM_CFG, DTIM_MODE, 0)
		bfmod_m(v, RX_DTIM_CFG, DTIM_FCS_CHECK, 0)
        mac_writel(v, sc->dev, REG_RX_DTIM_CFG);
    }
#endif

	scm2020_rf_power_on(sc);

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

#if defined(CONFIG_MAC_TX_TIMEOUT) && !defined(DISABLE_MAC_TX_TIMEOUT)
	txq_timeout_timer = osTimerNew(scm2020_tx_peridtimeout, osTimerPeriodic,
		sc, NULL);
	osTimerStart(txq_timeout_timer, msecs_to_ticks(CONFIG_MAC_TX_TIMEOUT_VAL / 2));
#endif

#ifdef CONFIG_SUPPORT_MU_EDCA
	for (i=0; i < SC_MU_EDCA_NR_TXQ; i++)
		sc->txq[i].txq_edca_timer = osTimerNew(scm2020_mu_edca_timeout,
						osTimerOnce, &sc->txq[i], NULL);
#endif

	ieee80211_start_all(ic);		/* start all vap's */

	ret = request_irq(dev->irq[0], scm2020_mac_isr, dev_name(dev), dev->pri[0], sc);
	if (ret)
		goto fail;

#ifdef CONFIG_WLAN_HW_SCM2020
	/* NB: work-around for irqs mismatch from GIC */
	ret = request_irq(dev->irq[1], scm2020_mac_isr, dev_name(dev), dev->pri[1], sc);
	if (ret)
		goto fail;
#endif

	return;

 fail:
	printk("%s: failed\n", __func__);
}

__iram__ void
scm2020_stop(void *arg)
{
	struct sc_softc *sc = arg;
	struct device *dev = sc->dev;
	struct ifnet *ifp = sc->ifp;
	struct ieee80211vap *vap;
	u8 i;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	ifp->if_drv_flags |= IFF_DRV_OACTIVE;

#ifdef CONFIG_SUPPORT_MU_EDCA
	for (i=0; i < SC_MU_EDCA_NR_TXQ; i++) {
		osTimerStop(sc->txq[i].txq_edca_timer);
		osTimerDelete(sc->txq[i].txq_edca_timer);
	}
#endif

#ifdef DEBUG
	if_printf(ifp, "stopping MAC controller\n");
#endif

	TAILQ_FOREACH(vap, &sc->ic->ic_vaps, iv_next) {
		scm2020_txq_flush(vap);
	}
#ifdef CONFIG_MAC_TX_TIMEOUT
	osTimerStop(txq_timeout_timer);
	osTimerDelete(txq_timeout_timer);
#endif

	free_irq(dev->irq[0], dev_name(dev));

    scm2020_rf_power_off(sc);

	scm2020_hmac_int_mask(sc, false);
	scm2020_hmac_enable(sc, false);

	/*
	 * Make sure things are done in the right sequence.
	 *
	 * (1) Drain rx.handler_task so that already received
	 *     frames are reclaimed.
	 * (2) Drain rx.refill_task so that on-going refill
	 *     will be done.
	 * (3) Call scm2020_rx_desc_deinit to return all mbufs
	 *     installed in the rx descriptor ring to the system.
	 * (4) Set sc.rx.no_refill to true so that tx.reclaim_task
	 *     won't try to refill the rx descriptor ring (again).
	 * (5) Drain tx.reclaim_task to return all mbufs in txq->done
	 *     to the system.
	 * (6) Drain txq_drop_task to clean all tx queues
	 *
	 */

	taskqueue_drain(sc->sc_tq, &sc->rx.handler_task);
	taskqueue_drain(sc->sc_tq, &sc->rx.refill_task);
	scm2020_rx_desc_deinit(sc);
	sc->rx.no_refill = true;
	taskqueue_drain(sc->sc_tq, &sc->tx.reclaim_task);
#ifdef CONFIG_SUPPORT_TWT
	taskqueue_drain(sc->sc_tq, &sc->twt_timeout_task);
#endif
	taskqueue_drain(sc->sc_tq, &sc->txq_drop_task);
#ifdef CONFIG_SCM2020_WLAN_PM
	taskqueue_drain(sc->sc_tq, &sc->ps_chk_bcn_miss_task);
#endif

}

/**
 * scm2020_ioctl() - parent ioctl() method
 *
 */
__iram__ static int
scm2020_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct sc_softc *sc = ifp->if_softc;
	int error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
				scm2020_init(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				scm2020_stop(sc);
		}
		break;
	case SIOCGIFMEDIA:
		break;
	case SIOCGIFADDR:
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

/**
 * scm2020_parent() - start/stop method
 *
 */
__iram__ static void
scm2020_parent(struct ieee80211com *ic)
{
	struct sc_softc *sc = (struct sc_softc *)ic->ic_softc;
	struct ifnet *ifp = sc->ifp;

	if (ifp->if_flags & IFF_UP) {
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
			scm2020_init(sc);
	} else {
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			scm2020_stop(sc);
	}
}

uint8_t g_sc_nr_max_amsdu = CONFIG_MAX_AMSDU_NUM;
uint8_t g_sc_nr_bds_hdr = CONFIG_BDS_HDR_NUM;
uint8_t g_sc_bds_adj_offset = 0;
uint8_t g_sc_bds_hdr_sz = 0;
#ifdef CONFIG_TX_KEEPALIVE_INTERVAL
uint8_t g_sc_txkeep_intvl = CONFIG_TX_KEEPALIVE_INTERVAL;
#else
uint8_t g_sc_txkeep_intvl = 20;
#endif
#ifdef CONFIG_WC_TX_KEEPALIVE_INTERVAL
uint32_t g_wc_txkeep_intvl = CONFIG_WC_TX_KEEPALIVE_INTERVAL;
#else
uint32_t g_wc_txkeep_intvl = 20;
#endif

#ifdef CONFIG_WATCHER_KEEPALIVE_NULL
uint8_t g_wc_txkeep_mode = 1;
#elif CONFIG_WATCHER_KEEPALIVE_GARP
uint8_t g_wc_txkeep_mode = 2;
#elif CONFIG_WATCHER_KEEPALIVE_MQTT
uint8_t g_wc_txkeep_mode = 3;
#else
uint8_t g_wc_txkeep_mode = 0;
#endif


#ifdef CONFIG_MONITOR_BEACON_MISS
uint8_t g_sc_monitor_bmiss = 1;
uint8_t g_sc_bmiss_thresold = CONFIG_BMISS_THRESHOLD;
#ifdef CONFIG_BMISS_DYN_WIN
uint8_t g_sc_bmiss_dyn_win_en = 1;
#else
uint8_t g_sc_bmiss_dyn_win_en = 0;
#endif /* CONFIG_BMISS_DYN_WIN */
#else
uint8_t g_sc_monitor_bmiss = 0;
uint8_t g_sc_bmiss_thresold = 0;
uint8_t g_sc_bmiss_dyn_win_en = 0;
#endif /* CONFIG_MONITOR_BEACON_MISS */
int8_t  g_sc_ap_proximate_rssi_thr = CONFIG_AP_PROXIMATE_THR;
uint8_t g_sc_ap_proximate_tx_pwr = CONFIG_AP_PROXIMATE_TXPWR;
uint8_t g_sc_ap_proximate_mode_dis = 0;

uint8_t g_sc_ap_support_stanum = CONFIG_SUPPORT_STA_NUM;
#ifdef CONFIG_SUPPORT_NODE_POOL
__dnode__ struct ieee80211_node_pool node_pool = {0};
#endif

static int
scm2020_set_quiet(struct ieee80211_node *ni, u_int8_t *quiet_elm)
{
	return 0;
}

extern void scm2020_scan_end_chk(struct ieee80211com *ic, struct ieee80211vap *vap);
__iram__ static int
scm2020_attach(struct device *dev)
{
	struct sc_softc *sc = dev->driver_data;
	struct ifnet *ifp;
	struct ieee80211com *ic;
	u8 bands[IEEE80211_MODE_BYTES] = { 0 };
    int i __maybe_unused;
    int headroom;

	sc->dev = dev;
	sc->tvap = scm2020_vap;
	sc->n_vap = ARRAY_SIZE(scm2020_vap);
	sc->ver.macver = mac_readl(sc->dev, REG_VER_INFO);
	sc->ver.macgit = mac_readl(sc->dev, REG_GIT_VER);
	/* NB: assumes all PHY instances have the same version */
	sc->ver.phyver = phy_readl(sc->dev, PHY_MODEM, PHY_VER_INFO);
	sc->ver.phygit = phy_readl(sc->dev, PHY_MODEM, PHY_GIT_VER);

	g_sc_bds_hdr_sz = (g_sc_nr_max_amsdu * (sizeof(struct bdesc) * g_sc_nr_bds_hdr));
#ifdef CONFIG_SUPPORT_BDS_ADJ
	g_sc_bds_adj_offset = g_sc_bds_hdr_sz;
#endif

	/* Allocate Rx descriptors */

	sc->rx.n_rx_desc = (1 << CONFIG_RX_BUF_NUM_LOG2);
	sc->rx.desc = kzalloc(sc->rx.n_rx_desc * sizeof(struct sc_rx_desc));

	/* Initialize PHY parameters */

	sc->phy.phy_init = phy_init;
	sc->phy.n_phy_init = ARRAY_SIZE(phy_init);
	sc->phy.phy_mon_init = phy_mon_init;
	sc->phy.n_phy_mon_init = ARRAY_SIZE(phy_mon_init);

	memcpy(sc->phy.tx_lut, tx_lut, sizeof(tx_lut));

	if (is_scm2020(sc->ver.phyver)
			&& (version(sc->ver.phyver) < 22)) {
		if(!is_chip(sc->ver.phyver)
					&& (version(sc->ver.phyver) >= 7
						|| is_scm2010(sc->ver.phyver))) {
			/* new gain table for PHY version 7 */
			memcpy(sc->phy.rx_lut, rx_lut_r2, sizeof(rx_lut_r2));
		} else {
			memcpy(sc->phy.rx_lut, rx_lut, sizeof(rx_lut));
		}
	} else {
		/* new gain table for PHY version 7 */
		memcpy(sc->phy.rx_lut, rx_lut_r2, sizeof(rx_lut_r2));
	}

	TASK_INIT(&sc->rx.refill_task, 0, rx_desc_refill, sc);
	TASK_INIT(&sc->rx.handler_task, 10, rx_done, sc);
	TASK_INIT(&sc->tx.reclaim_task, 0, tx_reclaim, sc);
	TASK_INIT(&sc->txq_drop_task, 0, scm2020_txq_drop, sc);
	TASK_INIT(&sc->ps_chk_bcn_miss_task, 0, scm2020_sta_ps_bmiss, sc);

#ifdef CONFIG_SUPPORT_TWT
	TASK_INIT(&sc->twt_timeout_task, configMAX_PRIORITIES - 1, twt_timeout_task, sc);
#endif
	sc->sc_tq = taskqueue_create_fast(dev_name(dev), M_ZERO,
			taskqueue_thread_enqueue, &sc->sc_tq);

	taskqueue_stack_size(&sc->sc_tq, 2560);
	taskqueue_start_threads(&sc->sc_tq, 1, PI_NET,
			"%s fast taskq", dev_name(dev));

	callout_init(&sc->doze_timer);

	/*
	 * IFP
	 * haiku creates a "master" ifnet which is a parent of all
	 * vap netifs.
	 */
	ifp = sc->ifp = if_alloc(IFT_IEEE80211);
	if (ifp == NULL) {
		printk("%s: could not allocate ifnet\n", dev_name(dev));
		goto fail;
	}
	ifp->if_softc = sc;
#ifdef CONFIG_SUPPORT_DUAL_INSTANCE
	if_initname(ifp, "wlan", dev_id(dev));
#else
	if_initname(ifp, "wlan", 0);
#endif
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init =  scm2020_init;
	ifp->if_ioctl = scm2020_ioctl;
	ifp->if_start = scm2020_start_tx;
	ifp->if_transmit = if_transmit;
	ifq_init((struct ifqueue *) &ifp->if_snd, ifp->if_xname);
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);

	/* IC */
	ic = sc->ic = ifp->if_l2com;
	ic->ic_softc = sc;
	ic->ic_name	= ifp->if_xname;
	ic->ic_parent = scm2020_parent;
	ic->ic_transmit = scm2020_transmit;

	wlan_mac_addr(ic->ic_macaddr);

	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */
	ic->ic_caps = 	(IEEE80211_C_STA 	| /* station mode */
			 IEEE80211_C_MONITOR	| /* monitor mode */
			 IEEE80211_C_HOSTAP 	| /* hostap mode */
			 IEEE80211_C_SHPREAMBLE | /* short preamble supported */
			 IEEE80211_C_SHSLOT 	| /* short slot time supported */
			 IEEE80211_C_WPA	| /* capable of WPA1+WPA2 */
			 IEEE80211_C_WME	| /* WME */
#ifdef CONFIG_BACKGROUND_SCAN
			 IEEE80211_C_BGSCAN	| /* capable of bg scanning */
#endif
#if defined(CONFIG_MBO) || defined(CONFIG_SUPPORT_WFA_CERTIFICATION)
			 IEEE80211_C_RADIOMEAS |
#endif
			 IEEE80211_C_PMGT	| /* CAPABILITY: Power mgmt */
			 IEEE80211_C_SWSLEEP    | /* software power management */
			 0);

	ic->ic_n_opmode = ARRAY_SIZE(scm2020_ht_cap);
	ic->ic_htcaps = scm2020_ht_cap;

#ifdef CONFIG_SUPPORT_VHT
	ic->ic_flags_ext |= IEEE80211_FEXT_VHT;
	ic->ic_vhtcaps = (
		/*	(IEEE80211_VHTCAP_MAX_MPDU_LENGTH_11454) | */
			(IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_NONE << IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_MASK_S) |
#ifdef CONFIG_SUPPORT_LDPC
			(IEEE80211_VHTCAP_RXLDPC) |
#endif
			(IEEE80211_VHTCAP_SHORT_GI_80) |
		/*	(IEEE80211_VHTCAP_SHORT_GI_160) | */
#ifdef CONFIG_SUPPORT_STBC
			(wlan_is_primary(sc) ? IEEE80211_VHTCAP_TXSTBC : 0) |
			(wlan_is_primary(sc) ? IEEE80211_VHTCAP_RXSTBC_2 : 0) |
#endif
		/*	(IEEE80211_VHTCAP_SU_BEAMFORMER_CAPABLE) | */
			(IEEE80211_VHTCAP_SU_BEAMFORMEE_CAPABLE) |
			(7 << IEEE80211_VHTCAP_BEAMFORMEE_STS_MASK_S) |
		/*	(0 << IEEE80211_VHTCAP_SOUNDING_DIMENSIONS_MASK_S) | */
		/*	(IEEE80211_VHTCAP_MU_BEAMFORMER_CAPABLE) | */
			(IEEE80211_VHTCAP_MU_BEAMFORMEE_CAPABLE) |
		/*	(IEEE80211_VHTCAP_VHT_TXOP_PS) | */
		/*	(IEEE80211_VHTCAP_HTC_VHT) | */
			(IEEE80211_VHTCAP_MAX_AMPDU_32K << IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK_S) |
		/*	(0 << IEEE80211_VHTCAP_VHT_LINK_ADAPTATION_VHT_MASK_S) | */
		/*	(IEEE80211_VHTCAP_RX_ANTENNA_PATTERN) | */
		/*	(IEEE80211_VHTCAP_TX_ANTENNA_PATTERN) | */
			0);

	ic->ic_vht_mcsinfo.rx_mcs_map = 0xFFFE;
	ic->ic_vht_mcsinfo.tx_mcs_map = 0xFFFE;
	if (wlan_is_primary(sc)) {
		for (i = 1; i < CONFIG_MIMO_CHAINS; i++) {
			ic->ic_vht_mcsinfo.rx_mcs_map = (ic->ic_vht_mcsinfo.rx_mcs_map << 2) + 2;
			ic->ic_vht_mcsinfo.tx_mcs_map = (ic->ic_vht_mcsinfo.tx_mcs_map << 2) + 2;
		}
	}
	ic->ic_vht_mcsinfo.rx_highest = 0;
	ic->ic_vht_mcsinfo.tx_highest = 0;

#endif /* CONFIG_SUPPORT_VHT */

	ic->ic_extcap = (
		/*	IEEE80211_EXTCAP_ECS | */
#if defined(CONFIG_MBO) || defined(CONFIG_SUPPORT_WFA_CERTIFICATION)
			IEEE80211_EXTCAP_BSS_TRANSITION |
#endif
#ifdef CONFIG_SUPPORT_MBSSID
			IEEE80211_EXTCAP_M_BSSID |
#endif
		/*	IEEE80211_EXTCAP_QOSMAP | */
#ifdef CONFIG_SUPPORT_VHT
			IEEE80211_EXTCAP_OPMODE_NOTIF |
#endif /* CONFIG_SUPPORT_VHT */
			0);

	ic->ic_excap_9_10 = (
#ifdef CONFIG_SUPPORT_TWT
		IEEE80211_EXTCAP10_TWT_REQUESTER |
#endif
		0);

#ifdef CONFIG_SUPPORT_DUAL_INSTANCE
	/* 2.4G AP may send 11B packets, but PHY-B does not support 11B */
	if (wlan_inst(sc) != PRIMARY_WLANID)
		ic->ic_flags_ext |= IEEE80211_FEXT_5G_ONLY;
#endif

#if defined(CONFIG_SUPPORT_STBC) || (CONFIG_MIMO_CHAINS > 1)
	if (wlan_inst(sc) == PRIMARY_WLANID)
		ic->ic_flags_ext |= IEEE80211_FEXT_MIMO;
#endif

#ifdef CONFIG_SUPPORT_HE
	ic->ic_flags_ext |= IEEE80211_FEXT_HE;

	ic->ic_he_cap = scm2020_he_cap;

#ifdef CONFIG_WLAN_HW_SCM2010
	ic->ic_he_mcsinfo.rx_mcs_map = 0xFFFC | CONFIG_SUPPORT_HE_RX_MCS_8_9;
	ic->ic_he_mcsinfo.tx_mcs_map = 0xFFFD;
#else
	ic->ic_he_mcsinfo.rx_mcs_map = 0xFFFE;
	ic->ic_he_mcsinfo.tx_mcs_map = 0xFFFE;
	if (wlan_is_primary(sc)) {
		for (i = 1; i < CONFIG_MIMO_CHAINS; i++) {
			ic->ic_he_mcsinfo.rx_mcs_map = (ic->ic_he_mcsinfo.rx_mcs_map << 2) + 2;
			ic->ic_he_mcsinfo.tx_mcs_map = (ic->ic_he_mcsinfo.tx_mcs_map << 2) + 2;
		}
	}
#endif
	ic->ic_ppe_thres_len = ppe_thres[CONFIG_MIMO_CHAINS - 1].len;
	memcpy(ic->ic_ppe_thres, ppe_thres[CONFIG_MIMO_CHAINS - 1].data, sizeof(ic->ic_ppe_thres));
#endif

	ic->ic_cryptocaps = (IEEE80211_CRYPTO_WEP 		|
				IEEE80211_CRYPTO_AES_CCM 	|
				IEEE80211_CRYPTO_AES_CCM_256 	|
				IEEE80211_CRYPTO_AES_GCM 	|
				IEEE80211_CRYPTO_AES_GCM_256 	|
				IEEE80211_CRYPTO_TKIP 		|
				IEEE80211_CRYPTO_TKIPMIC);
    headroom = sizeof(struct hw_tx_hdr) + g_sc_bds_hdr_sz + g_sc_bds_adj_offset;
#ifdef CONFIG_SUPPORT_AMSDU_TX
	/* Reserve sufficient space for AMSDU header */
	headroom += sizeof(struct ether_header);
#endif

	ic->ic_headroom = roundup(headroom, sizeof(u32));

	/* Supported channels */
	setbit(&bands, IEEE80211_MODE_11B);
	setbit(&bands, IEEE80211_MODE_11G);
	setbit(&bands, IEEE80211_MODE_11NG);
#ifdef CONFIG_IEEE80211_MODE_11A
	setbit(&bands, IEEE80211_MODE_11A);
#endif
#ifdef CONFIG_IEEE80211_MODE_11NA
	setbit(&bands, IEEE80211_MODE_11NA);
#endif
#ifdef CONFIG_IEEE80211_MODE_VHT_2GHZ
	setbit(&bands, IEEE80211_MODE_VHT_2GHZ);
#endif

#ifdef CONFIG_IEEE80211_MODE_VHT_5GHZ
	setbit(&bands, IEEE80211_MODE_VHT_5GHZ);
#endif

	ic->ic_rxstream = CONFIG_MIMO_CHAINS;
	ic->ic_txstream = CONFIG_MIMO_CHAINS;

	ieee80211_init_channels(ic, &regdomain, bands);

	ieee80211_ifattach(ic);

    ic->ic_txpowlimit = scm2020_rate2txpwr(2); /* 11b 1M will be sent w/ max. */
	sc->tv_set_txpwr_index = scm2020_tv_set_tx_pwr;
	sc->load_cal = scm2020_load_cal; /* for later reference */

	ieee80211msg_init();
#ifdef CONFIG_SUPPORT_MINIMUM_SCAN_TABLE
	/* In the STA mode, stop ieee80211com inactivity periodic timer[Power save], the STA will
	 * 1. Flush scan table when start the scan.
	 * 2. After notify the scan entry, delete the undesired ones.
	 * Then STA doesnot need ieee80211_scan_timeout in the ic_inact.
	 * Once it has SoftAP mode, enable ic_inact timer.
	 */
	callout_stop(&ic->ic_inact);
#endif

	ic->ic_newassoc = scm2020_newassoc;
	ic->ic_vap_create = scm2020_vap_create;
	ic->ic_vap_delete = scm2020_vap_delete;
	ic->ic_raw_xmit = scm2020_raw_xmit;
	ic->ic_updateslot = scm2020_update_slot;
	ic->ic_update_promisc = scm2020_update_promisc;
	sc->ampdu_rx_start = ic->ic_ampdu_rx_start;
	ic->ic_ampdu_rx_start = scm2020_ampdu_rx_start;
	sc->ampdu_rx_stop = ic->ic_ampdu_rx_stop;
	ic->ic_ampdu_rx_stop = scm2020_ampdu_rx_stop;
	ic->ic_scan_start = scm2020_scan_start;
	ic->ic_scan_end = scm2020_scan_end_chk;
	ic->ic_scan_set_mandatory_hwrates = scm2020_scan_set_mandatory_hwrates;
	ic->ic_set_channel = scm2020_set_channel;
#ifdef CONFIG_SUPPORT_VHT
	ic->ic_update_mu_group = scm2020_update_mu_group;
#endif
	ic->ic_clean_sta_aid = scm2020_set_sta_aid;
#ifdef CONFIG_SUPPORT_HE
	ic->ic_update_bss_color = scm2020_update_bss_color;
#ifdef CONFIG_SUPPORT_MU_EDCA
	ic->ic_update_muedca = scm2020_update_muedca;
#endif
#ifdef CONFIG_SUPPORT_UORA
	ic->ic_update_uora = scm2020_update_uora;
#endif
#ifdef CONFIG_SUPPORT_SR
	ic->ic_update_sr = scm2020_update_sr;
#endif
#endif

#ifdef CONFIG_SUPPORT_TWT
	ic->ic_get_tsf = scm2020_get_tsf;
	ic->ic_set_twt_timer = scm2020_set_twt_timer;
#endif

#ifdef CONFIG_SUPPORT_MCC
	if (IF_SUPPORT_DUAL_VIF(dev->flags))
		scm2020_mcc_init(ic);
#endif

	ic->ic_ifattach = netif_ifattach;
	ic->ic_ifdetach = netif_ifdetach;

	/*
	 * _ieee802111_bss_update will call ic_set_quiet if AP carry ie of quiet
	 * In order to avoid crash so give it a null function
	 */

	ic->ic_set_quiet = scm2020_set_quiet;

	scm2020_hmac_int_mask(sc, false);
	scm2020_hmac_init(sc);
	scm2020_hmac_enable(sc, true);
	scm2020_phy_init(sc);
	scm2020_rf_init(sc);
	scm2020_load_cal(sc);

	scm2020_init_tx_queue(sc);
#ifdef CONFIG_SCM2010_TX_TIMEOUT_DEBUG
	scm2020_init_tx_dbg();
#endif
#if !defined(CONFIG_LINK_TO_ROM) \
	&& defined(CONFIG_IEEE80211_DEBUG)
	/* NB: removed from the ROM library */
	ieee80211_announce(ic);
#endif

#ifdef CONFIG_IEEE80211_HOSTED
	ieee80211_set_mode(ic, IEEE80211_COM_M_HOSTED);
#elif defined(CONFIG_NUTTX) /* Ugly! */
	ieee80211_set_mode(ic, IEEE80211_COM_M_IPC);
#else
	ieee80211_set_mode(ic, IEEE80211_COM_M_STD);
#endif

	return 0;

 fail:
	return -ENOMEM;
}

#if 0
static int scm2020_detach(void *xsc)
{
	struct sc_softc *sc = xsc;
	struct ifnet *ifp = sc->ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	scm2020_stop(sc);

	ieee80211_ifdetach(ic);

	if_free(ifp);

	return 0;
}
#endif

static struct ieee80211vap *
scm2020_wlan_create_vap(struct device *dev, int idx);

__iram__ static int scm2020_wlan_start(struct device *dev)
{
	struct sc_softc *sc = dev->driver_data;
	struct ieee80211vap *vap;

	KASSERT(dev != NULL, ("dev == NULL"));
	KASSERT(sc != NULL, ("sc == NULL"));

	vap = scm2020_wlan_create_vap(dev, 0);
	sc->cvap = vap2tvap(vap);

#ifndef CONFIG_IEEE80211_HOSTED
	vap = scm2020_wlan_create_vap(dev, 1);
	KASSERT(vap != NULL, ("vap == NULL") );
#endif

	return 0;
}

__iram__ static int
scm2020_wlan_reset(struct device *dev)
{
	//TBD
	return 0;
}

static int
scm2020_wlan_remove_vap(struct device *dev, struct ieee80211vap *);

__iram__ static int
scm2020_wlan_stop(struct device *dev)
{
	struct sc_softc *sc = dev->driver_data;
	struct ieee80211vap *vap = &sc->cvap->vap;
	struct ifnet *ifp = vap->iv_ifp;

	KASSERT(ifp->if_type == IFT_ETHER, ("if_type == IFT_ETHER"));

	scm2020_wlan_remove_vap(dev, vap);

	return 0;
}

__iram__ static int
scm2020_wlan_num_max_vaps(struct device *dev)
{
	struct sc_softc *sc = dev->driver_data;

	return sc->n_vap;
}

__iram__ static struct ieee80211vap *
scm2020_wlan_create_vap(struct device *dev, int idx)
{
	struct sc_softc *sc = dev->driver_data;
	struct ifnet *ifp;
	struct ieee80211com *ic;
	struct ieee80211vap *vap;
	int id = (dev_id(dev) * 2) + idx;
	u8 vmacaddr[IEEE80211_ADDR_LEN];

	if (idx && !IF_SUPPORT_DUAL_VIF(dev->flags)) {
		return NULL;
	}

	ifp = sc->ifp;
	ic = (struct ieee80211com *)ifp->if_l2com;

	KASSERT(ic != NULL, ("ic == NULL"));

	IEEE80211_ADDR_COPY(vmacaddr, ic->ic_macaddr);
	vmacaddr[IEEE80211_ADDR_LEN - 1] += (2 * id);

	vap = ic->ic_vap_create(ic, "wlan", id, IEEE80211_M_STA, 0, NULL,
			vmacaddr);
	vap->iv_bmiss_max = CONFIG_MAX_BEACON_MISS;

	// We aren't connected to a WLAN, yet.
	if_link_state_change(vap->iv_ifp, LINK_STATE_DOWN);

	return vap;
}

__iram__ static int
scm2020_wlan_remove_vap(struct device *dev, struct ieee80211vap *vap)
{
	struct sc_softc *sc = dev->driver_data;
	struct ieee80211com* ic = sc->ic;

	ic->ic_vap_delete(vap);

	return 0;
}

__iram__ static struct ieee80211vap *
scm2020_wlan_get_vap(struct device *dev, int idx)
{
	struct sc_softc *sc = dev->driver_data;
	struct sc_vap *tvap;

	if (idx >= sc->n_vap)
		return NULL;

	tvap = &sc->tvap[idx];

	return tvap->in_use ? &tvap->vap : NULL;
}

__iram__ static int
scm2020_wlan_ctl_vap(struct device *dev, struct ieee80211vap *vap,
		u_long cmd, caddr_t data)
{
	int err;
	struct ifnet *ifp = vap->iv_ifp;

	(void)dev;

	err = ieee80211_ioctl(ifp, cmd, data);

	return err;
}

__iram__ static int
scm2020_wlan_version(struct device *dev, char *buf, int size)
{
	struct sc_softc *sc = dev->driver_data;

	snprintf(buf, size,
			"mac: 0x%x %d %c, 0x%x, phy: 0x%x %d %c, 0x%x",
			prodc(sc->ver.macver),
			version(sc->ver.macver),
			is_chip(sc->ver.macver) ? 'C' : 'F',
			sc->ver.macgit,
			prodc(sc->ver.phyver),
			version(sc->ver.phyver),
			is_chip(sc->ver.phyver) ? 'C' : 'F',
			sc->ver.phygit);

	return 0;
}

__iram__ static int
scm2020_wlan_probe(struct device *dev)
{
	struct sc_softc *sc;
	struct device *rf = device_get_by_name("rf");

	if (rf == NULL) {
		return -ENODEV;
	}

	sc = kzalloc(sizeof(*sc));

#ifdef CONFIG_DEBUG_MODE
	sc->debug_mode = true;
#endif

	sc->rf = rf;
	dev->driver_data = sc;

	scm2020_attach(dev);

	scm2020_wlan_pm_attach(sc);

	return wlan_start(dev);
}

__iram__ static int
scm2020_wlan_shutdown(struct device *dev)
{
	struct sc_softc *sc = dev->driver_data;
	struct ifnet *ifp = sc->ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		scm2020_stop(sc);

	return 0;
}

__dram__ struct wlan_ops scm2020_wlan_ops = {
	.start = scm2020_wlan_start,
	.reset = scm2020_wlan_reset,
	.stop = scm2020_wlan_stop,
	.num_max_vaps = scm2020_wlan_num_max_vaps,
	.create_vap = scm2020_wlan_create_vap,
	.remove_vap = scm2020_wlan_remove_vap,
	.get_vap = scm2020_wlan_get_vap,
	.ctl_vap = scm2020_wlan_ctl_vap,
};

static declare_driver(scm2020_wlan) = {
	.name = "scm2020-wlan",
	.probe = scm2020_wlan_probe,
	.shutdown = scm2020_wlan_shutdown,
#ifdef CONFIG_PM_DM
	.suspend = scm2020_wlan_suspend,
	.resume = scm2020_wlan_resume,
#endif
	.version = scm2020_wlan_version,
	.ops = &scm2020_wlan_ops,
};

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(wlandev, &wlandev, &_wlandev);
#else
__func_tab__  struct device *
(*wlandev)(int inst) = _wlandev;
#endif

__ilm_wlan__ struct device *_wlandev(int inst)
{
	struct device *dev;

	if (inst)
		dev = device_get_by_name("scm2020-wlan.1");
	else
		dev = device_get_by_name("scm2020-wlan");

	return dev;
}

__iram__ void *wlan_default_netif(int inst)
{
	struct device *dev;
	struct ieee80211vap *vap;
	struct netif *netif;
	int i;

	dev = wlandev(inst);
	for (i = 0; i < scm2020_wlan_num_max_vaps(dev); i++) {
		vap = scm2020_wlan_get_vap(dev, i);
		if (vap) {
			netif = &vap->iv_ifp->etherif;
			if (netif_is_up(netif) && netif_is_link_up(netif)) {
				return netif;
			}
		}
	}

	return NULL;
}

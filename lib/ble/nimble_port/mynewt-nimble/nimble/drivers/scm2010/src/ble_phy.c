/*
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

#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "hal/console.h"
#include "hal/irq.h"
#include "hal/device.h"
#include "hal/rom.h"
#include "hal/pm.h"
#include "hal/rf.h"
#include "hal/timer.h"
#include "hal/kmem.h"

#include "syscfg/syscfg.h"
#include "os/os.h"
#include "os/os_cputime.h"
#include "ble/xcvr.h"
#include "nimble/ble.h"
#include "nimble/nimble_opt.h"
#include "nimble/nimble_npl.h"
#include "controller/ble_phy.h"
#include "controller/ble_phy_trace.h"
#include "controller/ble_ll.h"
#include "controller/ble_ll_conn.h"

#include "rf.h"
#include "rf_reg.h"
#include "dma.h"
#include "sys.h"
#include "bit.h"

#define LL_TX_SETTLE            60  /* TX settling */
#define LL_TRX_SETTLE           60  /* TX-RX settling in the same channel */
#define LL_RX_SETTLE            60  /* RX settling */
#define LL_RTX_SETTLE           60  /* RX-TX settling in the same channel */
#define LL_RX_OFFSET            2   /* RX delay */

#define LL_COEX_CHECK_TIME      PTA_CHECK_TIME_US
#define LL_COEX_IO_TIME         PTA_BLE_IO_TIME_US

#define LL_MODE_SETTLE_TIME     10

#ifdef CONFIG_BLE_STATICS
struct ble_phy_statics
{
    uint8_t enable;

    uint32_t phy_1m_rx[BLE_PHY_NUM_CHANS];
    uint32_t phy_1m_rx_fail[BLE_PHY_NUM_CHANS];

    uint32_t phy_2m_rx[BLE_PHY_NUM_CHANS];
    uint32_t phy_2m_rx_fail[BLE_PHY_NUM_CHANS];

    uint32_t phy_coded_rx[BLE_PHY_NUM_CHANS];
    uint32_t phy_coded_rx_fail[BLE_PHY_NUM_CHANS];
};
#endif

extern struct ble_ll_conn_sm *g_ble_ll_conn_cur_sm;

/* BLE PHY data structure */
struct ble_phy_obj
{
    int8_t  phy_txpwr_dbm;
    uint8_t phy_chan;
    uint32_t crcinit;
    uint8_t phy_state;
    uint8_t phy_transition;
    uint8_t phy_rx_started;
    uint8_t phy_encrypted;
    uint8_t phy_privacy;
    uint8_t phy_tx_pyld_len;
    uint8_t phy_cur_phy_mode;
    uint8_t phy_tx_phy_mode;
    uint8_t phy_rx_phy_mode;
    int8_t  rx_pwr_compensation;
    uint32_t phy_access_address;
    struct ble_mbuf_hdr rxhdr;
    void *txend_arg;
    ble_phy_tx_end_func txend_cb;
    uint32_t phy_start_cputime;
    uint32_t phy_tx_end_cputime;
    uint32_t phy_rx_start_cputime;
    uint32_t phy_enc_pkt_cntr;
#ifdef CONFIG_BLE_STATICS
    struct ble_phy_statics statics;
#endif
    uint32_t pta_delay;
    uint8_t dtm;
    uint8_t dtm_chan_set;
	uint8_t adc_changed;
};
struct ble_phy_obj g_ble_phy_data;

/* XXX: if 27 byte packets desired we can make this smaller */
/* Global transmit/receive buffer */

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
#define BLE_DMA_BUFFER_ALIGNMENT		4
#define BLE_DMA_BUFFER_ALIGNMENT_MASK	(BLE_DMA_BUFFER_ALIGNMENT - 1)

static uint32_t *g_ble_phy_tx_buf;
static uint32_t *g_ble_phy_rx_buf;
static uint32_t *g_ble_phy_con_ind_buf;
#else
#define _attribute_ram_sec_     __attribute__((section(".dma_buffer")))
_attribute_ram_sec_ static uint32_t g_ble_phy_tx_buf[(BLE_PHY_MAX_PDU_LEN + 3) / 4];
_attribute_ram_sec_ static uint32_t g_ble_phy_rx_buf[(BLE_PHY_MAX_PDU_LEN + 3) / 4];
_attribute_ram_sec_ static uint32_t g_ble_phy_con_ind_buf[(BLE_PHY_MAX_PDU_LEN + 3) / 4];
#endif

uint16_t g_ble_phy_mode_pkt_start_off[BLE_PHY_NUM_MODE] = {
    [BLE_PHY_MODE_1M] = 40,
    [BLE_PHY_MODE_2M] = 24,
    [BLE_PHY_MODE_CODED_125KBPS] = 336,
    [BLE_PHY_MODE_CODED_500KBPS] = 336
};

uint16_t g_ble_phy_rx_delay[BLE_PHY_NUM_MODE] = {
    [BLE_PHY_MODE_1M] = 15,
    [BLE_PHY_MODE_2M] = 9,
    [BLE_PHY_MODE_CODED_125KBPS] = 12,
    [BLE_PHY_MODE_CODED_500KBPS] = 12
};

uint16_t g_ble_phy_tx_delay[BLE_PHY_NUM_MODE] = {
    [BLE_PHY_MODE_1M] = 5 + (8 * 0), /* If preamble length adjusted, modify this too */
    [BLE_PHY_MODE_2M] = 4 + (8 * 0), /* If preamble length adjusted, modify this too */
    [BLE_PHY_MODE_CODED_125KBPS] = 5,
    [BLE_PHY_MODE_CODED_500KBPS] = 5
};

/* Statistics */
STATS_SECT_START(ble_phy_stats)
    STATS_SECT_ENTRY(phy_isrs)
    STATS_SECT_ENTRY(tx_good)
    STATS_SECT_ENTRY(tx_fail)
    STATS_SECT_ENTRY(tx_late)
    STATS_SECT_ENTRY(tx_bytes)
    STATS_SECT_ENTRY(rx_starts)
    STATS_SECT_ENTRY(rx_aborts)
    STATS_SECT_ENTRY(rx_valid)
    STATS_SECT_ENTRY(rx_crc_err)
    STATS_SECT_ENTRY(rx_late)
    STATS_SECT_ENTRY(radio_state_errs)
    STATS_SECT_ENTRY(rx_hw_err)
    STATS_SECT_ENTRY(tx_hw_err)
STATS_SECT_END
STATS_SECT_DECL(ble_phy_stats) ble_phy_stats;

STATS_NAME_START(ble_phy_stats)
    STATS_NAME(ble_phy_stats, phy_isrs)
    STATS_NAME(ble_phy_stats, tx_good)
    STATS_NAME(ble_phy_stats, tx_fail)
    STATS_NAME(ble_phy_stats, tx_late)
    STATS_NAME(ble_phy_stats, tx_bytes)
    STATS_NAME(ble_phy_stats, rx_starts)
    STATS_NAME(ble_phy_stats, rx_aborts)
    STATS_NAME(ble_phy_stats, rx_valid)
    STATS_NAME(ble_phy_stats, rx_crc_err)
    STATS_NAME(ble_phy_stats, rx_late)
    STATS_NAME(ble_phy_stats, radio_state_errs)
    STATS_NAME(ble_phy_stats, rx_hw_err)
    STATS_NAME(ble_phy_stats, tx_hw_err)
STATS_NAME_END(ble_phy_stats)

extern __func_tab__ void (*ble_phy_chan_apply)(void);
extern __func_tab__ void (*ble_phy_mode_apply)(uint8_t phy_mode);
extern __func_tab__ void (*ble_phy_mode_coded_apply)(uint8_t direction, uint8_t phy_mode);

__ilm_ble__ static void
rf_enable_manual_standby(void)
{
    uint32_t v;

    /* enable manual mode, set standby mode */
    v = readl(RF_BASE_START + 0x04);
    v &= ~(0xFFFF);
    v |= (0x26A0);
    writel(v, RF_BASE_START + 0x04);

    /* wait manual mode settling time */
    udelay(LL_MODE_SETTLE_TIME);
}

__ilm_ble__ static void
rf_disable_manual_standby(void)
{
    uint32_t v;

    /* disable manual mode, set standby mode */
    v = readl(RF_BASE_START + 0x04);
    v &= ~(0xFFFF);
    v |= (0x22A0);
    writel(v, RF_BASE_START + 0x04);
}

__ilm_ble__ static void
rf_change_adc_clock(void)
{
    uint32_t v;

    /* ADC clock change to 80Mhz for BLE */
    v = readl(RF_BASE_START + 0x174);
    if (v & 1 << 14) {
        v &= ~(1 << 14);
        writel(v, RF_BASE_START + 0x174);
		g_ble_phy_data.adc_changed = 1;
	}
}

__ilm_ble__ static void
rf_restore_adc_clock(void)
{
	uint32_t v;

	if (g_ble_phy_data.adc_changed) {
		/* RF mode manual enable */
		v = readl(RF_BASE_START + 0x004);
		v |= (1 << 10);
        writel(v, RF_BASE_START + 0x004);


		v = readl(RF_BASE_START + 0x174);
        v |= (1 << 14);
        writel(v, RF_BASE_START + 0x174);

		/* RF mode manual disable */
		v = readl(RF_BASE_START + 0x004);
		v &= ~(1 << 10);
        writel(v, RF_BASE_START + 0x004);

		g_ble_phy_data.adc_changed = 0;
	}
}

__ilm_ble__ static void
_ble_phy_chan_apply(void)
{
    uint32_t access_addr_swap;

    if (g_ble_phy_data.dtm) {
        if (g_ble_phy_data.dtm_chan_set) {
            return;
        } else {
            g_ble_phy_data.dtm_chan_set = 1;
        }
    }

    rf_enable_manual_standby();
    rf_set_ble_channel(g_ble_phy_data.phy_chan);
    rf_disable_manual_standby();

    access_addr_swap = os_bswap_32(g_ble_phy_data.phy_access_address);
    rf_set_ble_access_code_value(access_addr_swap);

#if (BLE_LL_BT5_PHY_SUPPORTED == 1)
    rf_trigle_codedPhy_accesscode();
#endif

    rf_set_ble_crc_value(g_ble_phy_data.crcinit);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_chan_apply, &ble_phy_chan_apply, &_ble_phy_chan_apply);
#else
__func_tab__ void (*ble_phy_chan_apply)(void)
= _ble_phy_chan_apply;
#endif

#if (BLE_LL_BT5_PHY_SUPPORTED == 1)

__ilm_ble__ static void
_ble_phy_mode_coded_apply(uint8_t direction, uint8_t phy_mode)
{
    if (phy_mode == BLE_PHY_MODE_1M || phy_mode == BLE_PHY_MODE_2M) {
        return;
    }

    if (direction == BLE_PHY_STATE_RX) {
        write_reg16(0xE0470436,0x0cee);
        write_reg16(0xE0470004,0xa4f5);
    } else if (direction == BLE_PHY_STATE_TX) {
        if (phy_mode == BLE_PHY_MODE_CODED_125KBPS) {
            write_reg8(0xE0470015, 0x02);
            write_reg16(0xE0470436,0x0cf6);
            write_reg16(0xE0470004,0xb4f5);
        } else if (phy_mode == BLE_PHY_MODE_CODED_500KBPS) {
            write_reg8(0xE0470015, 0x03);
            write_reg16(0xE0470436,0x0cee);
            write_reg16(0xE0470004,0xa4f5);
        }
    }
}
#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_mode_coded_apply, &ble_phy_mode_coded_apply, &_ble_phy_mode_coded_apply);
#else
__func_tab__ void (*ble_phy_mode_coded_apply)(uint8_t direction, uint8_t phy_mode)
= _ble_phy_mode_coded_apply;
#endif

__ilm_ble__ static void
_ble_phy_mode_apply(uint8_t phy_mode)
{
    if (phy_mode == g_ble_phy_data.phy_cur_phy_mode) {
        return;
    }

    switch (phy_mode) {
    case BLE_PHY_MODE_1M:
        rf_switch_phy_1M();
        break;
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_2M_PHY)
    case BLE_PHY_MODE_2M:
        rf_switch_phy_2M();
        break;
#endif
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_CODED_PHY)
    case BLE_PHY_MODE_CODED_125KBPS:
    case BLE_PHY_MODE_CODED_500KBPS:
        rf_switch_phy_coded();
        break;
#endif
    }

    g_ble_phy_data.phy_cur_phy_mode = phy_mode;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_mode_apply, &ble_phy_mode_apply, &_ble_phy_mode_apply);
#else
__func_tab__ void (*ble_phy_mode_apply)(uint8_t phy_mode)
= _ble_phy_mode_apply;
#endif

void
_ble_phy_mode_set(uint8_t tx_phy_mode, uint8_t rx_phy_mode)
{
    g_ble_phy_data.phy_tx_phy_mode = tx_phy_mode;
    g_ble_phy_data.phy_rx_phy_mode = rx_phy_mode;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_mode_set, &ble_phy_mode_set, &_ble_phy_mode_set);
#else
__func_tab__ void (*ble_phy_mode_set)(uint8_t tx_phy_mode, uint8_t rx_phy_mode)
= _ble_phy_mode_set;
#endif

#endif

__ilm_ble__ int
_ble_phy_get_cur_phy(void)
{
#if (BLE_LL_BT5_PHY_SUPPORTED == 1)
    switch (g_ble_phy_data.phy_cur_phy_mode) {
        case BLE_PHY_MODE_1M:
            return BLE_PHY_1M;
        case BLE_PHY_MODE_2M:
            return BLE_PHY_2M;
        case BLE_PHY_MODE_CODED_125KBPS:
        case BLE_PHY_MODE_CODED_500KBPS:
            return BLE_PHY_CODED;
        default:
            assert(0);
            return -1;
    }
#else
    return BLE_PHY_1M;
#endif
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_get_cur_phy, &ble_phy_get_cur_phy, &_ble_phy_get_cur_phy);
#else
__func_tab__ int (*ble_phy_get_cur_phy)(void)
= _ble_phy_get_cur_phy;
#endif

/**
 * Copies the data from the phy receive buffer into a mbuf chain.
 *
 * @param dptr Pointer to receive buffer
 * @param rxpdu Pointer to already allocated mbuf chain
 *
 * NOTE: the packet header already has the total mbuf length in it. The
 * lengths of the individual mbufs are not set prior to calling.
 *
 */
__ilm_ble__ void
_ble_phy_rxpdu_copy(uint8_t *dptr, struct os_mbuf *rxpdu)
{
    uint32_t rem_len;
    uint32_t copy_len;
    uint32_t block_len;
    uint32_t block_rem_len;
    void *dst;
    void *src;
    struct os_mbuf * om;

    block_len = rxpdu->om_omp->omp_databuf_len;
    rem_len = OS_MBUF_PKTHDR(rxpdu)->omp_len;
    src = dptr;

    copy_len = block_len - rxpdu->om_pkthdr_len - 4;
    om = rxpdu;
    dst = om->om_data;

    while (true) {
        block_rem_len = copy_len;
        copy_len = min(copy_len, rem_len);

        dst = om->om_data;
        om->om_len = copy_len;
        rem_len -= copy_len;
        block_rem_len -= copy_len;

        memcpy(dst, src, copy_len);

        if (rem_len == 0) {
            break;
        }

        /* Move to next mbuf */
        om = SLIST_NEXT(om, om_next);
        /* KJW - this seems a bug. Let's see if this really happens. */
#if 1
        assert(0);
#endif
        copy_len = block_len;
        src += copy_len;
    }

    /* Copy header */
    memcpy(BLE_MBUF_HDR_PTR(rxpdu), &g_ble_phy_data.rxhdr,
           sizeof(struct ble_mbuf_hdr));
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_rxpdu_copy, &ble_phy_rxpdu_copy, &_ble_phy_rxpdu_copy);
#else
__func_tab__ void (*ble_phy_rxpdu_copy)(uint8_t *dptr, struct os_mbuf *rxpdu)
= _ble_phy_rxpdu_copy;
#endif

static int
ble_phy_set_start_now(void)
{
    return 0;
}

__ilm_ble__ void
_ble_phy_wfr_enable(int txrx, uint8_t tx_phy_mode, uint32_t wfr_usecs)
{
    uint8_t phy;

    phy = g_ble_phy_data.phy_cur_phy_mode;
    reg_rf_irq_mask |= FLD_RF_IRQ_FIRST_TIMEOUT;

    /*
     * We think timeout count is started when rx is enabled.
     * And LL can detect packet validation after receiving access address.
     * So, we set timeout value blow.
     * LL_RX_SETTEL(60) + LL_RX_OFFSET(2) + preamble + access address + rx delay + jitter value(active or sleep clock accuracy)
     */

    reg_rf_ll_rx_fst_timeout = LL_RX_SETTLE +
                               LL_RX_OFFSET +
                               g_ble_phy_mode_pkt_start_off[phy] +
                               g_ble_phy_rx_delay[phy] +
                               wfr_usecs;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_wfr_enable, &ble_phy_wfr_enable, &_ble_phy_wfr_enable);
#else
__func_tab__ void (*ble_phy_wfr_enable)(int txrx, uint8_t tx_phy_mode, uint32_t wfr_usecs)
= _ble_phy_wfr_enable;
#endif

__ilm_ble__ static void
ble_phy_tx_end_isr(void)
{
    uint8_t tx_phy_mode;
    uint8_t was_encrypted;
    uint8_t transition;

    /* Better be in TX state! */
    //assert(g_ble_phy_data.phy_state == BLE_PHY_STATE_TX);

    if (g_ble_phy_data.phy_state != BLE_PHY_STATE_TX) {
        return;
    }

    /* Store PHY on which we've just transmitted smth */
    tx_phy_mode = g_ble_phy_data.phy_cur_phy_mode;
    (void)tx_phy_mode;

    /* If this transmission was encrypted we need to remember it */
    was_encrypted = g_ble_phy_data.phy_encrypted;
    (void)was_encrypted;

    /* Call transmit end callback */
    if (g_ble_phy_data.txend_cb) {
        g_ble_phy_data.txend_cb(g_ble_phy_data.txend_arg);
    }

    transition = g_ble_phy_data.phy_transition;
    if (transition == BLE_PHY_TRANSITION_TX_RX) {
		if (ble_ll_state_get() == BLE_LL_STATE_ADV) {
			g_ble_phy_data.phy_state = BLE_PHY_STATE_RX;
		} else {
			uint32_t rx_settle;

			rx_settle = LL_TRX_SETTLE;

			rf_set_rx_settle_time(rx_settle);

			rx_settle += g_ble_phy_data.pta_delay;

#if (BLE_LL_BT5_PHY_SUPPORTED == 1)
			ble_phy_mode_coded_apply(BLE_PHY_STATE_RX, g_ble_phy_data.phy_rx_phy_mode);
			ble_phy_mode_apply(g_ble_phy_data.phy_rx_phy_mode);
#endif

			/*
			 * Important!!
			 * Channel must be change again after phy changed
			 */
			if (g_ble_phy_data.phy_rx_phy_mode != g_ble_phy_data.phy_tx_phy_mode) {
				rf_enable_manual_standby();
				rf_set_ble_channel(g_ble_phy_data.phy_chan);
				rf_disable_manual_standby();
			}

			ble_phy_rx();


			/* Q: Telink?
			 * What is the setting value to receive the packet at the desired time?
			 * What is the unit of reg_rf_ll_rx_fst_timeout?
			 */
			ble_phy_wfr_enable(BLE_PHY_WFR_ENABLE_TXRX, g_ble_phy_data.phy_tx_phy_mode, 4);

			/*
			 * RX enable before 2usec for active clock accuracy
			 * RX on hardware offset
			 */
			reg_rf_ll_cmd_schedule = g_ble_phy_data.phy_tx_end_cputime +
							         os_cputime_usecs_to_ticks(BLE_LL_IFS -
						             (rx_settle + LL_RX_OFFSET + 2));
			reg_rf_ll_ctrl3 |= FLD_RF_R_CMD_SCHDULE_EN;
			reg_rf_ll_cmd = 0x86;
		}
    } else {
        ble_phy_disable();
    }
}

__ilm_ble__ static void
ble_phy_rx_end_isr(void)
{
    int rc = 0;
    uint8_t *dptr;
    uint8_t crcok;
    uint32_t tx_settle;
    uint32_t tx_hw_oft;
    uint32_t tx_time_s2;
    uint32_t tx_time_s8;
    struct ble_mbuf_hdr *ble_hdr;

    /* Set RSSI and CRC status flag in header */
    ble_hdr = &g_ble_phy_data.rxhdr;
    memset(ble_hdr, 0, sizeof(struct ble_mbuf_hdr));

    ble_hdr->rxinfo.flags = ble_ll_state_get();
    ble_hdr->rxinfo.channel = g_ble_phy_data.phy_chan;
    ble_hdr->rxinfo.phy = ble_phy_get_cur_phy();
    ble_hdr->rxinfo.phy_mode = g_ble_phy_data.phy_cur_phy_mode;

    /* Q: Telink?
     * What is the offset of reg_rf_timestamp?
     */
    ble_hdr->beg_cputime = g_ble_phy_data.phy_rx_start_cputime -
        os_cputime_usecs_to_ticks(g_ble_phy_mode_pkt_start_off[ble_hdr->rxinfo.phy_mode] +
                                  g_ble_phy_rx_delay[ble_hdr->rxinfo.phy_mode]);

    dptr = (uint8_t *)g_ble_phy_rx_buf;

    ble_hdr->rxinfo.rssi = (int8_t)dptr[rf_ble_dma_rx_offset_rssi(dptr)];

    /* Count PHY crc errors and valid packets */
    crcok = rf_ble_packet_crc_ok(dptr);
    if (!crcok) {
        STATS_INC(ble_phy_stats, rx_crc_err);
    } else {
        STATS_INC(ble_phy_stats, rx_valid);
        ble_hdr->rxinfo.flags |= BLE_MBUF_HDR_F_CRC_OK;
    }

#if (BLE_LL_BT5_PHY_SUPPORTED == 1)
    ble_phy_mode_coded_apply(BLE_PHY_STATE_TX, g_ble_phy_data.phy_tx_phy_mode);
    ble_phy_mode_apply(g_ble_phy_data.phy_tx_phy_mode);
#endif

    /* Q: Telink?
     * What is the setting value to send the packet at the desired time?
     * Sometime reg_rf_timestamp is wrong.
     */

    tx_settle = LL_RTX_SETTLE;

    tx_hw_oft = g_ble_phy_tx_delay[g_ble_phy_data.phy_tx_phy_mode];

    rf_tx_settle_adjust(tx_settle);

    tx_settle += g_ble_phy_data.pta_delay;

    /*
     * Important!!
     * Channel must be change again after phy changed
     */
    if (g_ble_phy_data.phy_rx_phy_mode != g_ble_phy_data.phy_tx_phy_mode) {
        rf_enable_manual_standby();
        rf_set_ble_channel(g_ble_phy_data.phy_chan);
        rf_disable_manual_standby();
    }

    switch (ble_hdr->rxinfo.phy_mode) {
    case BLE_PHY_MODE_1M:
        g_ble_phy_data.phy_start_cputime = g_ble_phy_data.phy_rx_start_cputime +
                                           os_cputime_usecs_to_ticks(((dptr[5] + 5) * 8) +
                                                                     BLE_LL_IFS -
                                                                     g_ble_phy_rx_delay[ble_hdr->rxinfo.phy_mode]);
        break;
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_2M_PHY)
    case BLE_PHY_MODE_2M:
        g_ble_phy_data.phy_start_cputime = g_ble_phy_data.phy_rx_start_cputime +
                                           os_cputime_usecs_to_ticks(((dptr[5] + 5) * 4) +
                                                                     BLE_LL_IFS -
                                                                     g_ble_phy_rx_delay[ble_hdr->rxinfo.phy_mode]);
        break;
#endif
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_CODED_PHY)
    case BLE_PHY_MODE_CODED_125KBPS:
    case BLE_PHY_MODE_CODED_500KBPS:
        g_ble_phy_data.phy_start_cputime = g_ble_phy_data.phy_rx_start_cputime +
                                           os_cputime_usecs_to_ticks(16 + 24 + ((dptr[5] + 2) * 16) + 48 + 6 +
                                                                     BLE_LL_IFS -
                                                                     g_ble_phy_rx_delay[ble_hdr->rxinfo.phy_mode]);
        if (CPUTIME_GT(os_cputime_get32(), g_ble_phy_data.phy_start_cputime)) {
            g_ble_phy_data.phy_start_cputime = g_ble_phy_data.phy_rx_start_cputime +
                                               os_cputime_usecs_to_ticks(16 + 24 + ((dptr[5] + 2) * 64) + 192 + 24 +
                                                                         BLE_LL_IFS -
                                                                         g_ble_phy_rx_delay[ble_hdr->rxinfo.phy_mode]);
            ble_hdr->rxinfo.phy_mode = BLE_PHY_MODE_CODED_125KBPS;
        } else {
            ble_hdr->rxinfo.phy_mode = BLE_PHY_MODE_CODED_500KBPS;
        }
        break;
#endif
    default:
        assert(0);
    }

    /* set tx enable time */
    reg_rf_ll_cmd_schedule = g_ble_phy_data.phy_start_cputime - os_cputime_usecs_to_ticks(tx_settle + tx_hw_oft);

    if ((ble_hdr->rxinfo.flags & 0x07) != BLE_LL_STATE_DTM) {
        reg_rf_ll_ctrl3 |= FLD_RF_R_CMD_SCHDULE_EN;
        reg_rf_ll_cmd = 0x85;
    }

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_ENCRYPTION)
    if (g_ble_phy_data.phy_encrypted) {
        ble_hdr->rxinfo.need_dec = 1;
		if (g_ble_ll_conn_cur_sm) {
			ble_hdr->rxinfo.rx_pkt_cntr = g_ble_ll_conn_cur_sm->enc_data.rx_pkt_cntr;
		} else {
			ble_hdr->rxinfo.rx_pkt_cntr = g_ble_phy_data.phy_enc_pkt_cntr;
		}
    }
#endif

    rc = ble_ll_rx_start(dptr + 4,
                         g_ble_phy_data.phy_chan,
                         &g_ble_phy_data.rxhdr);
    if (rc < 0) {
        ble_phy_disable();
        return;
    }

    rc = ble_ll_rx_end(dptr + 4, ble_hdr);
    if (rc < 0) {
        ble_phy_disable();
    }
}

__ilm_ble__ static void
ble_phy_wifi_deny_isr(void)
{
    if (g_ble_phy_data.phy_state == BLE_PHY_STATE_TX) {
        if (g_ble_phy_data.txend_cb) {
            g_ble_phy_data.txend_cb(g_ble_phy_data.txend_arg);
        } else {
            ble_ll_wfr_timer_exp(NULL);
        }

    } else if (g_ble_phy_data.phy_state == BLE_PHY_STATE_RX) {
        ble_ll_wfr_timer_exp(NULL);
    }
}

__ilm_ble__ static int
ble_phy_isr(int arg, void *data)
{
    uint16_t irq_en;

    irq_en = reg_rf_irq_status;

    if (irq_en & FLD_RF_IRQ_RX) {
        g_ble_phy_data.phy_rx_start_cputime = reg_rf_timestamp;
        reg_rf_irq_mask &= ~(FLD_RF_IRQ_FIRST_TIMEOUT | FLD_RF_IRQ_RX_TIMEOUT);
        ble_phy_rx_end_isr();
#ifdef CONFIG_BLE_STATICS
        if (g_ble_phy_data.statics.enable) {
            if (g_ble_phy_data.phy_rx_phy_mode == BLE_PHY_MODE_1M) {
                g_ble_phy_data.statics.phy_1m_rx[g_ble_phy_data.phy_chan]++;
            } else if (g_ble_phy_data.phy_rx_phy_mode == BLE_PHY_MODE_2M) {
                g_ble_phy_data.statics.phy_2m_rx[g_ble_phy_data.phy_chan]++;
            } else {
                g_ble_phy_data.statics.phy_coded_rx[g_ble_phy_data.phy_chan]++;
            }
        }
#endif
    }

    if (irq_en & (FLD_RF_IRQ_FIRST_TIMEOUT | FLD_RF_IRQ_RX_TIMEOUT)) {

        /* Q: Telink?
         * Why does timeout interrupt occur
         * after RX end irq?
         */

        if (g_ble_phy_data.phy_state == BLE_PHY_STATE_RX) {
            if (!g_ble_phy_data.dtm) {
                ble_ll_wfr_timer_exp(NULL);
            } else {
                ble_phy_restart_rx();
            }
#ifdef CONFIG_BLE_STATICS
            if (g_ble_phy_data.statics.enable) {
                if (g_ble_phy_data.phy_rx_phy_mode == BLE_PHY_MODE_1M) {
                    g_ble_phy_data.statics.phy_1m_rx_fail[g_ble_phy_data.phy_chan]++;
                } else if (g_ble_phy_data.phy_rx_phy_mode == BLE_PHY_MODE_2M) {
                    g_ble_phy_data.statics.phy_2m_rx_fail[g_ble_phy_data.phy_chan]++;
                } else {
                    g_ble_phy_data.statics.phy_coded_rx_fail[g_ble_phy_data.phy_chan]++;
                }
            }
#endif
        }
    }

    if (irq_en & FLD_RF_IRQ_TX) {
        ble_phy_tx_end_isr();
    }

    if (irq_en & FLD_RF_IRQ_WIFI_DENY) {
        ble_phy_wifi_deny_isr();
    }

    reg_rf_irq_status = irq_en;

    return 0;
}

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
void
ble_dma_buffer_alloc(void)
{
	int dma_buf_len = BLE_PHY_MAX_PDU_LEN;

	if ((dma_buf_len & BLE_DMA_BUFFER_ALIGNMENT_MASK) != 0x00) {
		dma_buf_len += BLE_DMA_BUFFER_ALIGNMENT - (dma_buf_len & BLE_DMA_BUFFER_ALIGNMENT_MASK);
	}

	printk("dma_buf_len : %d\n", dma_buf_len);

	if (!g_ble_phy_tx_buf) {
		g_ble_phy_tx_buf = dma_kmalloc((dma_buf_len));
	}

	if (!g_ble_phy_rx_buf) {
		g_ble_phy_rx_buf = dma_kmalloc((dma_buf_len));
	}

	if (!g_ble_phy_con_ind_buf) {
		g_ble_phy_con_ind_buf = dma_kmalloc((dma_buf_len));
	}
}
#endif

/**
 * ble phy init
 *
 * Initialize the PHY.
 *
 * @return int 0: success; PHY error code otherwise
 */
int
_ble_phy_init(void)
{
    struct device *dev = device_get_by_name("ble");
    int rc;

    CLEAR_ALL_RFIRQ_STATUS;

    rf_mode_init();

    rf_set_ble_1M_mode();

    rf_switch_phy_1M();

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
	ble_dma_buffer_alloc();
#endif

    ble_tx_dma_config();

    ble_rx_dma_config();

    reg_dma_tx_wptr = 0;
    reg_dma_tx_rptr = FLD_DMA_RPTR_CLR;
    reg_dma_ctrl_reset = 1;
    reg_rf_irq_mask = FLD_RF_IRQ_RX | FLD_RF_IRQ_TX;

    if (device_get_by_name("pta")) {
        rf_set_pta_t1_time(LL_COEX_CHECK_TIME);
        rf_set_pta_t2_time(LL_COEX_IO_TIME);
        rf_3wire_pta_init(PTA_BLE_STATUS_TX);
        g_ble_phy_data.pta_delay = LL_COEX_CHECK_TIME;
		reg_rf_irq_mask |= FLD_RF_IRQ_WIFI_DENY;
    } else {
        rf_pta_disable();
        g_ble_phy_data.pta_delay = 0;
    }

    /*
     * Need Telink confirm
     * Before starting RX DMA, the TX DMA register must have
     * the following settings for normal operation
     */
    g_ble_phy_tx_buf[0] = 0x00000001;
    ble_rf_set_tx_dma(0, 252);
    reg_dma_src_addr(DMA0) = (uint32_t)g_ble_phy_tx_buf;

    //rf_set_power_level_index (RF_POWER_INDEX_P2p79dBm);

    /* Default phy to use is 1M */
    g_ble_phy_data.phy_cur_phy_mode = BLE_PHY_MODE_1M;
    g_ble_phy_data.phy_tx_phy_mode = BLE_PHY_MODE_1M;
    g_ble_phy_data.phy_rx_phy_mode = BLE_PHY_MODE_1M;

    g_ble_phy_data.rx_pwr_compensation = 0;

    /* Set phy channel to an invalid channel so first set channel works */
    g_ble_phy_data.phy_chan = BLE_PHY_NUM_CHANS;

    rc = request_irq(dev->irq[0], ble_phy_isr, "ble_irq", dev->pri[0], &g_ble_phy_data);
    if (rc) {
        printk("ble irq register failed\n");
        return -1;
    }

    printk("ble phy init %d\n", dev->irq[0]);

	/* HACK: no patch point for additional function, just call it here */
	patch_ble_ll_supp_feature();

    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_init, &ble_phy_init, &_ble_phy_init);
#else
__func_tab__ int (*ble_phy_init)(void)
= _ble_phy_init;
#endif

int
_ble_phy_rx(void)
{
	/* BLE RX start */
    rf_change_adc_clock();

    g_ble_phy_rx_buf[1] = 0;
    ble_rf_set_rx_dma((uint8_t *)g_ble_phy_rx_buf, 252);

    g_ble_phy_data.phy_state = BLE_PHY_STATE_RX;

    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_rx, &ble_phy_rx, &_ble_phy_rx);
#else
__func_tab__ int (*ble_phy_rx)(void)
= _ble_phy_rx;
#endif

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_ENCRYPTION)
void
_ble_phy_encrypt_enable(uint64_t pkt_counter, uint8_t *iv, uint8_t *key,
                       uint8_t is_master)
{
    g_ble_phy_data.phy_encrypted = 1;
    g_ble_phy_data.phy_enc_pkt_cntr = (uint32_t)pkt_counter;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_encrypt_enable, &ble_phy_encrypt_enable, &_ble_phy_encrypt_enable);
#else
__func_tab__ void (*ble_phy_encrypt_enable)(uint64_t pkt_counter, uint8_t *iv, uint8_t *key,
                       uint8_t is_master)
= _ble_phy_encrypt_enable;
#endif

void
_ble_phy_encrypt_set_pkt_cntr(uint64_t pkt_counter, int dir)
{
    g_ble_phy_data.phy_enc_pkt_cntr = (uint32_t)pkt_counter;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_encrypt_set_pkt_cntr, &ble_phy_encrypt_set_pkt_cntr, &_ble_phy_encrypt_set_pkt_cntr);
#else
__func_tab__ void (*ble_phy_encrypt_set_pkt_cntr)(uint64_t pkt_counter, int dir)
= _ble_phy_encrypt_set_pkt_cntr;
#endif

void
_ble_phy_encrypt_disable(void)
{
    g_ble_phy_data.phy_encrypted = 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_encrypt_disable, &ble_phy_encrypt_disable, &_ble_phy_encrypt_disable);
#else
__func_tab__ void (*ble_phy_encrypt_disable)(void)
= _ble_phy_encrypt_disable;
#endif

#endif

__ilm_ble__ void
_ble_phy_set_txend_cb(ble_phy_tx_end_func txend_cb, void *arg)
{
    /* Set transmit end callback and arg */
    g_ble_phy_data.txend_cb = txend_cb;
    g_ble_phy_data.txend_arg = arg;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_set_txend_cb, &ble_phy_set_txend_cb, &_ble_phy_set_txend_cb);
#else
__func_tab__ void (*ble_phy_set_txend_cb)(ble_phy_tx_end_func txend_cb, void *arg)
= _ble_phy_set_txend_cb;
#endif

int
_ble_phy_tx_set_start_time(uint32_t cputime, uint8_t rem_usecs)
{
    uint32_t start_time;
    uint32_t tx_settle;
    uint32_t tx_hw_oft;

    ble_phy_disable();

#if (BLE_LL_BT5_PHY_SUPPORTED == 1)
    ble_phy_mode_coded_apply(BLE_PHY_STATE_TX, g_ble_phy_data.phy_tx_phy_mode);
    ble_phy_mode_apply(g_ble_phy_data.phy_tx_phy_mode);
#endif

    /*
     * Important!!
     * Channel must be change again after phy changed
     */
    ble_phy_chan_apply();

    /* Q: Telink?
     * What is the setting value to send the packet at the desired time?
     */
    tx_settle = LL_TX_SETTLE;
    tx_hw_oft = g_ble_phy_tx_delay[g_ble_phy_data.phy_tx_phy_mode];

    rf_tx_settle_adjust(tx_settle); /* Temporary */

    start_time = cputime - os_cputime_usecs_to_ticks(tx_settle + tx_hw_oft); /* Temporary */
    start_time -= os_cputime_usecs_to_ticks(g_ble_phy_data.pta_delay);

    if (CPUTIME_LT(start_time + os_cputime_usecs_to_ticks(2), os_cputime_get32())) {
        return BLE_PHY_ERR_TX_LATE;
    }

    reg_rf_ll_cmd_schedule = start_time;
    reg_rf_ll_ctrl3 |= FLD_RF_R_CMD_SCHDULE_EN;

	if (!g_ble_phy_data.dtm && ble_ll_state_get() != BLE_LL_STATE_CONNECTION) {
		/*
		 * Automatically TX and RX.
		 */

		uint8_t phy;

		g_ble_phy_data.phy_state = BLE_PHY_STATE_RX;

		rf_set_rx_settle_time(LL_TRX_SETTLE);

		phy = g_ble_phy_data.phy_cur_phy_mode;
		reg_rf_irq_mask |= FLD_RF_IRQ_RX_TIMEOUT;
		reg_rf_rx_timeout = BLE_LL_IFS + LL_RX_SETTLE + LL_RX_OFFSET +
							g_ble_phy_mode_pkt_start_off[phy] +
							g_ble_phy_rx_delay[phy];

		g_ble_phy_rx_buf[1] = 0;
		ble_rf_set_rx_dma((uint8_t *)g_ble_phy_rx_buf, 252);

		reg_rf_ll_cmd = 0x87;
	} else {
		reg_rf_ll_cmd = 0x85;
	}

    g_ble_phy_data.phy_start_cputime = cputime;

    pm_stay(PM_DEVICE_BLE);

    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_tx_set_start_time, &ble_phy_tx_set_start_time, &_ble_phy_tx_set_start_time);
#else
__func_tab__ int (*ble_phy_tx_set_start_time)(uint32_t cputime, uint8_t rem_usecs)
= _ble_phy_tx_set_start_time;
#endif

int
_ble_phy_rx_set_start_time(uint32_t cputime, uint8_t rem_usecs)
{
    uint32_t start_time;

    ble_phy_disable();

#if (BLE_LL_BT5_PHY_SUPPORTED == 1)
    ble_phy_mode_coded_apply(BLE_PHY_STATE_RX, g_ble_phy_data.phy_rx_phy_mode);
    ble_phy_mode_apply(g_ble_phy_data.phy_rx_phy_mode);
#endif

    /*
     * Important!!
     * Channel must be change again after phy changed
     */
    ble_phy_chan_apply();

    /* Q: Telink?
     * What is the setting value to reciev the packet at the desired time?
     */
    rf_set_rx_settle_time(LL_RX_SETTLE); /* Temporary */

    ble_phy_rx();

    /* XXX: no need to delay Rx for g_ble_ll_sched_offset_ticks
     * just to allow a period of being deaf.
     */
    /* XXX: we don't want to patch ROM library functions for this.
     *      Instead, we will just remove g_ble_ll_sched_offset_ticks here
     *      except the case of DTM.
     */

	if (ble_ll_state_get() != BLE_LL_STATE_DTM) {
        if (cputime > g_ble_ll_sched_offset_ticks) {
            cputime -= g_ble_ll_sched_offset_ticks;
        }
    }

    start_time = cputime - os_cputime_usecs_to_ticks(LL_RX_SETTLE + LL_RX_OFFSET);/* Temporary */
    start_time -= os_cputime_usecs_to_ticks(g_ble_phy_data.pta_delay);

    /* if the start time is already passed, just start with a bit of delay
     * considering the settling time, especially for DTM
     */
    if (CPUTIME_LT(start_time, os_cputime_get32())) {
        start_time = os_cputime_get32() + os_cputime_usecs_to_ticks(50);
    }

    reg_rf_ll_cmd_schedule = start_time;
    reg_rf_ll_rx_fst_timeout = 0xFFFFFF;
    reg_rf_ll_ctrl3 |= FLD_RF_R_CMD_SCHDULE_EN;
    reg_rf_ll_cmd = 0x86;

    g_ble_phy_data.phy_start_cputime = cputime;

    pm_stay(PM_DEVICE_BLE);

    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_rx_set_start_time, &ble_phy_rx_set_start_time, &_ble_phy_rx_set_start_time);
#else
__func_tab__ int (*ble_phy_rx_set_start_time)(uint32_t cputime, uint8_t rem_usecs)
= _ble_phy_rx_set_start_time;
#endif

uint8_t *
_ble_phy_get_conn_ind_buf(void)
{
    return (uint8_t *)&g_ble_phy_con_ind_buf[1];
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_get_conn_ind_buf, &ble_phy_get_conn_ind_buf, &_ble_phy_get_conn_ind_buf);
#else
__func_tab__ uint8_t * (*ble_phy_get_conn_ind_buf)(void)
= _ble_phy_get_conn_ind_buf;
#endif

__ilm_ble__ static void
ble_phy_calc_tx_time(uint8_t payload_len)
{
    /* Set transmitted payload length */
    g_ble_phy_data.phy_tx_pyld_len = payload_len;

    switch (g_ble_phy_data.phy_cur_phy_mode) {
    case BLE_PHY_MODE_1M:
        g_ble_phy_data.phy_tx_end_cputime =
            g_ble_phy_data.phy_start_cputime + os_cputime_usecs_to_ticks((1 + 4 + 2 + payload_len + 3) * 8);
        break;
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_2M_PHY)
    case BLE_PHY_MODE_2M:
        g_ble_phy_data.phy_tx_end_cputime =
            g_ble_phy_data.phy_start_cputime + os_cputime_usecs_to_ticks((2 + 4 + 2 + payload_len + 3) * 4);
        break;
#endif
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_CODED_PHY)
    case BLE_PHY_MODE_CODED_125KBPS:
        g_ble_phy_data.phy_tx_end_cputime =
            g_ble_phy_data.phy_start_cputime +
            os_cputime_usecs_to_ticks(80 + 256 + 16 + 24 + ((2 + payload_len) * 64) + 192 + 24);
        break;
    case BLE_PHY_MODE_CODED_500KBPS:
        g_ble_phy_data.phy_tx_end_cputime =
            g_ble_phy_data.phy_start_cputime +
            os_cputime_usecs_to_ticks(80 + 256 + 16 + 24 + ((2 + payload_len) * 16) + 48 + 6);
        break;
#endif
    default:
        assert(0);
    }
}

__ilm_ble__ int
_ble_phy_tx_conn_ind(ble_phy_tx_pducb_t pducb, void *pducb_arg, uint8_t end_trans)
{
    int rc;
    uint8_t *dptr;
    uint8_t *pktptr;
    uint8_t payload_len;
    uint8_t hdr_byte;
    uint32_t dma_len;

    pktptr = (uint8_t *)&g_ble_phy_con_ind_buf[0];
    dptr = (uint8_t *)&g_ble_phy_con_ind_buf[1];

    ble_rf_set_tx_dma(0, 256);
    reg_dma_src_addr(DMA0) = (uint32_t)pktptr;

    dma_len = RF_TX_PAKET_DMA_LEN(34 + 2);
    pktptr[3] = (dma_len >> 24) & 0xff;
    pktptr[2] = (dma_len >> 16) & 0xff;
    pktptr[1] = (dma_len >> 8) & 0xff;
    pktptr[0] = dma_len & 0xff;


    /* Set PDU payload */
    payload_len = pducb(&dptr[2], pducb_arg, &hdr_byte);

    /* RAM representation has S0, LENGTH and S1 fields. (3 bytes) */
    dptr[0] = hdr_byte;
    dptr[1] = payload_len;

    /*
     * Last 3bit of connection request is SCA.
     * Sleep clock accuracy(SCA) set to 500ppm(0:251ppm ~ 500ppm)
     */
    dptr[35] &= ~(0x7 << 5);

    /* Set the PHY transition */
    g_ble_phy_data.phy_transition = end_trans;

    ble_phy_calc_tx_time(payload_len);

    /* If we already started transmitting, abort it! */
    g_ble_phy_data.phy_state = BLE_PHY_STATE_TX;

    rc = BLE_ERR_SUCCESS;

    return rc;
}
#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_tx_conn_ind, &ble_phy_tx_conn_ind, &_ble_phy_tx_conn_ind);
#else
__func_tab__ int (*ble_phy_tx_conn_ind)(ble_phy_tx_pducb_t pducb, void *pducb_arg, uint8_t end_trans)
= _ble_phy_tx_conn_ind;
#endif

__ilm_ble__ int
_ble_phy_tx(ble_phy_tx_pducb_t pducb, void *pducb_arg, uint8_t end_trans)
{
    int rc;
    uint8_t *dptr;
    uint8_t *pktptr;
    uint8_t payload_len;
    uint8_t hdr_byte;
    uint32_t dma_len;

	/* BLE TX start */
    rf_change_adc_clock();

    pktptr = (uint8_t *)&g_ble_phy_tx_buf[0];
    dptr = (uint8_t *)&g_ble_phy_tx_buf[1];

    /* Set PDU payload */
    payload_len = pducb(&dptr[2], pducb_arg, &hdr_byte);

    /* RAM representation has S0, LENGTH and S1 fields. (3 bytes) */
    dptr[0] = hdr_byte;
    dptr[1] = payload_len;

    dma_len = RF_TX_PAKET_DMA_LEN(payload_len + 2);
    pktptr[3] = (dma_len >> 24) & 0xff;
    pktptr[2] = (dma_len >> 16) & 0xff;
    pktptr[1] = (dma_len >> 8) & 0xff;
    pktptr[0] = dma_len & 0xff;

    ble_rf_set_tx_dma(0, 256);
    reg_dma_src_addr(DMA0) = (uint32_t)pktptr;

    /* Set the PHY transition */
    g_ble_phy_data.phy_transition = end_trans;

    ble_phy_calc_tx_time(payload_len);

    /* If we already started transmitting, abort it! */
    g_ble_phy_data.phy_state = BLE_PHY_STATE_TX;

    rc = BLE_ERR_SUCCESS;

    return rc;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_tx, &ble_phy_tx, &_ble_phy_tx);
#else
__func_tab__ int (*ble_phy_tx)(ble_phy_tx_pducb_t pducb, void *pducb_arg, uint8_t end_trans)
= _ble_phy_tx;
#endif

int
_ble_phy_txpwr_set(int dbm)
{
    /* "Rail" power level if outside supported range */
    dbm = ble_phy_txpower_round(dbm);

    g_ble_phy_data.phy_txpwr_dbm = dbm;

    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_txpwr_set, &ble_phy_txpwr_set, &_ble_phy_txpwr_set);
#else
__func_tab__ int (*ble_phy_txpwr_set)(int dbm)
= _ble_phy_txpwr_set;
#endif

int
_ble_phy_txpower_round(int dbm)
{
    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_txpower_round, &ble_phy_txpower_round, &_ble_phy_txpower_round);
#else
__func_tab__ int (*ble_phy_txpower_round)(int dbm)
= _ble_phy_txpower_round;
#endif

int
_ble_phy_txpwr_get(void)
{
    return g_ble_phy_data.phy_txpwr_dbm;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_txpwr_get, &ble_phy_txpwr_get, &_ble_phy_txpwr_get);
#else
__func_tab__ int (*ble_phy_txpwr_get)(void)
= _ble_phy_txpwr_get;
#endif

void
_ble_phy_set_rx_pwr_compensation(int8_t compensation)
{
    g_ble_phy_data.rx_pwr_compensation = compensation;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_set_rx_pwr_compensation, &ble_phy_set_rx_pwr_compensation, &_ble_phy_set_rx_pwr_compensation);
#else
__func_tab__ void (*ble_phy_set_rx_pwr_compensation)(int8_t compensation)
= _ble_phy_set_rx_pwr_compensation;
#endif

int
_ble_phy_setchan(uint8_t chan, uint32_t access_addr, uint32_t crcinit)
{
    g_ble_phy_data.phy_chan = chan;
    g_ble_phy_data.phy_access_address = access_addr;
    g_ble_phy_data.crcinit = crcinit;

    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_setchan, &ble_phy_setchan, &_ble_phy_setchan);
#else
__func_tab__ int (*ble_phy_setchan)(uint8_t chan, uint32_t access_addr, uint32_t crcinit)
= _ble_phy_setchan;
#endif

void
_ble_phy_restart_rx(void)
{
    ble_phy_disable();

    ble_phy_rx();

    rf_set_rx_settle_time(LL_RX_SETTLE); /* Temporary */
    reg_rf_ll_cmd_schedule = os_cputime_get32();
    reg_rf_ll_rx_fst_timeout = 0xFFFFFF;
    reg_rf_ll_ctrl3 |= FLD_RF_R_CMD_SCHDULE_EN;
    reg_rf_ll_cmd = 0x86;

    pm_stay(PM_DEVICE_BLE);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_restart_rx, &ble_phy_restart_rx, &_ble_phy_restart_rx);
#else
__func_tab__ void (*ble_phy_restart_rx)(void)
= _ble_phy_restart_rx;
#endif

void
_ble_phy_disable(void)
{

	/* BLE operation complete */
	rf_restore_adc_clock();

    /* Q: Telink?
     * Can rf_set_tx_rx_off() function be disable
     * even during TX or RX ?
     */
    g_ble_phy_data.phy_state = BLE_PHY_STATE_IDLE;
    rf_set_tx_rx_off_auto_mode();
    rf_set_tx_rx_off();

    pm_relax(PM_DEVICE_BLE);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_disable, &ble_phy_disable, &_ble_phy_disable);
#else
__func_tab__ void (*ble_phy_disable)(void)
= _ble_phy_disable;
#endif

uint32_t
_ble_phy_access_addr_get(void)
{
    return g_ble_phy_data.phy_access_address;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_access_addr_get, &ble_phy_access_addr_get, &_ble_phy_access_addr_get);
#else
__func_tab__ uint32_t (*ble_phy_access_addr_get)(void)
= _ble_phy_access_addr_get;
#endif

int
_ble_phy_state_get(void)
{
    return g_ble_phy_data.phy_state;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_state_get, &ble_phy_state_get, &_ble_phy_state_get);
#else
__func_tab__ int (*ble_phy_state_get)(void)
= _ble_phy_state_get;
#endif

int
_ble_phy_rx_started(void)
{
    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_rx_started, &ble_phy_rx_started, &_ble_phy_rx_started);
#else
__func_tab__ int (*ble_phy_rx_started)(void)
= _ble_phy_rx_started;
#endif

uint8_t
_ble_phy_xcvr_state_get(void)
{
    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_xcvr_state_get, &ble_phy_xcvr_state_get, &_ble_phy_xcvr_state_get);
#else
__func_tab__ uint8_t (*ble_phy_xcvr_state_get)(void)
= _ble_phy_xcvr_state_get;
#endif

uint8_t
_ble_phy_max_data_pdu_pyld(void)
{
    return BLE_LL_DATA_PDU_MAX_PYLD;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_max_data_pdu_pyld, &ble_phy_max_data_pdu_pyld, &_ble_phy_max_data_pdu_pyld);
#else
__func_tab__ uint8_t (*ble_phy_max_data_pdu_pyld)(void)
= _ble_phy_max_data_pdu_pyld;
#endif

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PRIVACY)
void
_ble_phy_resolv_list_enable(void)
{
    /*
     * a flag to mark privacy is enabled
     */
    g_ble_phy_data.phy_privacy = 1;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_resolv_list_enable, &ble_phy_resolv_list_enable, &_ble_phy_resolv_list_enable);
#else
__func_tab__ void (*ble_phy_resolv_list_enable)(void)
= _ble_phy_resolv_list_enable;
#endif

void
_ble_phy_resolv_list_disable(void)
{
    /*
     * a flag to mark privacy is disabled
     */
    g_ble_phy_data.phy_privacy = 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_resolv_list_disable, &ble_phy_resolv_list_disable, &_ble_phy_resolv_list_disable);
#else
__func_tab__ void (*ble_phy_resolv_list_disable)(void)
= _ble_phy_resolv_list_disable;
#endif

uint8_t
_ble_phy_resolv_list_enabled(void)
{
    /*
     * a flag to mark privacy is disabled
     */
    return g_ble_phy_data.phy_privacy;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_resolv_list_enabled, &ble_phy_resolv_list_enabled, &_ble_phy_resolv_list_enabled);
#else
__func_tab__ uint8_t (*ble_phy_resolv_list_enabled)(void)
= _ble_phy_resolv_list_enabled;
#endif

#endif

#if MYNEWT_VAL(BLE_LL_DTM)
void _ble_phy_enable_dtm(void)
{
    reg_rf_tx_mode2 &= (~FLD_RF_V_PN_EN);
    g_ble_phy_data.dtm = 1;
    g_ble_phy_data.dtm_chan_set = 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_enable_dtm, &ble_phy_enable_dtm, &_ble_phy_enable_dtm);
#else
__func_tab__ void (*ble_phy_enable_dtm)(void)
= _ble_phy_enable_dtm;
#endif

void _ble_phy_disable_dtm(void)
{
    reg_rf_tx_mode2 |= (FLD_RF_V_PN_EN);
    g_ble_phy_data.dtm = 0;
    g_ble_phy_data.dtm_chan_set = 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_disable_dtm, &ble_phy_disable_dtm, &_ble_phy_disable_dtm);
#else
__func_tab__ void (*ble_phy_disable_dtm)(void)
= _ble_phy_disable_dtm;
#endif

#endif

void
_ble_phy_rfclk_enable(void)
{
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_rfclk_enable, &ble_phy_rfclk_enable, &_ble_phy_rfclk_enable);
#else
__func_tab__ void (*ble_phy_rfclk_enable)(void)
= _ble_phy_rfclk_enable;
#endif

void
_ble_phy_rfclk_disable(void)
{
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_phy_rfclk_disable, &ble_phy_rfclk_disable, &_ble_phy_rfclk_disable);
#else
__func_tab__ void (*ble_phy_rfclk_disable)(void)
= _ble_phy_rfclk_disable;
#endif


#ifdef CONFIG_CMDLINE

#ifdef CONFIG_BLE_STATICS

#include <cli.h>

int
do_ble_phy_statics_start(int argc, char *argv[])
{
    memset(&g_ble_phy_data.statics, 0, sizeof(struct ble_phy_statics));
    g_ble_phy_data.statics.enable = 1;

	return 0;
}

int
do_ble_phy_statics_stop(int argc, char *argv[])
{
    g_ble_phy_data.statics.enable = 0;

	return 0;
}

int
do_ble_phy_statics_show(int argc, char *argv[])
{
	uint32_t sum;
	uint32_t sum_per;
	float total_per;
	int phy;
    int i;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	phy = atoi(argv[1]);

    if (phy & (1 << 0)) {
		sum = 0;
		sum_per = 0;
        printf("1M phys\n");
        printf("ch        total       timeout      per\n");
        for (i = 0; i < BLE_PHY_NUM_CHANS; i++) {
            uint32_t total = g_ble_phy_data.statics.phy_1m_rx[i] + g_ble_phy_data.statics.phy_1m_rx_fail[i];
            float per = 0;
            if (total) {
                per = (float)(g_ble_phy_data.statics.phy_1m_rx_fail[i] * 100) / total;
            }

            printf("%02d        %04d         %04d        %.1f\n",
                    i,
                    total,
                    g_ble_phy_data.statics.phy_1m_rx_fail[i],
                    per);

			sum += total;
			sum_per += g_ble_phy_data.statics.phy_1m_rx_fail[i];
        }

		total_per = (float)(sum_per * 100) / sum;
		printf("total PER %.1f\n", total_per);
    }

    if (phy & (1 << 1)) {
		sum = 0;
		sum_per = 0;
        printf("\n");
        printf("2M phys\n");
        printf("ch        total       timeout      per\n");
        for (i = 0; i < BLE_PHY_NUM_CHANS; i++) {
            uint32_t total = g_ble_phy_data.statics.phy_2m_rx[i] + g_ble_phy_data.statics.phy_2m_rx_fail[i];
            float per = 0;
            if (total) {
                per = (float)(g_ble_phy_data.statics.phy_2m_rx_fail[i] * 100) / total;
            }

            printf("%02d        %04d         %04d        %.1f\n",
                    i,
                    total,
                    g_ble_phy_data.statics.phy_2m_rx_fail[i],
                    per);


			sum += total;
			sum_per += g_ble_phy_data.statics.phy_2m_rx_fail[i];
        }

		total_per = (float)(sum_per * 100) / sum;
		printf("total PER %.1f\n", total_per);
    }

    if (phy & (1 << 2)) {
		sum = 0;
		sum_per = 0;
        printf("\n");
        printf("Coded phys\n");
        printf("ch        total       timeout      per\n");
        for (i = 0; i < BLE_PHY_NUM_CHANS; i++) {
            uint32_t total = g_ble_phy_data.statics.phy_coded_rx[i] + g_ble_phy_data.statics.phy_coded_rx_fail[i];
            float per = 0;
            if (total) {
                per = (float)(g_ble_phy_data.statics.phy_coded_rx_fail[i] * 100) / total;
            }

            printf("%02d        %04d         %04d        %.1f\n",
                    i,
                    total,
                    g_ble_phy_data.statics.phy_coded_rx_fail[i],
                    per);

			sum += total;
			sum_per += g_ble_phy_data.statics.phy_coded_rx_fail[i];
        }

		total_per = (float)(sum_per * 100) / sum;
		printf("total PER %.1f\n", total_per);
    }

	return 0;
}

static const struct cli_cmd ble_statics_cmd[] = {
	CMDENTRY(start, do_ble_phy_statics_start, "", ""),
	CMDENTRY(stop, do_ble_phy_statics_stop, "", ""),
	CMDENTRY(show, do_ble_phy_statics_show, "", ""),
};

static int do_ble_statics(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], ble_statics_cmd, ARRAY_SIZE(ble_statics_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(bs, do_ble_statics,
		"ble RX statics command",
		"bs start" OR
		"bs stop" OR
		"bs show <phy>"
);

#endif

#endif

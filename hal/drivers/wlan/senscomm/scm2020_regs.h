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
#ifndef __SCM2020_REG_H__
#define __SCM2020_REG_H__

/*
 * MAC registers
 */
#if defined(CONFIG_WLAN_HW_SCM2010)
#include "macregs-scm2010.h"
#elif defined(CONFIG_WLAN_HW_TL7118)
#include "macregs-scm2010.h" /* subject to change */
#else
#include "macregs-scm2020.h"
#endif

/* VIF */
#define REG_VIF_DIFF					(REG_VIF1_CFG0 - REG_VIF0_CFG0)
#define REG_VIF_OFFSET(r, n)				(REG_VIF0_##r + n * REG_VIF_DIFF)
#define REG_VIF_CFG0(n)					(REG_VIF_OFFSET(CFG0, n))
#define REG_VIF_CFG1(n)					(REG_VIF_OFFSET(CFG1, n))
#define REG_VIF_RX_FILTER0(n)				(REG_VIF_OFFSET(RX_FILTER0, n))
#define REG_VIF_RX_FILTER1(n)				(REG_VIF_OFFSET(RX_FILTER1, n))
#define REG_VIF_MAC0_L(n)				(REG_VIF_OFFSET(MAC0_L, n))
#define REG_VIF_MAC0_H(n)				(REG_VIF_OFFSET(MAC0_H, n))
#define REG_VIF_MAC1_L(n)				(REG_VIF_OFFSET(MAC1_L, n))
#define REG_VIF_MAC1_H(n)				(REG_VIF_OFFSET(MAC1_H, n))
#define REG_VIF_BSS0_L(n)				(REG_VIF_OFFSET(BSS0_L, n))
#define REG_VIF_BSS0_H(n)				(REG_VIF_OFFSET(BSS0_H, n))
#define REG_VIF_BSS1_L(n)				(REG_VIF_OFFSET(BSS1_L, n))
#define REG_VIF_BSS1_H(n)				(REG_VIF_OFFSET(BSS1_H, n))
#define REG_VIF_AID(n)					(REG_VIF_OFFSET(AID, n))
#define REG_VIF_BFM0(n)					(REG_VIF_OFFSET(BFM0, n))
#define REG_VIF_BFM1(n)					(REG_VIF_OFFSET(BFM1, n))
#define REG_VIF_TSF_L(n)				(REG_VIF_OFFSET(TSF_L, n))
#define REG_VIF_TSF_H(n)				(REG_VIF_OFFSET(TSF_H, n))
#define REG_VIF_TSF_OFFSET_L(n)				(REG_VIF_OFFSET(TSF_OFFSET_L, n))
#define REG_VIF_TSF_OFFSET_H(n)				(REG_VIF_OFFSET(TSF_OFFSET_H, n))
#define REG_VIF_TSF_TIME0(n)				(REG_VIF_OFFSET(TSF_TIME0, n))
#define REG_VIF_TSF_TIME1(n)				(REG_VIF_OFFSET(TSF_TIME1, n))
#define REG_VIF_TRIG_RESP_QUEUE_EN(n)			(REG_VIF_OFFSET(TRIG_RESP_QUEUE_EN, n))
#define REG_VIF_BSR(n)					(REG_VIF_OFFSET(BSR, n))
#define REG_VIF_MIN_MPDU_SPACING(n)			(REG_VIF_OFFSET(MIN_MPDU_SPACING, n))
#define REG_VIF_QNULL_CTRL_AC(n, m)			(REG_VIF_OFFSET(QNULL_CTRL_AC0, n) + 4 * m)
#define REG_VIF_UORA_CFG(n)				(REG_VIF_OFFSET(UORA_CFG, n))
#define REG_VIF_UORA_CFG1(n)				(REG_VIF_OFFSET(UORA_CFG1, n))
#define REG_VIF_NON_SRG_OBSS_PD(n)			(REG_VIF_OFFSET(NON_SRG_OBSS_PD, n))
#define REG_VIF_SRG_OBSS_PD(n)				(REG_VIF_OFFSET(SRG_OBSS_PD, n))
#define REG_VIF_TX_PWR(n)				(REG_VIF_OFFSET(TX_PWR, n))
#define REG_VIF_CFG_INFO(n)				(REG_VIF_OFFSET(CFG_INFO, n))
#define REG_VIF_MBSSID_MASK_L32(n)			(REG_VIF_OFFSET(MBSSID_MASK_L32, n))
#define REG_VIF_MBSSID_MASK_H16(n)			(REG_VIF_OFFSET(MBSSID_MASK_H16, n))
#define REG_VIF_BASIC_TRIG_PSDU_LEN(n)		(REG_VIF_OFFSET(BASIC_TRIG_PSDU_LEN, n))

/* TX */
#define REG_TX_DIFF					(REG_TX_QUEN_CMD1 - REG_TX_QUEN_CMD0)
#define REG_TX_OFFSET(r, n)				(REG_TX_##r##0 + n * REG_TX_DIFF)

#define REG_TX_QUEN_CMD(n)				(REG_TX_OFFSET(QUEN_CMD, n))
#define REG_TX_QUEN_TXOP(n)				(REG_TX_OFFSET(QUEN_TXOP, n))
#define REG_TX_QUEN_STATE(n)				(REG_TX_OFFSET(QUEN_STATE, n))
#define REG_TX_QUEN_TIMER(n)				(REG_TX_OFFSET(QUEN_TIMER, n))

#if defined(CONFIG_SUPPORT_UORA) || defined(CONFIG_SUPPORT_MU_EDCA)
#define TX_QUEN_CMD0_AID_MATCHED_QUEUE_EN_MASK                 0x2
#define TX_QUEN_CMD0_AID_MATCHED_QUEUE_EN_SHIFT                  1
#define TX_QUEN_CMD0_ASSO_UORA_QUEUE_EN_MASK                   0x4
#define TX_QUEN_CMD0_ASSO_UORA_QUEUE_EN_SHIFT                    2
#define TX_QUEN_CMD0_UNASSO_UORA_QUEUE_EN_MASK                 0x8
#define TX_QUEN_CMD0_UNASSO_UORA_QUEUE_EN_SHIFT                  3
#endif

/* Misc */
#define REG_HE_TB_PWR(n)				(REG_HE_TB_PWR01 + (n / 2) * 4)

#define bit_for_subtype(s)	(1 << (IEEE80211_FC0_SUBTYPE_##s >> IEEE80211_FC0_SUBTYPE_SHIFT))
#define mgnt(_stype) ( (_stype) << VIF0_RX_FILTER1_MGNT_FRAME_FILTER_BITMAP_SHIFT)
#define ctrl(_stype) ( (_stype) << VIF0_RX_FILTER1_CTRL_FRAME_FILTER_BITMAP_SHIFT)

static inline u32 mac_readl(struct device *dev, u32 offset)
{
	return readl(dev->base[0] + MAC_REG_OFFSET + offset);
}

static inline void mac_writel(u32 val, struct device *dev, u32 offset)
{
	writel(val, dev->base[0] + MAC_REG_OFFSET + offset);
}

/*
 * PHY registers
 */
#if defined(CONFIG_WLAN_HW_SCM2010)
#include "phyregs-scm2010.h"
#elif defined(CONFIG_WLAN_HW_TL7118)
#include "phyregs-tl7118.h" /* subject to change */
#else
#include "phyregs-scm2020.h"
#endif

#define PHY_MDM_OFFSET		(0x00000000)
#define PHY_RFI_OFFSET		(0x00008000)
#define PHY_REG_BANKSZ		(0x00004000)

typedef enum {
	PHY_MODEM		= 0,
	PHY_RFITF		= 1,
	PHY_NBLCK	 	= 2,
} PHY_BLOCK;

struct reg_set {
	PHY_BLOCK	blk;
	u16		offset;
	u32		val;
};

#define RSET(b, o, v)	{b, o, v}

#define PHY_SET_REGS(sc, regs, n)	do {			\
		const struct reg_set *r;			\
		int i;						\
		for (i = 0, r = &regs[0]; 			\
				i < n; i++, r++)	\
			phy_writel_mimo(r->val, sc, r->blk,	\
					r->offset);		\
} while(0)

#define PHY_BLK_OFFSET(b)	(PHY_MDM_OFFSET + b * (PHY_RFI_OFFSET - PHY_MDM_OFFSET))

#define PHY_VINTF_DIFF		(FW_OPBW_PRICH_VINTF1 - FW_OPBW_PRICH_VINTF0)
#define FW_OPBW_PRICH(v)	(FW_OPBW_PRICH_VINTF0 + PHY_VINTF_DIFF * v)
#define FW_SIGEXT(v)		(FW_SIGEXT_VINTF0 + PHY_VINTF_DIFF * v)
#ifdef CONFIG_WLAN_HW_SCM2020
#define FW_MIMO_EN(v)		(FW_MIMO_EN_VINTF0 + PHY_VINTF_DIFF * v)
#endif
#define FW_VHT_CONFIG(n, v)	(FW_VHT_CONFIG##n##_VINTF0 + PHY_VINTF_DIFF * v)
#define FW_HE_CONFIG(n, v)	(FW_HE_CONFIG##n##_VINTF0 + PHY_VINTF_DIFF * v)

/**
 * RF interface
 */

/* RFI_CTRLSIG_CFG_XX fields */
#define RFI_CTRLSIG_CFG(x)	(FW_RFI_CTRLSIG_CFG_00 + (x * 4))

#define RFI_CTRLSIG_CFG_EN	BIT(21)
#define RFI_CTRLSIG_CFG_POL	BIT(20)
#define RFI_CTRLSIG_CFG_SSEL	GENMASK(19, 18);
#define RFI_CTRLSIG_CFG_ESEL	GENMASK(17, 16);
#define RFI_CTRLSIG_CFG_D0	GENMASK(15,  8);
#define RFI_CTRLSIG_CFG_D1	GENMASK(15,  8);

#define TXEN			0x0
#define TXVALID			0x1
#define RXEN			0x2
#define ALWAYS			0x3

#define HIGH			0x0
#define LOW			0x1

#define RFCTRL(en, pol, on, off, d0, d1)	  \
	(((en) << 21) |				  \
	 ((pol) << 20) |			  \
	 (((on) & 0x3) << 18) |			  \
	 (((off) & 0x3) << 16) |		  \
	 (((d0) & 0xf) << 8) |			  \
	 ((d1) & 0xf))

static inline u32 phy_readl(struct device *dev, PHY_BLOCK blk, u16 offset)
{
	return readl(dev->base[1] + PHY_BLK_OFFSET(blk) + PHY_REG_BANKSZ * dev_id(dev) + offset);
}

static inline void phy_writel(u32 val, struct device *dev, PHY_BLOCK blk, u8 path, u16 offset)
{
	writel(val, dev->base[1] + PHY_BLK_OFFSET(blk) + PHY_REG_BANKSZ * path + offset);
}

#ifdef CONFIG_NET80211
static inline void phy_writel_mimo(u32 val, struct sc_softc *sc, PHY_BLOCK blk, u16 offset)
{
	if (ieee80211_mimo_enabled(sc->ic)) {
		phy_writel(val, sc->dev, blk, wlan_inst(sc), offset);
		phy_writel(val, sc->dev, blk, wlan_other_inst(sc), offset);
	}
	else
		phy_writel(val, sc->dev, blk, wlan_inst(sc), offset);
}
#endif /* CONFIG_NET80211 */

#endif

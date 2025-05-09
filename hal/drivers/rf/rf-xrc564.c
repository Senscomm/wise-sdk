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

#include <soc.h>
#include <hal/kernel.h>
#include <hal/console.h>
#include <hal/kmem.h>
#include "compat_param.h"
#include "systm.h"

#include <hal/device.h>
#include <hal/timer.h>
#include <hal/unaligned.h>
#include <hal/compiler.h>
#include <hal/bitops.h>
#include <hal/irq.h>
#include <hal/adc.h>
#include <hal/rf.h>	// will be dealt with later
#include <stdlib.h>

//#define __fib_v1__

u8 _rf_dbg = 0;
static struct xrc_driver_data {
	reg_write writefn;
	reg_read readfn;
} xrc_drv_data;

struct xrc_bt_rf_lut { // will be dealt with later
	u32 tx_lut[32];
	u32 rx_lut[5];
};

#define drv(dev)        ((struct xrc_driver_data *)(dev->driver_data))
#define regw(a, v)      (*drv(dev)->writefn)(a, v)
#define regr(a)         (*drv(dev)->readfn)(a)

#define phyver()        ((regr(0x000003f4) & 0xfff0) >> 4)
#define macver()        (((*(u32*)0x65008018) & 0xfff0) >> 4)
#define prodc()         ((regr(0x000003f4) & 0xffff0000) >> 16)
#define is_scm2020()    (prodc() == 0xB0BA)
#define is_scm2010()    (prodc() == 0xDCAF)

#define assert_spi(dev)	assert(drv(dev)->writefn && drv(dev)->readfn)

#define PHY_RFI_OFFSET	(0x00008000)

#define FW_PWR_EN		0x0000
#define FW_RFI_EN		0x0000
#define FW_RFI_XRC_CFG		0x0004
#define FW_RFI_XRC_CFG2	0x0008
#define FW_RFI_CTRLSIG_CFG_00	0x0010
#define FW_RFI_CTRLSIG_CFG_01	0x0014
#define PHY_RFI_TX_SIGGEN_CFG	0x0300
#define PHY_RFI_TX_TONEGEN_0	0x0304
#define PHY_RFI_TX_TONEGEN_1    0x0308
#define PHY_RFI_TX_TONEGEN_2    0x030C
#define PHY_RFI_TX_TONEGEN_3    0x0310
#define PHY_RFI_TX_TONEGEN_4    0x0314
#define PHY_RFI_TX_TONEGEN_5    0x0318
#define PHY_RFI_TX_TONEGEN_6    0x031C
#define PHY_RFI_TX_TONEGEN_7    0x0320
#define PHY_RFI_TX_DIG_GAIN_MAN	0x0220
#define FW_RXLUT_CFG0	0x0800
#define FW_RXLUT_MANSET_RXGAIN_OUT	0x0814
#define FW_RFI_RXDC_HPF_CFG	0x0520
#define WIFI_BLE_CTRLMUX_CFG1	0x7E0380 /* SYS_REG config */
#ifndef __fib_v1__
#define WF_PHY_CFG              0x7E0264 /* SYS_REG config */
#define DFE_CLK_CFG             0x7E0378 /* SYS_REG config */
//#define AUXADC_CTRL		0x7E0270 /* SYS_REG config */
#define AUXADC_DATA_0		0x7E0350 /* SYS_REG config */
#else
#define PMU_DLCPD_MPACR_ADDR	0x8E0004 /* PMU_REG config */
#define PMU_DLCPD_MPACR_DATA	0x8E0008 /* PMU_REG config */
#define PMU_DLCPD_IMR           0x8E000C /* PMU_REG config */
#define PMU_DLCPD_IFR           0x8E0010 /* PMU_REG config */
#endif
#define PHY_RFI_TESTMSR_CFG	0x0600
#define PHY_RFI_TESTMSR_TONECFG	0x0608
#define FW_RFI_TX_LO_LEAK_PARA	0x021C
#define FW_RFI_TX_IQ_MIS_COMP	0x0214
#define FW_RFI_RX_IQ_MIS_COMP	0x0514 // for IRR cal
#define PHY_RFI_TESTMSR_START	0x0604
#define PHY_RFI_TONEMSR0_QCH	0x062C
#define PHY_RFI_TONEMSR1_QCH	0x0638
#define PHY_RFI_TESTMSR_IDX	0x0610
#define FW_TXLUT_CFG0		0x1000
#define FW_RFI_TXLUTV2_ADDR	0x0120
#define FW_RFI_TXLUTV2_WRDATA	0x0124
#define FW_RFI_TXLUTV2_WRSTART	0x0128
#define FW_RFI_TX_LO_LEAK_MAN	0x0218
#define PHY_RFI_TONEMSR0_SUM	0x0630 // for IRR cal
#define PHY_RFI_TONEMSR1_SUM	0x063C // for IRR cal

static void phy_modem_write(struct device *dev, u32 offset, u32 data)
{
	regw(offset, data);
	if(_rf_dbg == 1)
		printf("[SCM2010_Write] 0x%08X 0x%08X\n", 0xf0f20000 + offset, data);
}

static u32 phy_modem_read(struct device *dev, u32 offset)
{
	u32 value = 0;
	value = regr(offset);
	if(_rf_dbg == 1)
		printf("[SCM2010__Read] 0x%08X 0x%08X\n", 0xf0f20000 + offset, value);

	return value;
}

static void phy_write(struct device *dev, u16 offset, u32 data)
{
	regw(PHY_RFI_OFFSET + offset, data);
	if(_rf_dbg == 1)
		printf("[SCM2010_Write] 0x%08X 0x%08X\n", 0xf0f20000 + PHY_RFI_OFFSET + offset, data);
}

static u32 phy_read(struct device *dev, u16 offset)
{
	u32 value = 0;
	value = regr(PHY_RFI_OFFSET + offset);
	if(_rf_dbg == 1)
		printf("[SCM2010__Read] 0x%08X 0x%08X\n", 0xf0f20000 + PHY_RFI_OFFSET + offset, value);

	return value;
}

static void xrc_write(struct device *dev, u32 addr, u32 data, u8 msb, u8 lsb)
{
	u32 v;
	v = readl(dev->base[0] + addr * 4);
	v &= ~GENMASK(msb, lsb);
	writel(v | (data << lsb), dev->base[0] + addr * 4);
	if(_rf_dbg == 1)
		printf("[XRC_RF_Write] 0x%08X 0x%08X\n"
				, dev->base[0] + addr * 4
				, v | (data << lsb));
}

static u16 xrc_read(struct device *dev, u32 addr)
{
	u16 v = readl((u32)dev->base[0] + addr * 4);
	if(_rf_dbg == 1)
		printf("[XRC_RF__Read] 0x%08X 0x%08X\n", addr * 4, v);
	return v;
}

static void xrc_write_full(struct device *dev, u32 addr, u32 data)
{
	writel(data, dev->base[0] + addr * 4);
#if 1
	if(_rf_dbg == 1)
		printf("[XRC_RF_Write] 0x%08X 0x%08X\n"
				, dev->base[0] + addr * 4
				, data);
#endif
}

struct xrc_bt_rf_lut xrc_bt_rf_lutt = {
    .tx_lut = {
        // TX will be dealt with later
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
		0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
		0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
		0x1e, 0x1f
    },
    .rx_lut = {
        // Rx will be dealt with later
		0x00, 0x01, 0x02, 0x03, 0x07
    }
};

#ifdef __fib_v1__
#define XRC_REG_INIT_MAXNUM 48
#else

#define XRC_REG_INIT_MAXNUM 70
#endif
static u32 xrc_reg_default[XRC_REG_INIT_MAXNUM][2] = {
#ifdef __fib_v1__
	{0x035, 0x89AB},	// LPF start , WIFI TX gain table update 2023.01.14 by tommy
	{0x036, 0xDE67},
	{0x037, 0x9ABC},
	{0x038, 0xBCDE},
	{0x039, 0xCDEA},
	{0x03A, 0x89AB},
	{0x03B, 0x4567},
	{0x03C, 0x0123},	// LPF stop
	{0x045, 0xF555},	// PA start
	{0x046, 0xFFFF},
	{0x047, 0xFFFF},
	{0x048, 0xFFFF},	// PA stop
	{0x04D, 0x0000},	// PAD start
	{0x04E, 0x1248},
	{0x04F, 0x36C9},
	{0x050, 0x7FDB},
	{0x051, 0x7FFF},
	{0x052, 0x7FFF},	// PAD stop, WIFI TX gain table update End
	{0x03D, 0xabcd},	// LPF start , BT TX gain table update 2023.02.24 by Han
	{0x03E, 0xcde9},
	{0x03F, 0xe9ab},
	{0x040, 0xabcd},
	{0x041, 0xcde9},
	{0x042, 0x89ab},
	{0x043, 0x4567},
	{0x044, 0x0123},	// LPF stop
	{0x049, 0xffff},	// PA
	{0x053, 0x003f},	// PAD start
	{0x054, 0x1240},
	{0x055, 0x3649},
	{0x056, 0x76db},	// PAD stop, BT TX gain table update End
	{0x12E, 0x9BC4},	// Improvement for TX psat Start
	{0x12F, 0x87C0},
	{0x05A, 0x0570},	// Improvement for TX psat end
	{0x122, 0x484D},	// Register 0x003
	{0x124, 0x2267},	// Register 0x01c
	{0x023, 0x00da},	//set WIFI LNA attenuation
#else
	{0x035, 0x5678},        // LPF start , A1 WIFI TX gain table update 2023.08.11 by Han
	{0x036, 0xCD34},
	{0x037, 0x89AB},
	{0x038, 0x4567},
	{0x039, 0x6783},
	{0x03A, 0x8945},
	{0x03B, 0x4567},
	{0x03C, 0x0123},        // LPF stop
	{0x045, 0xF555},        // PA start
	{0x046, 0xFFFF},
	{0x047, 0xFFFF},
	{0x048, 0xFFFF},        // PA stop
	{0x04D, 0x0000},        // PAD start
	{0x04E, 0x1248},
	{0x04F, 0x1249},
	{0x050, 0x36C9},
	{0x051, 0x7FDB},
	{0x052, 0x7FFF},        // PAD stop, A1 WIFI TX gain table update End
	{0x03D, 0xBCDE},        // LPF start , A1 BT TX gain table update 2023.08.11 by Han
	{0x03E, 0xDE9A},
	{0x03F, 0x9ABC},
	{0x040, 0xBCDE},
	{0x041, 0xCDEA},
	{0x042, 0x89AB},
	{0x043, 0x4567},
	{0x044, 0x0123},        // LPF stop
	{0x049, 0x0000},	// set PA gain to 0 in BLE mode, 20230828 update by Sami
	{0x04a, 0x0000},	// set PA gain to 0 in BLE mode, 20230828 update by Sami
	{0x04b, 0x0000},	// set PA gain to 0 in BLE mode, 20230828 update by Sami
	{0x04c, 0x0000},	// set PA gain to 0 in BLE mode, 20230828 update by Sami
	{0x02f, 0x0396},	// manually set BLE tx gain index 7
	{0x053, 0x003F},        // PAD start
	{0x054, 0x1200},
	{0x055, 0x3249},
	{0x056, 0x76DB},        // PAD stop, A1 BT TX gain table update End
	{0x12E, 0x9BC4},	// Improvement for TX psat Start
	{0x05A, 0x0508},	// Improvement for P1dB & PA settling time
	{0x11f, 0x2319},	// Improvement for PA settling time
	{0x080, 0x0017},	// Improvement 3dB for VCO-800MHz spur.
	{0x128, 0x03AF},	// Coarse CFO offset adjustment
	// BLE AGC setup start 20230829 update by Sami
	{0x069, 0x0410},
	{0x06A, 0xDF7D},
	{0x06B, 0x0827},
	{0x06C, 0x1042},
	{0x06D, 0x0004},
	{0x06E, 0x5050},
	{0x06F, 0x5050},
	{0x070, 0x7070},
	{0x071, 0x7070},
	{0x072, 0x0404},
	{0x073, 0x0404},
	{0x074, 0x0303},
	{0x075, 0x0303},
	{0x076, 0x4080},
	// BLE AGC setup stop
	// Improvement for 160M NF test 1 ADC, LPF LDO = 0.95V
	{0x122, 0x484D},	// Register 0x003
	{0x124, 0x2267},	// Register 0x01c
	// Improvement for 160M NF test 2 ADC, LPF LDO = 0.90V
	//{0x122, 0x4845},	// Register 0x003
	//{0x124, 0x2263},	// Register 0x01c
	// Improvement for 160M NF test 3 ADC, LPF LDO = 0.85V
	//{0x122, 0x4841},	// Register 0x003
	//{0x124, 0x2261},	// Register 0x01c
	{0x002, 0x0084},	// Improved receive for 11B ACK 20231017 by Tommy
	{0x025, 0x90e4},	//set WIFI LNA, TIA gain change
	{0x023, 0x00da},	//set WIFI LNA attenuation
#endif
	{0x01c, 0x2cf8},	//LPF csel=7, Improvement for RX NF and IQ swap Start
	{0x00c, 0x5018},	//turn off LNA EQ
	{0x093, 0x7070},	//BT CBPF rccal TCN
	{0x094, 0x7070},	//WIFI LPF rccal TCN
	{0x077, 0x1976},	//solve for glitches (+/-256) at ADC output
	{0x026, 0xaaab},	//set BT LNA attenuation
	{0x027, 0x000d},	//set BT LNA attenuation
	{0x02c, 0x5b6d},	//set WIFI LNA_BALCCAL=5
	{0x02d, 0x5b6d},	//set BT LNA_BALCCAL=5
	{0x135, 0x01e0},	//set RG_SX_LOGEN_SCA+15, lower the 1.6G and 3.2G spur, Improvement for RX NF and IQ swap End
	{0x12f, 0x86c0}		// TX TIA OP Mode Selection 0: ClassA ,1: ClassAB
};

static void xrc_opmode(struct device *dev, u8 mode)
/*
 * mode = 	0 : BT mode
 *		1 : WiFi mode
 *		2 : BB control mode
*/
{
	if(mode == 2)
	{
		xrc_write(dev, 0x01c, 0x0, 4, 4);	//  Default setting
	}
	else
	{
		xrc_write(dev, 0x01c, 0x1, 4, 4);	//  10’h01c[4:4] 1: WIFI/BT Mode selection by B_WIFI_BT_SEL_MAN, 0: WIFI/BT Mode selection by BaseBand
		xrc_write(dev, 0x01c, mode, 3, 3);	//  10’h01c[3:3] WIFI/BT Mode selection @ B_WIFI_BT_SEL_MAN_EN=1
	}
}

static void xrc_bandwidth_config(struct device *dev, u8 mode, u8 bw)
/*
 * mode = 	0 : BT mode
 *		1 : WiFi mode
 *		2 : BB control mode
*/
{
	if(mode == 2)
	{
		xrc_write(dev, 0x01b, 0x0, 14, 13);	//  Default setting
	}
	else if(mode == 1)
	{
		// bw : 0(20MHz), 1(40MHz)
		xrc_write(dev, 0x01b, 0x1, 13, 13);	//  10’h01b[13:13] WIFI data rate control by B_WIFI_DATA_RATE_MAN
		xrc_write(dev, 0x01b, bw, 12, 12);	//  10’h01b[12:12] WIFI data rate @manual mode, 0:20MHZ, 1:40MHz
	}
	else
	{
		// bw : 000：BLE1Mbps Mode, 001：BLE 2Mbps Mode, 010：BLE 125kbps Mode, 011：BLE 500kbps Mode, 100： BT BDR,  101： BT EDR 2Mbps,  11*：BT EDR 3Mbps
		xrc_write(dev, 0x01b, 0x1, 14, 14);	//  10’h01b[14:14] 1: BT data rate control by B_BT_DATA_RATE_MAN
		xrc_write(dev, 0x01c, bw, 2, 0);	//  10’h01c[2:0] BT data rate @manual mode
	}
}

static void xrc_channel_config_index(struct device *dev, int index)
{
	phy_write(dev, FW_RFI_XRC_CFG, (phy_read(dev, FW_RFI_XRC_CFG) & ~(0x7f)) | index);
}

static u8 xrc_afe_init(struct device *dev)
{
	xrc_write(dev, 0x05e, 0x2, 2, 0);	// 10’h05e[2:0]: 3’b010(3’b000) for 12MHz(24MHz) ADC clock @ BT Mode
	xrc_write(dev, 0x05e, 0x4, 8, 6);	// 10’h05e[8:6]: 3’b100(3’b111) for 80MHz(160MHz) ADC clock @ WIFI Mode
	xrc_write(dev, 0x07d, 0x1, 6, 6);	// TX dac iq swapped enable. Need check for BLE.
	phy_modem_write(dev, WIFI_BLE_CTRLMUX_CFG1, 0x00010001); // p0x_wifi_intf_ctl and p2x_ble_intf_ctrl ADC config iq swap enable in sys_reg
	return 0;
}

static void xrc_rf_reg_init(struct device *dev)
{
	int i;
	for (i=0; i<XRC_REG_INIT_MAXNUM; i++)
	{
		xrc_write_full(dev, xrc_reg_default[i][0], xrc_reg_default[i][1]);	// will be dealt with later
	}
}

static int xrc_rf_cfo_adj(struct device *dev, int ppm)
{
	int xtal_loadcap_old = xrc_read(dev, 0x128) & 0xff;
	int xtal_loadcap_new = xtal_loadcap_old - ppm;

	//printf("Xtal LoadCap [0x%2X -> ",xtal_loadcap_old);

	if(xtal_loadcap_new > 0xff)
		xtal_loadcap_new = 0xff;
	else if(xtal_loadcap_new < 0x40)
		xtal_loadcap_new = 0x40;

	if(xtal_loadcap_old == xtal_loadcap_new)
	{
		//printf("0x%2X] w/o update\n",xtal_loadcap_new);
		return 0;
	}
	else
	{
		//printf("0x%2X]\n",xtal_loadcap_new);
		xrc_write(dev, 0x128, xtal_loadcap_new, 7, 0);
		return 0;
	}
}

int _xiaohu_rx_iqcal_value[2];   //[0:DP(P0), 1:DG(P1)]
static void apply_tx_cal(struct device *dev, u8 print_en);

static int xrc_rf_set_channel(struct device *dev, u8 mode, u32 freq, u8 bw)
{
	u8 chan;

	chan = (freq == 2484 ? 13 : (freq - 2412) / 5);
	if(chan > 13) {
		printk("This is not the effective frequency range.\n");
		return -1;
	}

	/* Set channel and bandwidth */
	phy_write(dev, FW_RFI_XRC_CFG, 0x05000100 | (bw << 12) | chan);

	/* TX_LDO_PREP force "1" */
	phy_write(dev, FW_RFI_XRC_CFG, phy_read(dev, FW_RFI_XRC_CFG) | (1 << 31));
	udelay(32);

    /* XXX: Must ignore LDO_PREP during transition from normal to standby */
	xrc_write(dev, 0x001, 0x26a0, 15, 0);	// RF manual mode = standby
	udelay(10);

#ifndef __fib_v1__
	if(freq >= 2472)
	{
		xrc_write(dev, 0x05d, 1, 14, 14);	// ADC clock change
		xrc_write(dev, 0x127, 0x4, 15, 13); // ref spur reduction
		xrc_write(dev, 0x128, 0x1, 10, 10); // ref spur reduction
		phy_modem_write(dev, WIFI_BLE_CTRLMUX_CFG1, 0x000100c1); // p0x_wifi_intf_ctl and p2x_ble_intf_ctrl ADC config iq swap enable in sys_reg
		phy_modem_write(dev, DFE_CLK_CFG, (phy_modem_read(dev, DFE_CLK_CFG) & ~(5 << 5)) | (1 << 7));
		udelay(10);
		phy_modem_write(dev, DFE_CLK_CFG, phy_modem_read(dev, DFE_CLK_CFG) | (1 << 5));
	}
	else
	{
		xrc_write(dev, 0x05d, bw, 14, 14);		// ADC clock change
		xrc_write(dev, 0x127, 0x6, 15, 13); // ref spur reduction
		xrc_write(dev, 0x128, 0x0, 10, 10); // ref spur reduction
		phy_modem_write(dev, WIFI_BLE_CTRLMUX_CFG1, 0x00010081); // p0x_wifi_intf_ctl and p2x_ble_intf_ctrl ADC config iq swap enable in sys_reg
		phy_modem_write(dev, DFE_CLK_CFG, (phy_modem_read(dev, DFE_CLK_CFG) & ~(5 << 5)) | (bw << 7));
		udelay(10);
		phy_modem_write(dev, DFE_CLK_CFG, phy_modem_read(dev, DFE_CLK_CFG) | (1 << 5));
	}
#endif

	xrc_write(dev, 0x001, 0x22a0, 15, 0);		// RF manual mode = Modem control
	udelay(70);

	// Apply TX, RX cal value
	apply_tx_cal(dev, 0);														// TX cal LUT write
	phy_write(dev, FW_RFI_RX_IQ_MIS_COMP, (((_xiaohu_rx_iqcal_value[1] & 0xffff) << 16) | (_xiaohu_rx_iqcal_value[0] & 0xffff)));	// RX cal value write

	/* TX_LDO_PREP control by BB signal */
	phy_write(dev, FW_RFI_XRC_CFG, phy_read(dev, FW_RFI_XRC_CFG) & ~(1 << 31));

	return 0;
}

static int xrc_rf_config(struct device *dev,
		reg_write wrfn, reg_read rdfn)
{
	struct xrc_driver_data *drv = dev->driver_data;

	drv->writefn = wrfn;
	drv->readfn = rdfn;

	return 0;
}
u8 _def_digi_gain = 0;
static void xrc_phy_multi_tone(struct device *dev, u8 digi_gain, u8 tone_num, u8 tone1, u8 tone2, u8 tone3, u8 tone4, u8 tone5, u8 tone6, u8 tone7, u8 tone8)
{
#ifdef __fib_v1__
	phy_modem_write(dev, FW_PWR_EN, phy_modem_read(dev, FW_PWR_EN) | (0x3 << 28));
#else
	phy_modem_write(dev, WF_PHY_CFG, (phy_modem_read(dev, WF_PHY_CFG) & ~(1 << 24)) | (1 << 24));
#endif
	phy_write(dev, PHY_RFI_TX_SIGGEN_CFG, 0x01030000);
	phy_write(dev, PHY_RFI_TX_TONEGEN_0, tone1);
	phy_write(dev, PHY_RFI_TX_TONEGEN_1, tone2);
	phy_write(dev, PHY_RFI_TX_TONEGEN_2, tone3);
	phy_write(dev, PHY_RFI_TX_TONEGEN_3, tone4);
	phy_write(dev, PHY_RFI_TX_TONEGEN_4, tone5);
	phy_write(dev, PHY_RFI_TX_TONEGEN_5, tone6);
	phy_write(dev, PHY_RFI_TX_TONEGEN_6, tone7);
	phy_write(dev, PHY_RFI_TX_TONEGEN_7, tone8);
	phy_write(dev, PHY_RFI_TX_SIGGEN_CFG, 0x01130000 + ((1 << tone_num) - 1));
	if(_def_digi_gain == 0) _def_digi_gain = phy_read(dev, PHY_RFI_TX_DIG_GAIN_MAN) & 0xff;
	phy_write(dev, PHY_RFI_TX_DIG_GAIN_MAN, 0x80 | digi_gain);
}


static void xrc_phy_multi_tone_disable(struct device *dev)
{
	phy_write(dev, PHY_RFI_TX_SIGGEN_CFG, 0x00030000);
#ifdef __fib_v1__
	phy_modem_write(dev, FW_PWR_EN, phy_modem_read(dev, FW_PWR_EN) & ~(0x3 << 28));
#else
	phy_modem_write(dev, WF_PHY_CFG, (phy_modem_read(dev, WF_PHY_CFG) & ~(1 << 24)));
#endif
	if(_def_digi_gain)
	{
		phy_write(dev, PHY_RFI_TX_DIG_GAIN_MAN, _def_digi_gain);
		_def_digi_gain = 0;
	}
}

static void xrc_wifi_mode_manual_config(struct device *dev, u8 mode)
{
	phy_write(dev, FW_RFI_CTRLSIG_CFG_00, 0x00200000);
	phy_write(dev, FW_RFI_CTRLSIG_CFG_01, 0x00300000);
	if (mode == 0) // Auto
	{
		xrc_write(dev, 0x001, 0x26a0, 15, 0);		// RF manual mode = standby
		udelay(100);
		phy_write(dev, FW_RFI_EN, (phy_read(dev, FW_RFI_EN) & ~(0x77)));
		xrc_write(dev, 0x001, 0x22a0, 15, 0);		// RF manual mode = Modem control
		udelay(100);
	}
	else if(mode == 1) // TX
	{
		xrc_write(dev, 0x001, 0x26a0, 15, 0);		// RF manual mode = standby
		udelay(100);
		phy_write(dev, FW_RFI_EN, (phy_read(dev, FW_RFI_EN) & ~(0x77)) | 0x73);
		xrc_write(dev, 0x001, 0x46a0, 15, 0);		// RF manual mode = tx
		udelay(100);
	}
	else if(mode == 2) // RX
	{
		xrc_write(dev, 0x001, 0x26a0, 15, 0);		// RF manual mode = standby
		udelay(100);
		phy_write(dev, FW_RFI_EN, (phy_read(dev, FW_RFI_EN) & ~(0x77)) | 0x75);
		xrc_write(dev, 0x001, 0x86a0, 15, 0);		// RF manual mode = rx
		udelay(100);
	}
	else // Loopback
	{
		xrc_write(dev, 0x001, 0x26a0, 15, 0);		// RF manual mode = standby
		udelay(100);
		// RFI Clock Gate control bug fix for loob-back enable edit by Tommy 2023.11.23
		phy_write(dev, FW_RFI_EN, (phy_read(dev, FW_RFI_EN) & ~(0x10077)) | 0x77);
		phy_write(dev, FW_RFI_CTRLSIG_CFG_00, 0x002c0000);
		phy_write(dev, FW_RFI_CTRLSIG_CFG_01, 0x002c0000);
		xrc_write(dev, 0x001, 0x22a0, 15, 0);		// RF manual mode = Modem control
		udelay(100);
	}
}

#if 0
static u8 xrc_cal_init_setup(struct device *dev)
{
    //write_reg32(0xF1700378, 0x0000003f);
	//phy_write(dev, FW_RFI_XRC_CFG, 0x0500010d);
    //write_reg32(0xF1700380, 0x00000001);	// later
	phy_write(dev, FW_RFI_EN, 0x00000075);
	phy_write(dev, FW_RFI_XRC_CFG2, 0x01084210); // Enter Rx mode
	//xrc_write(dev, 0x001, 0x82a0, 15, 0); // Enter Rx mode
	//write_reg32(0xE0D0002C, 0x00); // later
	//write_reg32(0xE0D0002C, 0x01); // later

	//xrc_write(dev, 0x001, 0x2, 8, 6);	// 10’h001[8:6]: B_Xtal_40M_st=3’b010, 0.9ms after power up
	xrc_write(dev, 0x001, 0x22a0, 15, 0);	// 10’h001[8:6]: B_Xtal_40M_st=3’b010, 0.9ms after power up
	//xrc_write(dev, 0x002, 0x1, 9, 7);	// 10’h002[9:7]: B_Tsxsettle=3’b001, 60us after power up
	xrc_write(dev, 0x002, 0x0086, 15, 0);	// 10’h002[9:7]: B_Tsxsettle=3’b001, 60us after power up
	//xrc_write(dev, 0x01c, 0x3, 4, 3);  //Set B_WIFI_BT_SEL_MAN=1 and B_WIFI_BT_SEL_MAN_EN=1 (10’h01c[4:3]=2’b11)
	xrc_write(dev, 0x01c, 0x2d78, 15, 0);  //Set B_WIFI_BT_SEL_MAN=1 and B_WIFI_BT_SEL_MAN_EN=1 (10’h01c[4:3]=2’b11)
	//xrc_write(dev, 0x01b, 0x1, 13, 13);  //B_WIFI_DATA_RATE_MAN_EN, WIFI/BT Mode selection by B_WIFI_BT_SEL_MAN=1 (10’h01b[13]=2’b0)
	xrc_write(dev, 0x01b, 0x2fe1, 15, 0);  //B_WIFI_DATA_RATE_MAN=0(20M), WIFI/BT Mode selection by B_WIFI_BT_SEL_MAN=1 (10’h01b[13:12]=2’b10)
	xrc_channel_config_index(dev,14);
	phy_write(dev, FW_RXLUT_CFG0, 0x00000001);
	xrc_write(dev, 0x01f, 0x1, 10, 10); // B_WIFI_RXFE_GAIN_MAN_EN
	xrc_write(dev, 0x01f, 0x0, 9, 7); // B_WIFI_RXFE_GAIN_MAN_EN
	xrc_write(dev, 0x020, 0x1, 0, 0); // B_RX_LPF_GAIN_MAN_EN
	xrc_write(dev, 0x01f, 0x0, 4, 1); // B_WIFI_RXFE_GAIN_MAN_EN
	phy_write(dev, FW_RXLUT_MANSET_RXGAIN_OUT, 0x00000000);
	//xrc_write(dev, 0x05e, 0x2, 2, 0);	// 10’h05e[2:0]: 3’b010(3’b000) for 12MHz(24MHz) ADC clock @ BT Mode
	//xrc_write(dev, 0x05e, 0x4, 8, 6);	// 10’h05e[8:6]: 3’b100(3’b111) for 80MHz(160MHz) ADC clock @ WIFI Mode
	xrc_write(dev, 0x05e, 0x2f02, 15, 0);	// 10’h05e[2:0]: 3’b010(3’b000) for 12MHz(24MHz) ADC clock @ BT Mode
	xrc_write(dev, 0x006, 0x5202, 15, 0);	// 10’h006[9:9]: B_WIFI_TRX_SWITCH_SEL, 1: Enable the control of BB_WIFI_TX_LDO_PREP
	xrc_write(dev, 0x012, 0x0534, 15, 0);	// 10’h012[5:5]: B_PA_EN_CTRL, 1: PA control bt internal signal, 0: PA control by Baseband
	return 0;
}
#endif

// WiFi BW control to improve RX ACS performance edit by Tommy 2023.11.23
static int xrc_wifi_rx_lpf_ac_cal(struct device *dev, u8 printf_en)
{
	u8 wifi_lpf_csel, lpf_csel;
#if 0
	u8 b_wifi_40m_lpf_csel;
#endif
	xrc_write_full(dev, 0x01c, 0x2d78);	// WiFi Manual mode

#if 0
	for (int i = 0 ; i < 2 ; i++)
	{
		if ( i == 0 )	xrc_write_full(dev, 0x01b, (xrc_read(dev, 0x01b) & ~(0x3 << 12)) | (0x2 << 12));// 20M BW
		else	xrc_write_full(dev, 0x01b, (xrc_read(dev, 0x01b) & ~(0x3 << 12)) | (0x3 << 12));	// 40M BW, xrc_write_full(dev, 0x01b, 0x3fe1);

		xrc_write_full(dev, 0x001, 0x26a0);		// RF manual mode = standby
		udelay(50);
		xrc_write_full(dev, 0x001, 0x46a0);		// RF manual mode = Tx
		udelay(50);
		xrc_write_full(dev, 0x001, 0x26a0);		// RF manual mode = standby
		udelay(50);
		xrc_write_full(dev, 0x001, 0x86a0);		// RF manual mode = rx
		udelay(100);
		xrc_write_full(dev, 0x003, 0x2f94);		// LPF cal start
		udelay(100);
		if( i == 0)
		{
			b_wifi_20m_lpf_csel = (xrc_read(dev, 0x001)>>1) & 0x1f;  // b_wifi_20m_lpf_csel = RO_CAL_CBPF_CSEL (10’h001[5:1])
			xrc_write_full(dev, 0x01c, (0x2d60 & ~(0x1f << 5)) | (b_wifi_20m_lpf_csel << 5));
		}
		else
		{
			b_wifi_40m_lpf_csel = (xrc_read(dev, 0x001)>>1) & 0x1f;  // b_wifi_40m_lpf_csel = RO_CAL_CBPF_CSEL (10’h001[5:1])
			xrc_write_full(dev, 0x01d, (xrc_read(dev, 0x01d) & ~(0x1f << 0)) | (b_wifi_40m_lpf_csel << 0));
		}
		xrc_write_full(dev, 0x003, 0x2f84);		// LPF cal stop
	}

	xrc_write_full(dev, 0x01b, (xrc_read(dev, 0x01b) & ~(0x3 << 12)) | (0x0 << 12)); // Default setting
	xrc_write_full(dev, 0x001, 0x26a0);	// RF manual mode = standby
	udelay(50);
	xrc_write_full(dev, 0x001, 0x22a0);	// BB control mode

	if(printf_en)	printf("20M_LPF_CSEL = %d, 40M_LPF_CSEL = %d\n", b_wifi_20m_lpf_csel, b_wifi_40m_lpf_csel);
#else
		xrc_write_full(dev, 0x01b, (xrc_read(dev, 0x01b) & ~(0x3 << 12)) | (0x2 << 12));// 20M BW

		xrc_write_full(dev, 0x001, 0x26a0);		// RF manual mode = standby
		udelay(50);
		xrc_write_full(dev, 0x001, 0x86a0);		// RF manual mode = rx
		udelay(100);
		xrc_write_full(dev, 0x003, 0x2f94);		// LPF cal start
		udelay(100);
		lpf_csel = (xrc_read(dev, 0x001)>>1) & 0x1f;  // b_wifi_20m_lpf_csel = RO_CAL_CBPF_CSEL (10’h001[5:1])
		wifi_lpf_csel = lpf_csel + 8;
		if(wifi_lpf_csel > 0x1f)	wifi_lpf_csel = 0x1f;
		xrc_write_full(dev, 0x003, 0x2f84);		// LPF cal stop
								//
		xrc_write_full(dev, 0x01c, (wifi_lpf_csel << 5));
		xrc_write_full(dev, 0x01d, (lpf_csel << 11) | ((wifi_lpf_csel) << 0));
		xrc_write_full(dev, 0x01e, (lpf_csel << 6)); // bug fix

	xrc_write_full(dev, 0x01b, (xrc_read(dev, 0x01b) & ~(0x3 << 12)) | (0x0 << 12)); // Default setting
	xrc_write_full(dev, 0x001, 0x26a0);	// RF manual mode = standby
	udelay(50);
	xrc_write_full(dev, 0x001, 0x22a0);	// BB control mode

	if(printf_en)	printf("LPF_CSEL = %d\n", lpf_csel);
#endif
	return 0;
}

#if 0
static int xrc_bt_rx_lpf_ac_cal(struct device *dev)
{
	u8 b_bt_1m_cbpf_csel, b_bt_2m_cbpf_csel;

	xrc_write(dev, 0x01c, 0x2, 4, 3);  //Set B_WIFI_BT_SEL_MAN=0 and B_WIFI_BT_SEL_MAN_EN=1 (10’h01c[4:3]=2’b10)
	xrc_write(dev, 0x01c, 0x0, 2, 0);  //B_BT_DATA_RATE_MAN[2:0]=0 (10’h01c[2:0]=3’b000)
	xrc_write(dev, 0x01b, 0x1, 14, 14);  //B_BT_DATA_RATE_MAN_EN=1 (10’h01b[14]=1’b0)
	xrc_write(dev, 0x001, 0x10, 15, 11);  //5'b10000: Enter RX mode
	udelay(100);	// Waiting 100usec
	xrc_write(dev, 0x003, 0x1, 4, 4);  //10’h003[4]=1, B_CBPF_RC_CAL_EN=1, enable CBPF RC calibration
	for(int i = 0; i < 50; i++)
		if((xrc_read(dev, 0x000) & 0x400) == 0x400) break;	//Wait for rc_cal_done=1 (10’h000[10]=1)
	b_bt_1m_cbpf_csel = (xrc_read(dev, 0x001)>>1) & 0x1f;  // b_bt_1m_cbpf_csel = RO_CAL_CBPF_CSEL (10’h001[5:1])
	xrc_write(dev, 0x01d, b_bt_1m_cbpf_csel, 15, 11);  // B_BT_1M_CBPF_CSEL (10’h01d[15:11])=RO_CAL_CBPF_CSEL (10’h001[5:1])
	xrc_write(dev, 0x003, 0x0, 4, 4);  //B_CBPF_RC_CAL_EN=0, disable CBPF RC calibration

	xrc_write(dev, 0x01c, 0x1, 2, 0);  //B_BT_DATA_RATE_MAN[2:0]=1 (10’h01c[2:0]=3’b001)
	xrc_write(dev, 0x01b, 0x1, 14, 14);  //B_BT_DATA_RATE_MAN_EN=1 (10’h01b[14]=1’b0)
	xrc_write(dev, 0x003, 0x1, 4, 4);  //10’h003[4]=1, B_CBPF_RC_CAL_EN=1, enable CBPF RC calibration
	for(int i = 0; i < 50; i++)
		if((xrc_read(dev, 0x000) & 0x400) == 0x400) break;	//Wait for rc_cal_done=1 (10’h000[10]=1)
	b_bt_2m_cbpf_csel = (xrc_read(dev, 0x001)>>1) & 0x1f;  // b_bt_2m_cbpf_csel = RO_CAL_CBPF_CSEL (10’h001[5:1])
	xrc_write(dev, 0x01e, b_bt_2m_cbpf_csel, 10, 6);  // B_BT_2M_CBPF_CSEL (10’h01e[10:6]) =RO_CAL_CBPF_CSEL (10’h001[5:1])
	xrc_write(dev, 0x003, 0x0, 4, 4);  //B_CBPF_RC_CAL_EN=0, disable CBPF RC calibration

	return 0;
}

static int xrc_rx_lpf_ac_cal_to_normal_mode(struct device *dev)
{
	xrc_write(dev, 0x01c, 0x0, 4, 4);  //0: Set B_WIFI_BT_SEL_MAN (10’h01c[4]=2’b0)
	xrc_write(dev, 0x01b, 0x0, 13, 13);  //B_WIFI_DATA_RATE_MAN_EN, WIFI/BT Mode selection by B_WIFI_BT_SEL_MAN=1 (10’h01b[13]=2’b0)
	xrc_write(dev, 0x01b, 0x0, 14, 14);  //B_BT_DATA_RATE_MAN_EN=0 (10’h01b[14]=1’b0)
	return 0;
}

static int xrc_rx_lpf_ac_cal(struct device *dev)
{
	xrc_wifi_rx_lpf_ac_cal(dev);
	xrc_bt_rx_lpf_ac_cal(dev);
	xrc_rx_lpf_ac_cal_to_normal_mode(dev);

	return 0;
}
#endif

static int xrc_rx_dcoc_cal(struct device *dev, u8 lpf_gain, u8 digital_hpf_en, u8 printf_en)
{
	int i, j, k;

	// DCOC cal init
	xrc_channel_config_index(dev,6);
	xrc_write_full(dev, 0x00c, 0x7010);	// Set B_M4_RX_LNA_IN_SHORT=1 (10’h00c[13]=1), Set B_M4_RX_LNA_EN=0 (10’h00c[3]=0)
	xrc_write_full(dev, 0x00d, 0x0208);	// Set B_M4_RX_TIA_EN=0 (10’h00d[8]=0)
	xrc_write_full(dev, 0x008, 0x5394);	// Set TX_PAD_LDO from XRC 2023.01.17
	xrc_write_full(dev, 0x025, 0x9000);	// Set LNA gain = 0 from XRC 2023.01.18
	xrc_write_full(dev, 0x020, 0x00fe);	// BB control for LPF

	for (k = 1 ; k >= 0 ; k--)
	{
		xrc_write_full(dev, 0x01c, (xrc_read(dev, 0x01c) & ~(0x18)) | (1 << 4) | (k << 3));	// BT WiFi mode manual mode k=0(BT), k=1(WiFi)
		for (i = (k == 1 ? 0 : 1) ; i < 3 ; i++)
			// i = 	0 : ADC DCOC
			// 	1 : LPF DCOC
			// 	2 : TIA DCOC
		{
			if(k == 0)
			{
				xrc_channel_config_index(dev,19);		// ble ch:19
				///xrc_write_full(dev, 0x020, 0x00fe);	// BB control for LPF
				xrc_write_full(dev, 0x077, 0x1976);	//
				xrc_write_full(dev, 0x008, 0x5394);	// turn on PAD LDO
				xrc_write_full(dev, 0x00c, 0x7018);	// short LNA
				xrc_write_full(dev, 0x006, 0x7000);	// Set B_M4_RX_INPUT_CSHUNT=1 (10’h006[13]=1), channel control by manual
				xrc_write_full(dev, 0x028, 0x0000);	// LAN gain = 0
				xrc_write_full(dev, 0x029, 0xb900);	// LAN gain = 0
				xrc_write_full(dev, 0x02a, 0x31aa);	// CBPF gain config
				xrc_write_full(dev, 0x02b, 0x0ff7);	// CBPF gain config	,opt : 0x0ff7 or 0x0777
				xrc_write_full(dev, 0x00d, 0x0208);	// disable TIA in RX mode

				if(i == 2) // CBPF Gain change
				{
					//xrc_write_full(dev, 0x00c, 0x7018);	//
					xrc_write_full(dev, 0x00d, 0x0308);	// enable TIA in RX mode
					//xrc_write_full(dev, 0x02a, 0x77aa);
					//xrc_write_full(dev, 0x02b, 0x0773);
					xrc_write_full(dev, 0x02a, 0x31aa);
					xrc_write_full(dev, 0x02b, 0x0ff7);	//opt : 0x0ff7 or 0x0777
				}
			}
			else
			{
				xrc_write_full(dev, 0x006, 0x7002);	// Set B_M4_RX_INPUT_CSHUNT=1 (10’h006[13]=1), channel control by BB
				if(i == 2) // LNA, TIA turn on for TIA cal
				{
					if((lpf_gain < 16) && (k == 1))	xrc_write_full(dev, 0x020, 0x00e1 | ((lpf_gain & 0xF)<<1));	// Manual gain setting for LPF

					xrc_write_full(dev, 0x00c, 0x7118);	// Set B_M4_RX_LNA_EN=1 (10’h00c[3]=1)
					xrc_write_full(dev, 0x00d, 0x0308);	// Set B_M4_RX_TIA_EN=1 (10’h00d[8]=1)
				}
			}
			xrc_write_full(dev, 0x001, 0x26a0);		// RF manual mode = standby
			udelay(50);
			xrc_write_full(dev, 0x001, 0x86a0);		// RF manual mode = rx
			udelay(100);
			xrc_write_full(dev, 0x095, (1 << i));	// DCOC cal enable
			for(j = 0 ; j < 100 ; j++)
			{
				if(((xrc_read(dev, 0x11c) >> (7 + i)) & 0x1) == 1) break;
				udelay(50);
			}
			if(printf_en)
				printf("[%d] Cal done loop count = %d\n", i, j);
			xrc_write_full(dev, 0x095, 0x0000);	// DCOC cal disable
		}
	}
	// DCOC cal end
	xrc_write_full(dev, 0x008, 0x5294);	// unSet TX_PAD_LDO from XRC 2023.01.17
	xrc_write_full(dev, 0x006, 0x5202);	// Set B_M4_RX_INPUT_CSHUNT=1 (10’h006[13]=1)
	xrc_write_full(dev, 0x00c, 0x5118);	// Set B_M4_RX_LNA_IN_SHORT=1 (10’h00c[13]=1), Set B_M4_RX_LNA_EN=0 (10’h00c[3]=0)
	xrc_write_full(dev, 0x00d, 0x0308);	// Set B_M4_RX_TIA_EN=0 (10’h00d[8]=0)
	xrc_write_full(dev, 0x028, 0x4000);	// BT LAN gain default
	xrc_write_full(dev, 0x029, 0xb90e);	// BT LAN gain default
	xrc_write_full(dev, 0x02a, 0x31aa);	// BT CBPF gain default
	xrc_write_full(dev, 0x02b, 0x0ff7);	// BT CBPF gain default, opt : 0x0ff7 or 0x0777
	xrc_write_full(dev, 0x025, 0x90e4);	// Set LNA gain = default 2023.01.18
	xrc_write_full(dev, 0x01f, 0x03ac);	// BB control for TIA
	xrc_write_full(dev, 0x020, 0x00fe);	// BB control for LPF
	xrc_write_full(dev, 0x01c, xrc_read(dev, 0x01c) & ~(0x18));	// BT WiFi mode control by BB
	xrc_write_full(dev, 0x001, 0x26a0);	// RF manual mode = standby
	udelay(50);
	xrc_write_full(dev, 0x001, 0x22a0);	// BB control mode

	if(digital_hpf_en)	phy_write(dev, FW_RFI_RXDC_HPF_CFG, 0x18554);
	else			phy_write(dev, FW_RFI_RXDC_HPF_CFG, 0x00000);
#ifndef CONFIG_HOSTBOOT
	if(printf_en)
	{
		int adc_cal[2];
		u32 lpf_cal_value[16][2];
		u32 tia_cal_value[5][2];
		u32 cbpf_cal_value[5][2];
		u32 bttia_cal_value[5][2];
		u32 temp_reg;
		// ADC
		temp_reg = xrc_read(dev, 0x274 >> 2);
		adc_cal[0] = temp_reg & 0xff;
		if(adc_cal[0] >= 0x80) adc_cal[0] -= 0x100;
		adc_cal[1] = (temp_reg & 0xff00 >> 8);
		if(adc_cal[1] >= 0x80) adc_cal[1] -= 0x100;
		// WiFi LPF
		temp_reg = xrc_read(dev, 0x40C >> 2);
		lpf_cal_value[0][0] = temp_reg & 0x3f;
		lpf_cal_value[1][0] = (temp_reg >> 6) & 0x3f;
		lpf_cal_value[2][0] = (temp_reg >> 12) & 0xf;
		temp_reg = xrc_read(dev, 0x410 >> 2);
		lpf_cal_value[2][0] = lpf_cal_value[2][0] | ((temp_reg & 0x3) << 4);
		lpf_cal_value[3][0] = (temp_reg >> 2) & 0x3f;
		lpf_cal_value[4][0] = (temp_reg >> 8) & 0x3f;
		lpf_cal_value[5][0] = (temp_reg >> 14) & 0x3;
		temp_reg = xrc_read(dev, 0x414 >> 2);
		lpf_cal_value[5][0] = lpf_cal_value[5][0] | ((temp_reg & 0xf) << 2);
		lpf_cal_value[6][0] = (temp_reg >> 4) & 0x3f;
		lpf_cal_value[7][0] = (temp_reg >> 10) & 0x3f;
		temp_reg = xrc_read(dev, 0x418 >> 2);
		lpf_cal_value[8][0] = temp_reg & 0x3f;
		lpf_cal_value[9][0] = (temp_reg >> 6) & 0x3f;
		lpf_cal_value[10][0] = (temp_reg >> 12) & 0xf;
		temp_reg = xrc_read(dev, 0x41C >> 2);
		lpf_cal_value[10][0] = lpf_cal_value[10][0] | ((temp_reg & 0x3) << 4);
		lpf_cal_value[11][0] = (temp_reg >> 2) & 0x3f;
		lpf_cal_value[12][0] = (temp_reg >> 8) & 0x3f;
		lpf_cal_value[13][0] = (temp_reg >> 14) & 0x3;
		temp_reg = xrc_read(dev, 0x420 >> 2);
		lpf_cal_value[13][0] = lpf_cal_value[13][0] | ((temp_reg & 0xf) << 2);
		lpf_cal_value[14][0] = (temp_reg >> 4) & 0x3f;
		lpf_cal_value[15][0] = (temp_reg >> 10) & 0x3f;
		temp_reg = xrc_read(dev, 0x424 >> 2);
		lpf_cal_value[0][1] = temp_reg & 0x3f;
		lpf_cal_value[1][1] = (temp_reg >> 6) & 0x3f;
		lpf_cal_value[2][1] = (temp_reg >> 12) & 0xf;
		temp_reg = xrc_read(dev, 0x428 >> 2);
		lpf_cal_value[2][1] = lpf_cal_value[2][1] | ((temp_reg & 0x3) << 4);
		lpf_cal_value[3][1] = (temp_reg >> 2) & 0x3f;
		lpf_cal_value[4][1] = (temp_reg >> 8) & 0x3f;
		lpf_cal_value[5][1] = (temp_reg >> 14) & 0x3;
		temp_reg = xrc_read(dev, 0x42c >> 2);
		lpf_cal_value[5][1] = lpf_cal_value[5][1] | ((temp_reg & 0xf) << 2);
		lpf_cal_value[6][1] = (temp_reg >> 4) & 0x3f;
		lpf_cal_value[7][1] = (temp_reg >> 10) & 0x3f;
		temp_reg = xrc_read(dev, 0x430 >> 2);
		lpf_cal_value[8][1] = temp_reg & 0x3f;
		lpf_cal_value[9][1] = (temp_reg >> 6) & 0x3f;
		lpf_cal_value[10][1] = (temp_reg >> 12) & 0xf;
		temp_reg = xrc_read(dev, 0x434 >> 2);
		lpf_cal_value[10][1] = lpf_cal_value[10][1] | ((temp_reg & 0x3) << 4);
		lpf_cal_value[11][1] = (temp_reg >> 2) & 0x3f;
		lpf_cal_value[12][1] = (temp_reg >> 8) & 0x3f;
		lpf_cal_value[13][1] = (temp_reg >> 14) & 0x3;
		temp_reg = xrc_read(dev, 0x438 >> 2);
		lpf_cal_value[13][1] = lpf_cal_value[13][1] | ((temp_reg & 0xf) << 2);
		lpf_cal_value[14][1] = (temp_reg >> 4) & 0x3f;
		lpf_cal_value[15][1] = (temp_reg >> 10) & 0x3f;
		// WiFi TIA
		temp_reg = xrc_read(dev, 0x278 >> 2);
		tia_cal_value[0][0] = temp_reg & 0x3f;
		tia_cal_value[1][0] = (temp_reg >> 6) & 0x3f;
		tia_cal_value[2][0] = (temp_reg >> 12) & 0xf;
		temp_reg = xrc_read(dev, 0x27c >> 2);
		tia_cal_value[2][0] = tia_cal_value[2][0] | ((temp_reg & 0x3) << 4);
		tia_cal_value[3][0] = (temp_reg >> 2) & 0x3f;
		tia_cal_value[4][0] = (temp_reg >> 8) & 0x3f;
		temp_reg = xrc_read(dev, 0x400 >> 2);
		tia_cal_value[0][1] = (temp_reg >> 4) & 0x3f;
		tia_cal_value[1][1] = (temp_reg >> 10) & 0x3f;
		temp_reg = xrc_read(dev, 0x404 >> 2);
		tia_cal_value[2][1] = temp_reg & 0x3f;
		tia_cal_value[3][1] = (temp_reg >> 6) & 0x3f;
		tia_cal_value[4][1] = (temp_reg >> 12) & 0xf;
		temp_reg = xrc_read(dev, 0x408 >> 2);
		tia_cal_value[4][1] = tia_cal_value[4][1] | ((temp_reg & 0x3) << 4);
		// BT CBPF
		temp_reg = xrc_read(dev, 0x43C >> 2);
		cbpf_cal_value[0][0] = temp_reg & 0x3f;
		cbpf_cal_value[1][0] = (temp_reg >> 6) & 0x3f;
		cbpf_cal_value[2][0] = (temp_reg >> 12) & 0xf;
		temp_reg = xrc_read(dev, 0x440 >> 2);
		cbpf_cal_value[2][0] = cbpf_cal_value[2][0] | ((temp_reg & 0x3) << 4);
		cbpf_cal_value[3][0] = (temp_reg >> 2) & 0x3f;
		cbpf_cal_value[4][0] = (temp_reg >> 8) & 0x3f;
		temp_reg = xrc_read(dev, 0x444 >> 2);
		cbpf_cal_value[0][1] = temp_reg & 0x3f;
		cbpf_cal_value[1][1] = (temp_reg >> 6) & 0x3f;
		cbpf_cal_value[2][1] = (temp_reg >> 12) & 0xf;
		temp_reg = xrc_read(dev, 0x448 >> 2);
		cbpf_cal_value[2][1] = cbpf_cal_value[2][1] | ((temp_reg & 0x3) << 4);
		cbpf_cal_value[3][1] = (temp_reg >> 2) & 0x3f;
		cbpf_cal_value[4][1] = (temp_reg >> 8) & 0x3f;
		// BT TIA
		temp_reg = xrc_read(dev, 0x460 >> 2);
		bttia_cal_value[0][0] = temp_reg & 0x3f;
		bttia_cal_value[1][0] = (temp_reg >> 6) & 0x3f;
		bttia_cal_value[2][0] = (temp_reg >> 12) & 0xf;
		temp_reg = xrc_read(dev, 0x464 >> 2);
		bttia_cal_value[2][0] = bttia_cal_value[2][0] | ((temp_reg & 0x3) << 4);
		bttia_cal_value[3][0] = (temp_reg >> 2) & 0x3f;
		bttia_cal_value[4][0] = (temp_reg >> 8) & 0x3f;
		temp_reg = xrc_read(dev, 0x468 >> 2);
		bttia_cal_value[0][1] = temp_reg & 0x3f;
		bttia_cal_value[1][1] = (temp_reg >> 6) & 0x3f;
		bttia_cal_value[2][1] = (temp_reg >> 12) & 0xf;
		temp_reg = xrc_read(dev, 0x46c >> 2);
		bttia_cal_value[2][1] = bttia_cal_value[2][1] | ((temp_reg & 0x3) << 4);
		bttia_cal_value[3][1] = (temp_reg >> 2) & 0x3f;
		bttia_cal_value[4][1] = (temp_reg >> 8) & 0x3f;


		printf("DCOC cal value report\n");
		printf("[ADC] i_00 = %4d, q_00 = %4d\n", adc_cal[0], adc_cal[1]);
		for(i=0;i<16;i++)
			printf("[WiFi LPF] i_%2d = %4d, q_%2d = %4d\n", i, lpf_cal_value[i][0], i, lpf_cal_value[i][1]);
		for(i=0;i<5;i++)
			printf("[WiFi TIA] i_%2d = %4d, q_%2d = %4d\n", i, tia_cal_value[i][0], i, tia_cal_value[i][1]);
		for(i=0;i<5;i++)
			printf("[BT CBPF] i_%2d = %4d, q_%2d = %4d\n", i, cbpf_cal_value[i][0], i, cbpf_cal_value[i][1]);
		for(i=0;i<5;i++)
			printf("[BT TIA] i_%2d = %4d, q_%2d = %4d\n", i, bttia_cal_value[i][0], i, bttia_cal_value[i][1]);
	}
#endif
	return 0;
}


static int xrc_tx_lo_leakage_coarse_cal(struct device *dev, u8 printf_en)
{
	int i;

	xrc_write_full(dev, 0x001, 0x27a0);	// Standby mode, B_Xtal_40M_st:2.5ms
	udelay(50);
	xrc_write_full(dev, 0x01f, 0x05ac);	// B_WIFI_RXFE_GAIN_MAN_EN:1, B_WIFI_RXFE_GAIN_MAN:3
	// TXCL coarse cal performance improvement
	xrc_write_full(dev, 0x020, 0x01eb);	// B_RX_LPF_GAIN_MAN_EN:1, B_RX_LPF_GAIN_MAN:05, B_BT_RX_GAIN_MAN_EN:1, B_BT_RX_GAIN_MAN:7
	xrc_write_full(dev, 0x034, 0x03f1);	// B_TX_LPF_GAIN_MAN_EN:1, B_TX_LPF_GAIN_MAN:08, B_TX_PAD_GAIN_MAN_EN:1, B_TX_PAD_GAIN_MAN:7,B_TX_PA_GAIN_MAN_EN:1,B_TX_PA_GAIN_MAN:0
	xrc_write_full(dev, 0x123, 0x8364);	// RG_RX_LNALBK_EN:1
	xrc_write_full(dev, 0x00d, 0x0318);	// B_M5_RX_MIX_EN:1, B_M5_RX_TIA_EN:1
	xrc_write_full(dev, 0x012, 0x0534);	// B_M5_TX_PAD_EN:1, B_PA_EN_CTRL:1, B_M5_TX_PA_EN:1
	xrc_write_full(dev, 0x001, 0xffa0);	// Loopback mode
	udelay(100);
	xrc_write_full(dev, 0x09c, 0x0001);	// B_CAL_DAC_DOUT_MAN:1 (from TRX calibration value)
	xrc_write_full(dev, 0x095, 0x0010);	// RG_TXLO_LEAKAGE_EN:1
	for(i = 0; i < 50; i++)
	{
		if((xrc_read(dev, 0x11c) & 0x800) == 0x800) break;	//Wait for RO_TXLO_LEAKAGE_DONE=1 (10’h11c[11]=1)
		udelay(10);
	}
	xrc_write_full(dev, 0x001, 0x27a0);	// Standby mode, B_Xtal_40M_st:2.5ms
	xrc_write_full(dev, 0x123, 0x8360);	// RG_RX_LNALBK_EN:0
	xrc_write_full(dev, 0x00d, 0x0308);	// B_M5_RX_MIX_EN:0, B_M5_RX_TIA_EN:1
	xrc_write_full(dev, 0x012, 0x0514);	// B_M5_TX_PAD_EN:1, B_PA_EN_CTRL:0, B_M5_TX_PA_EN:1
	xrc_write_full(dev, 0x095, 0x0000);	// RG_TXLO_LEAKAGE_EN:0
	xrc_write_full(dev, 0x09c, 0x0000); // B_CAL_DAC_DOUT_MAN:0 (from BaseBand)
	xrc_write_full(dev, 0x01f, 0x01ac);	// B_WIFI_RXFE_GAIN_MAN_EN:0, B_WIFI_RXFE_GAIN_MAN:3
	xrc_write_full(dev, 0x020, 0x00fa);	// B_RX_LPF_GAIN_MAN_EN:0, B_RX_LPF_GAIN_MAN:13, B_BT_RX_GAIN_MAN_EN:0, B_BT_RX_GAIN_MAN:7
	xrc_write_full(dev, 0x034, 0x0ddc);	// B_TX_LPF_GAIN_MAN_EN:0, B_TX_LPF_GAIN_MAN:14, B_TX_PAD_GAIN_MAN_EN:0, B_TX_PAD_GAIN_MAN:7,B_TX_PA_GAIN_MAN_EN:0,B_TX_PA_GAIN_MAN:3
	xrc_write_full(dev, 0x001, 0x22a0);	// BB control mode, B_Xtal_40M_st:0.9ms

	if(printf_en)
	{
		u16 txlo_leakage_lut;
		txlo_leakage_lut = xrc_read(dev, 0x09c);
		printf("txlo_leakage_lut[Hex] = 0x%04x, txlo_leakage_lut_i[Dec] = %02d, txlo_leakage_lut_q[Dec] = %02d\n"
				, txlo_leakage_lut
				, (txlo_leakage_lut >> 1) & 0x3f
				, (txlo_leakage_lut >> 7) & 0x3f);
	}
	return 0;
}

u32 _xrc_temp_sens_en = 0;
// Added read temperature return edit by Tommy 2023.11.23
static int xrc_get_temp(struct device *dev, u8 print_en)
{
	struct device *dev_adc;
	int temp_deg, temp_deg_avg = 0, i;
	u16 temp_code;

	dev_adc = device_get_by_name("auxadc-xrc");

	if (!dev_adc) {
		phy_modem_write(dev, 0x7E0000 + AUXADC_CFG, 0x0b980028);	// Enable AUXADC
	}

	if(_xrc_temp_sens_en == 0)
	{
		xrc_write_full(dev, 0x12A, 0x7000);				// enable RF TSENS
		xrc_write_full(dev, 0x12B, 0x00e8);				// set to RG_TSENS_OP_IBIAS
		_xrc_temp_sens_en = 1;
		udelay(100);
	}

	if (!dev_adc) {
		phy_modem_write(dev, 0x7E0000 + AUXADC_CTRL, 0x00006400);
	}

	for(i=0 ; i<10; i++)
	{
		if (dev_adc) {
			adc_get_value_poll(dev_adc, ADC_SINGLE_CH_0, &temp_code, 1);
		} else {
			phy_modem_write(dev, 0x7E0000 + AUXADC_CTRL, 0x00006401);
			udelay(10);
			temp_code = (phy_modem_read(dev, AUXADC_DATA_0) & 0xffff);
		}
		temp_deg = ((temp_code * 3325 + 5120) / 10240) - 273;
		temp_deg_avg += temp_deg;
		if(print_en == 1)	printf("Current Temp = %4d, 0x%04x\n",temp_deg, temp_code);
	}
	if(print_en == 1)	printf("Average Temp = %4d\n",temp_deg_avg/i);

	temp_deg = (temp_deg_avg + i / 2) / i;
	if(temp_deg > 50)
	{
		xrc_write(dev, 0x120, 0x1, 1, 0);	// High temp PLL opt
		xrc_write(dev, 0x135, 0xf, 8, 5);	// Mid/Low temp PLL default
	}
	else
	{
		xrc_write(dev, 0x120, 0x2, 1, 0);	// High temp PLL default
		xrc_write(dev, 0x135, 0x3, 8, 5);	// Mid/Low temp PLL opt
	}

	if (!dev_adc) {
		phy_modem_write(dev, 0x7E0000 + AUXADC_CFG, 0x0b980000);	// Enable AUXADC
	}

	return temp_deg;
}

u32 delay = 50;
u8 meas = 5; // Increased average for added power-on cal stability edit by Tommy 2023.11.23
int _xiaohu_tx_cal_value[4][2];   //[LPF Gain Index][0:DCI, DCQ]/
int _xiaohu_tx_iq_cal_value[2];   //[0: DP(P0), DG(P1)]

// TX Gain table changed from 1dB to 0.5dB edit by Tommy 2023.11.23
static void apply_tx_cal(struct device *dev, u8 print_en)
{
	u8 lpf_gc_idx[32] =
	{
		8, 7, 6, 5, 4, 3, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 8, 7, 6, 5, 4, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
	};
	u8 i;

	phy_write(dev, FW_TXLUT_CFG0, 0x00010000); /* Tx LUT APB access en */
	for(i = 0; i < 32; i++) {
		int dc_i, dc_q;

		if(lpf_gc_idx[i] < 5)
		{
			dc_i = (_xiaohu_tx_cal_value[0][0] * (5 - lpf_gc_idx[i]) + _xiaohu_tx_cal_value[1][0] * lpf_gc_idx[i]) / 5;
			dc_q = (_xiaohu_tx_cal_value[0][1] * (5 - lpf_gc_idx[i]) + _xiaohu_tx_cal_value[1][1] * lpf_gc_idx[i]) / 5;
		}
		else if(lpf_gc_idx[i] < 10)
		{
			dc_i = (_xiaohu_tx_cal_value[1][0] * (10 - lpf_gc_idx[i]) + _xiaohu_tx_cal_value[2][0] * (lpf_gc_idx[i] - 5)) / 5;
			dc_q = (_xiaohu_tx_cal_value[1][1] * (10 - lpf_gc_idx[i]) + _xiaohu_tx_cal_value[2][1] * (lpf_gc_idx[i] - 5)) / 5;
		}
		else
		{
			dc_i = (_xiaohu_tx_cal_value[2][0] * (13 - lpf_gc_idx[i]) + _xiaohu_tx_cal_value[3][0] * (lpf_gc_idx[i] - 10)) / 3;
			dc_q = (_xiaohu_tx_cal_value[2][1] * (13 - lpf_gc_idx[i]) + _xiaohu_tx_cal_value[3][1] * (lpf_gc_idx[i] - 10)) / 3;
		}

		phy_write(dev, FW_RFI_TXLUTV2_ADDR, (1<<8) | (i * 2));
		phy_write(dev, FW_RFI_TXLUTV2_WRDATA, (dc_i & 0xffff) << 16 | (dc_q & 0xffff));
		phy_write(dev, FW_RFI_TXLUTV2_WRSTART, 0x1);
		phy_write(dev, FW_RFI_TXLUTV2_ADDR, (1<<8) | (i * 2 + 1));
		phy_write(dev, FW_RFI_TXLUTV2_WRDATA, (dc_i & 0xffff) << 16 | (dc_q & 0xffff));
		phy_write(dev, FW_RFI_TXLUTV2_WRSTART, 0x1);

		if(print_en)	printf("GT index = %2d, LPF_GC = %2d, DC_I = %4d, DC_Q = %4d\n",i, lpf_gc_idx[i], dc_i, dc_q);
	}
	phy_write(dev, FW_TXLUT_CFG0, 0x00000000); /* Tx LUT APB access dis */

	phy_write(dev, FW_RFI_TX_LO_LEAK_MAN, 0x00000000);
	phy_write(dev, FW_RFI_TX_IQ_MIS_COMP, (((_xiaohu_tx_iq_cal_value[1] & 0xffff) << 16) | (_xiaohu_tx_iq_cal_value[0] & 0xffff)));

	if(print_en)	printf("IQ cal (DP %04d, DG %04d)\n", _xiaohu_tx_iq_cal_value[0], _xiaohu_tx_iq_cal_value[1]);
}

static int xrc_tx_iq_lo_leakage_fine_cal(struct device *dev, u8 printf_en)
{
 	u8 i,k,m;
	u8 lpf_gc = 0;
	u8 lpf_stage = 0;
	u8 lpf_loop = 4;
	int cal_step = 1024;
	u8 find_loop = 11;
	int i_loop, q_loop;
	int opt_dc_i=0, opt_dc_q=0, opt_iq_p0=0, opt_iq_p1=0x1000;
	int opt_dc_i_tmp=0, opt_dc_q_tmp=0, opt_iq_p0_tmp=0, opt_iq_p1_tmp=0x1000;
	int opt_dc_i_tmp2=0, opt_dc_q_tmp2=0, opt_iq_p0_tmp2=0, opt_iq_p1_tmp2=0x1000;
	u32 opt_dc_val=0xffffffff, opt_iq_val=0xffffffff;
	u32 msr_val = 0xffffffff, msr_val2 = 0xffffffff;
	u32 msr_dc_pwr = 0, msr_iq_pwr = 0;
	int msr_dc_i = 0, msr_dc_q = 0, msr_iq_i = 0, msr_iq_q = 0;

	int step, start, stop;

	u32 meas_cnt = 0;

	phy_write(dev, FW_RFI_TX_LO_LEAK_MAN, 0x00000001);
	xrc_channel_config_index(dev, 6);	// 2442
	//xrc_phy_multi_tone(dev, 40, 1, 8, 0, 0, 0, 0, 0, 0, 0);	//xrc tone 40 1 8
	xrc_phy_multi_tone(dev, 40, 1, 8, 0, 0, 0, 0, 0, 0, 0);	//xrc tone 50 1 8
	xrc_write_full(dev, 0x034, 0x03F1);	//write 0xf0e000d0 0x3F1
	xrc_write_full(dev, 0x006, 0x5202);	//write 0xf0e00018 0x00005202
	xrc_write_full(dev, 0x00c, 0x5108);	//write 0xf0e00030 0x00005108
	xrc_write_full(dev, 0x012, 0x0534);	//PA force enable
	xrc_write_full(dev, 0x00d, 0x0108);	//write 0xf0e00034 0x00000108
	xrc_write_full(dev, 0x017, 0x0008);	//write 0xf0e0005c 0x00000008
//	xrc_write_full(dev, 0x020, 0x00EF);	//write 0xf0e00080 0x000000ef
	xrc_write_full(dev, 0x020, 0x00E1);	//write 0xf0e00080 0x000000ef // RX LPF gain control
	xrc_wifi_mode_manual_config(dev, 3);	// loopback mode

	xrc_write_full(dev, 0x12B, 0x04E9);		//write 0xf0e004ac 0x000004e9
	phy_write(dev, FW_RFI_TX_LO_LEAK_PARA, 0x0);	// write 0xf0f2821c 0x0
	phy_write(dev, FW_RFI_TX_IQ_MIS_COMP, 0x10000000);	// write 0xf0f28214 0x10000000
	phy_write(dev, PHY_RFI_TESTMSR_CFG, 0x00000100);		//write 0xf0f28600 0x00001100
	phy_write(dev, PHY_RFI_TESTMSR_TONECFG, 0x00200010);	//write 0xf0f28608 0x00200010
	phy_write(dev, PHY_RFI_TESTMSR_START, 0x1);		// write 0xf0f28604 0x00000001
	for(i=0;i<100;i++)
	{
		u32 temp_cnt = phy_read(dev, PHY_RFI_TESTMSR_IDX);
		if(meas_cnt == temp_cnt) break;
		meas_cnt = temp_cnt;
	}
	if(printf_en)	printf("i=%d, meas_cnt = %d\n",i,meas_cnt);
	phy_write(dev, PHY_RFI_TESTMSR_START, 0x0);		//write 0xf0f28604 0x00000000

	for (lpf_stage = 0 ; lpf_stage < (lpf_loop + 1); lpf_stage++)
	{
		if(lpf_stage == 3)
			lpf_gc = 13;
		else if(lpf_stage == 4)
			lpf_gc = 8;
		else
			lpf_gc = lpf_stage * 5;

		cal_step = 256;
		find_loop = 7;
		opt_dc_val = 0xffffffff;
		opt_iq_val = 0xffffffff;

		//xrc_phy_multi_tone(dev, 50 + (lpf_gc > 10 ? 20 : (lpf_gc * 2)) , 1, 8, 0, 0, 0, 0, 0, 0, 0);	//xrc tone 40+LPF_Atten 1 8
		//xrc_phy_multi_tone(dev, 46 + (lpf_gc * 2) , 1, 2, 0, 0, 0, 0, 0, 0, 0);	//xrc tone 40+LPF_Atten 1 8
		if(lpf_gc == 8)
			xrc_phy_multi_tone(dev, 60 , 1, 8, 0, 0, 0, 0, 0, 0, 0);	//xrc tone 60+LPF_Atten 1 8, update to power-on cal fail some chipsets.
		else
		{
			xrc_phy_multi_tone(dev, 40 , 1, 8, 0, 0, 0, 0, 0, 0, 0);	//xrc tone 40+LPF_Atten 1 8
			xrc_write_full(dev, 0x020, 0x00E1 + (lpf_gc << 1));	//write 0xf0e00080 0x000000ef // RX LPF gain control
		}
		xrc_write_full(dev, 0x034, 0x03e1 + (lpf_gc * 2));	// write 0xf0e000d0 0x221 + LPF gc[4:1]

		for (k = 0; k < find_loop; k++)
		{
			if ((k == 0) || (k == (find_loop - 1)))
				step = cal_step;
			else
				step = cal_step * 2;
			start = -cal_step;
			stop = cal_step;

			i_loop = start;
			q_loop = start;
			opt_dc_i_tmp = opt_dc_i + i_loop;
			opt_iq_p0_tmp = opt_iq_p0 + i_loop;
			opt_dc_q_tmp = opt_dc_q + q_loop;
			opt_iq_p1_tmp = opt_iq_p1 + q_loop;
			if(printf_en > 1)
			{
				printf("[Initial(loop(%d)]start = %d, stop = %d, step = %d, opt_dc_i_tmp = %d, opt_iq_p0_tmp = %d, opt_dc_q_tmp = %d, opt_iq_p1_tmp = %d\n",
					k, start, stop, step, opt_dc_i_tmp, opt_iq_p0_tmp, opt_dc_q_tmp, opt_iq_p1_tmp);
				udelay(1000);
			}
			for (i_loop = start; i_loop <= stop; i_loop += step)
			{
				opt_dc_i_tmp = opt_dc_i + i_loop;
				opt_iq_p0_tmp = opt_iq_p0 + i_loop;

				for (q_loop = start; q_loop <= stop; q_loop += step)
				{
					msr_dc_pwr = 0;
					msr_iq_pwr = 0;
					opt_dc_q_tmp = opt_dc_q + q_loop;
					opt_iq_p1_tmp = opt_iq_p1 + q_loop;

					if(lpf_gc != 8) phy_write(dev, FW_RFI_TX_LO_LEAK_PARA, (((opt_dc_i_tmp & 0xffff) << 16) | (opt_dc_q_tmp & 0xffff)));
					else		phy_write(dev, FW_RFI_TX_IQ_MIS_COMP, (((opt_iq_p1_tmp & 0xffff) << 16) | (opt_iq_p0_tmp & 0xffff)));
					if(printf_en > 1)
					{
						printf("[Calibration]i_loop = %d, q_loop = %d, opt_dc_i_tmp = %d, opt_iq_p0_tmp = %d, opt_dc_q_tmp = %d, opt_iq_p1_tmp = %d\n",
							i_loop, q_loop, opt_dc_i_tmp, opt_iq_p0_tmp, opt_dc_q_tmp, opt_iq_p1_tmp);
						udelay(1000);
					}

					for(m = 0; m < meas; m++)
					{
						// meas start
						meas_cnt = phy_read(dev, PHY_RFI_TESTMSR_IDX);
						phy_write(dev, PHY_RFI_TESTMSR_START, 0x1);	// write 0xf0f28604 0x00000001
						for(i=0;i<100;i++)
						{
							if(phy_read(dev, PHY_RFI_TESTMSR_IDX) == (meas_cnt + 3))	break;
						}
						phy_write(dev, PHY_RFI_TESTMSR_START, 0x0);	// write 0xf0f28604 0x00000000
						if(lpf_gc != 8)
						{
							msr_val = phy_read(dev, PHY_RFI_TONEMSR0_QCH);	// read 0xf0f2862c
							msr_dc_i = ((msr_val >> 16) & 0xFFFF);
							msr_dc_q = (msr_val & 0xFFFF);
							if (msr_dc_i >= 0x8000)
								msr_dc_i = (~(msr_dc_i) & 0x7fff) + 1;
							if (msr_dc_q >= 0x8000)
								msr_dc_q = (~(msr_dc_q) & 0x7fff) + 1;

							msr_dc_pwr += msr_dc_i * msr_dc_i + msr_dc_q * msr_dc_q;
						}
						else
						{
							msr_val2 = phy_read(dev, PHY_RFI_TONEMSR1_QCH);	// read 0xf0f28638
							msr_iq_i = ((msr_val2 >> 16) & 0xFFFF);
							msr_iq_q = ((msr_val2) & 0xFFFF);
							if (msr_iq_i >= 0x8000)
								msr_iq_i = (~(msr_iq_i) & 0x7fff) + 1;
							if (msr_iq_q >= 0x8000)
								msr_iq_q = (~(msr_iq_q) & 0x7fff) + 1;
							msr_iq_pwr += msr_iq_i * msr_iq_i + msr_iq_q * msr_iq_q;
						}

						if((printf_en == 1) && (m == (meas - 1)))
						{
							printf("msr_dc_pwr(%6d,%6d) = 0x%08x RAW = 0x%08x, msr_iq_pwr(%6d,%6d) = 0x%08x RAW = 0x%08x\n",
									opt_dc_i_tmp, opt_dc_q_tmp, msr_dc_pwr, msr_val, opt_iq_p0_tmp, opt_iq_p1_tmp, msr_iq_pwr, msr_val2);
							udelay(1000);
						}
					}

					if((msr_dc_pwr <= opt_dc_val) && (lpf_gc != 8))
					{
						opt_dc_i_tmp2 = opt_dc_i_tmp;
						opt_dc_q_tmp2 = opt_dc_q_tmp;
						opt_dc_val = msr_dc_pwr;
					}
					if((msr_iq_pwr <= opt_iq_val) && (lpf_gc == 8))
					{
						opt_iq_p0_tmp2 = opt_iq_p0_tmp;
						opt_iq_p1_tmp2 = opt_iq_p1_tmp;
						opt_iq_val = msr_iq_pwr;
					}
				}
			}
			cal_step = cal_step / 2;
			opt_dc_i = opt_dc_i_tmp2;
			opt_dc_q = opt_dc_q_tmp2;
			opt_iq_p0 = opt_iq_p0_tmp2;
			opt_iq_p1 = opt_iq_p1_tmp2;
		}
		// apply opt value
		phy_write(dev, FW_RFI_TX_LO_LEAK_PARA, (((opt_dc_i & 0xffff) << 16) | (opt_dc_q & 0xffff)));
		//phy_write(dev, FW_RFI_TX_IQ_MIS_COMP, (((opt_iq_p1 & 0xffff) << 16) | (opt_iq_p0 & 0xffff)));

		if(lpf_gc == 8)
		{
			_xiaohu_tx_iq_cal_value[0] = opt_iq_p0;
			_xiaohu_tx_iq_cal_value[1] = opt_iq_p1;
		}
		else
		{
			_xiaohu_tx_cal_value[lpf_stage][0] = opt_dc_i;
			_xiaohu_tx_cal_value[lpf_stage][1] = opt_dc_q;
		}

		if(printf_en > 1)
		{
			udelay (500000);
			printf("LPF(%02d) DC cal (DCI %04d, DCQ %04d, DC_pwr %d(value)), IQ cal (DP %04d, DG %04d, IQ_pwr %d(value))\n",
				lpf_gc, opt_dc_i, opt_dc_q, opt_dc_val, opt_iq_p0, opt_iq_p1, opt_iq_val);
			printf("_xiaohu_tx_cal_value[%02d][0] = %d\n", lpf_gc, opt_dc_i);
			printf("_xiaohu_tx_cal_value[%02d][1] = %d\n", lpf_gc, opt_dc_q);
			printf("tx_iq_cal_value[%02d][0] = %d\n", lpf_gc, opt_iq_p0);
			printf("tx_iq_cal_value[%02d][1] = %d\n", lpf_gc, opt_iq_p1);
			udelay(1000);
		}
	}

	if(printf_en == 1)
	{
		printf("Tx IQ cal done !!\n");
		for (u8 i= 0; i < 4; i++)
		{
			lpf_gc = (i==3?13:i*5);
			printf("_xiaohu_tx_cal_value(LPF_%02d):[DC_cal] DCI %5d DCQ %5d [IQ_cal] DP %5d DG %5d\n",
				lpf_gc, _xiaohu_tx_cal_value [i][0], _xiaohu_tx_cal_value [i][1], _xiaohu_tx_iq_cal_value[0], _xiaohu_tx_iq_cal_value[1]);
		}
	}
	// restore default value
	xrc_channel_config_index(dev, 6);			// 2442
	xrc_phy_multi_tone_disable(dev);
	xrc_write_full(dev, 0x034, 0x0c5a);			//0xf0e000d0
	xrc_write_full(dev, 0x006, 0x5202);			//0xf0e00018
	xrc_write_full(dev, 0x012, 0x0514);			//PA control by BB
	xrc_write_full(dev, 0x00c, 0x5118);			//0xf0e00030
	xrc_write_full(dev, 0x00d, 0x0308);			//0xf0e00034
	xrc_write_full(dev, 0x017, 0x0018);			//0xf0e0005c
	xrc_write_full(dev, 0x020, 0x00FE);			//0xf0e00080
	xrc_write_full(dev, 0x12B, 0x00E9);			//0xf0e004ac
	xrc_wifi_mode_manual_config(dev, 0);			// Auto mode

	apply_tx_cal(dev, 0);

	return 0;
}

static void apply_txcl_cal(struct device *dev, int DCI, int DCQ) __maybe_unused;
static void apply_txcl_cal(struct device *dev, int DCI, int DCQ)
{
	phy_write(dev, FW_RFI_TX_LO_LEAK_PARA, (((DCI & 0xffff) << 16) | (DCQ & 0xffff)));
	printf("Manual DC cal value (DCI %04d, DCQ %04d) applied\n",  DCI, DCQ);
}

static void apply_txssb_cal(struct device *dev, int DP, int DG) __maybe_unused;
static void apply_txssb_cal(struct device *dev, int DP, int DG)
{
	phy_write(dev, FW_RFI_TX_IQ_MIS_COMP, (((DG & 0xffff) << 16) | (DP & 0xffff)));
	printf("Manual IQ cal value (DP %04d, DG %04d) applied\n",  DP, DG);
}

static void xrc_wifi_mode_manual_config(struct device *dev, u8 mode);

static void xrc_wifi_tx_gain_manual_config(struct device *dev, int gain_index)
{
	xrc_write(dev, 0x02f, 0x1, 0, 0);	// 10’h02f[0:0]: TX Gain Control method in WIFI Mode, 1:B_WIFI_TX_GAIN_MAN, 0:Automatically controlled by internal signal
	xrc_write(dev, 0x02f, gain_index, 6, 2); // 10'h02f[6:2], TX Gain in WIFI Mode @ B_WIFI_TX_GAIN_MAN_EN=1,gain_index 0(min) ~ 31(max)
}


u8 rxg = 45;
static int xrc_rx_iq_cal(struct device *dev, u8 printf_en, u8 digi_gain)
{
	u8 i, k,m;
	int cal_step = 256;
	//u8 find_loop = 9;
	u8 find_loop = 7;
	int i_loop, q_loop;
	int opt_iq_p0=0, opt_iq_p1=0x1000;
	int opt_iq_p0_tmp=0, opt_iq_p1_tmp=0x1000;
	int opt_iq_p0_tmp2=0, opt_iq_p1_tmp2=0x1000;
	u32 opt_iq_val=0xffffffff;
	int msr_val;

	u32 msr_iq_pwr = 0;
	int msr_iq_i = 0, msr_iq_q = 0;

	int step, start, stop;
	u32 meas_cnt = 0;
///	u8 retry = 0, retry_loop = 3;

///	for (retry=0;retry<retry_loop;retry++)
///	{
	if(digi_gain < 0x80)
	{
		xrc_channel_config_index(dev, 6);	// 2442
//		xrc_phy_multi_tone(dev, 40, 1, 8, 0, 0, 0, 0, 0, 0, 0);	//xrc tone 40 1 8 (+5M tone)
//		xrc_phy_multi_tone(dev, 32, 1, 8, 0, 0, 0, 0, 0, 0, 0);	//xrc tone dig_gain 1 8 (+5M tone)
		xrc_phy_multi_tone(dev, digi_gain & 0x7f, 1, 8, 0, 0, 0, 0, 0, 0, 0);	//xrc tone dig_gain 1 8 (+5M tone)

		xrc_write_full(dev, 0x006, 0x5202);	//write 0xf0e00018 0x5202	([14] B_M5_RX_INPUT_CSHUNT=1)
		xrc_write_full(dev, 0x00c, 0x5118);  	//write 0xf0e00030 0x5118 ([14] B_M5_RX_LNA_IN_SHORT=1)

		//M5 enable block (set "1")
		//xrc_write_full(dev, 0x00c, 0x5118);	//write 0xf0e00030 0x5118 ([4] Rx LNA enable @Loop back mode)
		xrc_write_full(dev, 0x123, 0x8364);	//write 0xf0e0048c 0x8364 ([2] RX LNA BALUN LoopBack Mode Enable Control)
		xrc_write_full(dev, 0x00d, 0x0318);	//write 0xf0e00034 0x0318 ([9] RX TIA enable @Loop back mode, [4] RX MIX enable @Loop back mode)
		xrc_write_full(dev, 0x00e, 0x6300);	//write 0xf0e00038 0x6300 ([14] RX LPF enable @Loop back mode, [9] RX TIADCOC DAC enable @Loop back mode)
		xrc_write_full(dev, 0x010, 0x0318);	//write 0xf0e00040 0x0318 ([9] RX TIALPF BIAS enable @Loop back mode)
		xrc_write_full(dev, 0x00f, 0x6318);	//write 0xf0e0003c 0x6318 ([9] RX ADC enable @Loop back mode, [4] RX LPFDCOC DAC enable @Loop back mode)

		xrc_write_full(dev, 0x012, 0x0114);	//write 0xf0e00048 0x0514 ([4] TX PAD enable @Loop back mode)
		xrc_write_full(dev, 0x013, 0x5280);	//write 0xf0e0004c 0x5280 ([14] TX LPF enable @Loop back mode, [9] TX MIX enable @Loop back mode)

		//M5 disable block (set "0")
		//xrc_write_full(dev, 0x00c, 0x5118);	//write 0xf0e00030 0x5118 ([9] RX LNAEQ disable @Loop back mode)
		xrc_write_full(dev, 0x124, 0x2267);	//write 0xf0e00490 0x2267 ([10] RG_RX MIX_SEL disable)
		//xrc_write_full(dev, 0x00d, 0x0318);	//write 0xf0e00034 0x0318 ([14] RX MIXIP2K disable @Loop back mode)
		//xrc_write_full(dev, 0x00e, 0x6300);	//write 0xf0e00038 0x6300 ([4] RX MIXIP2K DAC disable @Loop back mode)
		//xrc_write_full(dev, 0x012, 0x0114);	//write 0xf0e00048 0x0514 ([15] TX TSSI disable @Loop back mode, [10] TX PA disable @Loop back mode)

//	xrc_write_full(dev, 0x017, 0x0018);	//write 0xf0e0005c 0x0018 (RXLODIV2 enable @Loop back mode = 1)

		// TX attenuation
		xrc_write_full(dev, 0x122, 0x484d);	//write 0xf0e00488 0x484d ([11:8] RG_RX_IQCAL_ATT = 8)

		xrc_wifi_mode_manual_config(dev, 3);	// loopback mode
		xrc_bandwidth_config(dev, 1, 0);	// WiFi BW : 20MHz

		// loopback gain control
		xrc_write_full(dev, 0x034, 0x0231);	//write 0xf0e000d0 0x0231 (PA/PAD/LPF=0/0/8)
		phy_write(dev, FW_RFI_TX_IQ_MIS_COMP, (((_xiaohu_tx_iq_cal_value[1] & 0xffff) << 16) | (_xiaohu_tx_iq_cal_value[0] & 0xffff)));
	}
	// default value setting
	opt_iq_p0 = 0;
	opt_iq_p1 = 0x1000;
	opt_iq_p0_tmp2 = 0;
	opt_iq_p1_tmp2 = 0x1000;

	if(digi_gain < 0x80)
	{
		phy_write(dev, FW_RXLUT_CFG0, ( rxg << 8) | (0x1 << 4));	//write 0xf0f28800 0x00003110 ([14:8] Rx LUT idx, [4] manual mode)

		xrc_write_full(dev, 0x123, 0x8365);	//write 0xf0e0048c 0x8365 ([0] RX IQCAL Enable = 1)
	}

	phy_write(dev, PHY_RFI_TESTMSR_CFG, 0x00001100);		//write 0xf0f28600 0x00001100
	phy_write(dev, PHY_RFI_TESTMSR_TONECFG, 0x00000070);	//write 0xf0f28608 0x00000010 ([6:0] tonemsr0_phstep = -16)
	phy_write(dev, FW_RFI_RX_IQ_MIS_COMP, 0x10000000);	//write 0xf0f28514 0x10000000
	udelay(10);

	phy_write(dev, PHY_RFI_TESTMSR_START, 0x1);		// write 0xf0f28604 0x00000001
	for(i=0;i<100;i++)
	{
		u32 temp_cnt = phy_read(dev, PHY_RFI_TESTMSR_IDX);
		if(meas_cnt == temp_cnt) break;
		meas_cnt = temp_cnt;
	}
	if(printf_en)	printf("i=%d, meas_cnt = %d\n",i,meas_cnt);
	phy_write(dev, PHY_RFI_TESTMSR_START, 0x0);		//write 0xf0f28604 0x00000000

	for (k = 0; k < find_loop; k++)
	{
		step = cal_step;
		start = -cal_step;
		stop = cal_step;

		i_loop = start;
		q_loop = start;
		opt_iq_p0_tmp = opt_iq_p0 + i_loop;
		opt_iq_p1_tmp = opt_iq_p1 + q_loop;

		if(printf_en > 1)
		{
			printf("[Initial(loop(%d)]start = %d, stop = %d, step = %d, opt_iq_p0_tmp = %d, opt_iq_p1_tmp = %d\n",
				k, start, stop, step, opt_iq_p0_tmp, opt_iq_p1_tmp);
			udelay(1000);
		}
		for (i_loop = start; i_loop <= stop; i_loop += step)
		{
			opt_iq_p0_tmp = opt_iq_p0 + i_loop;
			for (q_loop = start; q_loop <= stop; q_loop += step)
			{
				msr_iq_pwr = 0;
				opt_iq_p1_tmp = opt_iq_p1 + q_loop;

				phy_write(dev, FW_RFI_RX_IQ_MIS_COMP, (((opt_iq_p1_tmp & 0xffff) << 16) | (opt_iq_p0_tmp & 0xffff)));

				if(printf_en > 1)
				{
					printf("[Calibration]i_loop = %d, q_loop = %d, opt_iq_p0_tmp = %d, opt_iq_p1_tmp = %d\n",
						i_loop, q_loop, opt_iq_p0_tmp, opt_iq_p1_tmp);
					udelay(1000);
				}

				for(m = 0; m < meas; m++)
				{
					// meas start
					meas_cnt = phy_read(dev, PHY_RFI_TESTMSR_IDX);
					phy_write(dev, PHY_RFI_TESTMSR_START, 0x1);	// write 0xf0f28604 0x00000001
					for(i=0;i<100;i++)
					{
						if(phy_read(dev, PHY_RFI_TESTMSR_IDX) >= (meas_cnt + 3))	break;
					}
					if(printf_en == 1)	printf("i=%d, IDX = %d, meas_cnt = %d\n",i, phy_read(dev, PHY_RFI_TESTMSR_IDX), meas_cnt);
					phy_write(dev, PHY_RFI_TESTMSR_START, 0x0);	//write 0xf0f28604 0x00000000

					msr_val = phy_read(dev, PHY_RFI_TONEMSR0_SUM);	// read 0xf0f2862c
					msr_iq_i = ((msr_val>>16) & 0xFFFF);
					msr_iq_q = ((msr_val>>0) & 0xFFFF);
					if (msr_iq_i >= 0x8000)
						msr_iq_i = (~(msr_iq_i) & 0x7fff) + 1;
					if (msr_iq_q >= 0x8000)
						msr_iq_q = (~(msr_iq_q) & 0x7fff) + 1;
								msr_iq_pwr += msr_iq_i * msr_iq_i + msr_iq_q * msr_iq_q;

					if((printf_en == 1) && (m == (meas - 1)))
					{
						printf("msr_iq_pwr(%6d,%6d) = 0x%08x RAW = 0x%08x\n",
								opt_iq_p0_tmp, opt_iq_p1_tmp, msr_iq_pwr, msr_val);
						udelay(1000);
					}
				}

				if(msr_iq_pwr <= opt_iq_val)
				{
					opt_iq_p0_tmp2 = opt_iq_p0_tmp;
					opt_iq_p1_tmp2 = opt_iq_p1_tmp;
					opt_iq_val = msr_iq_pwr;
				}
			}
		}
		cal_step = cal_step / 2;
		opt_iq_p0 = opt_iq_p0_tmp2;
		opt_iq_p1 = opt_iq_p1_tmp2;
	}
	// apply opt value
	phy_write(dev, FW_RFI_RX_IQ_MIS_COMP, (((opt_iq_p1 & 0xffff) << 16) | (opt_iq_p0 & 0xffff)));
	_xiaohu_rx_iqcal_value[0] = opt_iq_p0;
	_xiaohu_rx_iqcal_value[1] = opt_iq_p1;

/*
	if (msr_iq_pwr < 0x20)
	{
		printf("retry (%d) : msr_iq_pwr(%6d,%6d) = 0x%08x \n", retry, opt_iq_p0, opt_iq_p1, opt_iq_val);
		break;
	}
	else
		printf("retry (%d) : msr_iq_pwr(%6d,%6d) = 0x%08x \n", retry, opt_iq_p0, opt_iq_p1, opt_iq_val);

	}
*/


	if(printf_en == 1)
	{
		printf("Rx IQ cal done !!\n");
		printf("RXG_idx(%02d) IQ cal (DP %04d, DG %04d, IQ_pwr %d(value))\n",
			rxg, opt_iq_p0, opt_iq_p1, opt_iq_val);
	}
	else if(digi_gain < 0x80)
	{
		// restore default value
		xrc_channel_config_index(dev, 6);	// 2442
		xrc_phy_multi_tone_disable(dev);

		xrc_write_full(dev, 0x006, 0x5202);	//0xf0e00018
		xrc_write_full(dev, 0x00c, 0x5118);  	//0xf0e00030
		xrc_write_full(dev, 0x00d, 0x0308);	//0xf0e00034
		xrc_write_full(dev, 0x00e, 0x6300);	//0xf0e00038
		xrc_write_full(dev, 0x010, 0x0318);	//0xf0e00040
		xrc_write_full(dev, 0x00f, 0x6318);	//0xf0e0003c
		xrc_write_full(dev, 0x012, 0x0514);	//0xf0e00048
		xrc_write_full(dev, 0x013, 0x5280);	//0xf0e0004c
		xrc_write_full(dev, 0x124, 0x2267);	//0xf0e00490
		xrc_write_full(dev, 0x122, 0x484d);	//0xf0e00488

		xrc_wifi_mode_manual_config(dev, 0);	// Auto mode
		xrc_bandwidth_config(dev, 2, 0);	// WiFi BW : 20MHz

		xrc_write_full(dev, 0x034, 0x0c5a);	//0xf0e000d0
		phy_write(dev, FW_RXLUT_CFG0, 0x0);	//0xf0f28800
		xrc_write_full(dev, 0x123, 0x8360);	//0xf0e0048c
		phy_write(dev, PHY_RFI_TESTMSR_CFG, 0x00001000);		//0xf0f28600
		phy_write(dev, PHY_RFI_TESTMSR_TONECFG, 0x00000000);	//0xf0f28608
		}
	return 0;
}

static void apply_rxiq_cal(struct device *dev, int DP, int DG) __maybe_unused;
static void apply_rxiq_cal(struct device *dev, int DP, int DG)
{
	phy_write(dev, FW_RFI_RX_IQ_MIS_COMP, (((DG & 0xffff) << 16) | (DP & 0xffff)));
	printf("Manual RxIQ cal value (DP %04d, DG %04d) applied\n",  DP, DG);
}

static void get_txiq_cal(struct device *dev) __maybe_unused;
static void get_txiq_cal(struct device *dev)
{
	int DP = (phy_read(dev, FW_RFI_TX_IQ_MIS_COMP) >> 0) & 0xffff;
	int DG = (phy_read(dev, FW_RFI_TX_IQ_MIS_COMP) >> 16) & 0xffff;

	if (DP >= 0x8000)
		DP = -(~(DP) & 0x7fff) + 1;
	if (DG >= 0x8000)
		DG = -(~(DG) & 0x7fff) + 1;
	printf("TxIQ cal value (DP %04d, DG %04d) \n",  DP, DG);
}

static void get_rxiq_cal(struct device *dev) __maybe_unused;
static void get_rxiq_cal(struct device *dev)
{
	int DP = (phy_read(dev, FW_RFI_RX_IQ_MIS_COMP) >> 0) & 0xffff;
	int DG = (phy_read(dev, FW_RFI_RX_IQ_MIS_COMP) >> 16) & 0xffff;

	if (DP >= 0x8000)
		DP = -(~(DP) & 0x7fff) + 1;
	if (DG >= 0x8000)
		DG = -(~(DG) & 0x7fff) + 1;
	printf("RxIQ cal value (DP %04d, DG %04d) \n",  DP, DG);
}

static int xrc_tx_dpd_cal(struct device *dev) __maybe_unused;
static int xrc_tx_dpd_cal(struct device *dev)
{
	xrc_write(dev, 0x001, 0x8, 15, 11);  //5'b01000: Enter Tx mode
	xrc_write(dev, 0x124, 0x1, 10, 10);  //RG_RX_MIX_SEL=1 (10’h124[10]=1), Select BEACON Mixer
	xrc_write(dev, 0x017, 0x1, 7, 7);  //B_M3_SX_DPDLODIV2_EN=1 (10’h017[7]=1), Enable DPDLODIV2 in TX mode
	// Enable RX LPF & ADC in TX mode
	xrc_write(dev, 0x00e, 0x1, 12, 12);  //B_M3_RX_LPF_EN=1 (10’h00e[12]=1)
	xrc_write(dev, 0x00f, 0x1, 7, 7);  //B_M3_RX_ADC_EN=1 (10’h00f[7]=1)
	xrc_write(dev, 0x007, 0x1, 7, 7);  //B_M3_RX_LPF_LDO_EN=1 (10’h007[7]=1)
	xrc_write(dev, 0x007, 0x1, 12, 12);  //B_M3_RX_ADC_LDO_EN=1 (10’h007[12]=1)

	// Setting WIFI TX Gain Mode and WIFI data rate
	xrc_opmode(dev, 1);	// WiFi mode
	xrc_bandwidth_config(dev, 1, 0);	// WiFi BW : 20MHz
	xrc_wifi_tx_gain_manual_config(dev, 16);		//will be dealt with later

	// do TX DPD in modem

	//Register Recover after TX LO Leakage Coarse Cal finished
	xrc_write(dev, 0x124, 0x0, 10, 10);  //RG_RX_MIX_SEL=0 (10’h0124[10]=0)
	xrc_write(dev, 0x017, 0x0, 7, 7);  //B_M3_SX_DPDLODIV2_EN=0 (10’h017[7]=0)
	xrc_write(dev, 0x00e, 0x0, 12, 12);  //B_M3_RX_LPF_EN=0 (10’h00e[12]=0)
	xrc_write(dev, 0x00f, 0x0, 7, 7);  //B_M3_RX_ADC_EN=0 (10’h00f[7]=0)
	xrc_write(dev, 0x007, 0x0, 7, 7);  //B_M3_RX_LPF_LDO_EN=0 (10’h007[7]=0)
	xrc_write(dev, 0x007, 0x0, 12, 12);  //B_M3_RX_ADC_LDO_EN=0 (10’h007[12]=0)

	return 0;
}

static int xrc_cfo_ofs(struct device *dev, int ofs)
{
	int current_ofs = 0xaf;
	int new_ofs;
	int new_ofs_temp;
	int ofs_temp;
	ofs = 0 - ofs;

	if (ofs <= 0)
	{
		if(ofs < -10)
		{
			current_ofs = 0x60;
			ofs = ofs + 10;
			new_ofs_temp = (current_ofs - 2) + ofs*3;
			if(new_ofs_temp < 0x40 )
			{
				ofs = (ofs - 2) + (0x60 - 0x40)*0.35;
				new_ofs_temp = 0x40 + ofs/0.5;
			}
			new_ofs = new_ofs_temp;
		}
		else
		{
			new_ofs = current_ofs + ofs*5;
			if(new_ofs < 0x80) new_ofs=0x80;
		}
	}
	else
	{
		new_ofs_temp = current_ofs + ofs*7;
		if (new_ofs_temp > 0xd0)
		{
			ofs_temp = (0xd0 - current_ofs)/7;
			if((0xd0 - current_ofs)%7 > 3) ofs_temp = ofs_temp + 1;
			new_ofs_temp = 0xd0 + ((ofs - ofs_temp)*9);
		}
		new_ofs = new_ofs_temp;
	}

	if(new_ofs > 0xfd) new_ofs=0xfd;
	if(new_ofs < 0x30) new_ofs=0x30;
	xrc_write(dev, 0x128, new_ofs, 7, 0);
	if(_rf_dbg == 1)	printf("current cfo setting value = 0x%02x\n",new_ofs);
	return 0;
}

#ifdef __fib_v1__
/* PICL OK interrupt mask for busy wait */
static void xrc_pilc_ok_irq_mask(struct device *dev)
{
	u32 v;

	v = phy_modem_read(dev, PMU_DLCPD_IMR);
	v |= (1 << 6);
	phy_modem_write(dev, PMU_DLCPD_IMR, v);
}

/* Need to check PICL OK after indirect write to PMU */
static void xrc_check_pmu_done(struct device *dev)
{
	u32 v;

	do {
		v = phy_modem_read(dev, PMU_DLCPD_IFR);
	} while (!(v & (1 << 6))); // wait PICL OK

	/* clear PICL OK */
	phy_modem_write(dev, PMU_DLCPD_IFR, v);
}
#endif
static int xrc_rf_init(struct device *dev, u8 mode)
{
	/*
	 * RF SW reset
	 * In case of DBG reset, will not initialize RF digital.
	 */
	xrc_write(dev, 0x000, 0x0, 1, 1);

#ifdef __fib_v1__
	xrc_pilc_ok_irq_mask(dev);
	// Set the Maximal allowed voltage index
	phy_modem_write(dev, PMU_DLCPD_MPACR_DATA, 0x00000011);	// 0x0d : 1.2V, 0x0e : 1.25V, 0x0f : 1.3V, 0x10 : 1.35V, 0x11 : 1.4V, 0x12 : 1.5V
	phy_modem_write(dev, PMU_DLCPD_MPACR_ADDR, 0x10003204);
    xrc_check_pmu_done(dev);

	// Set the Regulator Nominal voltage value
	phy_modem_write(dev, PMU_DLCPD_MPACR_DATA, 0x00000011); // 0x0d : 1.2V, 0x0e : 1.25V, 0x0f : 1.3V, 0x10 : 1.35V, 0x11 : 1.4V, 0x12 : 1.5V
	phy_modem_write(dev, PMU_DLCPD_MPACR_ADDR, 0x1000320e);
    xrc_check_pmu_done(dev);

	// Request the PMU to update voltage
	phy_modem_write(dev, PMU_DLCPD_MPACR_DATA, 0x00000001);
	phy_modem_write(dev, PMU_DLCPD_MPACR_ADDR, 0x10003206);
    xrc_check_pmu_done(dev);
#endif
	phy_write(dev, FW_RFI_XRC_CFG, 0x05000106);	// TX LDO control by BB signal

	xrc_rf_reg_init(dev);

	xrc_write(dev, 0x001, 0x2, 8, 6);	// 10’h001[8:6]: B_Xtal_40M_st=3’b010, 0.9ms after power up
	xrc_write(dev, 0x002, 0x1, 9, 7);	// 10’h002[9:7]: B_Tsxsettle=3’b001, 60us after power up

	xrc_afe_init(dev);

	xrc_write(dev, 0x006, 0x0, 9, 9);	// For power-on cal

	xrc_wifi_rx_lpf_ac_cal(dev, 0);
	xrc_rx_dcoc_cal(dev, 0x9, 1, 0); 	// [8]: Digital HPF enable, [7:0] TIA cal gain config
#ifdef __fib_v1__
	xrc_tx_lo_leakage_coarse_cal(dev, 0);
#else
	xrc_tx_lo_leakage_coarse_cal(dev, 0);
	xrc_tx_iq_lo_leakage_fine_cal(dev, 0);
	xrc_rx_iq_cal(dev, 0, 32); // Added RX IQ power-on-cal
#if 0
	// Work-round to reduce TX initial frequency offset edit by Tommy 2023.11.23
	// Apply pending due to performance deterioration of some chips.
	{
		xrc_write(dev, 0x007, 0x1, 12, 12); 	// ADC_LDO_EN for TX mode.
		xrc_write(dev, 0x00F, 0x1, 7, 7);		// ADC_EN for TX mode.
		xrc_write(dev, 0x010, 0x1, 2, 2);		// RSSIADC_EN for TX mode.
		xrc_write(dev, 0x017, 0x1, 2, 2);		// SX_RXLODIV2_EN for TX mode.
	}
#endif
#endif

	xrc_write(dev, 0x006, 0x1, 9, 9);	// 10’h006[9:9]: B_WIFI_TRX_SWITCH_SEL, 1: Enable the control of BB_WIFI_TX_LDO_PREP
	//xrc_write(dev, 0x006, 0x0, 9, 9);	// temp config

#ifdef __fib_v1__
	/*
	 * Setting for FIB_V1
	 * 10'h002[1:0]:B_LDO_st, 0, LDO settling time is 2usec
	 * 10'h002[3:2]:B_FC_T, 0, Fast charge time period is 2usec
	 * B_WIFI_TRX_SWTICH_SEL set to 0:Disable the control of BB_WIFI_TX_LDO_PREP.
	 */
	xrc_write(dev, 0x002, 0x0, 1, 0);
	xrc_write(dev, 0x002, 0x0, 3, 2);
	xrc_write(dev, 0x006, 0x0, 9, 9);
#endif

#ifndef __fib_v1__
	/* RF enter sleep mode */
	xrc_write(dev, 0x136, 0x58, 7, 0);
#endif

	return 0;
}

static int xrc_rf_power_cntrl(struct device *dev, u8 flag)
{
	/* In WLAN or BLE driver, RF mode can is controlled Fix me lator */

	/* RF Power down */
	if(flag == 0) {
		phy_write(dev, FW_RFI_XRC_CFG2, 0x00421084);
	}
	else {	/* RF Power up */
		phy_write(dev, FW_RFI_XRC_CFG2, 0x01f82082);
	}
	return 0;
}

struct rf_ops xrc_rf_ops = {
	.config = xrc_rf_config,
	.init = xrc_rf_init,
	.set_channel = xrc_rf_set_channel, /* path is interpreted as mode. */
	.get_temp = xrc_get_temp,
	.cfo_adj = xrc_rf_cfo_adj,
	.rf_control = xrc_rf_power_cntrl,
	.cfo_ofs = xrc_cfo_ofs
};

static int xrc_rf_probe(struct device *dev)
{
	dev->driver_data = &xrc_drv_data;

	return 0;
}

#ifdef CONFIG_PM_DM
static int xrc_rf_suspend(struct device *dev, u32 *idle)
{
	u32 val;

	/* set RFMODE(RFI_XRC_CFG2) of PHY as standby mode */
	val = phy_read(dev, FW_RFI_XRC_CFG2);
	val &= ~GENMASK(27, 12);
	phy_write(dev, FW_RFI_XRC_CFG2, val | (0x0421 << 12));

	/*
	 * set xtal settling time to 2.9ms.
	 * Maximize xtal settling time for reducing current because HW wakeup takes about 6ms.
	 */
	xrc_write(dev, 0x01, 0x07, 8, 6);

    _xrc_temp_sens_en = 0;

	return 0;
}

static int xrc_rf_resume(struct device *dev)
{
	struct xrc_driver_data *drv = drv(dev);
	u32 val;

	if (drv && drv->writefn) {
		phy_modem_write(dev, WIFI_BLE_CTRLMUX_CFG1, 0x00010081);

		/* set RFMODE(RFI_XRC_CFG2) of PHY as default mode */
		val = phy_read(dev, FW_RFI_XRC_CFG2);
		val &= ~GENMASK(27, 12);
		phy_write(dev, FW_RFI_XRC_CFG2, val | (0x1f82 << 12));

		val = phy_read(dev, FW_RFI_XRC_CFG);
		val |= GENMASK(8, 8);
		phy_write(dev, FW_RFI_XRC_CFG, val);
	}

	/* set xtal settling time to 0.9ms return to xrc recommend value */
	xrc_write(dev, 0x01, 0x02, 8, 6);

	return 0;
}
#endif

static declare_driver(rf) = {
	.name = "rf",
	.probe = xrc_rf_probe,
	.ops = &xrc_rf_ops,
#ifdef CONFIG_PM_DM
	.suspend = xrc_rf_suspend,
	.resume = xrc_rf_resume,
#endif
};

#ifdef CONFIG_RF_CLI
#include <string.h>
#include <cli.h>


static void xrc_bt_tx_gain_manual_config(struct device *dev, int gain_index) __maybe_unused;
static void xrc_bt_tx_gain_manual_config(struct device *dev, int gain_index)
{
	xrc_write(dev, 0x02f, 0x1, 1, 1);	// 10’h02f[1:1]: TX Gain Control method in BT Mode, 1:B_BT_TX_GAIN_MAN, 0:Automatically controlled by internal signal
	xrc_write(dev, 0x02f, gain_index, 11, 7); // 10'h02f[11:7], TX Gain in BT Mode @ B_BT_TX_GAIN_MAN_EN=1
}

static void xrc_wifi_rx_gain_manual_config(struct device *dev, int rxfe_gain_index, int rx_lpf_gain_index) __maybe_unused;
static void xrc_wifi_rx_gain_manual_config(struct device *dev, int rxfe_gain_index, int rx_lpf_gain_index)
{
	xrc_write(dev, 0x01f, 0x1, 10, 10);	// 10’h01f[10:10]: B_WIFI_RXFE_GAIN_MAN_EN, 1: B_WIFI_RXFE_GAIN_MAN, 0: BB_WIFI_RXFE_GAIN
	xrc_write(dev, 0x01f, rxfe_gain_index, 9, 7);	// 10’h01f[9:7]: RXFE GAIN value @manual mode
	xrc_write(dev, 0x020, 0x1, 0, 0);	// 10’h020[0:0]: LPF Gain control selection, 1: B_RX_LPF_GAIN_MAN<3:0>, 0: BB_WIFI_LPF_GAIN<3:0>
	xrc_write(dev, 0x020, rx_lpf_gain_index, 4, 1);	// 10’h020[4:1]: LPF Gain control selection
}

static void xrc_bt_rx_gain_manual_config(struct device *dev, int rx_gain) __maybe_unused;
static void xrc_bt_rx_gain_manual_config(struct device *dev, int rx_gain)
{
	xrc_write(dev, 0x020, 0x1, 8, 8);	// 10’h020[8:8]: BT RX Gain Control Method, 1: BT RX Gain from B_BT_RX_GAIN_MAN, 0: BT RX Gain from internal control
	xrc_write(dev, 0x020, rx_gain, 7, 5);	// 10’h020[7:5]: 000: ULG, Gain=-4dB, 001: LG, Gain=18dB, 010: MG, Gain=36dB, 011: HG, Gain=54dB, 111: UHG, Gain=66dB
}

static void xrc_test(struct device *dev) __maybe_unused;
static void xrc_test(struct device *dev)
{
	int fcs_ok = 100;
//	int vco_cal = (xrc_read(dev, 0x87) >> 1) & 0xFF;
//	xrc_write(dev, 0x086, (vco_cal << 1) + 1, 8, 0);		// VCO_CAL manual enable
	while(fcs_ok != 0)
	{
		int i = 0;
		fcs_ok = 0;
		udelay(1000);
#if 0
		//xrc_write(dev, 0x006, 0x0, 9, 9);		// B_WIFI_TRX_SWITCH_SEL = 0
		phy_write(dev, FW_RFI_XRC_CFG, 0x85000106);	// TX LDO control by BB signal
		udelay(10);
		//phy_modem_write(dev, 0x000c, 1);		// PHY SW_RESET
		//udelay(5);
		xrc_write(dev, 0x001, 0x26a0, 15, 0);		// RF manual mode = standby
		//xrc_write(dev, 0x006, 0x1, 9, 9);		// B_WIFI_TRX_SWITCH_SEL = 1
		//phy_write(dev, FW_RFI_XRC_CFG, 0x05000106);	// TX LDO control by BB signal
		udelay(20);
		xrc_write(dev, 0x001, 0x22a0, 15, 0);		// RF manual mode = control by modem
		udelay(70);
		//xrc_write(dev, 0x006, 0x1, 9, 9);		// B_WIFI_TRX_SWITCH_SEL = 1
		phy_write(dev, FW_RFI_XRC_CFG, 0x05000106);	// TX LDO control by BB signal
#else
		xrc_rf_set_channel(dev, 0, 2442, 0);
#endif
		phy_modem_write(dev, 0x000c, 1);		// PHY SW_RESET
		phy_modem_write(dev, 0x00fc, 1);		// PHY debug count reset
		printf(".");

		for(i=0;i<2000;i++)
		{
			fcs_ok = phy_modem_read(dev, 0x017c);
			if(fcs_ok > 1)
			{
#if 0
				printf("-2-");
				//xrc_write(dev, 0x006, 0x1, 9, 9);		// B_WIFI_TRX_SWITCH_SEL = 1
				phy_modem_write(dev, 0x00fc, 1);		// PHY debug count reset
				udelay (1000);
				for(j=0;j<2000;j++)
				{
					fcs_ok2 = phy_modem_read(dev, 0x017c);
					if(fcs_ok2 > 1)
					{
						printf("s_to_n FCS_OK = %3d, LDO_PRAP FCS_OK = %3d\n",fcs_ok, fcs_ok2);
						break;
					}
					udelay (1000);
				}
#endif
				break;
			}
			udelay(1000);
		}
	}
}

static inline int strtoul10(const char *s)
{
	return strtoul(s, NULL, 10);
}

static inline int strtoul16(const char *s)
{
	return strtoul(s, NULL, 16);
}

static int do_xrc(int argc, char *argv[])
{
	struct device *rf = device_get_by_name("rf");

	if (!rf)
		return CMD_RET_FAILURE;

	if (argc < 2)
		return CMD_RET_USAGE;

#ifndef CONFIG_MFG
	if (!strcmp("init", argv[1])) {
		if (argc < 4)
			return CMD_RET_USAGE;
		if (!strcmp("rf", argv[2])) {
			xrc_rf_reg_init(rf);
		} else if (!strcmp("afe", argv[2])) {
			xrc_afe_init(rf);
			return CMD_RET_FAILURE;
		} else
			return CMD_RET_FAILURE;
	} else if (!strcmp("off", argv[1])) {
		xrc_rf_power_cntrl(rf, 0);
	} else if (!strcmp("on", argv[1])) {
		xrc_rf_power_cntrl(rf, 1);
	} else if (!strcmp("write", argv[1])) {
		if (argc < 6)
			return CMD_RET_USAGE;
		xrc_write(rf, (u32)strtoul10(argv[2]), (u32)strtoul10(argv[3]), (u8)strtoul10(argv[4]), (u8)strtoul10(argv[5]));
	} else if (!strcmp("read", argv[1])) {
		u32 value;
		if (argc < 3)
			return CMD_RET_USAGE;
		value = xrc_read(rf, (u32)strtoul10(argv[2]));
		printf("[0x%x] 0x%08x\n", strtoul16(argv[2]), value);
	} else if (!strcmp("chan", argv[1])) {
		if (argc < 3)
			return CMD_RET_USAGE;
		xrc_channel_config_index(rf, (u8)strtoul10(argv[2]));
	} else if (!strcmp("rf", argv[1])) {
		if (!strcmp("bw", argv[2])) {
			if (argc < 5)
				return CMD_RET_USAGE;
			xrc_bandwidth_config(rf, (u8)strtoul10(argv[3]), (u8)strtoul10(argv[4]));
		} else if (!strcmp("init", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			xrc_rf_init(rf, (u8)strtoul10(argv[3]));
			printf("xrc_rf_init\n");
		} else if (!strcmp("dbg", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			_rf_dbg = (u8)strtoul10(argv[3]);
		} else if (!strcmp("delay", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			delay = (u32)strtoul10(argv[3]);
			printf("delay= %d\n", delay);
		} else if (!strcmp("meas", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			meas = (u8)strtoul10(argv[3]);
			printf("meas= %d\n", meas);
		} else if (!strcmp("rxg", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			rxg = (u8)strtoul10(argv[3]);
			printf("rxg = %d\n", rxg);
		} else if (!strcmp("apply", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			u8 print_en = (u8)strtoul10(argv[3]);
			apply_tx_cal(rf, print_en);
		} else if (!strcmp("temp", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			u8 print_en = (u8)strtoul10(argv[3]);
			xrc_get_temp(rf, print_en);
		} else if (!strcmp("set_txcl", argv[2])) {
			if (argc < 5)
				return CMD_RET_USAGE;
			int DCI = (u32)strtoul10(argv[3]);
			int DCQ = (u32)strtoul10(argv[4]);
			apply_txcl_cal(rf, DCI, DCQ);
		} else if (!strcmp("set_txssb", argv[2])) {
			if (argc < 5)
				return CMD_RET_USAGE;
			int DP = (u32)strtoul10(argv[3]);
			int DG = (u32)strtoul10(argv[4]);
			apply_txssb_cal(rf, DP, DG);
		} else if (!strcmp("set_rxiq", argv[2])) {
			if (argc < 5)
				return CMD_RET_USAGE;
			int DP = (u32)strtoul10(argv[3]);
			int DG = (u32)strtoul10(argv[4]);
			apply_rxiq_cal(rf, DP, DG);
		} else if (!strcmp("get_txiq", argv[2])) {
			get_txiq_cal(rf);
		} else if (!strcmp("get_rxiq", argv[2])) {
			get_rxiq_cal(rf);
		} else if (!strcmp("opmode", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			xrc_wifi_mode_manual_config(rf, (int)strtoul10(argv[3]));
		} else if (!strcmp("wifi_tgm", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			xrc_wifi_tx_gain_manual_config(rf, (int)strtoul10(argv[3]));
		} else if (!strcmp("wifi_rgm", argv[2])) {
			if (argc < 5)
				return CMD_RET_USAGE;
			xrc_wifi_rx_gain_manual_config(rf, (int)strtoul10(argv[3]), (int)strtoul10(argv[4]));
		} else if (!strcmp("bt_tgm", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			xrc_bt_tx_gain_manual_config(rf, (int)strtoul10(argv[3]));
		} else if (!strcmp("bt_rgm", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			xrc_bt_rx_gain_manual_config(rf, (int)strtoul10(argv[3]));
		} else if (!strcmp("rx_lpf_ac_cal", argv[2])) {
			if (argc < 3)
				return CMD_RET_USAGE;
			xrc_wifi_rx_lpf_ac_cal(rf, (u8)strtoul10(argv[3]));
		} else if (!strcmp("rx_dcoc_cal", argv[2])) {
			if (argc < 6)
				return CMD_RET_USAGE;
			xrc_rx_dcoc_cal(rf, (u8)strtoul10(argv[3]), (u8)strtoul10(argv[4]), (u8)strtoul10(argv[5]));
		} else if (!strcmp("tx_lo_leakage_coarse_cal", argv[2])) {
			xrc_tx_lo_leakage_coarse_cal(rf, (u8)strtoul10(argv[3]));
		} else if (!strcmp("tx_cal", argv[2])) {	// tx_iq_lo_leakage_fine_cal
			xrc_tx_iq_lo_leakage_fine_cal(rf, (u8)strtoul10(argv[3]));
		} else if (!strcmp("test", argv[2])) {
                        xrc_test(rf);
		} else if (!strcmp("rx_iq_cal", argv[2])) {
			xrc_rx_iq_cal(rf, (u8)strtoul10(argv[3]), (u8)strtoul10(argv[4]));
		} else if (!strcmp("tx_dpd_cal", argv[2])) {
			xrc_tx_dpd_cal(rf);
		}
	} else if (!strcmp("chan_freq", argv[1])) {
		if (argc < 5)
			return CMD_RET_USAGE;
		xrc_rf_set_channel(rf, (u8)strtoul10(argv[2]), (u32)strtoul10(argv[3]), (u8)strtoul10(argv[4]));
	} else if (!strcmp("cfo_adj", argv[1])) {
		if (argc < 3)
			return CMD_RET_USAGE;
		xrc_rf_cfo_adj(rf, (int)strtoul10(argv[2]));
	} else if(!strcmp("tone", argv[1])){
			int tone_offset[8] = {0,};
			int i = 0;
			if (argc < 4)
				return CMD_RET_USAGE;
			if((u8)strtoul10(argv[3]) == 0)
			{
				xrc_phy_multi_tone_disable(rf);
			}
			else
			{
				for (i = 4 ; i < argc ; i++)
				{
					tone_offset[i-4] = (int)strtoul10(argv[i]);
				}
				xrc_phy_multi_tone(rf, (u8)strtoul10(argv[2])
								, (u8)strtoul10(argv[3])
								, tone_offset[0]
								, tone_offset[1]
								, tone_offset[2]
								, tone_offset[3]
								, tone_offset[4]
								, tone_offset[5]
								, tone_offset[6]
								, tone_offset[7]
								);
			}
    }
#else
    /* To be added for MFG. */
    if (0) {
    }
#endif
	else if (!strcmp("cfo_ofs", argv[1])) {
                if (argc < 3)
			return CMD_RET_USAGE;
		xrc_cfo_ofs(rf,(int)strtoul10(argv[2]));
	} else
		return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

CMD(xrc, do_xrc,
	"CLI commands for xrc b/d",
#ifndef CONFIG_MFG
	"xrc init (rf / afe)" OR
	"xrc off" OR
	"xrc on" OR
	"xrc write addr data msb lsb" OR
	"xrc read addr" OR
	"xrc chan index" OR
	"xrc rf bw" OR
	"xrc rf init [0 or 1]" OR
	"xrc rf dbg [0 or 1]" OR
	"xrc rf opmode mode[0:auto mode / 1:TX mode / 2:RX mode / 3:Loopback mode]" OR
	"xrc rf wifi_tgm gain_index" OR
	"xrc rf wifi_rgm rxfe_gain_index rx_lpf_gain_index" OR
	"xrc rf rx_lpf_ac_cal printf_en" OR
	"xrc rf rx_dcoc_cal lpf_gain(if 16 = disable) digital_hpf_en printf_en" OR
	"xrc rf tx_lo_leakage_coarse_cal printf_en" OR
	"xrc rf tx_cal printf_en (tx_iq_lo_leakage_fine_cal)" OR
	"xrc rf rx_iq_cal" OR
	"xrc rf tx_dpd_cal" OR
	"xrc chan_freq mode freq bw" OR
	"xrc cfo_adj ppm" OR
	"xrc tone digi_gain[40: 0dB] tone_num[if 0 : disable] tone1_ofs [tone2_ofs tone3_ofs ... tone8_ofs]" OR
	"xrc rf delay(Txcal delay)" OR
	"xrc rf meas(Txcal meas_loop)" OR
	"xrc rf apply print_en" OR
	"xrc rf temp print_en" OR
	"xrc rf test" OR
	"xrc rf rxg value(RxLUT idx)" OR
	"xrc rf set_txcl DCI DCQ(DCI default 0, DCQ default 0)" OR
	"xrc rf set_txssb DP DG(DP default 0, DG default 4096)" OR
	"xrc rf set_rxiq DP DG(DP default 0, DG default 4096)" OR
	"xrc rf get_rxiq" OR
#endif
	"xrc cfo_ofs ofs"
);

#endif

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
#include <hal/rf.h>
#include <stdlib.h>

u8 _afe_dbg = 0;
u8 _rf_dbg = 0;

static struct mango_driver_data {
	reg_write writefn;
	reg_read readfn;
} mango_drv_data;

#define drv(dev)        ((struct mango_driver_data *)(dev->driver_data))
#define regw(a, v)      (*drv(dev)->writefn)(a, v)
#define regr(a)         (*drv(dev)->readfn)(a)

#define phyver()        ((regr(0x000003f4) & 0xfff0) >> 4)
#define macver()        (((*(u32*)0x65008018) & 0xfff0) >> 4)
#define prodc()         ((regr(0x000003f4) & 0xffff0000) >> 16)
#define is_scm2020()    (prodc() == 0xB0BA)
#define is_scm2010()    (prodc() == 0xDCAF)

#define assert_spi(dev)	assert(drv(dev)->writefn && drv(dev)->readfn)

// 2.4G PLL Config
u32 const mango_fpga_pll_24GHz_reg3[14] = {0x00A0, 0x20A1, 0x30A1, 0x00A1, 0x20A2, 0x30A2, 0x00A2, 0x20A3, 0x30A3, 0x00A3,
				     0x20A4, 0x30A4, 0x00A4, 0x10A5};
u32 const mango_fpga_pll_24GHz_reg4[14] = {0x3333, 0x0888, 0x1DDD, 0x3333, 0x0888, 0x1DDD, 0x3333, 0x0888, 0x1DDD, 0x3333,
				     0x0888, 0x1DDD, 0x3333, 0x2666};
// 5G PLL Config
u32 const mango_fpga_pll_50GHz_reg3[23] = {0x30CF, 0x00D0, 0x00D0, 0x10D1, 0x20D2, 0x30D3, 0x00D4, 0x00D4, 0x00DC, 0x00DC,
				     0x10DD, 0x20DE, 0x30DF, 0x00E0, 0x00E0, 0x10E1, 0x20E2, 0x30E3, 0x00E4, 0x00E5,
				     0x10E6, 0x20E7, 0x30E8};
u32 const mango_fpga_pll_50GHz_reg4[23] = {0x0CCC, 0x0000, 0x3333, 0x2666, 0x1999, 0x0CCC, 0x0000, 0x3333, 0x0000, 0x3333,
				     0x2666, 0x1999, 0x0CCC, 0x0000, 0x3333, 0x2666, 0x1999, 0x0CCC, 0x0000, 0x3333,
				     0x2666, 0x1999, 0x0CCC};

// MAX2829 RF Driver
static u32 mango_reg_default[13][2] = {
	{0x1140, 0x1140},	// Register 0
	{0x00CA, 0x00CA},	// Register 1
	{0x1007, 0x1007},	// Standby
	{0x30A2, 0x30A2},	// Integer-Divider Ratio
	{0x1DDD, 0x1DDD},	// Fractional-Divider Ratio
	{0x1824, 0x1824},	// Band Select and PLL
	{0x1C00, 0x1C00},	// Calibration
	{0x002A, 0x002A},	// Lowpass Filter
	{0x0025, 0x0025},	// Rx Control/RSSI
	{0x0200, 0x0200},	// Tx Linearity/Baseband Gain
	{0x03C0, 0x03C0},	// PA Bias DAC
	{0x007F, 0x007F},	// Rx Gain
	{0x0000, 0x0000}	// Tx VGA Gain
};

//mango write path mode addr data
static
void mango_write(struct device *dev, u8 path, u8 mode, u32 addr, u32 data)
/*
path : 0 , A path
path : 1 , B path
mode : 0 , Mango AFE SPI
mode : 1 , Mango RF SPI
*/
{
#ifdef USE_CSPI
	if(path == 0 && mode == 0)
	{
		regw(0x00008734, addr<<1);
		regw(0x00008738, data);
		regw(0x00008740, 0x1);
		if(_afe_dbg == 1)	printf("[AFEA SPI] Addr[0x%04X], Data[0x%08X]\n",addr,data);
		udelay(10);
	}
	else if(path == 0 && mode == 1)
	{
		regw(0x00008754, addr<<1);
		regw(0x0000875C, data);
		regw(0x00008760, 0x1);
		if(_rf_dbg == 1)	printf("[RFA  SPI] Addr[0x%04X], Data[0x%08X]\n",addr,data);
		udelay(10);
	}
	else if(path == 1 && mode == 0)
	{
		regw(0x0000C734, addr<<1);
		regw(0x0000C738, data);
		regw(0x0000C740, 0x1);
		if(_afe_dbg == 1)	printf("[AFEB SPI] Addr[0x%04X], Data[0x%08X]\n",addr,data);
		udelay(10);
	}
	else if(path == 1 && mode == 1)
	{
		regw(0x0000C754, addr<<1);
		regw(0x0000C75C, data);
		regw(0x0000C760, 0x1);
		if(_rf_dbg == 1)	printf("[RFB  SPI] Addr[0x%04X], Data[0x%08X]\n",addr,data);
		udelay(10);
	}
#else
	u8 spi_leng = 24 - mode * 6;
	regw(0x00008798 + (path * 0x4000) + (mode * 0x30), spi_leng);
	if (mode==0)
		regw(0x0000878C + (0x4000 * path) + (0x30 * mode), (addr<<8) | data);
	else
		regw(0x0000878C + (0x4000 * path) + (0x30 * mode), (data<<4) | addr);
	regw(0x00008784 + (0x4000 * path) + (0x30 * mode), 0x1);
	if(_rf_dbg == 1)        printf("[Mode:%d Path:%d  SPI] Addr[0x%04X], Data[0x%08X]\n",mode, path, addr, data);
#endif
}

//mango read path addr
static
u32 mango_read(struct device *dev, u8 path, u32 addr)
/*
Only AFE can read
path : 0 , Mango AFE A SPI
path : 1 , Mango AFE B SPI
*/
{
	u32 spi_read_value = 0;
#ifdef USE_CSPI
	if(path == 0)
	{
		regw(0x00008734, addr<<1 | 0x1);
		regw(0x00008740, 0x1);
		udelay(10);
		spi_read_value = regr(0x00008744);
	}
	else if(path == 1)
	{
		regw(0x0000C734, addr<<1 | 0x1);
		regw(0x0000C740, 0x1);
		udelay(10);
		spi_read_value = regr(0x0000C744);
	}
#else
	u8 spi_leng = 24;
	regw(0x00008798 + (path * 0x4000), spi_leng - 8);
	regw(0x0000878C + (0x4000 * path), (1 << (spi_leng - 1)) | (addr<<8));
	regw(0x00008784 + (0x4000 * path), 0x1);
	spi_read_value = regr(0x00008790 + (0x4000 * path)) & 0xff;
	if(_rf_dbg == 1)        printf("[Read Path:%d  SPI] Addr[0x%04X], Data[0x%02X]\n", path, addr, spi_read_value);
#endif
	return spi_read_value;
}

//mango chan_freq path freq
static
void mango_channel_config_freq(struct device *dev, u8 path, u32 freq)
{
	u32 calc_reg;
	u32 div_cont_1, div_cont_2;

	if(freq < 3000)
	{
		div_cont_1 = 4;
		div_cont_2 = 3;
		mango_write(dev, path, 0x1, 0x5, (mango_reg_default[0x5][path] & ~(0x41)) | 0x0);
	}
	else
	{
		div_cont_1 = 4;
		div_cont_2 = 5;
		if(freq < 5400)
			mango_write(dev, path, 0x1, 0x5, (mango_reg_default[0x5][path] & ~(0x41)) | 0x1);
		else
			mango_write(dev, path, 0x1, 0x5, (mango_reg_default[0x5][path] & ~(0x41)) | 0x41);
	}

	calc_reg = (1 << 16) * freq * div_cont_1 / (div_cont_2 * 20);

	mango_write(dev, path, 0x1, 0x3, ((calc_reg & 0x3) << 12) | calc_reg>>16);
	mango_write(dev, path, 0x1, 0x4, (calc_reg >> 2) & 0x3fff);

	//printf("[Freq] %04d, [Int] 0x%02X, [Frac] 0x%04X, [Frac_LSB] 0x%x\n",freq, calc_reg>>16, (calc_reg >> 2) & 0x3fff, calc_reg & 0x3);
}

//mango rf tbw path bw
//TX Bandwidth config
static
void mango_tx_bandwidth_config(struct device *dev, u8 path, int bw)
/*
	bw :	0, 20MHz
		1, 40MHz
*/
{
	u32 temp_reg;
	temp_reg = (mango_reg_default[7][path] & ~(0x3 << 5)) | (((bw << 1) | 1) << 5);
	mango_reg_default[7][path] = temp_reg;
	mango_write(dev, path, 0x1, 0x7, mango_reg_default[7][path]);
}

//mango rf rbw path bw
//RX Bandwidth config
static
void mango_rx_bandwidth_config(struct device *dev, u8 path, int bw)
/*
	bw :	0, 20MHz
		1, 40MHz
*/
{
	u32 temp_reg;
	temp_reg = (mango_reg_default[0x7][path] & ~(0x3 << 3)) | (((bw << 1) | 1) << 3);
	mango_reg_default[0x7][path] = temp_reg;
	mango_write(dev, path, 0x1, 0x7, mango_reg_default[0x7][path]);

	if(bw == 0)
	{
		mango_write(dev, path, 0, 0x71, 0x51);
	}
	else
	{
		mango_write(dev, path, 0, 0x71, 0xD1);
	}
}

static
u8 mango_afe_config_DLL(struct device *dev, u8 path, u8 DLL_En,
		u8 DLL_M, u8 DLL_N, u8 DLL_DIV)
/**
\brief Configures the AD9963 DLL block. DLL output clock is REFCLK*M/(N*DLL_DIV). REFCLK*M must be in [100,310]MHz. See the AD9963 for more details.
\param DLL_En DLL Enable (1=DLL enabled, 0=DLL disabled). Other arguments are ignored when DLL_En=0
\param DLL_M DLL multiplication (M) parameter; must be in [0,1,...,31] for multiplications of [1,2,...,32], constrained by M*REFCLK in [100, 310]MHz
\param DLL_N DLL division (N) parameter; must be one of [1,2,3,4,5,6,8]
\param DLL_DIV Secondary DLL divider; must be one of [1,2,4]
\return Returns 0 on success, 0xff for invalid paramters, 0xfe if DLLs fail to lock with new settings
*/
{
	u32 regVal;
	u32 bitsToSet_reg71;
	u32 lockAttempts = 100;

	if( (DLL_N == 7) || (DLL_N == 0) || (DLL_N > 8) || (DLL_M > 31) || (DLL_DIV == 0) || (DLL_DIV == 3) || (DLL_DIV > 4))
		return 0xff;

	/*
	AD9963 reg 0x60:
		7: DLL_EN (1=enable DLL)
		6:0 other block disables

	AD9963 reg 0x71:
		7:6 ADC/DAC clock selection
		5: reserved
		4: 1=enable DLL clock input
		3:0 DLL N (only valid values: 1,2,3,4,5,6,8)

	AD9963 reg 0x72:
		7: DLL locked (read-only)
		6:5 DLLDIV (secondary divider value)
		4:0 DLL M ([1:32] all valid)

	AD9963 reg 0x75:
		7:4 must be 0
		3: DLL_RESB: DLL reset (must transition low-to-high after any DLL parameter change)* see NOTE below
		2:0 must be 0

	*/

	//NOTE! The AD9963 datasheet claims DLL_RESB (reg 0x75[3]) is active low. I'm pretty sure
	// that's wrong (yet another AD9963 datasheet bit-flip). The code below treats DLL_RESB as
	// active high, the only interpreation we've seen work in hardware.


	if(DLL_En == 0) {
		//Assert DLL reset
		mango_write(dev, path, 0x0, 0x75, 0x08);

		//Power down DLL block, leave all other blocks powered on
		mango_write(dev, path, 0x0, 0x60, 0x00);

		//Disable DLL clock input, set ADC/DAC clock sources to ext ref clock
		mango_write(dev, path, 0x0, 0x71, 0xC0);

		return 0;

	} else {
		//Assert DLL reset
		mango_write(dev, path, 0x0, 0x75, 0x08);

		//Power up DLL block, leave all other blocks powered on
		mango_write(dev, path, 0x0, 0x60, 0x80);

		//Assert DLL clock enable, set DLL_N
		bitsToSet_reg71 = 0x10 | (DLL_N & 0xF);

		//reg71 has bits to preserve, so handle it separately for each AD
		regVal = (u32)mango_read(dev, path, 0x71);
		regVal = (regVal & ~0x1F) | bitsToSet_reg71;
		mango_write(dev, path, 0x0, 0x71, regVal);

		//Other registers are DLL-only, so we can write both ADs together

		//Set DLL_DIV and DLL_M
		mango_write(dev, path, 0x0, 0x72, ((DLL_DIV&0x3)<<5) | (DLL_M & 0x1F));

		//Release DLL reset (treating as active high)
		udelay(100);
		mango_write(dev, path, 0x0, 0x75, 0x00);

		//Wait for both DLLs to lock
		while( (lockAttempts > 0) && ( (mango_read(dev, path, 0x72) & 0x8080) != 0x8080) ) {
			lockAttempts--;
			udelay(10);
		}

		//If the wait-for-lock loop timed out, return an error
		if(lockAttempts == 0) return 0xfe;
		else return 0;
	}
}

static
u8 mango_afe_config_clocks(struct device *dev, u8 path, u8 DAC_clkSrc,
		u8 ADC_clkSrc, u8 ADC_clkDiv, u8 ADC_DCS)
/**
\brief Configures the ADC and DAC clock sources in the AD9963. Refer to the WARP v3 user guide and AD9963 for details on various clocking modes
\param DAC_clkSrc DAC clock source; must be AD_DACCLKSRC_DLL (use DLL clock) or AD_DACCLKSRC_EXT (use external reference clock) (1=DLL, 0=ext)
\param ADC_clkSrc ADC clock source; must be AD_ADCCLKSRC_DLL (use DLL clock) or AD_ADCCLKSRC_EXT (use external reference clock) (1=DLL, 0=ext)
\param ADC_clkDiv ADC clock divider; must be one of [AD_ADCCLKDIV_1, AD_ADCCLKDIV_2, AD_ADCCLKDIV_4] for divide-by of [1, 2, 4]
\param ADC_DCS ADC duty cycle stabilizer; must be AD_DCS_ON or AD_DCS_OFF. AD9963 datasheet recommends DCS be enabled only for ADC rates above 75MHz.
\return Returns 0 on success, 0xff for invalid paramters
*/
{

	u32 regVal;
	u32 bitsToSet_reg66, bitsToSet_reg71;

	/* AD9963 reg 0x66:
		7:6 Disable DAC clocks
		4:3 Disable ADC clocks
		2: Disable DCS
		1:0 ADCDIV

	  AD9963 reg 0x71:
		7: ADC clock selection (1=DLL, 0=ext)
		6: DAC clock selection (1=DLL, 0=ext)
		4:0 DLL config
	*/

	//Assert sane default bits, and any config bits user options require
	bitsToSet_reg66 = (ADC_DCS << 2) | (ADC_clkDiv & 0x3);
	bitsToSet_reg71 = (DAC_clkSrc << 6) | (ADC_clkSrc << 7);

	//For RFA and RFB, clear-then-set affected bits in clock config registers (0x66 and 0x71)
	regVal = (u32)mango_read(dev, path, 0x66);
	regVal = regVal & ~(0x07);
	regVal = regVal | bitsToSet_reg66;
	mango_write(dev, path, 0x0, 0x66, regVal);

	regVal = (u32)mango_read(dev, path, 0x71);
	regVal = regVal & ~(0xc0);
	regVal = regVal | bitsToSet_reg71;
	mango_write(dev, path, 0x0, 0x71, regVal);

	return 0;

}

static
u8 mango_afe_config_filters(struct device *dev, u8 path,
		u8 interpRate, u8 decimationRate)
/**
\brief Configures the digital rate-change filters in the AD9963. Changing filter settings affects the require data rate
at the TXD and TRXD ports. You must ensure all related paramters (AD9963 filters, I/Q rate in FPGA, AD9512 dividers) are consistent.
\param baseaddr Base memory address of w3_ad_controller pcore
\param csMask OR'd combination of RFA_AD_CS and RFB_AD_CS
\param interpRate Desired interpolation rate in AD9963; must be one of [1, 2, 4, 8]
\param decimationRate Desired decimation rate in AD9963; must be one of [1, 2]
\return Returns 0 on success, -1 for invalid paramters
*/
{
	/* AD9963 register 0x30:
		7:6 Reserved
		5: DEC_BP Bypass Rx decimation filter	0x20
		4: INT1_BP Bypass Tx INT1 filter		0x10
		3: INT0_BP Bypass Tx INT0 filter		0x08
		2: SRRC_BP Bypass Tx SRRC filter		0x04
		1: TXCLK_EN Enable Tx datapath clocks	0x02
		0: RXCLK_EN Enable Rx datapath clocks	0x01

		Tx filter config:
		1x: All Tx filters bypased
		2x: INT0 enabled
		4x: INT0, INT1 enbaled
		8x: All Tx fitlers enabled
	*/

	u32 regVal;

	//Enable Tx/Rx clocks by default
	regVal = 0x3;

	switch(interpRate) {
		case 1:
			regVal = regVal | 0x1C;
			break;
		case 2:
			regVal = regVal | 0x14;
			break;
		case 4:
			regVal = regVal | 0x04;
			break;
		case 8:
			break;
		default:
			//Invalid interp rate; return error
			return 0xff;
			break;
	}

	if(decimationRate == 1) {
		regVal = regVal | 0x20;
	} else if(decimationRate != 2) {
		//Invalid decimation rate; return error
		return -1;
	}

	//Write reg 0x30 in selected AD9963's
	mango_write(dev, path, 0x0, 0x30, regVal);

	return 0;
}

static
u8 mango_afe_set_TxDCO(struct device *dev, u8 path, u8 iqSel, u32 dco)
/**
\brief Sets the DC offset for the selected path (I or Q) in the selected AD9963s
\param baseaddr Base memory address of w3_ad_controller pcore
\param csMask OR'd combination of RFA_AD_CS and RFB_AD_CS
\param iqSel Select I or Q path; must be AD_CHAN_I(0) or AD_CHAN_Q(1)
\param dco DC offset to apply, in [0,1024]
*/
{

	//Sanity check inputs
	if(dco > 1023)
		return 0xff;

	if(iqSel == 0) {
		//AUXIO2=DAC10A=I DAC TxDCO
		mango_write(dev, path, 0x0, 0x49, ((dco & 0x3FF) >> 2)); //DAC10A data[9:2] - 8 MSB
		mango_write(dev, path, 0x0, 0x4A, (dco & 0x3)); //DAC10A {6'b0, data[1:0]} - 2 LSB
	}
	else if(iqSel == 1) {
		//AUXIO3=DAC10B=Q DAC TxDCO
		mango_write(dev, path, 0x0, 0x46, ((dco & 0x3FF) >> 2)); //DAC10B data[9:2] - 8 MSB
		mango_write(dev, path, 0x0, 0x47, (dco & 0x3)); //DAC10B {6'b0, data[1:0]} - 2 LSB
	}

	return 0;
}

//mango afe
static
u8 mango_afe_init(struct device *dev, u8 path)
/**
\brief Initializes the AD controller. This function must be called once at boot before any AD or RF operations will work
\param baseaddr Base memory address of w3_ad_controller pcore
\param clkdiv Clock divider for SPI serial clock (set to 3 for 160MHz bus)
*/
{
	//u32 rstMask;
	u32 reg5c, reg72, reg5c_check, reg72_check;

	if(path > 2) {
		printf("mango_afe_init: Invalid path parameter!\n");
		return 0xff;
	}

	//rstMask = 0x10;
	reg5c_check = 0x00000008;
	reg72_check = 0x00000001;
/*
	//Toggle AD resets (active low), Set SPI clock divider
	spi_write(baseaddr + ADCTRL_REG_CONFIG, 0);
	Xil_Out32(baseaddr + ADCTRL_REG_CONFIG, (clkdiv & ADCTRL_REG_CONFIG_MASK_CLKDIV) | rstMask);

	//Toggle soft reset, set SDIO pin to bidirectional (only way to do SPI reads)
	mango_write(dev, path, 0x0, 0x00, 0xBD); //SDIO=1, LSB_first=0, reset=1
	mango_write(dev, path, 0x0, 0x00, 0x99); //SDIO=1, LSB_first=0, reset=0
*/
	//Confirm the SPI ports are working
	//AD9963 reg5C should be 0x08 always, reg72 is 0x1 on boot
#if 1
	reg5c = (mango_read(dev, path, 0x5C))&reg5c_check;
	reg72 = (mango_read(dev, path, 0x72))&reg72_check;
#else
	reg5c = reg5c_check;
	reg72 = reg72_check;
#endif
	if((reg5c != reg5c_check) || (reg72 != reg72_check)) {
		printf("First AD SPI read was wrong: addr[5C]=0x%08x (should be 0x%08x), addr[72]=0x%08x (should be 0x%08x)\n", reg5c, reg5c_check, reg72, reg72_check);
		printf("Asserting AD9963 resets\n");
/*
		Xil_Out32(baseaddr + ADCTRL_REG_CONFIG, 0);
*/
		return 0xff;
	}

	/* Default AD9963 configuration:
		-External ref resistor (NOTE: apparent datasheet bug!)
		-Full-duplex mode (Tx data on TXD port, Rx data on TRXD port)
		-Power up everything except:
			-DAC12A, DAC12B, AUXADC (all unconnected on PCB)
			-DLL
		-Clocking:
			-DLL disabled
			-ADC clock = DAC clock = ext clock (nominally 80MHz)
		-Tx path:
			-Data in 2's complement (NOTE: datasheet bug!)
			-TXCLK is input at TXD sample rate
			-TXD is DDR, I/Q interleaved, I first
			-Tx interpolation filters bypassed
			-Tx gains:
				-Linear gain set to 100%
				-Linear-in-dB gain set to -3dB
				-DAC RSET set to 100%
			-Tx DCO DACs:
				-Enabled, configured for [0,+2]v range
				-Set to mid-scale output (approx equal to common mode voltage of I/Q diff pairs)
		-Rx path:
			-Data in 2's complement (NOTE: datasheet bug!)
			-TRXCLK is output at TRXD sample rate
			-TRXD is DDR, I/Q interleaved, I first
			-Decimation filter bypassed
			-RXCML output enabled (used by ADC driver diff amp)
			-ADC common mode buffer off (required for DC coupled inputs)
			-Rx I path negated digitally (to match swap of p/n traces on PCB)
	*/

	//Power on/off blocks
	mango_write(dev, path, 0x0, 0x40, 0x00); //DAC12A, DAC12B off
	mango_write(dev, path, 0x0, 0x60, 0x00); //DLL off, everything else on
	mango_write(dev, path, 0x0, 0x61, 0x03); //LDOs on, AUXADC off
	//xil_printf("AD TEST: reg61=0x%08x\n", ad_spi_read(baseaddr,  (adSel), 0x61));

	//Clocking setup
	// [7]=0: ADCCLK=ext clk
	// [6]=0: DACCLK=ext clk
	// [4]=0: disable DLL input ref
	// [3:0]: DLL divide ratio (only 1, 2, 3, 4, 5, 6, 8 valid)
	mango_write(dev, path, 0x0, 0x71, 0x01); //DLL ref input off, ADCCLK=extCLK, DACCLK=extCLK, DLL_DIV=1

	mango_write(dev, path, 0x0, 0x39, 0x01); //TXDBL_SEL

	//Reference resistor selection
	// Datasheet says reg62[0]=0 for internal resistor, reg62[0]=1 for external resistor
	// But experimentally DAC currents are much more stable with temperature when reg62[0]=0
	// I'm guessing this bit is flipped in the datasheet
	mango_write(dev, path, 0x0, 0x62, 0x00);

	//Clock disables and DCS
	// [7:3]=0: Enable internal clocks to ADCs/DACs
	// [1:0]=0: Set ADCCLK=ext clock (no division)
	// [2]=0: Enable ADC duty cycle stabilizer (recommended for ADC rates > 75MHz)
	mango_write(dev, path, 0x0, 0x66, 0x00); //Enable internal clocks, enable DCS

	//Aux DACs (Tx DC offset adjustment)
	// DAC10B=Q DCO, DAC10A=I DCO
	// DAC outputs update after LSB write (configured by reg40[0])
	mango_write(dev, path, 0x0, 0x45, 0x88); //DAC10B on, full scale = [0,+2]v
	mango_write(dev, path, 0x0, 0x46, 0x80); //DAC10B data[9:2]
	mango_write(dev, path, 0x0, 0x47, 0x00); //DAC10B {6'b0, data[1:0]}

	mango_write(dev, path, 0x0, 0x48, 0x88); //DAC10A on, full scale = [0,+2]v
	mango_write(dev, path, 0x0, 0x49, 0x80); //DAC10A data[9:2]
	mango_write(dev, path, 0x0, 0x50, 0x00); //DAC10A {6'b0, data[1:0]}

	//ADC common mode buffer: disabled for DC coupled inputs
	mango_write(dev, path, 0x0, 0x7E, 0x01);

	//Spectral inversion
	// Invert RxI (reg3D[2]=1) to match PCB
	// TxI, TxQ also swapped on PCB, but ignored here since -1*(a+jb) is just one of many phase shifts the Tx signal sees
	mango_write(dev, path, 0x0, 0x3D, 0x04); //Invert RxI to match PCB (board swaps +/- for better routing)

	//Rx clock and data order/format
	// [7:1]=[0 x 1 0 1 0 1] to match design of ad_bridge input registers (TRXD DDR relative to TRXCLK, I data first)
	// [0]=1 for 2's compliment (Datasheet says reg32[0]=0 for 2's compliment, but experiments show 0 is really straight-binary)
	mango_write(dev, path, 0x0, 0x32, 0xA7); // ADC clock is "0" deg.
	//mango_write(dev, path, 0x0, 0x32, 0xAF); // ADC clock is "180" deg.

	//Full-duplex mode (DACs/TXD and ADCs/TRXD both active all the time)
	mango_write(dev, path, 0x0, 0x3F, 0x01); //FD mode

	//Tx data format (datasheet bug! reg[31][0] is flipped; 1=2's complement, 0=straight binary)
	//0x17 worked great with latest ad_bridge (where TXCLK is ODDR(D1=1,D0=0,C=ad_ref_clk_90) and TXD are ODDR (D1=I,D2=Q,C=ad_ref_clk_0))
	//mango_write(dev, path, 0x0, 0x31, 0x97); //Txdata=DDR two's complement,

	// DAC clock phase control
	if((is_scm2020() && (phyver() == 4
		|| phyver() == 5
		|| (phyver() == 6 && macver() == 3)
		|| (phyver() >= 9 && phyver() < 17)))
			|| (is_scm2010() && phyver() < 9))
		mango_write(dev, path, 0x0, 0x31, 0x97); //Txdata=DDR two's complement,
	else
		mango_write(dev, path, 0x0, 0x31, 0x9f); //Txdata=DDR two's complement,

	//Tx/Rx data paths
	mango_write(dev, path, 0x0, 0x30, 0x3F); //Bypass all rate change filters, enable Tx/Rx clocks
//	mango_write(dev, path, 0x0, 0x30, 0x37); //INT0 on, enable Tx/Rx clocks
//	mango_write(dev, path, 0x0, 0x30, 0x27); //INT0+INT1 on, enable Tx/Rx clocks
//	mango_write(dev, path, 0x0, 0x30, 0x23); //INT0+INT1+SRCC on, enable Tx/Rx clocks

	//ADC RXCML output buffer requires special register process (see AD9963 datasheet pg. 21 "sub serial interface communications")
	mango_write(dev, path, 0x0, 0x05, 0x03); //Address both ADCs
	mango_write(dev, path, 0x0, 0x0F, 0x02); //Enable RXCML output
	mango_write(dev, path, 0x0, 0x10, 0x00); //Set I/Q offset to 0
	mango_write(dev, path, 0x0, 0xFF, 0x01); //Trigger ADC param update
	mango_write(dev, path, 0x0, 0x05, 0x00); //De-Address both ADCs

	//REFIO adjustment: set to default of 0.8v
	mango_write(dev, path, 0x0, 0x6E, 0x40);

	//Tx gains (it seems these registers default to non-zero, and maybe non-I/Q-matched values; safest to set them explicitly after reset)
	//I/Q GAIN1[5:0]: Fix5_0 value, Linear-in-dB, 0.25dB per bit
	//  0-25=>0dB-+6dB, 25-32:+6dB, 33-41:-6dB, 41-63=>-6dB-0dB
	mango_write(dev, path, 0x0, 0x68, 0); //IGAIN1
	mango_write(dev, path, 0x0, 0x6B, 0); //QGAIN1

	//I/Q GAIN2[5:0]: Fix5_0 value, Linear +/-2.5%, 0.08% per bit
	// 0:+0, 31:+max, 32:-max, 63:-0
	mango_write(dev, path, 0x0, 0x69, 0); //IGAIN2
	mango_write(dev, path, 0x0, 0x6C, 0); //QGAIN2

	//I/Q RSET[5:0]: Fix5_0, Linear +/-20%, 0.625% per bit
	// 0:-0, 31:-max, 32:+max, 63:+0
	mango_write(dev, path, 0x0, 0x6A, 0); //IRSET
	mango_write(dev, path, 0x0, 0x6D, 0); //QRSET

	//Digital output drive strengths: all 8mA
	mango_write(dev, path, 0x0, 0x63, 0xAA); //2 bits each: TRXD TRXIQ TRXCLK TXCLK

	//Disable Tx and Rx BIST modes
	mango_write(dev, path, 0x0, 0x50, 0x00); //Tx BIST control
	mango_write(dev, path, 0x0, 0x51, 0x00); //Rx BIST control

	return 0;
}

/* Mango board cal value apply*/
static const struct rf_cal_data mango_board_cal[] = {
	[0] = { /* Board #1 */
		{4096, 4096},	//TX SSB param_1
		{   0,    0},	//TX SSB param_0
		{-256, -384},	//TX CL ofs_q
		{ 256,  384},	//TX CL ofs_i
		{4096, 4096},	//RX IMB param_1
		{   0,    0},	//RX IMB param_0
		{-704, -512},	//RX DC ofs_q
		{ 640,   32}	//RX DC ofs_i
	},
	[1] = { /* Board #2 */
		{4096, 4096},	//TX SSB param_1
		{   0,    0},	//TX SSB param_0
		{-256, -256},	//TX CL ofs_q
		{   0, -256},	//TX CL ofs_i
		{4096, 4096},	//RX IMB param_1
		{   0,    0},	//RX IMB param_0
		{   0,    0},	//RX DC ofs_q
		{ 128,  256},	//RX DC ofs_i
	},
	[2] = { /* Board #3 */
		{4096, 4096},	//TX SSB param_1
		{   0,    0},	//TX SSB param_0
		{-1024, -768},	//TX CL ofs_q
		{ 512,  128},	//TX CL ofs_i
		{4096, 4096},	//RX IMB param_1
		{   0,    0},	//RX IMB param_0
		{-576, -512},	//RX DC ofs_q
		{ 576,    0},	//RX DC ofs_i
	},
	[3] = { /* Board #4 */
		{4096, 4096},	//TX SSB param_1
		{   0,    0},	//TX SSB param_0
		{-128,    0},	//TX CL ofs_q
		{ 128,  192},	//TX CL ofs_i
		{4096, 4096},	//RX IMB param_1
		{   0,    0},	//RX IMB param_0
		{-384, -736},	//RX DC ofs_q
		{ 512,  192},	//RX DC ofs_i
	},
	[4] = { /* Board #5 */
		{4096, 4096},	//TX SSB param_1
		{   0,    0},	//TX SSB param_0
		{-512, -256},	//TX CL ofs_q
		{ 256,  256},	//TX CL ofs_i
		{4096, 4096},	//RX IMB param_1
		{   0,    0},	//RX IMB param_0
		{-288, -352},	//RX DC ofs_q
		{ 160,   64},	//RX DC ofs_i
	},
	[5] = { /* Board #6 */
		{4096, 4096},	//TX SSB param_1
		{   0,    0},	//TX SSB param_0
		{-528, -112},	//TX CL ofs_q
		{   0, -384},	//TX CL ofs_i
		{4096, 4096},	//RX IMB param_1
		{   0,    0},	//RX IMB param_0
		{-448, -448},	//RX DC ofs_q
		{ 448,  -32},	//RX DC ofs_i
	},
	[6] = { /* Board #7 */
		{4096, 4096},	//TX SSB param_1
		{   0,    0},	//TX SSB param_0
		{-768, -192},	//TX CL ofs_q
		{ 512, -128},	//TX CL ofs_i
		{4096, 4096},	//RX IMB param_1
		{   0,    0},	//RX IMB param_0
		{-592, -336},	//RX DC ofs_q
		{ 192,  256},	//RX DC ofs_i
	},
};

//mango init rf
static
void mango_rf_spi_init(struct device *dev, u8 path)
{
	int i;
	for (i=0; i<13; i++)
	{
		//mango_write(dev, path, mode = 1(RF spi), addr, data);
		mango_write(dev, path, 0x1, i, mango_reg_default[i][path]);
	}
}


static int mango_rf_init(struct device *dev, u8 path)
{
	u8 clk_div_0 = 1;
	u8 clk_div_1 = 1;
	assert_spi(dev);
#ifdef USE_CSPI
	/* Enable PHY-RF SPI I/F */
	if (path) {
		regw(0x0000C730, 0x0000000b); // B path, Mango AFE SPI reset
		regw(0x0000C730, 0x0000000a); // B path, Mango AFE SPI
		regw(0x0000C750, 0x0000002b); // B path, Mango RF reset
		regw(0x0000C750, 0x0000002a); // B path, Mango RF
	} else {
		regw(0x00008730, 0x0000000b); // A path, Mango AFE SPI reset
		regw(0x00008730, 0x0000000a); // A path, Mango AFE SPI
		regw(0x00008750, 0x0000002b); // A path, Mango RF SPI reset
		regw(0x00008750, 0x0000002a); // A path, Mango RF SPI
	}
#else
	if (path) {
		regw(0x0000C730, 0x80000008); // B path, Mango AFE SPI
		regw(0x0000C750, 0x80000008); // B path, Mango RF
		regw(0x0000C780, 0x00980801 | (clk_div_1 << 2)); // B path, Mango AFE SPI
		regw(0x0000C7B0, 0x00920801 | (clk_div_1 << 2)); // B path, Mango RF
		regw(0x0000C780, 0x00980802 | (clk_div_1 << 2)); // B path, Mango AFE SPI
		regw(0x0000C7B0, 0x00920802 | (clk_div_1 << 2)); // B path, Mango RF
	}
	else
	{
		regw(0x00008730, 0x80000008); // A path, Mango AFE SPI
		regw(0x00008750, 0x80000008); // A path, Mango RF SPI
		regw(0x00008780, 0x00980801 | (clk_div_0 << 2)); // A path, Mango AFE SPI
		regw(0x000087B0, 0x00920801 | (clk_div_1 << 2)); // A path, Mango RF SPI
		regw(0x00008780, 0x00980802 | (clk_div_0 << 2)); // A path, Mango AFE SPI
		regw(0x000087B0, 0x00920802 | (clk_div_1 << 2)); // A path, Mango RF SPI
	}
#endif

	/* Initialize Mango AFE */
	mango_afe_init(dev, path);
	mango_afe_config_DLL(dev, path,1,1,1,1);
	mango_afe_config_clocks(dev, path,1,0,1,0);
	mango_afe_config_filters(dev, path,1,1);
	mango_afe_set_TxDCO(dev, path,0,0);

	/* Enable Mango RF SPI I/F */
	mango_rf_spi_init(dev, path);

	return 0;
}

static int mango_rf_config(struct device *dev,
		reg_write wrfn, reg_read rdfn)
{
	struct mango_driver_data *drv = dev->driver_data;

	drv->writefn = wrfn;
	drv->readfn = rdfn;

	return 0;
}

static int mango_rf_set_channel(struct device *dev,
		u8 path, u32 freq, u8 bw)
{
	mango_channel_config_freq(dev, path, freq);
	mango_tx_bandwidth_config(dev, path, bw);
	mango_rx_bandwidth_config(dev, path, bw);

	return 0;
}

static int mango_rf_power_cntrl(struct device *dev, u8 flag)
{
	if(flag == 0) {
		regw(0x8010, 0x00230000);
		//everything powered down
		mango_write(dev, 0, 0x0, 0x60, 0x7f);
	}
	else {
		regw(0x8010, 0x002c0000);
		udelay(50);

		//DLL off, everything else on
		mango_write(dev, 0, 0x0, 0x60, 0x80);

		// added for DLL Lock
		mango_write(dev, 0, 0x0, 0x75, 0x08);	// DLL Reset
		udelay(1);
		mango_write(dev, 0, 0x0, 0x75, 0x00);	// DLL Reconfig
	}

	return 0;
}

int mango_rf_get_cal_data(struct device *dev, u8 bd_no, struct rf_cal_data *data)
{
	assert(bd_no > 0 && bd_no <= ARRAY_SIZE(mango_board_cal));

	memcpy(data, &mango_board_cal[bd_no - 1], sizeof(*data));
	return 0;
}

static int mango_rf_probe(struct device *dev)
{
	dev->driver_data = &mango_drv_data;

	return 0;
}

struct rf_ops mango_rf_ops = {
	.config = mango_rf_config,
	.init = mango_rf_init,
	.set_channel = mango_rf_set_channel,
	.get_cal_data = mango_rf_get_cal_data,
	.rf_control = mango_rf_power_cntrl
};

static declare_driver(rf) = {
	.name = "rf",
	.probe = mango_rf_probe,
	.ops = &mango_rf_ops,
};

#ifdef CONFIG_RF_CLI
#include <string.h>
#include <cli.h>

//mango chan path band index
static
void mango_channel_config_index(struct device *dev, u8 path, int band, int index)
/*
 * band = 0: 2.4G, 1: 5G
 */
{
	if(band == 0)
	{
		mango_write(dev, path, 0x1, 0x5, (mango_reg_default[0x5][path] & ~(0x41)) | 0x0);
		mango_write(dev, path, 0x1, 0x3, mango_fpga_pll_24GHz_reg3[index-1]);
		mango_write(dev, path, 0x1, 0x4, mango_fpga_pll_24GHz_reg4[index-1]);
	}
	else
	{
		if(index <= 8)
			mango_write(dev, path, 0x1, 0x5, (mango_reg_default[0x5][path] & ~(0x41)) | 0x1);
		else
			mango_write(dev, path, 0x1, 0x5, (mango_reg_default[0x5][path] & ~(0x41)) | 0x41);
		mango_write(dev, path, 0x1, 0x3, mango_fpga_pll_50GHz_reg3[index-1]);
		mango_write(dev, path, 0x1, 0x4, mango_fpga_pll_50GHz_reg4[index-1]);
	}
}

//mango rf tgm path gain_index
//TX Manual(SPI) Gain setting
static
void mango_tx_gain_manual_config(struct device *dev, u8 path, int gain_index)
{
	u32 temp_reg;
	temp_reg = (mango_reg_default[0xc][path] & ~(0x3f)) | (gain_index & 0x3f);
	mango_reg_default[0xc][path] = temp_reg;
	mango_write(dev, path, 0x1, 0xc, mango_reg_default[0xc][path]);
}

//mango rf tgcfg path mode
//TX Gain setting config
static
void mango_tx_gain_mode_config(struct device *dev, u8 path, int mode)
// mode : 	0, HW control
// 		1, SPI control
{
	u32 temp_reg;
	temp_reg = (mango_reg_default[0x9][path] & ~(0x1 << 10)) | (mode << 10);
	mango_reg_default[0x9][path] = temp_reg;
	mango_write(dev, path, 0x1, 0x9, mango_reg_default[0x9][path]);
}

//mango rf rgm path lna_gain_index vga_gain_index
//RX Manual(SPI) Gain setting
static
void mango_rx_gain_manual_config(struct device *dev, u8 path,
		int lna_gain_index, int vga_gain_index)
{
	u32 temp_reg;
	temp_reg = (mango_reg_default[0xb][path] & ~(0x7f)) | (((lna_gain_index & 0x3) << 5) | (vga_gain_index & 0x1f));
	mango_reg_default[0xb][path] = temp_reg;
	mango_write(dev, path, 0x1, 0xb, mango_reg_default[0xb][path]);
}

//mango rf rgcfg path mode
//RX Gain setting config
static
void mango_rx_gain_mode_config(struct device *dev, u8 path, int mode)
// mode : 	0, HW control
// 		1, SPI control
{
	u32 temp_reg;
	temp_reg = (mango_reg_default[0x8][path] & ~(0x1 << 12)) | (mode << 12);
	mango_reg_default[0x8][path] = temp_reg;
	mango_write(dev, path, 0x1, 0x8, mango_reg_default[0x8][path]);
}

static
u8 mango_afe_config_power(struct device *dev, u8 path, u8 pwrState)
/*
pwrState : 0, everything powered down
pwrState : 1, DLL off, everything else on
*/
{
	u32 regVal;

	/* AD9963 reg 0x60:
		7: 0=power down DLL
		[6:0] 1=power down [DAC refrerence, IDAC, QDAC, clock input, ADC reference, QADC, IADC]
	*/

	//This function intentionally powers off the DLL, whether the user asks for all-on or all-off
	// This seemed like the safest approach, since the DLL requries an explicit reset whenever its config
	// changes or when it is powered up. This reset is done in the mango_afe_config_DLL() function in the correct
	// order to bring the DLL up in a good state.
#if 0 /* old version. not work*/
	if(pwrState == 0) regVal = 0x3F; //everything powered down
	else if(pwrState == 1) regVal = 0x00; //DLL off, everything else on
	else return 0xff;
	mango_write(dev, path, 0x0, 0x60, regVal);
#else
	if(pwrState == 0) {
		regw(0x8010, 0x00230000);
		//regVal = 0x3F; //everything powered down
		regVal = 0x7F;
		mango_write(dev, path, 0x0, 0x60, regVal);
	}
	else if(pwrState == 1) {
		regw(0x8010, 0x002c0000);
		udelay(50);
		//regVal = 0x00; //DLL off, everything else on
		regVal = 0x80;
		mango_write(dev, path, 0x0, 0x60, regVal);

		// added for DLL Lock
		mango_write(dev, path, 0x0, 0x75, 0x08);	// DLL Reset
		udelay(1);
		mango_write(dev, path, 0x0, 0x75, 0x00);	// DLL Reconfig
	}
	else return 0xff;
#endif
	return 0;
}

static
u8 mango_afe_set_TxGain1(struct device *dev, u8 path, u8 iqSel, u8 gain)
/**
\brief Sets the GAIN1 value (linear-in-dB adjustment +/- 6dB) for the selected path (I or Q) in the selected AD9963s.
Changing this gain value also changes the common mode voltage and DC offset of the selected path. We recommend leaving
this gain setting unchanged for optimal performance.
\param baseaddr Base memory address of w3_ad_controller pcore
\param csMask OR'd combination of RFA_AD_CS and RFB_AD_CS
\param iqSel Select I or Q path; must be AD_CHAN_I(0) or AD_CHAN_Q(1)
\param gain 6-bit gain value; [0:25] = [0:+6dB], [41,63] = [-6dB:0dB]
*/
{
	//6-bit Linear-in-dB gain +/- 6dB

	//Sanity check inputs
	if(gain>63)
		return 0xff;

	if(iqSel == 0) {
		mango_write(dev, path, 0x0, 0x68, (gain&0x3F)); //IGAIN1
	}
	else if(iqSel == 1) {
		mango_write(dev, path, 0x0, 0x6B, (gain&0x3F)); //QGAIN1
	}

	return 0;
}

static
u8 mango_afe_set_TxGain2(struct device *dev, u8 path, u8 iqSel, u8 gain)
/**
\brief Sets the GAIN2 value (linear adjustment +/- 2.5%) for the selected path (I or Q) in the selected AD9963s
Changing this gain value also changes the common mode voltage and DC offset of the selected path. We recommend leaving
this gain setting unchanged for optimal performance.
\param baseaddr Base memory address of w3_ad_controller pcore
\param csMask OR'd combination of RFA_AD_CS and RFB_AD_CS
\param iqSel Select I or Q path; must be AD_CHAN_I(0) or AD_CHAN_Q(1)
\param gain 6-bit gain value; [0:25] = [0:+2.5%], [41,63] = [-2.5%:0]
*/
{
//6-bit Linear gain +/- 2.5%
	if(iqSel == 0) {
		mango_write(dev, path, 0x0, 0x69, (gain&0x3F)); //IGAIN2
	}
	else if(iqSel == 1) {
		mango_write(dev, path, 0x0, 0x6C, (gain&0x3F)); //QGAIN2
	}

	return 0;
}

static inline int strtoul10(const char *s)
{
	return strtoul(s, NULL, 10);
}

static inline int strtoul16(const char *s)
{
	return strtoul(s, NULL, 16);
}

static int do_mango(int argc, char *argv[])
{
	struct device *rf = device_get_by_name("rf");

	if (!rf)
		return CMD_RET_FAILURE;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (!strcmp("init", argv[1])) {
		if (argc < 3)
			return CMD_RET_USAGE;
		if (!strcmp("rf", argv[2])) {
			mango_rf_spi_init(rf, 0);
			mango_rf_spi_init(rf, 1);
		} else if (!strcmp("afe", argv[2])) {
			mango_afe_init(rf, 0);
			mango_afe_init(rf, 1);
			return CMD_RET_FAILURE;
		} else
			return CMD_RET_FAILURE;
	} else if (!strcmp("write", argv[1])) {
		if (argc < 6)
			return CMD_RET_USAGE;
		mango_write(rf, (u8)strtoul10(argv[2]), (u8)strtoul10(argv[3]),
				strtoul16(argv[4]), strtoul16(argv[5]));
	} else if (!strcmp("read", argv[1])) {
		u32 value;
		if (argc < 4)
			return CMD_RET_USAGE;
		value = mango_read(rf, (u8)strtoul10(argv[2]), strtoul16(argv[3]));
		printf("[0x%x] 0x%08x\n", strtoul16(argv[3]), value);
	} else if (!strcmp("chan", argv[1])) {
		if (argc < 5)
			return CMD_RET_USAGE;

		mango_channel_config_index(rf, (u8)strtoul10(argv[2]),
				(int)strtoul10(argv[3]),
				(int)strtoul10(argv[4]));
	} else if (!strcmp("chan_freq", argv[1])) {
		if (argc < 4)
			return CMD_RET_USAGE;

		mango_channel_config_freq(rf, (u8)strtoul10(argv[2]),
				(u32)strtoul10(argv[3]));
	} else if (!strcmp("rf", argv[1])) {
		if (!strcmp("tbw", argv[2])) {
			if (argc < 5)
				return CMD_RET_USAGE;
			mango_tx_bandwidth_config(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]));
		}else if (!strcmp("dbg", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			_rf_dbg = (u8)strtoul10(argv[3]);
		} else if (!strcmp("rbw", argv[2])) {
			if (argc < 5)
				return CMD_RET_USAGE;
			mango_rx_bandwidth_config(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]));
		} else if (!strcmp("tgcfg", argv[2])) {
			if (argc < 5)
				return CMD_RET_USAGE;
			mango_tx_gain_mode_config(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]));
		} else if (!strcmp("tgm", argv[2])) {
			if (argc < 5)
				return CMD_RET_USAGE;
			mango_tx_gain_manual_config(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]));
		} else if (!strcmp("rgcfg", argv[2])) {
			if (argc < 5)
				return CMD_RET_USAGE;
			mango_rx_gain_mode_config(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]));
		} else if (!strcmp("rgm", argv[2])) {
			if (argc < 6)
				return CMD_RET_USAGE;
			mango_rx_gain_manual_config(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]),
					(int)strtoul10(argv[5]));
		}
	} else if (!strcmp("afe", argv[1])) {
		if (!strcmp("txdco", argv[2])) {
			if (argc < 6)
				return CMD_RET_USAGE;
			mango_afe_set_TxDCO(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]),
					(int)strtoul10(argv[5]));
		}else if (!strcmp("dbg", argv[2])) {
			if (argc < 4)
				return CMD_RET_USAGE;
			_afe_dbg = (u8)strtoul10(argv[3]);
		} else if (!strcmp("txgain1", argv[2])) {
			if (argc < 6)
				return CMD_RET_USAGE;
			mango_afe_set_TxGain1(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]),
					(int)strtoul10(argv[5]));
		} else if (!strcmp("txgain2", argv[2])) {
			if (argc < 6)
				return CMD_RET_USAGE;
			mango_afe_set_TxGain2(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]),
					(int)strtoul10(argv[5]));
		} else if (!strcmp("filter", argv[2])) {
			if (argc < 6)
				return CMD_RET_USAGE;
			mango_afe_config_filters(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]),
					(int)strtoul10(argv[5]));
		} else if (!strcmp("clock", argv[2])) {
			if (argc < 8)
				return CMD_RET_USAGE;
			mango_afe_config_clocks(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]),
					(int)strtoul10(argv[5]),
					(int)strtoul10(argv[6]),
					(int)strtoul10(argv[7]));
		} else if (!strcmp("dll", argv[2])) {
			if (argc < 8)
				return CMD_RET_USAGE;
			mango_afe_config_DLL(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]),
					(int)strtoul10(argv[5]),
					(int)strtoul10(argv[6]),
					(int)strtoul10(argv[7]));
		} else if (!strcmp("power", argv[2])) {
			if (argc < 5)
				return CMD_RET_USAGE;
			mango_afe_config_power(rf, (u8)strtoul10(argv[3]),
					(int)strtoul10(argv[4]));
		}
	} else
		return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

CMD(mango, do_mango,
	"CLI commands for Mango b/d",
	"mango init (rf / afe)" OR
	"mango write path mode addr data" OR
	"mango read path addr" OR
	"mango chan path band index" OR
	"mango chan_freq path freq" OR
	"mango rf tbw path bw" OR
	"mango rf rbw path bw" OR
	"mango rf tgcfg path mode" OR
	"mango rf tgm path gain_index" OR
	"mango rf rgcfg path mode" OR
	"mango rf rgm path lna_gain_index vga_gain_index" OR
	"mango rf dbg [0 or 1]" OR
	"mango afe txdco path iqSel dco" OR
	"mango afe txgain1 path iqSel gain" OR
	"mango afe txgain2 path iqSel gain" OR
	"mango afe filter path interpRate decimationRate" OR
	"mango afe clock path DAC_clkSrc ADC_clkSrc ADC_clkDiv ADC_DCS" OR
	"mango afe dll path DLL_En DLL_M DLL_N DLL_DIV" OR
	"mango afe dbg [0 or 1]" OR
	"mango afe power path pwrState"
);
#endif

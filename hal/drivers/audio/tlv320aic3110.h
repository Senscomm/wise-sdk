/*
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 * Copyright (c) 2019 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TLV320AIC3110_H__

#ifdef __cplusplus
extern "C" {
#endif

#undef BIT
#define BIT(n)	(1 << (n))

#undef BIT_MASK
#define BIT_MASK(n)	(BIT(n) - 1)

/* Register addresses */
#define PAGE_CONTROL_ADDR	0

/* Register addresses {page, address} and fields */
#define SOFT_RESET_ADDR		(struct reg_addr){0, 1}
#define SOFT_RESET_ASSERT	(1)

#define CLKGEN_MUX_ADDR		(struct reg_addr){0, 4}
#define CLKGEN_PLL_MASK		BIT_MASK(2)
#define CLKGEN_CODEC_MASK	BIT_MASK(2)
#define CLKGEN_PLL_MCLK		0
#define CLKGEN_PLL_BCLK		1
#define CLKGEN_PLL_GPIO1	2
#define CLKGEN_PLL_DIN		3
#define CLKGEN_CODEC_MCLK	0
#define CLKGEN_CODEC_BCLK	1
#define CLKGEN_CODEC_GPIO1	2
#define CLKGEN_CODEC_PLL	3
#define CLKGEN(pll, codec)	((((pll) & CLKGEN_PLL_MASK) << 2) \
							| ((codec) & CLKGEN_CODEC_MASK))

#define PLL_PR_ADDR			(struct reg_addr){0, 5}
#define PLL_PWRUP           BIT(7)
#define PLL_P_MASK			BIT_MASK(3)
#define PLL_R_MASK			BIT_MASK(3)
#define PLL_PR(p, r)		((PLL_PWRUP) \
                            | (((p) & PLL_P_MASK) << 4) \
							| ((r) & PLL_R_MASK))

#define PLL_J_ADDR			(struct reg_addr){0, 6}
#define PLL_J_MASK			BIT_MASK(6)
#define PLL_J(j)			((j) & PLL_J_MASK)

#define PLL_D_MSB_ADDR		(struct reg_addr){0, 7}
#define PLL_D_MSB_MASK		BIT_MASK(6)
#define PLL_D_MSB(d)		((d >> 8) & PLL_D_MSB_MASK)

#define PLL_D_LSB_ADDR		(struct reg_addr){0, 8}
#define PLL_D_LSB_MASK		BIT_MASK(8)
#define PLL_D_LSB(d)		((d) & PLL_D_LSB_MASK)

#define NDAC_DIV_ADDR		(struct reg_addr){0, 11}
#define NDAC_POWER_UP		BIT(7)
#define NDAC_POWER_UP_MASK	BIT(7)
#define NDAC_DIV_MASK		BIT_MASK(7)
#define NDAC_DIV(val)		((val) & NDAC_DIV_MASK)

#define MDAC_DIV_ADDR		(struct reg_addr){0, 12}
#define MDAC_POWER_UP		BIT(7)
#define MDAC_POWER_UP_MASK	BIT(7)
#define MDAC_DIV_MASK		BIT_MASK(7)
#define MDAC_DIV(val)		((val) & MDAC_DIV_MASK)

#define DAC_CLK_FREQ_MAX	49152000	/* 49.152 MHz */

#define DOSR_MSB_ADDR		(struct reg_addr){0, 13}
#define DOSR_MSB_MASK		BIT_MASK(2)

#define DOSR_LSB_ADDR		(struct reg_addr){0, 14}
#define DOSR_LSB_MASK		BIT_MASK(8)

#define DAC_MOD_CLK_FREQ_MAX	6758000 /* 6.758 MHz */

#define NADC_DIV_ADDR		(struct reg_addr){0, 18}
#define NADC_POWER_UP		BIT(7)
#define NADC_POWER_UP_MASK	BIT(7)
#define NADC_DIV_MASK		BIT_MASK(7)
#define NADC_DIV(val)		((val) & NADC_DIV_MASK)

#define MADC_DIV_ADDR		(struct reg_addr){0, 19}
#define MADC_POWER_UP		BIT(7)
#define MADC_POWER_UP_MASK	BIT(7)
#define MADC_DIV_MASK		BIT_MASK(7)
#define MADC_DIV(val)		((val) & MADC_DIV_MASK)

#define ADC_CLK_FREQ_MAX	49152000	/* 49.152 MHz */

#define AOSR_ADDR			(struct reg_addr){0, 20}
#define AOSR_MASK			BIT_MASK(8)

#define ADC_MOD_CLK_FREQ_MAX	6758000 /* 6.758 MHz */

#define IF_CTRL1_ADDR		(struct reg_addr){0, 27}
#define IF_CTRL_IFTYPE_MASK	BIT_MASK(2)
#define IF_CTRL_IFTYPE_I2S	0
#define IF_CTRL_IFTYPE_DSP	1
#define IF_CTRL_IFTYPE_RJF	2
#define IF_CTRL_IFTYPE_LJF	3
#define IF_CTRL_IFTYPE(val)	(((val) & IF_CTRL_IFTYPE_MASK) << 6)
#define IF_CTRL_WLEN_MASK	BIT_MASK(2)
#define IF_CTRL_WLEN(val)	(((val) & IF_CTRL_WLEN_MASK) << 4)
#define IF_CTRL_WLEN_16		0
#define IF_CTRL_WLEN_20		1
#define IF_CTRL_WLEN_24		2
#define IF_CTRL_WLEN_32		3
#define IF_CTRL_BCLK_OUT	BIT(3)
#define IF_CTRL_WCLK_OUT	BIT(2)

#define IF_CTRL2_ADDR		(struct reg_addr){0, 29}
#define IF_CTRL2_DINLOOP	BIT(5)
#define IF_CTRL2_ADCLOOP	BIT(4)
#define IF_CTRL2_BCLKINV	BIT(3)
#define IF_CTRL2_ACTIVE		BIT(2)
#define IF_CTRL2_BDIV_CLKIN_MASK	BIT_MASK(2)
#define IF_CTRL2_BDIV_CLKIN_DAC_CLK		0
#define IF_CTRL2_BDIV_CLKIN_DAC_MOD_CLK	1
#define IF_CTRL2_BDIV_CLKIN_ADC_CLK		2
#define IF_CTRL2_BDIV_CLKIN_ADC_MOD_CLK	3
#define IF_CTRL2_BDIV_CLKIN(val)	((val) & IF_CTRL2_BDIV_CLKIN_MASK)

#define BCLK_DIV_ADDR		(struct reg_addr){0, 30}
#define BCLK_DIV_POWER_UP	BIT(7)
#define BCLK_DIV_POWER_UP_MASK	BIT(7)
#define BCLK_DIV_MASK		BIT_MASK(7)
#define BCLK_DIV(val)		((val) & MDAC_DIV_MASK)

#define OVF_FLAG_ADDR		(struct reg_addr){0, 39}

#define DAC_PBLK_SEL_ADDR	(struct reg_addr){0, 60}
#define	DAC_PBLK_SEL_MASK	BIT_MASK(5)
#define	DAC_PBLK_SEL(val)	((val) & DAC_PBLK_SEL_MASK)

#define DAC_PATH_SETUP_ADDR		(struct reg_addr){0, 63}
#define DAC_LR_POWERUP_DEFAULT	(BIT(7) | BIT(6) | BIT(4) | BIT(2))
#define DAC_LR_POWERDN_DEFAULT	(BIT(4) | BIT(2))

#define DAC_VOL_CTRL_ADDR		(struct reg_addr){0, 64}
#define DAC_VOL_CTRL_UNMUTE_DEFAULT	(0)
#define DAC_VOL_CTRL_MUTE_DEFAULT	(BIT(3) | BIT(2))

#define ADC_PBLK_SEL_ADDR	(struct reg_addr){0, 61}
#define	ADC_PBLK_SEL_MASK	BIT_MASK(5)
#define	ADC_PBLK_SEL(val)	((val) & ADC_PBLK_SEL_MASK)

#define L_DIG_VOL_CTRL_ADDR	(struct reg_addr){0, 65}
#define R_DIG_VOL_CTRL_ADDR	(struct reg_addr){0, 66}
#define DRC_CTRL1_ADDR		(struct reg_addr){0, 68}
#define L_BEEP_GEN_ADDR		(struct reg_addr){0, 71}
#define BEEP_GEN_EN_BEEP	(BIT(7))
#define R_BEEP_GEN_ADDR		(struct reg_addr){0, 72}
#define BEEP_LEN_MSB_ADDR	(struct reg_addr){0, 73}
#define BEEP_LEN_MIB_ADDR	(struct reg_addr){0, 74}
#define BEEP_LEN_LSB_ADDR	(struct reg_addr){0, 75}

#define ADC_DMIC_CTRL_ADDR	(struct reg_addr){0, 81}
#define ADC_CH_POWERDN_DEFAULT	(0)
#define ADC_CH_POWERUP_DEFAULT	(BIT(7))

#define ADC_VOL_F_CTRL_ADDR	(struct reg_addr){0, 82}
#define ADC_VOL_CTRL_UNMUTE_DEFAULT	(0)
#define ADC_VOL_CTRL_MUTE_DEFAULT	BIT(7)

#define ADC_VOL_C_CTRL_ADDR	(struct reg_addr){0, 83}
#define	ADC_VOL_C_SEL_MASK	BIT_MASK(7)
#define ADC_VOL_C_ADJ(db)	(db * 2)
#define	ADC_VOL_C_SEL(val)	((val) & ADC_VOL_C_SEL_MASK)


/* Page 1 registers */
#define HEADPHONE_DRV_ADDR	(struct reg_addr){1, 31}
#define HEADPHONE_DRV_POWERUP	(BIT(7) | BIT(6))
#define HEADPHONE_DRV_CM_MASK	(BIT_MASK(2) << 3)
#define HEADPHONE_DRV_CM(val)	(((val) << 3) & HEADPHONE_DRV_CM_MASK)
#define HEADPHONE_DRV_RESERVED	(BIT(2))

#define HP_OUT_POP_RM_ADDR	(struct reg_addr){1, 33}
#define HP_OUT_POP_RM_ENABLE	(BIT(7))

#define OUTPUT_ROUTING_ADDR	(struct reg_addr){1, 35}
#define OUTPUT_ROUTING_HPL	(2 << 6)
#define OUTPUT_ROUTING_HPR	(2 << 2)

#define HPL_ANA_VOL_CTRL_ADDR	(struct reg_addr){1, 36}
#define HPR_ANA_VOL_CTRL_ADDR	(struct reg_addr){1, 37}
#define HPX_ANA_VOL_ENABLE	(BIT(7))
#define HPX_ANA_VOL_MASK	(BIT_MASK(7))
#define HPX_ANA_VOL(val)	(((val) & HPX_ANA_VOL_MASK) |	\
		HPX_ANA_VOL_ENABLE)
#define HPX_ANA_VOL_MAX		(0)
#define HPX_ANA_VOL_DEFAULT	(64)
#define HPX_ANA_VOL_MIN		(127)
#define HPX_ANA_VOL_MUTE	(HPX_ANA_VOL_MIN | ~HPX_ANA_VOL_ENABLE)
#define HPX_ANA_VOL_LOW_THRESH	(105)
#define HPX_ANA_VOL_FLOOR	(144)

#define HPL_DRV_GAIN_CTRL_ADDR	(struct reg_addr){1, 40}
#define HPR_DRV_GAIN_CTRL_ADDR	(struct reg_addr){1, 41}
#define	HPX_DRV_UNMUTE		(BIT(2))

#define HEADPHONE_DRV_CTRL_ADDR	(struct reg_addr){1, 44}
#define HEADPHONE_DRV_LINEOUT	(BIT(1) | BIT(2))

#define MICBIAS_OUTPUT_PWR_DOWN 0
#define MICBIAS_OUTPUT_2V       1
#define MICBIAS_OUTPUT_2_5V     2
#define MICBIAS_OUTPUT_AVDD     3
#define MICBIAS_CTRL_ADDR		(struct reg_addr){1, 46}
#define MICBIAS_PWRUP_WO_HSDET	BIT(3)
#define MICBIAS_OUTPUT_PWR_MASK	BIT_MASK(2)
#define MICBIAS_OUTPUT_PWR(val) ((val) & MICBIAS_OUTPUT_PWR_MASK)

#define MIC_PGA_CTRL_ADDR		(struct reg_addr){1, 47}
#define MIC_PGA_ENABLE			BIT(7)
#define MIC_PGA_MASK			BIT_MASK(7)
#define MIC_PGA_GAIN(val)	    (((val) & MIC_PGA_MASK) |\
                                MIC_PGA_ENABLE)

#define MIC_PGA_INPUT_NONE	0
#define MIC_PGA_INPUT_10K	1
#define MIC_PGA_INPUT_20K	2
#define MIC_PGA_INPUT_40K	3

#define MIC_PGA_IN_PSEL_ADDR	(struct reg_addr){1, 48}
#define MIC_PGA_IN_LP_MASK		(BIT_MASK(2) << 6)
#define MIC_PGA_IN_RP_MASK		(BIT_MASK(2) << 4)
#define MIC_PGA_IN_LM_MASK		(BIT_MASK(2) << 2)
#define MIC_PGA_INPUT_P(lp, rp, lm)	\
								((((lp) << 6) & MIC_PGA_IN_LP_MASK) |\
								 (((rp) << 4) & MIC_PGA_IN_RP_MASK) |\
								 (((lm) << 2) & MIC_PGA_IN_LM_MASK))

#define MIC_PGA_IN_MSEL_ADDR	(struct reg_addr){1, 49}
#define MIC_PGA_IN_CM_MASK		(BIT_MASK(2) << 6)
#define MIC_PGA_IN_RM_MASK		(BIT_MASK(2) << 4)
#define MIC_PGA_INPUT_M(cm, lp)	\
								((((cm) << 6) & MIC_PGA_IN_CM_MASK) |\
								 (((lp) << 4) & MIC_PGA_IN_RM_MASK))

/* Page 3 registers */
#define TIMER_MCLK_DIV_ADDR	(struct reg_addr){3, 16}
#define TIMER_MCLK_DIV_EN_EXT	(BIT(7))
#define TIMER_MCLK_DIV_MASK	(BIT_MASK(7))
#define TIMER_MCLK_DIV_VAL(val)	((val) & TIMER_MCLK_DIV_MASK)

struct reg_addr {
	uint8_t page; 		/* page number */
	uint8_t reg_addr; 		/* register address */
};

enum proc_block {
	/* highest performance class with each decimation filter */
	PRB_P11_DECIMATION_A = 11,
	PRB_P10_DECIMATION_B = 10,
	PRB_P18_DECIMATION_C = 18,
	PRB_R4_DECIMATION_A  =  4,
	PRB_R12_DECIMATION_B = 12,
	PRB_R18_DECIMATION_C = 18,
};

enum cm_voltage {
	CM_VOLTAGE_1P35 = 0,
	CM_VOLTAGE_1P5 = 1,
	CM_VOLTAGE_1P65 = 2,
	CM_VOLTAGE_1P8 = 3,
};

#ifdef __cplusplus
}
#endif

#endif /* __TLV320AIC3110_H__ */

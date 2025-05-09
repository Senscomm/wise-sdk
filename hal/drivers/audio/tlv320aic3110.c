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
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <errno.h>
#include <hal/init.h>
#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/i2s.h>
#include <hal/i2c.h>
#include <hal/kmem.h>
#include <hal/codec.h>

#include <cmsis_os.h>

#include "tlv320aic3110.h"

#define ERROR
#define DEBUG

#ifdef ERROR
#define LOG_ERR(...)					printk(__VA_ARGS__)
#else
#define LOG_ERR(...)					(void)0
#endif

#define CODEC_OUTPUT_ANA_VOLUME_MAX		0
#define CODEC_OUTPUT_ANA_VOLUME_MIN		(-78 * 2)

#define CODEC_OUTPUT_DIG_VOLUME_MAX		(24)
#define CODEC_OUTPUT_DIG_VOLUME_MIN		(-63.5)

struct codec_driver_data {
	struct device *dev;
	struct device *bus; /* I2C device */
	u16 addr; /* I2C address */
	osSemaphoreId_t sem; /* I2C completion */
	struct reg_addr	reg_addr_cache;
	struct fops devfs_ops;
};

static void codec_write_reg(struct device *dev, struct reg_addr reg,
			    uint8_t val);
static void codec_read_reg(struct device *dev, struct reg_addr reg,
			   uint8_t *val);
static void codec_soft_reset(struct device *dev);
static int codec_configure_dai(struct device *dev, audio_dai_cfg_t *cfg);
static int codec_configure_clocks(struct device *dev,
				  struct audio_codec_cfg *cfg);
static int codec_configure_filters(struct device *dev,
				   audio_dai_cfg_t *cfg);
static void codec_configure_output(struct device *dev);
static void codec_configure_input(struct device *dev);
static int codec_set_output_dig_volume(struct device *dev, float vol);
static int codec_get_output_dig_volume(struct device *dev, float *min,
        float *cur, float *max);

#define DUMP_ALL_REGS

#ifdef DUMP_ALL_REGS
static void codec_read_all_regs(struct device *dev);
#define CODEC_DUMP_REGS(dev)	codec_read_all_regs((dev))
#else
#define CODEC_DUMP_REGS(dev)
#endif

static bool codec_is_bclk_out(audio_dai_cfg_t *cfg)
{
	struct i2s_config *i2s = &cfg->i2s;

    /* XXX: can't check I2S_OPT_BIT_CLK_MASTER because it's zero. */
    return ((i2s->options & I2S_OPT_BIT_CLK_SLAVE) == 0);
}

static bool codec_is_wclk_out(audio_dai_cfg_t *cfg)
{
	struct i2s_config *i2s = &cfg->i2s;

    /* XXX: can't check I2S_OPT_FRAME_CLK_MASTER because it's zero. */
    return ((i2s->options & I2S_OPT_FRAME_CLK_SLAVE) == 0);
}

static int codec_configure(struct device *dev,
			   struct audio_codec_cfg *cfg)
{
	int ret;

	if (cfg->dai_type != AUDIO_DAI_TYPE_I2S) {
		LOG_ERR("dai_type must be AUDIO_DAI_TYPE_I2S");
		return -EINVAL;
	}

	codec_soft_reset(dev);

	ret = codec_configure_clocks(dev, cfg);
	if (ret == 0) {
		ret = codec_configure_dai(dev, &cfg->dai_cfg);
	}
	if (ret == 0) {
		ret = codec_configure_filters(dev, &cfg->dai_cfg);
	}
	if (ret == 0) {
	    codec_configure_output(dev);
    }
	if (ret == 0) {
	    codec_configure_input(dev);
    }

	return ret;
}

static int codec_start_output(struct device *dev)
{
	/* powerup DAC channels */
	codec_write_reg(dev, DAC_PATH_SETUP_ADDR, DAC_LR_POWERUP_DEFAULT);

	/* unmute DAC channels */
	codec_write_reg(dev, DAC_VOL_CTRL_ADDR, DAC_VOL_CTRL_UNMUTE_DEFAULT);

	CODEC_DUMP_REGS(dev);

	return 0;
}

static int codec_stop_output(struct device *dev)
{
	/* mute DAC channels */
	codec_write_reg(dev, DAC_VOL_CTRL_ADDR, DAC_VOL_CTRL_MUTE_DEFAULT);

	/* powerdown DAC channels */
	codec_write_reg(dev, DAC_PATH_SETUP_ADDR, DAC_LR_POWERDN_DEFAULT);

	return 0;
}

static void codec_mute_output(struct device *dev)
{
	/* mute DAC channels */
	codec_write_reg(dev, DAC_VOL_CTRL_ADDR, DAC_VOL_CTRL_MUTE_DEFAULT);
}

static void codec_unmute_output(struct device *dev)
{
	/* unmute DAC channels */
	codec_write_reg(dev, DAC_VOL_CTRL_ADDR, DAC_VOL_CTRL_UNMUTE_DEFAULT);
}

static int codec_start_input(struct device *dev)
{
	/* power up ADC channels */
	codec_write_reg(dev, ADC_DMIC_CTRL_ADDR, ADC_CH_POWERUP_DEFAULT);

	/* unmute ADC channels */
	codec_write_reg(dev, ADC_VOL_F_CTRL_ADDR, ADC_VOL_CTRL_UNMUTE_DEFAULT);

	CODEC_DUMP_REGS(dev);

	return 0;
}

static int codec_stop_input(struct device *dev)
{
	/* mute ADC channels */
	codec_write_reg(dev, ADC_VOL_F_CTRL_ADDR, ADC_VOL_CTRL_MUTE_DEFAULT);

	/* powerdown ADC channels */
	codec_write_reg(dev, ADC_DMIC_CTRL_ADDR, ADC_CH_POWERDN_DEFAULT);

	return 0;
}

static void codec_mute_input(struct device *dev)
{
	/* mute ADC channels */
	codec_write_reg(dev, ADC_VOL_F_CTRL_ADDR, ADC_VOL_CTRL_MUTE_DEFAULT);

}

static void codec_unmute_input(struct device *dev)
{
	/* unmute ADC channels */
	codec_write_reg(dev, ADC_VOL_F_CTRL_ADDR, ADC_VOL_CTRL_UNMUTE_DEFAULT);
}

static int codec_set_property(struct device *dev,
			      audio_property_t property,
			      audio_channel_t channel,
			      audio_property_value_t val)
{
	/* individual channel control not currently supported */
	if (channel != AUDIO_CHANNEL_ALL) {
		LOG_ERR("channel %u invalid. must be AUDIO_CHANNEL_ALL",
			channel);
		return -EINVAL;
	}

	switch (property) {
	case AUDIO_PROPERTY_OUTPUT_VOLUME:
		return codec_set_output_dig_volume(dev, val.vol.cur);

	case AUDIO_PROPERTY_OUTPUT_MUTE:
		if (val.mute) {
			codec_mute_output(dev);
		} else {
			codec_unmute_output(dev);
		}
		return 0;
	case AUDIO_PROPERTY_INPUT_MUTE:
		if (val.mute) {
			codec_mute_input(dev);
		} else {
			codec_unmute_input(dev);
		}
		return 0;

	default:
		break;
	}

	return -EINVAL;
}

static int codec_apply_properties(struct device *dev)
{
	/* nothing to do because there is nothing cached */
	return 0;
}

static int codec_get_property(struct device *dev,
			      audio_property_t property,
			      audio_channel_t channel,
			      audio_property_value_t *val)
{
	/* individual channel control not currently supported */
	if (channel != AUDIO_CHANNEL_ALL) {
		LOG_ERR("channel %u invalid. must be AUDIO_CHANNEL_ALL",
			channel);
		return -EINVAL;
	}

	switch (property) {
	case AUDIO_PROPERTY_OUTPUT_VOLUME:
    {
        float min, cur, max;
		codec_get_output_dig_volume(dev, &min, &cur, &max);
        val->vol.min = min;
        val->vol.cur = cur;
        val->vol.max = max;
        return 0;
    }

	case AUDIO_PROPERTY_OUTPUT_MUTE:
    {
        u8 reg;
        codec_read_reg(dev, DAC_VOL_CTRL_ADDR, &reg);
        val->mute = ((reg & DAC_VOL_CTRL_MUTE_DEFAULT) == DAC_VOL_CTRL_MUTE_DEFAULT);
		return 0;
    }
	case AUDIO_PROPERTY_INPUT_MUTE:
    {
        u8 reg;
        codec_read_reg(dev, ADC_VOL_F_CTRL_ADDR, &reg);
        val->mute = ((reg & ADC_VOL_CTRL_MUTE_DEFAULT) == ADC_VOL_CTRL_MUTE_DEFAULT);
		return 0;
    }
	default:
		break;
	}

	return -EINVAL;
}

static int codec_i2c_write(struct codec_driver_data *dev_data, u8 *buf, u32 len)
{
	int ret;

	ret = i2c_master_tx(dev_data->bus, dev_data->addr, buf, len);
	if (ret < 0) {
		LOG_ERR("Error %d from i2c_master_tx.\n", ret);
		return -EIO;
	}

	ret = osSemaphoreAcquire(dev_data->sem, 100);
	if (ret != osOK) {
		i2c_reset(dev_data->bus);
		LOG_ERR("No I2C response.\n");
		return -EIO;
	}

	return 0;
}

static int codec_i2c_read(struct codec_driver_data *dev_data, u8 *buf, u32 len)
{
	int ret;

	ret = i2c_master_rx(dev_data->bus, dev_data->addr, buf, len);
	if (ret < 0) {
		LOG_ERR("Error %d from i2c_master_rx.\n", ret);
		return -EIO;
	}

	ret = osSemaphoreAcquire(dev_data->sem, 100);
	if (ret != osOK) {
		i2c_reset(dev_data->bus);
		LOG_ERR("No I2C response.\n");
		return -EIO;
	}

	return 0;
}

static void codec_set_page(struct codec_driver_data *dev_data, struct reg_addr *reg)
{
	int ret;
	if (dev_data->reg_addr_cache.page != reg->page) {
		uint8_t data[2];
	   	data[0] = 0;
	   	data[1] = reg->page;
		ret = codec_i2c_write(dev_data, data, sizeof(data));
		if (ret < 0) {
			return;
		}
		dev_data->reg_addr_cache.page = reg->page;
	}
}

static void codec_write_reg(struct device *dev, struct reg_addr reg,
			    uint8_t val)
{
	struct codec_driver_data *const dev_data = dev->driver_data;
	uint8_t data[2];
	int ret;

	/* set page */
	codec_set_page(dev_data, &reg);

	data[0] = reg.reg_addr;
	data[1] = val;
	ret = codec_i2c_write(dev_data, data, sizeof(data));
	if (ret < 0) {
		return;
	}

#ifdef DEBUG
	printk("WR PG:%u REG:%02u VAL:0x%02x\n",
			reg.page, reg.reg_addr, val);
#endif
}

static void codec_read_reg(struct device *dev, struct reg_addr reg,
			   uint8_t *val)
{
	struct codec_driver_data *const dev_data = dev->driver_data;
	int ret;

	/* set page */
	codec_set_page(dev_data, &reg);

	ret = codec_i2c_write(dev_data, &reg.reg_addr, 1);
	if (ret < 0) {
		return;
	}

	ret = codec_i2c_read(dev_data, val, 1);
	if (ret < 0) {
		return;
	}

#ifdef DEBUG
	printk("RD PG:%u REG:%02u VAL:0x%02x\n",
			reg.page, reg.reg_addr, *val);
#endif
}

static void codec_soft_reset(struct device *dev)
{
	/* soft reset the DAC */
	codec_write_reg(dev, SOFT_RESET_ADDR, SOFT_RESET_ASSERT);
}

static int codec_configure_dai(struct device *dev, audio_dai_cfg_t *cfg)
{
	uint8_t val;
	struct i2s_config *i2s = &cfg->i2s;

    if (i2s->format == I2S_FMT_DATA_FORMAT_I2S) {
	    val = IF_CTRL_IFTYPE(IF_CTRL_IFTYPE_I2S);
    } else if (i2s->format == I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED) {
	    val = IF_CTRL_IFTYPE(IF_CTRL_IFTYPE_LJF);
    } else if (i2s->format == I2S_FMT_DATA_FORMAT_RIGHT_JUSTIFIED) {
	    val = IF_CTRL_IFTYPE(IF_CTRL_IFTYPE_RJF);
    } else {
		LOG_ERR("Unsupported format %u", i2s->format);
		return -EINVAL;
    }

	if (codec_is_bclk_out(cfg)) {
		val |= IF_CTRL_BCLK_OUT;
	}

	if (codec_is_wclk_out(cfg)) {
		val |= IF_CTRL_WCLK_OUT;
	}

	switch (i2s->word_size) {
	case AUDIO_PCM_WIDTH_16_BITS:
		val |= IF_CTRL_WLEN(IF_CTRL_WLEN_16);
		break;
	case AUDIO_PCM_WIDTH_20_BITS:
		val |= IF_CTRL_WLEN(IF_CTRL_WLEN_20);
		break;
	case AUDIO_PCM_WIDTH_24_BITS:
		val |= IF_CTRL_WLEN(IF_CTRL_WLEN_24);
		break;
	case AUDIO_PCM_WIDTH_32_BITS:
		val |= IF_CTRL_WLEN(IF_CTRL_WLEN_32);
		break;
	default:
		LOG_ERR("Unsupported PCM sample bit width %u",
				i2s->word_size);
		return -EINVAL;
	}

	codec_write_reg(dev, IF_CTRL1_ADDR, val);

	return 0;
}

static int codec_configure_clocks(struct device *dev,
				  struct audio_codec_cfg *cfg)
{
	uint64_t pll_clkin, codec_clkin = 0, dac_clk = 0, mod_clk;
	int p, r;
	uint64_t jd, j = 0, d = 0;
	int mdac, ndac, dosr, bclk_div = 1, mclk_div;
    int madc, nadc, aosr;
	struct i2s_config *i2s;
	bool found = false;

	i2s = &cfg->dai_cfg.i2s;
#ifdef DEBUG
	printk("MCLK %u Hz PCM Rate: %u Hz\n", cfg->mclk_freq,
			i2s->frame_clk_freq);
#endif

	pll_clkin = cfg->mclk_freq;
	p = r = 1; /* fixed for simplicity */
	for (ndac = 1; ndac <=  128 && !found; ndac++) {
		for (mdac = 1; mdac <= 128 && !found; mdac++) {
            /* Restrict DOSR so that we will end up using the same
             * value for AOSR.
             */
			for (dosr = 1; dosr < 0xff; dosr++) {
				mod_clk = i2s->frame_clk_freq * dosr;
				if (mod_clk > DAC_MOD_CLK_FREQ_MAX) {
					continue;
				}
				dac_clk = mod_clk * mdac;
				codec_clkin = dac_clk * ndac;
				if (dac_clk > DAC_CLK_FREQ_MAX) {
					continue;
				}
				if (codec_clkin < 80000000 || codec_clkin > 110000000) {
					continue;
				}
				if (codec_clkin % pll_clkin == 0) {
					j = codec_clkin / pll_clkin;
					d = 0;
					if ((pll_clkin / p) < 512000 || (pll_clkin / p) > 20000000) {
						continue;
					}
					if (r * j < 4 || r * j > 259) {
						continue;
					}
					found = true;
					break;
				} else {
					jd = (codec_clkin * 10000) / pll_clkin;
					j = jd / 10000;
					d = jd % 10000;
					if (j == 0) {
						continue;
					}
					if ((pll_clkin / p) < 10000000 || (pll_clkin / p) > 20000000) {
						continue;
					}
					found = true;
					break;
				}
			}
		}
	}

    if (!found) {
        printk("Couldn't find any sane values for MCLK freq. of %d.\n", cfg->mclk_freq);
        return -EINVAL;
    }

    /* Adjust against the overruns. */
    mdac--;
    ndac--;


    /* XXX: per-direction setting is not supported. */
    madc = mdac;
    nadc = ndac;
    aosr = dosr;

#ifdef DEBUG
    printk("CODEC clock-in freq; %llu, Processing freq: %llu Hz Modulator freq: %llu Hz\n",
            codec_clkin, dac_clk, mod_clk);
    printk("P: %u, R: %u, J.D: %u.%u, NDAC, NADC: %u MDAC, MADC: %u DOSR, AOSR: %u\n",
            p, r, (uint32_t)j, (uint32_t)d, ndac, mdac, dosr);
#endif


	/* enable PLL  */
#ifdef CONFIG_PLLCLKIN_GPIO1
	codec_write_reg(dev, CLKGEN_MUX_ADDR,
			CLKGEN(CLKGEN_PLL_GPIO1, CLKGEN_CODEC_PLL));
#else
	codec_write_reg(dev, CLKGEN_MUX_ADDR,
			CLKGEN(CLKGEN_PLL_MCLK, CLKGEN_CODEC_PLL));
#endif

	/* set P, R, then J, followed by D */
	codec_write_reg(dev, PLL_PR_ADDR, PLL_PR(p, r));
	codec_write_reg(dev, PLL_J_ADDR, PLL_J(j));
	codec_write_reg(dev, PLL_D_MSB_ADDR, PLL_D_MSB(d));
	codec_write_reg(dev, PLL_D_LSB_ADDR, PLL_D_LSB(d));

    if (codec_is_bclk_out(&cfg->dai_cfg)) {
		bclk_div = dosr * mdac / (i2s->word_size * 2U); /* stereo */
		if ((bclk_div * i2s->word_size * 2) > (dosr * mdac)) {
			LOG_ERR("Unable to generate BCLK %u from MCLK %u",
				i2s->frame_clk_freq * i2s->word_size * 2U,
				cfg->mclk_freq);
			return -EINVAL;
		}
#ifdef DEBUG
		printk("I2S Master BCLKDIV: %u\n", bclk_div);
#endif
	}

	/* set NDAC, then MDAC, followed by DOSR */
	codec_write_reg(dev, NDAC_DIV_ADDR,
			(uint8_t)(NDAC_DIV(ndac) | NDAC_POWER_UP_MASK));
	codec_write_reg(dev, MDAC_DIV_ADDR,
			(uint8_t)(MDAC_DIV(mdac) | MDAC_POWER_UP_MASK));
	codec_write_reg(dev, DOSR_MSB_ADDR, (uint8_t)((dosr >> 8) & DOSR_MSB_MASK));
	codec_write_reg(dev, DOSR_LSB_ADDR, (uint8_t)(dosr & DOSR_LSB_MASK));

	/* set NADC, MADC, and AOSR in a similar manner */
	codec_write_reg(dev, NADC_DIV_ADDR,
			(uint8_t)(NADC_DIV(nadc) | NADC_POWER_UP_MASK));
	codec_write_reg(dev, MADC_DIV_ADDR,
			(uint8_t)(MADC_DIV(madc) | MADC_POWER_UP_MASK));
	codec_write_reg(dev, AOSR_ADDR, (uint8_t)aosr);

    if (codec_is_bclk_out(&cfg->dai_cfg)) {
		codec_write_reg(dev, IF_CTRL2_ADDR,
				IF_CTRL2_BDIV_CLKIN(IF_CTRL2_BDIV_CLKIN_DAC_CLK));
		codec_write_reg(dev, BCLK_DIV_ADDR,
				BCLK_DIV(bclk_div) | BCLK_DIV_POWER_UP);
	}

	/* calculate MCLK divider to get ~1MHz */
	mclk_div = DIV_ROUND_UP(cfg->mclk_freq, 1000000);
	/* setup timer clock to be MCLK divided */
	codec_write_reg(dev, TIMER_MCLK_DIV_ADDR,
			TIMER_MCLK_DIV_EN_EXT | TIMER_MCLK_DIV_VAL(mclk_div));
#ifdef DEBUG
	printk("Timer MCLK Divider: %u\n", mclk_div);
#endif

	return 0;
}

static int codec_configure_filters(struct device *dev,
				   audio_dai_cfg_t *cfg)
{
	enum proc_block dac_blk, adc_blk;

	/* determine decimation filter type */
	if (cfg->i2s.frame_clk_freq >= AUDIO_PCM_RATE_192K) {
		dac_blk = PRB_P18_DECIMATION_C;
        adc_blk = PRB_R18_DECIMATION_C;
#ifdef DEBUG
		printk("PCM Rate: %u Filter C PRB P18, R18 selected\n",
				cfg->i2s.frame_clk_freq);
#endif
	} else if (cfg->i2s.frame_clk_freq >= AUDIO_PCM_RATE_96K) {
		dac_blk = PRB_P10_DECIMATION_B;
        adc_blk = PRB_R12_DECIMATION_B;
#ifdef DEBUG
		printk("PCM Rate: %u Filter B PRB P10, R12 selected\n",
				cfg->i2s.frame_clk_freq);
#endif
	} else {
		dac_blk = PRB_P11_DECIMATION_A;
        adc_blk = PRB_R4_DECIMATION_A;
#ifdef DEBUG
		printk("PCM Rate: %u Filter A PRB P11, R4 selected\n",
				cfg->i2s.frame_clk_freq);
#endif
	}

	codec_write_reg(dev, DAC_PBLK_SEL_ADDR, DAC_PBLK_SEL(dac_blk));
	codec_write_reg(dev, ADC_PBLK_SEL_ADDR, ADC_PBLK_SEL(adc_blk));
	return 0;
}

static void codec_configure_output(struct device *dev)
{
	uint8_t val;

	/*
	 * set common mode voltage to 1.65V (half of AVDD)
	 * AVDD is typically 3.3V
	 */
	codec_read_reg(dev, HEADPHONE_DRV_ADDR, &val);
	val &= ~HEADPHONE_DRV_CM_MASK;
	val |= HEADPHONE_DRV_CM(CM_VOLTAGE_1P65) | HEADPHONE_DRV_RESERVED;
	codec_write_reg(dev, HEADPHONE_DRV_ADDR, val);

	/* enable pop removal on power down/up */
	codec_read_reg(dev, HP_OUT_POP_RM_ADDR, &val);
	codec_write_reg(dev, HP_OUT_POP_RM_ADDR, val | HP_OUT_POP_RM_ENABLE);

	/* route DAC output to Headphone */
	val = OUTPUT_ROUTING_HPL | OUTPUT_ROUTING_HPR;
	codec_write_reg(dev, OUTPUT_ROUTING_ADDR, val);

	/* enable volume control on Headphone out */
	codec_write_reg(dev, HPL_ANA_VOL_CTRL_ADDR,
			HPX_ANA_VOL(HPX_ANA_VOL_DEFAULT));
	codec_write_reg(dev, HPR_ANA_VOL_CTRL_ADDR,
			HPX_ANA_VOL(HPX_ANA_VOL_DEFAULT));

	/* set headphone outputs as line-out */
	codec_write_reg(dev, HEADPHONE_DRV_CTRL_ADDR, HEADPHONE_DRV_LINEOUT);

	/* unmute headphone drivers */
	codec_write_reg(dev, HPL_DRV_GAIN_CTRL_ADDR, HPX_DRV_UNMUTE);
	codec_write_reg(dev, HPR_DRV_GAIN_CTRL_ADDR, HPX_DRV_UNMUTE);

	/* power up headphone drivers */
	codec_read_reg(dev, HEADPHONE_DRV_ADDR, &val);
	val |= HEADPHONE_DRV_POWERUP | HEADPHONE_DRV_RESERVED;
	codec_write_reg(dev, HEADPHONE_DRV_ADDR, val);
}

static void codec_configure_input(struct device *dev)
{
	uint8_t val;

	/*
	 * set MICBIAS to AVDD
	 */
	codec_read_reg(dev, MICBIAS_CTRL_ADDR, &val);
	val &= ~MICBIAS_OUTPUT_PWR_MASK;
	val |= MICBIAS_OUTPUT_PWR(MICBIAS_OUTPUT_AVDD) | MICBIAS_PWRUP_WO_HSDET;
	codec_write_reg(dev, MICBIAS_CTRL_ADDR, val);

	/* enable PGA with max. gain */
	codec_write_reg(dev, MIC_PGA_CTRL_ADDR, MIC_PGA_GAIN(2));

	/* set P-Terminal to MIC with 10k */
	val = MIC_PGA_INPUT_P(MIC_PGA_INPUT_10K, MIC_PGA_INPUT_NONE, MIC_PGA_INPUT_NONE);
	codec_write_reg(dev, MIC_PGA_IN_PSEL_ADDR, val);

	/* set M-Terminal to CM with 10k */
	val = MIC_PGA_INPUT_M(MIC_PGA_INPUT_10K, MIC_PGA_INPUT_NONE);
	codec_write_reg(dev, MIC_PGA_IN_MSEL_ADDR, val);

	/* set ADC volume to 10dB */
	codec_write_reg(dev, ADC_VOL_C_CTRL_ADDR, ADC_VOL_C_SEL(ADC_VOL_C_ADJ(10)));
}

static int codec_set_output_dig_volume(struct device *dev, float vol)
{
    int8_t val;

	if ((vol > CODEC_OUTPUT_DIG_VOLUME_MAX) ||
			(vol < CODEC_OUTPUT_DIG_VOLUME_MIN)) {
		LOG_ERR("Invalid volume %*f dB", vol);
		return -EINVAL;
	}

    val = (int8_t)(vol * 2);

	codec_write_reg(dev, L_DIG_VOL_CTRL_ADDR, (uint8_t)val);
	codec_write_reg(dev, R_DIG_VOL_CTRL_ADDR, (uint8_t)val);

	return 0;
}

static int codec_get_output_dig_volume(struct device *dev, float *min,
        float *cur, float *max)
{
	uint8_t val;
	int vol;

    *max = CODEC_OUTPUT_DIG_VOLUME_MAX;
    *min = CODEC_OUTPUT_DIG_VOLUME_MIN;

	codec_read_reg(dev, L_DIG_VOL_CTRL_ADDR, &val);

	vol = val;
	if (val & 0x80) {
		vol |= 0xffffff00; /* sign-extended */
	}
    *cur = (float)vol / 2;

	return 0;
}

#if (LOG_LEVEL >= LOG_LEVEL_DEBUG)
static void codec_read_all_regs(struct device *dev)
{
	uint8_t val;

	codec_read_reg(dev, SOFT_RESET_ADDR, &val);
	codec_read_reg(dev, CLKGEN_MUX_ADDR, &val);
	codec_read_reg(dev, PLL_PR_ADDR, &val);
	codec_read_reg(dev, PLL_J_ADDR, &val);
	codec_read_reg(dev, PLL_D_MSB_ADDR, &val);
	codec_read_reg(dev, PLL_D_LSB_ADDR, &val);
	codec_read_reg(dev, NDAC_DIV_ADDR, &val);
	codec_read_reg(dev, MDAC_DIV_ADDR, &val);
	codec_read_reg(dev, DOSR_MSB_ADDR, &val);
	codec_read_reg(dev, DOSR_LSB_ADDR, &val);
	codec_read_reg(dev, IF_CTRL1_ADDR, &val);
	codec_read_reg(dev, BCLK_DIV_ADDR, &val);
	codec_read_reg(dev, OVF_FLAG_ADDR, &val);
	codec_read_reg(dev, DAC_PBLK_SEL_ADDR, &val);
	codec_read_reg(dev, DAC_PATH_SETUP_ADDR, &val);
	codec_read_reg(dev, DAC_VOL_CTRL_ADDR, &val);
	codec_read_reg(dev, L_DIG_VOL_CTRL_ADDR, &val);
	codec_read_reg(dev, DRC_CTRL1_ADDR, &val);
	codec_read_reg(dev, L_BEEP_GEN_ADDR, &val);
	codec_read_reg(dev, R_BEEP_GEN_ADDR, &val);
	codec_read_reg(dev, BEEP_LEN_MSB_ADDR, &val);
	codec_read_reg(dev, BEEP_LEN_MIB_ADDR, &val);
	codec_read_reg(dev, BEEP_LEN_LSB_ADDR, &val);

	codec_read_reg(dev, HEADPHONE_DRV_ADDR, &val);
	codec_read_reg(dev, HP_OUT_POP_RM_ADDR, &val);
	codec_read_reg(dev, OUTPUT_ROUTING_ADDR, &val);
	codec_read_reg(dev, HPL_ANA_VOL_CTRL_ADDR, &val);
	codec_read_reg(dev, HPR_ANA_VOL_CTRL_ADDR, &val);
	codec_read_reg(dev, HPL_DRV_GAIN_CTRL_ADDR, &val);
	codec_read_reg(dev, HPR_DRV_GAIN_CTRL_ADDR, &val);
	codec_read_reg(dev, HEADPHONE_DRV_CTRL_ADDR, &val);

	codec_read_reg(dev, TIMER_MCLK_DIV_ADDR, &val);
}
#endif

static void i2c_notify(struct i2c_event *event, void *ctx)
{
	struct codec_driver_data *data = ctx;

	switch (event->type) {
	case I2C_EVENT_MASTER_TRANS_CMPL:
		osSemaphoreRelease(data->sem);
		break;
	default:
		LOG_ERR("Unknown I2C response: %d\n", event->type);
		break;
	}
}

static int aic3110_codec_probe(struct device *dev)
{
	struct codec_driver_data *priv;
	struct i2c_cfg i2c;
	struct file *file;
	char buf[32];

	priv = zalloc(sizeof(*priv));
	if (!priv) {
		return -ENOMEM;
	}

	priv->dev = dev;
	priv->bus = device_get_by_name(dev->io.bus);
	if (!priv->bus) {
		LOG_ERR("%s: failed to get a bus\n", dev_name(dev));
		return -ENOSYS;
	}

	i2c.role = I2C_ROLE_MASTER;
	i2c.addr_len = I2C_ADDR_LEN_7BIT;
	i2c.bitrate = 400 * 1000;
	i2c.dma_en = 0;
	i2c.pull_up_en = 1;

	priv->sem = osSemaphoreNew(1, 0, NULL);

	i2c_configure(priv->bus, &i2c, i2c_notify, priv);

	priv->addr = 0x18;
	priv->reg_addr_cache.page = 0xff;
	priv->devfs_ops.ioctl = codec_ioctl;

	sprintf(buf, "/dev/audio");

	file = vfs_register_device_file(buf, &priv->devfs_ops, dev);
	if (!file) {
		LOG_ERR("%s: failed to register as %s\n", dev_name(dev), buf);
		return -ENOSYS;
	}

	printk("%s: registered as %s.\n", dev_name(dev), buf);

	dev->driver_data = priv;

	return 0;
}

int aic3110_codec_shutdown(struct device *dev)
{
	struct codec_driver_data *priv = dev->driver_data;

	free(priv);

	return 0;
}

#ifdef CONFIG_PM_DM
static int aic3110_codec_suspend(struct device *dev, u32 *idle)
{
	return 0;
}

static int aic3110_codec_resume(struct device *dev)
{
	return 0;
}
#endif

static struct audio_codec_ops aic3110_codec_ops = {
	.configure			= codec_configure,
	.start_output		= codec_start_output,
	.stop_output		= codec_stop_output,
	.start_input		= codec_start_input,
	.stop_input		    = codec_stop_input,
	.set_property		= codec_set_property,
	.apply_properties	= codec_apply_properties,
	.get_property		= codec_get_property,
};

static declare_driver(audio) = {
    .name = "tlv320aic3110",
	.probe      = aic3110_codec_probe,
	.shutdown   = aic3110_codec_shutdown,
#ifdef CONFIG_PM_DM
	.suspend    = aic3110_codec_suspend,
	.resume     = aic3110_codec_resume,
#endif
	.ops        = &aic3110_codec_ops,
};

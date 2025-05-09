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

#include <stdio.h>
#include <errno.h>
#include <hal/init.h>
#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/pinctrl.h>
#include <hal/i2s.h>
#include <hal/i2c.h>
#include <hal/kmem.h>
#include <hal/timer.h>
#include <hal/codec.h>

#include <cmsis_os.h>

#define ERROR
#define DEBUG

#ifdef ERROR
#define LOG_ERR(...)					printk(__VA_ARGS__)
#else
#define LOG_ERR(...)					(void)0
#endif

#undef BIT
#define BIT(n)	(1 << (n))

#undef BIT_MASK
#define BIT_MASK(n)	(BIT(n) - 1)

#define RESET					(0x00)
#define RESET_CSM_ON			BIT(7)
#define RESET_MSC				BIT(6)
#define RESET_SEQ_DIS           BIT(5)
#define RESET_RST_DIG           BIT(4)
#define RESET_RST_CMG           BIT(3)
#define RESET_RST_MST           BIT(2)
#define RESET_RST_ADC_DIG       BIT(1)
#define RESET_RST_DAC_DIG       BIT(0)
#define RESET_ASSERT			(RESET_RST_DIG | RESET_RST_CMG | RESET_RST_MST |\
        RESET_RST_ADC_DIG | RESET_RST_DAC_DIG)
#define CLK00					(0x01)
#define CLK00_MCLK_SEL			BIT(7)
#define CLK00_MCLK_INV			BIT(6)
#define CLK00_MCLK_ON			BIT(5)
#define CLK00_BCLK_ON			BIT(4)
#define CLK00_CLKADC_ON			BIT(3)
#define CLK00_CLKDAC_ON			BIT(2)
#define CLK00_ANACLKADC_ON		BIT(1)
#define CLK00_ANACLKDAC_ON		BIT(0)
#define CLK01					(0x02)
#define CLK01_DIV_PRE_MASK		BIT_MASK(3)
#define CLK01_DIV_PRE(val)		(((val) & CLK01_DIV_PRE_MASK) << 5)
#define CLK01_MULT_PRE_MASK		BIT_MASK(2)
#define CLK01_MULT_PRE(val)		(((val) & CLK01_MULT_PRE_MASK) << 3)
#define CLK01_PATHSEL			BIT(2)
#define CLK01_DELYSEL_MASK		BIT_MASK(2)
#define CLK01_DELYSEL(val)		((val) & CLK01_DELYSEL_MASK)
#define CLK02					(0x03)
#define CLK03					(0x04)
#define CLK04					(0x05)
#define CLK05					(0x06)
#define CLK05_BCLK_CON			BIT(6)
#define CLK05_BCLK_INV			BIT(5)
#define CLK05_DIV_BCLK_MASK		BIT_MASK(5)
#define CLK05_DIV_BCLK(div)		((div - 1) & CLK05_DIV_BCLK_MASK)
#define CLK06					(0x07)
#define CLK06_TRI_BLRCK			BIT(5)
#define CLK06_TRI_ADCDAT		BIT(4)
#define CLK06_DIV_LRCLK_H_MASK	BIT_MASK(4)
#define CLK06_DIV_LRCLK_H(div)	(((div - 1) >> 8) & CLK06_DIV_LRCLK_H_MASK)
#define CLK07					(0x08)
#define CLK07_DIV_LRCLK_L(div)	(((div - 1)) & BIT_MASK(8))
#define SDP00					(0x09)
#define SDP00_IN_SEL            BIT(7)
#define SDP00_IN_MUTE           BIT(6)
#define SDP00_IN_LRP            BIT(5)
#define SDP00_IN_WL_MASK        BIT_MASK(3)
#define SDP00_IN_WL(wl)         (((wl) & SDP00_IN_WL_MASK) << 2)
#define SDP00_IN_WL_24          0
#define SDP00_IN_WL_20          1
#define SDP00_IN_WL_18          2
#define SDP00_IN_WL_16          3
#define SDP00_IN_WL_32          4
#define SDP00_IN_FMT_MASK       BIT_MASK(2)
#define SDP00_IN_FMT(fmt)       ((fmt) & SDP00_IN_FMT_MASK)
#define SDP00_IN_FMT_I2S        0
#define SDP00_IN_FMT_LJF        1
#define SDP00_IN_FMT_RJF        2
#define SDP01					(0x0a)
#define SDP01_OUT_MUTE          BIT(6)
#define SDP01_OUT_LRP           BIT(5)
#define SDP00_OUT_WL_MASK       BIT_MASK(3)
#define SDP00_OUT_WL(wl)        (((wl) & SDP00_OUT_WL_MASK) << 2)
#define SDP00_OUT_WL_24         0
#define SDP00_OUT_WL_20         1
#define SDP00_OUT_WL_18         2
#define SDP00_OUT_WL_16         3
#define SDP00_OUT_WL_32         4
#define SDP00_OUT_FMT_MASK      BIT_MASK(2)
#define SDP00_OUT_FMT(fmt)      ((fmt) & SDP00_OUT_FMT_MASK)
#define SDP00_OUT_FMT_I2S       0
#define SDP00_OUT_FMT_LJF       1
#define SDP00_OUT_FMT_RJF       2
#define SYS00					(0x0b)
#define SYS01					(0x0c)
#define SYS02					(0x0d)
#define SYS02_PDN_ANA			BIT(7)
#define SYS02_PDN_IBIASGEN		BIT(6)
#define SYS02_PDN_ADCBIASGEN	BIT(5)
#define SYS02_PDN_ADCVREFGEN	BIT(4)
#define SYS02_PDN_DACVREFGEN	BIT(3)
#define SYS02_PDN_VREF			BIT(2)
#define SYS02_VMIDSEL_MASK		BIT_MASK(2)
#define SYS02_VMIDSEL_PDN		(0)
#define SYS02_VMIDSEL_S_NORM	(1)
#define SYS02_VMIDSEL_NORM_OP	(2)
#define SYS02_VMIDSEL_S_FAST	(3)
#define SYS02_VMIDSEL(val)		((val) & SYS02_VMIDSEL_MASK)
#define SYS02_ADC_PDN			(SYS02_PDN_ADCBIASGEN | SYS02_PDN_ADCVREFGEN)
#define SYS02_DAC_PDN			(SYS02_PDN_DACVREFGEN)
#define SYS03					(0x0e)
#define SYS03_PDN_PGA           BIT(6)
#define SYS03_PDN_MOD           BIT(5)
#define SYS03_RST_MOD           BIT(4)
#define SYS03_VROI              BIT(3)
#define SYS03_LPVREFBUF         BIT(2)
#define SYS04					(0x0f)
#define SYS04_LPDAC             BIT(7)
#define SYS04_LPPGA             BIT(6)
#define SYS04_LPPGAOUT          BIT(5)
#define SYS04_LPVCMMOD          BIT(4)
#define SYS04_LPADCVRP          BIT(3)
#define SYS04_LPDACVRP          BIT(2)
#define SYS04_LPFLASH           BIT(1)
#define SYS04_LPINT1            BIT(0)
#define SYS05					(0x10)
#define SYS05_SYNCMODE          BIT(7)
#define SYS05_VMIDLOW_MASK      BIT_MASK(2)
#define SYS05_VMISLOW(vmid)     (((vmid) & SYS05_VMIDLOW_MASK) << 5)
#define SYS05_DAC_IBIAS_SW      BIT(4)
#define SYS05_IBIAS_SW_MASK     BIT_MASK(2)
#define SYS05_IBIAS_SW(bias)    (((bias) & SYS05_IBIAS_SW_MASK) << 2)
#define SYS05_IBIAS_SW_LVL_0    0
#define SYS05_IBIAS_SW_LVL_1    1
#define SYS05_IBIAS_SW_LVL_2    2
#define SYS05_IBIAS_SW_LVL_3    3
#define SYS05_VX2OFF            BIT(1)
#define SYS05_VX1SEL            BIT(0)
#define SYS06					(0x11)
#define SYS07					(0x12)
#define SYS07_PDN_DAC           BIT(1)
#define SYS07_ENPEFR            BIT(0)
#define SYS08					(0x13)
#define SYS08_HPSW				BIT(4)
#define SYS09					(0x14)
#define SYS09_DMIC_ON           BIT(6)
#define SYS09_LINSEL            BIT(4)
#define SYS09_PGAGAIN_MASK      BIT_MASK(4)
/* Actual gain will be G*3 dBm, e.g., 9 -> 27dBm. */
#define SYS09_PGAGAIN(gain)     ((gain) & SYS09_PGAGAIN_MASK)
#define ADC00					(0x15)
#define ADC01					(0x16)
#define ADC02					(0x17)
#define ADC03					(0x18)
#define ADC04					(0x19)
#define ADC05					(0x1a)
#define ADC06					(0x1b)
#define ADC06_HPFS1_MASK        BIT_MASK(5)
#define ADC06_HPFS1(coeff)      ((coeff) & ADC06_HPFS1_MASK)
#define ADC07					(0x1c)
#define ADC07_EQBYPASS          BIT(6)
#define ADC07_HPF               BIT(5)
#define ADC07_HPFS2_MASK        BIT_MASK(5)
#define ADC07_HPFS2(coeff)      ((coeff) & ADC07_HPFS2_MASK)
#define ADC08					(0x1d)
/* ADCEQ00 - ADCEQ18 */
#define DAC00					(0x31)
#define DAC00_DSMMUTE_TO        BIT(7)
#define DAC00_DSMMUTE           BIT(6)
#define DAC00_DEMMUTE           BIT(5)
#define DAC00_INV               BIT(4)
#define DAC00_RAMCLR            BIT(3)
#define DAC00_DSMDITH_OFF       BIT(2)
#define DAC01					(0x32)
#define DAC02					(0x33)
#define DAC03					(0x34)
#define DAC04					(0x35)
#define DAC05					(0x36)
#define DAC06					(0x37)
#define DAC06_RAMPRATE_MASK     BIT_MASK(4)
#define DAC06_RAMPRATE(r)       (((r) & DAC06_RAMPRATE_MASK) << 4)
#define DAC06_EQBYPASS          BIT(3)
/* DACEQ00 - DACEQ11 */
#define GPIO					(0x44)
#define GP						(0x45)
#define I2C						(0xfa)
#define FLAG					(0xfc)
#define CHIP00					(0xfd)
#define CHIP01					(0xfe)
#define CHIP02					(0xff)

#define CODEC_OUTPUT_DIG_VOLUME_MAX		(32)
#define CODEC_OUTPUT_DIG_VOLUME_MIN		(-95.5)

struct codec_driver_data {
    struct device *dev;
    struct device *bus; /* I2C device */
    u16 addr; /* I2C address */
    osSemaphoreId_t sem; /* I2C completion */
    struct fops devfs_ops;
};

static void codec_write_reg(struct device *dev, uint8_t reg, uint8_t val);
static void codec_read_reg(struct device *dev, uint8_t reg, uint8_t *val);
static void codec_soft_reset(struct device *dev);
static int codec_configure_dai(struct device *dev, audio_dai_cfg_t *cfg);
static int codec_configure_clocks(struct device *dev,
        struct audio_codec_cfg *cfg);
static int codec_configure_system(struct device *dev,
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

static bool codec_is_wclk_out(audio_dai_cfg_t *cfg) __maybe_unused;
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
        ret = codec_configure_system(dev, &cfg->dai_cfg);
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
    uint8_t val;

    codec_write_reg(dev, SYS02, SYS02_VMIDSEL(SYS02_VMIDSEL_S_NORM));

    codec_read_reg(dev, CLK00, &val);
    val |= (CLK00_CLKDAC_ON | CLK00_ANACLKDAC_ON);
    codec_write_reg(dev, CLK00, val);

    codec_read_reg(dev, RESET, &val);
    val |= RESET_CSM_ON;
    codec_write_reg(dev, RESET, val);

    udelay(1000);

    codec_write_reg(dev, SYS02, SYS02_VMIDSEL(SYS02_VMIDSEL_S_NORM));

    codec_read_reg(dev, SYS07, &val);
    val &= ~SYS07_PDN_DAC;
    codec_write_reg(dev, SYS07, val);

    codec_read_reg(dev, SYS03, &val);
    val &= ~(SYS03_VROI);
    val |= 2; /* no description in the datasheet */
    codec_write_reg(dev, SYS03, val);

    return 0;
}

static int codec_stop_output(struct device *dev)
{
    uint8_t val;

    codec_read_reg(dev, SYS03, &val);
    val |= (SYS03_VROI | SYS03_LPVREFBUF);
    val |= 3; /* no description in the datasheet */
    codec_write_reg(dev, SYS03, val);

    codec_read_reg(dev, SYS07, &val);
    val |= SYS07_PDN_DAC;
    codec_write_reg(dev, SYS07, val);

    codec_read_reg(dev, SYS02, &val);
    val &= ~SYS02_VMIDSEL_MASK;
    val |= (SYS02_PDN_DACVREFGEN | SYS02_VMIDSEL(SYS02_VMIDSEL_S_NORM));
    codec_write_reg(dev, SYS02, val);

    codec_read_reg(dev, RESET, &val);
    val |= (RESET_CSM_ON | RESET_RST_DAC_DIG);
    codec_write_reg(dev, RESET, val);

    codec_read_reg(dev, CLK00, &val);
    val &= ~(CLK00_CLKDAC_ON | CLK00_ANACLKDAC_ON);
    codec_write_reg(dev, CLK00, val);

    return 0;
}

/* XXX: SDP stands for 'Serial Data Port', which is
 *      CODEC's internal block for digital audio interface.
 *      So, the direction is opposite from what SoC sees it.
 */

static void codec_mute_output(struct device *dev)
{
    uint8_t val;

    codec_read_reg(dev, SDP00, &val);
    val |= SDP00_IN_MUTE;
    codec_write_reg(dev, SDP00, val);
}

static void codec_unmute_output(struct device *dev)
{
    uint8_t val;

    codec_read_reg(dev, SDP00, &val);
    val &= ~SDP00_IN_MUTE;
    codec_write_reg(dev, SDP00, val);
}

static int codec_start_input(struct device *dev)
{
    uint8_t val;

    codec_write_reg(dev, SYS02, SYS02_VMIDSEL(SYS02_VMIDSEL_S_NORM));

    codec_read_reg(dev, CLK00, &val);
    val |= (CLK00_CLKADC_ON | CLK00_ANACLKADC_ON);
    codec_write_reg(dev, CLK00, val);

    codec_read_reg(dev, RESET, &val);
    val |= RESET_CSM_ON;
    codec_write_reg(dev, RESET, val);

    udelay(1000);

    codec_write_reg(dev, SYS02, SYS02_VMIDSEL(SYS02_VMIDSEL_S_NORM));

    codec_read_reg(dev, SYS03, &val);
    val &= ~(SYS03_PDN_PGA | SYS03_PDN_MOD | SYS03_VROI);
    val |= 2; /* no description in the datasheet */
    codec_write_reg(dev, SYS03, val);

    udelay(50000); /* too much?? */

    codec_read_reg(dev, SDP01, &val);
    val &= ~SDP01_OUT_MUTE;
    codec_write_reg(dev, SDP01, val);

    CODEC_DUMP_REGS(dev);

    return 0;
}

static int codec_stop_input(struct device *dev)
{
    uint8_t val;

    codec_read_reg(dev, SDP01, &val);
    val |= SDP01_OUT_MUTE;
    codec_write_reg(dev, SDP01, val);

    codec_read_reg(dev, SYS03, &val);
    val |= (SYS03_PDN_PGA | SYS03_RST_MOD | SYS03_VROI | SYS03_LPVREFBUF);
    val |= 3; /* no description in the datasheet */
    codec_write_reg(dev, SYS03, val);

    codec_read_reg(dev, SYS02, &val);
    val &= ~SYS02_VMIDSEL_MASK;
    val |= (SYS02_PDN_ADCBIASGEN | SYS02_PDN_ADCVREFGEN |\
            SYS02_VMIDSEL(SYS02_VMIDSEL_S_NORM));
    codec_write_reg(dev, SYS02, val);

    codec_read_reg(dev, RESET, &val);
    val |= (RESET_CSM_ON | RESET_RST_ADC_DIG);
    codec_write_reg(dev, RESET, val);

    codec_read_reg(dev, CLK00, &val);
    val &= ~(CLK00_CLKADC_ON | CLK00_ANACLKADC_ON);
    codec_write_reg(dev, CLK00, val);

    return 0;
}

static void codec_mute_input(struct device *dev)
{
    uint8_t val;

    codec_read_reg(dev, SDP01, &val);
    val |= SDP01_OUT_MUTE;
    codec_write_reg(dev, SDP01, val);
}

static void codec_unmute_input(struct device *dev)
{
    uint8_t val;

    codec_read_reg(dev, SDP01, &val);
    val &= ~SDP01_OUT_MUTE;
    codec_write_reg(dev, SDP01, val);
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

                codec_read_reg(dev, SDP00, &reg);
                val->mute = ((reg & SDP00_IN_MUTE) == SDP00_IN_MUTE);
                return 0;
            }
        case AUDIO_PROPERTY_INPUT_MUTE:
            {
                u8 reg;

                codec_read_reg(dev, SDP01, &reg);
                val->mute = ((reg & SDP01_OUT_MUTE) == SDP01_OUT_MUTE);
                return 0;
            }
        default:
            break;
    }

    return -EINVAL;
}

static int codec_dump(struct device *dev)
{
    CODEC_DUMP_REGS(dev);

    return 0;
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

static void codec_write_reg(struct device *dev, uint8_t reg, uint8_t val)
{
    struct codec_driver_data *const dev_data = dev->driver_data;
    uint8_t data[2];
    int ret;

    data[0] = reg;
    data[1] = val;
    ret = codec_i2c_write(dev_data, data, sizeof(data));
    if (ret < 0) {
        LOG_ERR("codec_i2c_write error: %d\n", ret);
        return;
    }

#ifdef DEBUG
    printk("WR REG:0x%02x VAL:0x%02x\n", reg, val);
#endif
}

static void codec_read_reg(struct device *dev, uint8_t reg, uint8_t *val)
{
    struct codec_driver_data *const dev_data = dev->driver_data;
    int ret;

    ret = codec_i2c_write(dev_data, &reg, 1);
    if (ret < 0) {
        return;
    }

    ret = codec_i2c_read(dev_data, val, 1);
    if (ret < 0) {
        return;
    }

#ifdef DEBUG
    printk("RD REG:0x%02x VAL:0x%02x\n", reg, *val);
#endif
}

static void codec_soft_reset(struct device *dev)
{
    uint8_t val;

    codec_read_reg(dev, RESET, &val);
    val = RESET_ASSERT;
    codec_write_reg(dev, RESET, val);

    val = RESET_CSM_ON;
    codec_write_reg(dev, RESET, val);

    udelay(1000);

    val = SYS02_VMIDSEL(SYS02_VMIDSEL_S_NORM);
    codec_write_reg(dev, SYS02, val);
}

static int codec_configure_dai(struct device *dev, audio_dai_cfg_t *cfg)
{
    uint8_t val;
    struct i2s_config *i2s = &cfg->i2s;

    if (i2s->format == I2S_FMT_DATA_FORMAT_I2S) {
        val = SDP00_IN_FMT(SDP00_IN_FMT_I2S);
    } else if (i2s->format == I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED) {
        val = SDP00_IN_FMT(SDP00_IN_FMT_LJF);
    } else if (i2s->format == I2S_FMT_DATA_FORMAT_RIGHT_JUSTIFIED) {
        val = SDP00_IN_FMT(SDP00_IN_FMT_RJF);
    } else {
        LOG_ERR("Unsupported format %u", i2s->format);
        return -EINVAL;
    }

    switch (cfg->i2s.word_size) {
        case AUDIO_PCM_WIDTH_16_BITS:
            val |= SDP00_IN_WL(SDP00_IN_WL_16);
            break;
        case AUDIO_PCM_WIDTH_20_BITS:
            val |= SDP00_IN_WL(SDP00_IN_WL_20);
            break;
        case AUDIO_PCM_WIDTH_24_BITS:
            val |= SDP00_IN_WL(SDP00_IN_WL_24);
            break;
        case AUDIO_PCM_WIDTH_32_BITS:
            val |= SDP00_IN_WL(SDP00_IN_WL_32);
            break;
        default:
            LOG_ERR("Unsupported PCM sample bit width %u",
                    cfg->i2s.word_size);
            return -EINVAL;
    }

    codec_write_reg(dev, SDP00, val);
    /* Same layout for SDP01 */
    codec_write_reg(dev, SDP01, val);

    codec_read_reg(dev, RESET, &val);
    if (codec_is_bclk_out(cfg)) {
        val |= RESET_MSC;
    } else {
        val &= ~RESET_MSC;
    }
    codec_write_reg(dev, RESET, val);

    return 0;
}

struct reg_entry_t
{
    uint8_t reg;
    uint8_t val;
};

struct clk_reg_table {
    uint32_t ratio;	/* MCLK/LRCLK ratio */
    struct reg_entry_t reg[5];
};

static struct clk_reg_table es8311_clk_configs[] =
{
    {1536,{{CLK01, 0xa0},{CLK02, 0x10},{ADC01, 0x24},{CLK03, 0x20},{CLK04, 0x00}}},
    {1500,{{CLK01, 0x90},{CLK02, 0x19},{ADC01, 0x20},{CLK03, 0x19},{CLK04, 0x22}}},
    {1280,{{CLK01, 0x80},{CLK02, 0x10},{ADC01, 0x24},{CLK03, 0x20},{CLK04, 0x00}}},
    {1024,{{CLK01, 0x60},{CLK02, 0x10},{ADC01, 0x24},{CLK03, 0x20},{CLK04, 0x00}}},
    {1000,{{CLK01, 0x89},{CLK02, 0x19},{ADC01, 0x21},{CLK03, 0x19},{CLK04, 0x00}}},
    { 960,{{CLK01, 0x20},{CLK02, 0x1e},{ADC01, 0x20},{CLK03, 0x1e},{CLK04, 0x00}}},
    { 768,{{CLK01, 0x40},{CLK02, 0x10},{ADC01, 0x24},{CLK03, 0x20},{CLK04, 0x00}}},
    { 750,{{CLK01, 0x9a},{CLK02, 0x19},{ADC01, 0x21},{CLK03, 0x19},{CLK04, 0x22}}},
    { 544,{{CLK01, 0x20},{CLK02, 0x11},{ADC01, 0x24},{CLK03, 0x11},{CLK04, 0x00}}},
    { 512,{{CLK01, 0x20},{CLK02, 0x10},{ADC01, 0x24},{CLK03, 0x20},{CLK04, 0x00}}},
    { 500,{{CLK01, 0x92},{CLK02, 0x19},{ADC01, 0x21},{CLK03, 0x19},{CLK04, 0x00}}},
    { 480,{{CLK01, 0x00},{CLK02, 0x1e},{ADC01, 0x20},{CLK03, 0x1e},{CLK04, 0x00}}},
    { 384,{{CLK01, 0x00},{CLK02, 0x18},{ADC01, 0x21},{CLK03, 0x18},{CLK04, 0x00}}},
    { 375,{{CLK01, 0x00},{CLK02, 0x17},{ADC01, 0x22},{CLK03, 0x17},{CLK04, 0x00}}},
    { 320,{{CLK01, 0x92},{CLK02, 0x10},{ADC01, 0x24},{CLK03, 0x20},{CLK04, 0x00}}},
    { 272,{{CLK01, 0x00},{CLK02, 0x11},{ADC01, 0x24},{CLK03, 0x11},{CLK04, 0x00}}},
    { 256,{{CLK01, 0x00},{CLK02, 0x10},{ADC01, 0x24},{CLK03, 0x20},{CLK04, 0x00}}},
    { 250,{{CLK01, 0x9a},{CLK02, 0x19},{ADC01, 0x21},{CLK03, 0x19},{CLK04, 0x00}}},
    { 192,{{CLK01, 0x51},{CLK02, 0x10},{ADC01, 0x24},{CLK03, 0x20},{CLK04, 0x00}}},
    { 128,{{CLK01, 0x0a},{CLK02, 0x10},{ADC01, 0x24},{CLK03, 0x20},{CLK04, 0x00}}},
    {  64,{{CLK01, 0x10},{CLK02, 0x10},{ADC01, 0x24},{CLK03, 0x20},{CLK04, 0x00}}},
    {  48,{{CLK01, 0x1a},{CLK02, 0x21},{ADC01, 0x20},{CLK03, 0x20},{CLK04, 0x30}}},
    {  32,{{CLK01, 0x1a},{CLK02, 0x10},{ADC01, 0x24},{CLK03, 0x20},{CLK04, 0x00}}},
};

static void apply_clk_configs(struct device *dev, struct clk_reg_table *ct)
{
    for (int i = 0; i < ARRAY_SIZE(ct->reg); i++) {
        struct reg_entry_t *re = &ct->reg[i];
        codec_write_reg(dev, re->reg, re->val);
    }
}

static int calc_bclk_div(int ratio, struct i2s_config *i2s)
{
    int bclk_div = 1;

    /*
     * ratio = mclk / lrclk
     * bclk = mclk / bclk_div
     *
     * bclk >= lrclk * (2 * wl)
     * mclk / bclk_div >= lrclk * (2 * wl)
     * bclk_div <= mclk / (lrclk * (2 * wl))
     * bclk_div <= (mclk / lrclk) / (2 * wl)
     * bclk_div <= ratio / (2 * wl)
     *
     * We select bclk_div at its maximum to minimize BCLK frequency.
     */
    bclk_div = ratio / (2 * (i2s->word_size == 16 ? 16 : 32));
    if (!bclk_div) {
        bclk_div = 1;
    }

    if (bclk_div > 20) { /* nonlinear region */
        if (bclk_div >= 72) {
            bclk_div = 32;
        } else if (bclk_div >= 66) {
            bclk_div = 31;
        } else if (bclk_div >= 48) {
            bclk_div = 30;
        } else if (bclk_div >= 44) {
            bclk_div = 29;
        } else if (bclk_div >= 36) {
            bclk_div = 28;
        } else if (bclk_div >= 34) {
            bclk_div = 27;
        } else if (bclk_div >= 33) {
            bclk_div = 26;
        } else if (bclk_div >= 32) {
            bclk_div = 25;
        } else if (bclk_div >= 30) {
            bclk_div = 24;
        } else if (bclk_div >= 25) {
            bclk_div = 23;
        } else if (bclk_div >= 24) {
            bclk_div = 22;
        } else {
            bclk_div = 21;
        }
    }

    return bclk_div;
}

static int codec_configure_clocks(struct device *dev,
        struct audio_codec_cfg *cfg)
{
    struct i2s_config *i2s = &cfg->dai_cfg.i2s;
    uint8_t val;
    int ratio, bclk_div, i;

#ifdef DEBUG
    printk("MCLK %u Hz PCM Rate: %u Hz\n", cfg->mclk_freq,
            i2s->frame_clk_freq);
#endif

    val = (CLK00_MCLK_ON | CLK00_BCLK_ON);
    codec_write_reg(dev, CLK00, val);

    val = (CLK01_DIV_PRE(0) || CLK01_MULT_PRE(2));
    codec_write_reg(dev, CLK01, val);

    ratio = cfg->mclk_freq / i2s->frame_clk_freq;

    for (i = 0; i < ARRAY_SIZE(es8311_clk_configs); i++) {
        struct clk_reg_table *entry = &es8311_clk_configs[i];
        if (entry->ratio == ratio) {
            apply_clk_configs(dev, entry);
            break;
        }
    }

    if (i == ARRAY_SIZE(es8311_clk_configs)) {
        LOG_ERR("Invalid ratio %d/%d = %d\n", cfg->mclk_freq, i2s->frame_clk_freq, ratio);
        return -EINVAL;
    }

    if (codec_is_bclk_out(&cfg->dai_cfg)) {
        bclk_div = calc_bclk_div(ratio, i2s);
        val = CLK05_DIV_BCLK(bclk_div);
        codec_write_reg(dev, CLK05, val);
        val = CLK06_DIV_LRCLK_H(ratio);
        codec_write_reg(dev, CLK06, val);
        val = CLK07_DIV_LRCLK_L(ratio);
        codec_write_reg(dev, CLK07, val);
#ifdef DEBUG
        printk("I2S Master BCLKDIV: %u\n", bclk_div);
#endif
    }

    return 0;
}

static int codec_configure_system(struct device *dev, audio_dai_cfg_t *cfg)
{
    uint8_t val;

    codec_write_reg(dev, SYS01, 0x00);

    /* 3.3V */
    val = (SYS05_VX2OFF | SYS05_VX1SEL);
#ifdef CONFIG_ES8311_OUT_DRV_HP
    /* high bias for HP */
    val |= (SYS05_DAC_IBIAS_SW | SYS05_IBIAS_SW(SYS05_IBIAS_SW_LVL_3));
#endif
    codec_write_reg(dev, SYS05, val);

    /* ?? */
    codec_write_reg(dev, SYS06, 0x7f);
#ifdef CONFIG_ES8311_OUT_DRV_HP
    codec_write_reg(dev, SYS08, SYS08_HPSW);
#else
    codec_write_reg(dev, SYS08, 0x00);
#endif

    codec_read_reg(dev, SYS02, &val);
    val &= ~(SYS02_PDN_ANA | SYS02_PDN_IBIASGEN | SYS02_PDN_ADCBIASGEN |\
            SYS02_PDN_ADCVREFGEN | SYS02_PDN_DACVREFGEN | SYS02_PDN_VREF);
    val &= ~SYS02_VMIDSEL_MASK;
    val |= SYS02_VMIDSEL(SYS02_VMIDSEL_S_NORM);
    codec_write_reg(dev, SYS02, val);

    val = (SYS04_LPPGA | SYS04_LPDACVRP);
    codec_write_reg(dev, SYS04, val);

    return 0;
}

static void codec_configure_output(struct device *dev)
{
    uint8_t val;

    val = (DAC06_RAMPRATE(0) | DAC06_EQBYPASS);
    codec_write_reg(dev, DAC06, val);

    codec_write_reg(dev, DAC01, 191); /* 0dB */
}

static void codec_configure_input(struct device *dev)
{
    uint8_t val;

    val = (SYS09_LINSEL | SYS09_PGAGAIN(9)); /* 27dB */
    codec_write_reg(dev, SYS09, val);

    codec_write_reg(dev, ADC00, 0x00);
    codec_write_reg(dev, ADC06, ADC06_HPFS1(0x0a));

    val = (ADC07_EQBYPASS | ADC07_HPF | ADC07_HPFS2(0x0a));
    codec_write_reg(dev, ADC07, val);

    codec_write_reg(dev, ADC02, 191); /* 0dB */
}

static int codec_set_output_dig_volume(struct device *dev, float vol)
{
    uint8_t val;
    float diff;

    if ((vol > CODEC_OUTPUT_DIG_VOLUME_MAX) ||
            (vol < CODEC_OUTPUT_DIG_VOLUME_MIN)) {
        LOG_ERR("Invalid volume %*f dB", vol);
        return -EINVAL;
    }

    diff = vol - CODEC_OUTPUT_DIG_VOLUME_MIN;
    val = (uint8_t)(diff * 2);

    codec_write_reg(dev, DAC01, val);

    return 0;
}

static int codec_get_output_dig_volume(struct device *dev, float *min, float *cur, float *max)
{
    uint8_t val;
    float diff;

    *max = CODEC_OUTPUT_DIG_VOLUME_MAX;
    *min = CODEC_OUTPUT_DIG_VOLUME_MIN;

    codec_read_reg(dev, DAC01, &val);

    diff = (float)val / 2;

    *cur = CODEC_OUTPUT_DIG_VOLUME_MIN + diff;

    return 0;
}

#if (LOG_LEVEL >= LOG_LEVEL_DEBUG)
static void codec_read_all_regs(struct device *dev)
{
    uint8_t val;

    codec_read_reg(dev, RESET, &val);
    codec_read_reg(dev, CLK00, &val);
    codec_read_reg(dev, CLK01, &val);
    codec_read_reg(dev, CLK02, &val);
    codec_read_reg(dev, CLK03, &val);
    codec_read_reg(dev, CLK04, &val);
    codec_read_reg(dev, CLK05, &val);
    codec_read_reg(dev, CLK06, &val);
    codec_read_reg(dev, CLK07, &val);
    codec_read_reg(dev, SDP00, &val);
    codec_read_reg(dev, SDP01, &val);
    codec_read_reg(dev, SYS00, &val);
    codec_read_reg(dev, SYS01, &val);
    codec_read_reg(dev, SYS02, &val);
    codec_read_reg(dev, SYS03, &val);
    codec_read_reg(dev, SYS04, &val);
    codec_read_reg(dev, SYS05, &val);
    codec_read_reg(dev, SYS06, &val);
    codec_read_reg(dev, SYS07, &val);
    codec_read_reg(dev, SYS08, &val);
    codec_read_reg(dev, SYS09, &val);
    codec_read_reg(dev, ADC00, &val);
    codec_read_reg(dev, ADC01, &val);
    codec_read_reg(dev, ADC02, &val);
    codec_read_reg(dev, ADC03, &val);
    codec_read_reg(dev, ADC04, &val);
    codec_read_reg(dev, ADC05, &val);
    codec_read_reg(dev, ADC06, &val);
    codec_read_reg(dev, ADC07, &val);
    codec_read_reg(dev, ADC08, &val);
    codec_read_reg(dev, DAC00, &val);
    codec_read_reg(dev, DAC01, &val);
    codec_read_reg(dev, DAC02, &val);
    codec_read_reg(dev, DAC03, &val);
    codec_read_reg(dev, DAC04, &val);
    codec_read_reg(dev, DAC05, &val);
    codec_read_reg(dev, DAC06, &val);
    codec_read_reg(dev, GPIO, &val);
    codec_read_reg(dev, GP, &val);
    codec_read_reg(dev, I2C, &val);
    codec_read_reg(dev, FLAG, &val);
    codec_read_reg(dev, CHIP00, &val);
    codec_read_reg(dev, CHIP01, &val);
    codec_read_reg(dev, CHIP02, &val);
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

static int es8311_codec_probe(struct device *dev)
{
    struct codec_driver_data *priv;
    struct i2c_cfg i2c;
    struct file *file;
    char buf[32];
    u16 addr = 0x18;
    struct pinctrl_pin_map *pmap;
    int ret;

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

    pmap = pinctrl_lookup_platform_pinmap(dev, "ctrl");
    if (pmap != NULL) {
        ret = gpio_request(dev, "ctrl", pmap->pin);
        if (ret < 0) {
            printk("Error %d from gpio_request\n", ret);
            return -EINVAL;
        }
        ret = gpio_direction_output(pmap->pin, 1);
        if (ret < 0) {
            printk("Error %d from gpio_direction_output\n", ret);
            return -EINVAL;
        }
    }

#ifdef CONFIG_ES8311_I2C_ADDR_HI
    addr |= 0x1;
#endif

    i2c.role = I2C_ROLE_MASTER;
    i2c.addr_len = I2C_ADDR_LEN_7BIT;
    i2c.bitrate = 100 * 1000;
    i2c.dma_en = 0;
    i2c.pull_up_en = 1;

    priv->sem = osSemaphoreNew(1, 0, NULL);

    i2c_configure(priv->bus, &i2c, i2c_notify, priv);

    priv->addr = addr;
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

int es8311_codec_shutdown(struct device *dev)
{
    struct codec_driver_data *priv = dev->driver_data;

    free(priv);

    return 0;
}

#ifdef CONFIG_PM_DM
static int es8311_codec_suspend(struct device *dev, u32 *idle)
{
    return 0;
}

static int es8311_codec_resume(struct device *dev)
{
    return 0;
}
#endif

static struct audio_codec_ops es8311_codec_ops = {
    .configure			= codec_configure,
    .start_output		= codec_start_output,
    .stop_output		= codec_stop_output,
    .start_input		= codec_start_input,
    .stop_input		    = codec_stop_input,
    .set_property		= codec_set_property,
    .apply_properties	= codec_apply_properties,
    .get_property		= codec_get_property,
    .dump               = codec_dump,
};

static declare_driver(audio) = {
    .name = "es8311",
    .probe      = es8311_codec_probe,
    .shutdown   = es8311_codec_shutdown,
#ifdef CONFIG_PM_DM
    .suspend    = es8311_codec_suspend,
    .resume     = es8311_codec_resume,
#endif
    .ops        = &es8311_codec_ops,
};

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
#include <hal/init.h>
#include <hal/kernel.h>
#include <hal/pinctrl.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/i2s.h>
#include <hal/kmem.h>
#include <hal/dma.h>
#include <hal/timer.h>

#include "vfs.h"
#include "mmap.h"
#include "mem_slab.h"

#define OFT_I2S_CFG0                       	0x00
#define OFT_I2S_CFG1                       	0x04
#define OFT_I2S_CFG2                       	0x08
#define OFT_I2S_CFG3                       	0x0c
#define OFT_I2S_CFG4                       	0x10
#define OFT_I2S_CFG5                       	0x14

/* CFG0 is configured by soc.
 * These are just for reference.
 */
#define I2S_PYTHON_CFG0_AHB_EN				0x01
#define I2S_PYTHON_CFG0_CLK_EN				0x02
#define I2S_PYTHON_CFG0_SW_RST				0x10
#define I2S_PYTHON_CFG0_CLK_SEL				0x300

#define I2S_PYTHON_CFG1_IN_MODE				0x01
#define I2S_PYTHON_CFG1_IN_MODE_16			0x00
#define I2S_PYTHON_CFG1_IN_MODE_20_24		0x01
#define I2S_PYTHON_CFG1_OUT_MODE			0x02
#define I2S_PYTHON_CFG1_OUT_MODE_16			0x00
#define I2S_PYTHON_CFG1_OUT_MODE_20_24		0x02
#define I2S_PYTHON_CFG1_MBCLK_SEL			0x04
#define I2S_PYTHON_CFG1_CLK_EN				0x08
#define I2S_PYTHON_CFG1_CLK_DIV2			0x10
#define I2S_PYTHON_CFG1_BCLKINV				0x20
#define I2S_PYTHON_CFG1_DAC_MS				0x40
#define I2S_PYTHON_CFG1_ADC_MS				0x80
#define I2S_PYTHON_CFG1_LRP					0x100
#define I2S_PYRHON_CFG1_LRP_DSP_A			0x000
#define I2S_PYRHON_CFG1_LRP_DSP_B			0x100
#define I2S_PYRHON_CFG1_LRP_NOT_INVERT		0x000
#define I2S_PYRHON_CFG1_LRP_INVERT			0x100
#define I2S_PYTHON_CFG1_FORMAT				0x600
#define I2S_PYTHON_CFG1_FORMAT_RJ			0x000
#define I2S_PYTHON_CFG1_FORMAT_LJ			0x200
#define I2S_PYTHON_CFG1_FORMAT_IIS			0x400
#define I2S_PYTHON_CFG1_FORMAT_DSP			0x600
#define I2S_PYTHON_CFG1_WL					0x1800
#define I2S_PYTHON_CFG1_WL_16				0x0000
#define I2S_PYTHON_CFG1_WL_20				0x0800
#define I2S_PYTHON_CFG1_WL_24				0x1000
#define I2S_PYTHON_CFG1_LRSWAP				0x2000
#define I2S_PYTHON_CFG1_MBCLK_DIV			0x3c000
#define I2S_PYTHON_CFG1_MBCLK_DIV_S     	14
#define I2S_PYTHON_CFG2_ADC_MLRCLK_DIV		0x00000fff
#define I2S_PYTHON_CFG2_DAC_MLRCLK_DIV		0x0fff0000
#define I2S_PYTHON_CFG3_AHB_TXFIFO_NUM		0xf
#define I2S_PYTHON_CFG3_TXFIFO_TRIG_NUM		0xf0
#define I2S_PYTHON_CFG3_TXFIFO_TRIG_NUM_S	4
#define I2S_PYTHON_CFG3_AHB_RXFIFO_NUM		0xf00
#define I2S_PYTHON_CFG3_RXFIFO_TRIG_NUM		0xf000
#define I2S_PYTHON_CFG3_RST_FIFO_MANUAL		0x10000
#define I2S_PYTHON_CFG4_TX_RPTR_CLR			0x1
#define I2S_PYTHON_CFG4_TX_RPTR_EN			0x2
#define I2S_PYTHON_CFG4_TX_RPTR				0xfffc
#define I2S_PYTHON_CFG4_TX_MAX				0x3fff0000
#define I2S_PYTHON_CFG5_RX_WPTR_CLR			0x1
#define I2S_PYTHON_CFG5_RX_WPTR_EN			0x2
#define I2S_PYTHON_CFG5_RX_WPTR				0xfffc
#define I2S_PYTHON_CFG5_RX_MAX				0x3fff0000

#define i2s_writel(v, o)                 writel(v, dev->base[0] + o)
#define i2s_readl(o)                     readl(dev->base[0] + o)

#define MODULO_INC(val, max) { val = (++val < max) ? val : 0; }

#define DEBUG

#ifdef DEBUG
#define LOG_ERR(...)					printk(__VA_ARGS__)
#else
#define LOG_ERR(...)					(void)0
#endif

static const char *i2s_pin_name[] = {
	[0] 	= "din",
	[1] 	= "wclk",
	[2] 	= "dout",
	[3] 	= "bclk",
	[4] 	= "mclk"
};

struct queue_item {
	void *mem_block;
	size_t size;
};

/* Minimal ring buffer implementation */
struct ring_buf {
	struct queue_item *buf;
	u16 len;
	u16 head;
	u16 tail;
};

struct stream {
	int32_t state;
	enum i2s_dir dir;
	osSemaphoreId_t sem;

	struct device *dev;
	struct device *dev_dma;
	int dma_channel;
	struct dma_ctrl dma_ctrl;
	bool tx_stop_for_drain;

	struct i2s_config cfg;
	struct ring_buf mem_block_queue;
	void *mem_block;
	bool last_block;
	bool master;
	int (*stream_start)(struct stream *, struct device *dev);
	void (*stream_disable)(struct stream *, struct device *dev);
	void (*queue_drop)(struct stream *);
};

struct i2s_driver_data {
	struct stream rx;
	struct stream tx;
	struct fops devfs_ops;
	struct device *dev;
	struct clk *mclk;
	struct pinctrl_pin_map *pmap[5];
};

struct queue_item rx_ring_buf[CONFIG_I2S_PYTHON_RX_BLOCK_COUNT + 1];
struct queue_item tx_ring_buf[CONFIG_I2S_PYTHON_TX_BLOCK_COUNT + 1];

static unsigned int div_round_closest(u32 dividend, u32 divisor)
{
	return (dividend + (divisor / 2U)) / divisor;
}

static bool queue_is_empty(struct ring_buf *rb)
{
	u32 flags;

	local_irq_save(flags);

	if (rb->tail != rb->head) {
		/* Ring buffer is not empty */
		local_irq_restore(flags);
		return false;
	}

	local_irq_restore(flags);

	return true;
}

/*
 * Get data from the queue
 */
static int queue_get(struct ring_buf *rb, void **mem_block, size_t *size)
{
	u32 flags;

	local_irq_save(flags);

	if (queue_is_empty(rb) == true) {
		local_irq_restore(flags);
		return -ENOMEM;
	}

	*mem_block = rb->buf[rb->tail].mem_block;
	*size = rb->buf[rb->tail].size;
	MODULO_INC(rb->tail, rb->len);

	local_irq_restore(flags);

	return 0;
}

/*
 * Put data in the queue
 */
static int queue_put(struct ring_buf *rb, void *mem_block, size_t size)
{
	uint16_t head_next;
	u32 flags;

	local_irq_save(flags);

	head_next = rb->head;
	MODULO_INC(head_next, rb->len);

	if (head_next == rb->tail) {
		/* Ring buffer is full */
		local_irq_restore(flags);
		return -ENOMEM;
	}

	rb->buf[rb->head].mem_block = mem_block;
	rb->buf[rb->head].size = size;
	rb->head = head_next;

	local_irq_restore(flags);

	return 0;
}

static int python_i2s_reset(struct device *dev)
{
	u32 v;

	v = i2s_readl(OFT_I2S_CFG0);
	v |= I2S_PYTHON_CFG0_SW_RST;
	i2s_writel(v, OFT_I2S_CFG0);

	return 0;
}

static void python_i2s_enable_clock(struct device *dev, bool en)
{
	u32 v;

	v = i2s_readl(OFT_I2S_CFG1);
    if (en) {
	    v |= I2S_PYTHON_CFG1_CLK_EN;
    } else {
	    v &= ~I2S_PYTHON_CFG1_CLK_EN;
    }
	i2s_writel(v, OFT_I2S_CFG1);
}

static u32 diff(u32 val1, u32 val2)
{
    if (val1 > val2) {
        return val1 - val2;
    } else {
        return val2 - val1;
    }
}

static int python_i2s_set_clock(struct device *dev, u32 channel_length,
        u32 num_channels, u32 frame_clk_freq, bool tx)
{
	struct i2s_driver_data *priv = dev->driver_data;
	int div1 = 0, div2 = 0;
	u32 v;
	u32 mask = tx ? I2S_PYTHON_CFG2_DAC_MLRCLK_DIV : I2S_PYTHON_CFG2_ADC_MLRCLK_DIV;
	u32 shift = tx ? 16 : 0;
    u32 mclk_freq = clk_get_rate(priv->mclk);
    u32 gap, min_gap = (u32)(-1);

    for (int i = 1; ; i++) {
        u32 bit_clk_freq, d1, d2;
        d1 = i * 2;
        bit_clk_freq = div_round_closest(mclk_freq, d1);
        if (bit_clk_freq <= (channel_length * num_channels * frame_clk_freq)) {
            break;
        }
        d2 = div_round_closest(bit_clk_freq, frame_clk_freq);
        gap = diff(bit_clk_freq / d2, frame_clk_freq);
        if (gap <= min_gap) { /* If same, lower clock, i.e., higher divider would be preferred. */
            min_gap = gap;
            div1 = d1;
            div2 = d2;
            /* Don't break, but keep searching for lower bit clock freq.
             */
        }
    }

    if (!div1 || !div2) {
        LOG_ERR("Invalid clock configuration.\n");
		return -EINVAL;
    }

	div1 >>= 1;

	if (div1 > 15)
		return -EINVAL;

	v = i2s_readl(OFT_I2S_CFG1);
	v &= ~I2S_PYTHON_CFG1_MBCLK_DIV;
	v |= (div1 << I2S_PYTHON_CFG1_MBCLK_DIV_S);
	i2s_writel(v, OFT_I2S_CFG1);

	div2 -= 1;

	if (div2 > 0x2000)
		return -EINVAL;

	v = i2s_readl(OFT_I2S_CFG2);
	v &= ~mask;
	v |= (div2 << shift);
	i2s_writel(v, OFT_I2S_CFG2);

	return 0;
}

static int python_i2s_enable_mclk(struct device *dev, bool en)
{
#if 0 /* XXX: I2S_CLK_EN must be 1 even while in slave mode. */
	clk_enable(priv->mclk, en);
#endif

	return 0;
}

enum {
	MODE_20_24 = 0,
	MODE_16 = 1,
} PYTHON_I2S_MODE;

static int python_i2s_set_master(struct device *dev, bool tx, bool master)
{
	u32 v;

#if 0/* XXX: python-i2s does not allow different mode per direction. */
	u32 mask = tx ? I2S_PYTHON_CFG1_DAC_MS : I2S_PYTHON_CFG1_ADC_MS;
    u32 val = tx ? I2S_PYTHON_CFG1_DAC_MS : I2S_PYTHON_CFG1_ADC_MS;
#else
	u32 mask = (I2S_PYTHON_CFG1_DAC_MS | I2S_PYTHON_CFG1_ADC_MS);
    u32 flag = (I2S_PYTHON_CFG1_DAC_MS | I2S_PYTHON_CFG1_ADC_MS);
#endif

    (void) tx;

	v = i2s_readl(OFT_I2S_CFG1);
	v &= ~mask;
	if (master) {
		v |= flag;
	}
	i2s_writel(v, OFT_I2S_CFG1);

    /* Choose which block, DAC or ADC, will be fed with BCLK
     * while in slave mode.
     */
    /* XXX: while in master mode, adc_bclk and dac_bclk
     *      derive from the same source, mclk.
     *      This bit doesn't matter.
     */
    if (!master) {
        v = i2s_readl(OFT_I2S_CFG1);
        v &= ~I2S_PYTHON_CFG1_MBCLK_SEL;
        if (tx) { /* DAC */
            v |= I2S_PYTHON_CFG1_MBCLK_SEL;
        }
        i2s_writel(v, OFT_I2S_CFG1);
    }

	return 0;
}

static int python_i2s_set_mode(struct device *dev, bool tx, int mode)
{
	u32 v;
	u32 mask = tx ? I2S_PYTHON_CFG1_OUT_MODE : I2S_PYTHON_CFG1_IN_MODE;
	u32 m_20_24 = tx ? I2S_PYTHON_CFG1_OUT_MODE_20_24 : I2S_PYTHON_CFG1_IN_MODE_20_24;
	u32 m_16 = tx ? I2S_PYTHON_CFG1_OUT_MODE_16 : I2S_PYTHON_CFG1_IN_MODE_16;

	v = i2s_readl(OFT_I2S_CFG1);
	v &= ~mask;
	if (mode == MODE_20_24) {
		v |= m_20_24;
	} else if (mode == MODE_16) {
		v |= m_16;
	} else {
		return -EINVAL;
	}
	i2s_writel(v, OFT_I2S_CFG1);

	return 0;
}

enum {
	WL_16 = 0,
	WL_20 = 1,
	WL_24 = 2
} PYTHON_I2S_WL;

static int python_i2s_set_wordlengh(struct device *dev, int wl)
{
	u32 v;

	v = i2s_readl(OFT_I2S_CFG1);
	v &= ~I2S_PYTHON_CFG1_WL;
	if (wl == WL_16) {
		v |= I2S_PYTHON_CFG1_WL_16;
	} else if (wl == WL_20) {
		v |= I2S_PYTHON_CFG1_WL_20;
	} else if (wl == WL_24) {
		v |= I2S_PYTHON_CFG1_WL_24;
	} else {
		return -EINVAL;
	}
	i2s_writel(v, OFT_I2S_CFG1);

	return 0;
}

enum {
	FMT_RJ  = 0,
	FMT_LJ  = 1,
	FMT_I2S = 2,
	FMT_DSP = 3
} PYTHON_I2S_FMT;

static int python_i2s_set_format(struct device *dev, int fmt)
{
	u32 v;

	v = i2s_readl(OFT_I2S_CFG1);
	v &= ~I2S_PYTHON_CFG1_FORMAT;
	if (fmt == FMT_RJ) {
		v |= I2S_PYTHON_CFG1_FORMAT_RJ;
	} else if (fmt == FMT_LJ) {
		v |= I2S_PYTHON_CFG1_FORMAT_LJ;
	} else if (fmt == FMT_I2S) {
		v |= I2S_PYTHON_CFG1_FORMAT_IIS;
	} else if (fmt == FMT_DSP) {
		v |= I2S_PYTHON_CFG1_FORMAT_DSP;
	} else {
		return -EINVAL;
	}
	i2s_writel(v, OFT_I2S_CFG1);

	return 0;
}

static int python_i2s_set_clk_polarity(struct device *dev, bool invert)
{
	u32 v;

	v = i2s_readl(OFT_I2S_CFG1);
	v &= ~I2S_PYTHON_CFG1_BCLKINV;
	if (invert) {
		v |= I2S_PYTHON_CFG1_BCLKINV;
	}
	i2s_writel(v, OFT_I2S_CFG1);

	return 0;
}

static int python_i2s_configure(struct device *dev, enum i2s_dir dir,
		const struct i2s_config *i2s_cfg)
{
	struct i2s_driver_data *priv = dev->driver_data;

	/* For words greater than 16-bit the channel length is considered 32-bit */
	const u32 channel_length = i2s_cfg->word_size > 16U ? 32U : 16U;
#if 0 /* python-i2s always enables 2 channels. */
	/*
	 * comply with the i2s_config driver remark:
	 * When I2S data format is selected parameter channels is ignored,
	 * number of words in a frame is always 2.
	 */
	const u32 num_channels = (i2s_cfg->format & I2S_FMT_DATA_FORMAT_MASK) \
							 == I2S_FMT_DATA_FORMAT_I2S ? 2U : i2s_cfg->channels;
#else
	const u32 num_channels = 2;
#endif
	struct stream *stream;
	bool enable_mck;
	int ret;

	/* Ensure that the k_mem_slab's slabs are large enough for the
	 * specified block size
	 */
	if (i2s_cfg->block_size > i2s_cfg->mem_slab->info.block_size) {
		ret = -EINVAL;
		goto out;
	}

	if (dir == I2S_DIR_RX) {
		stream = &priv->rx;
	} else if (dir == I2S_DIR_TX) {
		stream = &priv->tx;
	} else if (dir == I2S_DIR_BOTH) {
		return -ENOSYS;
	} else {
		LOG_ERR("Either RX or TX direction must be selected\n");
		return -EINVAL;
	}

	if (stream->state != I2S_STATE_NOT_READY &&
	    stream->state != I2S_STATE_READY) {
		LOG_ERR("[%s, %d] invalid state %d\n", __func__, __LINE__,
				stream->state);
		return -EINVAL;
	}

	stream->master = true;
	if (i2s_cfg->options & I2S_OPT_FRAME_CLK_SLAVE ||
	    i2s_cfg->options & I2S_OPT_BIT_CLK_SLAVE) {
		stream->master = false;
	}

	if (i2s_cfg->frame_clk_freq == 0U) {
		stream->queue_drop(stream);
		memset(&stream->cfg, 0, sizeof(struct i2s_config));
		stream->state = I2S_STATE_NOT_READY;
		return 0;
	}

	memcpy(&stream->cfg, i2s_cfg, sizeof(struct i2s_config));

	/* conditions to enable master clock output */
	enable_mck = stream->master;


	python_i2s_enable_clock(dev, false);

	/* reset I2S block to start over */
	ret = python_i2s_reset(dev);
	if (ret < 0) {
		return ret;
	}

	/* set I2S bitclock */
	ret = python_i2s_set_clock(dev, channel_length, num_channels,
            i2s_cfg->frame_clk_freq, (dir == I2S_DIR_TX));
	if (ret < 0) {
		return ret;
	}


	if (enable_mck) {
		python_i2s_enable_mclk(dev, true);
	} else {
		python_i2s_enable_mclk(dev, false);
	}

	/*
	 * set I2S Data Format
	 * 16-bit data extended on 32-bit channel length excluded
	 */
	switch (i2s_cfg->word_size) {
	case 16U:
		python_i2s_set_wordlengh(dev, WL_16);
		python_i2s_set_mode(dev, (dir == I2S_DIR_TX), MODE_16);
		break;
	case 20U:
		python_i2s_set_wordlengh(dev, WL_20);
		python_i2s_set_mode(dev, (dir == I2S_DIR_TX), MODE_20_24);
		break;
	case 24U:
		python_i2s_set_wordlengh(dev, WL_24);
		python_i2s_set_mode(dev, (dir == I2S_DIR_TX), MODE_20_24);
		break;
	default:
		LOG_ERR("invalid word size\n");
		return -EINVAL;
	}

	/* set I2S Standard */
	switch (i2s_cfg->format & I2S_FMT_DATA_FORMAT_MASK) {
	case I2S_FMT_DATA_FORMAT_I2S:
		python_i2s_set_format(dev, FMT_I2S);
		break;

	case I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED:
		python_i2s_set_format(dev, FMT_LJ);
		break;

	case I2S_FMT_DATA_FORMAT_RIGHT_JUSTIFIED:
		python_i2s_set_format(dev, FMT_RJ);
		break;

	default:
		LOG_ERR("Unsupported I2S data format\n");
		return -EINVAL;
	}

	/* set I2S clock polarity */
	if ((i2s_cfg->format & I2S_FMT_CLK_FORMAT_MASK) == I2S_FMT_BIT_CLK_INV)
		python_i2s_set_clk_polarity(dev, true);
	else
		python_i2s_set_clk_polarity(dev, false);

	stream->state = I2S_STATE_READY;
out:
	return ret;
}

static const struct i2s_config *python_i2s_config_get(struct device *dev,
		enum i2s_dir dir)
{
	struct i2s_driver_data *priv = dev->driver_data;
	struct stream *stream;

	if (dir == I2S_DIR_RX) {
		stream = &priv->rx;
	} else if (dir == I2S_DIR_TX) {
		stream = &priv->tx;
	} else if (dir == I2S_DIR_BOTH) {
		return NULL;
	} else {
		LOG_ERR("Either RX or TX direction must be selected\n");
		return NULL;
	}

	return &stream->cfg;
}

static int python_i2s_state_get(struct device *dev, enum i2s_dir dir, enum i2s_state *state)
{
	struct i2s_driver_data *priv = dev->driver_data;
	struct stream *stream;

	if (dir == I2S_DIR_RX) {
		stream = &priv->rx;
	} else if (dir == I2S_DIR_TX) {
		stream = &priv->tx;
	} else if (dir == I2S_DIR_BOTH) {
		return -ENOSYS;
	} else {
		LOG_ERR("Either RX or TX direction must be selected\n");
		return -EINVAL;
	}

	*state = stream->state;
	return 0;
}

static int python_i2s_buf_read(struct device *dev, void **mem_block, size_t *size)
{
	struct i2s_driver_data *priv = dev->driver_data;
    struct stream *stream = &priv->rx;
	int ret;

	if (stream->state == I2S_STATE_NOT_READY) {
		LOG_ERR("[%s, %d] invalid state %d\n", __func__, __LINE__,
				stream->state);
		return -EIO;
	}

	if (stream->state != I2S_STATE_ERROR) {
		ret = osSemaphoreAcquire(stream->sem, pdMS_TO_TICKS(stream->cfg.timeout));
		if (ret != osOK) {
			return -ENODATA;
		}
	}

	/* Get data from the beginning of RX queue */
	ret = queue_get(&stream->mem_block_queue, mem_block, size);
	if (ret < 0) {
		return -EIO;
	}

	return 0;
}

static int python_i2s_buf_write(struct device *dev, void *mem_block, size_t size)
{
	struct i2s_driver_data *priv = dev->driver_data;
    struct stream *stream = &priv->tx;
	int ret;

	if (stream->state != I2S_STATE_RUNNING &&
        stream->state != I2S_STATE_STARTING &&
	    stream->state != I2S_STATE_READY) {
		LOG_ERR("[%s, %d] invalid state %d\n", __func__, __LINE__,
				stream->state);

		return -EIO;
	}

	ret = osSemaphoreAcquire(priv->tx.sem, pdMS_TO_TICKS(stream->cfg.timeout));
	if (ret != osOK) {
		return -EIO;
	}

	/* Add data to the end of the TX queue */
	queue_put(&stream->mem_block_queue, mem_block, size);

    if (stream->state == I2S_STATE_STARTING) {
        /* Delayed start */
        if (stream->stream_start(stream, dev) == 0) {
            stream->state = I2S_STATE_RUNNING;
        }
    }

	return 0;
}

static int python_i2s_trigger(struct device *dev, enum i2s_dir dir, enum i2s_trigger_cmd cmd)
{
	struct i2s_driver_data *priv = dev->driver_data;
	struct stream *stream;
	u32 flags;
	int ret;
    int retry;

	if (dir == I2S_DIR_RX) {
		stream = &priv->rx;
	} else if (dir == I2S_DIR_TX) {
		stream = &priv->tx;
	} else if (dir == I2S_DIR_BOTH) {
		return -ENOSYS;
	} else {
		LOG_ERR("Either RX or TX direction must be selected\n");
		return -EINVAL;
	}

	switch (cmd) {
	case I2S_TRIGGER_START:
        /* Wait for the previous data transaction to finish. */
        retry = 10000;
        while (stream->state == I2S_STATE_STOPPING && retry-- > 0) {
            udelay(100);
        }

		if (stream->state != I2S_STATE_READY) {
			LOG_ERR("START trigger: invalid state %d\n",
				    stream->state);
			return -EIO;
		}

		assert(stream->mem_block == NULL);

		ret = stream->stream_start(stream, dev);
        if (stream->dir == I2S_DIR_TX && ret == -ENOMEM) {
            /* No data was queued yet. */
            stream->state = I2S_STATE_STARTING;
            stream->last_block = false;
            break;
        } else if (ret < 0) {
			LOG_ERR("START trigger failed %d\n", ret);
			return ret;
		}

		stream->state = I2S_STATE_RUNNING;
		stream->last_block = false;
		break;

	case I2S_TRIGGER_STOP:
		local_irq_save(flags);
		if (stream->state != I2S_STATE_RUNNING) {
			local_irq_restore(flags);
			LOG_ERR("STOP trigger: invalid state %d\n",
					stream->state);
			return -EIO;
		}
do_trigger_stop:
		if (dma_ch_is_busy(stream->dev_dma, stream->dma_channel)) {
			stream->state = I2S_STATE_STOPPING;
			/*
			 * Indicate that the transition to I2S_STATE_STOPPING
			 * is triggered by STOP command
			 */
			stream->tx_stop_for_drain = false;
		} else {
			stream->stream_disable(stream, dev);
			stream->state = I2S_STATE_READY;
			stream->last_block = true;
		}
		local_irq_restore(flags);
		break;

	case I2S_TRIGGER_DRAIN:
		local_irq_save(flags);
		if (stream->state != I2S_STATE_RUNNING
                && stream->state != I2S_STATE_STARTING) {
			local_irq_restore(flags);
			LOG_ERR("DRAIN trigger: invalid state %d\n",
					stream->state);
			return -EIO;
		}

		if (dir == I2S_DIR_TX) {
			if ((queue_is_empty(&stream->mem_block_queue) == false) ||
						(dma_ch_is_busy(stream->dev_dma, stream->dma_channel))) {
				stream->state = I2S_STATE_STOPPING;
				/*
				 * Indicate that the transition to I2S_STATE_STOPPING
				 * is triggered by DRAIN command
				 */
				stream->tx_stop_for_drain = true;
			} else {
				stream->stream_disable(stream, dev);
				stream->state = I2S_STATE_READY;
			}
		} else if (dir == I2S_DIR_RX) {
			goto do_trigger_stop;
		} else {
			LOG_ERR("Unavailable direction\n");
			return -EINVAL;
		}
		local_irq_restore(flags);
		break;

	case I2S_TRIGGER_DROP:
		local_irq_save(flags);
		if (stream->state == I2S_STATE_NOT_READY) {
			LOG_ERR("DROP trigger: invalid state %d\n",
					stream->state);
			local_irq_restore(flags);
			return -EIO;
		}
		stream->stream_disable(stream, dev);
		stream->queue_drop(stream);
		stream->state = I2S_STATE_READY;
		local_irq_restore(flags);
		break;

	case I2S_TRIGGER_PREPARE:
		if (stream->state != I2S_STATE_ERROR) {
			LOG_ERR("PREPARE trigger: invalid state %d\n",
					stream->state);
			return -EIO;
		}
		stream->state = I2S_STATE_READY;
		stream->queue_drop(stream);
		break;

	default:
		LOG_ERR("Unsupported trigger command\n");
		return -EINVAL;
	}

	return 0;
}

int python_i2s_open(struct file *file)
{
	/* i2s configure will make stream state from I2S_STATE_NOT_READY to I2S_STATE_READY */
	return 0;
}

int python_i2s_release(struct file *file)
{
	struct device *dev = file->f_priv;
	struct i2s_driver_data *priv = dev->driver_data;

	if ((priv->tx.state == I2S_STATE_STARTING) ||
		(priv->tx.state == I2S_STATE_RUNNING) ||
		(priv->tx.state == I2S_STATE_STOPPING)) {
		return -EINPROGRESS;
	}

	if ((priv->rx.state == I2S_STATE_STARTING) ||
		(priv->rx.state == I2S_STATE_RUNNING) ||
		(priv->rx.state == I2S_STATE_STOPPING)) {
		return -EINPROGRESS;
	}

	priv->tx.state = I2S_STATE_NOT_READY;
	priv->rx.state = I2S_STATE_NOT_READY;

	return 0;
}

static void config_dma(struct device *dev, struct stream *stream)
{
	struct dma_ctrl *ctrl = &stream->dma_ctrl;
	bool tx = stream->dir == I2S_DIR_TX ? true : false;

	stream->dev_dma = device_get_by_name("dmac.1");
	if (!stream->dev_dma) {
		LOG_ERR("Not support DMA\n");
	}

	stream->dma_channel = -1;

	if (tx) {
		ctrl->src_mode = DMA_MODE_NORMAL;
		ctrl->dst_mode = DMA_MODE_HANDSHAKE;
		ctrl->src_req = 0;
		ctrl->dst_req = DMA1_HW_REQ_I2S_TX;
		ctrl->src_addr_ctrl = DMA_ADDR_CTRL_INCREMENT;
		ctrl->dst_addr_ctrl = DMA_ADDR_CTRL_FIXED;
		ctrl->src_width = DMA_WIDTH_WORD;
		ctrl->dst_width = DMA_WIDTH_WORD;
		ctrl->intr_mask = 0;
		ctrl->src_burst_size = DMA_SRC_BURST_SIZE_1;
	} else {
		ctrl->src_mode = DMA_MODE_HANDSHAKE;
		ctrl->dst_mode = DMA_MODE_NORMAL;
		ctrl->src_req = DMA1_HW_REQ_I2S_RX;
		ctrl->dst_req = 0;
		ctrl->src_addr_ctrl = DMA_ADDR_CTRL_FIXED;
		ctrl->dst_addr_ctrl = DMA_ADDR_CTRL_INCREMENT;
		ctrl->src_width = DMA_WIDTH_WORD;
		ctrl->dst_width = DMA_WIDTH_WORD;
		ctrl->intr_mask = 0;
		ctrl->src_burst_size = DMA_SRC_BURST_SIZE_1;
	}
}

static int dma_rx_callback(void *priv, dma_isr_status status);
static int dma_tx_callback(void *priv, dma_isr_status status);

static void setup_dma_desc(struct device *dev, struct dma_desc_chain *desc, u8 *buf,
		int len, bool tx)
{
	if (tx) {
		desc->src_addr = (uint32_t)buf;
		desc->dst_addr = (uint32_t)(dev->base[1]);
		desc->len = len / sizeof(u32);
	} else {
		desc->src_addr = (uint32_t)(dev->base[1]);
		desc->dst_addr = (uint32_t)buf;
		desc->len = len / sizeof(u32);
	}
}

static int start_dma(struct device *dev, struct stream *stream, u8 *buf, int len)
{
	bool tx = stream->dir == I2S_DIR_TX ? true : false;
	struct dma_desc_chain dma_desc;
	int ret;

	if (!stream->dev_dma) {
		return -EIO;
	}

	setup_dma_desc(dev, &dma_desc, buf, len, tx);

	ret = dma_copy_hw(stream->dev_dma, true, &stream->dma_ctrl, &dma_desc, 1,
			tx ? dma_tx_callback : dma_rx_callback,
			stream, &stream->dma_channel);
	if (ret) {
		return ret;
	}

	return 0;
}

static int reload_dma(struct device *dev, struct stream *stream, u8 *buf, int len)
{
	bool tx = stream->dir == I2S_DIR_TX ? true : false;
	struct dma_desc_chain dma_desc;
	int ret;

	if (!stream->dev_dma) {
		return -EIO;
	}

	setup_dma_desc(dev, &dma_desc, buf, len, tx);

	ret = dma_reload(stream->dev_dma, stream->dma_channel, &dma_desc, 1);
	if (ret) {
		return ret;
	}

	return 0;
}

static void hexdump_nz_k(void *buf, size_t buflen) __maybe_unused;
static void hexdump_nz_k(void *buf, size_t buflen)
{
	int i;
	u32 *p;

	for (i = 0, p = (u32 *)buf; i < buflen / 4; i++, p++) {
		if (p[0]) {
			printk("++\n");
			printk("0x%08x\n", p[0]);
			printk("0x%08x\n", p[1]);
			printk("0x%08x\n", p[2]);
			printk("0x%08x\n", p[3]);
			printk("--\n");
			break;
		}
	}
}

/* This function is executed in the interrupt context */
static int dma_rx_callback(void *priv, dma_isr_status status)
{
	struct stream *stream = (struct stream *)priv;
	struct device *dev = stream->dev;
	void *mblk_tmp = NULL;
	int ret;

    if (status == DMA_STATUS_ABORTED || stream->state == I2S_STATE_READY) {
        /* Already aborted. Do nothing. */
        return 0;
    }
	if (status < 0) {
		ret = -EIO;
		stream->state = I2S_STATE_ERROR;
		goto rx_disable;
	}

	assert(stream->mem_block != NULL);

	/* Stop reception if there was an error */
	if (stream->state == I2S_STATE_ERROR) {
		ret = -EIO;
		goto rx_disable;
	}

	mblk_tmp = stream->mem_block;
#if 0
	hexdump_nz_k(mblk_tmp, stream->cfg.block_size);
#endif

	/* Prepare to receive the next data block */
	ret = mem_slab_alloc(stream->cfg.mem_slab, &stream->mem_block, 0);
	if (ret < 0) {
		stream->state = I2S_STATE_ERROR;
		goto rx_disable;
	}

	ret = reload_dma(dev, stream, stream->mem_block, stream->cfg.block_size);
	if (ret < 0) {
		LOG_ERR("Failed to reload RX DMA: %d\n", ret);
		goto rx_disable;
	}

	/* All block data received */
	ret = queue_put(&stream->mem_block_queue, mblk_tmp, stream->cfg.block_size);
	if (ret < 0) {
		stream->state = I2S_STATE_ERROR;
		goto rx_disable;
	}
	osSemaphoreRelease(stream->sem);

	/* Stop reception if we were requested */
	if (stream->state == I2S_STATE_STOPPING) {
		stream->state = I2S_STATE_READY;
		goto rx_disable;
	}

	return ret;

rx_disable:
    if (mblk_tmp) {
	    mem_slab_free(stream->cfg.mem_slab, mblk_tmp);
    }
	stream->stream_disable(stream, dev);
	return ret;
}

static int dma_tx_callback(void *priv, dma_isr_status status)
{
	struct stream *stream = (struct stream *)priv;
	struct device *dev = stream->dev;
	size_t mem_block_size;
	int ret = 0;

	if (status < 0) {
		ret = -EIO;
		stream->state = I2S_STATE_ERROR;
		goto tx_disable;
	}

	assert(stream->mem_block != NULL);

	/* All block data sent */
	mem_slab_free(stream->cfg.mem_slab, stream->mem_block);
	stream->mem_block = NULL;

	if (status == DMA_STATUS_ABORTED) {
        /* Already aborted. Do nothing. */
        return 0;
    }

	/* Stop transmission if there was an error */
	if (stream->state == I2S_STATE_ERROR) {
		LOG_ERR("TX error detected\n");
		ret = -EIO;
		goto tx_disable;
	}

	/* Check if we finished transferring one block and stopping is requested */
	if ((stream->state == I2S_STATE_STOPPING) && (status == DMA_STATUS_COMPLETE)) {
		/*
		 * Check if all tx samples have been completely handled
		 * , in case of DRAIN command send all data in the transmit queue_is_empty
		 * and stop the transmission.
		 */
		if (queue_is_empty(&stream->mem_block_queue) == true) {
			stream->queue_drop(stream);
			stream->state = I2S_STATE_READY;
			goto tx_disable;
		} else if (stream->tx_stop_for_drain == false) {
			/*
			 * In case of STOP command, just stop the transmission
			 * at the current. The transmission can be resumed.
			 */
			stream->state = I2S_STATE_READY;
			goto tx_disable;
		}
		/* else: DRAIN trigger -> continue TX normally until queue is empty */
	}

	/* Stop transmission if we were requested */
	if (stream->last_block) {
		stream->state = I2S_STATE_READY;
		goto tx_disable;
	}

	/* Prepare to send the next data block */
	ret = queue_get(&stream->mem_block_queue, &stream->mem_block,
			&mem_block_size);
	if (ret < 0) {
		if (stream->state == I2S_STATE_STOPPING) {
			stream->state = I2S_STATE_READY;
		} else {

			/* XXX: we could start over when data will become available
			 *      instead of giving up.
			 */
#if 0
			stream->state = I2S_STATE_ERROR;
#else
			stream->state = I2S_STATE_STARTING;
#endif
		}
		goto tx_disable;
	}
	osSemaphoreRelease(stream->sem);

	ret = reload_dma(dev, stream, stream->mem_block, mem_block_size);
	if (ret < 0) {
		LOG_ERR("Failed to reload TX DMA: %d\n", ret);
		goto tx_disable;
	}

	return ret;

tx_disable:
	stream->stream_disable(stream, dev);
	return ret;
}

static int rx_stream_start(struct stream *stream, struct device *dev)
{
	int ret;

	assert(stream->dir == I2S_DIR_RX);

	ret = mem_slab_alloc(stream->cfg.mem_slab, &stream->mem_block, 0);
	if (ret < 0) {
		return ret;
	}

    python_i2s_set_master(dev, false, stream->master);

	/* Enable I2S clock propagation */
	/* XXX: this must be done after all other settings
	 *      were applied.
	 */
	python_i2s_enable_clock(dev, true);

	ret = start_dma(dev, stream, stream->mem_block, stream->cfg.block_size);
	if (ret < 0) {
		LOG_ERR("Failed to start RX DMA transfer: %d\n", ret);
		return ret;
	}

	return 0;
}

static int tx_stream_start(struct stream *stream, struct device *dev)
{
	size_t mem_block_size;
	int ret;

	assert(stream->dir == I2S_DIR_TX);

	ret = queue_get(&stream->mem_block_queue, &stream->mem_block,
			&mem_block_size);
	if (ret < 0) {
		return ret;
	}

	osSemaphoreRelease(stream->sem);

    python_i2s_set_master(dev, true, stream->master);

	/* Enable I2S clock propagation */
	/* XXX: this must be done after all other settings
	 *      were applied.
	 */
	python_i2s_enable_clock(dev, true);

	ret = start_dma(dev, stream, stream->mem_block, stream->cfg.block_size);
	if (ret < 0) {
		LOG_ERR("Failed to start TX DMA transfer: %d\n", ret);
		return ret;
	}

	return 0;
}

static void stream_disable(struct stream *stream, struct device *dev)
{
	dma_ch_abort(stream->dev_dma, stream->dma_channel);
	dma_ch_rel(stream->dev_dma, stream->dma_channel);
	stream->dma_channel = -1;
	if (stream->mem_block != NULL) {
		mem_slab_free(stream->cfg.mem_slab, stream->mem_block);
		stream->mem_block = NULL;
	}

	/*
	LL_I2S_Disable(cfg->i2s);
	*/
}

static void rx_stream_disable(struct stream *stream, struct device *dev)
{
	assert(stream->dir == I2S_DIR_RX);
    stream_disable(stream, dev);
}

static void tx_stream_disable(struct stream *stream, struct device *dev)
{
	assert(stream->dir == I2S_DIR_TX);
	stream_disable(stream, dev);
}

static void rx_queue_drop(struct stream *stream)
{
	size_t size;
	void *mem_block;
	int ret;

	assert(stream->dir == I2S_DIR_RX);

	while (queue_get(&stream->mem_block_queue, &mem_block, &size) == 0) {
		mem_slab_free(stream->cfg.mem_slab, mem_block);
	}

	do {
		ret = osSemaphoreAcquire(stream->sem, 0);
	} while (ret == osOK);
}

static void tx_queue_drop(struct stream *stream)
{
	size_t size;
	void *mem_block;
	unsigned int n = 0U;

	assert(stream->dir == I2S_DIR_TX);

	while (queue_get(&stream->mem_block_queue, &mem_block, &size) == 0) {
		mem_slab_free(stream->cfg.mem_slab, mem_block);
		n++;
	}

	for (; n > 0; n--) {
		osSemaphoreRelease(stream->sem);
	}
}

static int python_i2s_probe(struct device *dev)
{
	struct i2s_driver_data *priv;
	struct pinctrl_pin_map *pin;
	struct file *file;
	char buf[32];
	int i;
	int ret;
	u32 v;

	priv = kmalloc(sizeof(*priv));
	if (!priv) {
		return -ENOMEM;
	}

	memset(priv, 0, sizeof(struct i2s_driver_data));

	priv->mclk = clk_get(NULL, "i2s/mclk");
	if (priv->mclk == NULL) {
		LOG_ERR("mclk is not defined for (%s)\n", dev_name(dev));
		free(priv);
		return -ENODEV;
	}

	/* Set TX_FIFO_TRIG_NUM to a sane value. */
	v = i2s_readl(OFT_I2S_CFG3);
	v &= ~I2S_PYTHON_CFG3_TXFIFO_TRIG_NUM;
	v |= 1 << I2S_PYTHON_CFG3_TXFIFO_TRIG_NUM_S;
	i2s_writel(v, OFT_I2S_CFG3);

	priv->tx.tx_stop_for_drain = false;

	for (i = 0; i < ARRAY_SIZE(i2s_pin_name); i++) {
		pin = pinctrl_lookup_platform_pinmap(dev, i2s_pin_name[i]);
		if (pin == NULL) {
			LOG_ERR("pin is not defined for (%s)\n", i2s_pin_name[i]);
			free(priv);
			return -ENODEV;
		}
		ret = pinctrl_request_pin(dev, pin->id, pin->pin);
		if (ret) {
			LOG_ERR("pin is not available for (%s)\n", i2s_pin_name[i]);
			free(priv);
			return -ENODEV;
		}
		priv->pmap[i] = pin;
	}

	/* Initialize streams */
	priv->rx.dev = priv->tx.dev = dev;
	priv->rx.dir = I2S_DIR_RX;
	priv->tx.dir = I2S_DIR_TX;
	priv->rx.sem = osSemaphoreNew(CONFIG_I2S_PYTHON_RX_BLOCK_COUNT, 0, NULL);
	priv->tx.sem = osSemaphoreNew(CONFIG_I2S_PYTHON_TX_BLOCK_COUNT,
			CONFIG_I2S_PYTHON_TX_BLOCK_COUNT, NULL);
	if (!priv->rx.sem || !priv->tx.sem) {
		free(priv);
		return -ENOMEM;
	}

	priv->rx.mem_block_queue.buf = rx_ring_buf;
	priv->rx.mem_block_queue.len = ARRAY_SIZE(rx_ring_buf);
	priv->tx.mem_block_queue.buf = tx_ring_buf;
	priv->tx.mem_block_queue.len = ARRAY_SIZE(tx_ring_buf);

	/* XXX: depends on SOC_SCM2010 */
	config_dma(dev, &priv->tx);
	config_dma(dev, &priv->rx);

	priv->rx.stream_start = rx_stream_start;
	priv->rx.stream_disable = rx_stream_disable;
	priv->rx.queue_drop = rx_queue_drop;

	priv->tx.stream_start = tx_stream_start;
	priv->tx.stream_disable = tx_stream_disable;
	priv->tx.queue_drop = tx_queue_drop;

	priv->dev = dev;
	priv->devfs_ops.ioctl = i2s_ioctl;
	priv->devfs_ops.open = python_i2s_open;
	priv->devfs_ops.release = python_i2s_release;

	sprintf(buf, "/dev/i2s");

	file = vfs_register_device_file(buf, &priv->devfs_ops, dev);
	if (!file) {
		LOG_ERR("%s: failed to register as %s\n", dev_name(dev), buf);
		free(priv);
		return -ENOSYS;
	}

	printk("%s: registered as %s.\n", dev_name(dev), buf);

	dev->driver_data = priv;

	return 0;
}

int python_i2s_shutdown(struct device *dev)
{
	struct i2s_driver_data *priv = dev->driver_data;
    int i;

    /* Disable the clock */

	/* Return pins */

	for (i = 0; i < ARRAY_SIZE(priv->pmap); i++) {
		if (priv->pmap[i]) {
		    pinctrl_free_pin(dev, priv->pmap[i]->id,
                    priv->pmap[i]->pin);
		}
	}

	free(priv);

	return 0;
}

#ifdef CONFIG_PM_DM
static int python_i2s_suspend(struct device *dev, u32 *idle)
{
	return 0;
}

static int python_i2s_resume(struct device *dev)
{
	return 0;
}
#endif

struct i2s_ops python_i2s_ops = {
	.configure = python_i2s_configure,
	.config_get = python_i2s_config_get,
	.buf_read = python_i2s_buf_read,
	.buf_write = python_i2s_buf_write,
	.trigger = python_i2s_trigger,
	.state_get = python_i2s_state_get,
};

static declare_driver(i2s) = {
	.name       = "python-i2s",
	.probe      = python_i2s_probe,
	.shutdown   = python_i2s_shutdown,
#ifdef CONFIG_PM_DM
	.suspend    = python_i2s_suspend,
	.resume     = python_i2s_resume,
#endif
	.ops        = &python_i2s_ops,
};

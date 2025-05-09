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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <soc.h>
#include <hal/kernel.h>
#include <hal/bitops.h>
#include <hal/console.h>
#include <hal/timer.h>
#include <hal/kmem.h>
#include <hal/efuse.h>
#include <hal/adc.h>

#include "vfs.h"

#define AUXADC_CFG_INPUT_MASK	(0x07)
#define AUXADC_CFG_INPUT_CH(v)	(v & 0x7)
#define AUXADC_CFG_CLK_EN		(1 << 3)
#define AUXADC_CFG_RESET		(1 << 4)
#define AUXADC_CFG_EN			(1 << 5)
#define AUXADC_CFG_DISH			(1 << 6)
#define AUXADC_CFG_SDIF			(1 << 7)
#define AUXADC_CFG_OFFSET_MASK	(0x3F << 8)
#define AUXADC_CFG_OFFSET(v)	((v & 0x3F) << 8)
#define AUXADC_CFG_GCMP			(1 << 14)
#define AUXADC_CFG_VOBUF_EN		(1 << 15)
#define AUXADC_CFG_DIV_EN		(1 << 16)
#define AUXADC_CFG_BGR_EN		(1 << 17)
#define AUXADC_CFG_SBGR			((v & 0x3FF) << 18)
#define AUXADC_CFG_DI_VOBUF		(1 << 28)

#define AUXADC_CTRL_START		(1 << 0)
#define AUXADC_CTRL_MODE		(1 << 1)
#define AUXADC_CTRL_CLKSEL(v)	((v & 0x3) << 2)
#define AUXADC_CTRL_CH_GAP(v)	((v & 0xFF) << 8)
#define AUXADC_CTRL_INTR_CLR	(1 << 16)

#define AUXADC_CENTER_VALUE		0x800

struct auxadc_driver_data {
	struct fops devfs_ops;
	struct device *dev;

	volatile uint8_t adc_cal;

	uint8_t busy;
	uint8_t offset;

	uint8_t ch;
	uint8_t idx;
	uint8_t differ;
	uint16_t *data;
	uint32_t data_oft;
	uint32_t data_len;

	adc_cb cb;
	void *ctx;
};

#define auxadc_cfg_read()		readl(dev->base[0])
#define auxadc_cfg_write(v)		writel(v, dev->base[0])

#define auxadc_ctrl_read()		readl(dev->base[1])
#define auxadc_ctrl_write(v)	writel(v, dev->base[1])

#define auxadc_data_read(dev, ch)	readl(dev->base[2] + (ch * 4))

#define AUXADC_LINEAR_PARAM_IN_EFUSE_ROW 25


static int auxadc_get_linear_parameter(uint8_t is_odd, s32 *m, s32 *n)
{
	struct device *efuse_dev = NULL;
	u32 v;

    efuse_dev = device_get_by_name("efuse-scm2010");
    if (!efuse_dev || !efuse_dev->driver_data) {
        printk("AUXADC: No efuse device or not ready\n");
        return -ENOSYS;
    }

	efuse_read(efuse_dev, AUXADC_LINEAR_PARAM_IN_EFUSE_ROW, &v);
	(*m) = (v & 0x3FF);
	if (*m >= 512) *m -= 1024;

	if (is_odd)
		(*n) = (v & 0x1FFC00) >> 10;
	else
		(*n) = (v & 0xFFE00000) >> 21;

	if (*n >= 1024)
		*n -= 2048;

	return 0;
}

static int auxadc_data_read_linearized(struct device *dev, enum adc_channel ch)
{
	uint32_t v;

	int ret = 0;
	uint8_t is_odd = (ch & 0x01);
	s32 m = 0, n = 0;
	double dm = 0, dn = 0, dv = 0;

	v = auxadc_data_read(dev, (ch/2));
	if (is_odd)
		v = (v >> 16) & 0xFFF;
	else
		v = v & 0xFFF;

	printk("AUXADC: read value (%d -->", v);

	// read Efuse calibration parameter, if any
	ret = auxadc_get_linear_parameter(is_odd, &m, &n);
	if (ret < 0) {
		printk("%d)\n", v);
		return v;
	}
	// if efuse data not empty, perform linear regression
	if (m != 0 || n != 0) {
		// slope=dm, efues value=m:  m = (1-dm) * 10000
		dm = m;
		dm = 1.0 - (dm / 10000.0);
		// offset is 10 times integers of the original one
		dn = n;
		dn = dn / 10.0;
		dv = dm * v + dn;
		v = (unsigned int)round(dv);
		if (v > 4095)
			v = 4095; // avoid rounding to over 4095
	}
	printk("%d)\n", v);
	printk("AUXADC: read ch(%d): (m,n)=(%d, %d)-->(%d/10000, %d/10)\n", ch, m, n,
		(int)(dm*10000), (int)(dn*10));

	return v;
}

static void auxadc_start(struct device *dev)
{
	uint32_t v;

	v = auxadc_ctrl_read();
	v |= AUXADC_CTRL_START;
	auxadc_ctrl_write(v);
}

static void auxadc_enable(struct device *dev)
{
	uint32_t v;

	v = auxadc_cfg_read();
	v |= AUXADC_CFG_CLK_EN | AUXADC_CFG_EN;
	auxadc_cfg_write(v);
}

static void auxadc_disable(struct device *dev)
{
	uint32_t v;

	v = auxadc_cfg_read();
	v &= ~(AUXADC_CFG_CLK_EN | AUXADC_CFG_EN);
	auxadc_cfg_write(v);
}

static void auxadc_intr_clear(struct device *dev)
{
	uint32_t v;

	v = auxadc_ctrl_read();
	v |= AUXADC_CTRL_INTR_CLR;
	auxadc_ctrl_write(v);
}

static int aux_adc_configure(struct device *dev, enum adc_channel ch)
{
	uint32_t v;
	int differ;

	/* ADC configuration */
	v = auxadc_cfg_read();

	if (ch < ADC_DIFFER_CH_0_1) {
		v &= ~AUXADC_CFG_SDIF;
		differ = 0;
	} else {
		v |= AUXADC_CFG_SDIF;
		differ = 1;
		ch -= ADC_DIFFER_CH_0_1;
	}

	v &= ~AUXADC_CFG_INPUT_MASK;
	v |= AUXADC_CFG_INPUT_CH(ch);

	auxadc_cfg_write(v);

	return differ;
}

static int auxadc_get_value(struct device *dev, enum adc_channel ch, uint16_t *data, uint32_t len)
{
	struct auxadc_driver_data *priv = dev->driver_data;

	if (priv->busy) {
		return -EBUSY;
	}

	auxadc_enable(dev);

	priv->differ = aux_adc_configure(dev, ch);

	if (priv->differ) {
		priv->ch = ch - ADC_DIFFER_CH_0_1;
	} else {
		priv->ch = ch;
	}
	priv->data = data;
	priv->data_len = len;
	priv->data_oft = 0;
	priv->busy = 1;

	auxadc_start(dev);

	return 0;
}

static int auxadc_get_value_poll(struct device *dev, enum adc_channel ch, uint16_t *data, uint32_t len)
{
	struct auxadc_driver_data *priv = dev->driver_data;
	uint32_t v;
	int differ;
	int need_restart = 0;
	int i;

	if (priv->busy && priv->data) {
		need_restart = 1;
	} else {
		priv->busy = 1;
	}

	disable_irq(dev->irq[0]);

	auxadc_enable(dev);

	differ = aux_adc_configure(dev, ch);
	if (differ) {
		ch = ch - ADC_DIFFER_CH_0_1;
	}

	for (i = 0; i < len; i++) {
		auxadc_start(dev);

		udelay(10);

		if (differ) {
			v = auxadc_data_read(dev, ch);
		} else {
			v = auxadc_data_read_linearized(dev, ch);
		}
		data[i] = v & 0xFFF;
	}

	auxadc_disable(dev);

	if (need_restart) {
		auxadc_enable(dev);
		aux_adc_configure(dev, priv->ch);
		auxadc_start(dev);
		udelay(20);
	} else {
		priv->busy = 0;
	}

	enable_irq(dev->irq[0]);

	return 0;
}

static int auxadc_set_cb(struct device *dev, adc_cb cb, void *ctx)
{
	struct auxadc_driver_data *priv = dev->driver_data;

	priv->cb = cb;
	priv->ctx = ctx;

	return 0;
}

static int auxadc_reset(struct device *dev)
{
	uint32_t v;

    v = auxadc_cfg_read();
	v |= AUXADC_CFG_RESET;
	auxadc_cfg_write(v);

	return 0;
}

static int auxadc_irq(int irq, void *data)
{
	struct device *dev = data;
	struct auxadc_driver_data *priv = dev->driver_data;
	uint32_t v;

	/* interrupt clear */
	auxadc_intr_clear(dev);

	if (!priv->adc_cal) {
		priv->adc_cal = 1;
		return 0;
	}

	if (priv->data) {
		if (priv->differ) {
			v = auxadc_data_read(dev, priv->ch);
		} else {
			//v = auxadc_data_read(dev, priv->ch / 2);
			v = auxadc_data_read_linearized(dev, priv->ch);
		}
		priv->data[priv->data_oft] = v & 0xFFF;
		priv->data_oft++;

		if (priv->data_oft < priv->data_len) {
			auxadc_start(dev);
		} else {
			if (priv->cb) {
				priv->cb(priv->ctx);
			}
			priv->data = NULL;

			auxadc_disable(dev);

			priv->busy = 0;
		}
	}

	return 0;
}

static int auxadc_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct auxadc_driver_data *priv = file->f_priv;
	struct device *dev = priv->dev;
	struct adc_read_arg *read_arg;
	struct adc_set_cb_arg *cb_arg;
	int ret;

	switch (cmd) {
	case IOCTL_ADC_READ:
		read_arg = arg;
		ret = auxadc_get_value(dev, read_arg->ch, read_arg->data, read_arg->len);
		break;
	case IOCTL_ADC_SET_CB:
		cb_arg = arg;
		ret = auxadc_set_cb(dev, cb_arg->adc_cb, cb_arg->ctx);
		break;
	case IOCTL_ADC_RESET:
		ret = auxadc_reset(dev);
		break;
	default:
		ret = -EINVAL;
		break;
	}


	return ret;
}

static int auxadc_calibration(struct device *dev)
{
	struct auxadc_driver_data *priv = dev->driver_data;
    int offset;
    int data;
    uint32_t v;

	v = auxadc_cfg_read();
    v |= AUXADC_CFG_DISH;				/* DISH = 1: shoting VIP and VIN for offset calribration */
    v |= AUXADC_CFG_SDIF;				/* SDIF = 1: offset calibration must be done at diffrentail mode */
	v &= ~AUXADC_CFG_OFFSET_MASK;		/* OFFSET[5:0] must be set all 0 */
	v &= ~AUXADC_CFG_INPUT_MASK;		/* VIN0 ~ VIN15 could be floating */
    auxadc_cfg_write(v);

	priv->adc_cal = 0;

    /* trigger adc */
	auxadc_start(dev);

	/* calibration busy waiting */
	while (1) {
		if (priv->adc_cal) {
			break;
		}
	}

	data = auxadc_data_read(dev, 0) & 0xFFF;

    offset = AUXADC_CENTER_VALUE - data;

    v = auxadc_cfg_read();
	v &= ~(AUXADC_CFG_DISH | AUXADC_CFG_SDIF);
	auxadc_cfg_write(v);

    return offset;
}

static int auxadc_probe(struct device *dev)
{
	struct auxadc_driver_data *priv;
	struct file *file;
	char buf[32];
	uint32_t v;
	int ret;

	priv = kmalloc(sizeof(struct auxadc_driver_data));
	if (!priv) {
		printk("No memory\n");
		return -ENOMEM;
	}

	memset(priv, 0, sizeof(struct auxadc_driver_data));

	ret = request_irq(dev->irq[0], auxadc_irq, dev_name(dev), dev->pri[0], dev);
	if (ret) {
		printk("%s irq req is failed(%d)", dev_name(dev), ret);
		kfree(priv);
		return -1;
	}

	auxadc_enable(dev);
	printk("open : %p\n", priv->devfs_ops.open);

    v = auxadc_cfg_read();
	v |= AUXADC_CFG_VOBUF_EN | AUXADC_CFG_BGR_EN;
	auxadc_cfg_write(v);

	dev->driver_data = priv;
	priv->dev = dev;
	priv->offset = (auxadc_calibration(dev) / 2) & 0x3f;
	if (priv->offset == 0x3f) {
		/* fix the bug when calibration value is 0x3F */
		priv->offset = 0;
	}

	printk("ADC: value offset = 0x%02x\n", priv->offset);

	v = auxadc_cfg_read();
	v |= AUXADC_CFG_OFFSET(priv->offset);
	v &= ~(AUXADC_CFG_VOBUF_EN | AUXADC_CFG_BGR_EN);
	auxadc_cfg_write(v);

	auxadc_disable(dev);

	sprintf(buf, "/dev/adc");

	priv->devfs_ops.ioctl = auxadc_ioctl;

	file = vfs_register_device_file(buf, &priv->devfs_ops, priv);
	if (!file) {
		printk("%s: failed to register as %s\n", dev_name(dev), buf);
		return -1;
	}

	printk("ADC: %s registered as %s\n", dev_name(dev), buf);

	return 0;
}

static int auxadc_shutdown(struct device *dev)
{
	adc_reset(dev);
	auxadc_disable(dev);

	free_irq(dev->irq[0], dev_name(dev));
	free(dev->driver_data);

	return 0;
}

#ifdef CONFIG_PM_DM
static int auxadc_suspend(struct device *dev, u32 *idle)
{
	return 0;
}

static int auxadc_resume(struct device *dev)
{
	struct auxadc_driver_data *priv = dev->driver_data;
	uint32_t v;

	v = auxadc_cfg_read();
	v |= AUXADC_CFG_OFFSET(priv->offset);
	v &= ~(AUXADC_CFG_VOBUF_EN | AUXADC_CFG_BGR_EN);
	auxadc_cfg_write(v);

	return 0;
}
#endif


struct adc_ops adc_ops = {
	.get_value 		= auxadc_get_value,
	.get_value_poll = auxadc_get_value_poll,
	.reset			= auxadc_reset,
	.set_cb			= auxadc_set_cb,
};

static declare_driver(adc) = {
	.name		= "auxadc-xrc",
	.probe		= auxadc_probe,
	.shutdown   = auxadc_shutdown,
#ifdef CONFIG_PM_DM
	.suspend    = auxadc_suspend,
	.resume     = auxadc_resume,
#endif
	.ops 		= &adc_ops,
};

#ifdef CONFIG_SOC_SCM2010
#if !defined(CONFIG_USE_AUXADC)
#error ADC driver requires ADC devices. Select ADC devices or remove the driver
#endif
#endif

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
#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/spi-flash.h>

#include "spi-flash-internal.h"

#define OFT_SFC_FLMOD        0x00
#define OFT_SFC_FLBRT        0x04
#define OFT_SFC_FLCSH        0x08
#define OFT_SFC_FLPEM        0x0C
#define OFT_SFC_FLCMD        0x10
#define OFT_SFC_FLSTS        0x14
#define OFT_SFC_FLSEA        0x18
#define OFT_SFC_FLBEA        0x1C
#define OFT_SFC_FLDAT        0x20
#define OFT_SFC_FLWCP        0x24
#define OFT_SFC_FLCKDLY      0x28
#define OFT_SFC_FLSTS2       0x2C
#define OFT_SFC_CCTRL        0x30
#define OFT_SFC_WRCMD        0x38

#define SFC_FLMOD_HW_CS				(1 << 8)
#define SFC_FLMOD_BUS_ERROR_ON_WRITE		(1 << 7)
#define SFC_FLMOD_BUS_READY_CTRL		(1 << 3)

static u32 adc_readl(struct device *dev, u32 oft)
{
	return readl(dev->base[0] + oft);
}

static void adc_writel(u32 val, struct device *dev, u32 oft)
{
	writel(val, dev->base[0] + oft);
}

static u8 adc_readb(struct device *dev, u32 oft)
{
	return *((volatile u8 *)(dev->base[0] + oft));
}

static void adc_writeb(u8 byte, struct device *dev, u32 oft)
{
	*((volatile u8 *)(dev->base[0] + oft)) = byte;
}

static void adc_enable_cache(struct device *dev, bool enable)
{
	int timeout = 0;
	u32 v;

	if (enable == false) {
		adc_writel(0x0, dev, OFT_SFC_CCTRL);
		return;
	}

#if 0
	/* Invalidate */
	adc_writel(0x4, dev, OFT_SFC_CCTRL);
	do {
		v = adc_readl(dev, OFT_SFC_CCTRL);
	} while ((v & 0x4) && (timeout++ < 100));
	if (timeout == 100) {
		printk("%s: timeout\n", __func__);
		return;
	}

	/* Enable */
	adc_writel(0x1, dev, OFT_SFC_CCTRL);
#else
	(void)v;
	(void)timeout;
#endif
}

static void adc_set_cs(struct device *device, int high)
{
	u32 v = adc_readl(device, OFT_SFC_FLMOD) & ~SFC_FLMOD_HW_CS;

	if (high)
		v |= SFC_FLMOD_HW_CS;

	adc_writel(v, device, OFT_SFC_FLMOD);
}

static int adc_cmd_read(struct device *dev, const u8 *cmd, size_t cmd_len,
			void *rx, size_t data_len)
{
	u8 *buf = (u8 *) rx;
	int i;
	unsigned long flags;

	local_irq_save(flags);

	adc_enable_cache(dev, false);
	/* Keep CS low */
	adc_set_cs(dev, 0);

	for (i = 0; i < cmd_len; i++)
		adc_writeb(cmd[i], dev, OFT_SFC_FLDAT);

	for (i = 0; i < data_len; i++)
		buf[i] = adc_readb(dev, OFT_SFC_FLDAT);

	/* Let HW handle CS */
	adc_set_cs(dev, 1);
	adc_enable_cache(dev, true);

	local_irq_restore(flags);

	return 0;
}

/* 1 byte command */
static inline
int adc_cmd(struct device *dev, const u8 cmd, void *response, size_t len)
{
	return adc_cmd_read(dev, &cmd, 1, response, len);
}

static
int adc_cmd_write(struct device *dev, const u8 *cmd, size_t cmd_len,
		  void *tx, size_t data_len)
{
	u8 *buf = (u8 *) tx;
	int i;
	unsigned long flags;

	local_irq_save(flags);

	/* Disable SF cache and keep CS low */
	adc_enable_cache(dev, false);
	adc_set_cs(dev, 0);

	/* Command cycles */
	for (i = 0; i < cmd_len; i++)
		adc_writeb(cmd[i], dev, OFT_SFC_FLDAT);

	/* Data cycles */
	for (i = 0; i < data_len; i++)
		adc_writeb(buf[i], dev, OFT_SFC_FLDAT);

	/* Let HW handle CS and enable SF cache */
	adc_set_cs(dev, 1);
	adc_enable_cache(dev, true);

	local_irq_restore(flags);

	return 0;
}

static struct spi_flash_master_ops adc_sfc_ops = {
	.cmd_read = adc_cmd_read,
	.cmd_write = adc_cmd_write,
};

static int adc_sfc_probe(struct device *dev)
{
	struct spi_flash *flash;
	u32 mode;

       	mode = adc_readl(dev, OFT_SFC_FLMOD);
	adc_writel(0x0, dev, OFT_SFC_FLMOD);
	flash = spi_flash_probe(dev);
	adc_writel(mode, dev, OFT_SFC_FLMOD);

	if (!flash)
		return -ENODEV;
	spi_flash_add_device(flash);

	return 0;
}

static declare_driver(adc_fc) = {
	.name = "serial-flash-adc",
	.probe = adc_sfc_probe,
	.ops = &adc_sfc_ops,
};

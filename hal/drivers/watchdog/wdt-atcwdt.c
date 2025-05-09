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
#include <stdio.h>
#include <soc.h>
#include <hal/kernel.h>
#include <hal/console.h>
#include <hal/wdt.h>
#include <cmsis_os.h>

#include "vfs.h"

/* atcwdt wdt registers */
#define OFT_WDT_CTR   		0x10
#define OFT_WDT_RES		0x14
#define OFT_WDT_WEN    		0x18
#define OFT_WDT_STA    		0x1c

#define WDT_MAGIC_WP   		0x5AA5
#define WDT_MAGIC_RE   		0xCAFE

__weak void set_reset_vec(u8 hart, u32 addr)
{
	return;
}

struct atcwdt_wdt_priv {
	unsigned long period;
	u32 clk_rate;
	struct fops devfs_ops;
	struct device *dev;
} wdt_data;

#define UNLOCK(dev) 	writel(WDT_MAGIC_WP, dev->base[0] + OFT_WDT_WEN)

static int atcwdt_wdt_reset(struct device *wdt)
{
	UNLOCK(wdt);
	writel(WDT_MAGIC_RE, wdt->base[0] + OFT_WDT_RES);
	return 0;
}

#define LOG2(x)	({ int __n, __k = 0; for (__n = 1; __n < x; __n = __n * 2) __k++; __k;})
static u8 ptable[] = {
	[0 ... 7] 	= 0,
	[8 ... 9] 	= 1,
	[10] 		= 2,
	[11] 		= 3,
	[12] 		= 4,
	[13] 		= 5,
	[14] 		= 6,
	[15 ... 16]	= 7,
#ifdef CONFIG_ATCWDT200_32BIT_TIMER
	[17 ... 18]	= 8,
	[19 ... 20]	= 9,
	[21 ... 22]	= 10,
	[23 ... 24]	= 11,
	[25 ... 26]	= 12,
	[27 ... 28]	= 13,
	[29 ... 30]	= 14,
	[31] 		= 15,
#endif
};

__iram__ static int atcwdt_wdt_start(struct device *wdt, u64 timeout_ms)
{
	struct atcwdt_wdt_priv *priv = wdt->driver_data;
	u32 ctrl = 0;
	unsigned long intp, rstp;

	/*
	 * Set reset vector to ROM start routine before start watchdog.
	 * We have no chance to change reset vector before watchdog reset
	 * because SW can not control watchdog interrupt.
	 */
	set_reset_vec(0, 0x00100000);
	set_reset_vec(1, 0x00100000);

	intp = (priv->clk_rate * timeout_ms) / 1000UL;
	intp = LOG2(intp);
	if (intp >= ARRAY_SIZE(ptable)) {
		printk("Maximum watchdog timeout exceeded.\n");
		/* More appropriate error handling?
		 */
		intp = ARRAY_SIZE(ptable) - 1;
	}
	intp = ptable[intp];

	rstp = 7; /* reset after (clock period * 2 ^ 14) since interrupt occurs */

	/*
	 * If the watchdog interrupt is enabled, watchdog timer is not restart during
	 * the interrupt stage.
	 * Due to RTC inaccuracy, the watchdog interrupt was disabled to obtain margin.
	 */
	ctrl = (rstp << 8) | (intp << 4) | 0x9; /* RstEn:enable, Inten:disable, EXTCLK(RTC) */

	UNLOCK(wdt);
	writel(ctrl, wdt->base[0] + OFT_WDT_CTR);

	return 0;
}

__weak void park()
{
	printf("Bye bye~\n");
}

static int atcwdt_wdt_expire_now(struct device *wdt)
{
	park();

	return atcwdt_wdt_start(wdt, 100);
}

static int atcwdt_wdt_stop(struct device *wdt)
{
	UNLOCK(wdt);
	writel(0, wdt->base[0] + OFT_WDT_CTR);
	return 0;
}

/* devfs callbacks */

static int atcwdt_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct atcwdt_wdt_priv *priv = file->f_priv;
	struct device *dev = priv->dev;
	int ret = 0;
	u64 *timeout_ms;

	switch (cmd) {
	case IOCTL_WDT_START:
		timeout_ms = arg;
		ret = wdt_ops(dev)->start(dev, *timeout_ms);
		break;
	case IOCTL_WDT_STOP:
		ret = wdt_ops(dev)->stop(dev);
		break;
	case IOCTL_WDT_FEED:
		ret = wdt_ops(dev)->reset(dev);
		break;
	case IOCTL_WDT_EXPIRE_NOW:
		ret = wdt_ops(dev)->expire_now(dev);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int atcwdt_wdt_probe(struct device *wdt)
{
	struct atcwdt_wdt_priv *priv;
	struct clk *clk;
	char buf[32];
	struct file *file;

	priv = &wdt_data;
	clk = clk_get(wdt, "pclk");
	if (clk == NULL) {
		printk("%s: failed to get clock\n", dev_name(wdt));
		return -1;
	}
	priv->dev = wdt;
	priv->clk_rate = clk_get_rate(clk);

	wdt->driver_data = priv;

	printk("%s: @%p, clk=%lu\n", dev_name(wdt), wdt->base[0],
			priv->clk_rate);

	sprintf(buf, "/dev/watchdog");

	priv->devfs_ops.ioctl = atcwdt_ioctl;

	file = vfs_register_device_file(buf, &priv->devfs_ops, priv);
	if (!file) {
		printk("%s: failed to register as %s\n", dev_name(wdt), buf);
		return -1;
	}

	printk("WDT: %s registered as %s\n", dev_name(wdt), buf);

	return 0;
}

struct wdt_ops atcwdt_wdt_ops = {
	.start = atcwdt_wdt_start,
	.stop = atcwdt_wdt_stop,
	.reset = atcwdt_wdt_reset,
	.expire_now = atcwdt_wdt_expire_now,
};

static declare_driver(wdt) = {
	.name = "atcwdt",
	.probe = atcwdt_wdt_probe,
	.ops = &atcwdt_wdt_ops,
};

/**
 * WDT CLI commands
 */
#include <cli.h>

#include <stdio.h>
#include <stdlib.h>

#if 0
int do_wdt_start(int argc, char *argv[])
{
	struct device *wdt = device_get_by_name("atcwdt");
	unsigned timeout;

	if (!wdt)
		return CMD_RET_FAILURE;

	if (argc < 2)
		return CMD_RET_USAGE;

	timeout = strtoul(argv[1], NULL, 0);

	wdt_start(wdt, timeout, 0);

	return 0;
}

int do_wdt_reset(int argc, char *argv[])
{
	struct device *wdt = device_get_by_name("atcwdt");

	if (!wdt)
		return CMD_RET_FAILURE;

	wdt_reset(wdt);

	return 0;
}

int do_wdt_stop(int argc, char *argv[])
{
	struct device *wdt = device_get_by_name("atcwdt");

	if (!wdt)
		return CMD_RET_FAILURE;

	wdt_stop(wdt);

	return 0;
}

int do_wdt_expire_now(int argc, char *argv[])
{
	struct device *wdt = device_get_by_name("atcwdt");

	if (!wdt)
		return CMD_RET_FAILURE;

	wdt_expire_now(wdt);

	return 0;
}

static struct cli_cmd wdt_cmd[] = {
	CMDENTRY(start, do_wdt_start, "", ""),
	CMDENTRY(reset, do_wdt_reset, "", ""),
	CMDENTRY(stop, do_wdt_stop, "", ""),
	CMDENTRY(expire, do_wdt_expire_now, "", ""),
};

static int do_wdt(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], wdt_cmd, ARRAY_SIZE(wdt_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(wdt, do_wdt,
	"test routines for watchdog timer",
	"wdt start timeout_ms" OR
	"wdt reset" OR
	"wdt stop" OR
	"wdt expire"
);
#endif

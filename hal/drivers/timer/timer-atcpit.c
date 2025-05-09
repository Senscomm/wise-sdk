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
#include <string.h>
#include <stdio.h>

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/timer.h>
#include <hal/console.h>
#include <hal/pinctrl.h>
#include <hal/kmem.h>

#include "vfs.h"

/* Read out from configuration register? */
#define CONFIG_NR_CH 4

/* PIT 32-bit (4 channel) timer registers */
#define TIMER_INTEN    		0x14
#define TIMER_INTST    		0x18
#define TIMER_CHEN   		0x1C
#define TIMER_CHCTL(ch)  	(0x20 + ch * 0x10)
#define TIMER_CHRLD(ch)		(0x24 + ch * 0x10)
#define TIMER_CHCNT(ch)		(0x28 + ch * 0x10)

#define TIMER_CHCTL_APBCLK	0x8
#define TIMER_CHCTL_TMR32	0x1
#define TIMER_CHCTL_TMR16	0x2
#define TIMER_CHCTL_TMR8	0x3
#define TIMER_CHCTL_PWM		0x4
#define TIMER_CHCTL_MIX16	0x6
#define TIMER_CHCTL_MIX8	0x7


/* dedicate the timer's usage scenario */
#define TIMER_TMR_0			0
#define TIMER_TMR_1			1
#define TIMER_TMR_2			2
#define TIMER_TMR_3			3

/* timer enable with channel and timer id */
#define TIMER_CH_EN_BIT(ch, tmr)	((1 << tmr) << (ch * 4))


struct timer_driver_data {
	struct fops devfs_ops;
	struct device *dev;
	struct clk *clk;
	struct ch_data {
		u32 reload;
		u32 rate;
		u32 wrap;
		u32 config;
		struct pinctrl_pin_map *pin_pwm;
		timer_isr isr;
		void *ctx;
	} ch_data[CONFIG_NR_CH];
};

static u32 timer_readl(struct device *timer, u32 oft)
{
	return readl(timer->base[0] + oft);
}

static void timer_writel(u32 val, struct device *timer, u32 oft)
{
	writel(val, timer->base[0] + oft);
}

static int timer_has_pending_irq(struct device *timer)
{
	u32 irq = timer_readl(timer, TIMER_INTST);

	return (irq) != 0;
}

static int timer_check_enable_pwm(struct device *timer)
{
#ifndef CONFIG_USE_TIMER0_PWM
	if (dev_id(timer) == 0) {
		return -1;
	}
#endif

#ifndef CONFIG_USE_TIMER1_PWM
	if (dev_id(timer) == 1) {
		return -1;
	}
#endif

	return 0;
}

static void timer_reload(struct device *timer, u32 ch, u32 reload, bool enable)
{
	u32 en;

	timer_writel(reload, timer, TIMER_CHRLD(ch));
	if (enable) {
		en = timer_readl(timer, TIMER_CHEN);
		timer_writel(en & ~TIMER_CH_EN_BIT(ch, TIMER_TMR_0), timer, TIMER_CHEN);
		timer_writel(en | TIMER_CH_EN_BIT(ch, TIMER_TMR_0), timer, TIMER_CHEN);
	}
}

/* Common irq handler for all 4 channels */
static int atcpit_timer_irq(int irq, void *data)
{
	struct device *timer = data;
	struct timer_driver_data *priv = timer->driver_data;
	u32 st, ch, en, rl;

	st = timer_readl(timer, TIMER_INTST);
	timer_writel(st, timer, TIMER_INTST);

	for (ch = 0; ch < CONFIG_NR_CH; ch++) {
		if (!(st & TIMER_CH_EN_BIT(ch, TIMER_TMR_0)))
			continue;
		switch (priv->ch_data[ch].config & 0xf) {
			case HAL_TIMER_FREERUN:
				priv->ch_data[ch].wrap++;
				rl = timer_readl(timer, TIMER_CHRLD(ch));
				if (rl != priv->ch_data[ch].reload)
					timer_reload(timer, ch, priv->ch_data[ch].reload, true);
				break;
			case HAL_TIMER_ONESHOT:
				en = timer_readl(timer, TIMER_CHEN);
				timer_writel(en & ~TIMER_CH_EN_BIT(ch, TIMER_TMR_0), timer, TIMER_CHEN);
				if (priv->ch_data[ch].isr)
					priv->ch_data[ch].isr(TIMER_EVENT_EXPIRE, priv->ch_data[ch].ctx);
				break;
			case HAL_TIMER_PERIODIC:
				rl = timer_readl(timer, TIMER_CHRLD(ch));
				if (rl != priv->ch_data[ch].reload)
					timer_reload(timer, ch, priv->ch_data[ch].reload, false);
				if (priv->ch_data[ch].isr)
					priv->ch_data[ch].isr(TIMER_EVENT_EXPIRE, priv->ch_data[ch].ctx);
				break;
			default:
				break;
		}
	}

	return 0;
}

static int atcpit_timer_mux_pwm(struct device *timer, u8 ch)
{
	struct timer_driver_data *priv = timer->driver_data;
	char buf[] = "pwmX";

	buf[3] = '0' + ch;

	priv->ch_data[ch].pin_pwm = pinctrl_lookup_platform_pinmap(timer, buf);
	if (!priv->ch_data[ch].pin_pwm) {
		printk("no pin available for %s\n", dev_name(timer));
		return -1;
	}
	if (pinctrl_request_pin(timer, priv->ch_data[ch].pin_pwm->id, priv->ch_data[ch].pin_pwm->pin)) {
		return -1;
	}

	return 0;
}

static int atcpit_timer_mux_clear(struct device *timer, u8 ch)
{
	struct timer_driver_data *priv = timer->driver_data;

	if (priv->ch_data[ch].pin_pwm) {
		pinctrl_free_pin(timer, priv->ch_data[ch].pin_pwm->id, priv->ch_data[ch].pin_pwm->pin);
		priv->ch_data[ch].pin_pwm = NULL;
	}
	return 0;
}

static int atcpit_timer_setup(struct device *timer, u8 ch, u32 config,
			      u32 param, timer_isr isr, void *ctx)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 chctl;
	u32 reload = 0;
	unsigned long tclk;

#ifdef CONFIG_SYSTIMER
	if (dev_id(timer) == 1 && ch == TIMER_TMR_0) {
		return -EPERM;
	}
#endif

	if ((config & 0xf) == HAL_TIMER_PWM) {
		if (timer_check_enable_pwm(timer) < 0) {
			return -EPERM;
		}
	}

	chctl = timer_readl(timer, TIMER_CHCTL(ch));

	tclk = clk_get_rate(priv->clk);

	atcpit_timer_mux_clear(timer, ch);

	switch (config & 0xf) {
		case HAL_TIMER_FREERUN:
			if (isr)
				return -ENOTSUP;
			chctl = (chctl & ~0xF) | (TIMER_CHCTL_APBCLK | TIMER_CHCTL_TMR32);
			/* @param is rate (in HZ) */
			/* Free-run timer is implemented by software */
			/* HW will be expired every 10 sec. to wrap */
			reload = (tclk * 10 - 1);
			break;
		case HAL_TIMER_ONESHOT:
			/* @param is interval in usec */
			/* fall-through */
		case HAL_TIMER_PERIODIC:
			chctl = (chctl & ~0xF) | (TIMER_CHCTL_APBCLK | TIMER_CHCTL_TMR32);
			reload = (tclk / 1000000 * param - 1);
			break;
		case HAL_TIMER_PWM:
			if (atcpit_timer_mux_pwm(timer, ch)) {
				return -EIO;
			}

			chctl = (chctl & ~0xF) | (TIMER_CHCTL_APBCLK | TIMER_CHCTL_PWM);
			if (config & HAL_TIMER_PWM_PARK) {
				chctl |= (1 << 4);
			} else {
				chctl &= ~(1 << 4);
			}
			reload = (tclk / 1000000 * (param >> 16) - 1) << 16 | \
					 (tclk / 1000000 * (param & 0xFFFF) - 1) << 0;

			break;
	}

	timer_writel(chctl, timer, TIMER_CHCTL(ch));
	timer_writel(reload, timer, TIMER_CHRLD(ch));

	priv->ch_data[ch].reload = reload;
	priv->ch_data[ch].rate = param;
	priv->ch_data[ch].config = config;
	priv->ch_data[ch].ctx = ctx;
	if (config & HAL_TIMER_IRQ)
		priv->ch_data[ch].isr = isr;

	return 0;
}

static int atcpit_timer_start(struct device *timer, u8 ch)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 en;
	bool freerun;
	uint8_t tmr;

	if (ch >= CONFIG_NR_CH) {
		return -ENODEV;
	}

	freerun = ((priv->ch_data[ch].config & 0xf) == HAL_TIMER_FREERUN);
	if (freerun || priv->ch_data[ch].isr) {
		en = timer_readl(timer, TIMER_INTEN);
		timer_writel(en | TIMER_CH_EN_BIT(ch, TIMER_TMR_0), timer, TIMER_INTEN);
	}

	en = timer_readl(timer, TIMER_CHEN);
	if (priv->ch_data[ch].config & HAL_TIMER_PWM) {
		tmr = TIMER_TMR_3;
	} else {
		tmr = TIMER_TMR_0;
	}
	timer_writel(en | TIMER_CH_EN_BIT(ch, tmr), timer, TIMER_CHEN);

	/*
	 * Timer channel 0 counter is not initialized to its
	 * reload value when enabled for some unknown reason.
	 * Disablining and re-enabling the channel does work.
	 */
	timer_writel(en & ~TIMER_CH_EN_BIT(ch, tmr), timer, TIMER_CHEN);
	timer_writel(en | TIMER_CH_EN_BIT(ch, tmr), timer, TIMER_CHEN);

	return 0;
}

static int atcpit_timer_stop(struct device *timer, u8 ch)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 en;
	bool freerun;
	uint8_t tmr;

	if (ch >= CONFIG_NR_CH) {
		return -ENODEV;
	}

	en = timer_readl(timer, TIMER_CHEN);
	if (priv->ch_data[ch].config & HAL_TIMER_PWM) {
		tmr = TIMER_TMR_3;
	} else {
		tmr = TIMER_TMR_0;
	}
	timer_writel(en & ~TIMER_CH_EN_BIT(ch, tmr), timer, TIMER_CHEN);

	freerun = ((priv->ch_data[ch].config & 0xf) == HAL_TIMER_FREERUN);
	if (freerun || priv->ch_data[ch].isr) {
		en = timer_readl(timer, TIMER_INTEN);
		timer_writel(en & ~TIMER_CH_EN_BIT(ch, TIMER_TMR_0), timer, TIMER_INTEN);
	}
	return 0;
}

static int atcpit_timer_start_multi(struct device *timer, u8 chs)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 en;
	u32 en_update;
	bool freerun;
	uint8_t tmr;
	u8 ch;

	en_update = 0;
	en = timer_readl(timer, TIMER_INTEN);
	for (ch = 0; ch < CONFIG_NR_CH; ch++) {
		if (!(chs & (1 << ch))) {
			continue;
		}

		freerun = ((priv->ch_data[ch].config & 0xf) == HAL_TIMER_FREERUN);
		if (freerun || priv->ch_data[ch].isr) {
			en_update |= TIMER_CH_EN_BIT(ch, TIMER_TMR_0);
		}
	}
	timer_writel(en | en_update, timer, TIMER_INTEN);

	en_update = 0;
	en = timer_readl(timer, TIMER_CHEN);
	for (ch = 0; ch < CONFIG_NR_CH; ch++) {
		if (!(chs & (1 << ch))) {
			continue;
		}
		if (priv->ch_data[ch].config & HAL_TIMER_PWM) {
			tmr = TIMER_TMR_3;
		} else {
			tmr = TIMER_TMR_0;
		}

		en_update |= TIMER_CH_EN_BIT(ch, tmr);
	}
	timer_writel(en | en_update, timer, TIMER_CHEN);

	/*
	 * Timer channel 0 counter is not initialized to its
	 * reload value when enabled for some unknown reason.
	 * Disablining and re-enabling the channel does work.
	 */
	timer_writel(en & ~en_update, timer, TIMER_CHEN);
	timer_writel(en | en_update, timer, TIMER_CHEN);

	return 0;
}

static int atcpit_timer_stop_multi(struct device *timer, u8 chs)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 en;
	u32 en_update;
	bool freerun;
	uint8_t tmr;
	u8 ch;

	en = timer_readl(timer, TIMER_CHEN);
	en_update = 0;
	for (ch = 0; ch < CONFIG_NR_CH; ch++) {
		if (!(chs & (1 << ch))) {
			continue;
		}

		if (priv->ch_data[ch].config & HAL_TIMER_PWM) {
			tmr = TIMER_TMR_3;
		} else {
			tmr = TIMER_TMR_0;
		}
		en_update |= TIMER_CH_EN_BIT(ch, tmr);
	}
	timer_writel(en & ~en_update, timer, TIMER_CHEN);

	for (ch = 0; ch < CONFIG_NR_CH; ch++) {
		if (!(chs & (1 << ch))) {
			continue;
		}
		freerun = ((priv->ch_data[ch].config & 0xf) == HAL_TIMER_FREERUN);
		if (freerun || priv->ch_data[ch].isr) {
			en = timer_readl(timer, TIMER_INTEN);
		}
		en_update |= TIMER_CH_EN_BIT(ch, TIMER_TMR_0);
	}
	timer_writel(en & ~en_update, timer, TIMER_INTEN);

	return 0;
}

static unsigned long atcpit_timer_get_rate(struct device *timer)
{
	struct timer_driver_data *priv = timer->driver_data;

	return clk_get_rate(priv->clk);
}

static u32 atcpit_timer_get_value(struct device *timer, u8 ch)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 cnt = 0, ratio, v;
	u32 clkr = clk_get_rate(priv->clk);
	u32 flags;

	if (ch >= CONFIG_NR_CH) {
		return -ENODEV;
	}

	local_irq_save(flags);

	if ((priv->ch_data[ch].config & 0xf) != HAL_TIMER_FREERUN) {
		cnt = timer_readl(timer,TIMER_CHCNT(ch));
		goto out;
	}

	if (timer_has_pending_irq(timer))
		atcpit_timer_irq(timer->irq[0], timer);

	ratio = clkr / priv->ch_data[ch].rate;
	v = timer_readl(timer, TIMER_CHCNT(ch));
	cnt = (priv->ch_data[ch].reload - v) / ratio;
	cnt += (priv->ch_data[ch].reload / ratio) * priv->ch_data[ch].wrap;

 out:
	local_irq_restore(flags);
	return cnt;
}

static int atcpit_timer_adjust(struct device *timer, u8 ch, u32 adv)
{
	struct timer_driver_data *priv = timer->driver_data;
	u64 v;
	u32 adj, reload, count;
	u32 clkr = clk_get_rate(priv->clk);
	int err = 0;
	u32 flags;

	if (ch >= CONFIG_NR_CH) {
		return -ENODEV;
	}

	local_irq_save(flags);

	if (!adv)
		goto out;

	if (!(priv->ch_data[ch].config & HAL_TIMER_FREERUN)) {
		timer_reload(timer, ch, adv, false); goto out;
	}

	/* convert usec to # of clock durations */
	v = adv * (clkr / priv->ch_data[ch].rate);
	adj = (u32) v;

	count = timer_readl(timer, TIMER_CHCNT(ch));
	if (count >= adj) {
		reload = count - adj;
	} else {
		priv->ch_data[ch].wrap++;
		reload = priv->ch_data[ch].reload - (adj - count);
	}
	timer_reload(timer, ch, reload, true);
out:
	local_irq_restore(flags);

	return err;
}

static int atcpit_timer_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct timer_driver_data *priv = file->f_priv;
	struct device *dev = priv->dev;
	int ret = 0;

	switch (cmd) {
	case IOCTL_TIMER_CONFIGURE: {
		struct timer_cfg_arg *cfg_arg = arg;
		ret = timer_ops(dev)->setup(dev, cfg_arg->ch, cfg_arg->config, cfg_arg->param, cfg_arg->isr, cfg_arg->ctx);
		break;
	}
	case IOCTL_TIMER_START: {
		struct timer_start_arg *start_arg = arg;
		ret = timer_ops(dev)->start(dev, start_arg->ch);
		break;
	}
	case IOCTL_TIMER_STOP: {
		struct timer_stop_arg *stop_arg = arg;
		ret = timer_ops(dev)->stop(dev, stop_arg->ch);
		break;
	}
	case IOCTL_TIMER_START_MULTI: {
		struct timer_start_multi_arg *start_arg = arg;
		ret = timer_ops(dev)->start_multi(dev, start_arg->chs);
		break;
	}
	case IOCTL_TIMER_STOP_MULTI: {
		struct timer_stop_multi_arg *stop_arg = arg;
		ret = timer_ops(dev)->stop_multi(dev, stop_arg->chs);
		break;
	}
	case IOCTL_TIMER_VALUE: {
		struct timer_value_arg *value_arg = arg;
		uint32_t value;
		value = timer_ops(dev)->get_value(dev, value_arg->ch);
		*value_arg->value = value;
		ret = 0;
		break;
	}
	default:
		ret = -EINVAL;
	}

	return ret;
}

__iram__ static int atcpit_timer_probe(struct device *timer)
{
	struct timer_driver_data *priv;
	char buf[32];
	struct file *file;
	int idx;

	priv = kmalloc(sizeof(*priv));
	if (!priv) {
		return -ENOMEM;
	}

	memset(priv, 0, sizeof(struct timer_driver_data));

	priv->clk = clk_get(timer, "pclk");
	if (priv->clk == NULL) {
		return -1;
	}

	idx = dev_id(timer);
	sprintf(buf, "/dev/timer%d", idx);

	priv->dev = timer;
	priv->devfs_ops.ioctl = atcpit_timer_ioctl;

	file = vfs_register_device_file(buf, &priv->devfs_ops, priv);
	if (!file) {
		printk("%s: failed to register as %s\n", dev_name(timer), buf);
		return -1;
	}

	printk("TIMER: %s registered as %s\n", dev_name(timer), buf);

	request_irq(timer->irq[0], atcpit_timer_irq, dev_name(timer),
				timer->pri[0], timer);

	timer->driver_data = priv;

	return 0;
}

int atcpit_timer_shutdown(struct device *timer)
{
	u8 ch;
	for (ch = 0; ch < CONFIG_NR_CH; ch++) {
	    atcpit_timer_stop(timer, ch);
	}
	free_irq(timer->irq[0], dev_name(timer));
	free(timer->driver_data);

	return 0;
}

struct timer_ops atcpit_timer_ops = {
	.setup = atcpit_timer_setup,
	.start = atcpit_timer_start,
	.stop = atcpit_timer_stop,
	.start_multi = atcpit_timer_start_multi,
	.stop_multi = atcpit_timer_stop_multi,
	.get_rate = atcpit_timer_get_rate,
	.get_value = atcpit_timer_get_value,
	.adjust = atcpit_timer_adjust,
};

static declare_driver(timer) = {
	.name = "timer",
	.probe = atcpit_timer_probe,
    .shutdown = atcpit_timer_shutdown,
	.ops = &atcpit_timer_ops,
};

#ifdef CONFIG_SOC_SCM2010
#if !defined(CONFIG_USE_TIMER0) && !defined(CONFIG_USE_TIMER1)
#error Timer driver requires timer devices. Select Timer devices or remove the driver
#endif
#endif

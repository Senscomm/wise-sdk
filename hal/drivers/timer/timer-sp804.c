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
#include <soc.h>
#include <hal/kernel.h>
#include <hal/timer.h>
#include <hal/console.h>

/* FIXME: read out from configuration register */
#define CONFIG_NR_TIMER 4

/* SP804 registers */
#define LOD    		0x0
#define VAL    		0x4
#define CTL   		0x8
#define CLI	  	0xC
#define	RIS 		0x10
#define	MIS 		0x14
#define	RLD		0x18
#ifdef CONFIG_SOC_BCM2836
/*
 * The Timer in BCM2836 is based on ARM SP804, but it has
 * a few differences from the standard SP804:
 * - There is only one timer.
 * - It only runs in continuous mode.
 * - It has an extra clock pre-divider register.
 * - It has an extra stop-in-debug-mode control bit.
 *   It also has a 32-bit free running counter.
 */
#define	DIV 		0x1C
#define	CNT		0x20
#endif

struct timer_driver_data {
	struct clk *clk;
	u32 reload;
	u32 rate;
	u32 wrap;
	u32 config;
	timer_isr isr;
} timer_data[CONFIG_NR_TIMER];

static u32 timer_readl(struct device *timer, u32 oft)
{
	return readl(timer->base[0] + oft);
}

static void timer_writel(u32 val, struct device *timer, u32 oft)
{
	writel(val, timer->base[0] + oft);
}

static void timer_reload(struct device *timer, u32 reload, bool enable)
{
	u32 ctl;

	timer_writel(reload, timer, LOD);
	if (enable) {
		ctl = timer_readl(timer, CTL);
		timer_writel(ctl | (1 << 7), timer, CTL);
	}
}

static int sp804_timer_irq(int irq, void *data)
{
	struct device *timer = data;
	struct timer_driver_data *priv;
	u32 st, rl;

	st = timer_readl(timer, MIS);
	if (!st)
		return 0;

	timer_writel(0, timer, CLI);

	priv = &timer_data[dev_id(timer)];
	switch (priv->config & 0xF) {
	case HAL_TIMER_ONESHOT:
#ifdef CONFIG_SOC_BCM2836
		/* Not supported */
		assert(0);
#endif
		if (priv->isr)
			priv->isr();
		break;
	case HAL_TIMER_PERIODIC:
		rl = timer_readl(timer, LOD);
		if (rl != priv->reload)
			timer_reload(timer, priv->reload, false);
		if (priv->isr)
			priv->isr();
		break;
	default:
		break;
	}

	return 0;
}

static int sp804_timer_probe(struct device *timer)
{
	struct timer_driver_data *priv;

	priv = &timer_data[dev_id(timer)];
	priv->clk = clk_get(timer, "pclk");
	if (priv->clk == NULL) {
		return -1;
	}

	timer->driver_data = priv;

	return 0;
}

static int sp804_timer_setup(struct device *timer, u32 config,
			      u32 param, timer_isr isr)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 ctl;
	u32 reload = 0, prescaler;
	unsigned long tclk;

	ctl = timer_readl(timer, CTL);
	tclk = clk_get_rate(priv->clk);

	switch (config & 0xF) {
	case HAL_TIMER_FREERUN:
#ifdef CONFIG_SOC_BCM2836
		if (isr)
			return -ENOTSUP;
		/* @param is rate (in HZ) */
		prescaler = tclk / param - 1;
		ctl &= ~(0x00FF0000);
		ctl |= (prescaler << 16);
		timer_writel(ctl, timer, CTL);
#endif
		break;
	case HAL_TIMER_ONESHOT:
#ifdef CONFIG_SOC_BCM2836
		/* Not supported */
		assert(0);
#endif
		/* @param is interval in usec */
		/* fall-through */
	case HAL_TIMER_PERIODIC:
#ifdef CONFIG_SOC_BCM2836
		prescaler = tclk / 1000000 - 1;
		timer_writel(prescaler, timer, DIV);
#endif
		reload = param - 1;
		timer_writel(reload, timer, LOD);
		timer_writel(reload, timer, RLD);
		break;
	}

	priv->reload = reload;
	priv->rate = param;
	priv->config = config;
	if (config & HAL_TIMER_IRQ)
		priv->isr = isr;
	if (isr)
		request_irq(timer->irq[0], sp804_timer_irq, dev_name(timer),
				timer->pri[0], timer);

	return 0;
}

static int sp804_timer_start(struct device *timer)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 ctl;

	ctl = timer_readl(timer, CTL);

#ifdef CONFIG_SOC_BCM2836
	if ((priv->config & 0xF) == HAL_TIMER_FREERUN) {
		ctl |= (1 << 9);
		timer_writel(ctl, timer, CTL);
		return 0;
	}
#endif
	if (priv->isr)
		ctl |= (1 << 5);
	ctl |= (1 << 7);

	timer_writel(ctl, timer, CTL);

	return 0;
}

static int sp804_timer_stop(struct device *timer)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 ctl;

	ctl = timer_readl(timer, CTL);

#ifdef CONFIG_SOC_BCM2836
	if ((priv->config & 0xF) == HAL_TIMER_FREERUN) {
		ctl &= ~(1 << 9);
		timer_writel(ctl, timer, CTL);
		return 0;
	}
#endif

	if (priv->isr)
		ctl &= ~(1 << 5);
	ctl &= ~(1 << 7);

	timer_writel(ctl, timer, CTL);

	return 0;
}

static unsigned long sp804_timer_get_rate(struct device *timer)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 tclk;
	u32 ctl;

	ctl = timer_readl(timer, CTL);
	tclk = clk_get_rate(priv->clk);
#ifdef CONFIG_SOC_BCM2836
	if ((priv->config & 0xF) == HAL_TIMER_FREERUN)
		return tclk / (((ctl & 0x00FF0000) >> 16) + 1);
	return tclk / (timer_readl(timer, DIV) + 1);
#else
	return tclk;
#endif
}

static u32 sp804_timer_get_value(struct device *timer)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 cnt;
	u32 flags;

	local_irq_save(flags);

	if ((priv->config & 0xF) == HAL_TIMER_FREERUN) {
#ifdef CONFIG_SOC_BCM2836
		cnt = timer_readl(timer, CNT);
		goto out;
#else
		assert(0);
#endif
	}

	cnt = timer_readl(timer, VAL);

 out:
	local_irq_restore(flags);
	return cnt;

}

struct timer_ops sp804_timer_ops = {
	.setup = sp804_timer_setup,
	.start = sp804_timer_start,
	.stop = sp804_timer_stop,
	.get_rate = sp804_timer_get_rate,
	.get_value = sp804_timer_get_value,
};

static declare_driver(timer) = {
	.name = "timer",
	.probe = sp804_timer_probe,
	.ops = &sp804_timer_ops,
};

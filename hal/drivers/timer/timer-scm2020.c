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

#define CONFIG_NR_TIMER 3

#define TMR_CER    	0x00	/* Count Enable */
#define TMR_CMR    	0x04	/* Count Mode */
#define TMR_CRSR   	0x08	/* Count Restart */
#define TMR_CCR		0x0C	/* Clock Control */
#define TMR_Tn_Mm(n, m) (0x10 + 0x10 * n + 4 * m)	/* Match */
#define TMR_PLVRn(n)	(0x40 + 4 * n)	/* Preload Value */
#define TMR_PLCRn(n)	(0x50 + 4 * n)  /* Preload Control */
#define TMR_IERn(n)	(0x60 + 4 * n)	/* Interrupt Enable */
#define TMR_ICRn(n)	(0x70 + 4 * n)	/* Interrupt Clear */
#define TMR_SRn(n)	(0x80 + 4 * n)	/* Status */
#define TMR_CRn(n)	(0x90 + 4 * n)	/* Count */

struct timer_driver_data {
	struct clk *clk;
	u32 inst;
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

static int timer_has_pending_irq(struct device *timer)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 inst = priv->inst;
	u32 irq = timer_readl(timer, TMR_SRn(inst));

	return (irq & 0x7) != 0;
}

/* irq handler for a single instance */
static int sc20_timer_irq(int irq, void *data)
{
	struct device *timer = data;
	struct timer_driver_data *priv = timer->driver_data;
	u32 inst = priv->inst;
	u32 st, en;

	st = timer_readl(timer, TMR_SRn(inst));
	timer_writel(st, timer, TMR_ICRn(inst));

	switch (priv->config & 0xf) {
		case HAL_TIMER_FREERUN:
			priv->wrap++;
			break;
		case HAL_TIMER_ONESHOT:
			en = timer_readl(timer, TMR_CER);
			timer_writel(en & (1 << inst), timer, TMR_CER);
			if (priv->isr)
				priv->isr();
			break;
		case HAL_TIMER_PERIODIC:
			if (priv->isr)
				priv->isr();
			break;
		default:
			break;
	}

	return 0;
}

static int sc20_timer_probe(struct device *timer)
{
	struct timer_driver_data *priv;

	priv = &timer_data[dev_id(timer)];
	priv->clk = clk_get(timer, "pclk");
	if (priv->clk == NULL) {
		return -1;
	}
	priv->inst = dev_id(timer);

	timer->driver_data = priv;

	return 0;
}

static int sc20_timer_setup(struct device *timer, u32 config,
			      u32 param, timer_isr isr)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 cmod;
	u32 pctl;
	u32 match = 0;
	u32 tclk;
	bool freerun = false;
	u32 inst = priv->inst;

	/* Count Mode register : periodic timer mode */
	cmod = timer_readl(timer, TMR_CMR);
	cmod = cmod & ~(1 << inst);
	timer_writel(cmod, timer, TMR_CMR);

	tclk = clk_get_rate(priv->clk);

	switch (config & 0xf) {
		case HAL_TIMER_FREERUN:
			if (isr)
				return -ENOTSUP;
			/* @param is rate (in HZ) */
			/* Free-run timer is implemented by software */
			/* HW will be expired every 10 sec. to wrap */
			match = (tclk * 10 - 1);
			freerun = true;
			break;
		case HAL_TIMER_ONESHOT:
			/* @param is interval in usec */
			/* fall-through */
		case HAL_TIMER_PERIODIC:
			if(tclk < 10000000)
				match = (tclk * param / 1000000  - 1);
			else
				match = (tclk / 1000000 * param  - 1);
			break;
	}

	/* Match register */
	timer_writel(match, timer, TMR_Tn_Mm(inst, 0));

	/* Preload Value register : reload with 0 when matched  */
	timer_writel(0, timer, TMR_PLVRn(inst));

	/* Preload Control register : enable Match comparator 0 */
	pctl = timer_readl(timer, TMR_PLCRn(inst));
	pctl = (pctl & ~0x3) | 0x1;
	timer_writel(pctl, timer, TMR_PLCRn(inst));

	priv->reload = match;
	priv->rate = param;
	priv->config = config;
	if (config & HAL_TIMER_IRQ)
		priv->isr = isr;
	if (freerun || isr)
		request_irq(timer->irq[0], sc20_timer_irq, dev_name(timer),
				timer->pri[0], timer);

	return 0;
}

static int sc20_timer_start(struct device *timer)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 en;
	bool freerun = ((priv->config & 0xf) == HAL_TIMER_FREERUN);
	u32 inst = priv->inst;

	if (freerun || priv->isr) {
		en = timer_readl(timer, TMR_IERn(inst));
		timer_writel(en | 0x1, timer, TMR_IERn(inst));
	}

	en = timer_readl(timer, TMR_CER);
	timer_writel(en | (1 << inst), timer, TMR_CER);

	return 0;
}

static int sc20_timer_stop(struct device *timer)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 en;
	bool freerun = ((priv->config & 0xf) == HAL_TIMER_FREERUN);
	u32 inst = priv->inst;

	en = timer_readl(timer, TMR_CER);
	timer_writel(en & ~(1 << inst), timer, TMR_CER);
	if (freerun || priv->isr) {
		en = timer_readl(timer, TMR_IERn(inst));
		timer_writel(en & ~0x1, timer, TMR_IERn(inst));
	}
	return 0;
}

static unsigned long sc20_timer_get_rate(struct device *timer)
{
	struct timer_driver_data *priv = timer->driver_data;

	return clk_get_rate(priv->clk);
}

static u32 sc20_timer_get_value(struct device *timer)
{
	struct timer_driver_data *priv = timer->driver_data;
	u32 cnt = 0, ratio;
	u32 clkr = clk_get_rate(priv->clk);
	u32 inst = priv->inst;

	cnt = timer_readl(timer, TMR_CRn(inst));
	if ((priv->config & 0xf) != HAL_TIMER_FREERUN)
		goto out;

	if (timer_has_pending_irq(timer))
		sc20_timer_irq(timer->irq[0], timer);

	ratio = clkr / priv->rate;
	cnt /=  ratio;
	cnt += (priv->reload / ratio) * priv->wrap;

 out:
	return cnt;

}

struct timer_ops sc20_timer_ops = {
	.setup = sc20_timer_setup,
	.start = sc20_timer_start,
	.stop = sc20_timer_stop,
	.get_rate = sc20_timer_get_rate,
	.get_value = sc20_timer_get_value,
};

static declare_driver(timer) = {
	.name = "timer",
	.probe = sc20_timer_probe,
	.ops = &sc20_timer_ops,
};

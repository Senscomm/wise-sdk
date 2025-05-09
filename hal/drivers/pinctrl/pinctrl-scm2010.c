/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <soc.h>
#include <hal/kernel.h>
#include <hal/console.h>
#include <hal/device.h>
#include <hal/pinctrl.h>
#include <hal/clk.h>

#include <cmsis_os.h>

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "vfs.h"

#define dbg(args, ...)

/* SCM2010 GPIO registers definitions */
#define OFT_GPIO_IN          0x00 /* PAD X's value */
#define OFT_GPIO_PU          0x04 /* PAD X's pull up enable */
#define OFT_GPIO_PD          0x08 /* PAD X's pull down enable */
#define OFT_GPIO_IE          0x0c /* PAD X's input enable */
#define OFT_GPIO_OEN         0x10 /* PAD X's output enable */
#define OFT_GPIO_OUT         0x14 /* PAD X's output value */
#define OFT_GPIO_DS          0x18 /* PAD X's driving strength */
#define OFT_GPIO_MODE0       0x20 /* PAD [ 7: 0] MODE selector */
#define OFT_GPIO_MODE1       0x24 /* PAD [15: 8] MODE selector */
#define OFT_GPIO_MODE2       0x28 /* PAD [23:16] MODE selector */
#define OFT_GPIO_MODE3       0x2c /* PAD [24:24] MODE selector */
#define OFT_GPIO_EDGE_RISE   0x30 /* PAD X's rising edge interrupt */
#define OFT_GPIO_EDGE_FALL   0x40 /* PAD X's falling edge interrupt */
#define OFT_GPIO_EDGE_CLR    0x50 /* PAD X's edge check clear */
#define OFT_GPIO_EDGE_INTR   0x60 /* PAD X's edge interrupt status */


/* SCM2010 pinmux definitions */

#define MUX_NUM_BANK		 (3) /* GPIO_MODE[2-0] */
#define MUX_SEL_MASK(w) 	 ((1 << w) - 1)

#define pinctrl_addr(p, oft) (u32 *)((p)->base[1] + oft * 4)
#define gpio_addr(p, oft) (u32 *)((p)->base[0] + oft)

#define MAX_GPIO_NUM		 (25)

static struct pinctrl_pin_hw_mux_desc {
	u16 bank     :2; 	/* GPIO_MODE[bank] */
	u16 offset   :5;	/* bit offset from base addr */
	u16 width    :3;	/* # of bits, 0 : not available */
	u16 gpio_fsel:4;    /* fsel for gpio function */
} pinctrl_hw_mxdesc[] = {
	[0] 	= 	{0,  0, 4, 8},
	[1] 	= 	{0,  4, 4, 8},
	[2] 	= 	{0,  8, 4, 8},
	[3] 	= 	{0, 12, 4, 8},
	[4] 	= 	{0, 16, 4, 8},
	[5] 	= 	{0, 20, 4, 8},
	[6] 	= 	{0, 24, 4, 8},
	[7] 	= 	{0, 28, 4, 8},
	[8] 	= 	{1,  0, 4, 8},
	[9] 	= 	{1,  4, 4, 8},
    [10] 	= 	{1,  8, 4, 8},
    [11] 	= 	{1, 12, 4, 8},
    [12] 	= 	{1, 16, 4, 8},
    [13] 	=   {1, 20, 4, 8},
    [14] 	= 	{1, 24, 4, 8},
    [15] 	= 	{1, 28, 4, 8},
    [16] 	=   {2,  0, 4, 8},
    [17] 	=   {2,  4, 4, 8},
    [18] 	= 	{2,  8, 4, 8},
    [19] 	= 	{2, 12, 4, 8},
    [20] 	= 	{2, 16, 4, 8},
    [21] 	= 	{2, 20, 4, 8},
    [22] 	= 	{2, 24, 4, 8},
    [23] 	= 	{2, 28, 4, 8},
    [24] 	= 	{3,  0, 4, 8},
};

static struct pinctrl_driver_data {
	int pull_en; 	/* pull support*/
	int int_en;  	/* interrupt support*/
	int debnce_en;	/* debounce support */
	int ch_num;	/* # of gpio channels */
} pmx_drv_data;

static struct pinctrl_pin_desc pmx_pins[MAX_GPIO_NUM];

struct pinmux {
	u32 pin;
	const char *func;
	u32 fsel; /* function: 0 - 10 */
	struct pinctrl_pin_desc *desc;
};

struct pmx_func {
	const char *devname;
	struct pinmux *pmx;
	unsigned nr_pmx;
};

struct scm2010_gpio_intr {
	void (*intr_cb)(u32 pin, void *ctx);
	void *ctx;
};

struct gpio_driver_data {
	struct fops devfs_ops;
	struct device *dev;
#ifdef CONFIG_SCM2010_PINCTRL_INT_WORKAROUND
	osTimerId_t int_wa_timer;
	int int_wa_last_tick; /* in ticks */
	int int_wa_period;
	int int_wa_duration;
#endif
} gpio_data;

static struct scm2010_gpio_intr gpio_intr[MAX_GPIO_NUM];

#define MUX(id, nr, value) {	\
	.pin	= nr,		\
	.func 	= id,		\
	.fsel 	= value		\
}

#ifdef CONFIG_USE_UART0
struct pinmux uart0_pin_mux[] = {
	MUX("cts", 19, 3),
	MUX("rts", 20, 3),
	MUX("rxd",  0, 4),
	MUX("rxd", 21, 0),
	MUX("txd",  1, 4),
	MUX("txd", 22, 0),
	MUX("txd",  8, 4),
};
#endif

#ifdef CONFIG_USE_UART1
struct pinmux uart1_pin_mux[] = {
	MUX("cts", 19, 4),
	MUX("rts", 20, 4),
	MUX("rxd",  0, 0),
	MUX("rxd",  6, 4),
	MUX("rxd", 17, 4),
	MUX("txd",  1, 0),
	MUX("txd",  7, 4),
	MUX("txd",  8, 3),
	MUX("txd", 18, 4),
};
#endif

#ifdef CONFIG_USE_UART2
struct pinmux uart2_pin_mux[] = {
	MUX("cts", 17, 3),
	MUX("rts", 18, 3),
	MUX("rxd",  0, 3),
	MUX("rxd",  4, 4),
	MUX("rxd", 15, 3),
	MUX("rxd", 15, 4),
	MUX("txd",  2, 2),
	MUX("txd",  5, 4),
	MUX("txd",  1, 3),
	MUX("txd",  8, 0),
	MUX("txd", 16, 3),
	MUX("txd", 16, 4),
};
#endif

#ifdef CONFIG_USE_TIMER0_PWM
struct pinmux timer0_pwm_pin_mux[] = {
	MUX("pwm0", 0, 7),
	MUX("pwm1", 1, 7),
	MUX("pwm2", 2, 7),
	MUX("pwm3", 3, 7),
	MUX("pwm0",15, 7),
	MUX("pwm1",16, 7),
	MUX("pwm2",17, 7),
	MUX("pwm3",18, 7),
	MUX("pwm0",21, 7),
	MUX("pwm1",22, 7),
	MUX("pwm2",23, 7),
	MUX("pwm3",24, 7),
};
#endif

#ifdef CONFIG_USE_TIMER1_PWM
struct pinmux timer1_pwm_pin_mux[] = {
	MUX("pwm0", 4, 7),
	MUX("pwm1", 5, 7),
	MUX("pwm2", 6, 7),
	MUX("pwm3", 7, 7),
	MUX("pwm0",19, 7),
	MUX("pwm1",20, 7),
};
#endif

#ifdef CONFIG_USE_SPI0
struct pinmux spi0_pin_mux[] = {
	MUX("clk", 12, 0),
	MUX("cs",  11, 0),
	MUX("mosi",13, 0),
	MUX("miso",14, 0),
	MUX("wp",  10, 0),
	MUX("hold", 9, 0),
};
#endif

#ifdef CONFIG_USE_SPI0
#endif

#ifdef CONFIG_USE_SPI1
struct pinmux spi1_pin_mux[] = {
	MUX("clk", 16, 0),
	MUX("cs",  15, 0),
	MUX("mosi",17, 0),
	MUX("miso",18, 0),
	MUX("wp",  19, 0),
	MUX("hold",20, 0),
};
#endif

#ifdef CONFIG_USE_SPI2
struct pinmux spi2_pin_mux[] = {
	MUX("clk", 3, 5),
	MUX("cs",  2, 5),
	MUX("mosi",4, 5),
	MUX("miso",5, 5),
	MUX("wp",  6, 5),
	MUX("hold",7, 5),

	MUX("clk", 16, 5),
	MUX("cs",  15, 5),
	MUX("mosi",17, 5),
	MUX("miso",18, 5),
	MUX("wp",19, 5),
	MUX("hold",20, 5),

	MUX("clk", 22, 5),
	MUX("cs",  21, 5),
	MUX("mosi",23, 5),
	MUX("miso",24, 5),
};
#endif

#ifdef CONFIG_SDIO
struct pinmux sdio_pin_mux[] = {
	MUX("data2", 15, 1),
	MUX("data3", 16, 1),
	MUX("clk",   17, 1),
	MUX("cmd",   18, 1),
	MUX("data0", 19, 1),
	MUX("data1", 20, 1),
};
#endif

#ifdef CONFIG_USE_I2C0
struct pinmux i2c0_pin_mux[] = {
	MUX("scl",  2, 6),
	MUX("sda",  3, 6),
	MUX("scl", 15, 6),
	MUX("sda", 16, 6),
	MUX("scl", 23, 0),
	MUX("sda", 24, 0),
};
#endif

#ifdef CONFIG_USE_I2C1
struct pinmux i2c1_pin_mux[] = {
	MUX("scl",  4, 6),
	MUX("sda",  5, 6),
	MUX("scl",  6, 0),
	MUX("sda",  7, 0),
	MUX("scl", 17, 6),
	MUX("sda", 18, 6),
	MUX("scl", 19, 6),
	MUX("sda", 20, 6),
};
#endif

#ifdef CONFIG_USE_I2S
struct pinmux i2s_pin_mux[] = {
	MUX("din",  3, 2),
	MUX("wclk", 4, 2),
	MUX("dout", 5, 2),
	MUX("bclk", 6, 2),
	MUX("mclk", 7, 2),
	MUX("din", 16, 2),
	MUX("wclk",17, 2),
	MUX("dout",18, 2),
	MUX("bclk",19, 2),
	MUX("mclk",20, 2),
};
#endif


#define pf(name, m) { 				\
	.devname = name, 			\
	.pmx = m##_pin_mux, 			\
	.nr_pmx = ARRAY_SIZE(m##_pin_mux), 	\
}

struct pmx_func pmx_functions[] = {
#ifdef CONFIG_USE_UART0
	pf("atcuart.0", uart0),
#endif
#ifdef CONFIG_USE_UART1
	pf("atcuart.1", uart1),
#endif
#ifdef CONFIG_USE_UART2
	pf("atcuart.2", uart2),
#endif
#ifdef CONFIG_USE_TIMER0_PWM
	pf("timer.0", timer0_pwm),
#endif
#ifdef CONFIG_USE_TIMER1_PWM
	pf("timer.1", timer1_pwm),
#endif
#ifdef CONFIG_USE_SPI0
#ifdef CONFIG_USE_SPI0_FLASH
	pf("atcspi200-xip.0", spi0),
#else
	pf("atcspi.0", spi0),
#endif
#ifdef CONFIG_USE_SPI1
#ifdef CONFIG_USE_SPI1_FLASH
	pf("atcspi200-xip.1", spi1),
#else
	pf("atcspi.1", spi1),
#endif
#endif
#ifdef CONFIG_USE_SPI2
#ifdef CONFIG_USE_SPI2_FLASH
	pf("atcspi200-xip.2", spi2),
#else
	pf("atcspi.2", spi2),
#endif
#endif
#endif
#ifdef CONFIG_SDIO
	pf("sdio", sdio),
#endif
#ifdef CONFIG_USE_I2C0
	pf("atci2c.0", i2c0),
#endif
#ifdef CONFIG_USE_I2C1
	pf("atci2c.1", i2c1),
#endif
#ifdef CONFIG_USE_I2S
	pf("python-i2s", i2s),
#endif
};

static struct pinmux *pinctrl_get_pinmux(const char *devname,
		const char *id, u32 pin)
{
	struct pmx_func *pfn;
	struct pinmux *pmx;
	int i;

	for (i = 0; i < ARRAY_SIZE(pmx_functions); i++) {
		pfn = &pmx_functions[i];
		if (!strcmp(pfn->devname, devname))
			goto pfn_found;
	}
	return NULL;

 pfn_found:
	for (i = 0; i < pfn->nr_pmx; i++) {
		pmx = &pfn->pmx[i];
		if (!strcmp(pmx->func, id)
				&& pmx->pin == pin)
			return pmx;
	}

	return NULL;
}

static int scm2010_gpio_set_direction(struct device *pctl, u32 pin, bool input)
{
	u32 *addr, v;
	struct pinctrl_driver_data *prv = pctl->driver_data;

	if (pin >= prv->ch_num) {
		return -EINVAL;
	}

	if (input) {
		addr = gpio_addr(pctl, OFT_GPIO_OEN);
		v = readl(addr);
		v &= ~(1 << pin);
		writel(v, addr);
		addr = gpio_addr(pctl, OFT_GPIO_IE);
		v = readl(addr);
		v |= (1 << pin);
		writel(v, addr);
	} else {
		addr = gpio_addr(pctl, OFT_GPIO_IE);
		v = readl(addr);
		v &= ~(1 << pin);
		writel(v, addr);
		addr = gpio_addr(pctl, OFT_GPIO_OEN);
		v = readl(addr);
		v |= (1 << pin);
		writel(v, addr);
	}

	return 0;
}

static int pmx_bank(u32 pin)
{
	return pinctrl_hw_mxdesc[pin].bank;
}

static u8 pmx_get(struct device *pctl, u32 pin)
{
	u32 *addr, v;
	int index;
	u32 mask, width, shift;

	index = pmx_bank(pin);
	addr = pinctrl_addr(pctl, index);

	shift = pinctrl_hw_mxdesc[pin].offset;
	width = pinctrl_hw_mxdesc[pin].width;
	mask = MUX_SEL_MASK(width) << shift;

	if (!mask)
		return 0;

	v = readl(addr) & mask;

	return v >> shift;
}

static void pmx_eval(u32 pin, u8 func, u32 *pm)
{
	u32 mask, width, shift;

	shift = pinctrl_hw_mxdesc[pin].offset;
	width = pinctrl_hw_mxdesc[pin].width;
	mask = MUX_SEL_MASK(width) << shift;

	if (!mask)
		return;

	*pm &= ~mask;
	*pm |= func << shift;
}

static void pmx_set(struct device *pctl, u32 pin, u8 func)
{
	u32 *addr;
	u32 v;

	addr = pinctrl_addr(pctl, pmx_bank(pin));
	v = readl(addr);
	pmx_eval(pin, func, &v);

	writel(v, addr);
}

static void pmx_write(struct device *pctl, struct pinmux *pmx)
{
	unsigned long flags;

	local_irq_save(flags);

	pmx_set(pctl, pmx->pin, pmx->fsel);

	local_irq_restore(flags);
}

static int scm2010_pmx_request(struct device *pctl, u32 pin)
{
	struct pinctrl_driver_data *prv = pctl->driver_data;
	assert(pin < prv->ch_num);
	/*
	 * There is nothing to do here because
	 * actual pinmux setting will be done
	 * by scm2010_pmx_set_mux()
	 */
	return 0;
}

static int scm2010_pmx_free(struct device *pctl, u32 pin)
{
	struct pinmux *pmx;
	struct pinctrl_driver_data *prv = pctl->driver_data;

	assert(pin < prv->ch_num);

	pmx = pmx_pins[pin].driver_data; /* set_mux() */
	if (pmx) {
		pmx_pins[pin].driver_data = NULL;
		/*
		 * Make sure it turn to be GPIO input
		 * with pull-up or pull-down to prevent
		 * leakage
		 */
	}

	return 0;
}

static int scm2010_gpio_request_enable(struct device *pctl, u32 pin)
{
	struct pinctrl_driver_data *prv = pctl->driver_data;

	if (pin > prv->ch_num) {
		return -EINVAL;
	}

	if (pmx_get(pctl, pin) == pinctrl_hw_mxdesc[pin].gpio_fsel) {
		/* Already set GPIO */
		return 0;
	}

	/* Safe to set the direction input before enabling GPIO function */
	scm2010_gpio_set_direction(pctl, pin, true);

	pmx_set(pctl, pin, pinctrl_hw_mxdesc[pin].gpio_fsel);

	return 0;
}

static int scm2010_gpio_disable_free(struct device *pctl, u32 pin)
{
	struct pinctrl_driver_data *prv = pctl->driver_data;

	assert(pin < prv->ch_num);

	scm2010_gpio_set_direction(pctl, pin, false);

	return 0;
}

static int scm2010_pmx_set_mux(struct device *pctl, struct device *client,
		const char *id, u32 pin)
{
	struct pinmux *pmx = pinctrl_get_pinmux(dev_name(client), id, pin);
	struct pinctrl_driver_data *prv = pctl->driver_data;

	assert(pin < prv->ch_num);

	if (!pctl || !pmx)
		return -ENODEV;

	pmx_write(pctl, pmx);

	pmx_pins[pin].driver_data = pmx; /* keep mux owner */
	pmx->desc = &pmx_pins[pin];

	return 0;
}

#ifdef CONFIG_SCM2010_PINIO_PM_CTRL
__iram__ int scm2010_pm_powerdown(struct device *pctl)
{
#ifdef CONFIG_PM_IO

	/* Nothing to do. Will be taken care of PM, WLAN, and RF drivers.
	 */

#endif

	return 0;
}

__iram__ int scm2010_pm_powerup(struct device *pctl)
{
#ifdef CONFIG_PM_IO

	/* Nothing to do. Will be taken care of PM, WLAN, and RF drivers.
	 */

#endif
	return 0;
}
#endif

static struct pinmux_ops scm2010_pmx_ops = {
	.request 	= scm2010_pmx_request,
	.free_pmx 		= scm2010_pmx_free,
	.set_mux	= scm2010_pmx_set_mux,
	.gpio_request_enable = scm2010_gpio_request_enable,
	.gpio_disable_free = scm2010_gpio_disable_free,
	.gpio_set_direction = scm2010_gpio_set_direction,
};

static int scm2010_pin_config_set(struct device *pctl, unsigned pin, unsigned config)
{
	u32 cmd, arg, v;
	u32 mask, sel;
	volatile u32 *addr;
	struct pinctrl_driver_data *prv = pctl->driver_data;

	if (pin >= prv->ch_num) {
		return -EINVAL;
	}

	cmd = config >> 16;
	arg = config & PIN_CONFIG_PARAM_MASK;

	mask = 1 << pin;
	sel = arg & 0x1;

	if (cmd == PIN_CONFIG_INPUT_ENABLE) {
#if 0
        /* XXX: Not sure about this */

		assert(prv->int_en);
		addr = gpio_addr(pctl, OFT_PD_INEN);
		v = readl(addr) & ~mask;
		v |= sel;
		writel(v, addr);
#endif
	} else {
		if (sel)
			assert(prv->pull_en);
		if (cmd == PIN_CONFIG_PULL_DISABLE) {
			addr = gpio_addr(pctl, OFT_GPIO_PD);
			v = readl(addr) & ~mask;
			writel(v, addr);
			addr = gpio_addr(pctl, OFT_GPIO_PU);
			v = readl(addr) & ~mask;
			writel(v, addr);
		} else if (cmd == PIN_CONFIG_PULL_UP) {
			addr = gpio_addr(pctl, OFT_GPIO_PD);
			v = readl(addr) & ~mask;
			writel(v, addr);
			addr = gpio_addr(pctl, OFT_GPIO_PU);
			v = readl(addr) | mask;
			writel(v, addr);
		} else if (cmd == PIN_CONFIG_PULL_DOWN) {
			addr = gpio_addr(pctl, OFT_GPIO_PU);
			v = readl(addr) & ~mask;
			writel(v, addr);
			addr = gpio_addr(pctl, OFT_GPIO_PD);
			v = readl(addr) | mask;
			writel(v, addr);
		} else if (cmd == PIN_CONFIG_DRIVE_STRENGTH) {
			addr = gpio_addr(pctl, OFT_GPIO_DS);
			v = readl(addr) | mask;
			writel(v, addr);
		}
	}

	return 0;
}

static struct pinconf_ops scm2010_pinconf_ops = {
	.pin_config_set = scm2010_pin_config_set,
};

static int scm2010_gpio_get_value(struct device *pctl, u32 pin)
{
	u32 *addr, v;
	unsigned long flags;
	struct pinctrl_driver_data *prv = pctl->driver_data;

	if (pin >= prv->ch_num) {
		return -EINVAL;
	}

	addr = gpio_addr(pctl, OFT_GPIO_OEN);
	v = readl(addr);

	if (v & (1 << pin)) {
		addr = gpio_addr(pctl, OFT_GPIO_OUT);
	} else {
		addr = gpio_addr(pctl, OFT_GPIO_IN);
	}

	local_irq_save(flags);
	v = readl(addr) >> pin;
	local_irq_restore(flags);

	return v & 1;
}

static int scm2010_gpio_set_value(struct device *pctl, u32 pin, int value)
{
	u32 *addr, v;
	unsigned long flags;
	struct pinctrl_driver_data *prv = pctl->driver_data;

	if (pin >= prv->ch_num) {
		return -EINVAL;
	}

	addr = gpio_addr(pctl, OFT_GPIO_OUT);
	value = (value & 1) << pin;

	local_irq_save(flags);
	v = readl(addr) & ~(1 << pin);
	v |= value;
	writel(v, addr);
	local_irq_restore(flags);

	return 0;
}

static int scm2010_gpio_irq_enable(struct device *pctl, u32 pin, int edge,
		void (*intr_cb)(u32 pin, void *ctx), void *ctx)
{
	u32 *addr, v;
	unsigned long flags;
	struct pinctrl_driver_data *prv = pctl->driver_data;

	if (pin >= prv->ch_num) {
		return -EINVAL;
	}

	local_irq_save(flags);

	addr = gpio_addr(pctl, OFT_GPIO_EDGE_CLR);
	v = readl(addr);
	v &= ~(1 << pin);
	writel(v, addr);

	if (edge == PIN_INTR_RISE_EDGE) {
		addr = gpio_addr(pctl, OFT_GPIO_EDGE_RISE);
		v = readl(addr);
		v |= (1 << pin);
		writel(v, addr);

		addr = gpio_addr(pctl, OFT_GPIO_EDGE_FALL);
		v = readl(addr);
		v &= ~(1 << pin);
		writel(v, addr);
	} else if (edge == PIN_INTR_FALL_EDGE) {
		addr = gpio_addr(pctl, OFT_GPIO_EDGE_FALL);
		v = readl(addr);
		v |= (1 << pin);
		writel(v, addr);

		addr = gpio_addr(pctl, OFT_GPIO_EDGE_RISE);
		v = readl(addr);
		v &= ~(1 << pin);
		writel(v, addr);
	} else if (edge == PIN_INTR_BOTH_EDGE) {
		addr = gpio_addr(pctl, OFT_GPIO_EDGE_FALL);
		v = readl(addr);
		v |= (1 << pin);
		writel(v, addr);

		addr = gpio_addr(pctl, OFT_GPIO_EDGE_RISE);
		v = readl(addr);
		v |= (1 << pin);
		writel(v, addr);
	}

	gpio_intr[pin].intr_cb = intr_cb;
	gpio_intr[pin].ctx = ctx;

	local_irq_restore(flags);

	return 0;
}

static int scm2010_gpio_irq_disable(struct device *pctl, u32 pin)
{
	u32 *addr, v;
	unsigned long flags;
	struct pinctrl_driver_data *prv = pctl->driver_data;

	if (pin >= prv->ch_num) {
		return -EINVAL;
	}

	local_irq_save(flags);

	addr = gpio_addr(pctl, OFT_GPIO_EDGE_CLR);
	v = readl(addr);
	v |= (1 << pin);
	writel(v, addr);

	addr = gpio_addr(pctl, OFT_GPIO_EDGE_RISE);
	v = readl(addr) & ~(1 << pin);
	v &= ~(1 << pin);
	writel(v, addr);

	addr = gpio_addr(pctl, OFT_GPIO_EDGE_FALL);
	v = readl(addr) & ~(1 << pin);
	v &= ~(1 << pin);
	writel(v, addr);

	gpio_intr[pin].intr_cb = NULL;
	gpio_intr[pin].ctx = NULL;

	local_irq_restore(flags);

	return 0;
}

static struct gpio_ops scm2010_gpio_ops = {
	.get 	 = scm2010_gpio_get_value,
	.set 	 = scm2010_gpio_set_value,
	.irq_en  = scm2010_gpio_irq_enable,
	.irq_dis = scm2010_gpio_irq_disable,
};


#ifdef CONFIG_SCM2010_PINIO_PM_CTRL
static struct pin_pm_ops scm2010_pm_ops = {
	.powerdown 	= scm2010_pm_powerdown,
	.powerup 	= scm2010_pm_powerup,
};
#endif

struct pinctrl_ops scm2010_pinctrl_ops = {
	.pins 		= pmx_pins,
	.npins 		= ARRAY_SIZE(pmx_pins),
	.pmxops 	= &scm2010_pmx_ops,
	.confops 	= &scm2010_pinconf_ops,
	.gpops		= &scm2010_gpio_ops,
#ifdef CONFIG_SCM2010_PINIO_PM_CTRL
	.pmops		= &scm2010_pm_ops,
#endif
};

static int scm2010_gpio_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct gpio_driver_data *priv = file->f_priv;
	struct device *dev = priv->dev;
	struct gpio_configure *config;
	struct gpio_read_value *read_value;
	struct gpio_write_value *write_value;
	struct gpio_interrupt_enable *intr_en;
	struct gpio_interrupt_disable *intr_dis;
	int ret = 0;

	switch (cmd) {
	case IOCTL_GPIO_CONFIGURE:
		config = arg;

		/* set gpio */
		ret = scm2010_gpio_request_enable(dev, config->pin);

		if (config->property == GPIO_OUTPUT) {
			ret = scm2010_gpio_set_direction(dev, config->pin, false);
		} else {
			ret = scm2010_gpio_set_direction(dev, config->pin, true);
			if (!ret) {
				if (config->property == GPIO_INPUT) {
					ret = scm2010_pin_config_set(dev, config->pin,
							PIN_CONFIG_MK_CMD(PIN_CONFIG_PULL_DISABLE, 0));
				} else if (config->property == GPIO_INPUT_PULL_UP) {
					ret = scm2010_pin_config_set(dev, config->pin,
							PIN_CONFIG_MK_CMD(PIN_CONFIG_PULL_UP, 0));
				} else if (config->property == GPIO_INPUT_PULL_DOWN) {
					ret = scm2010_pin_config_set(dev, config->pin,
							PIN_CONFIG_MK_CMD(PIN_CONFIG_PULL_DOWN, 0));
				}
			}
		}

		break;

	case IOCTL_GPIO_READ:
		read_value = arg;
		ret = scm2010_gpio_get_value(dev, (u32)read_value->pin);
		if (!(ret < 0)) {
			read_value->value = ret;
			ret = 0;
		}
		break;

	case IOCTL_GPIO_WRITE:
		write_value = arg;
		ret = scm2010_gpio_set_value(dev, (u32)write_value->pin, write_value->value);
		break;

	case IOCTL_GPIO_INTERRUPT_ENABLE:
		intr_en = arg;
		ret = scm2010_gpio_irq_enable(dev, intr_en->pin, (int)intr_en->type, intr_en->intr_cb, intr_en->ctx);
		break;

	case IOCTL_GPIO_INTERRUPT_DISABLE:
		intr_dis = arg;
		ret = scm2010_gpio_irq_disable(dev, intr_dis->pin);
		break;

	default:
		ret = -1;
		break;


	}

	return ret;
}

static int scm2010_gpio_irq(int irq, void *data)
{
	struct gpio_driver_data *priv = data;
	struct device *dev = priv->dev;
	u32 v, v2, *addr;
	int i;

	addr = gpio_addr(dev, OFT_GPIO_EDGE_INTR);
	v = readl(addr);

	/* clear pending interrupts */
	addr = gpio_addr(dev, OFT_GPIO_EDGE_CLR);
	v2 = readl(addr);
	v2 |= v;
	writel(v2, addr);

	/* re-enable them 'cause it will be disabled once cleared. */
	v2 &= ~v;
	writel(v2, addr);

	i = 0;
	while (v) {
		if (v & 0x1) {
			/* execute application callback function */
			if (gpio_intr[i].intr_cb) {
				gpio_intr[i].intr_cb(i, gpio_intr[i].ctx);
			}
		}
		v >>= 1;
		i++;
	};

#ifdef CONFIG_SCM2010_PINCTRL_INT_WORKAROUND
	osTimerStop(priv->int_wa_timer);
	osTimerStart(priv->int_wa_timer, priv->int_wa_period);
	priv->int_wa_last_tick = osKernelGetTickCount();
#if 0
	printk("[%s, %d] WA started.\n", __func__, __LINE__);
#endif
#endif

	return 0;
}

#ifdef CONFIG_SCM2010_PINCTRL_INT_WORKAROUND

static void do_reenable_interrupt(void *argument)
{
	struct gpio_driver_data *priv = argument;
	struct device *dev = priv->dev;
	u32 v, v2, *addr;

	addr = gpio_addr(dev, OFT_GPIO_EDGE_CLR);
	v2 = readl(addr);
	addr = gpio_addr(dev, OFT_GPIO_EDGE_INTR);
	v = readl(addr);
	if (v) {
		__nds__plic_complete_interrupt(dev->irq[0]);
	} else if (v2 != 0xffffffff) {
		int now = osKernelGetTickCount();
		if (abs(now - priv->int_wa_last_tick) < priv->int_wa_duration) {
			osTimerStop(priv->int_wa_timer);
			osTimerStart(priv->int_wa_timer, priv->int_wa_period);
#if 0
			printk("[%s, %d] WA keeps going.\n", __func__, __LINE__);
#endif
		} else {
#if 0
			printk("[%s, %d] Done for WA.\n", __func__, __LINE__);
#endif
		}
	} else {
#if 0
		printk("[%s, %d] Done for WA.\n", __func__, __LINE__);
#endif
	}
}

#endif

int scm2010_pinctrl_probe(struct device *dev)
{
	struct gpio_driver_data *priv;
	struct pmx_func *pfn;
	struct pinmux *pmx;
	int ret, fn, i;
	char buf[32];
	struct file *file;
	s8 func __maybe_unused;

	ret = pinctrl_register_controller(dev);

	for (fn = 0; fn < ARRAY_SIZE(pmx_functions); fn++) {
		pfn = pmx_functions + fn;
		dbg("%s:\n", pfn->devname);
		for (i = 0; i < pfn->nr_pmx; i++) {
			pmx = pfn->pmx + i;
			func = pmx_get(dev, pmx->pin);
			dbg("- [%5s]: pmx func=%d\n", pmx->func, func);
		}
	}

    pmx_drv_data.pull_en = 1;
	pmx_drv_data.int_en = 1;
	pmx_drv_data.debnce_en = 0;
	pmx_drv_data.ch_num = ARRAY_SIZE(pmx_pins);

	dev->driver_data = &pmx_drv_data;

	priv = &gpio_data;
	priv->dev = dev;

#ifdef CONFIG_SCM2010_PINCTRL_INT_WORKAROUND
	gpio_data.int_wa_timer = osTimerNew(do_reenable_interrupt, osTimerOnce, priv, NULL);
	gpio_data.int_wa_period = pdMS_TO_TICKS(CONFIG_SCM2010_PINCTRL_INT_WA_PERIOD);
	gpio_data.int_wa_duration = pdMS_TO_TICKS(CONFIG_SCM2010_PINCTRL_INT_WA_DURATION);
#endif

	sprintf(buf, "/dev/gpio");

	priv->devfs_ops.ioctl = scm2010_gpio_ioctl;

	file = vfs_register_device_file(buf, &priv->devfs_ops, priv);
	if (!file) {
		printk("%s: failed to register as %s\n", dev_name(dev), buf);
		return -1;
	}

	request_irq(dev->irq[0], scm2010_gpio_irq, dev_name(dev), dev->pri[0], priv);

	printk("GPIO: %s registered as %s\n", dev_name(dev), buf);

	return ret;
}

static declare_driver(pinctrl) = {
	.name 	= "scm2010,pinctrl",
	.probe 	= scm2010_pinctrl_probe,
	.ops 	= &scm2010_pinctrl_ops,
};

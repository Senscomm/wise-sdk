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

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define dbg(args, ...)

/* SCM1010 pinmux definitions */

#define OFT_PD_CONFIG 		0x10
#define OFT_PD_DATAIN 		0x20
#define OFT_PD_DATAOUT 		0x24
#define OFT_PD_DIR		0x28
#define OFT_PD_INEN		0x50
#define OFT_PD_IMOD0		0x54
#define OFT_PD_IMOD1		0x58
#define OFT_PD_IMOD2		0x5C
#define OFT_PD_IMOD3		0x60
#define OFT_PD_PULLEN		0x40
#define OFT_PD_PULLTYPE		0x44

#define MUX_NUM_BANK		(2) /* pinmux[1-0] */
#define MUX_SEL_MASK(w) 	((1 << w) - 1)

#define pinctrl_addr(p, oft) (u32 *)((p)->base[1] + oft * 4)
#define gpio_addr(p, oft) (u32 *)((p)->base[0] + oft)

static struct pinctrl_pin_hw_mux_desc {
	u16 bank:1; 	/* pinmux0 or pinmux1 */
	u16 offset:5;	/* bit offset from base addr */
	u16 width:3;	/* # of bits, 0 : not available */
	u16 gpio_fsel:3;  /* fsel for gpio function */
} pinctrl_hw_mxdesc[] = {
	[0] 	= 	{0,  0, 3, 1},
	[1] 	= 	{0,  3, 3, 1},
	[2] 	= 	{0,  6, 3, 1},
	[3] 	= 	{0,  9, 3, 1},
	[4] 	= 	{0, 12, 3, 1},
	[5] 	= 	{0, 15, 3, 1},
	[6] 	= 	{0, 18, 3, 1},
	[7] 	= 	{0, 21, 3, 1},
	[8] 	= 	{0, 24, 3, 1},
	[9] 	= 	{0, 27, 3, 1},
       [10] 	= 	{1,  0, 3, 1},
       [11] 	= 	{1,  3, 3, 1},
       [12] 	= 	{1,  6, 3, 1},
#ifndef CONFIG_GPIO13_TOGGLE
       [13] 	=       {1,  0, 0, 0}, /* separated */
#else
       [13] 	=       {1,  9, 3, 1}, /* separated */
#endif
       [14] 	= 	{1, 12, 3, 1},
       [15] 	= 	{1, 15, 3, 1},
       [16] 	=       {1,  0, 0, 2}, /* separated */
       [17] 	=       {1,  0, 0, 2}, /* separated */
       [18] 	= 	{1, 18, 1, 0},
       [19] 	= 	{1, 19, 1, 0},
       [20] 	= 	{1, 20, 3, 0},
       [21] 	= 	{1, 23, 3, 0},
       [22] 	= 	{1, 26, 3, 0},
       [23] 	= 	{1, 29, 3, 0},
       [24] 	= 	{0, 30, 2, 0},
};

/* XXX HW state to apply during PM */
/* Simple approach taken because fast switching is advantageous */
struct pinctrl_pm_hw_data {
	u32 pmx[MUX_NUM_BANK];	/* pinmux(es) */
	u32 gdir;		/* gpio direction */
	u32 gout;		/* gpio out */
	u32 gpty;		/* gpio pull type */
	u32 gpen;		/* gpio pull enable */
};

static struct pinctrl_driver_data {
	int pull_en; 	/* pull support*/
	int int_en;  	/* interrupt support*/
	int debnce_en;	/* debounce support */
	int ch_num;	/* # of gpio channels */
	struct pinctrl_pm_hw_data pm;
	struct pinctrl_pm_hw_data pm_copy;
} pmx_drv_data;

static struct pinctrl_pin_desc pmx_pins[32];

struct pinmux {
	u32 pin;
	const char *func;
	u32 fsel; /* function: 0 - 7 */
	struct pinctrl_pin_desc *desc;
};

struct pmx_func {
	const char *devname;
	struct pinmux *pmx;
	unsigned nr_pmx;
};

#define MUX(id, nr, value) {	\
	.pin	= nr,		\
	.func 	= id,		\
	.fsel 	= value		\
}

#ifdef CONFIG_PM_IO
struct pinmux jtag_pin_mux[] = {
	MUX("tck",  0, 0),
	MUX("tms",  1, 0),
	MUX("tdo",  2, 0),
	MUX("tdi",  3, 0),
	MUX("trst", 4, 0),
};
#endif

#ifdef CONFIG_GPIO13_TOGGLE
struct pinmux clk_pin_mux[] = {
	MUX("32k_out",  13, 5),
};
#endif

#ifdef CONFIG_I2C
struct pinmux i2c_pin_mux[] = {
	MUX("sda", 10, 4),
	MUX("sda",  7, 1),
	MUX("scl",  9, 4),
	MUX("scl",  0, 7),
};
#endif

#ifdef CONFIG_USE_SPI0
struct pinmux spi0_pin_mux[] = {
	MUX("mosi", 6, 2),
	MUX("mosi", 2, 3),
	MUX("miso", 3, 3),
	MUX("miso",13, 3),
	MUX("clk",  4, 2),
	MUX("clk",  0, 3),
	MUX("cs",   5, 2),
	MUX("cs",   1, 3),
	MUX("wp",   6, 4),
	MUX("hold", 5, 4),
};
#endif

#ifdef CONFIG_USE_SPI1
struct pinmux spi1_pin_mux[] = {
	MUX("mosi", 7, 3),
	MUX("miso", 8, 3),
	MUX("wp",   9, 3),
	MUX("hold",10, 3),
	MUX("clk", 11, 3),
	MUX("cs",  12, 3),
};
#endif

#ifdef CONFIG_USE_UART0
struct pinmux uart0_pin_mux[] = {
	MUX("cts",  7, 2),
	MUX("rts",  8, 2),
	MUX("rxd",  5, 0),
	MUX("rxd", 20, 1),
	MUX("txd",  6, 0),
	MUX("txd", 21, 1),
};
#endif

#ifdef CONFIG_USE_UART1
struct pinmux uart1_pin_mux[] = {
	MUX("cts", 12, 2),
	MUX("cts",  5, 3),
	MUX("rts", 11, 2),
	MUX("rxd",  2, 2),
	MUX("rxd",  9, 5),
	MUX("txd",  3, 2),
	MUX("txd", 10, 5),
};
#endif

#ifdef CONFIG_USE_UART2
struct pinmux uart2_pin_mux[] = {
	MUX("rxd", 20, 5),
	MUX("rxd", 22, 5),
	MUX("rxd", 14, 7),
	MUX("rxd", 15, 7),
	MUX("txd", 21, 5),
	MUX("txd", 23, 5),
	MUX("txd",  2, 7),
	MUX("txd",  3, 7),
};
#endif

/* FIXME: PWM, SDIO, I2S */

#define pf(name, m) { 				\
	.devname = name, 			\
	.pmx = m##_pin_mux, 			\
	.nr_pmx = ARRAY_SIZE(m##_pin_mux), 	\
}

struct pmx_func pmx_functions[] = {
#ifdef CONFIG_PM_IO
	pf("jtag", jtag),
#endif
#ifdef CONFIG_GPIO13_TOGGLE
	pf("clk", clk),
#endif
#ifdef CONFIG_I2C
	pf("i2c", i2c),
#endif
#ifdef CONFIG_USE_SPI0
	pf("atcspi.0", spi0),
#endif
#ifdef CONFIG_USE_SPI1
	pf("atcspi200-xip", spi1),
#endif
#ifdef CONFIG_USE_UART0
	pf("atcuart.0", uart0),
#endif
#ifdef CONFIG_USE_UART1
	pf("atcuart.1", uart1),
#endif
#ifdef CONFIG_USE_UART2
	pf("atcuart.2", uart2),
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

static int scm1010_gpio_set_direction(struct device *pctl, u32 pin, bool input)
{
	u32 *addr, v;

	addr = gpio_addr(pctl, OFT_PD_DIR);
	v = readl(addr) & ~(1 << pin);
	if (!input)
		v |= (1 << pin);
	writel(v, addr);

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

static int scm1010_pmx_request(struct device *pctl, u32 pin)
{
	struct pinctrl_driver_data *prv = pctl->driver_data;
	assert(pin < prv->ch_num);
	/*
	 * There is nothing to do here because
	 * actual pinmux setting will be done
	 * by scm1010_pmx_set_mux()
	 */
	return 0;
}

static int scm1010_pmx_free(struct device *pctl, u32 pin)
{
	struct pinmux *pmx;
	struct pinctrl_driver_data *prv = pctl->driver_data;

	assert(pin < prv->ch_num);

	pmx = pmx_pins[pin].driver_data; /* set_mux() */
	if (pmx) {
		pmx_pins[pin].driver_data = NULL;
		/*
		 * FIXME: make sure it turn to be GPIO input
		 * with pull-up or pull-down to prevent
		 * leakage
		 */
	}

	return 0;
}

static int scm1010_gpio_request_enable(struct device *pctl, u32 pin)
{
	struct pinctrl_driver_data *prv = pctl->driver_data;

	assert(pin < prv->ch_num);

	/* Safe to set the direction input before enabling GPIO function */
	scm1010_gpio_set_direction(pctl, pin, true);

	pmx_set(pctl, pin, pinctrl_hw_mxdesc[pin].gpio_fsel);

	return 0;
}

static int scm1010_gpio_disable_free(struct device *pctl, u32 pin)
{
	struct pinctrl_driver_data *prv = pctl->driver_data;

	assert(pin < prv->ch_num);

	scm1010_gpio_set_direction(pctl, pin, false);

	return 0;
}

static int scm1010_pmx_set_mux(struct device *pctl, struct device *client,
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

static struct pinmux_ops scm1010_pmx_ops = {
	.request 	= scm1010_pmx_request,
	.free 		= scm1010_pmx_free,
	.set_mux	= scm1010_pmx_set_mux,
	.gpio_request_enable = scm1010_gpio_request_enable,
	.gpio_disable_free = scm1010_gpio_disable_free,
	.gpio_set_direction = scm1010_gpio_set_direction,
};

static int scm1010_pin_config_set(struct device *pctl, unsigned pin, unsigned config)
{
	u32 cmd, arg, v;
    	u32 mask, sel;
	volatile u32 *addr;
	struct pinctrl_driver_data *prv = pctl->driver_data;

	assert(pin < prv->ch_num);

	cmd = config >> 16;
	arg = config & PIN_CONFIG_PARAM_MASK;

	mask = 1 << pin;
	sel = arg & 0x1;

	if (cmd == PIN_CONFIG_INPUT_ENABLE) {
		assert(prv->int_en);
		addr = gpio_addr(pctl, OFT_PD_INEN);
		v = readl(addr) & ~mask;
		v |= sel;
		writel(v, addr);
	} else {
		if (sel)
			assert(prv->pull_en);
		if (cmd == PIN_CONFIG_PULL_UP) {
			addr = gpio_addr(pctl, OFT_PD_PULLTYPE);
			v = readl(addr) & ~mask;
			writel(v, addr);
		} else if (cmd == PIN_CONFIG_PULL_DOWN) {
			addr = gpio_addr(pctl, OFT_PD_PULLTYPE);
			v = readl(addr) | mask;
			writel(v, addr);
		}
		addr = gpio_addr(pctl, OFT_PD_PULLEN);
		v = readl(addr) & ~mask;
		v |= sel;
		writel(v, addr);
	}

	return 0;
}

static struct pinconf_ops scm1010_pinconf_ops = {
	.pin_config_set = scm1010_pin_config_set,
};

static int scm1010_gpio_get_value(struct device *pctl, u32 pin)
{
	u32 *addr, v;
	unsigned long flags;
	struct pinctrl_driver_data *prv = pctl->driver_data;

	assert(pin < prv->ch_num);

	addr = gpio_addr(pctl, OFT_PD_DATAIN);

	local_irq_save(flags);
	v = readl(addr) >> pin;
	local_irq_restore(flags);

	return v & 1;
}

static int scm1010_gpio_set_value(struct device *pctl, u32 pin, int value)
{
	u32 *addr, v;
	unsigned long flags;
	struct pinctrl_driver_data *prv = pctl->driver_data;

	assert(pin < prv->ch_num);

	addr = gpio_addr(pctl, OFT_PD_DATAOUT);
	value = (value & 1) << pin;

	local_irq_save(flags);
	v = readl(addr) & ~(1 << pin);
	v |= value;
	writel(v, addr);
	local_irq_restore(flags);

	return 0;
}

static int scm1010_gpio_to_irq(struct device *pctl, u32 pin)
{
	u32 *addr, v;
	unsigned long flags;
	struct pinctrl_driver_data *prv = pctl->driver_data;

	assert(pin < prv->ch_num);

	addr = gpio_addr(pctl, OFT_PD_INEN);

	local_irq_save(flags);
	v = readl(addr) & ~(1 << pin);
	v |= 1 << pin;
	writel(v, addr);
	local_irq_restore(flags);

	return 0;
}

static struct gpio_ops scm1010_gpio_ops = {
	.get 	= scm1010_gpio_get_value,
	.set 	= scm1010_gpio_set_value,
	.to_irq = scm1010_gpio_to_irq,
};


__iram__ int scm1010_pm_powerdown(struct device *pctl)
{
#ifdef CONFIG_PM_IO
	struct pinctrl_driver_data *prv = pctl->driver_data;
	int i;

	for (i = 0; i < MUX_NUM_BANK; i++)
		prv->pm_copy.pmx[i] = readl(pinctrl_addr(pctl, i));
	prv->pm_copy.gdir = readl(gpio_addr(pctl, OFT_PD_DIR));
	prv->pm_copy.gout = readl(gpio_addr(pctl, OFT_PD_DATAOUT));
	prv->pm_copy.gpty = readl(gpio_addr(pctl, OFT_PD_PULLTYPE));
	prv->pm_copy.gpen = readl(gpio_addr(pctl, OFT_PD_PULLEN));

	for (i = 0; i < MUX_NUM_BANK; i++)
		writel(prv->pm.pmx[i], pinctrl_addr(pctl, i));

	writel(1, GPIO_MOD);

	writel(prv->pm.gdir, gpio_addr(pctl, OFT_PD_DIR));
	writel(prv->pm.gout, gpio_addr(pctl, OFT_PD_DATAOUT));
	writel(prv->pm.gpty, gpio_addr(pctl, OFT_PD_PULLTYPE));
	writel(prv->pm.gpen, gpio_addr(pctl, OFT_PD_PULLEN));
#endif
	return 0;
}

__iram__ int scm1010_pm_powerup(struct device *pctl)
{
#ifdef CONFIG_PM_IO
	struct pinctrl_driver_data *prv = pctl->driver_data;
	int i;

	writel(0x0, GPIO_MOD);

	for (i = 0; i < MUX_NUM_BANK; i++)
		writel(prv->pm_copy.pmx[i], pinctrl_addr(pctl, i));
	writel(prv->pm_copy.gdir, gpio_addr(pctl, OFT_PD_DIR));
	writel(prv->pm_copy.gout, gpio_addr(pctl, OFT_PD_DATAOUT));
	writel(prv->pm_copy.gpty, gpio_addr(pctl, OFT_PD_PULLTYPE));
	writel(prv->pm_copy.gpen, gpio_addr(pctl, OFT_PD_PULLEN));
#endif
	return 0;
}

static struct pin_pm_ops scm1010_pm_ops = {
	.powerdown 	= scm1010_pm_powerdown,
	.powerup 	= scm1010_pm_powerup,
};

static struct pinctrl_ops scm1010_pinctrl_ops = {
	.pins 		= pmx_pins,
	.npins 		= ARRAY_SIZE(pmx_pins),
	.pmxops 	= &scm1010_pmx_ops,
	.confops 	= &scm1010_pinconf_ops,
	.gpops		= &scm1010_gpio_ops,
	.pmops		= &scm1010_pm_ops,
};

#ifdef CONFIG_PM_IO
static void pinctrl_config_pm(struct pinctrl_pm_hw_data *pm)
{
	int i, pin;
	struct pinctrl_pin_hw_mux_desc *hwdesc;
	struct pinctrl_platform_pinmap *p_pinmap;

	/* 1. Default PM hw settings : GPIO input with pull-up */
	for (pin = 0; pin < ARRAY_SIZE(pinctrl_hw_mxdesc); pin++) {
		hwdesc = &pinctrl_hw_mxdesc[pin];
		pmx_eval(pin, hwdesc->gpio_fsel, &pm->pmx[pmx_bank(pin)]);
	}
	pm->gdir = 0x0;
	pm->gout = 0x0;
	pm->gpty = 0x0;
	pm->gpen = 0x01FCFFFF;

	/* 2. Board-specific PM hw settings */
	p_pinmap = pinctrl_get_platform_pinmap();
	if (p_pinmap) {
		struct pinctrl_pin_map *pinmap;
		struct pm_conf *pd;
		struct pinmux *pmx;

		for (i = 0; i < p_pinmap->nr_map; i++) {
			pinmap = &p_pinmap->map[i];
			pin = pinmap->pin;
			pd = pinmap->pmconf;
			if (pd) {
				/* switch to GPIO function */
				pm->gdir &= ~(0x1 << pin);
				pm->gdir |= (pd->pm_out << pin);
				if (pd->pm_out) {
					pm->gout &= ~(0x1 << pin);
					pm->gout |= (pd->pm_val << pin);
				} else {
					pm->gpty &= ~(0x1 << pin);
					pm->gpty |= (pd->pm_pdn << pin);
					pm->gpen &= ~(0x1 << pin);
					pm->gpen |= (pd->pm_pen << pin);
				}
			} else {
				/* remain as the current alternate function */
				pmx = pinctrl_get_pinmux(pinmap->func, pinmap->id, pin);
				pmx_eval(pin, pmx->fsel, &pm->pmx[pmx_bank(pin)]);
			}
		}
	}
#if 0
	printk("[%s, %d] pmx:0x%x, 0x%x, gdir:0x%x, gout:0x%x, gpty:0x%x, gpen:0x%x\n",
			__func__, __LINE__,
			pm->pmx[0], pm->pmx[1], pm->gdir, pm->gout, pm->gpty, pm->gpen);
#endif
}
#endif

static int scm1010_pinctrl_probe(struct device *dev)
{
	struct pmx_func *pfn;
	struct pinmux *pmx;
	int ret, fn, i;
	u32 *addr, v;
	s8 func __maybe_unused;
	struct clk *clk;

	ret = pinctrl_register_controller(dev);

	clk = clk_get(NULL, "gpio/pclk");
	clk_enable(clk, 1);

	for (fn = 0; fn < ARRAY_SIZE(pmx_functions); fn++) {
		pfn = pmx_functions + fn;
		dbg("%s:\n", pfn->devname);
		for (i = 0; i < pfn->nr_pmx; i++) {
			pmx = pfn->pmx + i;
			func = pmx_get(dev, pmx->pin);
			dbg("- [%5s]: pmx func=%d\n", pmx->func, func);
		}
	}
	addr = gpio_addr(dev, OFT_PD_CONFIG);
	v = readl(addr);
	pmx_drv_data.pull_en = (v >> 31) & 0x1;
	pmx_drv_data.int_en = (v >> 30) & 0x1;
	pmx_drv_data.debnce_en = (v >> 29) & 0x1;
	pmx_drv_data.ch_num = v & 0x3F;

#ifdef CONFIG_PM_IO
	pinctrl_config_pm(&pmx_drv_data.pm);
#endif

	dev->driver_data = &pmx_drv_data;

	return ret;
}

static declare_driver(pinctrl) = {
	.name 	= "scm1010,pinctrl",
	.probe 	= scm1010_pinctrl_probe,
	.ops 	= &scm1010_pinctrl_ops,
};

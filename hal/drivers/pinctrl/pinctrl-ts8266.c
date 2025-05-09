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
#include <hal/device.h>
#include <hal/pinctrl.h>
#include <hal/clk.h>

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define dbg(args, ...)

/* TS8266 pinmux definitions */

#define OFT_PIN_MUX0		0x24
#define OFT_PIN_MUX1		0x28

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

#define MUX_SEL_WIDTH		(3)
#define MUX_SEL_MASK 		((1 << MUX_SEL_WIDTH) - 1)

#define pinctrl_addr(p, oft) (u32 *)((p)->base[1] + oft)
#define gpio_addr(p, oft) (u32 *)((p)->base[0] + oft)

static struct pinctrl_driver_data {
	int pull_en; 	/* pull support*/
	int int_en;  	/* interrupt support*/
	int debnce_en;	/* debounce support */
	int ch_num;	/* # of gpio channels */
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

struct pinmux i2c_pin_mux[] = {
	MUX("sda", 10, 4),
	MUX("sda",  3, 6),
	MUX("sda", 13, 7),
	MUX("scl",  9, 4),
	MUX("scl",  2, 6),
	MUX("scl", 13, 0),
};

struct pinmux spi0_pin_mux[] = {
	MUX("mosi", 2, 3),
	MUX("mosi", 6, 5),
	MUX("miso", 3, 3),
	MUX("miso",13, 3),
	MUX("clk",  4, 2),
	MUX("clk",  0, 3),
	MUX("cs",   1, 3),
	MUX("cs",   5, 5),
	MUX("wp",   6, 4),
	MUX("hold", 5, 4),
};

struct pinmux spi1_pin_mux[] = {
	MUX("mosi",   7, 3),
	MUX("miso",  8, 3),
	MUX("hold", 9, 3),
	MUX("wp",  10, 3),
	MUX("clk", 11, 3),
	MUX("cs",  12, 3),
};

struct pinmux uart0_pin_mux[] = {
	MUX("cts",  7, 2),
	MUX("cts",  1, 7),
	MUX("rts",  8, 2),
	MUX("rts",  2, 7),
	MUX("rxd",  5, 2),
	MUX("rxd",  0, 5),
	MUX("txd",  6, 2),
	MUX("txd", 13, 5),
};

struct pinmux uart1_pin_mux[] = {
	MUX("cts", 12, 2),
	MUX("cts",  5, 3),
	MUX("rts", 11, 2),
	MUX("rts",  0, 6),
	MUX("rxd",  2, 2),
	MUX("rxd",  4, 6),
	MUX("txd",  3, 2),
	MUX("txd",  1, 6),
};

/* FIXME: PWM, SDIO */

#define pf(name, m) { 				\
	.devname = name, 			\
	.pmx = m##_pin_mux, 			\
	.nr_pmx = ARRAY_SIZE(m##_pin_mux), 	\
}

struct pmx_func pmx_functions[] = {
	pf("i2c", i2c),
	pf("atcspi.0", spi0),
	pf("atcspi200-xip", spi1),
	pf("atcuart.0", uart0),
	pf("atcuart.1", uart1),
};

static struct pinmux *pinctrl_get_pinmux(struct device *device,
		const char *id, u32 pin)
{
	struct pmx_func *pfn;
	struct pinmux *pmx;
	int i;

	for (i = 0; i < ARRAY_SIZE(pmx_functions); i++) {
		pfn = &pmx_functions[i];
		if (!strcmp(pfn->devname, dev_name(device)))
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

static int ts8266_gpio_set_direction(struct device *pctl, u32 pin, bool input)
{
	u32 *addr, v;

	addr = gpio_addr(pctl, OFT_PD_DIR);
	v = readl(addr) & ~(1 << pin);
	if (!input)
		v |= (1 << pin);
	writel(v, addr);

	return 0;
}

static u8 pmx_get(struct device *pctl, u32 pin)
{
	u32 *addr, v;
	int index;
	u32 mask, shift;

	index = (pin < 10) ? 0 : 4;
	addr = pinctrl_addr(pctl, index);

	shift = (MUX_SEL_WIDTH * (pin % 10));
	mask = MUX_SEL_MASK << shift;
	v = readl(addr) & mask;

	return v >> shift;
}

static void pmx_set(struct device *pctl, u32 pin, u8 func)
{
	u32 *addr, v;
	int index;
	u32 mask, shift;

	index = (pin < 10) ? 0 : 4;
	addr = pinctrl_addr(pctl, index);

	shift = (MUX_SEL_WIDTH * (pin % 10));
	mask = MUX_SEL_MASK << shift;
	v = readl(addr) & ~mask;
	v |= func << shift;

	writel(v, addr);
}

static void pmx_write(struct device *pctl, struct pinmux *pmx)
{
	unsigned long flags;

	local_irq_save(flags);

	pmx_set(pctl, pmx->pin, pmx->fsel);

	local_irq_restore(flags);
}

static int ts8266_pmx_request(struct device *pctl, u32 pin)
{
	struct pinctrl_driver_data *cfg = pctl->driver_data;
	assert(pin < cfg->ch_num);
	/*
	 * There is nothing to do here because
	 * actual pinmux setting will be done
	 * by ts8266_pmx_set_mux()
	 */
	return 0;
}

static int ts8266_pmx_free(struct device *pctl, u32 pin)
{
	struct pinmux *pmx;
	struct pinctrl_driver_data *cfg = pctl->driver_data;

	assert(pin < cfg->ch_num);

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

static int ts8266_gpio_request_enable(struct device *pctl, u32 pin)
{
	struct pinctrl_driver_data *cfg = pctl->driver_data;

	assert(pin < cfg->ch_num);

	/* Safe to set the direction input before enabling GPIO function */
	ts8266_gpio_set_direction(pctl, pin, true);

	pmx_set(pctl, pin, 1); /* fun1 : gpio */

	return 0;
}

static int ts8266_gpio_disable_free(struct device *pctl, u32 pin)
{
	struct pinctrl_driver_data *cfg = pctl->driver_data;

	assert(pin < cfg->ch_num);

	ts8266_gpio_set_direction(pctl, pin, false);

	return 0;
}

static int ts8266_pmx_set_mux(struct device *pctl, struct device *client,
		const char *id, u32 pin)
{
	struct pinmux *pmx = pinctrl_get_pinmux(client, id, pin);
	struct pinctrl_driver_data *cfg = pctl->driver_data;

	assert(pin < cfg->ch_num);

	if (!pctl || !pmx)
		return -ENODEV;

	pmx_write(pctl, pmx);

	pmx_pins[pin].driver_data = pmx; /* keep mux owner */
	pmx->desc = &pmx_pins[pin];

	return 0;
}

static struct pinmux_ops ts8266_pmx_ops = {
	.request 	= ts8266_pmx_request,
	.free 		= ts8266_pmx_free,
	.set_mux	= ts8266_pmx_set_mux,
	.gpio_request_enable = ts8266_gpio_request_enable,
	.gpio_disable_free = ts8266_gpio_disable_free,
	.gpio_set_direction = ts8266_gpio_set_direction,
};

static int ts8266_pin_config_set(struct device *pctl, unsigned pin, unsigned config)
{
	u32 cmd, arg, v;
    	u32 mask, sel;
	volatile u32 *addr;
	struct pinctrl_driver_data *cfg = pctl->driver_data;

	assert(pin < cfg->ch_num);

	cmd = config >> 16;
	arg = config & PIN_CONFIG_PARAM_MASK;

	mask = 1 << pin;
	sel = arg & 0x1;

	if (cmd == PIN_CONFIG_INPUT_ENABLE) {
		assert(cfg->int_en);
		addr = gpio_addr(pctl, OFT_PD_INEN);
		v = readl(addr) & ~mask;
		v |= sel;
		writel(v, addr);
	} else {
		if (sel)
			assert(cfg->pull_en);
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

static struct pinconf_ops ts8266_pinconf_ops = {
	.pin_config_set = ts8266_pin_config_set,
};

static int ts8266_gpio_get_value(struct device *pctl, u32 pin)
{
	u32 *addr, v;
	unsigned long flags;
	struct pinctrl_driver_data *cfg = pctl->driver_data;

	assert(pin < cfg->ch_num);

	addr = gpio_addr(pctl, OFT_PD_DATAIN);

	local_irq_save(flags);
	v = readl(addr) >> pin;
	local_irq_restore(flags);

	return v & 1;
}

static int ts8266_gpio_set_value(struct device *pctl, u32 pin, int value)
{
	u32 *addr, v;
	unsigned long flags;
	struct pinctrl_driver_data *cfg = pctl->driver_data;

	assert(pin < cfg->ch_num);

	addr = gpio_addr(pctl, OFT_PD_DATAOUT);
	value = (value & 1) << pin;

	local_irq_save(flags);
	v = readl(addr) & ~(1 << pin);
	v |= value;
	writel(v, addr);
	local_irq_restore(flags);

	return 0;
}

static int ts8266_gpio_to_irq(struct device *pctl, u32 pin)
{
	u32 *addr, v;
	unsigned long flags;
	struct pinctrl_driver_data *cfg = pctl->driver_data;

	assert(pin < cfg->ch_num);

	addr = gpio_addr(pctl, OFT_PD_INEN);

	local_irq_save(flags);
	v = readl(addr) & ~(1 << pin);
	v |= 1 << pin;
	writel(v, addr);
	local_irq_restore(flags);

	return 0;
}

static struct gpio_ops ts8266_gpio_ops = {
	.get 	= ts8266_gpio_get_value,
	.set 	= ts8266_gpio_set_value,
	.to_irq = ts8266_gpio_to_irq,
};

static struct pinctrl_ops ts8266_pinctrl_ops = {
	.pins 		= pmx_pins,
	.npins 		= ARRAY_SIZE(pmx_pins),
	.pmxops 	= &ts8266_pmx_ops,
	.confops 	= &ts8266_pinconf_ops,
	.gpops		= &ts8266_gpio_ops,
};

static int ts8266_pinctrl_probe(struct device *dev)
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
	dev->driver_data = &pmx_drv_data;

	return ret;
}

static declare_driver(pinctrl) = {
	.name 	= "ts8266,pinctrl",
	.probe 	= ts8266_pinctrl_probe,
	.ops 	= &ts8266_pinctrl_ops,
};

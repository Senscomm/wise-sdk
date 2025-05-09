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

/* BCM2836 GPIO, pinmux definitions */

#define GPFSEL0			0x0
#define GPFSEL1			0x4
#define GPFSEL2			0x8
#define GPFSEL3			0xC
#define GPFSEL4			0x10
#define GPFSEL5			0x14
#define GPSET0			0x1C
#define GPSET1			0x20
#define GPCLR0			0x28
#define GPCLR1			0x2C
#define GPLEV0			0x34
#define GPLEV1			0x38
#define GPEDS0			0x40
#define GPEDS1			0x44
#define GPREN0			0x4C
#define GPREN1			0x50
#define GPFEN0			0x58
#define GPFEN1			0x5C
#define GPHEN0			0x64
#define GPHEN1			0x68
#define GPLEN0			0x70
#define GPLEN1			0x74
#define GPAREN0			0x7C
#define GPAREN1			0x80
#define GPAFEN0			0x88
#define GPAFEN1			0x8C
#define GPPUD			0x94
#define GPPUDCLK0		0x98
#define GPPUDCLK1		0x9C

/* FSEL */
#define FSEL_GPIO_IN		0x0
#define FSEL_GPIO_OUT		0x1
#define FSEL_ALT_0		0x4
#define FSEL_ALT_1		0x5
#define FSEL_ALT_2		0x6
#define FSEL_ALT_3		0x7
#define FSEL_ALT_4		0x3
#define FSEL_ALT_5		0x2

#define pinctrl_addr(p, oft) (u32 *)((p)->base[0] + oft)

static struct pinctrl_pin_desc pmx_pins[54];

static struct pinctrl_driver_data {
	int ch_num;	/* # of gpio channels */
} pmx_drv_data;

struct pinmux {
	u32 pin;
	const char *func;
	u32 fsel; /* function: FSEL_GPIO_IN(OUT), FSEL_ALT_0(~5) */
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

struct pinmux uart0_pin_mux[] = {
	MUX("txd", 14, FSEL_ALT_0),
	MUX("rxd", 15, FSEL_ALT_0),
};

#define pf(name, m) { 				\
	.devname = name, 			\
	.pmx = m##_pin_mux, 			\
	.nr_pmx = ARRAY_SIZE(m##_pin_mux), 	\
}

struct pmx_func pmx_functions[] = {
	pf("pl011", uart0),
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

static int bcm2836_gpio_set_direction(struct device *pctl, u32 pin, bool input)
{
	/* FIXME : TO DO */

	return 0;
}

static void pmx_set(struct device *pctl, u32 pin, u8 func)
{
	int reg;
	int item;
	u32 *addr;
	u32 v;

	reg = pin / 10;
	item = pin % 10;
	addr = pinctrl_addr(pctl, GPFSEL0 + 4 * reg);
	v = readl(addr);
	v &= ~(0x7 << (item * 3));
	v |= ((func & 0x7) << (item * 3));

	writel(v, addr);
}

static void pmx_write(struct device *pctl, struct pinmux *pmx)
{
	unsigned long flags;

	local_irq_save(flags);

	pmx_set(pctl, pmx->pin, pmx->fsel);

	local_irq_restore(flags);
}

static int bcm2836_pmx_request(struct device *pctl, u32 pin)
{
	struct pinctrl_driver_data *prv = pctl->driver_data;
	assert(pin < prv->ch_num);
	/*
	 * There is nothing to do here because
	 * actual pinmux setting will be done
	 * by bcm2836_pmx_set_mux()
	 */
	return 0;
}

static int bcm2836_pmx_free(struct device *pctl, u32 pin)
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

static int bcm2836_gpio_request_enable(struct device *pctl, u32 pin)
{
	struct pinctrl_driver_data *prv = pctl->driver_data;

	assert(pin < prv->ch_num);

	/* FIXME : TO DO */

	return 0;
}

static int bcm2836_gpio_disable_free(struct device *pctl, u32 pin)
{
	struct pinctrl_driver_data *prv = pctl->driver_data;

	assert(pin < prv->ch_num);

	bcm2836_gpio_set_direction(pctl, pin, false);

	return 0;
}

static int bcm2836_pmx_set_mux(struct device *pctl, struct device *client,
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

static struct pinmux_ops bcm2836_pmx_ops = {
	.request 	= bcm2836_pmx_request,
	.free 		= bcm2836_pmx_free,
	.set_mux	= bcm2836_pmx_set_mux,
	.gpio_request_enable = bcm2836_gpio_request_enable,
	.gpio_disable_free = bcm2836_gpio_disable_free,
	.gpio_set_direction = bcm2836_gpio_set_direction,
};

static int bcm2836_pin_config_set(struct device *pctl, unsigned pin, unsigned config)
{
	/* FIXME : TO DO */

	return 0;
}

static struct pinconf_ops bcm2836_pinconf_ops = {
	.pin_config_set = bcm2836_pin_config_set,
};

static int bcm2836_gpio_get_value(struct device *pctl, u32 pin)
{
	/* FIXME : TO DO */

	return 0;
}

static int bcm2836_gpio_set_value(struct device *pctl, u32 pin, int value)
{
	/* FIXME: TO DO */

	return 0;
}

static int bcm2836_gpio_to_irq(struct device *pctl, u32 pin)
{
	/* FIXME : TO DO */

	return 0;
}

static struct gpio_ops bcm2836_gpio_ops = {
	.get 	= bcm2836_gpio_get_value,
	.set 	= bcm2836_gpio_set_value,
	.to_irq = bcm2836_gpio_to_irq,
};


int bcm2836_pm_powerdown(struct device *pctl)
{
	/* FIXME : TO DO */

	return 0;
}

int bcm2836_pm_powerup(struct device *pctl)
{
	/* FIXME : TO DO */

	return 0;
}

static struct pin_pm_ops bcm2836_pm_ops = {
	.powerdown 	= bcm2836_pm_powerdown,
	.powerup 	= bcm2836_pm_powerup,
};

static struct pinctrl_ops bcm2836_pinctrl_ops = {
	.pins 		= pmx_pins,
	.npins 		= ARRAY_SIZE(pmx_pins),
	.pmxops 	= &bcm2836_pmx_ops,
	.confops 	= &bcm2836_pinconf_ops,
	.gpops		= &bcm2836_gpio_ops,
	.pmops		= &bcm2836_pm_ops,
};

static int bcm2836_pinctrl_probe(struct device *dev)
{
	pmx_drv_data.ch_num = 54;
	dev->driver_data = &pmx_drv_data;

	return pinctrl_register_controller(dev);
}

static declare_driver(pinctrl) = {
	.name 	= "bcm2836,pinctrl",
	.probe 	= bcm2836_pinctrl_probe,
	.ops 	= &bcm2836_pinctrl_ops,
};

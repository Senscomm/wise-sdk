/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
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
#include <hal/clk.h>
#include <hal/console.h>

#if 0
#define dbg(f...) printk(f)
#else
#define dbg(f...)
#endif
#define err(f...) printk(f)

#include <string.h>
#include <stdio.h>


/*
 * device clocks
 */

static int dev_clk_enable(struct clk *clk, int enable)
{
	if (!clk->hw.base)
		return 0;

	u32 v = readl(clk->hw.base);

	v &= ~(1 << clk->hw.shift);
	if (enable) {
		v |= (1 << clk->hw.shift);
	}

	writel(v, clk->hw.base);

	return 0;
}

static struct clk_ops dev_ops = {
	.enable = &dev_clk_enable,
};

#define dev_clk(n, p, b, s) {	\
	.name 		= n,			\
	.parent 	= clksym(p),	\
	.hw.base 	= (void *)b,	\
	.hw.shift 	= s,			\
	.ops		= &dev_ops,		\
}


#ifdef CONFIG_USE_DEFAULT_CLK

/*
 * UART (ATCUART)
 */
static CLK_ARRAY(uart) = {
	{
		.name   = "atcuart.0/pclk",
#ifdef CONFIG_SYS_FPGA
		.rate   = 20000000,
#else
		.rate   = 40000000,
#endif
	},
	{
		.name 	= "atcuart.1/pclk",
#ifdef CONFIG_SYS_FPGA
		.rate   = 20000000,
#else
		.rate   = 40000000,
#endif
	},
	{
		.name 	= "atcuart.2/pclk",
#ifdef CONFIG_SYS_FPGA
		.rate   = 20000000,
#else
		.rate   = 40000000,
#endif
	}
};

/*
 * SPI (ATCSPI)
 */
static CLK_ARRAY(spi) = {
	{
#ifdef CONFIG_SPI_FLASH
		.name   = "atcspi200-xip.0/pclk",
#else
		.name   = "atcspi.0/pclk",
#endif
		.rate   = 40000000,
	},
	{
		.name 	= "atcspi.1/pclk",
		.rate   = 40000000,
	},
    {
		.name 	= "atcspi.2/pclk",
		.rate   = 40000000,
	}
};

/*
 * Timer (ATCPIT)
 */

static CLK_ARRAY(timer) = {
	{
		.name 	    = "timer.0/pclk",
        .hw.base    = (void *)SYS(PIT0_CFG),
        .hw.shift   = 8,
        .hw.mask    = 0x3,
		.rate       = 40000000,
	},
	{
		.name 	    = "timer.1/pclk",
        .hw.base    = (void *)SYS(PIT1_CFG),
        .hw.shift   = 8,
        .hw.mask    = 0x3,
		.rate       = 40000000,
	}
};

/*
 * Watchdog (ATCWDT)
 */
static CLK_ARRAY(wdog) = {
    {
		.name 	= "atcwdt/pclk",
		.rate   = 40000000,
	}
};

/*
 * SDIO (SmartDV)
 */
static CLK_ARRAY(sdio) = {
	{
		.name 	    = "sdio/pclk",
		.rate       = 40000000,
		.hw.base    = SYS(SDIO_CTRL),
		.hw.shift   = 1,
		.ops        = &dev_ops,
	}
};

/*
 * PTA
 */
static CLK_ARRAY(pta) = {
    {
		.name 	    = "pta/hclk",
		.rate       = 40000000,
		.hw.base    = SYS(DEF_CLK_CFG),
		.hw.shift   = 0,
		.ops        = &dev_ops,
	}
};

/*
 * I2S
 */
static CLK_ARRAY(i2s) = {
    {
		.name 	= "i2s/mclk",
		.rate   = 24000000,
	}
};

#else /* CONFIG_USE_DEFAULT_CLK */

/* Multiplexer */

static int mask(int n)
{
	int i = 2;
	while (n > i) i *= 2;
	return (i - 1);
}

#define getbf(v, m, s)	((v & (m << s)) >> s)
#define setbf(v, m, s, n)  do {		\
	v = (v & ~(m << s)) | (n << s);	\
} while (0)

static int get_mux(struct clk *clk)
{
	u32 v, m, s;

	if (!clk->hw.base)
		return 0;

	v = readl(clk->hw.base);
	m = mask(clk->nr_input);
	s = clk->hw.shift;

	return getbf(v, m, s);
}

static void set_mux(struct clk *clk, int sel)
{
	u32 v, m, s;

	if (!clk->hw.base)
		return;

	v = readl(clk->hw.base);
	m = mask(clk->nr_input);
	s = clk->hw.shift;

	setbf(v, m, s, sel);
	writel(v, clk->hw.base);
}

static int mux_enable(struct clk *clk, int enable)
{
	u32 v = get_mux(clk);

	if (!clk->parent)
		clk->parent = clk->cmx[v];

	dbg("%s: CLK_CTRL=0x%08x, parent=%s\n",
		__func__, v, clk->parent ? clk->parent->name : "?");

	return 0;
}

static u32 mux_get_rate(struct clk *clk)
{
	mux_enable(clk, true);

	assert(clk->parent);

	clk->rate = clk_get_rate(clk->parent);
	return clk->rate;
}

static int mux_set_parent(struct clk *clk, struct clk *parent)
{
	int i;

	for (i = 0; i < clk->nr_input; i++) {
		if (clk->cmx[i] == parent) {
			/* yes, we do pointer comparison */
			set_mux(clk, i);
			clk->parent = parent;
			return 0;
		}
	}
	return -1;
}

static struct clk *mux_get_parent(struct clk *clk)
{
	u32 v = get_mux(clk);

	return clk->cmx[v];
}

static struct clk_ops mux_ops = {
	.enable		= mux_enable,
	.get_rate	= mux_get_rate,
	.set_parent	= mux_set_parent,
	.get_parent	= mux_get_parent,
};

/* Divider */

static int div_enable(struct clk *clk, int enable)
{
	u32 div, v, m, s;

	if (!clk->hw.base)
		return 0;

	v = readl(clk->hw.base);
	m = mask(clk->nr_input);
	s = clk->hw.shift;

	div = getbf(v, m, s);
	clk->div = div + 1;

	return 0;
}

static u32 div_get_rate(struct clk *clk)
{
	assert(clk->parent);
	assert(clk->div);

	div_enable(clk, true);

	return clk_get_rate(clk->parent) / clk->div;
}

static u32 div_get_div(struct clk *clk)
{
	div_enable(clk, true);

	return clk->div;
}

static int div_set_div(struct clk *clk, u32 div)
{
	u32 v, m, s;

	if (!clk->hw.base)
		return 0;

	v = readl(clk->hw.base);
	m = mask(clk->nr_input);
	s = clk->hw.shift;

	setbf(v, m, s, (div - 1));
	writel(v, clk->hw.base);

	clk->div = div;

	return 0;
}

static struct clk_ops div_ops = {
	.enable   = div_enable,
	.get_rate = div_get_rate,
	.get_div  = div_get_div,
	.set_div  = div_set_div,
};

/* SCM2010 clock definitions */

/*
 * source clocks
 */

static CLK(xtal_40m) = {
	.name = "xtal_40m", 	/* XTAL_clk (40MHz) */
	.rate = 40000000,
};

static CLK(pll_480m) = {
	.name = "pll_480m", 	/* PLL_clk (480MHz) */
	.rate = 480000000,
};

static CLK(osc_32k) = {
	.name = "osc_32k", 		/* 32k_osc_clk (32768Hz) */
	.rate = 32768,
};

/*
 * core clocks
 */

static struct clk *mux_core[] = {
	[0] = llsym(struct clk, xtal_40m, clk),
	[1] = llsym(struct clk, pll_480m, clk),
};

static CLK(core) = {	/* 40MHz / 480MHz */
	.name 		= "core",
	.cmx 		= mux_core,
	.nr_input 	= ARRAY_SIZE(mux_core),
	.ops		= &mux_ops,
	.hw.base	= (void *)SYS(SYS_CLK_CTRL),
	.hw.shift	= 12,
};

static CLK(div_d25) = {
	.name 	  	= "div_d25",
	.parent  	= clksym(core),
	.div 	 	= 1,
	.nr_input 	= 8,
	.ops 	 	= &div_ops,
	.hw.base	= (void *)SYS(SYS_CLK_CTRL),
	.hw.shift	= 0,
};

static CLK(d25) = {
	.name = "d25",
	.parent = clksym(div_d25),
};

static CLK(div_n22) = {
	.name 	  	= "div_n22",
	.parent  	= clksym(d25),
	.div 	 	= 1,
	.nr_input 	= 8,
	.ops 	 	= &div_ops,
	.hw.base	= (void *)SYS(SYS_CLK_CTRL),
	.hw.shift	= 3,
};

static CLK(n22) = {
	.name = "n22",
	.parent = clksym(div_n22),
};

static CLK(div_hclk) = {
	.name 	  	= "div_hclk",
	.parent  	= clksym(n22),
	.div 	 	= 1,
	.nr_input 	= 8,
	.ops 	 	= &div_ops,
	.hw.base	= (void *)SYS(SYS_CLK_CTRL),
	.hw.shift	= 6,
};

static CLK(hclk) = {
	.name = "hclk",
	.parent = clksym(div_hclk),
};

static CLK(div_pclk) = {
	.name 	  	= "div_pclk",
	.parent  	= clksym(hclk),
	.div 	 	= 1,
	.nr_input 	= 8,
	.ops 	 	= &div_ops,
	.hw.base	= (void *)SYS(SYS_CLK_CTRL),
	.hw.shift	= 9,
};

static CLK(pclk) = {
	.name = "pclk",
	.parent = clksym(div_pclk),
};

static CLK(div_2) = {
	.name 	  	= "div_2",
	.parent  	= clksym(pll_480m),
	.div 	 	= 2,
	.ops 	 	= &div_ops,
};

static CLK(div_6) = {
	.name 	  	= "div_6",
	.parent  	= clksym(pll_480m),
	.div 	 	= 6,
	.ops 	 	= &div_ops,
};

static CLK(240m) = {
	.name = "240m",
	.parent = clksym(div_2),
};

static CLK(80m) = {
	.name = "80m",
	.parent = clksym(div_6),
};

/*
 * Peripheral clock definitions
 * xxx/hclk means host i/f clock
 * and xxx/pclk means peripheral clock.
 * Usually, host i/f clock derives from HCLK,
 * and peripheral clock derives from PCLK.
 * But scm2010 doesn't follow this convention,
 * Refer to 'parent' clock with care.
 */

/*
 * UART (ATCUART)
 */

static CLK(div_uart0) = {
	.name 	  	= "div_uart0",
	.parent  	= clksym(xtal_40m),
#ifdef CONFIG_SYS_FPGA
	.div 	 	= 2,
#else
	.div 	 	= 1,
#endif
	.nr_input 	= 8,
	.ops 	 	= &div_ops,
	.hw.base	= (void *)SYS(UART0_CFG),
	.hw.shift	= 8,
};

static CLK(div_uart1) = {
	.name 	  	= "div_uart1",
	.parent  	= clksym(xtal_40m),
#ifdef CONFIG_SYS_FPGA
	.div 	 	= 2,
#else
	.div 	 	= 1,
#endif
	.nr_input 	= 8,
	.ops 	 	= &div_ops,
	.hw.base	= (void *)SYS(UART1_CFG),
	.hw.shift	= 8,
};

static CLK(div_uart2) = {
	.name 	  	= "div_uart2",
	.parent  	= clksym(xtal_40m),
#ifdef CONFIG_SYS_FPGA
	.div 	 	= 2,
#else
	.div 	 	= 1,
#endif
	.nr_input 	= 8,
	.ops 	 	= &div_ops,
	.hw.base	= (void *)SYS(UART2_CFG),
	.hw.shift	= 8,
};

static CLK_ARRAY(uart) = {
	dev_clk("atcuart.0/hclk", pclk, SYS(UART0_CFG), 0),
	dev_clk("atcuart.0/pclk", div_uart0, SYS(UART0_CFG), 1),
	dev_clk("atcuart.1/hclk", pclk, SYS(UART1_CFG), 0),
	dev_clk("atcuart.1/pclk", div_uart1, SYS(UART1_CFG), 1),
	dev_clk("atcuart.2/hclk", pclk, SYS(UART2_CFG), 0),
	dev_clk("atcuart.2/pclk", div_uart2, SYS(UART2_CFG), 1),
};

/*
 * SPI (ATCSPI)
 */

static struct clk *mux_spi0_1[] = {
	[0] = llsym(struct clk, xtal_40m, clk),
	[1] = llsym(struct clk, 240m, clk),
};

static struct clk *mux_spi2[] = {
	[0] = llsym(struct clk, xtal_40m, clk),
	[1] = llsym(struct clk, 80m, clk),
};

static CLK(mux_spi0) = {	/* 40MHz / 240MHz */
	.name 		= "mux_spi0",
	.cmx 		= mux_spi0_1,
	.nr_input 	= ARRAY_SIZE(mux_spi0_1),
	.ops		= &mux_ops,
	.hw.base	= (void *)SYS(SPI0_CFG),
	.hw.shift	= 8,
};

static CLK(mux_spi1) = {	/* 40MHz / 240MHz */
	.name 		= "mux_spi1",
	.cmx 		= mux_spi0_1,
	.nr_input 	= ARRAY_SIZE(mux_spi0_1),
	.ops		= &mux_ops,
	.hw.base	= (void *)SYS(SPI1_CFG),
	.hw.shift	= 8,
};

static CLK(mux_spi2) = {	/* 40MHz / 80MHz */
	.name 		= "mux_spi2",
	.cmx 		= mux_spi2,
	.nr_input 	= ARRAY_SIZE(mux_spi2),
	.ops		= &mux_ops,
	.hw.base	= (void *)SYS(SPI2_CFG),
	.hw.shift	= 8,
};

static CLK_ARRAY(spi) = {
#ifdef CONFIG_USE_SPI0_FLASH
	dev_clk("atcspi200-xip.0/hclk", pclk, SYS(SPI0_CFG), 0),
	dev_clk("atcspi200-xip.0/pclk", mux_spi0, SYS(SPI0_CFG), 1),
#else
	dev_clk("atcspi.0/hclk", pclk, SYS(SPI0_CFG), 0),
	dev_clk("atcspi.0/pclk", mux_spi0, SYS(SPI0_CFG), 1),
#endif
#ifdef CONFIG_USE_SPI1_FLASH
	dev_clk("atcspi200-xip.1/hclk", pclk, SYS(SPI1_CFG), 0),
	dev_clk("atcspi200-xip.1/pclk", mux_spi1, SYS(SPI1_CFG), 1),
#else
	dev_clk("atcspi.1/hclk", pclk, SYS(SPI1_CFG), 0),
	dev_clk("atcspi.1/pclk", mux_spi1, SYS(SPI1_CFG), 1),
#endif
#ifdef CONFIG_USE_SPI2_FLASH
	dev_clk("atcspi200-xip.2/hclk", pclk, SYS(SPI2_CFG), 0),
	dev_clk("atcspi200-xip.2/pclk", mux_spi2, SYS(SPI2_CFG), 1),
#else
	dev_clk("atcspi.2/hclk", pclk, SYS(SPI2_CFG), 0),
	dev_clk("atcspi.2/pclk", mux_spi2, SYS(SPI2_CFG), 1),
#endif
};

/*
 * Timer (ATCPIT)
 */

static CLK_ARRAY(timer) = {
	dev_clk("timer.0/hclk", pclk, SYS(PIT0_CFG), 0),
    /* Use APB clock for clock source */
	dev_clk("timer.0/pclk", pclk, SYS(PIT0_CFG), 0),
	dev_clk("timer.1/hclk", pclk, SYS(PIT1_CFG), 0),
    /* Use APB clock for clock source */
	dev_clk("timer.1/pclk", pclk, SYS(PIT1_CFG), 0),
};

/*
 * Watchdog (ATCWDT)
 */
static CLK_ARRAY(wdog) = {
	dev_clk("atcwdt/hclk", pclk, 0, 0),
	dev_clk("atcwdt/pclk", osc_32k, 0, 0),
};

/*
 * SDIO (SmartDV)
 */
static CLK_ARRAY(sdio) = {
	dev_clk("sdio/hclk", hclk, SYS(SDIO_CTRL), 0),
	dev_clk("sdio/pclk", pclk, SYS(SDIO_CTRL), 1),
};

/*
 * PTA
 */
static CLK_ARRAY(pta) = {
	dev_clk("pta/hclk", hclk, SYS(DEF_CLK_CFG), 0),
};

/*
 * I2S
 */

/* XXX: choose to define get_rate, set_rate not to waste too much
 *      memory just to define 'dedicated' dividers.
 */

static CLK(div_10) = {
	.name 	  	= "div_10",
	.parent  	= clksym(pll_480m),
	.div 	 	= 10,
	.ops 	 	= &div_ops,
};

static u32 i2s_clk_get_rate(struct clk *clk)
{
	u32 v;
	u32 div, rate;

	v = readl(SYS(I2S_CFG(0)));
	div = ((v & 0x300) >> 8);
	if (div == 0 || div == 3) {
		/* XXX: pad input can't be determined. */
		div = 1;
	} else if (div < 3) {
		div *= 2;
	}

	rate = clk_get_rate(clk->parent) / div;

	return rate;
}

static int i2s_clk_set_rate(struct clk* clk, u32 rate)
{
	u32 v;
	u32 div;

	if (rate < 12000000) {
	   rate = 12000000;
	}
	if (rate > 48000000) {
		rate = 48000000;
	}

	div = 48000000 / rate;
	rate = 48000000 / div;

	if (div == 1) {
		div = 0;
	} else {
		div /= 2;
	}

	v = readl(SYS(I2S_CFG(0)));
	v &= ~0x300;
	v |= ((div & 0x3) << 8);
	writel(v, SYS(I2S_CFG(0)));

	clk->rate = rate;

	return 0;
}

static struct clk_ops i2s_clk_ops = {
	.enable   = dev_clk_enable,
	.get_rate = i2s_clk_get_rate,
	.set_rate = i2s_clk_set_rate,
};

static CLK(i2s) = {
	.name 		= "i2s/mclk",
	.parent  	= clksym(div_10),
	.ops 	 	= &i2s_clk_ops,
	.hw.base	= (void *)SYS(I2S_CFG(0)),
	.hw.shift	= 1,
};

/*
 * I2C (ATCIIC)
 */

static CLK_ARRAY(i2c) = {
	dev_clk("atci2c.0/pclk", pclk, 0, 0),
	dev_clk("atci2c.1/pclk", pclk, 0, 0),
};

#endif /* CONFIG_USE_DEFAULT_CLK */

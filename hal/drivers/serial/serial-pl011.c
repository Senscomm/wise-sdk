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
#include <hal/clk.h>
#include <hal/pinctrl.h>
#include <hal/serial.h>
#include <hal/console.h>
#include <hal/kmem.h>

#include <string.h>
#include <stdio.h>

#undef putc

#if 1
#define warn(args...)
#define dbg(args...)
#else
#define warn(args...) printk(args)
#define dbg(args...) printk(args)
#endif
#define err(args...) printk(args)

/*
 * FIXME: dma+fifo+irq
 */

#define pl011_addr(device, oft) (u32 *)((device->base[0] + oft))

#define OFT_PL011_DR			0x00
#define OFT_PL011_RSR 			0x04
#define OFT_PL011_ECR 			0x04
#define OFT_PL011_FR  			0x18
#define OFT_PL011_IBRD			0x24
#define OFT_PL011_FBRD			0x28
#define OFT_PL011_LCRH			0x2c
#define OFT_PL011_CR			0x30
#define OFT_PL011_FLS 			0x34
#define OFT_PL011_IMSC			0x38
#define OFT_PL011_MIS 			0x40
#define OFT_PL011_ICR 			0x44
#define OFT_PL011_DMACR			0x48

#define UART_PL011_RSR_OE		0x08
#define UART_PL011_RSR_BE		0x04
#define UART_PL011_RSR_PE		0x02
#define UART_PL011_RSR_FE		0x01

#define UART_PL011_FR_TXFE		0x80
#define UART_PL011_FR_RXFF		0x40
#define UART_PL011_FR_TXFF		0x20
#define UART_PL011_FR_RXFE		0x10
#define UART_PL011_FR_BUSY		0x08
#define UART_PL011_FR_TMSK		(UART_PL011_FR_TXFF + UART_PL011_FR_BUSY)

/*
 *  PL011 definitions
 */
#define UART_PL011_LCRH_SPS		(1 << 7)
#define UART_PL011_LCRH_WLEN_8		(3 << 5)
#define UART_PL011_LCRH_WLEN_7		(2 << 5)
#define UART_PL011_LCRH_WLEN_6		(1 << 5)
#define UART_PL011_LCRH_WLEN_5		(0 << 5)
#define UART_PL011_LCRH_FEN		(1 << 4)
#define UART_PL011_LCRH_STP2		(1 << 3)
#define UART_PL011_LCRH_EPS		(1 << 2)
#define UART_PL011_LCRH_PEN		(1 << 1)
#define UART_PL011_LCRH_BRK		(1 << 0)

#define UART_PL011_CR_CTSEN		(1 << 15)
#define UART_PL011_CR_RTSEN		(1 << 14)
#define UART_PL011_CR_OUT2		(1 << 13)
#define UART_PL011_CR_OUT1		(1 << 12)
#define UART_PL011_CR_RTS		(1 << 11)
#define UART_PL011_CR_DTR		(1 << 10)
#define UART_PL011_CR_RXE		(1 << 9)
#define UART_PL011_CR_TXE		(1 << 8)
#define UART_PL011_CR_LPE		(1 << 7)
#define UART_PL011_CR_IIRLP		(1 << 2)
#define UART_PL011_CR_SIREN		(1 << 1)
#define UART_PL011_CR_UARTEN		(1 << 0)

#define UART_PL011_IMSC_OEIM		(1 << 10)
#define UART_PL011_IMSC_BEIM		(1 << 9)
#define UART_PL011_IMSC_PEIM		(1 << 8)
#define UART_PL011_IMSC_FEIM		(1 << 7)
#define UART_PL011_IMSC_RTIM		(1 << 6)
#define UART_PL011_IMSC_TXIM		(1 << 5)
#define UART_PL011_IMSC_RXIM		(1 << 4)
#define UART_PL011_IMSC_DSRMIM		(1 << 3)
#define UART_PL011_IMSC_DCDMIM		(1 << 2)
#define UART_PL011_IMSC_CTSMIM		(1 << 1)
#define UART_PL011_IMSC_RIMIM		(1 << 0)

struct pl011_port {
	struct serial_port port;
	struct clk *pclk;
	struct pinctrl_pin_map *pmap[4];
};

#define to_pl011_port(port) \
	container_of(port, struct pl011_port, port)

static u32 pl011_readl(struct device *dev, u32 offset)
{
	u32 *addr, v;

	addr = pl011_addr(dev, offset);
	v = readl(addr);

	return v;
}

static void pl011_writel(u32 val, struct device *dev, u32 offset)
{
	writel(val, pl011_addr(dev, offset));
}

static int pl011_fifo_is_full(struct device *dev)
{
	u32 v = readl(pl011_addr(dev, OFT_PL011_FR));

	return (v & UART_PL011_FR_TXFF);
}

static int pl011_fifo_is_empty(struct device *dev)
{
	u32 v = readl(pl011_addr(dev, OFT_PL011_FR));

	return (v & UART_PL011_FR_TXFE);
}

static int pl011_tstc(struct device *dev)
{
	u32 v = readl(pl011_addr(dev, OFT_PL011_FR));

	return !(v & UART_PL011_FR_RXFE);
}

static int pl011_putc(struct device *dev, char c)
{
	if (pl011_fifo_is_full(dev))
		return -EAGAIN;

	pl011_writel(c, dev, OFT_PL011_DR);

	return 0;
}

static int pl011_getc(struct device *dev)
{
	u32 v;

	if (!pl011_tstc(dev))
		return -EAGAIN;

	v = readl(pl011_addr(dev, OFT_PL011_DR));
	if (v & 0xff00) {
		/* Error */
		writel(-1, pl011_addr(dev, OFT_PL011_ECR));
		return -EINVAL;
	}

	return (int) v;
}

/**
 *
 * Returns 0 if baudrate is invalid.
 */

static int pl011_set_termios(struct serial_port *port, struct termios *termios)
{
	struct pl011_port *plport = to_pl011_port(port);
	struct device *dev = port->device;
	struct clk *p __maybe_unused, *uartclk = plport->pclk;
	int baud, div;
	u32 lcrh = 0, cr, v = 0;

	switch (termios->c_cflag & CSIZE) {
		case CS5:
			lcrh |= UART_PL011_LCRH_WLEN_5;
			break;
		case CS6:
			lcrh |= UART_PL011_LCRH_WLEN_6;
			break;
		case CS7:
			lcrh |= UART_PL011_LCRH_WLEN_7;
			break;
		default:
			lcrh |= UART_PL011_LCRH_WLEN_8;
			break;
	}

	if (termios->c_cflag & CSTOPB)
		lcrh |= UART_PL011_LCRH_STP2;

	if (termios->c_cflag & PARENB) {
		lcrh |= UART_PL011_LCRH_PEN;
		if (!(termios->c_cflag & PARODD))
			lcrh |= UART_PL011_LCRH_EPS;
	}

	cr = pl011_readl(dev, OFT_PL011_CR);
	cr |= UART_PL011_CR_UARTEN | UART_PL011_CR_TXE | UART_PL011_CR_RXE;

	if (termios->c_cflag & CRTSCTS) {
		if (cr & UART_PL011_CR_RTS)
			cr |= UART_PL011_CR_RTSEN;
		cr |= UART_PL011_CR_CTSEN;
	} else {
		cr &= ~(UART_PL011_CR_RTSEN | UART_PL011_CR_CTSEN);
	}

	baud = uart_get_baudrate(termios);
	uartclk = plport->pclk;
#if 0
	/* FIXME: select optimal source clock given baud rate */
	for (i = 0; i < clk_num_source(uartclk); i++) {
		p = clk_get_source(uartclk, i);
		dbg("inspecting %s (%lu Hz)\n", p->name, clk_get_rate(p));

		if (clk_get_rate(p) >= baud * 16) {
			dbg("clk: %s sets %s as the input\n",
				uartclk->name, p->name);

			clk_set_parent(uartclk, p);
			ret = 0;
			break;
		}
	}
	if (i == clk_num_source(uartclk))
		return -EINVAL;

	clk_set_rate(uartclk, baud * 16);
#endif
	div = ((clk_get_rate(uartclk) * 4) + (baud / 2)) / baud;
	dbg("baud=%d, uartclk=%lu, div=0x%08x\n", baud, clk_get_rate(uartclk), div);

	pl011_writel(0, dev, OFT_PL011_CR);

	pl011_writel(div >> 6, dev, OFT_PL011_IBRD);
	pl011_writel(div & 0x3f, dev, OFT_PL011_FBRD);

	v = UART_PL011_IMSC_RTIM | UART_PL011_IMSC_RXIM;
	pl011_writel(v, dev, OFT_PL011_IMSC);

	lcrh |= UART_PL011_LCRH_FEN; /* Enable FIFO */
	pl011_writel(lcrh, dev, OFT_PL011_LCRH);

	pl011_writel(cr, dev, OFT_PL011_CR);

	memcpy(&port->oldtermios, termios, sizeof(*termios));

	return 0;
}

static int pl011_get_termios(struct serial_port *port, struct termios *termios) __maybe_unused;
static int pl011_get_termios(struct serial_port *port, struct termios *termios)
{
	memcpy(termios, &port->oldtermios, sizeof(*termios));

	return 0;
}

static void pl011_enable_interrupt(struct device *dev, u32 mask)
{
	u32 v = pl011_readl(dev, OFT_PL011_IMSC);

	pl011_writel(v | mask, dev, OFT_PL011_IMSC);
}

static u32 pl011_disable_interrupt(struct device *dev, u32 mask)
{
	u32 v = pl011_readl(dev, OFT_PL011_IMSC);

	pl011_writel(v & ~mask, dev, OFT_PL011_IMSC);

	return v;
}

static void pl011_tx_enable(struct device *dev, bool enable)
{
	u32 v = pl011_readl(dev, OFT_PL011_IMSC) & ~UART_PL011_IMSC_TXIM;

	if (enable)
		v |= UART_PL011_IMSC_TXIM;

	pl011_writel(v, dev, OFT_PL011_IMSC);
}

static bool pl011_tx_stopped(struct device *dev)
{
	u32 v = pl011_readl(dev, OFT_PL011_IMSC);

	return (v & UART_PL011_IMSC_TXIM) != UART_PL011_IMSC_TXIM;
}

static int pl011_console_write(struct serial_port *port, char *s, int n)
{
	struct device *dev = port->device;
	int i;
	u32 old_mask;

	old_mask = pl011_disable_interrupt(dev, UART_PL011_IMSC_TXIM);
	for (i = 0; i < n; i++, s++) {
		if (*s == '\n')
			while(pl011_putc(dev, '\r'));
		while(pl011_putc(dev, *s));
	}
	pl011_enable_interrupt(dev, old_mask);
	return i;
}

static int pl011_irq(int irq, void *data)
{
	struct pl011_port *plport = data;
	struct serial_port *port = &plport->port;
	struct device *dev = port->device;
	char c;
	portBASE_TYPE resched = false;
	int ret;
	u32 status = pl011_readl(dev, OFT_PL011_MIS);

	if (status & UART_PL011_IMSC_TXIM) {
		while (!(pl011_fifo_is_full(dev))) {
			ret = xQueueReceiveFromISR(port->queue[TX], &c, &resched);
			if (ret == pdFALSE) {
				pl011_tx_enable(dev, false);
				break;
			}
			pl011_putc(dev, c);
		}
	}
	if (status & (UART_PL011_IMSC_RTIM | UART_PL011_IMSC_RXIM)) {
		while ((ret = pl011_getc(dev)) >= 0) {
			c = (char) (ret & 0xff);
			xQueueSendFromISR(port->queue[RX], &c, &resched);
		}
	}

	portEND_SWITCHING_ISR(resched);

	return 0;
}

static int pl011_port_init(struct serial_port *port)
{
	struct pl011_port *plport = to_pl011_port(port);
	int i, ret = 0;

	/* Enable the clocks */
	clk_enable(plport->pclk, 1);

	/* Grab the pin */
	if (!plport->pmap[TXD] || !plport->pmap[RXD])
		return -EBUSY;

	for (i = 0; i < ARRAY_SIZE(plport->pmap); i++) {
		if ((plport->pmap[i]) &&
		    ((ret = pinctrl_request_pin(port->device,
						plport->pmap[i]->id,
						plport->pmap[i]->pin)) != 0)) {
			err("failed to claim pin %d\n", plport->pmap[i]->pin);
			goto free_pin;
		}
	}

	/* Configure interface */
	if ((plport->pmap[CTS] && plport->pmap[CTS]->pin != -1) ||
	    (plport->pmap[RTS] && plport->pmap[RTS]->pin != -1)) {
		port->oldtermios.c_cflag |= CRTSCTS;
	}
	pl011_set_termios(port, &port->oldtermios);

	/* Request interrupts */
	ret = request_irq(port->device->irq[0], pl011_irq,
			  dev_name(port->device), 0, plport);
	if (ret)
		goto free_pin;

	return 0;

 free_pin:
	for (; i >= 0; i--) {
		if (plport->pmap[i] && plport->pmap[i]->pin != -1)
			pinctrl_free_pin(port->device, plport->pmap[i]->id,
					 plport->pmap[i]->pin);
	}
	return ret;
}

static int pl011_port_putc(struct serial_port *port, char c)
{
	struct device *dev = port->device;
	unsigned long flags;
	int ret;

	if (pl011_fifo_is_full(dev))
		return -EAGAIN;

	local_irq_save(flags);

	ret = pl011_putc(dev, c);
	if (pl011_fifo_is_full(dev))
		pl011_tx_enable(dev, true);

	local_irq_restore(flags);

	return ret;
}

static void pl011_port_start_tx(struct serial_port *port)
{
	int len = uxQueueMessagesWaitingFromISR(port->queue[TX]);
	unsigned long flags;
	char ch;

	local_irq_save(flags);

	if (pl011_tx_stopped(port->device)) {
		while (len-- > 0 && !pl011_fifo_is_full(port->device)) {
			xQueueReceive(port->queue[TX], &ch, 0);
			pl011_port_putc(port, ch);
		}
	}

	local_irq_restore(flags);
}

static int
pl011_port_ioctl(struct serial_port *port, unsigned cmd, void *arg)
{
	struct termios *termios = arg;

	switch (cmd) {
	case TCSETS:
		return pl011_set_termios(port, termios);
	default:
		return -ENOTTY;
	}
}

static struct serial_port_ops pl011_port_ops = {
	.init		= pl011_port_init,
	.start_tx	= pl011_port_start_tx,
	.ioctl		= pl011_port_ioctl,
	.putc		= pl011_port_putc,
	.con_write	= pl011_console_write,
};

static int pl011_probe(struct device *dev)
{
	struct pl011_port *plport;
	struct serial_port *port;

	plport = kmalloc(sizeof(*plport));
	if (!plport)
		return -ENOMEM;

	memset(plport, 0, sizeof(*plport));
	port = &plport->port;

	port->oldtermios.c_cflag = CS8;
	port->oldtermios.c_ispeed = CONFIG_BAUDRATE;

	/* clk */
	plport->pclk = clk_get(dev, "pclk");
	if (!plport->pclk) {
		err("%s: pclk=%p\n", dev_name(dev), plport->pclk);
		return -ENODEV;
	}

	/* pinctrl */
	plport->pmap[TXD] = pinctrl_lookup_platform_pinmap(dev, "txd");
	plport->pmap[RXD] = pinctrl_lookup_platform_pinmap(dev, "rxd");
	plport->pmap[CTS] = pinctrl_lookup_platform_pinmap(dev, "cts");
	plport->pmap[RTS] = pinctrl_lookup_platform_pinmap(dev, "rts");

	if (plport->pmap[CTS] && plport->pmap[CTS]->pin != -1 &&
		plport->pmap[RTS] && plport->pmap[RTS]->pin != -1)
			port->oldtermios.c_cflag |= CRTSCTS;

	printk("%s: @%p, irq=%2d, clk=%d\n",
		   dev_name(dev), dev->base[0], dev->irq[0], clk_get_rate(plport->pclk));

	dev->driver_data = plport;

	port->device = dev;
	port->ops = &pl011_port_ops;
	port->id = dev_id(dev);

	return register_serial_port(port);
}

int pl011_suspend(struct device *dev, u32 *idle)
{
	struct pl011_port *plport = dev->driver_data;
	struct serial_port *port = &plport->port;
	u32 busy = 0;

	if (!port->initialized)
		return 0;

	busy |= uxQueueMessagesWaitingFromISR(port->queue[TX]) ? 1: 0;
	busy |= uxQueueMessagesWaitingFromISR(port->queue[RX]) ? 1 << 1: 0;
	busy |= pl011_fifo_is_empty(dev) ? 0 : 1 << 2;

	return busy? -EBUSY: 0;
}

int pl011_resume(struct device *dev)
{
	return 0;
}

static declare_driver(pl011) = {
	.name	= "pl011",
	.probe	= pl011_probe,
	.suspend= pl011_suspend,
	.resume = pl011_resume,
};

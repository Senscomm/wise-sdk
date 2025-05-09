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
#ifdef CONFIG_PINCTRL
#include <hal/pinctrl.h>
#endif
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

#define sc20uart_addr(device, oft) (u32 *)((device->base[0] + oft))

#define OFT_SC20UART_RBR		0x00
#define OFT_SC20UART_THR 		0x00
#define OFT_SC20UART_DLL 		0x00
#define OFT_SC20UART_IER  		0x04
#define OFT_SC20UART_DLH		0x04
#define OFT_SC20UART_IIR		0x08
#define OFT_SC20UART_FCR		0x08
#define OFT_SC20UART_LCR		0x0C
#define OFT_SC20UART_MCR 		0x10
#define OFT_SC20UART_LSR		0x14
#define OFT_SC20UART_MSR 		0x18
#define OFT_SC20UART_SCR 		0x1C
#define OFT_SC20UART_FOR 		0x24
#define OFT_SC20UART_ABR 		0x28

/* UART_IER (Interrupt Enable) */

/* EOR interrupt enable */
#define SC20UART_IER_EORIE		0x200
/* High Speed UART Enable */
#define SC20UART_IER_HSE		0x100
/* DMA Requests Enable */
#define SC20UART_IER_DMAE		0x80
/* UART Unit Enable */
#define SC20UART_IER_UUE		0x40
/* NRZ Coding Enable */
#define SC20UART_IER_NRZE		0x20
/* Receiver Time-out Interrupt Enable */
#define SC20UART_IER_RTOIE		0x10
/* Modem Interrupt Enable */
#define SC20UART_IER_MIE		0x08
/* Receive Line Status Interrupt Enable */
#define SC20UART_IER_RLSE		0x04
/* Transmit Data Request Interrupt Enable */
#define SC20UART_IER_TIE		0x02
/* Receive Data Available Interrupt Enable */
#define SC20UART_IER_RAVIE		0x01

/* UART_IIR (Interrupt Identification Register) */

/*
 * TOD([3]), IID10([2:1]), and NIP([0]) are combined
 * to be interpreted as an interrupt ID as follows.
 *
 * INTRID[3:0]		Meaning
 *
 * 0x00			Modem status (CTS, DSR, RI, DCD)
 * 0x02			Transmit FIFO requests data
 * 0x04			Receive data available
 * 0x06			Receive error (Overrun, Parity, Framing, Break, FIFO error)
 * 0x0C			Rx Timeout detected (FIFO mode only)
 */
#define SC20UART_IIR_INTRID		0x0F
#define SC20UART_IIR_INTRID_MSR		0x00
#define SC20UART_IIR_INTRID_THR		0x02
#define SC20UART_IIR_INTRID_RBR		0x04
#define SC20UART_IIR_INTRID_LSR		0x06
#define SC20UART_IIR_INTRID_RTI		0x0C

/* UART_FCR (FIFO Control Register) */

/* Interrupt Trigger Level (threshold) */
#define SC20UART_FCR_ITL		0xC0
/* ITL = 0 : 1 byte or more */
#define SC20UART_FCR_ITL_0		0x00
/* ITL = 1 : 8 byte or more */
#define SC20UART_FCR_ITL_1		0x40
/* ITL = 2 : 16 byte or more */
#define SC20UART_FCR_ITL_2		0x80
/* ITL = 3 : 32 byte or more */
#define SC20UART_FCR_ITL_3		0xC0
/* 32-Bit Peripheral Bus */
#define SC20UART_FCR_BUS		0x20
/* Trailing Bytes */
#define SC20UART_FCR_TRAIL		0x10
/* Transmitter Interrupt Level */
#define SC20UART_FCR_TIL		0x08
/* Reset Transmit FIFO */
#define SC20UART_FCR_RESETTF		0x04
/* Reset Receive FIFO */
#define SC20UART_FCR_RESETRF		0x02
/* Transmit and Receive FIFO Enable */
#define SC20UART_FCR_TRFIFOE		0x01

/* UART_LCR (Line Control Register) */

/* Divisor Latch Access Bit */
#define SC20UART_LCR_DLAB		0x80
/* Send Break */
#define SC20UART_LCR_SB			0x40
/* Sticky Parity */
#define SC20UART_LCR_STKYP		0x20
/* Even Parity Select */
#define SC20UART_LCR_EPS		0x10
/* Parity Enable */
#define SC20UART_LCR_PEN		0x08
/* Stop Bits */
#define SC20UART_LCR_STB		0x04
/* Word Length Select */
#define SC20UART_LCR_WLS10		0x03
#define SC20UART_LCR_WLS10_7		0x02
#define SC20UART_LCR_WLS10_8		0x03

/* UART_MCR (Modem Control Register) */

/* DMA RxReq for empty RX FIFO enable */
#define SC20UART_MCR_EPT_RXREQ_EN	0x80
/* Mask bit for EOR interrupt */
#define SC20UART_MCR_EOR_INT_MASK	0x40
/* Auto-flow Control Enable */
#define SC20UART_MCR_AFE		0x20
/* Loopback Mode */
#define SC20UART_MCR_LOOP		0x10
/* OUT2 Signal Control */
#define SC20UART_MCR_OUT2		0x08
/* Test Bit */
#define SC20UART_MCR_OUT1		0x04
/* Request to Send */
#define SC20UART_MCR_RTS		0x02
/* Data Terminal Ready */
#define SC20UART_MCR_DTR		0x01

/* UART_LSR (Line Status Register) */

/* FIFO Error Status */
#define SC20UART_LSR_FIFOE		0x80
/* Transmit Empty */
#define SC20UART_LSR_TEMT		0x40
/* Transmit Data Request */
#define SC20UART_LSR_TDRQ		0x20
/* Break Interrupt */
#define SC20UART_LSR_BI			0x10
/* Framing Error */
#define SC20UART_LSR_FE			0x08
/* Parity Error */
#define SC20UART_LSR_PE			0x04
/* Overrun Error */
#define SC20UART_LSR_OE			0x02
/* Data Ready */
#define SC20UART_LSR_DR			0x01

#define SC20UART_LSR_RXERR \
	(SC20UART_LSR_FIFOE | SC20UART_LSR_FE | SC20UART_LSR_PE | \
	 SC20UART_LSR_OE)

/* UART_FOR (Receive FIFO Occupancy Register) */

/* Byte Count */
#define SC20UART_FOR_BYTE_COUNT		0x3F

/* UART_ABR (Auto-Baud Control Register */

/* Auto-Baud Enable */
#define SC20UART_ABR_ABE		0x01

struct sc20uart_port {
	struct serial_port port;

	struct clk *pclk;
#ifdef CONFIG_PINCTRL
	struct pinctrl_pin_map *pmap[4];
#endif
	struct {
		int depth;  /* fifo size in byte */
		int fill; /* current fifo depth (occupancy) */
	} fifo;
	int rx_err;
	int fifo_ctl;
};

#define to_sc20uart_port(port) \
	container_of(port, struct sc20uart_port, port)

static u32 sc20uart_readl(struct device *dev, u32 offset)
{
	u32 *addr, v;

	addr = sc20uart_addr(dev, offset);
	v = readl(addr);

	return v;
}

static void sc20uart_writel(u32 val, struct device *dev, u32 offset)
{
	writel(val, sc20uart_addr(dev, offset));
}

static void sc20uart_tx_enable(struct device *dev, bool enable)
{
	u32 v = sc20uart_readl(dev, OFT_SC20UART_IER) & ~SC20UART_IER_TIE;
	if (enable)
		v |= SC20UART_IER_TIE;

	sc20uart_writel(v, dev, OFT_SC20UART_IER);
}

static int sc20uart_fifo_is_full(struct sc20uart_port *atport)
{
	return (atport->fifo.fill == atport->fifo.depth);
}

static int sc20uart_getc(struct device *dev)
{
	u32 v;

	v = readl(sc20uart_addr(dev, OFT_SC20UART_LSR));

	if (v & SC20UART_LSR_FIFOE)
		return -EINVAL;

	if (!(v & SC20UART_LSR_DR))
		return -EAGAIN;

	v = readl(sc20uart_addr(dev, OFT_SC20UART_RBR));

	return (int) v;
}

static inline int sc20uart_tstc(struct device *dev)
{
	u32 v = readl(sc20uart_addr(dev, OFT_SC20UART_LSR));

	return (v & SC20UART_LSR_DR);
}

#ifndef abs
#define abs(x) ((x) < 0 ? -(x) : (x))
#endif

static void sc20uart_set_baudrate(struct serial_port *port, int baud)
{
	struct sc20uart_port *atport = to_sc20uart_port(port);
	struct device *dev = port->device;
	struct clk *uartclk = atport->pclk;
	int div;
	int os = 16;	/* fixed */
	int lc = sc20uart_readl(dev, OFT_SC20UART_LCR);
	int freq = clk_get_rate(uartclk);

	div = (clk_get_rate(uartclk) / (baud * os));

	if (abs(div * baud * os - freq) > abs((div + 1) * baud * os - freq))
		div++;

	lc |= SC20UART_LCR_DLAB;
	sc20uart_writel(lc, dev, OFT_SC20UART_LCR);

	sc20uart_writel((u8)(div >> 8), dev, OFT_SC20UART_DLH);
	sc20uart_writel((u8)div, dev, OFT_SC20UART_DLL);

	lc &= ~SC20UART_LCR_DLAB;
	sc20uart_writel(lc, dev, OFT_SC20UART_LCR);

	dbg("baud=%d, uartclk=%lu, div=0x%08x\n", baud, clk_get_rate(uartclk),
	    div);
}

static
int sc20uart_set_termios(struct serial_port *port, struct termios *termios)
{
	struct sc20uart_port *atport = to_sc20uart_port(port);
	struct device *dev = port->device;
	int baud;
	u32 lc = 0, fc = 0, mc, v = 0;

	baud = uart_get_baudrate(termios);
	sc20uart_set_baudrate(port, baud);

	switch (termios->c_cflag & CSIZE) {
		case CS5:
		case CS6:
			err("%s: not supported wsize(%s)\n", dev_name(dev),
					(termios->c_cflag & CSIZE) == CS5 ? "CS5" : "CS6");
			return -1;
		case CS7:
			lc |= SC20UART_LCR_WLS10_7;
			break;
		default:
			lc |= SC20UART_LCR_WLS10_8;
			break;
	}

	if (termios->c_cflag & CSTOPB)
		lc |= SC20UART_LCR_STB;

	if (termios->c_cflag & PARENB) {
		lc &= ~SC20UART_LCR_STKYP;
		if (!(termios->c_cflag & PARODD))
			lc |= SC20UART_LCR_EPS;
		else
			lc &= ~SC20UART_LCR_EPS;
		lc |= SC20UART_LCR_PEN;
	}

	/* XXX: what would be the best values for FIFO thresholds ? */
	fc |= SC20UART_FCR_ITL_0;
	fc |= (SC20UART_FCR_TRFIFOE | SC20UART_FCR_RESETTF | SC20UART_FCR_RESETRF);

	mc = sc20uart_readl(dev, OFT_SC20UART_MCR);
	if (termios->c_cflag & CRTSCTS) {
		mc |= SC20UART_MCR_AFE;
	} else {
		mc &= ~SC20UART_MCR_AFE;
	}

	sc20uart_writel(lc, dev, OFT_SC20UART_LCR);
	sc20uart_writel(fc, dev, OFT_SC20UART_FCR);
	sc20uart_writel(mc, dev, OFT_SC20UART_MCR);

	fc &= ~(SC20UART_FCR_RESETTF | SC20UART_FCR_RESETRF);
	atport->fifo_ctl = fc;

	v = SC20UART_IER_UUE | SC20UART_IER_RLSE | SC20UART_IER_RAVIE | SC20UART_IER_RTOIE;
	sc20uart_writel(v, dev, OFT_SC20UART_IER);

	memcpy(&port->oldtermios, termios, sizeof(*termios));

	return 0;
}

static inline
int sc20uart_get_termios(struct serial_port *port, struct termios *termios)
{
	memcpy(termios, &port->oldtermios, sizeof(*termios));

	return 0;
}

static inline
void sc20uart_enable_interrupt(struct device *dev, u32 mask)
{
	u32 v = sc20uart_readl(dev, OFT_SC20UART_IER);

	sc20uart_writel(v | mask, dev, OFT_SC20UART_IER);
}

static inline u32 sc20uart_disable_interrupt(struct device *dev, u32 mask)
{
	u32 v = sc20uart_readl(dev, OFT_SC20UART_IER);

	sc20uart_writel(v & ~mask, dev, OFT_SC20UART_IER);

	return v;
}

static bool sc20uart_tx_stopped(struct device *dev)
{
	u32 v = sc20uart_readl(dev, OFT_SC20UART_IER);

	return (v & SC20UART_IER_TIE) != SC20UART_IER_TIE;
}


static int sc20uart_port_putc(struct serial_port *port, char c)
{
	struct sc20uart_port *atport = to_sc20uart_port(port);
	unsigned long flags;

	if (sc20uart_fifo_is_full(atport))
		return -EAGAIN;

	local_irq_save(flags);
	sc20uart_writel(c, port->device, OFT_SC20UART_THR);
	atport->fifo.fill++;
	if (sc20uart_fifo_is_full(atport))
		sc20uart_tx_enable(port->device, true);
	local_irq_restore(flags);

	return 0;
}

static int sc20uart_tx_empty(struct device *dev)
{
#define SC20UART_TX_EMPTY (SC20UART_LSR_TEMT)

	u32 v = sc20uart_readl(dev, OFT_SC20UART_LSR);
	return (v & SC20UART_TX_EMPTY) == SC20UART_TX_EMPTY;
}

static int sc20uart_tx_fifo_empty(struct device *dev)
{
#define SC20UART_TXFIFO_EMPTY (SC20UART_LSR_TDRQ)

	u32 v = sc20uart_readl(dev, OFT_SC20UART_LSR);
	return (v & SC20UART_TXFIFO_EMPTY) == SC20UART_TXFIFO_EMPTY;
}


static void sc20uart_console_putc(struct serial_port *port, char c)
{
	while (!sc20uart_tx_fifo_empty(port->device));
	sc20uart_writel(c, port->device, OFT_SC20UART_THR);
}

static int sc20uart_console_write(struct serial_port *port, char *s, int n)
{
	int i;

	for (i = 0; i < n; i++, s++) {
		if (*s == '\n') {
			sc20uart_console_putc(port, '\r');
			/*continue;*/
		}
		sc20uart_console_putc(port, *s);
	}
	return i;
}

static void sc20uart_port_start_tx(struct serial_port *port)
{
	struct sc20uart_port *atport = to_sc20uart_port(port);
	int len = uxQueueMessagesWaitingFromISR(port->queue[TX]);
	unsigned long flags;
	char ch;

	local_irq_save(flags);

	if (sc20uart_tx_stopped(port->device)) {
		while (len-- > 0 && !sc20uart_fifo_is_full(atport)) {
			xQueueReceive(port->queue[TX], &ch, 0);
			sc20uart_port_putc(port, ch);
		}
	}

	local_irq_restore(flags);
}

static int sc20uart_irq(int irq, void *data)
{
	struct sc20uart_port *atport = data;
	struct serial_port *port = &atport->port;
	struct device *dev = port->device;
	struct device *serial = port->device;
	char c;
	portBASE_TYPE resched = false;
	int ret, len;
	u32 intid, lsr;

	intid = sc20uart_readl(serial, OFT_SC20UART_IIR);
	intid &= SC20UART_IIR_INTRID;

	if (intid == SC20UART_IIR_INTRID_THR) {
		if (atport->fifo_ctl & SC20UART_FCR_TIL)
			atport->fifo.fill = 0;
		else
			atport->fifo.fill = atport->fifo.depth / 2;

		len = uxQueueMessagesWaitingFromISR(port->queue[TX]);
		if (len == 0) {
			/* No more data to transmit - disable tx interrupt */
			sc20uart_tx_enable(serial, false);
			goto txdone;
		}
		while (len-- > 0 && !sc20uart_fifo_is_full(atport)) {
			xQueueReceiveFromISR(port->queue[TX], &c,
					     &resched);
			sc20uart_port_putc(port, c);
			len--;
		}
	}
 txdone:
	if (intid == SC20UART_IIR_INTRID_RBR ||
	    intid == SC20UART_IIR_INTRID_RTI) {
		while ((ret = sc20uart_getc(serial)) >= 0) {
			c = (char) (ret & 0xff);
			xQueueSendFromISR(port->queue[RX], &c, &resched);
		}
	}
	if (intid == SC20UART_IIR_INTRID_LSR) {
		lsr = sc20uart_readl(serial, OFT_SC20UART_LSR);
		lsr &= SC20UART_LSR_RXERR;
		atport->rx_err += lsr? 1 : 0;
		/* err("%s: rx error %08x\n", dev_name(serial), lsr); */
		/* reset rx fifo for recovery */
		sc20uart_writel(atport->fifo_ctl | SC20UART_FCR_RESETRF, dev, OFT_SC20UART_FCR);
	}

	portYIELD_FROM_ISR(resched);

	return 0;
}

static int sc20uart_port_init(struct serial_port *port)
{
	struct sc20uart_port *atport = to_sc20uart_port(port);
	int i __maybe_unused, ret = 0;
	u32 mc;

	/* Enable the clocks */
	clk_enable(atport->pclk, 1);

#ifdef CONFIG_PINCTRL
	/* Grab the pin */
	if (!atport->pmap[TXD] || !atport->pmap[RXD])
		return -EBUSY;

	for (i = 0; i < ARRAY_SIZE(atport->pmap); i++) {
		if ((atport->pmap[i]) &&
		    ((ret = pinctrl_request_pin(port->device,
						atport->pmap[i]->id,
						atport->pmap[i]->pin)) != 0)) {
			err("failed to claim pin %d\n", atport->pmap[i]->pin);
			goto free_pin;
		}
	}

	/* Configure interface */
	if ((atport->pmap[CTS] && atport->pmap[CTS]->pin != -1) ||
	    (atport->pmap[RTS] && atport->pmap[RTS]->pin != -1)) {
		port->oldtermios.c_cflag |= CRTSCTS;
	}
#endif
	sc20uart_set_termios(port, &port->oldtermios);

	/*
	 * OUT2 connects the UART interrupt output to the interrupt controller unit.
	 */
	mc = sc20uart_readl(port->device, OFT_SC20UART_MCR);
	mc |= (SC20UART_MCR_OUT2 | SC20UART_MCR_EOR_INT_MASK);
	sc20uart_writel(mc, port->device, OFT_SC20UART_MCR);

	/* Request interrupts */
	ret = request_irq(port->device->irq[0], sc20uart_irq,
			  dev_name(port->device), port->device->pri[0],
			  atport);
	if (ret)
		goto free_pin;
	return 0;

 free_pin:
#ifdef CONFIG_PINCTRL
	for (; i >= 0; i--) {
		if (atport->pmap[i] && atport->pmap[i]->pin != -1)
			pinctrl_free_pin(port->device, atport->pmap[i]->id,
					 atport->pmap[i]->pin);
	}
#endif
	return ret;
}

static int
sc20uart_port_ioctl(struct serial_port *port, unsigned cmd, void *arg)
{
	struct termios *termios = arg;

	switch (cmd) {
	case TCSETS:
		return sc20uart_set_termios(port, termios);
	default:
		return -ENOTTY;
	}
}

static struct serial_port_ops sc20uart_port_ops = {
	.init 		= sc20uart_port_init,
	.start_tx 	= sc20uart_port_start_tx,
	.ioctl 		= sc20uart_port_ioctl,
	.con_write 	= sc20uart_console_write,
};

static int sc20uart_probe(struct device *dev)
{
	struct sc20uart_port *atport;
	struct serial_port *port;

	atport = kmalloc(sizeof(*atport));
	if (!atport)
		return -ENOMEM;

	memset(atport, 0, sizeof(*atport));
	port = &atport->port;
	port->oldtermios.c_cflag = CS8;
	port->oldtermios.c_ispeed = CONFIG_BAUDRATE;

	/* clk */
	atport->pclk = clk_get(dev, "pclk");
	if (!atport->pclk) {
		err("%s: pclk=%p\n", dev_name(dev), atport->pclk);
		return -ENODEV;
	}

#ifdef CONFIG_PINCTRL
	/* pinctrl */
	atport->pmap[TXD] = pinctrl_lookup_platform_pinmap(dev, "txd");
	atport->pmap[RXD] = pinctrl_lookup_platform_pinmap(dev, "rxd");
	atport->pmap[CTS] = pinctrl_lookup_platform_pinmap(dev, "cts");
	atport->pmap[RTS] = pinctrl_lookup_platform_pinmap(dev, "rts");
	if (atport->pmap[CTS] && atport->pmap[CTS]->pin != -1 &&
	    atport->pmap[RTS] && atport->pmap[RTS]->pin != -1)
		port->oldtermios.c_cflag |= CRTSCTS;
#endif

	/* FIFO */
	atport->fifo.depth = 64; /* fixed */
	atport->fifo.fill = 0;

	dbg("AndesShape(tm) SC20UART100 %s@%p: irq=%2d clk=%d\n", dev_name(dev), dev->base[0],
	       dev->irq[0], clk_get_rate(atport->pclk));

	dev->driver_data = atport;

	port->device = dev;
	port->ops = &sc20uart_port_ops;
	port->id = dev_id(dev);

	return register_serial_port(port);
}

int sc20uart_suspend(struct device *dev, u32 *idle)
{
	struct sc20uart_port *atport = dev->driver_data;
	struct serial_port *port = &atport->port;
	u32 busy = 0;

	if (!port->initialized)
		return 0;
	busy |= uxQueueMessagesWaitingFromISR(port->queue[TX]) ? 1: 0;
	busy |= uxQueueMessagesWaitingFromISR(port->queue[RX]) ? 1 << 1: 0;
	busy |= sc20uart_tx_empty(dev) ? 0 : 1 << 2;
	busy |= (readl(sc20uart_addr(dev, OFT_SC20UART_LSR)) &
		 SC20UART_LSR_DR) ? 1 << 3: 0;
	return busy? -EBUSY: 0;
}

int sc20uart_resume(struct device *dev)
{
	return 0;
}

static declare_driver(sc20uart) = {
	.name 	= "sc20uart",
	.probe 	= sc20uart_probe,
	.suspend = sc20uart_suspend,
	.resume = sc20uart_resume,
};

/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
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
#include <hal/uart.h>
#include <hal/pm.h>
#include <hal/dma.h>

#include <string.h>
#include <stdio.h>

#define atcuart_addr(device, oft) (u32 *)((device->base[0] + oft))

#define OFT_ATCUART_HWC				0x10
#define OFT_ATCUART_OSC				0x14
#define OFT_ATCUART_RBR				0x20
#define OFT_ATCUART_THR 			0x20
#define OFT_ATCUART_DLL 			0x20
#define OFT_ATCUART_IER  			0x24
#define OFT_ATCUART_DLM				0x24
#define OFT_ATCUART_IIR				0x28
#define OFT_ATCUART_FCR				0x28
#define OFT_ATCUART_LCR				0x2C
#define OFT_ATCUART_MCR 			0x30
#define OFT_ATCUART_LSR				0x34
#define OFT_ATCUART_MSR 			0x38
#define OFT_ATCUART_SCR 			0x3C

#define UART_ATCUART_IER_EMSI			0x08
#define UART_ATCUART_IER_ELSI			0x04
#define UART_ATCUART_IER_ETHEI			0x02
#define UART_ATCUART_IER_ERBI			0x01

#define UART_ATCUART_IIR_INTRID			0x0F
#define UART_ATCUART_IIR_INTRID_LSR		0x06
#define UART_ATCUART_IIR_INTRID_RBR		0x04
#define UART_ATCUART_IIR_INTRID_RTI		0x0C
#define UART_ATCUART_IIR_INTRID_THR		0x02
#define UART_ATCUART_IIR_INTRID_MSR		0x00

#define UART_ATCUART_FCR_RFIFOT			0xC0
#define UART_ATCUART_FCR_RFIFOT_0		0x00
#define UART_ATCUART_FCR_RFIFOT_1		0x40
#define UART_ATCUART_FCR_RFIFOT_2		0x80
#define UART_ATCUART_FCR_RFIFOT_3		0xC0
#define UART_ATCUART_FCR_TFIFOT			0x30
#define UART_ATCUART_FCR_TFIFOT_0		0x00
#define UART_ATCUART_FCR_TFIFOT_1		0x10
#define UART_ATCUART_FCR_TFIFOT_2		0x20
#define UART_ATCUART_FCR_TFIFOT_3		0x30
#define UART_ATCUART_FCR_DMAE			0x08
#define UART_ATCUART_FCR_TFIFORST		0x04
#define UART_ATCUART_FCR_RFIFORST		0x02
#define UART_ATCUART_FCR_FIFOE			0x01

#define UART_ATCUART_MCR_AFE			0x20
#define UART_ATCUART_MCR_LOOP			0x10
#define UART_ATCUART_MCR_OUT2			0x08
#define UART_ATCUART_MCR_OUT1			0x04
#define UART_ATCUART_MCR_RTS			0x02
#define UART_ATCUART_MCR_DTR			0x01

#define UART_ATCUART_LCR_DLAB			0x80
#define UART_ATCUART_LCR_BC			0x40
#define UART_ATCUART_LCR_SPS			0x20
#define UART_ATCUART_LCR_EPS			0x10
#define UART_ATCUART_LCR_PEN			0x08
#define UART_ATCUART_LCR_STB			0x04
#define UART_ATCUART_LCR_WLS			0x03
#define UART_ATCUART_LCR_WLS_5			0x00
#define UART_ATCUART_LCR_WLS_6			0x01
#define UART_ATCUART_LCR_WLS_7			0x02
#define UART_ATCUART_LCR_WLS_8			0x03

#define UART_ATCUART_LSR_ERRF			0x80
#define UART_ATCUART_LSR_TEMT			0x40
#define UART_ATCUART_LSR_THRE			0x20
#define UART_ATCUART_LSR_LBRE			0x10
#define UART_ATCUART_LSR_FE			0x08
#define UART_ATCUART_LSR_PE			0x04
#define UART_ATCUART_LSR_OE			0x02
#define UART_ATCUART_LSR_DR			0x01

#define UART_ATCUART_LSR_RXERR \
    (UART_ATCUART_LSR_ERRF | UART_ATCUART_LSR_FE | UART_ATCUART_LSR_PE | \
     UART_ATCUART_LSR_OE)

//#define FULL_DUPLEX_DEBUG

struct dma_ctx {
    uint8_t is_tx;
    int dma_ch;
    struct dma_ctrl ctrl;
    struct dma_desc_chain desc;
    struct device *dev;
};

struct uart_tr {
    uint8_t *tx_buf;
    uint32_t tx_len;
    uint32_t tx_oft;

    uint8_t *rx_buf;
    uint32_t rx_len;
    uint32_t rx_oft;
};

struct raw_access {
    uint8_t enable;

    uart_cb cb;
    void *cb_ctx;

    struct uart_event event;
    struct uart_tr tr;

    uint8_t dma_en;
    struct device *dma_dev;
    struct dma_ctx tx_dma_ctx;
    struct dma_ctx rx_dma_ctx;
};

#ifdef CONFIG_SOC_SCM2010
struct uart_ctx {
    uint32_t osc;
    uint32_t ier;
    uint32_t dll;
    uint32_t dlm;
    uint32_t lcr;
    uint32_t mcr;
};
#endif

struct atcuart_port {
    struct serial_port port;
    struct raw_access raw;

    struct clk *pclk;
    struct pinctrl_pin_map *pmap[4];
    struct {
        int depth;  /* fifo size in byte */
        int fill; /* current fifo depth (occupancy) */
    } fifo;
    int rx_err;
    int fifo_ctl;
#ifdef CONFIG_SOC_SCM2010
    struct uart_ctx ctx;
#endif
};

#define to_atcuart_port(port) \
    container_of(port, struct atcuart_port, port)

/* IRAM section */
__iram__ int atcuart_probe(struct device *dev);
__iram__ int atcuart_shutdown(struct device *dev);

#undef putc

struct serial_port_ops atcuart_port_ops;

#if 1
#define warn(args...)
#define dbg(args...)
#else
#define warn(args...) printk(args)
#define dbg(args...) printk(args)
#endif
#define err(args...) printk(args)

static u32 atcuart_readl(struct device *dev, u32 offset)
{
    u32 *addr, v;

    addr = atcuart_addr(dev, offset);
    v = readl(addr);

    return v;
}

static void atcuart_writel(u32 val, struct device *dev, u32 offset)
{
    writel(val, atcuart_addr(dev, offset));
}

static void atcuart_tx_enable(struct device *dev, bool enable)
{
    u32 v = atcuart_readl(dev, OFT_ATCUART_IER) & ~UART_ATCUART_IER_ETHEI;

    if (enable) {
        v |= UART_ATCUART_IER_ETHEI;
        pm_stay(PM_DEVICE_UART);
    } else {
        pm_relax(PM_DEVICE_UART);
    }

    atcuart_writel(v, dev, OFT_ATCUART_IER);
}

static void atcuart_rx_enable(struct device *dev, bool enable)
{
    u32 v = atcuart_readl(dev, OFT_ATCUART_IER) & ~UART_ATCUART_IER_ERBI;

    if (enable) {
        v |= UART_ATCUART_IER_ERBI;
        pm_stay(PM_DEVICE_UART);
    } else {
        pm_relax(PM_DEVICE_UART);
    }

    atcuart_writel(v, dev, OFT_ATCUART_IER);
}

static int atcuart_fifo_is_full(struct atcuart_port *atport)
{
    return (atport->fifo.fill == atport->fifo.depth);
}

static int atcuart_getc(struct device *dev)
{
    u32 v;

    v = readl(atcuart_addr(dev, OFT_ATCUART_LSR));

    if (v & UART_ATCUART_LSR_ERRF)
        return -EINVAL;

    if (!(v & UART_ATCUART_LSR_DR))
        return -EAGAIN;

    v = readl(atcuart_addr(dev, OFT_ATCUART_RBR));

    return (int) v;
}

static inline int atcuart_tstc(struct device *dev)
{
    u32 v = readl(atcuart_addr(dev, OFT_ATCUART_LSR));

    return (v & UART_ATCUART_LSR_DR);
}

#ifndef abs
#define abs(x) ((x) < 0 ? -(x) : (x))
#endif

    static inline
int atcuart_get_termios(struct serial_port *port, struct termios *termios)
{
    memcpy(termios, &port->oldtermios, sizeof(*termios));

    return 0;
}

    static inline
void atcuart_enable_interrupt(struct device *dev, u32 mask)
{
    u32 v = atcuart_readl(dev, OFT_ATCUART_IER);

    atcuart_writel(v | mask, dev, OFT_ATCUART_IER);
}

static inline u32 atcuart_disable_interrupt(struct device *dev, u32 mask)
{
    u32 v = atcuart_readl(dev, OFT_ATCUART_IER);

    atcuart_writel(v & ~mask, dev, OFT_ATCUART_IER);

    return v;
}

static bool atcuart_tx_stopped(struct device *dev)
{
    u32 v = atcuart_readl(dev, OFT_ATCUART_IER);

    return (v & UART_ATCUART_IER_ETHEI) != UART_ATCUART_IER_ETHEI;
}

int atcuart_port_putc(struct serial_port *port, char c)
{
    struct atcuart_port *atport = to_atcuart_port(port);
    unsigned long flags;

    if (atcuart_fifo_is_full(atport))
        return -EAGAIN;

    local_irq_save(flags);
    atcuart_writel(c, port->device, OFT_ATCUART_THR);
    atport->fifo.fill++;
    if (atcuart_fifo_is_full(atport))
        atcuart_tx_enable(port->device, true);
    local_irq_restore(flags);

    return 0;
}

int atcuart_tx_empty(struct device *dev)
{
#define ATCUART_TX_EMPTY (UART_ATCUART_LSR_TEMT)

    u32 v = atcuart_readl(dev, OFT_ATCUART_LSR);
    return (v & ATCUART_TX_EMPTY) == ATCUART_TX_EMPTY;
}

static int atcuart_tx_fifo_empty(struct device *dev)
{
#define ATCUART_TXFIFO_EMPTY (UART_ATCUART_LSR_THRE)

    u32 v = atcuart_readl(dev, OFT_ATCUART_LSR);
    return (v & ATCUART_TXFIFO_EMPTY) == ATCUART_TXFIFO_EMPTY;
}

static void atcuart_set_baudrate(struct serial_port *port, int baud)
{
    struct atcuart_port *atport = to_atcuart_port(port);
    struct device *dev = port->device;
    struct clk *uartclk = atport->pclk;
    int div;
    int os = atcuart_readl(dev, OFT_ATCUART_OSC) & 0x1f;
    int lc = atcuart_readl(dev, OFT_ATCUART_LCR);
    int freq = clk_get_rate(uartclk);

    div = (clk_get_rate(uartclk) / (baud * os));

    if (abs(div * baud * os - freq) > abs((div + 1) * baud * os - freq))
        div++;

    lc |= UART_ATCUART_LCR_DLAB;
    atcuart_writel(lc, dev, OFT_ATCUART_LCR);

    atcuart_writel((u8)(div >> 8), dev, OFT_ATCUART_DLM);
    atcuart_writel((u8)div, dev, OFT_ATCUART_DLL);

    lc &= ~UART_ATCUART_LCR_DLAB;
    atcuart_writel(lc, dev, OFT_ATCUART_LCR);

    dbg("baud=%d, uartclk=%lu, div=0x%08x\n", baud, clk_get_rate(uartclk),
            div);
}


static int atcuart_set_termios(struct serial_port *port, struct termios *termios)
{
    struct atcuart_port *atport = to_atcuart_port(port);
    struct device *dev = port->device;
    int baud;
    u32 lc = 0, fc = 0, mc, v = 0;

    baud = uart_get_baudrate(termios);
    atcuart_set_baudrate(port, baud);

    switch (termios->c_cflag & CSIZE) {
        case CS5:
            lc |= UART_ATCUART_LCR_WLS_5;
            break;
        case CS6:
            lc |= UART_ATCUART_LCR_WLS_6;
            break;
        case CS7:
            lc |= UART_ATCUART_LCR_WLS_7;
            break;
        default:
            lc |= UART_ATCUART_LCR_WLS_8;
            break;
    }

    if (termios->c_cflag & CSTOPB)
        lc |= UART_ATCUART_LCR_STB;

    if (termios->c_cflag & PARENB) {
        lc &= ~UART_ATCUART_LCR_SPS;
        if (!(termios->c_cflag & PARODD))
            lc |= UART_ATCUART_LCR_EPS;
        else
            lc &= ~UART_ATCUART_LCR_EPS;
        lc |= UART_ATCUART_LCR_PEN;
    }

    /* XXX: what would be the best values for FIFO thresholds ? */
    fc |= (UART_ATCUART_FCR_RFIFOT_0 | UART_ATCUART_FCR_TFIFOT_0);
    fc |= (UART_ATCUART_FCR_FIFOE | UART_ATCUART_FCR_TFIFORST | UART_ATCUART_FCR_RFIFORST);

    mc = atcuart_readl(dev, OFT_ATCUART_MCR);
    if (termios->c_cflag & CRTSCTS) {
        mc |= UART_ATCUART_MCR_AFE;
    } else {
        mc &= ~UART_ATCUART_MCR_AFE;
    }

    atcuart_writel(lc, dev, OFT_ATCUART_LCR);
    atcuart_writel(fc, dev, OFT_ATCUART_FCR);
    atcuart_writel(mc, dev, OFT_ATCUART_MCR);

    fc &= ~(UART_ATCUART_FCR_TFIFORST | UART_ATCUART_FCR_RFIFORST);
    atport->fifo_ctl = fc;

    v = UART_ATCUART_IER_ELSI | UART_ATCUART_IER_ERBI;
    atcuart_writel(v, dev, OFT_ATCUART_IER);

    memcpy(&port->oldtermios, termios, sizeof(*termios));

    return 0;
}

static int atcuart_irq_serial(int irq, void *data)
{
    struct atcuart_port *atport = data;
    struct serial_port *port = &atport->port;
    struct device *dev = port->device;
    struct device *serial = port->device;
    char c;
    portBASE_TYPE resched = false;
    int ret, len;
    u32 intid, lsr;

    intid = atcuart_readl(serial, OFT_ATCUART_IIR);
    intid &= UART_ATCUART_IIR_INTRID;

    if (intid == UART_ATCUART_IIR_INTRID_THR) {
        atport->fifo.fill = 0;

        len = uxQueueMessagesWaitingFromISR(port->queue[TX]);
        if (len == 0) {
            /* No more data to transmit - disable tx interrupt */
            atcuart_tx_enable(serial, false);
            goto txdone;
        }
        while (len > 0 && !atcuart_fifo_is_full(atport)) {
            xQueueReceiveFromISR(port->queue[TX], &c,
                    &resched);
            atcuart_port_putc(port, c);
            len--;
        }
    }
txdone:
    if (intid == UART_ATCUART_IIR_INTRID_RBR ||
            intid == UART_ATCUART_IIR_INTRID_RTI) {
        while ((ret = atcuart_getc(serial)) >= 0) {
            c = (char) (ret & 0xff);
            xQueueSendFromISR(port->queue[RX], &c, &resched);
        }
    }
    if (intid == UART_ATCUART_IIR_INTRID_LSR) {
        lsr = atcuart_readl(serial, OFT_ATCUART_LSR);
        lsr &= UART_ATCUART_LSR_RXERR;
        atport->rx_err += lsr? 1 : 0;
        /* err("%s: rx error %08x\n", dev_name(serial), lsr); */
        /* reset rx fifo for recovery */
        atcuart_writel(atport->fifo_ctl | UART_ATCUART_FCR_RFIFORST, dev, OFT_ATCUART_FCR);
    }

    portYIELD_FROM_ISR(resched);

    return 0;
}

static int atcuart_irq_raw(int irq, void *data)
{
    struct atcuart_port *atport = data;
    struct serial_port *port = &atport->port;
    struct device *dev = port->device;
    struct raw_access *raw= &atport->raw;
    uint32_t intid, lsr;
    uint32_t i;
    int ret;

    intid = atcuart_readl(dev, OFT_ATCUART_IIR);
    intid &= UART_ATCUART_IIR_INTRID;

    if (intid == UART_ATCUART_IIR_INTRID_THR) {
        atport->fifo.fill = 0;

        if (raw->tr.tx_oft == raw->tr.tx_len) {
            atcuart_tx_enable(dev, false);

#ifdef FULL_DUPLEX_DEBUG
            printk("T%d-", atport->port.id);
#endif

            if (raw->cb) {
                raw->event.type = UART_EVENT_TX_CMPL;
                raw->event.err = UART_ERR_NO;
                raw->cb(&raw->event, raw->cb_ctx);
            }
        } else {
            for (i = raw->tr.tx_oft; i < raw->tr.tx_len; i++) {
                atcuart_writel(raw->tr.tx_buf[i], dev, OFT_ATCUART_THR);
                atport->fifo.fill++;
                raw->tr.tx_oft++;
                if (atcuart_fifo_is_full(atport)) {
                    break;
                }
            }
        }
    }

    if (intid == UART_ATCUART_IIR_INTRID_RBR) {
        for (i = raw->tr.rx_oft; i < raw->tr.rx_len; i++) {
            ret = atcuart_getc(dev);
            if (ret < 0) {
                break;
            }
            raw->tr.rx_buf[i] = ret & 0xff;
            raw->tr.rx_oft++;
        }

        if (raw->tr.rx_oft == raw->tr.rx_len) {
            atcuart_rx_enable(dev, false);

#ifdef FULL_DUPLEX_DEBUG
            printk("R%d-", atport->port.id);
#endif

            if (raw->cb) {
                raw->event.type = UART_EVENT_RX_CMPL;
                raw->event.err = UART_ERR_NO;
                raw->cb(&raw->event, raw->cb_ctx);
            }
        }
    }

    if (intid == UART_ATCUART_IIR_INTRID_RTI) {
        while ((ret = atcuart_getc(dev)) >= 0) {
        }
    }

    if (intid == UART_ATCUART_IIR_INTRID_LSR) {
        lsr = atcuart_readl(dev, OFT_ATCUART_LSR);
        lsr &= UART_ATCUART_LSR_RXERR;

        if (atport->raw.enable && atport->raw.dma_en) {
            dma_ch_abort(atport->raw.dma_dev, atport->raw.rx_dma_ctx.dma_ch);
            atport->raw.rx_dma_ctx.dma_ch = -1;
        }

        atport->rx_err += lsr? 1 : 0;

        atcuart_rx_enable(dev, false);

        atcuart_writel(atport->fifo_ctl | UART_ATCUART_FCR_RFIFORST, dev, OFT_ATCUART_FCR);

        if (raw->cb) {
            raw->event.type = UART_EVENT_RX_CMPL;
            raw->event.err = UART_ERR_NO;

            if (lsr & UART_ATCUART_LSR_LBRE) {
                raw->event.err |= UART_ERR_LINE_BREAK;
            }
            if (lsr & UART_ATCUART_LSR_FE) {
                raw->event.err |= UART_ERR_FRAMING;
            }
            if (lsr & UART_ATCUART_LSR_PE) {
                raw->event.err |= UART_ERR_PARITY;
            }
            if (lsr & UART_ATCUART_LSR_OE) {
                raw->event.err |= UART_ERR_OVERRUN;
            }

            raw->cb(&raw->event, raw->cb_ctx);
        }
    }

    return 0;
}

static int atcuart_irq(int irq, void *data)
{
    struct atcuart_port *atport = data;

    if (atport->raw.enable) {
        atcuart_irq_raw(irq, data);
    } else {
        atcuart_irq_serial(irq, data);
    }

    return 0;
}


__iram__ int atcuart_port_init(struct serial_port *port)
{
    struct atcuart_port *atport = to_atcuart_port(port);
    int i, ret = 0;

    /* Enable the clocks */

    clk_enable(atport->pclk, 1);

    /* Configure interface */

    for (i = 0; i < ARRAY_SIZE(atport->pmap); i++) {
        if ((atport->pmap[i]) &&
                ((ret = pinctrl_request_pin(port->device,
                                            atport->pmap[i]->id,
                                            atport->pmap[i]->pin)) != 0)) {
            err("failed to claim pin %d\n", atport->pmap[i]->pin);
            goto free_pin;
        }
    }

    if ((atport->pmap[CTS] && atport->pmap[CTS]->pin != -1) ||
            (atport->pmap[RTS] && atport->pmap[RTS]->pin != -1)) {
        port->oldtermios.c_cflag |= CRTSCTS;
    }
    atcuart_set_termios(port, &port->oldtermios);

    /* Request interrupts */

    ret = request_irq(port->device->irq[0], atcuart_irq,
            dev_name(port->device), port->device->pri[0], atport);
    if (ret)
        goto free_pin;
    return 0;

free_pin:
    for (; i >= 0; i--) {
        if (atport->pmap[i] && atport->pmap[i]->pin != -1)
            pinctrl_free_pin(port->device, atport->pmap[i]->id,
                    atport->pmap[i]->pin);
    }
    return ret;
}

static void atcuart_console_putc(struct serial_port *port, char c)
{
    while (!atcuart_tx_fifo_empty(port->device));
    atcuart_writel(c, port->device, OFT_ATCUART_THR);
}

int atcuart_console_write(struct serial_port *port, char *s, int n)
{
    int i;

    for (i = 0; i < n; i++, s++) {
        if (*s == '\n') {
            atcuart_console_putc(port, '\r');
            /*continue;*/
        }
        atcuart_console_putc(port, *s);
    }
    return i;
}

void atcuart_port_start_tx(struct serial_port *port)
{
    struct atcuart_port *atport = to_atcuart_port(port);
    int len;
    unsigned long flags;
    char ch;

    local_irq_save(flags);

    if (atcuart_tx_stopped(port->device)) {
        len = uxQueueMessagesWaitingFromISR(port->queue[TX]);
        while (len-- > 0 && !atcuart_fifo_is_full(atport)) {
            if (xQueueReceiveFromISR(port->queue[TX], &ch, 0) == pdPASS) {
                atcuart_port_putc(port, ch);
            }
        }
    }

    local_irq_restore(flags);
}

int atcuart_port_ioctl(struct serial_port *port, unsigned cmd, void *arg)
{
    struct device *dev = port->device;
    struct termios *termios;
    struct uart_init_arg *init_arg;
    struct uart_tx_arg *tx_arg;
    struct uart_rx_arg *rx_arg;
    struct uart_get_rx_len_arg *rx_len_arg;

    switch (cmd) {
        case TCSETS:
            {
                int ret;
                unsigned long flags;
                termios = arg;
                local_irq_save(flags);
                ret = atcuart_set_termios(port, termios);
                local_irq_restore(flags);
                return ret;
            }
        case IOCTL_UART_INIT:
            init_arg = arg;
            return uart_init(dev, init_arg->cfg, init_arg->cb, init_arg->cb_ctx);
        case IOCTL_UART_DEINIT:
            return uart_deinit(dev);
        case IOCTL_UART_TX:
            tx_arg = arg;
            return uart_tx(dev, tx_arg->tx_buf, tx_arg->tx_len);
        case IOCTL_UART_RX:
            rx_arg = arg;
            return uart_rx(dev, rx_arg->rx_buf, rx_arg->rx_len);
        case IOCTL_UART_RESET:
            return uart_reset(dev);
        case IOCTL_UART_GET_RX_LEN:
            rx_len_arg = arg;
            *(rx_len_arg->len) = uart_get_rx_len(dev);
            return 0;
        default:
            return -ENOTTY;
    }
}

static void actuart_raw_errata(struct device *dev)
{
    struct atcuart_port *atport = dev->driver_data;
    struct raw_access *raw = &atport->raw;
    uint8_t dummy;

    /* If deinitailize PIO mode and initialize DMA mode,
     * 1bytes garbage data occurs during RX.
     * If you start DMA 1 byte read and immediately abort,
     * 1 byte trash can be removed.
     * Starting in DMA mode without deinitialize PIO mode and
     * applying this routine has no effect on operation.
     */

    raw->rx_dma_ctx.desc.dst_addr = (uint32_t)&dummy;
    raw->rx_dma_ctx.desc.len = 1;
    dma_copy_hw(raw->dma_dev, false, &raw->rx_dma_ctx.ctrl,
            &raw->rx_dma_ctx.desc, 1, NULL, NULL, &raw->rx_dma_ctx.dma_ch);
    dma_ch_abort(raw->dma_dev, raw->rx_dma_ctx.dma_ch);
    raw->rx_dma_ctx.dma_ch = -1;

    /* Same applies to Tx. */
    raw->tx_dma_ctx.desc.src_addr = (uint32_t)&dummy;
    raw->tx_dma_ctx.desc.len = 1;
    dma_copy_hw(raw->dma_dev, false, &raw->tx_dma_ctx.ctrl,
            &raw->tx_dma_ctx.desc, 1, NULL, NULL, &raw->tx_dma_ctx.dma_ch);
    dma_ch_abort(raw->dma_dev, raw->tx_dma_ctx.dma_ch);
    raw->tx_dma_ctx.dma_ch = -1;
}

static int atcuart_raw_init(struct device *dev, struct uart_cfg *cfg, uart_cb cb, void *ctx)
{
    struct atcuart_port *atport = dev->driver_data;
    struct serial_port *port;
    struct raw_access *raw = &atport->raw;
    struct termios termios;
    struct device *dma_dev = NULL;
    uint8_t did = dev_id(dev);

    if (atport->port.id == CONFIG_SERIAL_CONSOLE_PORT) {
        printk("already use console port\n");
        return -EBUSY;
    }

    if (raw->enable) {
        printk("already initialized\n");
        return -EBUSY;
    }

    if (cfg->dma_en) {
        if (did == 0) {
            dma_dev = device_get_by_name("dmac.0");
        } else if (did == 1) {
            dma_dev = device_get_by_name("dmac.0");
        } else if (did == 2) {
            dma_dev = device_get_by_name("dmac.1");
        }

        if (!dma_dev) {
            printk("DMA is not enabled\n");
            return -EINVAL;
        }
    }

    port = &atport->port;
    if (!port->initialized) {
        int i, ret = 0;

        for (i = 0; i < 2; i++) {
            port->queue[i] = xQueueCreate(CONFIG_SERIAL_BUFFER_SIZE, sizeof(char));
            if (port->queue[i] == NULL) {
                ret = -ENOMEM;
            }
        }

        if (!ret) {
            if (port->ops->init) {
                ret = port->ops->init(port);
            }
        }

        if (!ret) {
            port->initialized = 1;
        }

        if (ret) {
            for (i = 0; i < 2; i++) {
                if (port->queue[i]) {
                    vQueueDelete(port->queue[i]);
                }
            }

            return ret;
        }

    }

    memset(raw, 0, sizeof(struct raw_access));
    memset(&termios, 0, sizeof(termios));

    termios.c_ispeed = cfg->baudrate;

    switch (cfg->parity) {
        case UART_ODD_PARITY:
            termios.c_cflag |= PARODD;
            break;
        case UART_EVENT_PARITY:
            termios.c_cflag |= PARENB;
            break;
        case UART_NO_PARITY:
        default:
            termios.c_cflag &= ~PARENB;
            break;
    }

    if (cfg->stop_bits == UART_STOP_BIT_2) {
        termios.c_cflag |= CSTOPB;
    } else {
        termios.c_cflag &= ~CSTOPB;
    }


    switch (cfg->data_bits) {
        case UART_DATA_BITS_5:
            termios.c_cflag |= CS5;
            break;
        case UART_DATA_BITS_6:
            termios.c_cflag |= CS6;
            break;
        case UART_DATA_BITS_7:
            termios.c_cflag |= CS7;
            break;
        case UART_DATA_BITS_8:
            termios.c_cflag |= CS8;
            break;
    }

    atcuart_set_termios(&atport->port, &termios);

    raw->cb = cb;
    raw->cb_ctx = ctx;
    raw->dma_en = cfg->dma_en;

    if (raw->dma_en) {
        raw->dma_dev = dma_dev;
        if (did == 0) {
            raw->tx_dma_ctx.ctrl.dst_req = DMA0_HW_REQ_UART0_TX;
            raw->rx_dma_ctx.ctrl.src_req = DMA0_HW_REQ_UART0_RX;
        } else if (did == 1) {
            raw->tx_dma_ctx.ctrl.dst_req = DMA0_HW_REQ_UART1_TX;
            raw->rx_dma_ctx.ctrl.src_req = DMA0_HW_REQ_UART1_RX;
        } else if (did == 2) {
            raw->tx_dma_ctx.ctrl.dst_req = DMA1_HW_REQ_UART2_TX;
            raw->rx_dma_ctx.ctrl.src_req = DMA1_HW_REQ_UART2_RX;
        }

        if (raw->dma_dev) {
            raw->tx_dma_ctx.is_tx = 1;
            raw->tx_dma_ctx.dma_ch = -1;
            raw->tx_dma_ctx.dev = dev;
            raw->tx_dma_ctx.ctrl.src_mode = DMA_MODE_NORMAL;
            raw->tx_dma_ctx.ctrl.dst_mode = DMA_MODE_HANDSHAKE;
            raw->tx_dma_ctx.ctrl.src_req = 0;
            raw->tx_dma_ctx.ctrl.src_addr_ctrl = DMA_ADDR_CTRL_INCREMENT;
            raw->tx_dma_ctx.ctrl.dst_addr_ctrl = DMA_ADDR_CTRL_FIXED;
            raw->tx_dma_ctx.ctrl.src_width = DMA_WIDTH_BYTE;
            raw->tx_dma_ctx.ctrl.dst_width = DMA_WIDTH_BYTE;
            raw->tx_dma_ctx.ctrl.intr_mask = DMA_INTR_ABT_MASK;
            raw->tx_dma_ctx.ctrl.src_burst_size = DMA_SRC_BURST_SIZE_1;
            raw->tx_dma_ctx.desc.dst_addr = (uint32_t)(dev->base[0] + OFT_ATCUART_THR);

            raw->rx_dma_ctx.is_tx = 0;
            raw->rx_dma_ctx.dma_ch = -1;
            raw->rx_dma_ctx.dev = dev;
            raw->rx_dma_ctx.ctrl.src_mode = DMA_MODE_HANDSHAKE;
            raw->rx_dma_ctx.ctrl.dst_mode = DMA_MODE_NORMAL;
            raw->rx_dma_ctx.ctrl.dst_req = 0;
            raw->rx_dma_ctx.ctrl.src_addr_ctrl = DMA_ADDR_CTRL_FIXED;
            raw->rx_dma_ctx.ctrl.dst_addr_ctrl = DMA_ADDR_CTRL_INCREMENT;
            raw->rx_dma_ctx.ctrl.src_width = DMA_WIDTH_BYTE;
            raw->rx_dma_ctx.ctrl.dst_width = DMA_WIDTH_BYTE;
            raw->rx_dma_ctx.ctrl.intr_mask = DMA_INTR_ABT_MASK;
            raw->tx_dma_ctx.ctrl.src_burst_size = DMA_SRC_BURST_SIZE_1;
            raw->rx_dma_ctx.desc.src_addr = (uint32_t)(dev->base[0] + OFT_ATCUART_RBR);

            atport->fifo_ctl |= UART_ATCUART_FCR_DMAE |
                UART_ATCUART_FCR_TFIFOT_1 |
                UART_ATCUART_FCR_RFIFOT_1;
            atcuart_writel(atport->fifo_ctl, dev, OFT_ATCUART_FCR);

            actuart_raw_errata(dev);
        } else {
            raw->dma_en = 0;
            atport->fifo_ctl &= ~(UART_ATCUART_FCR_DMAE |
                UART_ATCUART_FCR_TFIFOT_1 |
                UART_ATCUART_FCR_RFIFOT_1);
            atcuart_writel(atport->fifo_ctl, dev, OFT_ATCUART_FCR);
        }
    }

    atcuart_writel(UART_ATCUART_IER_ELSI, dev, OFT_ATCUART_IER);

    printk("UART%d raw access enabled (%s mode)\n", dev_id(dev), raw->dma_en ? "DMA" : "PIO");

    raw->enable = 1;

    return 0;
}

static int atcuart_raw_deinit(struct device *dev)
{
    struct atcuart_port *atport = dev->driver_data;
    struct raw_access *raw = &atport->raw;

    uart_reset(dev);

    printk("UART%d raw access disabled\n", dev_id(dev));

    raw->enable = 0;

    return 0;
}

static int atcuart_dma_done_handler(void *ctx, dma_isr_status status)
{
    struct dma_ctx *dma_ctx = ctx;
    struct device *dev = dma_ctx->dev;
    struct atcuart_port *atport = dev->driver_data;
    struct raw_access *raw = &atport->raw;

    if ((status == DMA_STATUS_NONE)
            || (status == DMA_STATUS_ABORTED)
            || (status == DMA_STATUS_ERROR))
        return -1;

    if (raw->dma_en) {
        if (dma_ctx->is_tx) {
            raw->tx_dma_ctx.dma_ch = -1;
        } else {
            raw->rx_dma_ctx.dma_ch = -1;
        }
    }

    if (raw->cb) {
        if (dma_ctx->is_tx) {
            raw->event.type = UART_EVENT_TX_CMPL;
        } else {
            raw->event.type = UART_EVENT_RX_CMPL;
        }

        raw->event.err = UART_ERR_NO;

#ifdef FULL_DUPLEX_DEBUG
        printk("%c%d-", dma_ctx->is_tx ? 't' : 'r', atport->port.id);
#endif
        raw->cb(&raw->event, raw->cb_ctx);
    }

    return 0;
}

static int atcuart_dma_configure(struct device *dev, uint8_t *buf, uint32_t len, uint8_t is_tx)
{
    struct atcuart_port *atport = dev->driver_data;
    struct raw_access *raw = &atport->raw;
    int ret;

    if (!raw->dma_dev) {
        return -EIO;
    }

    if (!((uint32_t)buf & 0xF0000000)) {
        printk("invalid buffer address for DMA %p\n", buf);
        return -EINVAL;
    }

    if (is_tx) {
        raw->tx_dma_ctx.desc.src_addr = (uint32_t)buf;
        raw->tx_dma_ctx.desc.len = len;
        ret = dma_copy_hw(raw->dma_dev, false, &raw->tx_dma_ctx.ctrl,
                &raw->tx_dma_ctx.desc, 1,
                atcuart_dma_done_handler,
                &raw->tx_dma_ctx, &raw->tx_dma_ctx.dma_ch);
        if (ret) {
            return ret;
        }
    } else {
        raw->rx_dma_ctx.desc.dst_addr = (uint32_t)buf;
        raw->rx_dma_ctx.desc.len = len;
        ret = dma_copy_hw(raw->dma_dev, false, &raw->rx_dma_ctx.ctrl,
                &raw->rx_dma_ctx.desc, 1,
                atcuart_dma_done_handler,
                &raw->rx_dma_ctx, &raw->rx_dma_ctx.dma_ch);
        if (ret) {
            return ret;
        }
    }

    return 0;
}

static int atcuart_transmit(struct device *dev, uint8_t *tx_buf, uint32_t tx_len)
{
    struct atcuart_port *atport = dev->driver_data;
    struct raw_access *raw = &atport->raw;
    uint32_t flags;

    if (!raw->enable) {
        return -EPERM;
    }

    if (raw->dma_en) {
        int ret;

#ifdef FULL_DUPLEX_DEBUG
        printk("t%d+", atport->port.id);
#endif

        if (raw->tx_dma_ctx.dma_ch >= 0) {
            return -EINPROGRESS;
        }

        ret =  atcuart_dma_configure(dev, tx_buf, tx_len, 1);
        if (ret) {
            return ret;
        }
    } else {
        uint32_t i;
        uint8_t tx_isr_enable = 0;

        if (raw->tr.tx_len != raw->tr.tx_oft) {
            return -EINPROGRESS;
        }

        raw->tr.tx_buf = tx_buf;
        raw->tr.tx_len = tx_len;
        raw->tr.tx_oft = 0;

#ifdef FULL_DUPLEX_DEBUG
        printk("T%d+", atport->port.id);
#endif

        local_irq_save(flags);

        for (i = 0; i < raw->tr.tx_len; i++) {
            atcuart_writel(raw->tr.tx_buf[i], dev, OFT_ATCUART_THR);
            atport->fifo.fill++;
            raw->tr.tx_oft++;
            if (atcuart_fifo_is_full(atport)) {
                atcuart_tx_enable(dev, true);
                tx_isr_enable = 1;
                break;
            }
        }


        if (!tx_isr_enable) {
            atcuart_tx_enable(dev, true);
        }

        local_irq_restore(flags);

    }

    return 0;
}

static int atcuart_recevie(struct device *dev, uint8_t *rx_buf, uint32_t rx_len)
{
    struct atcuart_port *atport = dev->driver_data;
    struct raw_access *raw = &atport->raw;

    if (!raw->enable) {
        return -EPERM;
    }

    if (raw->dma_en) {
        int ret;

#ifdef FULL_DUPLEX_DEBUG
        printk("r%d+", atport->port.id);
#endif

        if (raw->rx_dma_ctx.dma_ch >= 0) {
            return -EINPROGRESS;
        }
        ret =  atcuart_dma_configure(dev, rx_buf, rx_len, 0);

        if (ret) {
            return ret;
        }

        raw->tr.rx_buf = rx_buf;
        raw->tr.rx_len = rx_len;
        raw->tr.rx_oft = 0;

    } else {

#ifdef FULL_DUPLEX_DEBUG
        printk("R%d+", atport->port.id);
#endif

        if (raw->tr.rx_len != raw->tr.rx_oft) {
            return -EINPROGRESS;
        }
        raw->tr.rx_buf = rx_buf;
        raw->tr.rx_len = rx_len;
        raw->tr.rx_oft = 0;

        atcuart_rx_enable(dev, true);
    }

    return 0;
}

static int atcuart_reset(struct device *dev)
{
    struct atcuart_port *atport = dev->driver_data;
    struct raw_access *raw = &atport->raw;
    uint32_t fc;
    int ret;

    if (!raw->enable) {
        return -EPERM;
    }

    atcuart_rx_enable(dev, false);
    atcuart_tx_enable(dev, false);

    memset(&raw->tr, 0, sizeof(struct uart_tr));

    if (raw->dma_en) {
        if (raw->tx_dma_ctx.dma_ch >= 0) {
            dma_ch_abort(raw->dma_dev, raw->tx_dma_ctx.dma_ch);
            raw->tx_dma_ctx.dma_ch = -1;
        }

        if (raw->rx_dma_ctx.dma_ch >= 0) {
            dma_ch_abort(raw->dma_dev, raw->rx_dma_ctx.dma_ch);
            raw->rx_dma_ctx.dma_ch = -1;
        }
    }

    while ((ret = atcuart_getc(dev)) >= 0) {
    }

    fc = (UART_ATCUART_FCR_TFIFORST | UART_ATCUART_FCR_RFIFORST);
    fc |= atport->fifo_ctl;

    atcuart_writel(fc, dev, OFT_ATCUART_FCR);

    return 0;
}

static int atcuart_get_rx_len(struct device *dev)
{
    struct atcuart_port *atport = dev->driver_data;
    struct raw_access *raw = &atport->raw;
    int len;

    if (!raw->enable) {
        return -EPERM;
    }

    if (raw->dma_en) {
        int yet_to_go = dma_ch_get_trans_size(raw->dma_dev, raw->rx_dma_ctx.dma_ch);
        len = raw->tr.rx_len - yet_to_go;
    } else {
        len = raw->tr.rx_oft;
    }

    return len;
}

__iram__ int atcuart_probe(struct device *dev)
{
    struct atcuart_port *atport;
    struct serial_port *port;
    int v;

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

    /* pinctrl */
    atport->pmap[TXD] = pinctrl_lookup_platform_pinmap(dev, "txd");
    atport->pmap[RXD] = pinctrl_lookup_platform_pinmap(dev, "rxd");
    atport->pmap[CTS] = pinctrl_lookup_platform_pinmap(dev, "cts");
    atport->pmap[RTS] = pinctrl_lookup_platform_pinmap(dev, "rts");
    if (atport->pmap[CTS] && atport->pmap[CTS]->pin != -1 &&
            atport->pmap[RTS] && atport->pmap[RTS]->pin != -1)
        port->oldtermios.c_cflag |= CRTSCTS;

    /* FIFO */
    v = atcuart_readl(dev, OFT_ATCUART_HWC) & 0x3;
    atport->fifo.depth = 1 << (4 + v);
    atport->fifo.fill = 0;

    dbg("AndesShape(tm) ATCUART100 %s@%p: irq=%2d clk=%d\n", dev_name(dev), dev->base[0],
            dev->irq[0], clk_get_rate(atport->pclk));

    dev->driver_data = atport;

    port->device = dev;

    /* assign for future patch function */

    atcuart_port_ops.init = atcuart_port_init;
    atcuart_port_ops.start_tx = atcuart_port_start_tx;
    atcuart_port_ops.ioctl = atcuart_port_ioctl;
    atcuart_port_ops.con_write = atcuart_console_write;

    port->ops = &atcuart_port_ops;
    port->id = dev_id(dev);

    return register_serial_port(port);
}

__iram__ int atcuart_shutdown(struct device *dev)
{
    struct atcuart_port *atport = dev->driver_data;
    struct serial_port *port = &atport->port;
    int i;

    /* XXX: need to unregister serial console? */

    /* Free interrupts */

    free_irq(dev->irq[0], dev_name(dev));

    /* Disable FIFO and return pins */

    atcuart_writel(UART_ATCUART_FCR_TFIFORST, dev, OFT_ATCUART_FCR);
    atcuart_writel(0, dev, OFT_ATCUART_FCR);

    for (i = 0; i < ARRAY_SIZE(atport->pmap); i++) {
        if (atport->pmap[i]) {
            pinctrl_free_pin(port->device, atport->pmap[i]->id,
                    atport->pmap[i]->pin);
        }
    }

    /* Disable the clock */

    clk_enable(atport->pclk, 0);

    return 0;
}

#ifdef CONFIG_PM_DM
__iram__ int atcuart_suspend(struct device *dev, u32 *idle)
{
    struct atcuart_port *atport = dev->driver_data;
    struct serial_port *port = &atport->port;

    if (!port->initialized)
        return 0;

#ifdef CONFIG_SOC_SCM2010
    atport->ctx.osc = atcuart_readl(dev, OFT_ATCUART_OSC);
    atport->ctx.ier = atcuart_readl(dev, OFT_ATCUART_IER);
    atport->ctx.lcr = atcuart_readl(dev, OFT_ATCUART_LCR);
    atport->ctx.mcr = atcuart_readl(dev, OFT_ATCUART_MCR);

    /* DLAB = 1 to access DLL DLM */
    atcuart_writel(atport->ctx.lcr | 0x80, dev, OFT_ATCUART_LCR);

    atport->ctx.dll = atcuart_readl(dev, OFT_ATCUART_DLL);
    atport->ctx.dlm = atcuart_readl(dev, OFT_ATCUART_DLM);

    /* DLAB = 0 to access DLL DLM */
    atcuart_writel(atport->ctx.lcr & ~0x80, dev, OFT_ATCUART_LCR);

    return 0;
#else
    u32 busy = 0;

    busy |= uxQueueMessagesWaitingFromISR(port->queue[TX]) ? 1: 0;
    busy |= uxQueueMessagesWaitingFromISR(port->queue[RX]) ? 1 << 1: 0;
    busy |= atcuart_tx_empty(dev) ? 0 : 1 << 2;
    busy |= (readl(atcuart_addr(dev, OFT_ATCUART_LSR)) &
            UART_ATCUART_LSR_DR) ? 1 << 3: 0;
    return busy? -EBUSY: 0;
#endif
}

__iram__ int atcuart_resume(struct device *dev)
{
    struct atcuart_port *atport = dev->driver_data;
    struct serial_port *port = &atport->port;
#ifdef CONFIG_SOC_SCM2010
    uint32_t fc;
#endif

    if (!port->initialized)
        return 0;

#ifdef CONFIG_SOC_SCM2010
    fc = atport->fifo_ctl;
    fc |= UART_ATCUART_FCR_TFIFORST | UART_ATCUART_FCR_RFIFORST;
    atcuart_writel(atport->ctx.osc, dev, OFT_ATCUART_OSC);
    atcuart_writel(fc, dev, OFT_ATCUART_FCR);
    atcuart_writel(atport->ctx.lcr, dev, OFT_ATCUART_LCR);
    atcuart_writel(atport->ctx.mcr, dev, OFT_ATCUART_MCR);

    /* DLAB = 1 to access DLL DLM */
    atcuart_writel(atport->ctx.lcr | 0x80, dev, OFT_ATCUART_LCR);

    atcuart_writel(atport->ctx.dll, dev, OFT_ATCUART_DLL);
    atcuart_writel(atport->ctx.dlm, dev, OFT_ATCUART_DLM);

    /* DLAB = 0 to access DLL DLM */
    atcuart_writel(atport->ctx.lcr & ~0x80, dev, OFT_ATCUART_LCR);

    /* enable interrupt */
    atcuart_writel(atport->ctx.ier, dev, OFT_ATCUART_IER);
#endif

    return 0;
}
#endif

struct uart_ops uart_ops = {
    .init 		    = atcuart_raw_init,
    .deinit 	    = atcuart_raw_deinit,
    .transmit	    = atcuart_transmit,
    .receive	    = atcuart_recevie,
    .reset 		    = atcuart_reset,
    .get_rx_len 	= atcuart_get_rx_len,
};

/* important as static and all of functions should be located in here */
static declare_driver(atcuart) = {
    .name		= "atcuart",
    .probe		= atcuart_probe,
    .shutdown	= atcuart_shutdown,
#ifdef CONFIG_PM_DM
    .suspend	= atcuart_suspend,
    .resume		= atcuart_resume,
#endif
    .ops		= &uart_ops,
};

#ifdef CONFIG_SOC_SCM2010
#if !defined(CONFIG_USE_UART0) && !defined(CONFIG_USE_UART1) && !defined(CONFIG_USE_UART2)
#error UART driver requires UART devices. Select UART devices or remove the driver
#endif
#endif

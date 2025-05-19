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
 *
 */

/**
 * AndeShape ATCSPI200 SPI flash controller driver
 *
 * NOTE:
 * - Only 3-byte address is supported
 * - SPI mode only (no dual- or quad-mode)
 * - PIO only (no DMA)
 *
 */

#include <string.h>

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/kmem.h>
#include <hal/pinctrl.h>
#include <hal/spi-flash.h>
#include <hal/timer.h>
#include <hal/unaligned.h>
#include <hal/clk.h>

#include "spi-flash-internal.h"

/*#define DEBUG*/

#ifdef DEBUG
#define dbg(arg ...) printk(arg);
#else
static void dbg(const char *fmt, ...)
{
	return;
}
#endif
#define err(arg ...) printk(arg);

#define bfield_get_w(v, pos, width) (((v) >> (pos)) & ((1 << (width)) - 1))
#define bfield_get(v, end, start) bfield_get_w(v, start, (end-start+1))

#define bfield_set_w(v, pos, width) (((v) & ((1 << (width)) - 1)) << (pos))
#define bfield_set(v, end, start) bfield_set_w(v, start, (end-start+1))

/* AndeShape ATCSPI200 registers */
#define IdRev 		0x0
#define TransFmt 	0x10
#define DirectIO	0x14
#define TransCtrl	0x20
#define Cmd		    0x24
#define Addr		0x28
#define Data		0x2c
#define Ctrl		0x30
#define Status		0x34
#define IntrEn		0x38
#define IntrSt		0x3c
#define Timing		0x40
#define MemCtrl		0x50
#define Config		0x7c

#if defined(CONFIG_SOC_SCM2010) || defined(CONFIG_SOC_TL7118)
struct atcspi200_ctx {
	u32 trans_fmt;
	u32 timing;
	u32 mem_ctrl;
};
#endif

struct atcspi200_driver_data {
	struct pinctrl_pin_map *pmap[6];
	int cpol;
	int cpha;
	int delay;
	int sclk;
#ifdef CONFIG_PM_DM
	struct atcspi200_ctx ctx;
#endif
};

enum {
	ATCSPI200_CS 	= 0,
	ATCSPI200_CLK 	= 1,
	ATCSPI200_MOSI 	= 2,
	ATCSPI200_MISO 	= 3,
	ATCSPI200_HOLD 	= 4,
	ATCSPI200_WP 	= 5
};

static const char *pins[] = { "cs", "clk", "mosi", "miso", "hold", "wp"};

/* Refer to Table 17. from AndeShape_ATCSPI200_DS087_V1.8.pdf. */
/* 4-bytes address mode? */
static const struct fast_read_cmd atcspi200_readcmds[6] = {
    /* Read (1-1-1) */
    [0] = {
        .opcode  = 0x03,
        .address = 1,
        .data    = 1,
        .mode    = 0,
        .dummy   = 0,
    },
    /* Fast Read (1-1-1) */
    [1] = {
        .opcode  = 0x0b,
        .address = 1,
        .data    = 1,
        .mode    = 0,
        .dummy   = 8,
    },
    /* Fast Read Dual Output (1-1-2) */
    [2] = {
        .opcode  = 0x3b,
        .address = 1,
        .data    = 2,
        .mode    = 0,
        .dummy   = 8,
    },
    /* Fast Read Quad Output (1-1-4) */
    [3] = {
        .opcode  = 0x6b,
        .address = 1,
        .data    = 4,
        .mode    = 0,
        .dummy   = 8,
    },
    /* Fast Read Dual I/O (1-2-2) */
    [4] = {
        .opcode  = 0xbb,
        .address = 2,
        .data    = 2,
        .mode    = 4,
        .dummy   = 0,
    },
    /* Fast Read Quad I/O (1-4-4) */
    [5] = {
        .opcode  = 0xeb,
        .address = 4,
        .data    = 4,
        .mode    = 2,
        .dummy   = 4,
    },
};

static inline u32 atcspi200_readl(struct device *dev, u32 oft)
{
	return readl(dev->base[0] + oft);
}

static inline void atcspi200_writel(u32 val, struct device *dev, u32 oft)
{
	writel(val, dev->base[0] + oft);
}

static u32 _get_addr(const u8 *cmd)
{
    u32 addr = 0;

    addr |= cmd[1] << 16;
    addr |= cmd[2] << 8;
    addr |= cmd[3];

    return addr;
}

#ifndef CONFIG_ATCSPI200_COMPACT
static void atcspi200_clear_fifo(struct device *dev)
{
	u32 v;

	atcspi200_writel(0x6, dev, Ctrl);

	do {
		v = atcspi200_readl(dev, Ctrl);
	} while (bfield_get(v, 2, 1) != 0);
}

static void atcspi200_wait_idle(struct device *dev)
{
	u32 v;

	do {
		v = atcspi200_readl(dev, Status);
	} while (bfield_get(v, 0, 0) == 0x1);
}

static void atcspi200_wait_txfifo(struct device *dev)
{
	u32 v;

	do {
		v = atcspi200_readl(dev, Status);
	} while (bfield_get(v, 23, 23) == 0x1);
}

static void atcspi200_wait_rxfifo(struct device *dev)
{
	u32 v;

	do {
		v = atcspi200_readl(dev, Status);
	} while (bfield_get(v, 14, 14) == 0x1);
}
#endif

static
u32 atcspi200_transfer_control(size_t cmdlen, size_t dummylen,
                               size_t txlen, size_t rxlen)
{
    u8 addrEn = 0, dummycnt = 0;
	u32 mode = 7, v = 0;

	assert(cmdlen + txlen + rxlen > 0);
	assert(txlen <= 0x200);
	assert(rxlen <= 0x200);

    if (cmdlen >= 4) {
        /* means: 1 byte instruction | 3(4) bytes address */
        addrEn = 1;
    }

	if (txlen > 0)
        mode = rxlen > 0 ? 0x3: 0x1; /* write-read or write-only */
	else if (rxlen > 0)
		mode = 0x2; /* read-only */

    /* Consider dummy explicitly. */
    if (dummylen > 0) {
        /* Convert to 'plus dummy' mode. */
        switch (mode) {
        case 0x2:
            /* read only -> dummy + read */
            mode = 0x9;
            break;
        case 0x3:
            /* write + read -> write + dummy + read */
            mode = 0x5;
            break;
        case 0x1:
            /* write only -> dummy+ write */
            mode = 0x8;
            break;
        default:
            /* what to do? */
            break;
        }
        dummycnt = dummylen - 1;
    }

    v |= bfield_set(1, 30, 30);      /* CmdEn */
    v |= bfield_set(addrEn, 29, 29); /* AddrEn */
	v |= bfield_set(mode, 27, 24);   /* TransMode */
    v |= bfield_set(dummycnt, 10, 9);/* DummyCnt */
	v |= txlen > 0 ? bfield_set(txlen - 1, 20, 12) : 0; /* WrTranCnt */
	v |= rxlen > 0 ? bfield_set(rxlen - 1, 8, 0) : 0;   /* RdTranCnt */

	return v;
}

static
u32 atcspi200_mio_transfer_control(struct fast_read_cmd *cmd,
                                   size_t rxlen)
{
    u8 dummycnt = 0, dualquad = 0, addrfmt = 0, tokenen = 0;
	u32 mode = 0, v = 0;

	assert(cmd);
	assert(cmd->opcode);
    assert(rxlen);
	assert(rxlen <= 0x200);

    if (cmd->dummy) {
        mode = 9; /* dummy + read */
        dummycnt = ((cmd->dummy * cmd->address) / 8) - 1;
    } else
	    mode = 2; /* read-only */

    if (cmd->data == 2)
        dualquad = 1;
    else if (cmd->data == 4)
        dualquad = 2;

    if (dualquad && cmd->address == cmd->data)
        addrfmt = 1;
    else
        addrfmt = 0;

    if (cmd->mode)
        tokenen = 1;

    v |= bfield_set(1, 30, 30);        /* CmdEn */
    v |= bfield_set(1, 29, 29);        /* AddrEn */
    v |= bfield_set(addrfmt, 28, 28);  /* AddrFmt */
	v |= bfield_set(mode, 27, 24);     /* TransMode */
	v |= bfield_set(dualquad, 23, 22); /* DualQuad */
	v |= bfield_set(tokenen, 21, 21);  /* TokenEn */
    v |= bfield_set(dummycnt, 10, 9);  /* DummyCnt */
	v |= rxlen > 0 ? bfield_set(rxlen - 1, 8, 0) : 0;   /* RdTranCnt */

	return v;
}

#ifndef CONFIG_ATCSPI200_COMPACT
/**
 * atcspi200_cmd_read() - spi flash read command handler
 * @dev: SPI flash controller
 * @cmd: array of command and address bytes
 * @cmdlen: sizeof @cmd in bytes
 * @dummylen: length of dummy bytes to insert after address
 * @rx: rx buffer
 * @rxlen: sizeof @rx
 */
static
int atcspi200_cmd_read(struct device *dev, const u8 *cmd,
                size_t cmdlen, size_t dummylen,
                void *rx, size_t rxlen)
{
	u32 v, i, addr;
	u8 *buf = (u8 *) rx;
	unsigned long flags;

	atcspi200_wait_idle(dev);
	atcspi200_clear_fifo(dev);

	local_irq_save(flags);

	v = atcspi200_transfer_control(cmdlen, dummylen, 0, rxlen);
	atcspi200_writel(v, dev, TransCtrl);

    /* Address */
	if (bfield_get(v, 29, 29)) {
        addr = _get_addr(cmd);
	    atcspi200_writel(addr, dev, Addr);
    }

	/* Start SPI transfer by writing to Cmd */
	atcspi200_writel(cmd[0], dev, Cmd);

	/* Read cycles */
	for (i = 0; i < rxlen; i++) {
		atcspi200_wait_rxfifo(dev);
		buf[i] = atcspi200_readl(dev, Data) & 0xff;
	}

	local_irq_restore(flags);

	return 0;
}

/**
 * atcspi200_cmd_mio_read() - spi flash MIO read command handler
 * @dev: SPI flash controller
 * @cmd: Multi I/O read command
 * @rx: rx buffer
 * @rxlen: sizeof @rx
 */
static
int atcspi200_cmd_mio_read(struct device *dev,
                struct fast_read_cmd *cmd, u32 addr,
                void *rx, size_t rxlen)
{
	u32 v, i;
	u8 *buf = (u8 *) rx;
	unsigned long flags;

	atcspi200_wait_idle(dev);
	atcspi200_clear_fifo(dev);

	local_irq_save(flags);

	v = atcspi200_mio_transfer_control(cmd, rxlen);
	atcspi200_writel(v, dev, TransCtrl);

    /* Address */
    atcspi200_writel(addr, dev, Addr);

	/* Start SPI transfer by writing to Cmd */
	atcspi200_writel(cmd->opcode, dev, Cmd);

	/* Read cycles */
	for (i = 0; i < rxlen; i++) {
		atcspi200_wait_rxfifo(dev);
		buf[i] = atcspi200_readl(dev, Data) & 0xff;
	}

	local_irq_restore(flags);

	return 0;
}

/**
 * atcspi200_cmd_write() - spi flash write command handler
 * @dev: SPI flash controller
 * @cmd: array of command and address bytes
 * @cmdlen: sizeof @cmd in bytes
 * @tx: tx buffer
 * @txlen: sizeof @tx
 *
 */
static
int atcspi200_cmd_write(struct device *dev, const u8 *cmd, size_t cmdlen,
			void *tx, size_t txlen)
{
	u32 v, i, addr;
	u8 *buf = (u8 *) tx;
	unsigned long flags;

	atcspi200_wait_idle(dev);
	atcspi200_clear_fifo(dev);

	local_irq_save(flags);

	/* TransCtrl */
	v = atcspi200_transfer_control(cmdlen, 0, txlen, 0);
	atcspi200_writel(v, dev, TransCtrl);

    /* Address */
	if (bfield_get(v, 29, 29)) {
        addr = _get_addr(cmd);
	    atcspi200_writel(addr, dev, Addr);
    }

	/* Start SPI transfer by writing to Cmd */
	atcspi200_writel(cmd[0], dev, Cmd);

	/* Write cycles */
	for (i = 0; i < txlen; i++) {
		atcspi200_wait_txfifo(dev);
		atcspi200_writel(buf[i], dev, Data);
	}

	local_irq_restore(flags);

	return 0;
}
#endif

/**
 * atcspi200_set_mm_rcmd() - configure memory-mapped read command
 * @dev: SPI flash controller
 * @cmd: command to be used during memory-mapped read
 *
 */
static
bool atcspi200_set_mm_rcmd(struct device *dev,
                           const struct fast_read_cmd *cmd,
                           bool dryrun)
{
	struct atcspi200_driver_data *priv = dev->driver_data;
    int i;

    for (i = 0; i < ARRAY_SIZE(atcspi200_readcmds); i++) {
        if (!memcmp(cmd, &atcspi200_readcmds[i], sizeof(*cmd)))
            break;
    }

    if (dryrun)
        return (i < ARRAY_SIZE(atcspi200_readcmds));

    if (i < ARRAY_SIZE(atcspi200_readcmds)) {
        if (atcspi200_readl(dev, MemCtrl) & 0x100)
            udelay(priv->delay);
        atcspi200_writel(i, dev, MemCtrl);

        return true;
    }

	return false;
}

/**
 * atcspi200_is_quad_feasible() - query if quad mode is feasible
 * @dev: SPI flash controller
 *
 */
static
bool atcspi200_is_quad_feasible(struct device *dev)
{
	struct atcspi200_driver_data *priv = dev->driver_data;
    int i;

	for (i = 0; i < ARRAY_SIZE(pins); i++) {
        if (i == ATCSPI200_WP || i == ATCSPI200_HOLD) {
            if (priv->pmap[i] == NULL)
                return false;
        }
    }

    return true;
}

#ifndef CONFIG_ATCSPI200_COMPACT
static struct spi_flash_master_ops atcspi200_ops = {
	.cmd_read = atcspi200_cmd_read,
	.cmd_mio_read = atcspi200_cmd_mio_read,
	.cmd_write = atcspi200_cmd_write,
    .set_mm_rcmd = atcspi200_set_mm_rcmd,
    .is_quad_feasible = atcspi200_is_quad_feasible,
	.max_xfer_size = 0x200,
};
#endif

#ifdef CONFIG_ATCSPI200_COMPACT
enum flash_opt {
	FLASH_READ		= 0,
	FLASH_WRITE		= 1,
	FLASH_ERASE		= 2,
	FLASH_NOWAIT	= 3,
};

__ilm__ static void flash_wait(struct device *dev, uint32_t exp)
{
	uint32_t v;

	do {
		v = atcspi200_readl(dev, Status);
	} while ((v & exp) != 0);
}

__ilm__ static void flash_clear_fifo(struct device *dev)
{
	uint32_t v;

	atcspi200_writel(0x06, dev, Ctrl);

	do {
		v = atcspi200_readl(dev, Ctrl);
	} while (v & 0x06);
}

__ilm__ static void flash_exec_cmd(struct device *dev, uint8_t cmd, uint32_t trans)
{
	flash_wait(dev, 0x01);
	flash_clear_fifo(dev);

	atcspi200_writel(trans, dev, TransCtrl);
	atcspi200_writel(cmd, dev, Cmd);
}

__ilm__ static uint32_t flash_read_status(struct device *dev)
{
	flash_exec_cmd(dev, SPI_FLASH_OP_RDSR, 0x42000000);
	flash_wait(dev, 1 << 14);

	return atcspi200_readl(dev, Data);
}


__ilm__ static void flash_write_enable(struct device *dev)
{
	int status = 0;

	do {
		flash_exec_cmd(dev, SPI_FLASH_OP_WREN, 0x47000000);
		status = flash_read_status(dev);
	} while ((status & SR_WEL) == 0x00);
}


__ilm__ static void flash_write_disable(struct device *dev)
{
	int status = 0;

	do {
		flash_exec_cmd(dev, SPI_FLASH_OP_WRDI, 0x47000000);
		status = flash_read_status(dev);
	} while ((status & SR_WEL) == SR_WEL);
}

__ilm__ static int flash_write_in_progress(struct device *dev, uint32_t timeout)
{
	uint32_t status;
	uint64_t now;
	uint64_t ts;
	uint64_t to;

	now = hal_timer_value();
	to = now + timeout;

	do {
		status = flash_read_status(dev);
		if ((status & SR_WIP) == 0) {
			return 0;
		}

		ts = hal_timer_value();
	} while (ts < to);

	return -1;
}

__ilm__ static
int flash_operation(struct device *dev, uint32_t addr, uint8_t *buf, uint32_t size,
					uint8_t cmd, uint32_t trans, uint8_t opt, uint32_t timeout)
{
	unsigned long flags;
	int i;
	int ret = 0;

	local_irq_save(flags);

	if (opt != FLASH_READ) {
		flash_write_enable(dev);
	}

	atcspi200_writel(addr, dev, Addr);

	flash_exec_cmd(dev, cmd, trans);

	if (opt == FLASH_WRITE || opt == FLASH_READ) {
		for (i = 0; i < size; i++) {
			if (opt == FLASH_WRITE) {
				/* wait tx fifo */
				flash_wait(dev, 1 << 23);
				atcspi200_writel(buf[i], dev, Data);
			} else if (opt == FLASH_READ) {
				/* wait rx fifo */
				flash_wait(dev, 1 << 14);
				buf[i] = atcspi200_readl(dev, Data) & 0xFF;
			}
		}
	}

	if (opt == FLASH_WRITE || opt == FLASH_ERASE) {
		ret = flash_write_in_progress(dev, timeout);
	}

	if (opt != FLASH_READ) {
		flash_write_disable(dev);
	}

	local_irq_restore(flags);

	return ret;
}


static
int atcspi200_cmd_read_compact(struct device *dev, const u8 *cmd,
                			   size_t cmdlen, size_t dummylen,
                			   void *rx, size_t rxlen)
{
	u32 v, addr = 0;

	v = atcspi200_transfer_control(cmdlen, dummylen, 0, rxlen);

	if (bfield_get(v, 29, 29)) {
		addr = _get_addr(cmd);
	}

	flash_operation(dev, addr, rx, rxlen, cmd[0], v, FLASH_READ, 0);

	return 0;
}


static
int atcspi200_cmd_mio_read_compact(struct device *dev,
								   struct fast_read_cmd *cmd, u32 addr,
								   void *rx, size_t rxlen)
{
	u32 v;

	v = atcspi200_mio_transfer_control(cmd, rxlen);

	flash_operation(dev, addr, rx, rxlen, cmd->opcode, v, FLASH_READ, 0);

	return 0;
}

static
int atcspi200_cmd_write_compact(struct device *dev, const u8 *cmd, size_t cmdlen,
								void *tx, size_t txlen)
{
	struct spi_flash *flash = NULL;
	u32 v, addr = 0;
	u32 timeout = 500 * 1000;
	u8 opt = FLASH_WRITE;

	v = atcspi200_transfer_control(cmdlen, 0, txlen, 0);

	if (bfield_get(v, 29, 29)) {
		addr = _get_addr(cmd);
		flash = spi_flash_find_by_addr((off_t)(addr + dev->base[1]));
	}

	if (flash) {
		int i;

		timeout = flash->pp_timeout;

		for (i = 0; i < 4; i++ ){
			if (cmd[0] == flash->erase_info[i].opcode) {
				timeout = flash->erase_info[i].timeout;
				opt = FLASH_ERASE;
				break;
			}
		}
	}

	return flash_operation(dev, addr, tx, txlen, cmd[0], v, opt, timeout);
}



static struct spi_flash_master_ops atcspi200_ops_compact = {
	.cmd_read = atcspi200_cmd_read_compact,
	.cmd_mio_read = atcspi200_cmd_mio_read_compact,
	.cmd_write = atcspi200_cmd_write_compact,
	.set_mm_rcmd = atcspi200_set_mm_rcmd,
    .is_quad_feasible = atcspi200_is_quad_feasible,
	.max_xfer_size = 0x200,
};
#endif


#ifdef CONFIG_ATCSPI200_BITBANG

/*
 * Bit banging
 *
 * This has been developed for debugging purpose, and is not yet
 * complete. It is kept just in case.
 */

#define cs   	pmap[ATCSPI200_CS]->pin
#define clk  	pmap[ATCSPI200_CLK]->pin
#define mosi 	pmap[ATCSPI200_MOSI]->pin
#define miso 	pmap[ATCSPI200_MISO]->pin
#define wp   	pmap[ATCSPI200_WP]->pin
#define hold 	pmap[ATCSPI200_HOLD]->pin

static inline int atcspi200_set_cs(struct device *dev, int bit)
{
	struct atcspi200_driver_data *priv = dev->driver_data;
	return gpio_set_value(priv->cs, bit);
}

static inline int atcspi200_set_sclk(struct device *dev, int bit)
{
	struct atcspi200_driver_data *priv = dev->driver_data;
	return gpio_set_value(priv->clk, bit);
}

static inline int atcspi200_toggle_sclk(struct device *dev)
{
	struct atcspi200_driver_data *priv = dev->driver_data;
	int v = priv->sclk;
	priv->sclk ^= 1;

	return gpio_set_value(priv->clk, v ^ 1);
}

static inline int atcspi200_set_mosi(struct device *dev, int bit)
{
	struct atcspi200_driver_data *priv = dev->driver_data;
	return gpio_set_value(priv->mosi, bit);
}

static inline int atcspi200_get_miso(struct device *dev)
{
	struct atcspi200_driver_data *priv = dev->driver_data;
	return gpio_get_value(priv->miso);
}

static int atcspi200_write_byte(struct device *dev, u8 byte)
{
	struct atcspi200_driver_data *priv = dev->driver_data;
	int i;

	for (i = 0; i < 8; i++, byte <<= 1) {
		if (priv->cpha)
			atcspi200_toggle_sclk(dev);

		atcspi200_set_mosi(dev, (byte & 0x80) == 0x80);
		udelay(priv->delay);
		atcspi200_toggle_sclk(dev);
		udelay(priv->delay);

		if (!priv->cpha)
			atcspi200_toggle_sclk(dev);
	}

	return 0;
}

static int atcspi200_read_byte(struct device *dev)
{
	struct atcspi200_driver_data *priv = dev->driver_data;
	int i, byte = 0;

	for (i = 0; i < 8; i++) {
		if (priv->cpha)
			atcspi200_toggle_sclk(dev);

		byte |= atcspi200_get_miso(dev) & 0x1;
		byte <<= 1;
		udelay(priv->delay);
		atcspi200_toggle_sclk(dev);
		udelay(priv->delay);

		if (!priv->cpha)
			atcspi200_toggle_sclk(dev);
	}

	return byte;
}

static
int atcspi200_gpio_cmd_read(struct device *dev, const u8 *cmd,
                size_t cmdlen, size_t dummy_len,
			    void *rx, size_t rxlen)
{
	u8 *buf = rx;
	int i;
	unsigned long flags;

	local_irq_save(flags);

	atcspi200_set_cs(dev, 0);

	for (i = 0; i < cmdlen; i++)
		atcspi200_write_byte(dev, cmd[i]);

	for (i = 0; i < rxlen; i++)
		buf[i] = atcspi200_read_byte(dev);

	atcspi200_set_cs(dev, 1);

	local_irq_restore(flags);

	return 0;
}

static
int atcspi200_gpio_cmd_write(struct device *dev, const u8 *cmd, size_t cmdlen,
			     void *tx, size_t txlen)
{
	u8 *buf = tx;
	int i;
	unsigned long flags;

	local_irq_save(flags);

	atcspi200_set_cs(dev, 0);

	for (i = 0; i < cmdlen; i++)
		atcspi200_write_byte(dev, cmd[i]);

	for (i = 0; i < txlen; i++)
		atcspi200_write_byte(dev, buf[i]);

	atcspi200_set_cs(dev, 1);

	local_irq_restore(flags);

	return 0;
}

static struct spi_flash_master_ops atcspi200_gpio_ops = {
	.cmd_read = atcspi200_gpio_cmd_read,
	.cmd_write= atcspi200_gpio_cmd_write,
};

static struct driver atcspi200_driver;

static inline int atcspi200_gpio_probe(struct device *dev)
{
	struct atcspi200_driver_data *priv;
	struct spi_flash *flash;
	int i = 0;

	priv = kmalloc(sizeof(*priv));
	if (!priv)
		return -ENOMEM;

	priv->sclk = priv->cpol = 0;
	priv->cpha = 0;
	priv->delay = 10; /* 10 usec */

	for (i = 0; i < ARRAY_SIZE(pins); i++) {
		struct pinctrl_pin_map *pmap;

		pmap = pinctrl_lookup_platform_pinmap(dev, pins[i]);
		if (pmap == NULL) {
			err("pin is not specified for %s/%s\n",
			       dev_name(dev), pins[i]);
			goto free_pin;
		} else if (gpio_request(dev, pmap->id, pmap->pin) < 0) {
			err("failed to claim pin#%d as gpio\n", pmap->pin);
			goto free_pin;
		}
		priv->pmap[i] = pmap;
	}

	if (gpio_direction_output(priv->cs, 1) ||
	    gpio_direction_output(priv->clk, priv->sclk) ||
	    gpio_direction_output(priv->mosi, 0) ||
	    gpio_direction_output(priv->wp, 1) ||
	    gpio_direction_output(priv->hold, 1) ||
	    gpio_direction_input(priv->miso))
		return -EINVAL;

	dev->driver_data = priv;

	atcspi200_driver.ops = &atcspi200_gpio_ops;

	flash = spi_flash_probe(dev);
	if (!flash)
		return -ENODEV;

	spi_flash_add_device(flash);
	return 0;

 free_pin:
	for (; i >= 0; i--) {
		struct pinctrl_pin_map *pmap = priv->pmap[i];
		if (pmap && pmap->pin != -1)
			gpio_free(dev, pmap->id, pmap->pin);
	}

	return -EBUSY;
}
#endif

#ifdef CONFIG_PM_DM
static int atcspi200_suspend(struct device *dev, u32 *idle)
{
#ifdef CONFIG_SOC_SCM2010
	struct atcspi200_driver_data *priv = dev->driver_data;

	priv->ctx.trans_fmt = atcspi200_readl(dev, TransFmt);
	priv->ctx.timing = atcspi200_readl(dev, Timing);
	priv->ctx.mem_ctrl = atcspi200_readl(dev, MemCtrl);
#endif

	return 0;
}

static int atcspi200_resume(struct device *dev)
{
#ifdef CONFIG_SOC_SCM2010
	struct atcspi200_driver_data *priv = dev->driver_data;

	atcspi200_writel(priv->ctx.trans_fmt, dev, TransFmt);
	atcspi200_writel(priv->ctx.timing, dev, Timing);
	atcspi200_writel(priv->ctx.mem_ctrl, dev, MemCtrl);
#endif

	return 0;
}
#endif

__ilm__ static int atcspi200_clock_update(struct device *dev)
{
	unsigned long rate;
	struct clk *clk;
	struct clk *mux_spi0;
	struct clk *clk_240m;
	u32 v;

	mux_spi0 = clk_get(NULL, "mux_spi0");
	clk_240m = clk_get(NULL, "240m");

	if (!mux_spi0 || !clk_240m) {
		if ((clk = clk_get(dev, "pclk")) == NULL ||
		    (rate = clk_get_rate(clk)) == 0)
			return -EINVAL;
	} else {
		rate = 240000000;
	}

	v = 0;
	v |= bfield_set(rate/(CONFIG_ATCSPI_SCLK_HZ*2) - 1, 7, 0); /* SCLK_DIV */
	v |= bfield_set(2, 11, 8); /* CSHT */
	v |= bfield_set(2, 13, 12); /* CS2CLK */
	atcspi200_writel(v, dev, Timing);

	if (mux_spi0 && clk_240m) {
		clk_set_parent(mux_spi0, clk_240m);
	}

	return 0;
}

static int atcspi200_probe(struct device *dev)
{
	struct atcspi200_driver_data *priv;
	struct spi_flash *flash;
	struct clk *clk;
	u32 v = atcspi200_readl(dev, IdRev);
	int i = 0;
	unsigned long rate;

	if (bfield_get(v, 31, 12) != 0x02002)
		return -EINVAL;

	/* Timing */
	if (atcspi200_clock_update(dev) < 0) {
		return -EINVAL;
	}

	if ((clk = clk_get(dev, "pclk")) == NULL ||
		(rate = clk_get_rate(clk)) == 0)
		return -EINVAL;

	dbg("AndeShape(tm) ATCSPI200 (rev%d.%d) @%p clk=%luHz\n",
	       bfield_get(v, 11, 4), bfield_get(v, 3, 0),
	       dev->base[0], rate);

#ifdef CONFIG_ATCSPI200_BITBANG
	return atcspi200_gpio_probe(dev);
#endif

	priv = kmalloc(sizeof(*priv));
	if (!priv)
		return -ENOMEM;

	priv->sclk = priv->cpol = 0;
	priv->cpha = 0;
	priv->delay = 10; /* 10 usec */

	for (i = 0; i < ARRAY_SIZE(pins); i++) {
		struct pinctrl_pin_map *pmap;

		pmap = pinctrl_lookup_platform_pinmap(dev, pins[i]);
		if (pmap == NULL) {
            if (i != ATCSPI200_WP && i != ATCSPI200_HOLD) {
                /* WP and HOLD can be undefined when Quad mode is
                 * not supported.
                 */
			    goto free_pin;
            }
        }
		if (0 && i == ATCSPI200_CS) {
			if (gpio_request(dev, pmap->id, pmap->pin) < 0 ||
			    gpio_direction_output(pmap->pin, 1) < 0) {
				err("failed to claim pin#%d as gpio\n", pmap->pin);
				goto free_pin;
			}
		} else if (pmap && pinctrl_request_pin(dev, pmap->id, pmap->pin) < 0) {
			err("failed to claim pin#%d for %s\n", pmap->pin, dev_name(dev));
			goto free_pin;
		}
		priv->pmap[i] = pmap;
	}
	dev->driver_data = priv;

	/* Transfer format (TransFmt) */
	v = atcspi200_readl(dev, TransFmt) & bfield_set(0x3, 1, 0);
	v |= bfield_set(0x2, 17, 16); /* AddrLen: 3-byte address */
	v |= bfield_set(7, 12, 8); /* DataLen: byte-wise transfer */
	if (0) {
		v |= bfield_set(priv->cpol, 1, 1);
		v |= bfield_set(priv->cpha, 0, 0);
	}
	atcspi200_writel(v, dev, TransFmt);

	flash = spi_flash_probe(dev);
	if (!flash)
		return -ENODEV;

	spi_flash_add_device(flash);
	return 0;

 free_pin:
	for (; i >= 0; i--) {
		struct pinctrl_pin_map *pmap = priv->pmap[i];
		if (pmap && pmap->pin != -1)
			pinctrl_free_pin(dev, pmap->id, pmap->pin);
	}

	return -EBUSY;
}


static declare_driver(atcspi200) = {
	.name = "atcspi200-xip",
	.probe = atcspi200_probe,
#ifdef CONFIG_PM_DM
	.suspend = atcspi200_suspend,
	.resume	= atcspi200_resume,
#endif
#ifdef CONFIG_ATCSPI200_COMPACT
	.ops = &atcspi200_ops_compact,
#else
	.ops = &atcspi200_ops,
#endif
};

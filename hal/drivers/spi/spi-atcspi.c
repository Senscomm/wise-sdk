/*
 * Copyright 2022-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <soc.h>
#include <hal/kernel.h>
#include <hal/bitops.h>
#include <hal/spi.h>
#include <hal/console.h>
#include <hal/kmem.h>
#include <hal/kernel.h>
#include <hal/clk.h>
#include <hal/pinctrl.h>
#include <hal/dma.h>

#include "vfs.h"

#define ATCSPI_ID_REV					0x00
#define ATCSPI_TRANS_FORMAT				0x10
#define ATCSPI_DIRECT_IO_CTRL			0x14
#define ATCSPI_TRANS_CTRL				0x20
#define ATCSPI_CMD						0x24
#define ATCSPI_ADDR						0x28
#define ATCSPI_DATA						0x2c
#define ATCSPI_CTRL						0x30
#define ATCSPI_STATUS					0x34
#define ATCSPI_INTR_ENABLE				0x38
#define ATCSPI_INTR_STATUS				0x3c
#define ATCSPI_TIMING					0x40
#define ATCSPI_SLAVE_STATUS				0x60
#define ATCSPI_SLAVE_DATA_COUNT			0x64
#define ATCSPI_CFG_REG					0x7c

#define ATCSPI_SLAVE_READ_SINGLE_IO		0x0B
#define ATCSPI_SLAVE_READ_DUAL_IO		0x0C
#define ATCSPI_SLAVE_READ_QUAD_IO		0x0E

#define ATCSPI_SLAVE_WRITE_SINGLE_IO	0x51
#define ATCSPI_SLAVE_WRITE_DUAL_IO		0x52
#define ATCSPI_SLAVE_WRITE_QUAD_IO		0x54

#define ATCSPI_TRANS_ADDR_LEN(v)		(((v - 1) & 0x03) << 16)
#define ATCSPI_TRANS_BYTE				(7 << 8)
#define ATCSPI_TRANS_WORD				(7 << 8 | 1 << 7)
#define ATCSPI_TRANS_LSB_FIRST			(1 << 3)
#define ATCSPI_TRANS_SLAVE_MODE			(1 << 2)

#define ATCSPI_TRANS_ADDR_MASK			0x04

#define ATCSPI_TRANS_CTRL_CMD_EN		(0x01 << 30)
#define ATCSPI_TRANS_CTRL_ADDR_EN		(0x01 << 29)
#define ATCSPI_TRNAS_CTRL_ADDR_DATA		(0x01 << 28);
#define ATCSPI_TRANS_CTRL_MODE(v)		((v & 0x0f) << 24)
#define ATCSPI_TRANS_CTRL_IO_FORMAT(v)	((v & 0x03) << 22)
#define ATCSPI_TRANS_CTRL_TX_LEN(v)		((v - 1) << 12)
#define ATCSPI_TRANS_CTRL_DUMMY_CNT(v)	(((v - 1) & 0x03) << 9);
#define ATCSPI_TRANS_CTRL_RX_LEN(v)		(v - 1)

#define ATCSPI_TRANS_CTRL_MODE_MASK		(0x0f)
#define ATCSPI_TRANS_CTRL_DUMMY_MASK	(0x03)

#define ATCSPI_TRANS_MAX_LEN			512

#define ATCSPI_CTRL_TX_FIFO_THRES		(8 << 16)
#define ATCSPI_CTRL_RX_FIFO_THRES		(8 << 8)
#define ATCSPI_CTRL_TX_DMA_EN			(1 << 4)
#define ATCSPI_CTRL_RX_DMA_EN			(1 << 3)
#define ATCSPI_CTRL_FIFO_CLEAR			(1 << 2 | 1 << 1)
#define ATCSPI_CTRL_RESET				(1 << 0)

#define ATCSPI_INTR_SLV_CMD				(1 << 5)
#define ATCSPI_INTR_TRANS_END			(1 << 4)
#define ATCSPI_INTR_TX_FIFO				(1 << 3)
#define ATCSPI_INTR_RX_FIFO				(1 << 2)
#define ATCSPI_INTR_TX_UNDERUN			(1 << 1)
#define ATCSPI_INTR_RX_OVERRUN			(1 << 0)

#define ATCSPI_TXFIFO_FULL				(1 << 23)
#define ATCSPI_TXFIFO_EMPTY				(1 << 22)
#define ATCSPI_RXFIFO_EMPTY				(1 << 14)

#define ATCSPI_START_TRIGGER			0xFF

#define ATCSPI_SLV_STATUS_UNDERRUN		(1 << 18)
#define ATCSPI_SLV_STATUS_OVERRUN		(1 << 17)
#define ATCSPI_SLV_READY				(1 << 16)

enum spi_transfer_mode {
    WRITE_READ_SAMETIME	= 0,
    WRITE_ONLY			= 1,
    READ_ONLY			= 2,
    WRITE_AND_READ		= 3,
    READ_AND_WRITE		= 4,
    NO_DATA				= 7,
    DUMMY_WRITE			= 8,
    DUMMY_READ			= 9,
};

#define spi_write(v, o) do {		\
    writel(v, dev->base[0] + o);	\
} while (0)

#define spi_read(o) readl(dev->base[0] + o)

static const char *g_pins[] = { "cs", "clk", "mosi", "miso", "wp", "hold"};
static struct pinctrl_pin_map *g_pmap[6];
#if defined(CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_SLAVE) && defined(CONFIG_SOC_SCM2010)
/* XXX: this is a work-around to support a scenario where
 *      scm1612 is connected to a SPI master alongside with
 *      other SPI slaves, i.e., 1:N master-slave configuration.
 *      scm1612 will have to release DATA1-3 to enable other slaves
 *      to drive it after it's done addressed
 *      and reconfigure it back to its own use only when it has been
 *      addressed again.
 *
 *      (a: slave command recvd) - (b: configure DATA1-3 pins) -
 *      (c: serve a master) - (d: release DATA1-3 pins) -
 *      (a:) ...
 *
 *      The thing is that (b) needs some time. If SPI clock is fast enough,
 *      there is high chance of scm1612, i.e., slave, can't be ready to transmit
 *      data on time.
 *
 *      So, when we use this feature, we must make sure the master will insert
 *      enough DUMMY cycles after sending a slave command.
 */
#define MODE_GPIO 	0x888
#endif

#ifndef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC

#if defined(CONFIG_USE_SPI0) && !defined(CONFIG_SPI_FLASH)
static uint8_t g_extra_buf0[ATCSPI_TRANS_MAX_LEN] __attribute__((section(".dma_buffer")));
#endif
#ifdef CONFIG_USE_SPI1
static uint8_t g_extra_buf1[ATCSPI_TRANS_MAX_LEN] __attribute__((section(".dma_buffer")));
#endif
#ifdef CONFIG_USE_SPI2
static uint8_t g_extra_buf2[ATCSPI_TRANS_MAX_LEN] __attribute__((section(".dma_buffer")));
#endif

#endif

struct spi_trans_ctrl {
    enum spi_data_io_format data_io_format;

    int rx_len;
    int rx_len_extra;
    int rx_buf_oft;
    uint8_t *rx_buf;
    uint8_t rx_ready;

    int tx_len;
    int tx_len_extra;
    int tx_buf_oft;
    uint8_t *tx_buf;
    uint8_t tx_ready;

    uint8_t master_requested;

    uint8_t *extra_buf;
};

struct spi_driver_data {
    struct fops devfs_ops;
    struct device *dev;
    uint8_t	configured;
    uint8_t mode;
    uint8_t dev_busy;

    enum spi_role role;
    bool dma_enable;
    bool not_support_quad;
    uint8_t slave_extra_dummy_cycle;

    spi_cb cb;
    void *cb_ctx;
    struct spi_event event;

    struct spi_trans_ctrl trans_ctrl;

    struct device *dma_dev;
    uint8_t tx_dma_hw_req;
    uint8_t rx_dma_hw_req;
    int tx_dma_ch;
    int rx_dma_ch;

#ifdef CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_SLAVE
    uint8_t pin_miso;
    uint32_t mode_offset;
    uint32_t mode_mask;
    uint32_t mode_rega; /* Address of GPIO_MODEx register */
    uint32_t mode_spi;  /* Value of GPIO_MODEx register for SPI */
    uint32_t mode_gpio; /* Value of GPIO_MODEx register for GPIO */
#endif

    int slave; /* current counterpart as a master */

#ifdef CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_MASTER
    struct pinctrl_pin_map *pmap_cs;    /* back-up in case we need to restore. */
    int cs_gpio[32];
#endif
};

#ifdef CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_SLAVE
#ifdef CONFIG_SPI_CONTROL_MISO_FROM_ILM
__ilm__
#endif
static void atcspi_acquire_bus(struct device *dev)
{
#ifdef CONFIG_SOC_SCM2010
    struct spi_driver_data *priv = dev->driver_data;
    uint32_t v __maybe_unused;

    /*
     * Software change to GPIO, need re-change to SPI
     * before TX in slave mode.
     */
    writel(priv->mode_spi, priv->mode_rega);
#endif
}

#ifdef CONFIG_SPI_CONTROL_MISO_FROM_ILM
__ilm__
#endif
static void atcspi_release_bus(struct device *dev)
{
#ifdef CONFIG_SOC_SCM2010
    struct spi_driver_data *priv = dev->driver_data;

    /*
     * ATCSPI slave do not release SPI bus after trafer complete.
     * So, software change SPI pins to open drain GPIO to avoid affecting
     * other slaves.
     */
    writel(priv->mode_gpio, priv->mode_rega);
#endif
}
#endif

static void atcspi_tx_pio(struct device *dev, struct spi_trans_ctrl *trans_ctrl)
{
    int i;

    for (i = trans_ctrl->tx_buf_oft; i < trans_ctrl->tx_len + trans_ctrl->tx_len_extra; i++) {
        if (spi_read(ATCSPI_STATUS) & ATCSPI_TXFIFO_FULL) {
            break;
        }

        spi_write(trans_ctrl->tx_buf[trans_ctrl->tx_buf_oft], ATCSPI_DATA);
        trans_ctrl->tx_buf_oft++;

        if (trans_ctrl->tx_buf_oft == trans_ctrl->tx_len) {
            trans_ctrl->tx_buf = trans_ctrl->extra_buf;
        }
    }
}

static void atcspi_rx_pio(struct device *dev, struct spi_trans_ctrl *trans_ctrl)
{
    int i;

    for (i = trans_ctrl->rx_buf_oft; i < trans_ctrl->rx_len + trans_ctrl->rx_len_extra; i++) {
        if (spi_read(ATCSPI_STATUS) & ATCSPI_RXFIFO_EMPTY) {
            break;
        }

        trans_ctrl->rx_buf[trans_ctrl->rx_buf_oft] = spi_read(ATCSPI_DATA) & 0xff;
        trans_ctrl->rx_buf_oft++;

        if (trans_ctrl->rx_buf_oft == trans_ctrl->rx_len) {
            trans_ctrl->rx_buf = trans_ctrl->extra_buf;
        }
    }
}

static void atcspi_intr_clear(struct device *dev, uint32_t intr_status)
{
    uint32_t status;

    status = spi_read(ATCSPI_INTR_STATUS);
    status &= intr_status;
    spi_write(status, ATCSPI_INTR_STATUS);
}

static void atcspi_intr_enable(struct device *dev, uint32_t intr)
{
    uint32_t intr_en;

    intr_en = spi_read(ATCSPI_INTR_ENABLE);
    intr_en |= intr;
    spi_write(intr_en, ATCSPI_INTR_ENABLE);
}

static void atcspi_intr_disable(struct device *dev, uint32_t intr)
{
    uint32_t intr_en;

    intr_en = spi_read(ATCSPI_INTR_ENABLE);
    intr_en &= ~intr;
    spi_write(intr_en, ATCSPI_INTR_ENABLE);
}

static void atcspi_fifo_clear(struct device *dev)
{
    uint32_t v;

    do {
        v = spi_read(ATCSPI_CTRL);
        v |= ATCSPI_CTRL_FIFO_CLEAR;
        spi_write(v, ATCSPI_CTRL);

        do {
            v = spi_read(ATCSPI_CTRL);
        } while ((v & ATCSPI_CTRL_FIFO_CLEAR) != 0);

        v = spi_read(ATCSPI_STATUS);
    } while (!(v & ATCSPI_TXFIFO_EMPTY) || !(v & ATCSPI_RXFIFO_EMPTY));
}

static int atcspi_trans_cmd_config(struct device *dev, struct spi_cmd_cfg *cmd_cfg,
        uint8_t *buf, int len, bool is_tx)
{
    struct spi_driver_data *priv = dev->driver_data;
    struct spi_trans_ctrl *trans_ctrl = &priv->trans_ctrl;
    uint32_t v;
    uint32_t intr;

    if (!priv->configured) {
        return -EPERM;
    }

    intr = ATCSPI_INTR_TRANS_END;

    v = ATCSPI_TRANS_CTRL_CMD_EN;
    v |= ATCSPI_TRANS_CTRL_IO_FORMAT(trans_ctrl->data_io_format);

    if (is_tx) {
        trans_ctrl->rx_len = 0;
        trans_ctrl->rx_len_extra = 0;
        trans_ctrl->rx_buf = NULL;
        trans_ctrl->rx_buf_oft = 0;
        trans_ctrl->tx_len = len;
        trans_ctrl->tx_len_extra = 0;
        trans_ctrl->tx_buf = buf;
        trans_ctrl->tx_buf_oft = 0;

        v |= ATCSPI_TRANS_CTRL_TX_LEN(len);
    } else {
        trans_ctrl->rx_len = len;
        trans_ctrl->rx_len_extra = 0;
        trans_ctrl->rx_buf = buf;
        trans_ctrl->rx_buf_oft = 0;
        trans_ctrl->tx_len = 0;
        trans_ctrl->tx_len_extra = 0;
        trans_ctrl->tx_buf = NULL;
        trans_ctrl->tx_buf_oft = 0;

        v |= ATCSPI_TRANS_CTRL_RX_LEN(len);
    }

    if (cmd_cfg->addr_len != SPI_ADDR_NONE) {
        uint32_t tran_format;

        tran_format = spi_read(ATCSPI_TRANS_FORMAT);
        tran_format &= ~ATCSPI_TRANS_ADDR_LEN(ATCSPI_TRANS_ADDR_MASK);
        tran_format |= ATCSPI_TRANS_ADDR_LEN(cmd_cfg->addr_len);
        spi_write(tran_format, ATCSPI_TRANS_FORMAT);

        v |= ATCSPI_TRANS_CTRL_ADDR_EN;
        if (cmd_cfg->addr_io_format == SPI_ADDR_IO_SAME_DATA_PHASE) {
            v |= ATCSPI_TRNAS_CTRL_ADDR_DATA;
        }
    }

    if (len) {
        if (is_tx) {
            if (cmd_cfg->dummy_cycle) {
                v |= ATCSPI_TRANS_CTRL_DUMMY_CNT(cmd_cfg->dummy_cycle);
                v |= ATCSPI_TRANS_CTRL_MODE(DUMMY_WRITE);
            } else {
                v |= ATCSPI_TRANS_CTRL_MODE(WRITE_ONLY);
            }

            if (!priv->dma_enable) {
                intr |= (ATCSPI_INTR_TX_FIFO);
            }
        } else {
            if (cmd_cfg->dummy_cycle) {
                v |= ATCSPI_TRANS_CTRL_DUMMY_CNT(cmd_cfg->dummy_cycle);
                v |= ATCSPI_TRANS_CTRL_MODE(DUMMY_READ);
            } else {
                v |= ATCSPI_TRANS_CTRL_MODE(READ_ONLY);
            }

            if (!priv->dma_enable) {
                intr |= (ATCSPI_INTR_RX_FIFO);
            }
        }
    } else {
        v |= ATCSPI_TRANS_CTRL_MODE(NO_DATA);
    }

    atcspi_intr_enable(dev, intr);
    spi_write(v, ATCSPI_TRANS_CTRL);

    return 0;
}

static int atcspi_trans_config(struct device *dev, uint8_t trx_mode, uint8_t *tx_buf, int tx_len,
        uint8_t *rx_buf, int rx_len)
{
    struct spi_driver_data *priv = dev->driver_data;
    struct spi_trans_ctrl *trans_ctrl = &priv->trans_ctrl;
    uint32_t v;
    uint32_t intr;


    if (!priv->configured) {
        return -EPERM;
    }

    trans_ctrl->rx_len = rx_len;
    trans_ctrl->rx_len_extra = 0;
    trans_ctrl->rx_buf = rx_buf;
    trans_ctrl->rx_buf_oft = 0;
    trans_ctrl->tx_len = tx_len;
    trans_ctrl->tx_len_extra = 0;
    trans_ctrl->tx_buf = tx_buf;
    trans_ctrl->tx_buf_oft = 0;

    v = ATCSPI_TRANS_CTRL_IO_FORMAT(trans_ctrl->data_io_format);

    intr = ATCSPI_INTR_TRANS_END;

    if (rx_len && tx_len) {

        if (trx_mode == SPI_TRX_SAMETIME) {
            int len;

            if (rx_len != tx_len) {
                len = rx_len > tx_len ? rx_len : tx_len;
            } else {
                len = tx_len;
            }

            if (rx_len > tx_len) {
                trans_ctrl->tx_len_extra = rx_len - tx_len;
                memset(&trans_ctrl->extra_buf[tx_len], 0, trans_ctrl->tx_len_extra);
            } else {
                trans_ctrl->rx_len_extra = tx_len - rx_len;
            }

            v |= ATCSPI_TRANS_CTRL_RX_LEN(len);
            v |= ATCSPI_TRANS_CTRL_TX_LEN(len);

            /* write and read at the same time */
            v |= ATCSPI_TRANS_CTRL_MODE(WRITE_READ_SAMETIME);
        } else {
            if (trx_mode == SPI_TRX_TX_FIRST) {
                v |= ATCSPI_TRANS_CTRL_MODE(WRITE_AND_READ);
            } else {
                v |= ATCSPI_TRANS_CTRL_MODE(READ_AND_WRITE);
            }

            v |= ATCSPI_TRANS_CTRL_RX_LEN(rx_len);
            v |= ATCSPI_TRANS_CTRL_TX_LEN(tx_len);
        }


        if (!priv->dma_enable) {
            intr |= (ATCSPI_INTR_TX_FIFO | ATCSPI_INTR_RX_FIFO);
        }

        trans_ctrl->tx_ready = 1;
        trans_ctrl->rx_ready = 1;

    } else if (tx_len) {
        v |= ATCSPI_TRANS_CTRL_TX_LEN(tx_len);

        if (priv->role == SPI_ROLE_SLAVE && priv->slave_extra_dummy_cycle) {
            /* dummy, write */
            v |= ATCSPI_TRANS_CTRL_DUMMY_CNT(priv->slave_extra_dummy_cycle);
            v |= ATCSPI_TRANS_CTRL_MODE(DUMMY_WRITE);
        } else {
            /* write only */
            v |= ATCSPI_TRANS_CTRL_MODE(WRITE_ONLY);
        }

        if (!priv->dma_enable) {
            intr |= (ATCSPI_INTR_TX_FIFO);
        }

        trans_ctrl->tx_ready = 1;
    } else if (rx_len) {
        v |= ATCSPI_TRANS_CTRL_RX_LEN(rx_len);

        if (priv->role == SPI_ROLE_SLAVE && priv->slave_extra_dummy_cycle) {
            /* dummy, read */
            v |= ATCSPI_TRANS_CTRL_DUMMY_CNT(priv->slave_extra_dummy_cycle);
            v |= ATCSPI_TRANS_CTRL_MODE(DUMMY_READ);
        } else {
            /* read only */
            v |= ATCSPI_TRANS_CTRL_MODE(READ_ONLY);
        }

        if (!priv->dma_enable) {
            intr |= (ATCSPI_INTR_RX_FIFO);
        }

        trans_ctrl->rx_ready = 1;
    }

    atcspi_intr_enable(dev, intr);
    spi_write(v, ATCSPI_TRANS_CTRL);

    return 0;
}

static int atcspi_configure(struct device *dev, struct spi_cfg *cfg, spi_cb cb, void *ctx)
{
    struct spi_driver_data *priv = dev->driver_data;
    struct clk *parent;
    struct clk *mux_spi;
    char spi_mux_name[32];
    struct clk *clk;
    uint32_t rate;
    uint32_t v;

    spi_reset(dev);

    if (priv->not_support_quad &&
            cfg->data_io_format == SPI_DATA_IO_FORMAT_QUAD) {
        return -EINVAL;
    }

    if (cfg->dma_en && !priv->dma_dev) {
        printk("DMA is not enabled\n");
        return -EINVAL;
    }

#ifdef CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_MASTER
    if (cfg->role == SPI_ROLE_SLAVE && cfg->master_cs_bitmap) {
        return -EINVAL;
    }
#endif

    priv->mode = cfg->mode;
    priv->role = cfg->role;
    priv->dma_enable = cfg->dma_en;
    priv->slave_extra_dummy_cycle = cfg->slave_extra_dummy_cycle;

    priv->trans_ctrl.data_io_format = cfg->data_io_format;

    /* setup SPI transfer format */
    v = cfg->mode;
    if (cfg->role == SPI_ROLE_SLAVE) {
        v |= ATCSPI_TRANS_SLAVE_MODE;
        atcspi_intr_enable(dev, ATCSPI_INTR_SLV_CMD);
    }
    if (cfg->bit_order == SPI_BIT_LSB_FIRST) {
        v |= ATCSPI_TRANS_LSB_FIRST;
    }
    v |= ATCSPI_TRANS_BYTE; /* DataLen: byte-wise transfer */
    spi_write(v, ATCSPI_TRANS_FORMAT);

    /* setup SPI control */
    v = ATCSPI_CTRL_TX_FIFO_THRES | ATCSPI_CTRL_RX_FIFO_THRES;
    if (cfg->dma_en) {
        v |= ATCSPI_CTRL_TX_DMA_EN | ATCSPI_CTRL_RX_DMA_EN;
    }
    spi_write(v, ATCSPI_CTRL);

    /* setup SPI timing */
    sprintf(spi_mux_name, "%s%d", "mux_spi", dev_id(dev));
    mux_spi = clk_get(NULL, spi_mux_name);
    if (cfg->clk_src == SPI_CLK_SRC_XTAL) {
        parent = clk_get(NULL, "xtal_40m");
        clk_set_parent(mux_spi, parent);
    } else if (cfg->clk_src == SPI_CLK_SRC_PLL) {
        if (dev_id(dev) == 2) {
            parent = clk_get(NULL, "80m");
        } else {
            parent = clk_get(NULL, "240m");
        }

        clk_set_parent(mux_spi, parent);
    }

    if ((clk = clk_get(dev, "pclk")) == NULL ||
            (rate = clk_get_rate(clk)) == 0) {
        return -EINVAL;
    }

    v = (cfg->clk_div_2mul - 1) & 0xFF; /* SCLK_DIV */
    v |= 0x2 << 8; /* CSHT */
    v |= 0x2 << 12; /* CS2CLK */
    spi_write(v, ATCSPI_TIMING);


    priv->cb = cb;
    priv->cb_ctx = ctx;

#ifdef CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_MASTER
    for (int i = 0; i < ARRAY_SIZE(priv->cs_gpio); i++) {
        priv->cs_gpio[i] = -1;
    }

    for (int i = 0, slv = 0; i < 32; i++) {
        int ret;
        if (cfg->master_cs_bitmap & (1 << i)) {
            /* GPIO i is to be assigned to slave 'slv'. */
            pinctrl_free_pin(dev, "cs", i); /* May return an error. It's okay. */
            ret = gpio_request(dev, "cs", i);
            if (ret < 0) {
                printk("Error %d from gpio_request\n", ret);
                return -EINVAL;
            }
            ret = gpio_direction_output(i, 1);
            if (ret < 0) {
                printk("Error %d from gpio_direction_output\n", ret);
                return -EINVAL;
            }
            priv->cs_gpio[slv] = i;
#if 1
            printk("Pin %d has successfully been assigned to slave %d.\n", i, slv);
#endif
            slv++;
        }
    }

    if (!cfg->master_cs_bitmap) {
        /* Configure original pinmux for CS:
         * (1) because we didn't do so in probe at all, and
         * (2) we might have configured it as a GPIO previously.
         */
        gpio_free(dev, "cs", priv->pmap_cs->pin); /* May return error. */
        /* XXX: gpio_free will implicitly disable input of the pin,
         *      which is not right for a slave.
         *      So, let's re-enable it.
         */
        if (cfg->role == SPI_ROLE_SLAVE) {
            gpio_direction_input(priv->pmap_cs->pin);
        }
        pinctrl_request_pin(dev, priv->pmap_cs->id, priv->pmap_cs->pin);
    }
#endif

    priv->configured = 1;

    atcspi_fifo_clear(dev);

#ifdef CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_SLAVE
    /* These must be done after CS has been acquired. */
    /* XXX: but needs to be done only once at the first configuration. */
    if (priv->mode_spi == 0) {
        priv->mode_spi = readl(priv->mode_rega);
    }
    if (priv->mode_gpio == 0) {
        priv->mode_gpio = (priv->mode_spi & ~priv->mode_mask) |\
                          (MODE_GPIO << priv->mode_offset);
    }
    if (priv->role == SPI_ROLE_SLAVE) {
        /* Disable pin output so that
         * other slaves will be able to
         * control it. */
        v = readl(GPIO_BASE_ADDR + 0x10);
        v &= ~(0x7 << priv->pin_miso);
        writel(v, GPIO_BASE_ADDR + 0x10);

        atcspi_release_bus(dev);
    }
#endif

    return 0;
}

static int atcspi_reset(struct device *dev)
{
    struct spi_driver_data *priv = dev->driver_data;
    uint32_t v;

    if (priv->dma_enable) {
        if (priv->tx_dma_ch >= 0) {
            dma_ch_abort(priv->dma_dev, priv->tx_dma_ch);
            priv->tx_dma_ch = -1;
        }

        if (priv->rx_dma_ch >= 0) {
            dma_ch_abort(priv->dma_dev, priv->rx_dma_ch);
            priv->rx_dma_ch = -1;
        }
    }

    v = spi_read(ATCSPI_CTRL);
    v |= ATCSPI_CTRL_RESET;
    spi_write(v, ATCSPI_CTRL);

    atcspi_fifo_clear(dev);

#ifdef CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_SLAVE
    if (priv->role == SPI_ROLE_SLAVE) {
        atcspi_release_bus(dev);
    }
#endif

    return 0;
}

static int atcspi_dma_configure(struct device *dev, uint8_t *tx_buf, int tx_len,
        uint8_t *rx_buf, int rx_len)
{
    struct spi_driver_data *priv = dev->driver_data;
    struct dma_ctrl ctrl;
    struct dma_desc_chain dma_desc[2];
    int desc_num;
    int ret;

    if (!priv->dma_dev) {
        return -EIO;
    }

    if (tx_buf && tx_len) {
        if (!((uint32_t)tx_buf & 0xF0000000)) {
            printk("invalid buffer address for DMA %p\n", tx_buf);
            return -EINVAL;
        }

        ctrl.src_mode = DMA_MODE_NORMAL;
        ctrl.dst_mode = DMA_MODE_HANDSHAKE;
        ctrl.src_req = 0;
        ctrl.dst_req = priv->tx_dma_hw_req;
        ctrl.src_addr_ctrl = DMA_ADDR_CTRL_INCREMENT;
        ctrl.dst_addr_ctrl = DMA_ADDR_CTRL_FIXED;
        ctrl.src_width = DMA_WIDTH_BYTE;
        ctrl.dst_width = DMA_WIDTH_BYTE;
        ctrl.intr_mask = DMA_INTR_TC_MASK | DMA_INTR_ABT_MASK;
        ctrl.src_burst_size = DMA_SRC_BURST_SIZE_8;

        dma_desc[0].src_addr = (uint32_t)tx_buf;
        dma_desc[0].dst_addr = (uint32_t)(dev->base[0] + ATCSPI_DATA);
        dma_desc[0].len = tx_len;

        desc_num = 1;

        if (priv->trans_ctrl.tx_len_extra) {
            dma_desc[1].src_addr = (uint32_t)priv->trans_ctrl.extra_buf;
            dma_desc[1].dst_addr = (uint32_t)(dev->base[0] + ATCSPI_DATA);
            dma_desc[1].len = priv->trans_ctrl.tx_len_extra;
            desc_num = 2;
        }

        ret = dma_copy_hw(priv->dma_dev, false, &ctrl, dma_desc, desc_num, NULL, NULL, &priv->tx_dma_ch);
        if (ret) {
            return ret;
        }
    }

    if (rx_buf && rx_len) {
        if (!((uint32_t)rx_buf & 0xF0000000)) {
            printk("invalid buffer address for DMA %p\n", rx_buf);
            return -EINVAL;
        }

        ctrl.src_mode = DMA_MODE_HANDSHAKE;
        ctrl.dst_mode = DMA_MODE_NORMAL;
        ctrl.src_req = priv->rx_dma_hw_req;
        ctrl.dst_req = 0;
        ctrl.src_addr_ctrl = DMA_ADDR_CTRL_FIXED;
        ctrl.dst_addr_ctrl = DMA_ADDR_CTRL_INCREMENT;
        ctrl.src_width = DMA_WIDTH_BYTE;
        ctrl.dst_width = DMA_WIDTH_BYTE;
        ctrl.intr_mask = DMA_INTR_TC_MASK | DMA_INTR_ABT_MASK;
        ctrl.src_burst_size = DMA_SRC_BURST_SIZE_8;

        dma_desc[0].src_addr  = (uint32_t)(dev->base[0] + ATCSPI_DATA);
        dma_desc[0].dst_addr = (uint32_t)rx_buf;
        dma_desc[0].len = rx_len;

        desc_num = 1;

        if (priv->trans_ctrl.rx_len_extra) {
            dma_desc[1].src_addr = (uint32_t)(dev->base[0] + ATCSPI_DATA);
            dma_desc[1].dst_addr = (uint32_t)priv->trans_ctrl.extra_buf;
            dma_desc[1].len = priv->trans_ctrl.rx_len_extra;
            desc_num = 2;
        }

        ret = dma_copy_hw(priv->dma_dev, false, &ctrl, dma_desc, desc_num, NULL, NULL, &priv->rx_dma_ch);
        if (ret) {
            return ret;
        }
    }


    return 0;
}

static void atcspi_master_control_cs(struct device *dev, int slave, int val)
{
#ifdef CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_MASTER
    struct spi_driver_data *priv = dev->driver_data;

    if (priv->cs_gpio[slave] < 0) {
        return;
    }

#if 0
    printk("cs(%d): %d\n", priv->cs_gpio[slave], val);
#endif

    gpio_set_value(priv->cs_gpio[slave], val);
#endif
}

static int atcspi_master_transfer(struct device *dev, int slave, uint8_t trx_mode,
        uint8_t *tx_buf, int tx_len, uint8_t *rx_buf, int rx_len)
{
    struct spi_driver_data *priv = dev->driver_data;
    int ret;

    if (priv->role != SPI_ROLE_MASTER) {
        return -EIO;
    }

    if (priv->dev_busy) {
        return -EINPROGRESS;
    }

    if (tx_len > ATCSPI_TRANS_MAX_LEN || rx_len > ATCSPI_TRANS_MAX_LEN) {
        return -EINVAL;
    }

    if (tx_len == 0 && rx_len == 0) {
        return -EINVAL;
    }

    if (tx_len != 0 &&
            rx_len != 0 &&
            priv->trans_ctrl.data_io_format != SPI_DATA_IO_FORMAT_SINGLE) {
        printk("tx and rx only use single io format\n");
        return -EIO;
    }


    if ((ret = atcspi_trans_config(dev, trx_mode, tx_buf, tx_len, rx_buf, rx_len)) < 0) {
        printk("transfer config error\n");
        return ret;
    }

    if (priv->dma_enable) {
        ret = atcspi_dma_configure(dev, tx_buf, tx_len, rx_buf, rx_len);
        if (ret) {
            printk("dma config error\n");
            return ret;
        }
    }

    priv->dev_busy = 1;

    priv->slave = slave;
    atcspi_master_control_cs(dev, slave, 0);

    spi_write(ATCSPI_START_TRIGGER, ATCSPI_CMD);

    return 0;
}

static int atcspi_master_tx(struct device *dev, int slave, uint8_t *tx_buf, int len)
{
    return atcspi_master_transfer(dev, slave, 0, tx_buf, len, NULL, 0);
}

static int atcspi_master_rx(struct device *dev, int slave, uint8_t *rx_buf, int len)
{
    return atcspi_master_transfer(dev, slave, 0, NULL, 0, rx_buf, len);
}

static int atcspi_master_tx_rx(struct device *dev, int slave, uint8_t trx_mode,
        uint8_t *tx_buf, int tx_len, uint8_t *rx_buf, int rx_len)
{
    return atcspi_master_transfer(dev, slave, trx_mode, tx_buf, tx_len, rx_buf, rx_len);
}

static int atcspi_master_transfer_with_cmd(struct device *dev, int slave,
        struct spi_cmd_cfg *cmd_cfg, uint8_t *buf, int len, bool is_tx)
{
    struct spi_driver_data *priv = dev->driver_data;
    int ret;

    if (priv->role != SPI_ROLE_MASTER) {
        return -EIO;
    }

    if (priv->dev_busy) {
        return -EINPROGRESS;
    }

    if (len > ATCSPI_TRANS_MAX_LEN) {
        return -EINVAL;
    }

    if (!is_tx && len == 0) {
        return -EINVAL;
    }

    if ((ret = atcspi_trans_cmd_config(dev, cmd_cfg, buf, len, is_tx)) < 0) {
        return ret;
    }

    if (cmd_cfg->addr_len != SPI_ADDR_NONE) {
        spi_write(cmd_cfg->addr, ATCSPI_ADDR);
    }

    if (priv->dma_enable) {
        if (is_tx) {
            ret = atcspi_dma_configure(dev, buf, len, NULL, 0);
        } else {
            ret = atcspi_dma_configure(dev, NULL, 0, buf, len);
        }

        if (ret) {
            return ret;
        }
    }

    priv->dev_busy = 1;
    priv->slave = slave;
    atcspi_master_control_cs(dev, slave, 0);

    spi_write(cmd_cfg->cmd, ATCSPI_CMD);

    return 0;
}

static int atcspi_master_tx_with_cmd(struct device *dev, int slave,
        struct spi_cmd_cfg *cmd_cfg, uint8_t *tx_buf, int len)
{
    return atcspi_master_transfer_with_cmd(dev, slave, cmd_cfg, tx_buf, len, 1);
}


static int atcspi_master_rx_with_cmd(struct device *dev, int slave,
        struct spi_cmd_cfg *cmd_cfg, uint8_t *rx_buf, int len)
{
    return atcspi_master_transfer_with_cmd(dev, slave, cmd_cfg, rx_buf, len, 0);
}

static int atcspi_check_slave_status(struct device *dev)
{
    struct spi_driver_data *priv = dev->driver_data;
    uint32_t v;

    v = spi_read(ATCSPI_SLAVE_STATUS);

    if (v & ATCSPI_SLV_STATUS_UNDERRUN ||
            v & ATCSPI_SLV_STATUS_OVERRUN) {

        atcspi_reset(dev);

        if (priv->trans_ctrl.master_requested) {
            priv->trans_ctrl.master_requested = 0;
            return -1;
        }
    }

    return 0;
}

static int atcspi_slave_trans_prepare(struct device *dev, uint8_t trx_mode, uint8_t *tx_buf, int tx_len,
        uint8_t *rx_buf, int rx_len)
{
    struct spi_driver_data *priv = dev->driver_data;
    int ret;

    if (priv->role != SPI_ROLE_SLAVE) {
        return -EIO;
    }

    if (priv->dev_busy) {
        return -EINPROGRESS;
    }

    if (atcspi_check_slave_status(dev)) {
        return -EIO;
    }

    if (tx_len > ATCSPI_TRANS_MAX_LEN || rx_len > ATCSPI_TRANS_MAX_LEN) {
        return -EINVAL;
    }

    if (tx_len == 0 && rx_len == 0) {
        return -EINVAL;
    }

    if (tx_len != 0 &&
            rx_len != 0 &&
            priv->trans_ctrl.data_io_format != SPI_DATA_IO_FORMAT_SINGLE) {
        printk("tx and rx only use single io format\n");
        return -EIO;
    }

    if ((ret = atcspi_trans_config(dev, trx_mode, tx_buf, tx_len, rx_buf, rx_len)) < 0) {
        return ret;
    }

    if (priv->dma_enable) {
        ret = atcspi_dma_configure(dev, tx_buf, tx_len, rx_buf, rx_len);
        if (ret) {
            return ret;
        }
    } else {
        if (tx_len) {
            atcspi_tx_pio(dev, &priv->trans_ctrl);
        }
    }

    priv->dev_busy = 1;

    spi_write(ATCSPI_SLV_READY, ATCSPI_SLAVE_STATUS);

    return 0;
}

static int atcspi_slave_set_tx_buf(struct device *dev, uint8_t *tx_buf, int len)
{
    return atcspi_slave_trans_prepare(dev, 0, tx_buf, len, NULL, 0);
}

static int atcspi_slave_set_rx_buf(struct device *dev, uint8_t *rx_buf, int len)
{
    return atcspi_slave_trans_prepare(dev, 0, NULL, 0, rx_buf, len);
}

static int atcspi_slave_set_tx_rx_buf(struct device *dev, uint8_t trx_mode, uint8_t *tx_buf, int tx_len,
        uint8_t *rx_buf, int rx_len)
{
    return atcspi_slave_trans_prepare(dev, trx_mode, tx_buf, tx_len, tx_buf, rx_len);
}

static int atcspi_slave_cancel(struct device *dev)
{
    struct spi_driver_data *priv = dev->driver_data;

    if (priv->dev_busy) {

        atcspi_reset(dev);

        priv->dev_busy = 0;
    }

    return 0;
}

static int atcspi_slave_set_uesr_state(struct device *dev, uint16_t user_state)
{
    struct spi_driver_data *priv = dev->driver_data;
    uint32_t v;

    if (priv->role != SPI_ROLE_SLAVE) {
        return -EIO;
    }

    v = spi_read(ATCSPI_SLAVE_STATUS);
    v |= user_state;
    spi_write(v, ATCSPI_SLAVE_STATUS);

    return 0;
}

static int atcspi_irq(int irq, void *data)
{
    struct device *dev = data;
    struct spi_driver_data *priv = dev->driver_data;
    struct spi_trans_ctrl *trans_ctrl = &priv->trans_ctrl;
    struct spi_event *event= &priv->event;
    uint32_t intr_status;

    intr_status = spi_read(ATCSPI_INTR_STATUS);

    if (intr_status & ATCSPI_INTR_SLV_CMD) {
        uint8_t cmd;
        uint8_t need_event = 0;

        cmd = spi_read(ATCSPI_CMD);
        if (cmd == ATCSPI_SLAVE_READ_SINGLE_IO ||
                cmd == ATCSPI_SLAVE_READ_DUAL_IO   ||
                cmd == ATCSPI_SLAVE_READ_QUAD_IO) {
            if (!trans_ctrl->tx_ready) {
                event->type = SPI_EVENT_SLAVE_TX_REQ;
                need_event = 1;
                trans_ctrl->master_requested = 1;
            }
        } else if (cmd == ATCSPI_SLAVE_WRITE_SINGLE_IO ||
                cmd == ATCSPI_SLAVE_WRITE_DUAL_IO   ||
                cmd == ATCSPI_SLAVE_WRITE_QUAD_IO) {
            if (!trans_ctrl->rx_ready) {
                event->type = SPI_EVENT_SLAVE_RX_REQ;
                need_event = 1;
                trans_ctrl->master_requested = 1;
            }
        } else {
            event->type = SPI_EVENT_SLAVE_USER_DEFINE_REQ;
            event->data.sl_user_cmd.user_cmd = cmd;
            need_event = 1;
        }

        if (priv->cb && need_event) {
            priv->cb(event, priv->cb_ctx);
            memset(event, 0, sizeof(struct spi_event));
        }
    }

    if (intr_status & ATCSPI_INTR_TX_FIFO) {
        if ((trans_ctrl->tx_len + trans_ctrl->tx_len_extra) == trans_ctrl->tx_buf_oft) {
            atcspi_intr_disable(dev, ATCSPI_INTR_TX_FIFO);
        } else {
            atcspi_tx_pio(dev, trans_ctrl);
        }
    }

    if (intr_status & ATCSPI_INTR_RX_FIFO) {
        if ((trans_ctrl->rx_len + trans_ctrl->rx_len_extra) == trans_ctrl->rx_buf_oft) {
            atcspi_intr_disable(dev, ATCSPI_INTR_RX_FIFO);
        } else {
            atcspi_rx_pio(dev, trans_ctrl);
        }
    }

    if (intr_status & ATCSPI_INTR_TRANS_END) {
        if (!(spi_read(ATCSPI_STATUS) & ATCSPI_RXFIFO_EMPTY)) {
            atcspi_rx_pio(dev, trans_ctrl);
        }

        if (priv->role == SPI_ROLE_SLAVE) {
            uint32_t v;

#ifdef CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_SLAVE
            atcspi_release_bus(dev);
#endif
            event->type = SPI_EVENT_SLAVE_TRANS_CMPL;

            event->data.sl_cmpl.err = SPI_SLAVE_SUCCESS;
            v = spi_read(ATCSPI_SLAVE_STATUS);
            if (v & ATCSPI_SLV_STATUS_UNDERRUN) {
                event->data.sl_cmpl.err |= SPI_SLAVE_ERR_UNDERRUN;
            }

            if (v & ATCSPI_SLV_STATUS_OVERRUN) {
                event->data.sl_cmpl.err |= SPI_SLAVE_ERR_OVERRUN;
            }

            spi_write(v, ATCSPI_SLAVE_STATUS);

            v = spi_read(ATCSPI_SLAVE_DATA_COUNT);
            event->data.sl_cmpl.rx_amount = v & 0x3FF;
            event->data.sl_cmpl.tx_amount = (v >> 16) & 0x3FF;

            if (event->data.sl_cmpl.rx_amount > ATCSPI_TRANS_MAX_LEN ||
                    event->data.sl_cmpl.tx_amount > ATCSPI_TRANS_MAX_LEN) {
                event->data.sl_cmpl.err |= SPI_SLAVE_ERR_INVALID_LEN;
            }
        } else {
            event->type = SPI_EVENT_MASTER_TRANS_CMPL;
        }

        if (event->data.sl_cmpl.err != SPI_SLAVE_SUCCESS) {
            atcspi_reset(dev);
        }

        atcspi_intr_disable(dev, ATCSPI_INTR_TRANS_END |
                ATCSPI_INTR_RX_FIFO   |
                ATCSPI_INTR_TX_FIFO);

        trans_ctrl->rx_ready = 0;
        trans_ctrl->tx_ready = 0;
        trans_ctrl->master_requested = 0;

        if (priv->tx_dma_ch >= 0) {
            dma_ch_rel(priv->dma_dev, priv->tx_dma_ch);
            priv->tx_dma_ch = -1;
        }

        if (priv->rx_dma_ch >= 0) {
            dma_ch_rel(priv->dma_dev, priv->rx_dma_ch);
            priv->rx_dma_ch = -1;
        }

        if (priv->cb) {
            priv->cb(event, priv->cb_ctx);
            memset(event, 0, sizeof(struct spi_event));
        }

        priv->dev_busy = 0;

        if (priv->role == SPI_ROLE_MASTER) {
            atcspi_master_control_cs(dev, priv->slave, 1);
        }
    }

    atcspi_intr_clear(dev, intr_status);

    return 0;
}

#ifdef CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_SLAVE
#ifdef CONFIG_SPI_CONTROL_MISO_FROM_ILM
    __ilm__
#endif
static int atcspi_irq_pre(int irq, void *data)
{
    struct device *dev = data;
    uint32_t intr_status;

    intr_status = spi_read(ATCSPI_INTR_STATUS);
    if (intr_status & ATCSPI_INTR_SLV_CMD) {
        atcspi_acquire_bus(dev);
    }

    return atcspi_irq(irq, data);
}
#endif

static int atcspi_ioctl(struct file *file, unsigned int cmd, void *arg)
{
    struct spi_driver_data *priv = file->f_priv;
    struct device *dev = priv->dev;
    struct spi_cfg_arg *cfg_arg;
    struct spi_master_arg *master_arg;
    struct spi_slave_arg *slave_arg;
    int ret;

    switch (cmd) {
        case IOCTL_SPI_CONFIGURE:
            cfg_arg = arg;
            ret = atcspi_configure(dev, cfg_arg->cfg, cfg_arg->cb, cfg_arg->cb_ctx);
            break;
        case IOCTL_SPI_RESET:
            ret = atcspi_reset(dev);
            break;
        case IOCTL_SPI_MASTER_TX:
            master_arg = arg;
            ret = atcspi_master_transfer(dev, master_arg->slave, 0, master_arg->tx_buf,
                    master_arg->tx_len, NULL, 0);
            break;
        case IOCTL_SPI_MASTER_RX:
            master_arg = arg;
            ret = atcspi_master_transfer(dev, master_arg->slave, 0, NULL, 0,
                    master_arg->rx_buf, master_arg->rx_len);
            break;
        case IOCTL_SPI_MASTER_TX_RX:
            master_arg = arg;
            ret = atcspi_master_transfer(dev, master_arg->slave, master_arg->trx_mode,
                    master_arg->tx_buf, master_arg->tx_len, master_arg->rx_buf,
                    master_arg->rx_len);
            break;
        case IOCTL_SPI_MASTER_TX_WITH_CMD:
            master_arg = arg;
            ret = atcspi_master_transfer_with_cmd(dev, master_arg->slave, master_arg->cmd_cfg,
                    master_arg->tx_buf, master_arg->tx_len, 1);
            break;
        case IOCTL_SPI_MASTER_RX_WITH_CMD:
            master_arg = arg;
            ret = atcspi_master_transfer_with_cmd(dev, master_arg->slave, master_arg->cmd_cfg,
                    master_arg->rx_buf, master_arg->rx_len, 0);
            break;
        case IOCTL_SPI_SLAVE_SET_TX_BUF:
            slave_arg = arg;
            ret = atcspi_slave_trans_prepare(dev, 0, slave_arg->tx_buf, slave_arg->tx_len, NULL, 0);
            break;
        case IOCTL_SPI_SLAVE_SET_RX_BUF:
            slave_arg = arg;
            ret = atcspi_slave_trans_prepare(dev, 0, NULL, 0, slave_arg->rx_buf, slave_arg->rx_len);
            break;
        case IOCTL_SPI_SLAVE_SET_TX_RX_BUF:
            slave_arg = arg;
            ret = atcspi_slave_trans_prepare(dev, slave_arg->trx_mode, slave_arg->tx_buf, slave_arg->tx_len,
                    slave_arg->rx_buf, slave_arg->rx_len);
            break;
        case IOCTL_SPI_SLAVE_SET_USER_STATE:
            slave_arg = arg;
            ret = atcspi_slave_set_uesr_state(dev, slave_arg->user_state);
            break;
        case IOCTL_SPI_SLAVE_CANCEL:
            ret = atcspi_slave_cancel(dev);
            break;
        default:
            ret = -EINVAL;
            break;
    }

    return ret;
}

static int atcspi_probe(struct device *dev)
{
    struct spi_driver_data *priv;
    struct file *file;
    char buf[32];
    int ret;
    int id;
    int i;

    priv = kmalloc(sizeof(struct spi_driver_data));
    if (!priv) {
        printk("No memory\n");
        return -ENOMEM;
    }

    memset(priv, 0, sizeof(struct spi_driver_data));

    for (i = 0; i < 6; i++) {
        struct pinctrl_pin_map *pmap = g_pmap[i];

        pmap = pinctrl_lookup_platform_pinmap(dev, g_pins[i]);
        if (pmap == NULL) {
            if (!strcmp(g_pins[i], "wp") ||
                    !strcmp(g_pins[i], "hold")) {

                priv->not_support_quad = 1;
                continue;
            }
            printk("pin is not specified for %s/%s\n",
                    dev_name(dev), g_pins[i]);
            goto free_pin;
#ifdef CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_MASTER
        } else if (!strcmp(g_pins[i], "cs")) {
            priv->pmap_cs = pmap;
            /* Do not request the pin here.
             * We will do so in configure() if needed.
             */
#endif
        } else if (pinctrl_request_pin(dev, pmap->id, pmap->pin) < 0) {
            if (!strcmp(g_pins[i], "wp") ||
                    !strcmp(g_pins[i], "hold")) {

                priv->not_support_quad = 1;
                continue;
            }
            printk("failed to claim pin#%d as gpio\n", pmap->pin);
            goto free_pin;
        }
#if defined(CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_SLAVE) && defined(CONFIG_SOC_SCM2010)
        if (!strcmp(g_pins[i], "miso")) {
            uint32_t mode_offset, mode_mask;

            mode_offset = (pmap->pin & 0x07) * 4;
            mode_mask = 0xfff << mode_offset; /* DATA1-3 */

            priv->mode_offset = mode_offset;
            priv->mode_mask = mode_mask;
            priv->pin_miso = pmap->pin;
            priv->mode_rega = IOMUX_BASE_ADDR + ((pmap->pin / 8) * 4);
        }
#endif
    }

    priv->dev = dev;

#ifdef CONFIG_SPI_SUPPORT_MULTI_SLAVES_AS_A_SLAVE
    ret = request_irq(dev->irq[0], atcspi_irq_pre, dev_name(dev), dev->pri[0], dev);
#else
    ret = request_irq(dev->irq[0], atcspi_irq, dev_name(dev), dev->pri[0], dev);
#endif
    if (ret) {
        printk("%s irq req is failed(%d)", dev_name(dev), ret);
        kfree(priv);
        return -1;
    }

    dev->driver_data = priv;
    priv->devfs_ops.ioctl = atcspi_ioctl;

    id = dev_id(dev);
    if (id == 0 || id == 2) {
        if (id == 0) {
            priv->tx_dma_hw_req = DMA0_HW_REQ_SPI0_TX;
            priv->rx_dma_hw_req = DMA0_HW_REQ_SPI0_RX;
        } else {
            priv->tx_dma_hw_req = DMA0_HW_REQ_SPI2_TX;
            priv->rx_dma_hw_req = DMA0_HW_REQ_SPI2_RX;
        }

        priv->dma_dev = device_get_by_name("dmac.0");
        if (!priv->dma_dev) {
            printk("Not support DMA\n");
        }
    } else {
        priv->tx_dma_hw_req = DMA1_HW_REQ_SPI1_TX;
        priv->rx_dma_hw_req = DMA1_HW_REQ_SPI1_RX;

        priv->dma_dev = device_get_by_name("dmac.1");
        if (!priv->dma_dev) {
            printk("Not support DMA\n");
        }
    }

    priv->tx_dma_ch = -1;
    priv->rx_dma_ch = -1;

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
    priv->trans_ctrl.extra_buf = dma_kmalloc(ATCSPI_TRANS_MAX_LEN);
#else
#if defined(CONFIG_USE_SPI0) && !defined(CONFIG_SPI_FLASH)
    if (id == 0) {
        priv->trans_ctrl.extra_buf = g_extra_buf0;
    }
#endif
#ifdef CONFIG_USE_SPI1
    if (id == 1) {
        priv->trans_ctrl.extra_buf = g_extra_buf1;
    }
#endif
#ifdef CONFIG_USE_SPI2
    if (id == 2) {
        priv->trans_ctrl.extra_buf = g_extra_buf2;
    }
#endif
#endif

    if (!priv->trans_ctrl.extra_buf) {
        assert(0);
    }

    memset(buf, 0, sizeof(buf));
    sprintf(buf, "/dev/spi%d", id);
    file = vfs_register_device_file(buf, &priv->devfs_ops, priv);
    if (!file) {
        printk("%s: failed to register as %s\n", dev_name(dev), buf);
    }

    return 0;

free_pin:
    for (; i >= 0; i--) {
        struct pinctrl_pin_map *pmap = g_pmap[i];

        if (pmap && pmap->pin != -1) {
            gpio_free(dev, pmap->id, pmap->pin);
        }
    }

    return -EBUSY;
}

static int atcspi_shutdown(struct device *dev)
{
    struct spi_driver_data *priv = dev->driver_data;

    atcspi_reset(dev);

    free_irq(dev->irq[0], dev_name(dev));
#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
    if (priv->trans_ctrl.extra_buf) {
        dma_free(priv->trans_ctrl.extra_buf);
    }
#else
    (void)priv;
#endif
    free(dev->driver_data);

    return 0;
}

#ifdef CONFIG_PM_DM
static int atcspi_suspend(struct device *dev, u32 *idle)
{
    return 0;
}

static int atcspi_resume(struct device *dev)
{
    return 0;
}
#endif

struct spi_ops spi_ops = {
    .configure = atcspi_configure,
    .reset = atcspi_reset,
    .master_tx = atcspi_master_tx,
    .master_rx = atcspi_master_rx,
    .master_tx_rx = atcspi_master_tx_rx,
    .master_tx_with_cmd = atcspi_master_tx_with_cmd,
    .master_rx_with_cmd = atcspi_master_rx_with_cmd,
    .slave_set_tx_buf = atcspi_slave_set_tx_buf,
    .slave_set_rx_buf = atcspi_slave_set_rx_buf,
    .slave_set_tx_rx_buf = atcspi_slave_set_tx_rx_buf,
    .slave_cancel = atcspi_slave_cancel,
    .slave_set_uesr_state = atcspi_slave_set_uesr_state,
};

static declare_driver(spi) = {
    .name 		= "atcspi",
    .probe 		= atcspi_probe,
    .shutdown   = atcspi_shutdown,
#ifdef CONFIG_PM_DM
    .suspend    = atcspi_suspend,
    .resume     = atcspi_resume,
#endif
    .ops 		= &spi_ops,
};

#ifdef CONFIG_SOC_SCM2010
#if !defined(CONFIG_USE_SPI1) && !defined(CONFIG_USE_SPI2)
#if defined(CONFIG_USE_SPI0) && defined(CONFIG_SPI_FLASH)
#error SPI driver requires SPI devices. Select SPI devices or remove the driver
#endif
#endif
#endif

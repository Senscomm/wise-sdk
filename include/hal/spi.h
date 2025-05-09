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

#ifndef __SPI_H__
#define __SPI_H__

#include <hal/device.h>

/*
 * High-level device driver
 */

#define IOCTL_SPI_CONFIGURE 			(1)
#define IOCTL_SPI_RESET					(2)
#define IOCTL_SPI_MASTER_TX				(3)
#define IOCTL_SPI_MASTER_RX				(4)
#define IOCTL_SPI_MASTER_TX_RX			(5)
#define IOCTL_SPI_MASTER_TX_WITH_CMD	(6)
#define IOCTL_SPI_MASTER_RX_WITH_CMD	(7)
#define IOCTL_SPI_SLAVE_SET_TX_BUF		(8)
#define IOCTL_SPI_SLAVE_SET_RX_BUF		(9)
#define IOCTL_SPI_SLAVE_SET_TX_RX_BUF	(10)
#define IOCTL_SPI_SLAVE_SET_USER_STATE	(11)
#define IOCTL_SPI_SLAVE_CANCEL			(12)

enum spi_trx_mode {
    SPI_TRX_SAMETIME,
    SPI_TRX_TX_FIRST,
    SPI_TRX_RX_FIRST,
};

enum spi_role {
    SPI_ROLE_MASTER,
    SPI_ROLE_SLAVE,
};

enum spi_mode {
    SPI_MODE_0, /* active high, odd edge sampling */
    SPI_MODE_1, /* active high, even edge sampling */
    SPI_MODE_2, /* active low, odd edge sampling */
    SPI_MODE_3, /* active low, even edge sampling */
};

enum spi_data_io_format {
    SPI_DATA_IO_FORMAT_SINGLE,
    SPI_DATA_IO_FORMAT_DUAL,
    SPI_DATA_IO_FORMAT_QUAD,
};

enum spi_bit_order {
    SPI_BIT_MSB_FIRST,
    SPI_BIT_LSB_FIRST,
};

enum spi_clk_src {
    /* spi0, spi1 and spi2 use XTAL 40Mhz */
    SPI_CLK_SRC_XTAL,
    /* spi0 and spi1 use 240Mhz, spi2 uses 80Mhz */
    SPI_CLK_SRC_PLL,
};

enum spi_addr_len {
    SPI_ADDR_NONE,
    SPI_ADDR_1_BYTE,
    SPI_ADDR_2_BYTE,
    SPI_ADDR_3_BYTE,
    SPI_ADDR_4_BYTE,
};

enum spi_addr_io_fromat {
    SPI_ADDR_IO_SINGLE,
    SPI_ADDR_IO_SAME_DATA_PHASE,
};

enum spi_dummy_cycle {
    SPI_NO_DUMMY_CYCLE				= 0,
    SPI_SINGLE_IO_8_DUMMY_CYCLE 	= 1,
    SPI_SINGLE_IO_16_DUMMY_CYCLE 	= 2,
    SPI_SINGLE_IO_24_DUMMY_CYCLE 	= 3,
    SPI_SINGLE_IO_32_DUMMY_CYCLE 	= 4,
    SPI_DUAL_IO_4_DUMMY_CYCLE 		= 1,
    SPI_DUAL_IO_8_DUMMY_CYCLE 		= 2,
    SPI_DUAL_IO_12_DUMMY_CYCLE 		= 3,
    SPI_DUAL_IO_16_DUMMY_CYCLE 		= 4,
    SPI_QUAD_IO_2_DUMMY_CYCLE 		= 1,
    SPI_QUAD_IO_4_DUMMY_CYCLE 		= 2,
    SPI_QUAD_IO_6_DUMMY_CYCLE 		= 3,
    SPI_QUAD_IO_8_DUMMY_CYCLE 		= 4,
};

struct spi_cfg {
    enum spi_role role;
    enum spi_mode mode;
    enum spi_data_io_format data_io_format;
    enum spi_bit_order bit_order;
    enum spi_clk_src clk_src;
    enum spi_dummy_cycle slave_extra_dummy_cycle;
    uint32_t master_cs_bitmap; /* Bitmap of (up to 32) GPIOs to select slaves. */
    uint8_t clk_div_2mul;
    uint8_t dma_en;
};

struct spi_cmd_cfg {
    uint8_t cmd;
    enum spi_addr_io_fromat addr_io_format;
    enum spi_addr_len addr_len;
    uint32_t addr;
    enum spi_dummy_cycle dummy_cycle;
};

enum spi_transfer_err {
    SPI_SLAVE_SUCCESS		  = 0,
    SPI_SLAVE_ERR_UNDERRUN	  = 1 << 0,
    SPI_SLAVE_ERR_OVERRUN	  = 1 << 1,
    SPI_SLAVE_ERR_INVALID_LEN = 1 << 2,
};

enum spi_event_type {
    SPI_EVENT_MASTER_TRANS_CMPL,
    SPI_EVENT_SLAVE_TRANS_CMPL,
    SPI_EVENT_SLAVE_RX_REQ,
    SPI_EVENT_SLAVE_TX_REQ,
    SPI_EVENT_SLAVE_USER_DEFINE_REQ,
};

struct spi_slave_user_cmd {
    uint8_t user_cmd;
};

struct spi_slave_cmpl {
    int err;
    int rx_amount;
    int tx_amount;
};

struct spi_event {
    enum spi_event_type type;
    union {
        struct spi_slave_cmpl sl_cmpl;
        struct spi_slave_user_cmd sl_user_cmd;
    } data;
};

typedef void (*spi_cb)(struct spi_event *event, void *ctx);

struct spi_cfg_arg {
    struct spi_cfg *cfg;
    spi_cb cb;
    void *cb_ctx;
};

struct spi_master_arg {
    int slave;
    struct spi_cmd_cfg *cmd_cfg;
    enum spi_trx_mode trx_mode;
    uint8_t *tx_buf;
    uint32_t tx_len;
    uint8_t *rx_buf;
    uint32_t rx_len;
};

struct spi_slave_arg {
    uint16_t user_state;
    enum spi_trx_mode trx_mode;
    uint8_t *tx_buf;
    uint32_t tx_len;
    uint8_t *rx_buf;
    uint32_t rx_len;
};

/*
 * Low-level device driver
 */


struct spi_ops {
    int (*configure)(struct device *dev, struct spi_cfg *cfg, spi_cb cb, void *ctx);
    int (*master_tx)(struct device *dev, int slave, uint8_t *tx_buf, int len);
    int (*master_rx)(struct device *dev, int slave, uint8_t *rx_buf, int len);
    int (*master_tx_rx)(struct device *dev, int slave, uint8_t trx_mode,
            uint8_t *tx_buf, int tx_len, uint8_t *rx_buf, int rx_len);
    int (*master_tx_with_cmd)(struct device *dev, int slave, struct spi_cmd_cfg *cmd_cfg,
            uint8_t *tx_buf, int len);
    int (*master_rx_with_cmd)(struct device *dev, int slave, struct spi_cmd_cfg *cmd_cfg,
            uint8_t *rx_buf, int len);
    int (*slave_set_tx_buf)(struct device *dev, uint8_t *tx_buf, int len);
    int (*slave_set_rx_buf)(struct device *dev, uint8_t *rx_buf, int len);
    int (*slave_set_tx_rx_buf)(struct device *dev, uint8_t trx_mode, uint8_t *tx_buf, int tx_len, uint8_t *rx_buf, int rx_len);
    int (*slave_set_uesr_state)(struct device *dev, uint16_t user_state);
    int (*slave_cancel)(struct device *dev);
    int (*reset)(struct device *dev);
};

#define spi_ops(x)		((struct spi_ops *)(x)->driver->ops)

static __inline__ int spi_configure(struct device *dev, struct spi_cfg *cfg, spi_cb cb, void *ctx)
{
    if (!dev)
        return -ENODEV;

    return spi_ops(dev)->configure(dev, cfg, cb, ctx);
}

static __inline__ int spi_master_tx(struct device *dev, int slave,
        uint8_t *tx_buf, int len)
{
    if (!dev)
        return -ENODEV;

    return spi_ops(dev)->master_tx(dev, slave, tx_buf, len);
}

static __inline__ int spi_master_rx(struct device *dev, int slave,
        uint8_t *rx_buf, int len)
{
    if (!dev)
        return -ENODEV;

    return spi_ops(dev)->master_rx(dev, slave, rx_buf, len);
}

static __inline__ int spi_master_tx_rx(struct device *dev, int slave,
        uint8_t trx_mode, uint8_t *tx_buf, int tx_len, uint8_t *rx_buf, int rx_len)
{
    if (!dev)
        return -ENODEV;

    return spi_ops(dev)->master_tx_rx(dev, slave, trx_mode, tx_buf, tx_len, rx_buf, rx_len);
}

static __inline__ int spi_master_tx_with_cmd(struct device *dev, int slave,
        struct spi_cmd_cfg *cmd_cfg, uint8_t *tx_buf, int len)
{
    if (!dev)
        return -ENODEV;

    return spi_ops(dev)->master_tx_with_cmd(dev, slave, cmd_cfg, tx_buf, len);
}

static __inline__ int spi_master_rx_with_cmd(struct device *dev, int slave,
        struct spi_cmd_cfg *cmd_cfg, uint8_t *rx_buf, int len)
{
    if (!dev)
        return -ENODEV;

    return spi_ops(dev)->master_rx_with_cmd(dev, slave, cmd_cfg, rx_buf, len);
}

static __inline__ int spi_slave_set_tx_buf(struct device *dev, uint8_t *tx_buf, int len)
{
    if (!dev)
        return -ENODEV;

    return spi_ops(dev)->slave_set_tx_buf(dev, tx_buf, len);
}

static __inline__ int spi_slave_set_rx_buf(struct device *dev, uint8_t *rx_buf, int len)
{
    if (!dev)
        return -ENODEV;

    return spi_ops(dev)->slave_set_rx_buf(dev, rx_buf, len);
}

static __inline__ int spi_slave_set_tx_rx_buf(struct device *dev, uint8_t trx_mode, uint8_t *tx_buf, int tx_len,
        uint8_t *rx_buf, int rx_len)
{
    if (!dev)
        return -ENODEV;

    return spi_ops(dev)->slave_set_tx_rx_buf(dev, trx_mode, tx_buf, tx_len, rx_buf, rx_len);
}

static __inline__ int spi_slave_set_uesr_state(struct device *dev, uint16_t user_state)
{
    if (!dev)
        return -ENODEV;

    return spi_ops(dev)->slave_set_uesr_state(dev, user_state);
}

static __inline__ int spi_slave_cancel(struct device *dev)
{
    if (!dev)
        return -ENODEV;

    return spi_ops(dev)->slave_cancel(dev);
}

static __inline__ int spi_reset(struct device *dev)
{
    if (!dev)
        return -ENODEV;

    return spi_ops(dev)->reset(dev);
}

#endif // __SPI_H__

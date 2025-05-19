/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __SCM_SPI_H__
#define __SCM_SPI_H__

#include <stdint.h>

#include "wise_err.h"

#define SCM_SPI_TRANSFER_MAX_LEN	512

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SPI device index
 */
enum scm_spi_idx {
    SCM_SPI_IDX_0,
    SCM_SPI_IDX_1,
    SCM_SPI_IDX_2,
    SCM_SPI_IDX_MAX,
};

/**
 * SPI device role
 */
enum scm_spi_role {
    SCM_SPI_ROLE_MASTER,
    SCM_SPI_ROLE_SLAVE,
};

/**
 * SPI device mode
 */
enum scm_spi_mode {
    SCM_SPI_MODE_0,
    SCM_SPI_MODE_1,
    SCM_SPI_MODE_2,
    SCM_SPI_MODE_3,
};


/**
 * SPI device data io format
 */
enum scm_spi_data_io_format {
    SCM_SPI_DATA_IO_FORMAT_SINGLE,
    SCM_SPI_DATA_IO_FORMAT_DUAL,
    SCM_SPI_DATA_IO_FORMAT_QUAD,
};

/**
 * SPI device bit order
 */
enum scm_spi_bit_order {
    SCM_SPI_BIT_ORDER_MSB_FIRST,
    SCM_SPI_BIT_ORDER_LSB_FIRST,
};

/**
 * SPI device clock source
 */
enum scm_spi_clk_src {
    SCM_SPI_CLK_SRC_XTAL,	/* 40Mhz */
    SCM_SPI_CLK_SRC_PLL,	/* spi0 and spi1 use 240Mhz, spi2 uses 80Mhz */
};

/**
 * SPI dummy cycles
 */
enum scm_spi_dummy_cycle {
    SCM_SPI_DUMMY_CYCLE_NONE		 = 0,
    SCM_SPI_DUMMY_CYCLE_SINGLE_IO_8  = 1,
    SCM_SPI_DUMMY_CYCLE_SINGLE_IO_16 = 2,
    SCM_SPI_DUMMY_CYCLE_SINGLE_IO_24 = 3,
    SCM_SPI_DUMMY_CYCLE_SINGLE_IO_32 = 4,
    SCM_SPI_DUMMY_CYCLE_DUAL_IO_4	 = 1,
    SCM_SPI_DUMMY_CYCLE_DUAL_IO_8	 = 2,
    SCM_SPI_DUMMY_CYCLE_DUAL_IO_12	 = 3,
    SCM_SPI_DUMMY_CYCLE_DUAL_IO_16	 = 4,
    SCM_SPI_DUMMY_CYCLE_QUAD_IO_2	 = 1,
    SCM_SPI_DUMMY_CYCLE_QUAD_IO_4	 = 2,
    SCM_SPI_DUMMY_CYCLE_QUAD_IO_6	 = 3,
    SCM_SPI_DUMMY_CYCLE_QUAD_IO_8	 = 4,
};

/**
 * SPI device properties
 */
struct scm_spi_cfg {
    /* master or slave */
    enum scm_spi_role role;
    /* CPOL and CPHA mode */
    enum scm_spi_mode mode;
    /* data io format */
    enum scm_spi_data_io_format data_io_format;
    /* bit order */
    enum scm_spi_bit_order bit_order;
    /* Extend SPI dummy clock cycle when slave mode.
     * Extra dummy cycle is applied in slave TX or RX mode.
     * It is not applied in slave TRX mode.
     */
    enum scm_spi_dummy_cycle slave_extra_dummy_cycle;
    /* Bitmap of (up to 32) GPIOs to select slaves.
    */
    uint32_t master_cs_bitmap;
    /* clock source */
    enum scm_spi_clk_src clk_src;
    /* clock divide number. SCLK = clk_src / ((clk_div_2mul) * 2)
     * if this value is zero, SCLK = clk_src */
    uint8_t clk_div_2mul;
    /* DMA enable flag */
    uint8_t dma_en;
};

/**
 * SPI io format
 */
enum scm_spi_addr_io_format {
    SCM_SPI_ADDR_IO_SINGLE,
    SCM_SPI_ADDR_IO_SAME_DATA,
};

/**
 * SPI address length
 */
enum scm_spi_addr_len {
    SCM_SPI_ADDR_LEN_NONE,
    SCM_SPI_ADDR_LEN_1BYTES,
    SCM_SPI_ADDR_LEN_2BYTES,
    SCM_SPI_ADDR_LEN_3BYTES,
    SCM_SPI_ADDR_LEN_4BYTES,
};

/**
 * SPI command parameter
 */
struct scm_spi_cmd_cfg {
    uint8_t cmd;
    enum scm_spi_addr_io_format addr_io_format;
    enum scm_spi_addr_len addr_len;
    uint32_t addr;
    enum scm_spi_dummy_cycle dummy_cycle;
};

/**
 * SPI tx/rx mode
 */
enum scm_spi_trx_mode {
    SCM_SPI_TRX_SAMETIME,
    SCM_SPI_TRX_TX_FIRST,
    SCM_SPI_TRX_RX_FIRST,
};

/**
 * SPI event types
 */
enum scm_spi_event_type {
    SCM_SPI_EVENT_MASTER_TRANS_CMPL,
    SCM_SPI_EVENT_SLAVE_TRANS_CMPL,
    SCM_SPI_EVENT_SLAVE_RX_REQUEST,
    SCM_SPI_EVENT_SLAVE_TX_REQUEST,
    SCM_SPI_EVENT_SLAVE_USER_CMD,
};

/**
 * SPI slave errors
 */
enum scm_spi_slave_cmpl_err {
    SCM_SPI_SAVLE_CMPL_NO_ERR 	   = 0,
    SCM_SPI_SAVLE_CMPL_UNDERRUN    = (1 << 0),
    SCM_SPI_SAVLE_CMPL_OEVERRUN    = (1 << 1),
    SCM_SPI_SAVLE_CMPL_INVALID_LEN = (1 << 2),
};

/**
 * SPI slave user command parameter
 */
struct scm_spi_event_slave_user_cmd {
    uint8_t user_cmd;
};

/**
 * SPI event slave complete
 */
struct scm_spi_event_slave_cmpl {
    int err;
    int rx_amount;
    int tx_amount;
};

/**
 * SPI event
 */
struct scm_spi_event {
    enum scm_spi_event_type type;
    union {
        struct scm_spi_event_slave_user_cmd sl_user_cmd;
        struct scm_spi_event_slave_cmpl sl_cmpl;
    } data;
};

/**
 * SPI notification callback
 */
typedef int (*scm_spi_notify)(struct scm_spi_event *event, void *ctx);


/**
 * @brief Initialize SPI
 *
 * @param[in] idx index of the SPI
 */
int scm_spi_init(enum scm_spi_idx idx);

/**
 * @brief Deinitialize SPI
 *
 * @param[in] idx index of the SPI
 */
int scm_spi_deinit(enum scm_spi_idx idx);

/**
 * @brief configuration of SPI
 *
 * @param[in] idx index of the SPI
 * @param[in] cfg the config parameter
 * @param[in] notify event notification function
 * @param[in] ctx user context associated with the notification event
 */
int scm_spi_configure(enum scm_spi_idx idx, struct scm_spi_cfg *cfg, scm_spi_notify notify, void *ctx);

/**
 * @brief Reset SPI
 *
 * @param[in] idx index of the SPI
 */
int scm_spi_reset(enum scm_spi_idx idx);

/**
 * @brief SPI master to transmit data
 *
 * @param[in] idx index of the SPI
 * @param[in] slave index of the target slave
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the buffer
 * @param[in] timeout wait ms time for spi transmit complete
 */
int scm_spi_master_tx(enum scm_spi_idx idx, int slave, uint8_t *tx_buf,
        uint32_t tx_len, uint32_t timeout);

/**
 * @brief SPI master to transmit data
 *
 * @param[in] idx index of the SPI
 * @param[in] slave index of the target slave
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the buffer
 */
int scm_spi_master_tx_async(enum scm_spi_idx idx, int slave, uint8_t *tx_buf,
        uint32_t tx_len);

/**
 * @brief SPI master to receive data
 *
 * @param[in] idx index of the SPI
 * @param[in] slave index of the target slave
 * @param[in] rx_buf buffer to receive
 * @param[in] rx_len length of the buffer
 * @param[in] timeout wait ms time for spi receive complete
 */
int scm_spi_master_rx(enum scm_spi_idx idx, int slave, uint8_t *rx_buf,
        uint32_t rx_len, uint32_t timeout);

/**
 * @brief SPI master to receive data
 *
 * @param[in] idx index of the SPI
 * @param[in] slave index of the target slave
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the buffer
 */
int scm_spi_master_rx_async(enum scm_spi_idx idx, int slave, uint8_t *tx_buf,
        uint32_t tx_len);

/**
 * @brief SPI master to transmit and receive data
 * 	      This API can be used only in single IO data format
 *
 * @param[in] idx index of the SPI
 * @param[in] slave index of the target slave
 * @param[in] trx_mode master tx/rx mode
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the transmit buffer
 * @param[in] rx_buf buffer to receive
 * @param[in] rx_len length of the receive buffer
 * @param[in] timeout wait ms time for spi transfer complete
 */
int scm_spi_master_tx_rx(enum scm_spi_idx idx, int slave,
        enum scm_spi_trx_mode trx_mode, uint8_t *tx_buf, uint32_t tx_len,
        uint8_t *rx_buf, uint32_t rx_len, uint32_t timeout);

/**
 * @brief SPI master to transmit and receive data
 * 	      This API can be used only in single IO data format
 *
 * @param[in] idx index of the SPI
 * @param[in] slave index of the target slave
 * @param[in] trx_mode master tx/rx mode
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the transmit buffer
 * @param[in] rx_buf buffer to receive
 * @param[in] rx_len length of the receive buffer
 */
int scm_spi_master_tx_rx_async(enum scm_spi_idx idx, int slave,
        enum scm_spi_trx_mode trx_mode, uint8_t *tx_buf, uint32_t tx_len,
        uint8_t *rx_buf, uint32_t rx_len);

/**
 * @brief SPI master to transmit with command and address for flash device
 *
 * @param[in] idx index of the SPI
 * @param[in] slave index of the target slave
 * @param[in] cmd_cfg config value of command, address and dummy count
 * @param[in] tx_buf buffer to receive
 * @param[in] tx_len length of the buffer
 * @param[in] timeout wait ms time for spi transfer complete
 */
int scm_spi_master_tx_with_cmd(enum scm_spi_idx idx, int slave,
        struct scm_spi_cmd_cfg *cmd_cfg, uint8_t *tx_buf, uint32_t tx_len,
        uint32_t timeout);

/**
 * @brief SPI master to transmit with command and address for flash device
 *
 * @param[in] idx index of the SPI
 * @param[in] slave index of the target slave
 * @param[in] cmd_cfg config value of command, address and dummy count
 * @param[in] tx_buf buffer to receive
 * @param[in] tx_len length of the buffer
 */
int scm_spi_master_tx_with_cmd_async(enum scm_spi_idx idx, int slave,
        struct scm_spi_cmd_cfg *cmd_cfg, uint8_t *tx_buf, uint32_t tx_len);

/**
 * @brief SPI master to receive with command and address for flash device
 *
 * @param[in] idx index of the SPI
 * @param[in] slave index of the target slave
 * @param[in] cmd_cfg config value of command, address and dummy count
 * @param[in] rx_buf buffer to receive
 * @param[in] rx_len length of the buffer
 * @param[in] timeout wait ms time for spi transfer complete
 */
int scm_spi_master_rx_with_cmd(enum scm_spi_idx idx, int slave,
        struct scm_spi_cmd_cfg *cmd_cfg, uint8_t *rx_buf, uint32_t rx_len,
        uint32_t timeout);

/**
 * @brief SPI master to receive with command and address for flash device
 *
 * @param[in] idx index of the SPI
 * @param[in] slave index of the target slave
 * @param[in] cmd_cfg config value of command, address and dummy count
 * @param[in] rx_buf buffer to receive
 * @param[in] rx_len length of the buffer
 */
int scm_spi_master_rx_with_cmd_async(enum scm_spi_idx idx, int slave,
        struct scm_spi_cmd_cfg *cmd_cfg, uint8_t *rx_buf, uint32_t rx_len);


/**
 * @brief SPI slave to prepare to transmit data
 *
 * @param[in] idx index of the SPI
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the buffer
 */
int scm_spi_slave_set_tx_buf(enum scm_spi_idx idx, uint8_t *tx_buf, uint32_t tx_len);

/**
 * @brief SPI slave to prepare to receive data
 *
 * @param[in] idx index of the SPI
 * @param[in] tx_buf buffer to receive
 * @param[in] tx_len length of the buffer
 */
int scm_spi_slave_set_rx_buf(enum scm_spi_idx idx, uint8_t *rx_buf, uint32_t rx_len);

/**
 * @brief SPI slave to prepare transmit and receive data
 * 	      This API can be used only in single IO data format
 *
 * @param[in] idx index of the SPI
 * @param[in] trx_mode slave tx/rx mode
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the transmit buffer
 * @param[in] rx_buf buffer to receive
 * @param[in] rx_len length of the receive buffer
 */
int scm_spi_slave_set_tx_rx_buf(enum scm_spi_idx idx, enum scm_spi_trx_mode trx_mode,
        uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len);

/**
 * @brief SPI slave to transmit state data
 * 	      If master request slave state via command(0x05, 0x15, 0x25), slave send 32bit state
 * 	      Among 32-bit states, 16 bits of lsb can be set by the user.
 *
 * @param[in] idx index of the SPI
 * @param[in] user_state state of slave device
 */
int scm_spi_slave_set_user_state(enum scm_spi_idx idx, uint16_t user_state);


/**
 * @brief SPI slave to cancel waiting master trigger
 *
 * @param[in] idx index of the SPI
 */
int scm_spi_slave_cancel(enum scm_spi_idx idx);

#ifdef __cplusplus
}
#endif

#endif //__SCM_SPI_H__

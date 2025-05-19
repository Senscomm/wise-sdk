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

#ifndef __SCM_I2C_H__
#define __SCM_I2C_H__

#include <stdint.h>

#include "wise_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * I2C device index
 */
enum scm_i2c_idx {
	SCM_I2C_IDX_0,
	SCM_I2C_IDX_1,
	SCM_I2C_IDX_GPIO,
	SCM_I2C_IDX_MAX,
};

/**
 * I2C device role
 */
enum scm_i2c_role {
	SCM_I2C_ROLE_MASTER,
	SCM_I2C_ROLE_SLAVE,
};

/**
 * I2C device addressing
 */
enum scm_i2c_addr_len {
	SCM_I2C_ADDR_LEN_7BIT,
	SCM_I2C_ADDR_LEN_10BIT,
};

/**
 * I2C device properties
 */
struct scm_i2c_cfg {
	enum scm_i2c_role role;         /* master or slave */
	enum scm_i2c_addr_len addr_len; /* addressing length */
	uint32_t bitrate;               /* target bitrate in bps */
	uint16_t slave_addr;            /* used for slave mode only */
	uint8_t dma_en;                 /* use of the DMA */
	uint8_t pull_up_en;             /* use of the internal pull up */
};

/**
 * I2C event types
 */
enum scm_i2c_event_type {
	SCM_I2C_EVENT_MASTER_TRANS_CMPL,
	SCM_I2C_EVENT_SLAVE_RX_REQUEST,
	SCM_I2C_EVENT_SLAVE_TX_REQUEST,
	SCM_I2C_EVENT_SLAVE_RX_CMPL,
	SCM_I2C_EVENT_SLAVE_TX_CMPL,
};

/**
 * I2C event data when event type is SCM_I2C_EVENT_MASTER_TRANS_CMPL
 */
struct scm_i2c_event_master_trans_cmpl {
	int tx_len;  /* number of transferred bytes */
	int rx_len;  /* number of transferred bytes */
};

/**
 * I2C event data when event type is SCM_I2C_EVENT_SLAVE_TX_CMPL
 */
struct scm_i2c_event_slave_tx_cmpl {
	int len;  		   /* number of transferred bytes */
	uint8_t truncated; /* underrun, application supplied not enough tx buffer */
};

/**
 * I2C event data when event type is SCM_I2C_EVENT_SLAVE_RX_CMPL
 */
struct scm_i2c_event_slave_rx_cmpl {
	int len;  		   /* number of transferred bytes */
	uint8_t truncated; /* overrun, application supplied not enough rx buffer */
};

/*
 * I2C event
 */
struct scm_i2c_event {
	enum scm_i2c_event_type type;
	union {
		struct scm_i2c_event_master_trans_cmpl master_trans_cmpl;
		struct scm_i2c_event_slave_tx_cmpl slave_tx_cmpl;
		struct scm_i2c_event_slave_rx_cmpl slave_rx_cmpl;
	} data;
};

/**
 * I2C notification callback
 */
typedef int (*scm_i2c_notify)(struct scm_i2c_event *event, void *ctx);

/**
 * @brief Initialize I2C
 *
 * @param[in] idx index of the I2C
 */
int scm_i2c_init(enum scm_i2c_idx idx);

/**
 * @brief Deinitialize I2C
 *
 * @param[in] idx index of the I2C
 */
int scm_i2c_deinit(enum scm_i2c_idx idx);

/**
 * @brief Configure I2C
 *
 * @param[in] idx index of the I2C
 * @param[in] cfg the cfg parameter
 * @param[in] notify event notification function
 * @param[in] ctx user context associated with the notification event
 */
int scm_i2c_configure(enum scm_i2c_idx idx, struct scm_i2c_cfg *cfg, scm_i2c_notify notify, void *ctx);

/**
 * @brief Reset I2C
 *
 * @param[in] idx index of the I2C
 */
int scm_i2c_reset(enum scm_i2c_idx idx);

/**
 * @brief Probe the I2C bus to see if there is a device with this address is present
 *
 * @param[in] idx index of the I2C
 * @param[in] addr slave address
 * @param[in] status probed status
 */
int scm_i2c_master_probe(enum scm_i2c_idx idx, uint16_t addr, uint8_t *status);

/**
 * @brief I2C master to transmit data
 *
 * @param[in] idx index of the I2C
 * @param[in] addr slave address (If idx is SCM_I2C_IDX_GPIO, write bit must be included in address.)
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the buffer
 * @param[in] timeout wait ms time for i2c complete
 */
int scm_i2c_master_tx(enum scm_i2c_idx idx, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len, uint32_t timeout);

/**
 * @brief I2C master to transmit data
 *
 * @param[in] idx index of the I2C
 * @param[in] addr slave address
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the buffer
 */
int scm_i2c_master_tx_async(enum scm_i2c_idx idx, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len);

/**
 * @brief I2C master to receive data
 *
 * @param[in] idx index of the I2C
 * @param[in] addr slave address (If idx is SCM_I2C_IDX_GPIO, read bit must be included in address.)
 * @param[in] rx_buf buffer to receive
 * @param[in] rx_len length of the buffer
 * @param[in] timeout wait ms time for i2c complete
 */
int scm_i2c_master_rx(enum scm_i2c_idx idx, uint16_t addr, uint8_t *rx_buf, uint32_t rx_len, uint32_t timeout);

/**
 * @brief I2C master to receive data
 *
 * @param[in] idx index of the I2C
 * @param[in] addr slave address
 * @param[in] rx_buf buffer to receive
 * @param[in] rx_len length of the buffer
 */
int scm_i2c_master_rx_async(enum scm_i2c_idx idx, uint16_t addr, uint8_t *rx_buf, uint32_t rx_len);

/**
 * @brief I2C master to transmit data and receive data without the stop condition
 *
 * @param[in] idx index of the I2C
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the buffer
 * @param[in] rx_buf buffer to receive
 * @param[in] rx_len length of the buffer
 * @param[in] timeout wait ms time for i2c complete
 */
int scm_i2c_master_tx_rx(enum scm_i2c_idx idx, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len, uint32_t timeout);

/**
 * @brief I2C master to transmit data and receive data without the stop condition
 *
 * @param[in] idx index of the I2C
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the buffer
 * @param[in] rx_buf buffer to receive
 * @param[in] rx_len length of the buffer
 */
int scm_i2c_master_tx_rx_async(enum scm_i2c_idx idx, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len);

/**
 * @brief I2C slave to transmit data. This function must be called upon SCM_I2C_EVENT_SLAVE_TX_REQUEST event.
 *
 * @param[in] idx index of the I2C
 * @param[in] addr slave address
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the buffer
 */
int scm_i2c_slave_tx(enum scm_i2c_idx idx, uint8_t *tx_buf, uint32_t tx_len);

/**
 * @brief I2C slave to receive data. This function must be called upon SCM_I2C_EVENT_SLAVE_RX_REQUEST event.
 *
 * @param[in] idx index of the I2C
 * @param[in] rx_buf buffer to receive
 * @param[in] rx_len length of the buffer
 */
int scm_i2c_slave_rx(enum scm_i2c_idx idx, uint8_t *rx_buf, uint32_t rx_len);

#ifdef __cplusplus
}
#endif

#endif //__SCM_I2C_H__

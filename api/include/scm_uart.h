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

#ifndef __SCM_UART_H__
#define __SCM_UART_H__

#include <stdint.h>

#include "hal/uart.h"
#include "wise_err.h"

#define SCM_UART_TRANSFER_MAX_LEN	CONFIG_CLI_UARTBUF_MAXLEN

/**
 * UART device index
 */
enum scm_uart_idx {
	SCM_UART_IDX_0,
	SCM_UART_IDX_1,
	SCM_UART_IDX_2,
	SCM_UART_IDX_MAX,
};

enum scm_uart_baudrate {
	SCM_UART_BDR_50		= 50,
	SCM_UART_BDR_75		= 75,
	SCM_UART_BDR_110	= 110,
	SCM_UART_BDR_134	= 134,
	SCM_UART_BDR_150	= 150,
	SCM_UART_BDR_200	= 200,
	SCM_UART_BDR_300	= 300,
	SCM_UART_BDR_600	= 600,
	SCM_UART_BDR_1200	= 1200,
	SCM_UART_BDR_1800	= 1800,
	SCM_UART_BDR_2400	= 2400,
	SCM_UART_BDR_4800	= 4800,
	SCM_UART_BDR_9600	= 9600,
	SCM_UART_BDR_19200	= 19200,
	SCM_UART_BDR_38400	= 38400,
	SCM_UART_BDR_57600	= 57600,
	SCM_UART_BDR_115200	= 115200,
};

enum scm_uart_data_bits {
	SCM_UART_DATA_BITS_5,
	SCM_UART_DATA_BITS_6,
	SCM_UART_DATA_BITS_7,
	SCM_UART_DATA_BITS_8,
};

enum scm_uart_parity {
	SCM_UART_NO_PARITY,
	SCM_UART_ODD_PARITY,
	SCM_UART_EVENT_PARITY,
};

enum scm_uart_stop_bits {
	SCM_UART_STOP_BIT_1,
	SCM_UART_STOP_BIT_2,
};

/**
 * UART device properties
 */
struct scm_uart_cfg {
	enum scm_uart_baudrate baudrate;
	enum scm_uart_data_bits data_bits;
	enum scm_uart_parity parity;
	enum scm_uart_stop_bits stop_bits;
	uint8_t dma_en;
};

/**
 * UART event types
 */
enum scm_uart_event_type {
	SCM_UART_EVENT_TX_CMPL,
	SCM_UART_EVENT_RX_CMPL,
};

enum scm_uart_err {
	SCM_UART_ERR_NO			= 0,
	SCM_UART_ERR_OVERRUN	= (1 << 0),
	SCM_UART_ERR_PARITY		= (1 << 1),
	SCM_UART_ERR_FRAMING	= (1 << 2),
	SCM_UART_ERR_LINE_BREAK = (1 << 3),
};


/**
 * UART event
 */
struct scm_uart_event {
	enum scm_uart_event_type type;
	uint8_t err;
};

/**
 * UART notification callback
 */
typedef int (*scm_uart_notify)(enum scm_uart_idx idx, struct scm_uart_event *event,
        void *ctx);


/**
 * @brief Initialize UART
 *
 * @param[in] idx index of the UART
 * @param[in] cfg the config parameter
 */
int scm_uart_init(enum scm_uart_idx idx, struct scm_uart_cfg *cfg);

/**
 * @brief Deinitialize UART
 *
 * @param[in] idx index of the UART
 */
int scm_uart_deinit(enum scm_uart_idx idx);

/**
 * @brief UART transmit data
 *
 * @param[in] idx index of the UART
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the buffer
 * @param[in] timeout wait ms time for uart transmit complete, or -1(indefinitely)
 */
int scm_uart_tx(enum scm_uart_idx idx, uint8_t *tx_buf, uint32_t tx_len, int timeout);

/**
 * @brief UART transmit data
 *
 * @param[in] idx index of the UART
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the buffer
 * @param[in] notify event notification function
 * @param[in] ctx user context associated with the notification event
 */
int scm_uart_tx_async(enum scm_uart_idx idx, uint8_t *tx_buf, uint32_t tx_len,
        scm_uart_notify notify, void *ctx);

/**
 * @brief UART receive data
 *
 * @param[in] idx index of the UART
 * @param[in] rx_buf buffer to receive
 * @param[in/out] (in) rx_len length of the buffer, (out) actual received length
 * @param[in] timeout wait ms time for uart receive complete, or -1(indefinitely)
 */
int scm_uart_rx(enum scm_uart_idx idx, uint8_t *rx_buf, uint32_t *rx_len, int timeout);

/**
 * @brief UART receive data
 *
 * @param[in] idx index of the UART
 * @param[in] tx_buf buffer to transmit
 * @param[in] tx_len length of the buffer
 * @param[in] notify event notification function
 * @param[in] ctx user context associated with the notification event
 */
int scm_uart_rx_async(enum scm_uart_idx idx, uint8_t *tx_buf, uint32_t tx_len,
        scm_uart_notify notify, void *ctx);

/**
 * @brief UART reset
 *
 * @param[in] idx index of the UART
 */

int scm_uart_reset(enum scm_uart_idx idx);

#endif //__SCM_UART_H__

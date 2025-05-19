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

#ifndef __UART_H__
#define __UART_H__

#include <hal/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * High-level device driver
 */

#define IOCTL_UART_INIT		    0
#define IOCTL_UART_DEINIT	    1
#define IOCTL_UART_TX		    2
#define IOCTL_UART_RX		    3
#define IOCTL_UART_RESET	    4
#define IOCTL_UART_GET_RX_LEN	5

enum uart_baudrate {
	UART_BDR_50		= 50,
	UART_BDR_75		= 75,
	UART_BDR_110	= 110,
	UART_BDR_134	= 134,
	UART_BDR_150	= 150,
	UART_BDR_200	= 200,
	UART_BDR_300	= 300,
	UART_BDR_600	= 600,
	UART_BDR_1200	= 1200,
	UART_BDR_1800	= 1800,
	UART_BDR_2400	= 2400,
	UART_BDR_4800	= 4800,
	UART_BDR_9600	= 9600,
	UART_BDR_19200	= 19200,
	UART_BDR_38400	= 38400,
	UART_BDR_57600	= 57600,
	UART_BDR_115200	= 115200,
};

enum uart_data_bits {
	UART_DATA_BITS_5,
	UART_DATA_BITS_6,
	UART_DATA_BITS_7,
	UART_DATA_BITS_8,
};

enum uart_parity {
	UART_NO_PARITY,
	UART_ODD_PARITY,
	UART_EVENT_PARITY,
};

enum uart_stop_bits {
	UART_STOP_BIT_1,
	UART_STOP_BIT_2,
};

struct uart_cfg {
	enum uart_baudrate baudrate;
	enum uart_data_bits data_bits;
	enum uart_parity parity;
	enum uart_stop_bits stop_bits;
	uint8_t dma_en;
};

enum uart_event_type {
	UART_EVENT_TX_CMPL,
	UART_EVENT_RX_CMPL,
};

enum uart_err {
	UART_ERR_NO			= 0,
	UART_ERR_OVERRUN	= (1 << 0),
	UART_ERR_PARITY		= (1 << 1),
	UART_ERR_FRAMING	= (1 << 2),
	UART_ERR_LINE_BREAK = (1 << 3),
};

struct uart_event {
	enum uart_event_type type;
	uint8_t err;
};

typedef void (*uart_cb)(struct uart_event *event, void *ctx);

struct uart_init_arg {
	struct uart_cfg *cfg;
	uart_cb cb;
	void *cb_ctx;
};

struct uart_tx_arg {
	uint8_t *tx_buf;
	uint32_t tx_len;
};

struct uart_rx_arg {
	uint8_t *rx_buf;
	uint32_t rx_len;
};

struct uart_get_rx_len_arg {
	uint32_t *len;
};

/*
 * Low-level device driver
 */

struct uart_ops {
	int (*init)(struct device *dev, struct uart_cfg *cfg, uart_cb cb, void *ctx);
	int (*deinit)(struct device *dev);
	int (*transmit)(struct device *dev, uint8_t *tx_buf, uint32_t tx_len);
	int (*receive)(struct device *dev, uint8_t *rx_buf, uint32_t rx_len);
	int (*reset)(struct device *dev);
	int (*get_rx_len)(struct device *dev);
};

#define uart_ops(x)		((struct uart_ops *)(x)->driver->ops)

static __inline__ int uart_init(struct device *dev, struct uart_cfg *cfg, uart_cb cb, void *ctx)
{
	if (!dev)
		return -ENODEV;

	return uart_ops(dev)->init(dev, cfg, cb, ctx);
}

static __inline__ int uart_deinit(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	return uart_ops(dev)->deinit(dev);
}

static __inline__ int uart_tx(struct device *dev, uint8_t *tx_buf, uint32_t tx_len)
{
	if (!dev)
		return -ENODEV;

	return uart_ops(dev)->transmit(dev, tx_buf, tx_len);
}

static __inline__ int uart_rx(struct device *dev, uint8_t *rx_buf, uint32_t rx_len)
{
	if (!dev)
		return -ENODEV;

	return uart_ops(dev)->receive(dev, rx_buf, rx_len);
}

static __inline__ int uart_reset(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	return uart_ops(dev)->reset(dev);
}

static __inline__ int uart_get_rx_len(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	return uart_ops(dev)->get_rx_len(dev);
}

#ifdef __cplusplus
}
#endif

#endif //__UART_H__

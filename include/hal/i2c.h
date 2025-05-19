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
#ifndef __I2C_H__
#define __I2C_H__

#include <stdint.h>
#include <hal/device.h>
#include <hal/clk.h>
#include <freebsd/errors.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * High-level device driver
 */

#define IOCTL_I2C_CONFIGURE     (1)
#define IOCTL_I2C_RESET         (2)
#define IOCTL_I2C_MASTER_PROBE  (3)
#define IOCTL_I2C_MASTER_TX     (4)
#define IOCTL_I2C_MASTER_RX     (5)
#define IOCTL_I2C_MASTER_TX_RX  (6)
#define IOCTL_I2C_SLAVE_TX      (7)
#define IOCTL_I2C_SLAVE_RX      (8)

enum i2c_role {
	I2C_ROLE_MASTER,
	I2C_ROLE_SLAVE,
};

enum i2c_addr_len {
	I2C_ADDR_LEN_7BIT,
	I2C_ADDR_LEN_10BIT,
};

enum i2c_event_type {
	I2C_EVENT_MASTER_TRANS_CMPL,
	I2C_EVENT_SLAVE_RX_REQUESTED,
	I2C_EVENT_SLAVE_TX_REQUESTED,
	I2C_EVENT_SLAVE_RX_CMPL,
	I2C_EVENT_SLAVE_TX_CMPL,
};

struct i2c_event_master_trans_cmpl {
	int tx_len;
	int rx_len;
};

struct i2c_event_slave_tx_cmpl {
	int len;
	uint8_t truncated;
};

struct i2c_event_slave_rx_cmpl {
	int len;
	uint8_t truncated;
};

struct i2c_event {
	enum i2c_event_type type;
	union {
		struct i2c_event_master_trans_cmpl master_trans_cmpl;
		struct i2c_event_slave_tx_cmpl slave_tx_cmpl;
		struct i2c_event_slave_rx_cmpl slave_rx_cmpl;
	} data;
};

typedef void (*i2c_cb)(struct i2c_event *event, void *ctx);

struct i2c_cfg {
	enum i2c_role role;
	enum i2c_addr_len addr_len;
	uint32_t bitrate; /* Kbps */
	uint16_t slave_addr;
	uint8_t dma_en;
	uint8_t pull_up_en;
};

struct i2c_cfg_arg {
	struct i2c_cfg *cfg;
	i2c_cb cb;
	void *cb_ctx;
};

struct i2c_master_probe_arg {
	uint16_t addr;
	uint8_t *status;
};

struct i2c_master_tx_arg {
	uint16_t addr;
	uint8_t *tx_buf;
	uint32_t tx_len;
};

struct i2c_master_rx_arg {
	uint16_t addr;
	uint8_t *rx_buf;
	uint32_t rx_len;
};

struct i2c_master_tx_rx_arg {
	uint16_t addr;
	uint8_t *tx_buf;
	uint32_t tx_len;
	uint8_t *rx_buf;
	uint32_t rx_len;
};

struct i2c_slave_tx_arg {
	uint8_t *tx_buf;
	uint32_t tx_len;
};

struct i2c_slave_rx_arg {
	uint8_t *rx_buf;
	uint32_t rx_len;
};

/*
 * Low-level device driver
 */


struct i2c_ops {
	int (*configure)(struct device *dev, struct i2c_cfg *cfg, i2c_cb cb, void *ctx);
	int (*reset)(struct device *dev);
	int (*master_probe)(struct device *dev, uint16_t addr, uint8_t *status);
	int (*master_tx)(struct device *dev, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len);
	int (*master_rx)(struct device *dev, uint16_t addr, uint8_t *rx_buf, uint32_t rx_len);
	int (*master_tx_rx)(struct device *dev, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len);
	int (*slave_tx)(struct device *dev, uint8_t *tx_buf, uint32_t tx_len);
	int (*slave_rx)(struct device *dev, uint8_t *rx_buf, uint32_t rx_len);
};

#define i2c_ops(x)  ((struct i2c_ops *)(x)->driver->ops)

static __inline__ int i2c_configure(struct device *dev, struct i2c_cfg *cfg, i2c_cb cb, void *ctx)
{
	if (!dev)
		return -ENODEV;

	if (!i2c_ops(dev)->configure)
		return -ENOSYS;

	return i2c_ops(dev)->configure(dev, cfg, cb, ctx);
}

static __inline__ int i2c_reset(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	if (!i2c_ops(dev)->reset)
		return -ENOSYS;

	return i2c_ops(dev)->reset(dev);
}

static __inline__ int i2c_master_probe(struct device *dev, uint16_t addr, uint8_t *status)
{
	if (!dev)
		return -ENODEV;

	if (!i2c_ops(dev)->master_probe)
		return -ENOSYS;

	return i2c_ops(dev)->master_probe(dev, addr, status);
}

static __inline__ u32 i2c_master_tx(struct device *dev, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len)
{
	if (!dev)
		return 0;

	if (!i2c_ops(dev)->master_tx)
		return -ENOSYS;

	return i2c_ops(dev)->master_tx(dev, addr, tx_buf, tx_len);
}

static __inline__ u32 i2c_master_rx(struct device *dev, uint16_t addr, uint8_t *rx_buf, uint32_t rx_len)
{
	if (!dev)
		return 0;

	if (!i2c_ops(dev)->master_rx)
		return -ENOSYS;

	return i2c_ops(dev)->master_rx(dev, addr, rx_buf, rx_len);
}

static __inline__ u32 i2c_master_tx_rx(struct device *dev,
									   uint16_t addr,
									   uint8_t *tx_buf, uint32_t tx_len,
									   uint8_t *rx_buf, uint32_t rx_len)
{
	if (!dev)
		return 0;

	if (!i2c_ops(dev)->master_tx_rx)
		return -ENOSYS;

	return i2c_ops(dev)->master_tx_rx(dev, addr, tx_buf, tx_len, rx_buf, rx_len);
}

static __inline__ u32 i2c_slave_tx(struct device *dev, uint8_t *tx_buf, uint32_t tx_len)
{
	if (!dev)
		return 0;

	if (!i2c_ops(dev)->slave_tx)
		return -ENOSYS;

	return i2c_ops(dev)->slave_tx(dev, tx_buf, tx_len);
}

static __inline__ u32 i2c_slave_rx(struct device *dev, uint8_t *rx_buf, uint32_t rx_len)
{
	if (!dev)
		return 0;

	if (!i2c_ops(dev)->slave_rx)
		return -ENOSYS;

	return i2c_ops(dev)->slave_rx(dev, rx_buf, rx_len);
}

#ifdef __cplusplus
}
#endif

#endif

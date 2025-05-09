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

#include <stdio.h>
#include <hal/init.h>
#include <hal/kernel.h>
#include <hal/pinctrl.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/i2c.h>
#include <hal/kmem.h>
#include <hal/dma.h>

#include "vfs.h"
#include "mmap.h"

#define OFT_ATCI2C_ID                       0x00
#define OFT_ATCI2C_CFG                      0x10
#define OFT_ATCI2C_INTEN                    0x14
#define OFT_ATCI2C_STATUS                   0x18
#define OFT_ATCI2C_ADDR                     0x1c
#define OFT_ATCI2C_DATA                     0x20
#define OFT_ATCI2C_CTRL                     0x24
#define OFT_ATCI2C_CMD                      0x28
#define OFT_ATCI2C_SETUP                    0x2c
#define OFT_ATCI2C_TPM                      0x30

#define I2C_ATCI2C_INTEN_CMPL               (1 << 9)
#define I2C_ATCI2C_INTEN_BYTERECV           (1 << 8)
#define I2C_ATCI2C_INTEN_BYTETRANS          (1 << 7)
#define I2C_ATCI2C_INTEN_START              (1 << 6)
#define I2C_ATCI2C_INTEN_STOP               (1 << 5)
#define I2C_ATCI2C_INTEN_ARBLOSE            (1 << 4)
#define I2C_ATCI2C_INTEN_ADDRHIT            (1 << 3)
#define I2C_ATCI2C_INTEN_FIFOHALF           (1 << 2)
#define I2C_ATCI2C_INTEN_FIFOFULL           (1 << 1)
#define I2C_ATCI2C_INTEN_FIFOEMPTY          (1 << 0)

#define I2C_ATCI2C_STATUS_LINESDA           (1 << 14)
#define I2C_ATCI2C_STATUS_LINESCL           (1 << 13)
#define I2C_ATCI2C_STATUS_GENCALL           (1 << 12)
#define I2C_ATCI2C_STATUS_BUSBUSY           (1 << 11)
#define I2C_ATCI2C_STATUS_ACK               (1 << 10)
#define I2C_ATCI2C_STATUS_CMPL              (1 << 9)
#define I2C_ATCI2C_STATUS_BYTERECV          (1 << 8)
#define I2C_ATCI2C_STATUS_BYTETRANS         (1 << 7)
#define I2C_ATCI2C_STATUS_START             (1 << 6)
#define I2C_ATCI2C_STATUS_STOP              (1 << 5)
#define I2C_ATCI2C_STATUS_ARBLOSS           (1 << 4)
#define I2C_ATCI2C_STATUS_ADDRHIT           (1 << 3)
#define I2C_ATCI2C_STATUS_FIFOHALF          (1 << 2)
#define I2C_ATCI2C_STATUS_FIFOFULL          (1 << 1)
#define I2C_ATCI2C_STATUS_FIFOEMPTY         (1 << 0)

#define I2C_ATCI2C_CTRL_PHASE_START         (1 << 12)
#define I2C_ATCI2C_CTRL_PHASE_ADDR          (1 << 11)
#define I2C_ATCI2C_CTRL_PHASE_DATA          (1 << 10)
#define I2C_ATCI2C_CTRL_PHASE_STOP          (1 << 9)
#define I2C_ATCI2C_CTRL_DIR_MASTER_TX       (0 << 8)
#define I2C_ATCI2C_CTRL_DIR_MASTER_RX       (1 << 8)
#define I2C_ATCI2C_CTRL_DIR_SLAVE_RX        (0 << 8)
#define I2C_ATCI2C_CTRL_DIR_SLAVE_TX        (1 << 8)
#define I2C_ATCI2C_CTRL_DATACNT(cnt)        (cnt & 0xFF)

#define I2C_ATCI2C_CMD_NO_ACTION            (0)
#define I2C_ATCI2C_CMD_MASTER_TRANS         (1)
#define I2C_ATCI2C_CMD_ACK                  (2)
#define I2C_ATCI2C_CMD_NACK                 (3)
#define I2C_ATCI2C_CMD_CLEAR_FIFO           (4)
#define I2C_ATCI2C_CMD_RESET                (5)

#define I2C_ATCI2C_SETUP_T_SUDAT(time)      (time << 24)
#define I2C_ATCI2C_SETUP_T_SP(time)         (time << 21)
#define I2C_ATCI2C_SETUP_T_HDDAT(time)      (time << 16)
#define I2C_ATCI2C_SETUP_T_SCLRATIO(ratio)  (ratio << 13)
#define I2C_ATCI2C_SETUP_T_SCLHI(time)      (time << 4)
#define I2C_ATCI2C_SETUP_DMA_EN             (1 << 3)
#define I2C_ATCI2C_SETUP_MASTER             (1 << 2)
#define I2C_ATCI2C_SETUP_SLAVE              (0 << 2)
#define I2C_ATCI2C_SETUP_ADDR_LEN_10BIT     (1 << 1)
#define I2C_ATCI2C_SETUP_ADDR_LEN_7BIT      (0 << 1)
#define I2C_ATCI2C_SETUP_IICEN              (1 << 0)

#ifdef CONFIG_SOC_SCM2010
#define ATCI2C_FIFO_SIZE                    (4)
#endif

#define ATCI2C_TRANS_MAX					(256)
#define ATCI2C_DATA_CNT_MASK				(0xFF)

#define ATCI2C_DEFAULT_PCLK                 (40 * 1000 * 1000)
#define ATCI2C_DEFAULT_BITRATE              (400 * 1000)

#define atci2c_writel(v, o)                 writel(v, dev->base[0] + o)
#define atci2c_readl(o)                     readl(dev->base[0] + o)

enum i2c_state {
	I2C_STATE_UNKNOWN,
	I2C_STATE_MASTER_IDLE,
	I2C_STATE_MASTER_TX,
	I2C_STATE_MASTER_RX,
	I2C_STATE_MASTER_TX_RX,
	I2C_STATE_SLAVE_IDLE,
	I2C_STATE_SLAVE_TX,
	I2C_STATE_SLAVE_RX,
};

struct i2c_driver_data {
	struct fops devfs_ops;
	struct device *dev;
	struct pinctrl_pin_map *pin_scl;
	struct pinctrl_pin_map *pin_sda;
	enum i2c_role role;
	uint8_t dma_en;
	uint8_t *tx_buf;
	uint32_t tx_len;
	uint32_t tx_offset;
	uint8_t *rx_buf;
	uint32_t rx_len;
	uint32_t rx_offset;
	uint16_t slave_addr;
	enum i2c_state state;
	struct i2c_event event;
	i2c_cb cb;
	void *cb_ctx;

	struct device *dma_dev;
	uint8_t dma_hw_req;
	int dma_ch;
};

static int i2c_dma_config(struct device *dev, uint8_t *buf, int len, bool is_tx)
{
	struct i2c_driver_data *priv = dev->driver_data;
	struct dma_ctrl ctrl;
	struct dma_desc_chain dma_desc;
	int ret;

	if (!priv->dma_dev) {
		return -EIO;
	}

	if (!((uint32_t)buf & 0xF0000000)) {
		return -EINVAL;
	}

	priv->dma_ch = -1;

	if (is_tx) {
		ctrl.src_mode = DMA_MODE_NORMAL;
		ctrl.dst_mode = DMA_MODE_HANDSHAKE;
		ctrl.src_req = 0;
		ctrl.dst_req = priv->dma_hw_req;
		ctrl.src_addr_ctrl = DMA_ADDR_CTRL_INCREMENT;
		ctrl.dst_addr_ctrl = DMA_ADDR_CTRL_FIXED;
		ctrl.src_width = DMA_WIDTH_BYTE;
		ctrl.dst_width = DMA_WIDTH_BYTE;
		ctrl.intr_mask = DMA_INTR_TC_MASK | DMA_INTR_ABT_MASK;
		ctrl.src_burst_size = DMA_SRC_BURST_SIZE_1;

		dma_desc.src_addr = (uint32_t)buf;
		dma_desc.dst_addr = (uint32_t)(dev->base[0] + OFT_ATCI2C_DATA);
		dma_desc.len = len;

		ret = dma_copy_hw(priv->dma_dev, false, &ctrl, &dma_desc, 1, NULL, NULL, &priv->dma_ch);
		if (ret) {
			return ret;
		}
	} else {
		ctrl.src_mode = DMA_MODE_HANDSHAKE;
		ctrl.dst_mode = DMA_MODE_NORMAL;
		ctrl.src_req = priv->dma_hw_req;
		ctrl.dst_req = 0;
		ctrl.src_addr_ctrl = DMA_ADDR_CTRL_FIXED;
		ctrl.dst_addr_ctrl = DMA_ADDR_CTRL_INCREMENT;
		ctrl.src_width = DMA_WIDTH_BYTE;
		ctrl.dst_width = DMA_WIDTH_BYTE;
		ctrl.intr_mask = DMA_INTR_TC_MASK | DMA_INTR_ABT_MASK;
		ctrl.src_burst_size = DMA_SRC_BURST_SIZE_1;

		dma_desc.src_addr = (uint32_t)(dev->base[0] + OFT_ATCI2C_DATA);
		dma_desc.dst_addr = (uint32_t)buf;
		dma_desc.len = len;

		ret = dma_copy_hw(priv->dma_dev, false, &ctrl, &dma_desc, 1, NULL, NULL, &priv->dma_ch);
		if (ret) {
			return ret;
		}
	}

	if (priv->role == I2C_ROLE_SLAVE) {
		uint32_t v;

		v = atci2c_readl(OFT_ATCI2C_SETUP);
		v |= I2C_ATCI2C_SETUP_DMA_EN;
		atci2c_writel(v, OFT_ATCI2C_SETUP);

		v = atci2c_readl(OFT_ATCI2C_CTRL);
		v &= ~ATCI2C_DATA_CNT_MASK;
		v |= I2C_ATCI2C_CTRL_DATACNT(len);
		atci2c_writel(v, OFT_ATCI2C_CTRL);
	}

	return 0;
}

static int i2c_master_start(struct device *dev)
{
	struct i2c_driver_data *priv = dev->driver_data;
	uint32_t v;
	uint32_t stop_phase = 0;
	uint32_t fifo_int = 0;
	uint32_t trans_dir = 0;
	uint32_t len = 0;
	int ret;

	/* set ADDR, INTEN, CTRL registers based on the request */
	if (priv->state == I2C_STATE_MASTER_TX) {
		stop_phase = I2C_ATCI2C_CTRL_PHASE_STOP;
		if (!priv->dma_en) {
			fifo_int = I2C_ATCI2C_INTEN_FIFOEMPTY;
		} else {
			ret = i2c_dma_config(dev, priv->tx_buf, priv->tx_len, true);
			if (ret) {
				return ret;
			}
		}
		trans_dir = I2C_ATCI2C_CTRL_DIR_MASTER_TX;
		len = priv->tx_len;
	} else if (priv->state == I2C_STATE_MASTER_TX_RX) {
		stop_phase = 0;
		if (!priv->dma_en) {
			fifo_int = I2C_ATCI2C_INTEN_FIFOEMPTY;
		} else {
			ret = i2c_dma_config(dev, priv->tx_buf, priv->tx_len, true);
			if (ret) {
				return ret;
			}
		}
		trans_dir = I2C_ATCI2C_CTRL_DIR_MASTER_TX;
		len = priv->tx_len;
	} else if (priv->state == I2C_STATE_MASTER_RX) {
		stop_phase = I2C_ATCI2C_CTRL_PHASE_STOP;
		if (!priv->dma_en) {
			fifo_int = I2C_ATCI2C_INTEN_FIFOFULL;
		} else {
			ret = i2c_dma_config(dev, priv->rx_buf, priv->rx_len, false);
			if (ret) {
				return ret;
			}
		}
		trans_dir = I2C_ATCI2C_CTRL_DIR_MASTER_RX;
		len = priv->rx_len;
	}

	v = priv->slave_addr;
	atci2c_writel(v, OFT_ATCI2C_ADDR);

	v = I2C_ATCI2C_INTEN_CMPL | \
		I2C_ATCI2C_INTEN_ARBLOSE | \
		fifo_int;
	atci2c_writel(v, OFT_ATCI2C_INTEN);

	v = I2C_ATCI2C_CTRL_PHASE_START | \
		I2C_ATCI2C_CTRL_PHASE_ADDR | \
		I2C_ATCI2C_CTRL_PHASE_DATA | \
		stop_phase | \
		trans_dir |
		I2C_ATCI2C_CTRL_DATACNT(len);
	atci2c_writel(v, OFT_ATCI2C_CTRL);

	/* start the transaction */
	v = I2C_ATCI2C_CMD_MASTER_TRANS;
	atci2c_writel(v, OFT_ATCI2C_CMD);

	return 0;
}

static void i2c_cmpl(struct device *dev)
{
	struct i2c_driver_data *priv = dev->driver_data;
	uint32_t v;

	if (priv->cb) {
		priv->cb(&priv->event, priv->cb_ctx);
	}

	if (priv->role == I2C_ROLE_MASTER) {
		v = 0;
		atci2c_writel(v, OFT_ATCI2C_INTEN);

		priv->state = I2C_STATE_MASTER_IDLE;
	} else {
		v = I2C_ATCI2C_CMD_CLEAR_FIFO;
		atci2c_writel(v, OFT_ATCI2C_CMD);

		v = I2C_ATCI2C_INTEN_CMPL | \
			I2C_ATCI2C_INTEN_ADDRHIT;
		atci2c_writel(v, OFT_ATCI2C_INTEN);

		priv->state = I2C_STATE_SLAVE_IDLE;

		if (priv->dma_en) {
			v = atci2c_readl(OFT_ATCI2C_SETUP);
			v &= ~I2C_ATCI2C_SETUP_DMA_EN;
			atci2c_writel(v, OFT_ATCI2C_SETUP);
		}
	}
}

static int i2c_irq(int irq, void *data)
{
	struct device *dev = (struct device *)data;
	struct i2c_driver_data *priv = dev->driver_data;
	uint32_t status;
	uint32_t v;
	int len;

	status = atci2c_readl(OFT_ATCI2C_STATUS);

	switch (priv->state) {

	case I2C_STATE_MASTER_TX:
	case I2C_STATE_MASTER_TX_RX:
		if ((status & I2C_ATCI2C_STATUS_FIFOEMPTY) && !priv->dma_en) {
			/* put fifo until full or tx length reached */
			while (1) {
				if (priv->tx_offset == priv->tx_len) {
					break;
				}

				v = priv->tx_buf[priv->tx_offset];
				atci2c_writel(v, OFT_ATCI2C_DATA);
				priv->tx_offset++;

				v = atci2c_readl(OFT_ATCI2C_STATUS);
				if (v & I2C_ATCI2C_STATUS_FIFOFULL) {
					v = I2C_ATCI2C_CMD_MASTER_TRANS;
					atci2c_writel(v, OFT_ATCI2C_CMD);
					break;
				}
			}
		}
		if (status & I2C_ATCI2C_STATUS_CMPL) {
			v = atci2c_readl(OFT_ATCI2C_CTRL);
			len = I2C_ATCI2C_CTRL_DATACNT(v);
			priv->event.data.master_trans_cmpl.tx_len = priv->tx_len - len;
			if (priv->dma_ch >= 0) {
				dma_ch_rel(priv->dma_dev, priv->dma_ch);
				priv->dma_ch = -1;
			}
			/* tx complete, move to tx_rx or complete */
			if (priv->state == I2C_STATE_MASTER_TX_RX) {
				v = I2C_ATCI2C_STATUS_CMPL;
				atci2c_writel(v, OFT_ATCI2C_STATUS);
				priv->state = I2C_STATE_MASTER_RX;
				i2c_master_start(dev);
				return 0;
			} else {
				priv->event.type = I2C_EVENT_MASTER_TRANS_CMPL;
				i2c_cmpl(dev);
			}
		}
	break;

	case I2C_STATE_MASTER_RX:
		if (status & I2C_ATCI2C_STATUS_FIFOFULL && !priv->dma_en) {
			/* get fifo until empty or rx length reached */
			while (1) {
				if (priv->rx_offset == priv->rx_len) {
					break;
				}
				priv->rx_buf[priv->rx_offset] = atci2c_readl(OFT_ATCI2C_DATA);
				priv->rx_offset++;
				v = atci2c_readl(OFT_ATCI2C_STATUS);
				if (v & I2C_ATCI2C_STATUS_FIFOEMPTY) {
					v = I2C_ATCI2C_CMD_MASTER_TRANS;
					atci2c_writel(v, OFT_ATCI2C_CMD);
					break;
				}
			}
		}
		if (status & I2C_ATCI2C_STATUS_CMPL) {
			v = atci2c_readl(OFT_ATCI2C_CTRL);
			len = I2C_ATCI2C_CTRL_DATACNT(v);
			/* rx complete, get left over fifo, and complete */
			if (!priv->dma_en) {
				while (1) {
					v = atci2c_readl(OFT_ATCI2C_STATUS);
					if (v & I2C_ATCI2C_STATUS_FIFOEMPTY) {
						break;
					}
					if (priv->rx_offset == priv->rx_len) {
						break;
					}
					priv->rx_buf[priv->rx_offset] = atci2c_readl(OFT_ATCI2C_DATA);
					priv->rx_offset++;
				}
				priv->event.type = I2C_EVENT_MASTER_TRANS_CMPL;
				priv->event.data.master_trans_cmpl.rx_len = priv->rx_len - len;
			}
			i2c_cmpl(dev);

			if (priv->dma_ch >= 0) {
				dma_ch_rel(priv->dma_dev, priv->dma_ch);
				priv->dma_ch = -1;
			}
		}
	break;

	case I2C_STATE_SLAVE_IDLE:
		if (status & I2C_ATCI2C_STATUS_ADDRHIT) {
			/* read the direction and request data to the upper layer */
			v = atci2c_readl(OFT_ATCI2C_CTRL);

			if (v & I2C_ATCI2C_CTRL_DIR_SLAVE_TX) {
				if (!priv->dma_en) {
					v = atci2c_readl(OFT_ATCI2C_INTEN);
					v |= I2C_ATCI2C_INTEN_FIFOEMPTY;
					atci2c_writel(v, OFT_ATCI2C_INTEN);
				}
				if (priv->cb) {
					priv->event.type = I2C_EVENT_SLAVE_TX_REQUESTED;
					priv->cb(&priv->event, priv->cb_ctx);
				}
				if (priv->state == I2C_STATE_SLAVE_IDLE) {
					v = I2C_ATCI2C_CMD_RESET;
					atci2c_writel(v, OFT_ATCI2C_CMD);
					priv->event.type = I2C_EVENT_SLAVE_TX_CMPL;
					priv->event.data.slave_tx_cmpl.len = 0;
					printk("i2c slave error, event %d\n", priv->event.type);
					i2c_cmpl(dev);
				}
			} else {
				if (!priv->dma_en) {
					v = atci2c_readl(OFT_ATCI2C_INTEN);
					v |= I2C_ATCI2C_INTEN_FIFOFULL;
					atci2c_writel(v, OFT_ATCI2C_INTEN);
				}
				if (priv->cb) {
					priv->event.type = I2C_EVENT_SLAVE_RX_REQUESTED;
					priv->cb(&priv->event, priv->cb_ctx);
				}
				if (priv->state == I2C_STATE_SLAVE_IDLE) {
					v = I2C_ATCI2C_CMD_RESET;
					atci2c_writel(v, OFT_ATCI2C_CMD);
					priv->event.type = I2C_EVENT_SLAVE_RX_CMPL;
					priv->event.data.slave_rx_cmpl.len = 0;
					printk("i2c slave error, event %d\n", priv->event.type);
					i2c_cmpl(dev);
				}
			}
			/*
			 * isr handler should call slave_tx or slave_rx so that the state changes
			 * if still in SLAVE_IDLE, means something went wrong
			 */
		}
	break;

	case I2C_STATE_SLAVE_TX:
		if (status & I2C_ATCI2C_STATUS_FIFOEMPTY && !priv->dma_en) {
			/* put fifo until full or tx length reached */
			while (1) {
				if (priv->tx_offset < priv->tx_len) {
					v = priv->tx_buf[priv->tx_offset];
				} else {
					v = 0;
				}
				atci2c_writel(v, OFT_ATCI2C_DATA);
				priv->tx_offset++;
				v = atci2c_readl(OFT_ATCI2C_STATUS);
				if (v & I2C_ATCI2C_STATUS_FIFOFULL) {
					break;
				}
			}
		}
		if (status & I2C_ATCI2C_STATUS_CMPL) {
			/* tx complete */
			priv->event.type = I2C_EVENT_SLAVE_TX_CMPL;
			v = atci2c_readl(OFT_ATCI2C_CTRL);
			len = I2C_ATCI2C_CTRL_DATACNT(v);
			if (priv->dma_en) {
				len = priv->tx_len - len;
			} else if (len == 0) {
				len = ATCI2C_TRANS_MAX;
			}
			if (len <= priv->tx_len) {
				priv->event.data.slave_tx_cmpl.len = len;
				priv->event.data.slave_tx_cmpl.truncated = 0;
			} else {
				priv->event.data.slave_tx_cmpl.len = priv->tx_len;
				priv->event.data.slave_tx_cmpl.truncated = 1;
			}
			i2c_cmpl(dev);

			if (priv->dma_ch >= 0) {
				dma_ch_rel(priv->dma_dev, priv->dma_ch);
				priv->dma_ch = -1;
			}
		}
	break;

	case I2C_STATE_SLAVE_RX:
		if (status & I2C_ATCI2C_STATUS_FIFOFULL && !priv->dma_en) {
			/* get fifo until empty or rx length reached */
			while (1) {
				v = atci2c_readl(OFT_ATCI2C_DATA);
				if (priv->rx_offset < priv->rx_len) {
					priv->rx_buf[priv->rx_offset] = v;
				}
				priv->rx_offset++;

				v = atci2c_readl(OFT_ATCI2C_STATUS);
				if (v & I2C_ATCI2C_STATUS_FIFOEMPTY) {
					break;
				}
			}
		}
		if (status & I2C_ATCI2C_STATUS_CMPL) {
			/* rx complete, get left over fifo, complete */
			if (!priv->dma_en) {
				while (1) {
					v = atci2c_readl(OFT_ATCI2C_STATUS);
					if (v & I2C_ATCI2C_STATUS_FIFOEMPTY) {
						break;
					}
					v = atci2c_readl(OFT_ATCI2C_DATA);
					if (priv->rx_offset < priv->rx_len) {
						priv->rx_buf[priv->rx_offset] = v;
					}
					priv->rx_offset++;
				}
			}

			priv->event.type = I2C_EVENT_SLAVE_RX_CMPL;
			v = atci2c_readl(OFT_ATCI2C_CTRL);
			len = I2C_ATCI2C_CTRL_DATACNT(v);
			if (priv->dma_en) {
				len = priv->rx_len - len;
			} else if (len == 0) {
				len = ATCI2C_TRANS_MAX;
			}
			if (len <= priv->rx_len) {
				v = atci2c_readl(OFT_ATCI2C_CTRL);
				priv->event.data.slave_rx_cmpl.len = len;
				priv->event.data.slave_rx_cmpl.truncated = 0;
			} else {
				priv->event.data.slave_rx_cmpl.len = priv->rx_len;
				priv->event.data.slave_rx_cmpl.truncated = 1;
			}
			i2c_cmpl(dev);

			if (priv->dma_ch >= 0) {
				dma_ch_rel(priv->dma_dev, priv->dma_ch);
				priv->dma_ch = -1;
			}
		}

	break;

	default:
		assert(0);
	break;
	}

	atci2c_writel(status, OFT_ATCI2C_STATUS);

	return 0;
}

#define is_standard(b)     (((b) <= 100) ? true : false) /* ~100Kbps */
#define is_fast(b)         (((b) > 100 && (b) <= 400) ? true : false) /* ~400Kbps */
#define is_fast_plus(b)    (((b) > 400 && (b) <= 1000) ? true : false) /* ~1Mbps */
#define ROUNDUP_F2I(x)     ((x) >= 0 ? (int)((x) + 0.5) : (int)((x) - 0.5))

static void atci2c_i2c_config_timing(struct device *dev, struct i2c_cfg *cfg)
{
    struct clk *clk;
    int TPM, T_SCLHi, T_SCLRatio, T_SP;
    int T_SUDAT;
    int T_HDDAT;
    int bitrate = cfg->bitrate, ratio, pclk;
    float TPM_f, T_SP_f, T_SCLHi_f;
    float desired_setup, desired_hold, T_SUDAT_f, T_HDDAT_f, tpclk;
    uint32_t v;

    assert(bitrate != 0);

    /* Refer to https://github.com/Senscomm/wise/issues/2662#issuecomment-2450567384 */

    /* As an I2C slave, the spike suppression width, the data setup time
     * and the data hold time must be programmed properly.
     * As an I2C master, the I2C-bus clock frequency must be programmed as well.
     */
    /* Configure the same timing parameters both for master and slave.
     */

    clk = clk_get(dev, "pclk");
    pclk = clk_get_rate(clk);

    pclk  = pclk / 1000;
    bitrate = bitrate / 1000;

    tpclk = 1 / (float)pclk;

    /* T_SCLRatio */

    if (is_standard(bitrate)) {
        T_SCLRatio = 0;
    } else {
        T_SCLRatio = 1;
    }

    ratio = (T_SCLRatio == 0 ? 1 : 2);

    /* TPM */

    TPM_f = (((float)ratio / (2 * (float)bitrate)) - (2 * tpclk))
        / ((2 + 1 + 511/*0x1ff*/ + (float)T_SCLRatio) * tpclk) - 1;

    if (TPM_f < 0) {
        TPM_f = 0.0;
        TPM = 0;
    } else {
        TPM = ROUNDUP_F2I(TPM_f);
        TPM = min(TPM, 0x1f); /* 5bits long. */
    }

    /* T_SP */

    if (is_standard(bitrate)) {
        T_SP_f = 1.0;
        T_SP = 1;
    } else {
        T_SP_f = 0.00005 / (tpclk * ((float)TPM + 1));
        T_SP = ROUNDUP_F2I(T_SP_f);
    }

    /* T_SCLHi */

    T_SCLHi_f = ((1 / (2 * (float)bitrate)) - (2 * tpclk))
        / (tpclk * ((float)TPM + 1)) - (2 + (float)T_SP);
    T_SCLHi = ROUNDUP_F2I(T_SCLHi_f);
    T_SCLHi = min(T_SCLHi, 0x1ff); /* 9bits long. */

    /* T_SUDAT */

    desired_setup = 0.00025; /* 250 ns */
    if (is_fast(bitrate)) {
        desired_setup = 0.0001;  /* 100 ns */
    } else if (is_fast_plus(bitrate)) {
        desired_setup = 0.00005;  /* 50 ns */
    }
    T_SUDAT_f = ((desired_setup - 2 * tpclk) / (tpclk * ((float)TPM + 1)))\
                - (2 + (float)T_SP);
    if (T_SUDAT_f < 0) {
        T_SUDAT_f = 0.0;
        T_SUDAT = 0;
    } else {
        T_SUDAT = ROUNDUP_F2I(T_SUDAT_f);
        T_SUDAT = min(T_SUDAT, 0x1f); /* 5bits long. */
    }

    /* T_HDDAT */

    if (is_fast_plus(bitrate)) {
        T_HDDAT_f = 0.0;
        T_HDDAT = 0;
    } else {
        desired_hold = 0.0003; /* 300 ns */
        T_HDDAT_f = ((desired_hold - 2 * tpclk) / (tpclk * ((float)TPM + 1)))\
                    - (2 + (float)T_SP);
        if (T_HDDAT_f < 0) {
            T_HDDAT_f = 0.0;
            T_HDDAT = 0;
        } else {
            T_HDDAT = ROUNDUP_F2I(T_HDDAT_f);
            T_HDDAT = min(T_HDDAT, 0x1f); /* 5bits long. */
        }
    }

#if 0
    printf("T_SCLRatio:\t %d\n", T_SCLRatio);
    printf("TPM:\t\t %d\n", TPM);
    printf("T_SP:\t\t %d\n", T_SP);
    printf("T_SCLHi:\t %d\n", T_SCLHi);
    printf("T_SUDAT:\t %d\n", T_SUDAT);
    printf("T_HDDAT:\t %d\n", T_HDDAT);
#endif

    /* program timing multiplier */
    atci2c_writel((TPM & 0x1f), OFT_ATCI2C_TPM);

    v = atci2c_readl(OFT_ATCI2C_SETUP);
    v &= ~(I2C_ATCI2C_SETUP_T_SCLHI(0x1ff) | I2C_ATCI2C_SETUP_T_SP(0x7)
            | I2C_ATCI2C_SETUP_T_SCLRATIO(1) | I2C_ATCI2C_SETUP_T_SUDAT(0x1f)
            | I2C_ATCI2C_SETUP_T_HDDAT(0x1f));
    v |= I2C_ATCI2C_SETUP_T_SP(T_SP);
    v |= I2C_ATCI2C_SETUP_T_SCLHI(T_SCLHi);
    v |= I2C_ATCI2C_SETUP_T_SCLRATIO(T_SCLRatio);
    v |= I2C_ATCI2C_SETUP_T_SUDAT(T_SUDAT);
    v |= I2C_ATCI2C_SETUP_T_HDDAT(T_HDDAT);
	atci2c_writel(v, OFT_ATCI2C_SETUP);
}

static int atci2c_i2c_configure(struct device *dev, struct i2c_cfg *cfg, i2c_cb cb, void *ctx)
{
	struct i2c_driver_data *priv = dev->driver_data;
	uint32_t v;

	if (cfg->dma_en && !priv->dma_dev) {
		printk("DMA is not enabled\n");
		return -EINVAL;
	}

	/* XXX: is this working?? */
	v = readl(GPIO_BASE_ADDR + 4);
	if (cfg->pull_up_en) {
		v |= (1 << priv->pin_scl->pin) | (1 << priv->pin_sda->pin);
	} else {
		v &= ~((1 << priv->pin_scl->pin) | (1 << priv->pin_sda->pin));
	}
	writel(v, GPIO_BASE_ADDR + 4);

	v = I2C_ATCI2C_CMD_RESET;
	atci2c_writel(v, OFT_ATCI2C_CMD);

	/* disable */
	v = atci2c_readl(OFT_ATCI2C_SETUP);
	v &= ~I2C_ATCI2C_SETUP_IICEN;
	atci2c_writel(v, OFT_ATCI2C_SETUP);

	atci2c_i2c_config_timing(dev, cfg);

	v = atci2c_readl(OFT_ATCI2C_SETUP);
	if (cfg->dma_en) {
		v |= I2C_ATCI2C_SETUP_DMA_EN;
	} else {
		v &= ~I2C_ATCI2C_SETUP_DMA_EN;
	}

	if (cfg->role == I2C_ROLE_MASTER) {
		v |= I2C_ATCI2C_SETUP_MASTER;
	} else {
		v &= ~I2C_ATCI2C_SETUP_MASTER;
	}

	if (cfg->addr_len == I2C_ADDR_LEN_10BIT) {
		v |= I2C_ATCI2C_SETUP_ADDR_LEN_10BIT;
	} else {
		v &= ~I2C_ATCI2C_SETUP_ADDR_LEN_10BIT;
	}
	v |= I2C_ATCI2C_SETUP_IICEN;
	atci2c_writel(v, OFT_ATCI2C_SETUP);

	if (cfg->role == I2C_ROLE_MASTER) {
		priv->state = I2C_STATE_MASTER_IDLE;
	} else {
		v = cfg->slave_addr;
		atci2c_writel(v, OFT_ATCI2C_ADDR);

		v = I2C_ATCI2C_INTEN_CMPL | \
			I2C_ATCI2C_INTEN_ADDRHIT;
		atci2c_writel(v, OFT_ATCI2C_INTEN);

		priv->state = I2C_STATE_SLAVE_IDLE;
	}

	priv->cb = cb;
	priv->cb_ctx = ctx;
	priv->dma_en = cfg->dma_en;
	priv->role = cfg->role;

	return 0;
}

static int atci2c_i2c_reset(struct device *dev)
{
	struct i2c_driver_data *priv = dev->driver_data;
	uint32_t v;

	if (priv->dma_en && priv->dma_ch >= 0) {
		dma_ch_abort(priv->dma_dev, priv->dma_ch);
	}

	v = I2C_ATCI2C_CMD_RESET;
	atci2c_writel(v, OFT_ATCI2C_CMD);

	return 0;
}

static int atci2c_i2c_master_probe(struct device *dev, uint16_t addr, uint8_t *status)
{
	struct i2c_driver_data *priv = dev->driver_data;
	uint32_t v;

	if (priv->role != I2C_ROLE_MASTER) {
		return -EINVAL;
	}

	/* try transaction without data phase to see if address is acked */
	v = I2C_ATCI2C_CTRL_PHASE_START | \
		I2C_ATCI2C_CTRL_PHASE_ADDR | \
		I2C_ATCI2C_CTRL_PHASE_STOP | \
		I2C_ATCI2C_CTRL_DIR_MASTER_TX |
		I2C_ATCI2C_CTRL_DATACNT(0);
	atci2c_writel(v, OFT_ATCI2C_CTRL);

	v = addr;
	atci2c_writel(v, OFT_ATCI2C_ADDR);

	v = I2C_ATCI2C_CMD_MASTER_TRANS;
	atci2c_writel(v, OFT_ATCI2C_CMD);

	while (1) {
		v = atci2c_readl(OFT_ATCI2C_STATUS);
		if (v & I2C_ATCI2C_STATUS_CMPL) {
			break;
		}
	}

	v |= I2C_ATCI2C_STATUS_CMPL;
	atci2c_writel(v, OFT_ATCI2C_STATUS);

	if (v & I2C_ATCI2C_STATUS_ACK) {
		*status = 1;
	} else {
		*status = 0;
	}

	return 0;
}

static int atci2c_i2c_master_tx(struct device *dev, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len)
{
	struct i2c_driver_data *priv = dev->driver_data;

	if (tx_len > ATCI2C_TRANS_MAX || tx_len == 0) {
		return -EINVAL;
	}

	priv->tx_buf = tx_buf;
	priv->tx_len = tx_len;
	priv->tx_offset = 0;
	priv->slave_addr = addr;
	priv->event.data.master_trans_cmpl.tx_len = 0;
	priv->event.data.master_trans_cmpl.rx_len = 0;
	priv->state = I2C_STATE_MASTER_TX;

	return i2c_master_start(dev);
}

static int atci2c_i2c_master_rx(struct device *dev, uint16_t addr, uint8_t *rx_buf, uint32_t rx_len)
{
	struct i2c_driver_data *priv = dev->driver_data;

	if (rx_len > ATCI2C_TRANS_MAX || rx_len == 0) {
		return -EINVAL;
	}

	priv->rx_buf = rx_buf;
	priv->rx_len = rx_len;
	priv->rx_offset = 0;
	priv->slave_addr = addr;
	priv->event.data.master_trans_cmpl.tx_len = 0;
	priv->event.data.master_trans_cmpl.rx_len = 0;
	priv->state = I2C_STATE_MASTER_RX;

	return i2c_master_start(dev);
}

static int atci2c_i2c_master_tx_rx(struct device *dev,
								   uint16_t addr,
								   uint8_t *tx_buf, uint32_t tx_len,
								   uint8_t *rx_buf, uint32_t rx_len)
{
	struct i2c_driver_data *priv = dev->driver_data;

	if (tx_len > ATCI2C_TRANS_MAX || tx_len == 0) {
		return -EINVAL;
	}
	if (rx_len > ATCI2C_TRANS_MAX || rx_len == 0) {
		return -EINVAL;
	}

	priv->tx_buf = tx_buf;
	priv->tx_len = tx_len;
	priv->tx_offset = 0;
	priv->rx_buf = rx_buf;
	priv->rx_len = rx_len;
	priv->rx_offset = 0;
	priv->slave_addr = addr;
	priv->event.data.master_trans_cmpl.tx_len = 0;
	priv->event.data.master_trans_cmpl.rx_len = 0;
	priv->state = I2C_STATE_MASTER_TX_RX;

	return i2c_master_start(dev);
}

static int atci2c_i2c_slave_tx(struct device *dev, uint8_t *tx_buf, uint32_t tx_len)
{
	struct i2c_driver_data *priv = dev->driver_data;
	int ret;

	if (tx_len > ATCI2C_TRANS_MAX || tx_len == 0) {
		return -EINVAL;
	}

	priv->tx_buf = tx_buf;
	priv->tx_len = tx_len;
	priv->tx_offset = 0;
	priv->state = I2C_STATE_SLAVE_TX;

	if (priv->dma_en) {
		ret = i2c_dma_config(dev, tx_buf, tx_len, true);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

static int atci2c_i2c_slave_rx(struct device *dev, uint8_t *rx_buf, uint32_t rx_len)
{
	struct i2c_driver_data *priv = dev->driver_data;
	int ret;

	if (rx_len > ATCI2C_TRANS_MAX || rx_len == 0) {
		return -EINVAL;
	}

	priv->rx_buf = rx_buf;
	priv->rx_len = rx_len;
	priv->rx_offset = 0;
	priv->state = I2C_STATE_SLAVE_RX;

	if (priv->dma_en) {
		ret = i2c_dma_config(dev, rx_buf, rx_len, false);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

static int atci2c_i2c_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct i2c_driver_data *priv = file->f_priv;
	struct device *dev = priv->dev;
	int ret = 0;

	switch (cmd) {
	case IOCTL_I2C_CONFIGURE: {
		struct i2c_cfg_arg *cfg_arg = arg;
		ret = i2c_ops(dev)->configure(dev, cfg_arg->cfg, cfg_arg->cb, cfg_arg->cb_ctx);
		break;
	}
	case IOCTL_I2C_RESET: {
		ret = i2c_ops(dev)->reset(dev);
		break;
	}
	case IOCTL_I2C_MASTER_PROBE: {
		struct i2c_master_probe_arg *probe_arg = arg;
		ret = i2c_ops(dev)->master_probe(dev, probe_arg->addr, probe_arg->status);
		break;
	}
	case IOCTL_I2C_MASTER_TX: {
		struct i2c_master_tx_arg *tx_arg = arg;
		ret = i2c_ops(dev)->master_tx(dev, tx_arg->addr, tx_arg->tx_buf, tx_arg->tx_len);
		break;
	}
	case IOCTL_I2C_MASTER_RX: {
		struct i2c_master_rx_arg *rx_arg = arg;
		ret = i2c_ops(dev)->master_rx(dev, rx_arg->addr, rx_arg->rx_buf, rx_arg->rx_len);
		break;
	}
	case IOCTL_I2C_MASTER_TX_RX: {
		struct i2c_master_tx_rx_arg *tx_rx_arg = arg;
		ret = i2c_ops(dev)->master_tx_rx(dev, tx_rx_arg->addr,
										 tx_rx_arg->tx_buf, tx_rx_arg->tx_len,
										 tx_rx_arg->rx_buf, tx_rx_arg->rx_len);
		break;
	}
	case IOCTL_I2C_SLAVE_TX: {
		struct i2c_slave_tx_arg *tx_arg = arg;
		ret = i2c_ops(dev)->slave_tx(dev, tx_arg->tx_buf, tx_arg->tx_len);
		break;
	}
	case IOCTL_I2C_SLAVE_RX: {
		struct i2c_slave_rx_arg *rx_arg = arg;
		ret = i2c_ops(dev)->slave_rx(dev, rx_arg->rx_buf, rx_arg->rx_len);
		break;
	}
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int atci2c_i2c_probe(struct device *dev)
{
	struct i2c_driver_data *priv;
	struct pinctrl_pin_map *pin_scl;
	struct pinctrl_pin_map *pin_sda;
	struct file *file;
	char buf[32];
	int idx;
	int ret;

	pin_scl = pinctrl_lookup_platform_pinmap(dev, "scl");
	pin_sda = pinctrl_lookup_platform_pinmap(dev, "sda");
	if (pin_scl == NULL || pin_sda == NULL) {
		printk("no pin available for %s\n", dev_name(dev));
		return -1;
	}

	ret = pinctrl_request_pin(dev, pin_scl->id, pin_scl->pin);
	if (ret) {
		goto free_pin;
	}

	ret = pinctrl_request_pin(dev, pin_sda->id, pin_sda->pin);
	if (ret) {
		goto free_pin;
	}

	ret = request_irq(dev->irq[0], i2c_irq, dev_name(dev), dev->pri[0], dev);
	if (ret) {
		goto free_pin;
	}

	priv = kmalloc(sizeof(*priv));
	if (!priv) {
		goto free_pin;
	}

	memset(priv, 0, sizeof(struct i2c_driver_data));

	priv->pin_scl = pin_scl;
	priv->pin_sda = pin_sda;

	priv->dev = dev;
	priv->devfs_ops.ioctl = atci2c_i2c_ioctl;

	idx = dev_id(dev);
	if (idx == 0) {
		priv->dma_hw_req = DMA0_HW_REQ_I2C0;
		priv->dma_dev = device_get_by_name("dmac.0");
		if (!priv->dma_dev) {
			printk("Not support DMA\n");
		}
	} else {
		priv->dma_hw_req = DMA1_HW_REQ_I2C1;
		priv->dma_dev = device_get_by_name("dmac.1");
		if (!priv->dma_dev) {
			printk("Not support DMA\n");
		}
	}

	sprintf(buf, "/dev/i2c%d", idx);

	file = vfs_register_device_file(buf, &priv->devfs_ops, priv);
	if (!file) {
		printk("%s: failed to register as %s\n", dev_name(dev), buf);
		free(priv);
		return -ENOSYS;
	}

	dev->driver_data = priv;

	printk("I2C: %s registered as %s\n", dev_name(dev), buf);

	return 0;

free_pin:
	pinctrl_free_pin(dev, pin_scl->id, pin_scl->pin);
	pinctrl_free_pin(dev, pin_sda->id, pin_sda->pin);

	return ret;
}

int atci2c_i2c_shutdown(struct device *dev)
{
	uint32_t v;

	v = atci2c_readl(OFT_ATCI2C_SETUP);
	v &= ~I2C_ATCI2C_SETUP_IICEN;
	atci2c_writel(v, OFT_ATCI2C_SETUP);

	free_irq(dev->irq[0], dev_name(dev));
	free(dev->driver_data);

	return 0;
}

#ifdef CONFIG_PM_DM
static int atci2c_i2c_suspend(struct device *dev, u32 *idle)
{
	return 0;
}

static int atci2c_i2c_resume(struct device *dev)
{
	return 0;
}
#endif

struct i2c_ops atci2c_i2c_ops = {
	.configure = atci2c_i2c_configure,
	.reset = atci2c_i2c_reset,
	.master_probe = atci2c_i2c_master_probe,
	.master_tx = atci2c_i2c_master_tx,
	.master_rx = atci2c_i2c_master_rx,
	.master_tx_rx = atci2c_i2c_master_tx_rx,
	.slave_tx = atci2c_i2c_slave_tx,
	.slave_rx = atci2c_i2c_slave_rx,
};

static declare_driver(i2c) = {
	.name       = "atci2c",
	.probe      = atci2c_i2c_probe,
	.shutdown   = atci2c_i2c_shutdown,
#ifdef CONFIG_PM_DM
	.suspend    = atci2c_i2c_suspend,
	.resume     = atci2c_i2c_resume,
#endif
	.ops        = &atci2c_i2c_ops,
};

#ifdef CONFIG_SOC_SCM2010
#if !defined(CONFIG_USE_I2C0) && !defined(CONFIG_USE_I2C1)
#error I2C driver requires I2C devices. Select I2C devices or remove the driver
#endif
#endif

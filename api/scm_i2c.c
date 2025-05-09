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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <cmsis_os.h>
#include "hal/i2c.h"

#include "sys/ioctl.h"
#include "scm_i2c.h"

struct scm_i2c_data {
	int fd;
	char *dev_name;
	uint8_t sync;
	osSemaphoreId_t sem;
	scm_i2c_notify notify;
	scm_i2c_notify user_notify;
	void *user_ctx;
};

static struct scm_i2c_data i2c_data[SCM_I2C_IDX_MAX] = {
	{
		.fd = -1,
		.dev_name = "/dev/i2c0",
	},
	{
		.fd = -1,
		.dev_name = "/dev/i2c1",
	},
	{
		.fd = -1,
		.dev_name = "/dev/i2c-gpio",
	},
};

static void i2c_notify(struct i2c_event *event, void *ctx)
{
	struct scm_i2c_data *data = (struct scm_i2c_data *)ctx;
	struct scm_i2c_event scm_event;

	scm_event.type = event->type;

	switch (scm_event.type) {
	case SCM_I2C_EVENT_MASTER_TRANS_CMPL:
		if (data->sync) {
			osSemaphoreRelease(data->sem);
		} else {
			scm_event.data.master_trans_cmpl.tx_len =
				event->data.master_trans_cmpl.tx_len;
			scm_event.data.master_trans_cmpl.rx_len =
				event->data.master_trans_cmpl.rx_len;

			data->user_notify(&scm_event, data->user_ctx);
		}
	break;
	case SCM_I2C_EVENT_SLAVE_RX_REQUEST:
	case SCM_I2C_EVENT_SLAVE_TX_REQUEST:
		data->user_notify(&scm_event, data->user_ctx);
	break;
	case SCM_I2C_EVENT_SLAVE_RX_CMPL:
		scm_event.data.slave_rx_cmpl.len =
			event->data.slave_rx_cmpl.len;
		scm_event.data.slave_rx_cmpl.truncated =
			event->data.slave_rx_cmpl.truncated;

		data->user_notify(&scm_event, data->user_ctx);
	break;
	case SCM_I2C_EVENT_SLAVE_TX_CMPL:
		scm_event.data.slave_tx_cmpl.len =
			event->data.slave_tx_cmpl.len;
		scm_event.data.slave_tx_cmpl.truncated =
			event->data.slave_tx_cmpl.truncated;

		data->user_notify(&scm_event, data->user_ctx);
	break;
	default:
	break;
	}
}

int scm_i2c_init(enum scm_i2c_idx idx)
{
	int fd;

	if (i2c_data[idx].fd > 0) {
		return WISE_ERR_INVALID_STATE;
	}

	fd = open(i2c_data[idx].dev_name, 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	i2c_data[idx].fd = fd;
	i2c_data[idx].sem = osSemaphoreNew(1, 0, NULL);

	return WISE_OK;
}

int scm_i2c_deinit(enum scm_i2c_idx idx)
{
	if (i2c_data[idx].fd <= 0) {
		return WISE_ERR_INVALID_STATE;
	}

	close(i2c_data[idx].fd);
	i2c_data[idx].fd = -1;

	osSemaphoreDelete(i2c_data[idx].sem);

	return WISE_OK;
}

int scm_i2c_configure(enum scm_i2c_idx idx, struct scm_i2c_cfg *cfg, scm_i2c_notify notify, void *ctx)
{
	struct i2c_cfg_arg arg;
	struct i2c_cfg i2c_cfg;
	int ret;

	if (i2c_data[idx].fd <= 0) {
		return WISE_ERR_INVALID_STATE;
	}

	i2c_data[idx].user_notify = notify;
	i2c_data[idx].user_ctx = ctx;

	i2c_cfg.role = (enum i2c_role)cfg->role;
	i2c_cfg.addr_len = (enum i2c_addr_len)cfg->addr_len;
	i2c_cfg.bitrate = cfg->bitrate;
	i2c_cfg.slave_addr = cfg->slave_addr;
	i2c_cfg.dma_en = cfg->dma_en;
	i2c_cfg.pull_up_en = cfg->pull_up_en;

	arg.cfg = &i2c_cfg;
	arg.cb = i2c_notify;
	arg.cb_ctx = &i2c_data[idx];

	ret = ioctl(i2c_data[idx].fd, IOCTL_I2C_CONFIGURE, &arg);
	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_i2c_reset(enum scm_i2c_idx idx)
{
	int ret;

	if (i2c_data[idx].fd <= 0) {
		return WISE_ERR_INVALID_STATE;
	}

	ret = ioctl(i2c_data[idx].fd, IOCTL_I2C_RESET, NULL);
	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_i2c_master_probe(enum scm_i2c_idx idx, uint16_t addr, uint8_t *status)
{
	struct i2c_master_probe_arg arg;
	int ret;

	if (i2c_data[idx].fd <= 0) {
		return WISE_ERR_INVALID_STATE;
	}

	arg.addr = addr;
	arg.status = status;

	ret = ioctl(i2c_data[idx].fd, IOCTL_I2C_MASTER_PROBE, &arg);
	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

static int scm_i2c_master_tx_common(enum scm_i2c_idx idx, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len)
{
	struct i2c_master_tx_arg arg;

	arg.addr = addr;
	arg.tx_buf = tx_buf;
	arg.tx_len = tx_len;

	return ioctl(i2c_data[idx].fd, IOCTL_I2C_MASTER_TX, &arg);
}

int scm_i2c_master_tx(enum scm_i2c_idx idx, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len, uint32_t timeout)
{
	int ret;

	if (i2c_data[idx].fd <= 0) {
		return WISE_ERR_INVALID_STATE;
	}

	i2c_data[idx].sync = 1;

	ret = scm_i2c_master_tx_common(idx, addr, tx_buf, tx_len);
	if (ret) {
		return WISE_ERR_IOCTL;
	}

	ret = osSemaphoreAcquire(i2c_data[idx].sem, timeout);
	if (ret != osOK) {
		ioctl(i2c_data[idx].fd, IOCTL_I2C_RESET, NULL);
		return WISE_ERR_TIMEOUT;
	}

	return WISE_OK;
}

int scm_i2c_master_tx_async(enum scm_i2c_idx idx, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len)
{
	int ret;

	if (i2c_data[idx].fd <= 0) {
		return WISE_ERR_INVALID_STATE;
	}

	i2c_data[idx].sync = 0;
	ret = scm_i2c_master_tx_common(idx, addr, tx_buf, tx_len);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_i2c_master_rx_common(enum scm_i2c_idx idx, uint16_t addr, uint8_t *rx_buf, uint32_t rx_len)
{
	struct i2c_master_rx_arg arg;

	arg.addr = addr;
	arg.rx_buf = rx_buf;
	arg.rx_len = rx_len;

	return ioctl(i2c_data[idx].fd, IOCTL_I2C_MASTER_RX, &arg);
}

int scm_i2c_master_rx(enum scm_i2c_idx idx, uint16_t addr, uint8_t *rx_buf, uint32_t rx_len, uint32_t timeout)
{
	int ret;

	if (i2c_data[idx].fd <= 0) {
		return WISE_ERR_INVALID_STATE;
	}

	i2c_data[idx].sync = 1;

	ret = scm_i2c_master_rx_common(idx, addr, rx_buf, rx_len);
	if (ret) {
		return WISE_ERR_IOCTL;
	}

	ret = osSemaphoreAcquire(i2c_data[idx].sem, timeout);
	if (ret != osOK) {
		ioctl(i2c_data[idx].fd, IOCTL_I2C_RESET, NULL);
		return WISE_ERR_TIMEOUT;
	}

	return WISE_OK;
}

int scm_i2c_master_rx_async(enum scm_i2c_idx idx, uint16_t addr, uint8_t *rx_buf, uint32_t rx_len)
{
	int ret;

	if (i2c_data[idx].fd <= 0) {
		return WISE_ERR_INVALID_STATE;
	}

	i2c_data[idx].sync = 0;

	ret = scm_i2c_master_rx_common(idx, addr, rx_buf, rx_len);
	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_i2c_master_tx_rx_common(enum scm_i2c_idx idx, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len)
{
	struct i2c_master_tx_rx_arg arg;

	arg.addr = addr;
	arg.tx_buf = tx_buf;
	arg.tx_len = tx_len;
	arg.rx_buf = rx_buf;
	arg.rx_len = rx_len;

	return ioctl(i2c_data[idx].fd, IOCTL_I2C_MASTER_TX_RX, &arg);
}

int scm_i2c_master_tx_rx(enum scm_i2c_idx idx, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len, uint32_t timeout)
{
	int ret;

	if (i2c_data[idx].fd <= 0) {
		return WISE_ERR_INVALID_STATE;
	}

	i2c_data[idx].sync = 1;

	ret = scm_i2c_master_tx_rx_common(idx, addr, tx_buf, tx_len, rx_buf, rx_len);
	if (ret) {
		return WISE_ERR_IOCTL;
	}

	ret = osSemaphoreAcquire(i2c_data[idx].sem, timeout);
	if (ret != osOK) {
		ioctl(i2c_data[idx].fd, IOCTL_I2C_RESET, NULL);
		return WISE_ERR_TIMEOUT;
	}

	return WISE_OK;
}

int scm_i2c_master_tx_rx_async(enum scm_i2c_idx idx, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len)
{
	int ret;

	if (i2c_data[idx].fd <= 0) {
		return WISE_ERR_INVALID_STATE;
	}

	i2c_data[idx].sync = 0;

	ret = scm_i2c_master_tx_rx_common(idx, addr, tx_buf, tx_len, rx_buf, rx_len);
	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_i2c_slave_tx(enum scm_i2c_idx idx, uint8_t *tx_buf, uint32_t tx_len)
{
	struct i2c_slave_tx_arg arg;
	int ret;

	if (i2c_data[idx].fd <= 0) {
		return WISE_ERR_INVALID_STATE;
	}

	arg.tx_buf = tx_buf;
	arg.tx_len = tx_len;

	ret = ioctl(i2c_data[idx].fd, IOCTL_I2C_SLAVE_TX, &arg);
	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_i2c_slave_rx(enum scm_i2c_idx idx, uint8_t *rx_buf, uint32_t rx_len)
{
	struct i2c_slave_rx_arg arg;
	int ret;

	if (i2c_data[idx].fd <= 0) {
		return WISE_ERR_INVALID_STATE;
	}

	arg.rx_buf = rx_buf;
	arg.rx_len = rx_len;

	ret = ioctl(i2c_data[idx].fd, IOCTL_I2C_SLAVE_RX, &arg);
	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

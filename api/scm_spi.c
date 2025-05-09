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

#include "errno.h"
#include "hal/spi.h"
#include "sys/ioctl.h"
#include "scm_spi.h"

struct scm_spi_data {
    int fd;
    char *dev_name;
    osSemaphoreId_t sem;
    uint8_t sync;
    scm_spi_notify notify;
    void *user_ctx;
};

static struct scm_spi_data spi_data[SCM_SPI_IDX_MAX] = {
    {
        .fd = -1,
        .dev_name = "/dev/spi0",
    },
    {
        .fd = -1,
        .dev_name = "/dev/spi1",
    },
    {
        .fd = -1,
        .dev_name = "/dev/spi2",
    },
};

static void spi_notify(struct spi_event *event, void *ctx)
{
    struct scm_spi_data *data = ctx;
    struct scm_spi_event scm_event;

    scm_event.type = (enum scm_spi_event_type)event->type;

    switch (scm_event.type) {
        case SCM_SPI_EVENT_MASTER_TRANS_CMPL:
            if (data->sync) {
                osSemaphoreRelease(data->sem);
            } else {
                if (data->notify) {
                    data->notify(&scm_event, ctx);
                }
            }
            break;
        case SCM_SPI_EVENT_SLAVE_TRANS_CMPL:
            if (data->notify) {
                scm_event.data.sl_cmpl.err =
                    event->data.sl_cmpl.err;
                scm_event.data.sl_cmpl.rx_amount =
                    event->data.sl_cmpl.rx_amount;
                scm_event.data.sl_cmpl.tx_amount =
                    event->data.sl_cmpl.tx_amount;
                data->notify(&scm_event, data->user_ctx);
            }
            break;
        case SCM_SPI_EVENT_SLAVE_RX_REQUEST:
        case SCM_SPI_EVENT_SLAVE_TX_REQUEST:
            if (data->notify) {
                data->notify(&scm_event, data->user_ctx);
            }
            break;
        case SCM_SPI_EVENT_SLAVE_USER_CMD:
            if (data->notify) {
                scm_event.data.sl_user_cmd.user_cmd =
                    event->data.sl_user_cmd.user_cmd;
                data->notify(&scm_event, data->user_ctx);
            }
            break;
    }
}

int scm_spi_init(enum scm_spi_idx idx)
{
    int fd;

    if (spi_data[idx].fd > 0) {
        return WISE_ERR_INVALID_STATE;
    }

    fd = open(spi_data[idx].dev_name, 0, 0);
    if (fd < 0) {
        return WISE_ERR_NOT_FOUND;
    }

    spi_data[idx].fd = fd;
    spi_data[idx].sem = osSemaphoreNew(1, 0, NULL);

    return WISE_OK;
}

int scm_spi_deinit(enum scm_spi_idx idx)
{
    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    close(spi_data[idx].fd);
    spi_data[idx].fd = 0;
    osSemaphoreDelete(spi_data[idx].sem);

    return WISE_OK;
}

int scm_spi_configure(enum scm_spi_idx idx, struct scm_spi_cfg *cfg, scm_spi_notify notify, void *ctx)
{
    struct spi_cfg_arg arg;
    struct spi_cfg spi_cfg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    spi_data[idx].notify = notify;
    spi_data[idx].user_ctx = ctx;

    spi_cfg.role = (enum spi_role)cfg->role;
    spi_cfg.mode = (enum spi_mode)cfg->mode;
    spi_cfg.data_io_format = (enum spi_data_io_format)cfg->data_io_format;
    spi_cfg.bit_order = (enum spi_bit_order)cfg->bit_order;
    spi_cfg.slave_extra_dummy_cycle = (enum spi_dummy_cycle)cfg->slave_extra_dummy_cycle;
    spi_cfg.master_cs_bitmap = cfg->master_cs_bitmap;
    spi_cfg.clk_src = cfg->clk_src;
    spi_cfg.clk_div_2mul = cfg->clk_div_2mul;
    spi_cfg.dma_en = cfg->dma_en;

    arg.cfg = &spi_cfg;
    arg.cb = spi_notify;
    arg.cb_ctx = &spi_data[idx];

    ret = ioctl(spi_data[idx].fd, IOCTL_SPI_CONFIGURE, &arg);
    if (ret) {
        return WISE_ERR_IOCTL;
    }

    return WISE_OK;
}

int scm_spi_reset(enum scm_spi_idx idx)
{
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    ret = ioctl(spi_data[idx].fd, IOCTL_SPI_RESET, NULL);
    if (ret) {
        return WISE_ERR_IOCTL;
    }

    return WISE_OK;
}

static int scm_spi_master_transfer(struct scm_spi_data *spid, void *arg,
        int ioctl_cmd, uint32_t timeout)
{
    int ret;

    ret = ioctl(spid->fd, ioctl_cmd, arg);
    if (ret) {
        return WISE_ERR_IOCTL;
    }

    if  (spid->sync) {
        ret = osSemaphoreAcquire(spid->sem, timeout);
        if (ret != osOK) {
            /* need automatically reset in API or reset by application */
            ioctl(spid->fd, IOCTL_SPI_RESET, NULL);
            return WISE_ERR_TIMEOUT;
        }
    }

    return WISE_OK;
}

int scm_spi_master_tx(enum scm_spi_idx idx, int slave, uint8_t *tx_buf,
        uint32_t tx_len, uint32_t timeout)
{
    struct spi_master_arg arg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.slave = slave;
    arg.tx_buf = tx_buf;
    arg.tx_len = tx_len;

    spi_data[idx].sync = 1;

    ret = scm_spi_master_transfer(&spi_data[idx], &arg, IOCTL_SPI_MASTER_TX, timeout);

    return ret;
}

int scm_spi_master_tx_async(enum scm_spi_idx idx, int slave, uint8_t *tx_buf,
        uint32_t tx_len)
{
    struct spi_master_arg arg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.slave = slave;
    arg.tx_buf = tx_buf;
    arg.tx_len = tx_len;

    spi_data[idx].sync = 0;

    ret = scm_spi_master_transfer(&spi_data[idx], &arg, IOCTL_SPI_MASTER_TX, 0);

    return ret;
}

int scm_spi_master_rx(enum scm_spi_idx idx, int slave, uint8_t *rx_buf,
        uint32_t rx_len, uint32_t timeout)
{
    struct spi_master_arg arg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.slave = slave;
    arg.rx_buf = rx_buf;
    arg.rx_len = rx_len;

    spi_data[idx].sync = 1;

    ret = scm_spi_master_transfer(&spi_data[idx], &arg, IOCTL_SPI_MASTER_RX, timeout);

    return ret;
}

int scm_spi_master_rx_async(enum scm_spi_idx idx, int slave, uint8_t *rx_buf,
        uint32_t rx_len)
{
    struct spi_master_arg arg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.slave = slave;
    arg.rx_buf = rx_buf;
    arg.rx_len = rx_len;

    spi_data[idx].sync = 0;

    ret = scm_spi_master_transfer(&spi_data[idx], &arg, IOCTL_SPI_MASTER_RX, 0);

    return ret;
}

int scm_spi_master_tx_rx(enum scm_spi_idx idx, int slave,
        enum scm_spi_trx_mode trx_mode, uint8_t *tx_buf,
        uint32_t tx_len, uint8_t *rx_buf,
        uint32_t rx_len, uint32_t timeout)
{
    struct spi_master_arg arg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.slave = slave;
    arg.trx_mode = (enum spi_trx_mode)trx_mode;
    arg.tx_buf = tx_buf;
    arg.tx_len = tx_len;
    arg.rx_buf = rx_buf;
    arg.rx_len = rx_len;

    spi_data[idx].sync = 1;

    ret = scm_spi_master_transfer(&spi_data[idx], &arg, IOCTL_SPI_MASTER_TX_RX, timeout);

    return ret;
}

int scm_spi_master_tx_rx_async(enum scm_spi_idx idx, int slave,
        enum scm_spi_trx_mode trx_mode, uint8_t *tx_buf,
        uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len)
{
    struct spi_master_arg arg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.slave = slave;
    arg.trx_mode = (enum spi_trx_mode)trx_mode;
    arg.tx_buf = tx_buf;
    arg.tx_len = tx_len;
    arg.rx_buf = rx_buf;
    arg.rx_len = rx_len;

    spi_data[idx].sync = 0;

    ret = scm_spi_master_transfer(&spi_data[idx], &arg, IOCTL_SPI_MASTER_TX_RX, 0);

    return ret;
}

int scm_spi_master_tx_with_cmd(enum scm_spi_idx idx, int slave,
        struct scm_spi_cmd_cfg *cmd_cfg, uint8_t *tx_buf,
        uint32_t tx_len, uint32_t timeout)
{
    struct spi_master_arg arg;
    struct spi_cmd_cfg spi_cmd_cfg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    spi_cmd_cfg.cmd = cmd_cfg->cmd;
    spi_cmd_cfg.addr_io_format = (enum spi_addr_io_fromat)cmd_cfg->addr_io_format;
    spi_cmd_cfg.addr_len = (enum spi_addr_len)cmd_cfg->addr_len;
    spi_cmd_cfg.addr = cmd_cfg->addr;
    spi_cmd_cfg.dummy_cycle = (enum spi_dummy_cycle)cmd_cfg->dummy_cycle;

    arg.slave = slave;
    arg.cmd_cfg = &spi_cmd_cfg;
    arg.tx_buf = tx_buf;
    arg.tx_len = tx_len;

    spi_data[idx].sync = 1;

    ret = scm_spi_master_transfer(&spi_data[idx], &arg, IOCTL_SPI_MASTER_TX_WITH_CMD, timeout);

    return ret;
}

int scm_spi_master_tx_with_cmd_async(enum scm_spi_idx idx, int slave,
        struct scm_spi_cmd_cfg *cmd_cfg, uint8_t *tx_buf,
        uint32_t tx_len)
{
    struct spi_master_arg arg;
    struct spi_cmd_cfg spi_cmd_cfg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    spi_cmd_cfg.cmd = cmd_cfg->cmd;
    spi_cmd_cfg.addr_io_format = (enum spi_addr_io_fromat)cmd_cfg->addr_io_format;
    spi_cmd_cfg.addr_len = (enum spi_addr_len)cmd_cfg->addr_len;
    spi_cmd_cfg.addr = cmd_cfg->addr;
    spi_cmd_cfg.dummy_cycle = (enum spi_dummy_cycle)cmd_cfg->dummy_cycle;

    arg.slave = slave;
    arg.cmd_cfg = &spi_cmd_cfg;
    arg.tx_buf = tx_buf;
    arg.tx_len = tx_len;

    spi_data[idx].sync = 0;

    ret = scm_spi_master_transfer(&spi_data[idx], &arg, IOCTL_SPI_MASTER_TX_WITH_CMD, 0);

    return ret;
}

int scm_spi_master_rx_with_cmd(enum scm_spi_idx idx, int slave,
        struct scm_spi_cmd_cfg *cmd_cfg, uint8_t *rx_buf,
        uint32_t rx_len, uint32_t timeout)
{
    struct spi_master_arg arg;
    struct spi_cmd_cfg spi_cmd_cfg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    spi_cmd_cfg.cmd = cmd_cfg->cmd;
    spi_cmd_cfg.addr_io_format = (enum spi_addr_io_fromat)cmd_cfg->addr_io_format;
    spi_cmd_cfg.addr_len = (enum spi_addr_len)cmd_cfg->addr_len;
    spi_cmd_cfg.addr = cmd_cfg->addr;
    spi_cmd_cfg.dummy_cycle = (enum spi_dummy_cycle)cmd_cfg->dummy_cycle;

    arg.slave = slave;
    arg.cmd_cfg = &spi_cmd_cfg;
    arg.rx_buf = rx_buf;
    arg.rx_len = rx_len;

    spi_data[idx].sync = 1;

    ret = scm_spi_master_transfer(&spi_data[idx], &arg, IOCTL_SPI_MASTER_RX_WITH_CMD, timeout);

    return ret;
}

int scm_spi_master_rx_with_cmd_async(enum scm_spi_idx idx, int slave,
        struct scm_spi_cmd_cfg *cmd_cfg, uint8_t *rx_buf,
        uint32_t rx_len)
{
    struct spi_master_arg arg;
    struct spi_cmd_cfg spi_cmd_cfg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    spi_cmd_cfg.cmd = cmd_cfg->cmd;
    spi_cmd_cfg.addr_io_format = (enum spi_addr_io_fromat)cmd_cfg->addr_io_format;
    spi_cmd_cfg.addr_len = (enum spi_addr_len)cmd_cfg->addr_len;
    spi_cmd_cfg.addr = cmd_cfg->addr;
    spi_cmd_cfg.dummy_cycle = (enum spi_dummy_cycle)cmd_cfg->dummy_cycle;

    arg.slave = slave;
    arg.cmd_cfg = &spi_cmd_cfg;
    arg.rx_buf = rx_buf;
    arg.rx_len = rx_len;

    spi_data[idx].sync = 0;

    ret = scm_spi_master_transfer(&spi_data[idx], &arg, IOCTL_SPI_MASTER_RX_WITH_CMD, 0);

    return ret;
}

int scm_spi_slave_set_tx_buf(enum scm_spi_idx idx, uint8_t *tx_buf, uint32_t tx_len)
{
    struct spi_slave_arg arg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.tx_buf = tx_buf;
    arg.tx_len = tx_len;

    ret = ioctl(spi_data[idx].fd, IOCTL_SPI_SLAVE_SET_TX_BUF, &arg);
    if (ret) {
        return WISE_ERR_IOCTL;
    }

    return WISE_OK;
}

int scm_spi_slave_set_rx_buf(enum scm_spi_idx idx, uint8_t *rx_buf, uint32_t rx_len)
{
    struct spi_slave_arg arg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.rx_buf = rx_buf;
    arg.rx_len = rx_len;

    ret = ioctl(spi_data[idx].fd, IOCTL_SPI_SLAVE_SET_RX_BUF, &arg);
    if (ret) {
        return WISE_ERR_IOCTL;
    }

    return WISE_OK;
}

int scm_spi_slave_set_tx_rx_buf(enum scm_spi_idx idx, enum scm_spi_trx_mode trx_mode,
        uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len)
{
    struct spi_slave_arg arg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.trx_mode = (enum spi_trx_mode)trx_mode;
    arg.tx_buf = tx_buf;
    arg.tx_len = tx_len;
    arg.rx_buf = rx_buf;
    arg.rx_len = rx_len;

    ret = ioctl(spi_data[idx].fd, IOCTL_SPI_SLAVE_SET_TX_RX_BUF, &arg);
    if (ret) {
        return WISE_ERR_IOCTL;
    }

    return WISE_OK;
}

int scm_spi_slave_set_user_state(enum scm_spi_idx idx, uint16_t user_state)
{
    struct spi_slave_arg arg;
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.user_state = user_state;

    ret = ioctl(spi_data[idx].fd, IOCTL_SPI_SLAVE_SET_USER_STATE, &arg);
    if (ret) {
        return WISE_ERR_IOCTL;
    }

    return WISE_OK;
}

int scm_spi_slave_cancel(enum scm_spi_idx idx)
{
    int ret;

    if (spi_data[idx].fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    ret = ioctl(spi_data[idx].fd, IOCTL_SPI_SLAVE_CANCEL, NULL);
    if (ret) {
        return WISE_ERR_IOCTL;
    }

    return WISE_OK;
}

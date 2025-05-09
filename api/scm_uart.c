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
#include <fcntl.h>
#include <hal/kernel.h>
#include <hal/console.h>

#include <cmsis_os.h>

#include "errno.h"
#include "sys/ioctl.h"
#include "scm_uart.h"

struct scm_uart_intf_data {
    struct scm_uart_data *port;
    osSemaphoreId_t done;
    osSemaphoreId_t prot;
    uint8_t sync;
    scm_uart_notify notify;
    void *ctx;
};

struct scm_uart_data {
    int fd;
    char *dev_name;
    enum scm_uart_idx idx;
    struct scm_uart_intf_data tx;
    struct scm_uart_intf_data rx;
};

static struct scm_uart_data uart_data[SCM_UART_IDX_MAX] = {
    {
        .fd = -1,
        .dev_name = "/dev/ttyS0",
        .idx = SCM_UART_IDX_0,
    },
    {
        .fd = -1,
        .dev_name = "/dev/ttyS1",
        .idx = SCM_UART_IDX_1,
    },
    {
        .fd = -1,
        .dev_name = "/dev/ttyS2",
        .idx = SCM_UART_IDX_2,
    },
};

static void scm_uart_lock(struct scm_uart_intf_data *intf)
{
    osSemaphoreAcquire(intf->prot, osWaitForever);
}

static void scm_uart_unlock(struct scm_uart_intf_data *intf)
{
    osSemaphoreRelease(intf->prot);
}

static int scm_uart_wait_completion(struct scm_uart_intf_data *intf, int timeout)
{
    int ret;

    ret = osSemaphoreAcquire(intf->done, timeout == -1 ? osWaitForever :
            pdMS_TO_TICKS(timeout));

    return ret;
}

static void scm_uart_complete(struct scm_uart_intf_data *intf)
{
    osSemaphoreRelease(intf->done);
}

    static inline
struct scm_uart_intf_data *scm_uart_event_to_intf(struct scm_uart_data *priv,
        struct scm_uart_event *evt)
{
    return ((evt->type == SCM_UART_EVENT_TX_CMPL) ? &priv->tx : &priv->rx);
}

static void uart_notify(struct uart_event *event, void *ctx)
{
    struct scm_uart_data *priv = ctx;
    struct scm_uart_event scm_event;
    struct scm_uart_intf_data *intf;

    scm_event.err = event->err;
    scm_event.type = (enum scm_uart_event_type)event->type;

    assert(scm_event.type == SCM_UART_EVENT_TX_CMPL
            || scm_event.type == SCM_UART_EVENT_RX_CMPL);

    if (scm_event.err != 0) {
        return;
    }

    intf = scm_uart_event_to_intf(priv, &scm_event);

    if (intf->sync) {
        scm_uart_complete(intf);
    } else {
        if (intf->notify) {
            intf->notify(priv->idx, &scm_event, intf->ctx);
        }
        scm_uart_unlock(intf);
    }
}

static int scm_uart_sync(struct scm_uart_intf_data *intf, int timeout)
{
    int ret;

    ret = scm_uart_wait_completion(intf, timeout);
    if (ret != osOK) {
        return WISE_ERR_TIMEOUT;
    }

    return WISE_OK;
}

static void scm_uart_init_intf(struct scm_uart_data *port,
        struct scm_uart_intf_data *intf)
{
    intf->port = port;
    intf->sync = 1;
    intf->notify = NULL;
    intf->ctx = NULL;
    intf->done = osSemaphoreNew(1, 0, NULL);
    intf->prot = osSemaphoreNew(1, 1, NULL);
}

static void scm_uart_intf_deinit(struct scm_uart_intf_data *intf)
{
    osSemaphoreDelete(intf->done);
    osSemaphoreDelete(intf->prot);
}

int scm_uart_init(enum scm_uart_idx idx, struct scm_uart_cfg *cfg)
{
    struct uart_init_arg arg;
    struct scm_uart_data *port;
    struct uart_cfg uart_cfg;
    int fd;
    int ret;

    port = &uart_data[idx];

    if (port->fd > 0) {
        return WISE_ERR_INVALID_STATE;
    }

    /* XXX: we need O_RDWR not to disrupt console port operations.
    */
    fd = open(port->dev_name, O_RDWR, 0);
    if (fd < 0) {
        return WISE_ERR_NOT_FOUND;
    }

    port->fd = fd;
    scm_uart_init_intf(port, &port->tx);
    scm_uart_init_intf(port, &port->rx);

    uart_cfg.baudrate = cfg->baudrate;
    uart_cfg.data_bits = cfg->data_bits;
    uart_cfg.parity = cfg->parity;
    uart_cfg.stop_bits = cfg->stop_bits;
    uart_cfg.dma_en = cfg->dma_en;

    arg.cfg = &uart_cfg;
    arg.cb = uart_notify;
    arg.cb_ctx = port;

    ret = ioctl(port->fd, IOCTL_UART_INIT, &arg);
    if (ret) {
        scm_uart_intf_deinit(&port->tx);
        scm_uart_intf_deinit(&port->rx);
        close(fd);
        port->fd = -1;
        return WISE_ERR_IOCTL;
    }

    return WISE_OK;
}

int scm_uart_deinit(enum scm_uart_idx idx)
{
    int ret;
    struct scm_uart_data *port = &uart_data[idx];

    if (port->fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    ret = ioctl(port->fd, IOCTL_UART_DEINIT, NULL);
    if (ret) {
        return WISE_ERR_IOCTL;
    }

    close(port->fd);
    port->fd = 0;

    scm_uart_intf_deinit(&port->tx);
    scm_uart_intf_deinit(&port->rx);

    return WISE_OK;
}

int scm_uart_tx(enum scm_uart_idx idx, uint8_t *tx_buf, uint32_t tx_len,
        int timeout)
{
    struct scm_uart_data *priv = &uart_data[idx];
    struct scm_uart_intf_data *intf = &priv->tx;
    struct uart_tx_arg arg;
    int ret;

    scm_uart_lock(intf);

    if (priv->fd <= 0) {
        scm_uart_unlock(intf);
        return WISE_ERR_INVALID_STATE;
    }

    arg.tx_buf = tx_buf;
    arg.tx_len = tx_len;

    intf->sync = 1;

    ret = ioctl(priv->fd, IOCTL_UART_TX, &arg);
    if (ret) {
        scm_uart_unlock(intf);
        return WISE_ERR_IOCTL;
    }

    ret = scm_uart_sync(intf, timeout);
    if (ret == WISE_ERR_TIMEOUT) {
        /* Still need to return TIMEOUT. */
        int ret2 = ioctl(priv->fd, IOCTL_UART_RESET, NULL);
        if (ret2) {
            ret = WISE_ERR_IOCTL;
        }
    }

    scm_uart_unlock(intf);

    return ret;
}

int scm_uart_tx_async(enum scm_uart_idx idx, uint8_t *tx_buf, uint32_t tx_len,
        scm_uart_notify notify, void *ctx)
{
    struct scm_uart_data *priv = &uart_data[idx];
    struct scm_uart_intf_data *intf = &priv->tx;
    struct uart_tx_arg arg;
    int ret;

    scm_uart_lock(intf);

    if (priv->fd <= 0) {
        scm_uart_unlock(intf);
        return WISE_ERR_INVALID_STATE;
    }

    arg.tx_buf = tx_buf;
    arg.tx_len = tx_len;

    intf->sync = 0;
    intf->notify = notify;
    intf->ctx = ctx;

    ret = ioctl(priv->fd, IOCTL_UART_TX, &arg);
    if (ret) {
        scm_uart_unlock(intf);
        return WISE_ERR_IOCTL;
    }

    /* The same interface can't be accessed by API functions
     * until this one is known to be completed.
     *
     * So, there is no unlocking yet.
     */

    return WISE_OK;
}

int scm_uart_rx(enum scm_uart_idx idx, uint8_t *rx_buf, uint32_t *rx_len,
        int timeout)
{
    struct scm_uart_data *priv = &uart_data[idx];
    struct scm_uart_intf_data *intf = &priv->rx;
    struct uart_rx_arg rx_arg;
    struct uart_get_rx_len_arg rx_len_arg;
    int ret;

    scm_uart_lock(intf);

    if (priv->fd <= 0) {
        scm_uart_unlock(intf);
        return WISE_ERR_INVALID_STATE;
    }

    rx_arg.rx_buf = rx_buf;
    rx_arg.rx_len = *rx_len;

    intf->sync = 1;

    ret = ioctl(priv->fd, IOCTL_UART_RX, &rx_arg);
    if (ret) {
        scm_uart_unlock(intf);
        return WISE_ERR_IOCTL;
    }

    ret = scm_uart_sync(intf, timeout);

    /* Read actual received length regardless of the result.
     */
    rx_len_arg.len = rx_len;
    ioctl(priv->fd, IOCTL_UART_GET_RX_LEN, &rx_len_arg);

    if (ret == WISE_ERR_TIMEOUT) {
        int ret2; /* Still need to return TIMEOUT by ret. */
        ret2 = ioctl(priv->fd, IOCTL_UART_RESET, NULL);
        if (ret2) {
            ret = WISE_ERR_IOCTL;
        }
    }

    scm_uart_unlock(intf);

    return ret;
}

int scm_uart_rx_async(enum scm_uart_idx idx, uint8_t *rx_buf, uint32_t rx_len,
        scm_uart_notify notify, void *ctx)
{
    struct scm_uart_data *priv = &uart_data[idx];
    struct scm_uart_intf_data *intf = &priv->rx;
    struct uart_rx_arg arg;
    int ret;

    scm_uart_lock(intf);

    if (priv->fd <= 0) {
        scm_uart_unlock(intf);
        return WISE_ERR_INVALID_STATE;
    }

    arg.rx_buf = rx_buf;
    arg.rx_len = rx_len;

    intf->sync = 0;
    intf->notify = notify;
    intf->ctx = ctx;

    ret = ioctl(priv->fd, IOCTL_UART_RX, &arg);
    if (ret) {
        scm_uart_unlock(intf);
        return WISE_ERR_IOCTL;
    }

    /* The same interface can't be accessed by API functions
     * until this one is known to be completed.
     *
     * So, there is no unlocking yet.
     */

    return WISE_OK;
}

int scm_uart_reset(enum scm_uart_idx idx)
{
    struct scm_uart_data *priv = &uart_data[idx];
    int ret;

    if (priv->fd <= 0) {
        ret = WISE_ERR_INVALID_STATE;
        goto done;
    }

    ret = ioctl(priv->fd, IOCTL_UART_RESET, NULL);
    if (ret) {
        ret = WISE_ERR_IOCTL;
    }

done:
    return ret;
}

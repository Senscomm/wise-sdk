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
#include <stdlib.h>
#include <string.h>

#include "hal/kernel.h"
#include "cmsis_os.h"

#include "scm_spi.h"

/* Buffer definitions for SPI communication using DMA */

#ifdef CONFIG_USE_DMA_ALLOCATION
static uint8_t *tx_buf;
static uint8_t *rx_buf;
#else
static uint8_t tx_buf[SCM_SPI_TRANSFER_MAX_LEN] __attribute__((section(".dma_buffer")));
static uint8_t rx_buf[SCM_SPI_TRANSFER_MAX_LEN] __attribute__((section(".dma_buffer")));
#endif

static struct scm_spi_cfg ms_cfg; /* Configuration structure for SPI master */

int spi_master_init(void)
{
    scm_spi_init(SCM_SPI_IDX_2);

    return 0;
}

/* Function to configure SPI master with specific settings */
int spi_master_config(enum scm_spi_data_io_format io_format, scm_spi_notify notify)
{
    int ret;

#ifdef CONFIG_USE_DMA_ALLOCATION
    if (!tx_buf) {
        tx_buf = dma_malloc(SCM_SPI_TRANSFER_MAX_LEN);
        rx_buf = dma_malloc(SCM_SPI_TRANSFER_MAX_LEN);
    }
#endif

    memset(&ms_cfg, 0, sizeof(ms_cfg));
    ms_cfg.role = SCM_SPI_ROLE_MASTER;           /* Set role to master */
    ms_cfg.mode = SCM_SPI_MODE_0;                /* SPI mode 0 */
    ms_cfg.data_io_format = io_format;           /* Set data IO format */
    ms_cfg.bit_order = SCM_SPI_BIT_ORDER_MSB_FIRST; /* MSB first */
    ms_cfg.clk_src = SCM_SPI_CLK_SRC_XTAL;       /* Clock source */
    ms_cfg.clk_div_2mul = 20;                    /* Clock divider */
    ms_cfg.dma_en = 1;                           /* Enable DMA */

    ret = scm_spi_configure(SCM_SPI_IDX_2, &ms_cfg, notify, NULL);

    return ret;
}

/* Function to transmit data over SPI */
int spi_master_tx(uint32_t count, uint32_t timeout)
{
    struct scm_spi_cmd_cfg cmd_cfg;
    int ret;
    int len;

    memset(&cmd_cfg, 0, sizeof(struct scm_spi_cmd_cfg));
    memset(tx_buf, 0, SCM_SPI_TRANSFER_MAX_LEN);

    sprintf((char *)tx_buf, "[MS->SL] SCM SPI Msg #%d", count);
    len = strlen((const char *)tx_buf);

    /* Set command based on the data IO format */
    if (ms_cfg.data_io_format == SCM_SPI_DATA_IO_FORMAT_SINGLE) {
        cmd_cfg.cmd = 0x51; /* Command for single IO */
        cmd_cfg.dummy_cycle = SCM_SPI_DUMMY_CYCLE_SINGLE_IO_8;
    } else if (ms_cfg.data_io_format == SCM_SPI_DATA_IO_FORMAT_DUAL) {
        cmd_cfg.cmd = 0x52; /* Command for dual IO */
        cmd_cfg.dummy_cycle = SCM_SPI_DUMMY_CYCLE_DUAL_IO_8;
    } else {
        cmd_cfg.cmd = 0x54; /* Command for quad IO */
        cmd_cfg.dummy_cycle = SCM_SPI_DUMMY_CYCLE_QUAD_IO_8;
    }

    /* Execute transmit function based on whether timeout is set */
    if (timeout) {
        ret = scm_spi_master_tx_with_cmd(SCM_SPI_IDX_2, 0, &cmd_cfg, tx_buf,
                len, timeout);
    } else {
        ret = scm_spi_master_tx_with_cmd_async(SCM_SPI_IDX_2, 0, &cmd_cfg, tx_buf, len);
    }

    return ret;
}

/* Function to receive data over SPI */
int spi_master_rx(uint32_t timeout)
{
    struct scm_spi_cmd_cfg cmd_cfg;
    int ret;

    memset(&cmd_cfg, 0, sizeof(struct scm_spi_cmd_cfg));
    memset(rx_buf, 0, SCM_SPI_TRANSFER_MAX_LEN);

    /* Set command based on the data IO format */
    if (ms_cfg.data_io_format == SCM_SPI_DATA_IO_FORMAT_SINGLE) {
        cmd_cfg.cmd = 0x0B; /* Command for single IO */
        cmd_cfg.dummy_cycle = SCM_SPI_DUMMY_CYCLE_SINGLE_IO_8;
    } else if (ms_cfg.data_io_format == SCM_SPI_DATA_IO_FORMAT_DUAL) {
        cmd_cfg.cmd = 0x0C; /* Command for dual IO */
        cmd_cfg.dummy_cycle = SCM_SPI_DUMMY_CYCLE_DUAL_IO_8;
    } else {
        cmd_cfg.cmd = 0x0E; /* Command for quad IO */
        cmd_cfg.dummy_cycle = SCM_SPI_DUMMY_CYCLE_QUAD_IO_8;
    }

    if (timeout) {
        ret = scm_spi_master_rx_with_cmd(SCM_SPI_IDX_2, 0, &cmd_cfg, rx_buf, SCM_SPI_TRANSFER_MAX_LEN, timeout);
    } else {
        ret = scm_spi_master_rx_with_cmd_async(SCM_SPI_IDX_2, 0, &cmd_cfg, rx_buf, SCM_SPI_TRANSFER_MAX_LEN);
    }

    return ret;
}

void spi_master_recv_msg_print(void)
{
    printf("%s\n", (char *)rx_buf);
}

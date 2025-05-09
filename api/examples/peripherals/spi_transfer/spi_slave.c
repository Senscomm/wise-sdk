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

/* Buffers for SPI communication using DMA */
#ifdef CONFIG_USE_DMA_ALLOCATION
static uint8_t *tx_buf;
static uint8_t *rx_buf;
#else
static uint8_t tx_buf[SCM_SPI_TRANSFER_MAX_LEN] __attribute__((section(".dma_buffer")));
static uint8_t rx_buf[SCM_SPI_TRANSFER_MAX_LEN] __attribute__((section(".dma_buffer")));
#endif

static struct scm_spi_cfg sl_cfg; /* Configuration structure for SPI slave */

/* Function to initialize SPI in slave mode */
int spi_slave_init(void)
{
	scm_spi_init(SCM_SPI_IDX_1);

	return 0;
}

/* Function to configure SPI slave with specific settings */
int spi_slave_config(enum scm_spi_data_io_format io_format, scm_spi_notify notify)
{
	int ret;

#ifdef CONFIG_USE_DMA_ALLOCATION
	if (!tx_buf) {
		tx_buf = dma_malloc(SCM_SPI_TRANSFER_MAX_LEN);
		rx_buf = dma_malloc(SCM_SPI_TRANSFER_MAX_LEN);
	}
#endif

	memset(&sl_cfg, 0, sizeof(sl_cfg));
	sl_cfg.role = SCM_SPI_ROLE_SLAVE;            /* Set role to slave */
	sl_cfg.mode = SCM_SPI_MODE_0;                /* SPI mode 0 */
	sl_cfg.data_io_format = io_format;           /* Set data IO format */
	sl_cfg.bit_order = SCM_SPI_BIT_ORDER_MSB_FIRST; /* MSB first */
	sl_cfg.clk_src = SCM_SPI_CLK_SRC_XTAL;       /* Clock source */
	sl_cfg.clk_div_2mul = 1;                     /* Clock divider */
	sl_cfg.dma_en = 1;                           /* Enable DMA */

	ret = scm_spi_configure(SCM_SPI_IDX_1, &sl_cfg, notify, NULL);

	return ret;
}

/* Function to prepare and set the SPI slave transmit buffer */
int spi_slave_tx(uint32_t count)
{
	int ret;

	memset(tx_buf, 0, SCM_SPI_TRANSFER_MAX_LEN);

	sprintf((char *)tx_buf, "[SL->MS] SCM SPI Msg #%d", count);

	ret = scm_spi_slave_set_tx_buf(SCM_SPI_IDX_1, tx_buf, SCM_SPI_TRANSFER_MAX_LEN);

	return ret;
}

/* Function to set the SPI slave receive buffer */
int spi_slave_rx(void)
{
	int ret;

	memset(rx_buf, 0, SCM_SPI_TRANSFER_MAX_LEN);

	ret = scm_spi_slave_set_rx_buf(SCM_SPI_IDX_1, rx_buf, SCM_SPI_TRANSFER_MAX_LEN);

	return ret;
}

void spi_slave_recv_msg_print(int len)
{
    if (len < SCM_SPI_TRANSFER_MAX_LEN) {
        rx_buf[len] = '\0';
    }
	printf("%s\n", rx_buf);
}

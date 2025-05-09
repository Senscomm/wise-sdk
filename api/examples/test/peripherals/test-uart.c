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
#include <string.h>
#include <stdlib.h>

#include "hal/kernel.h"
#include "hal/timer.h"
#include "hal/console.h"
#include "scm_uart.h"

#define TEST_COUNT		100

#define TEST_TIMEOUT	1000

#define TEST_MSG_SIZE	25

const char *test_msg = "[Xiaohu] UART Sanity Test";

static uint8_t *tx_buf;
static uint8_t *rx_buf;

static struct scm_uart_cfg uart_tx_cfg = {
	.baudrate = SCM_UART_BDR_115200,
	.data_bits = SCM_UART_DATA_BITS_8,
	.parity = SCM_UART_NO_PARITY,
	.stop_bits = SCM_UART_STOP_BIT_1,
	.dma_en = 0,
};

static struct scm_uart_cfg uart_rx_cfg = {
	.baudrate = SCM_UART_BDR_115200,
	.data_bits = SCM_UART_DATA_BITS_8,
	.parity = SCM_UART_NO_PARITY,
	.stop_bits = SCM_UART_STOP_BIT_1,
	.dma_en = 0,
};

static void hex_dump(char *title, uint8_t *buf, int len)
{
	int i;

	printf("%s\n", title);

	for (i = 0; i < len; i++) {
		if (i != 0 && (i % 16) == 0) {
			printf("\n%02x ", buf[i]);
		} else {
			printf("%02x ", buf[i]);
		}
	}

	printf("\n");
}


int uart_test(int dma_en, enum scm_uart_idx tx_id, enum scm_uart_idx rx_id)
{
	int ret;
	int i;

	printf("************ UART%d -> UART%d (%s MODE) ************\n", tx_id, rx_id,
			dma_en == 1 ? "DMA" : "PIO");

	uart_tx_cfg.dma_en = dma_en;
	uart_rx_cfg.dma_en = dma_en;

	ret = scm_uart_init(tx_id, &uart_tx_cfg);
	if (ret) {
		printk("UART%d initialize failed %x\n", tx_id, ret);
		return -1;
	}

	ret = scm_uart_init(rx_id, &uart_rx_cfg);
	if (ret) {
		printk("UART%d initialize failed %x\n", rx_id, ret);
		return -1;
	}

	memcpy(tx_buf, test_msg , TEST_MSG_SIZE);
	memset(rx_buf, 0, TEST_MSG_SIZE + 1);

	for (i = 0; i < TEST_COUNT; i++) {
		ret = scm_uart_rx_async(rx_id, rx_buf, TEST_MSG_SIZE, NULL, NULL);
		if (ret) {
			printk("UART%d RX error %x\n", rx_id, ret);
			return -1;
		}

		ret = scm_uart_tx(tx_id, tx_buf, TEST_MSG_SIZE, TEST_TIMEOUT);
		if (ret) {
			printk("UART%d TX error %x\n", tx_id, ret);
			return -1;
		}

		udelay(10000);

		if (memcmp(tx_buf, rx_buf, TEST_MSG_SIZE)) {
			printf("Data error!!!!!\n");
			hex_dump("TXBUF", tx_buf, TEST_MSG_SIZE);
			hex_dump("RXBUF", rx_buf, TEST_MSG_SIZE);

			udelay(1000 * 1000);
			return -1;
		} else {
			printf("[%04d]UART%d RX: %s\n", i, rx_id, (char *)rx_buf);
			memset(rx_buf, 0, TEST_MSG_SIZE);
		}
	}

	ret = scm_uart_deinit(tx_id);
	if (ret) {
		printk("UART%d deinitialize failed %x\n", tx_id, ret);
		return -1;
	}

	ret = scm_uart_deinit(rx_id);
	if (ret) {
		printk("UART%d deinitialize failed %x\n", rx_id, ret);
		return -1;
	}

	return 0;
}

int main(void)
{
	tx_buf = dma_malloc(TEST_MSG_SIZE);
	rx_buf = dma_malloc(TEST_MSG_SIZE + 1);

	if (uart_test(1, SCM_UART_IDX_1, SCM_UART_IDX_2) < 0) {
		assert(0);
	}

	if (uart_test(1, SCM_UART_IDX_2, SCM_UART_IDX_1) < 0) {
		assert(0);
	}

	if (uart_test(0, SCM_UART_IDX_1, SCM_UART_IDX_2) < 0) {
		assert(0);
	}

	if (uart_test(0, SCM_UART_IDX_2, SCM_UART_IDX_1) < 0) {
		assert(0);
	}

	dma_free(tx_buf);
	dma_free(rx_buf);

	return 0;
}

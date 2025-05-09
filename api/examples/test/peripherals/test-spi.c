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

#include "hal/kernel.h"
#include "hal/timer.h"
#include "hal/console.h"
#include "scm_spi.h"

#define TEST_COUNT		100

#define TEST_TIMEOUT	1000

#define TEST_MSG_SIZE	24

#define TEST_BUF_SIZE 	33

const char *io_str[3] = {
	"SINGLE",
	"DUAL",
	"QUAD",
};

const int io_dummy_cycle[3] = {
	SCM_SPI_DUMMY_CYCLE_SINGLE_IO_8,
	SCM_SPI_DUMMY_CYCLE_DUAL_IO_8,
	SCM_SPI_DUMMY_CYCLE_QUAD_IO_8,
};

const char *test_msg = "[Xiaohu] SPI Sanity Test";

static uint8_t *ms_tx_buf;
static uint8_t *ms_rx_buf;
static uint8_t *sl_tx_buf;
static uint8_t *sl_rx_buf;

struct scm_spi_cfg spi_ms_cfg = {
	.role = SCM_SPI_ROLE_MASTER,
	.mode = SCM_SPI_MODE_0,
	.data_io_format = SCM_SPI_DATA_IO_FORMAT_SINGLE,
	.bit_order = SCM_SPI_BIT_ORDER_MSB_FIRST,
	.clk_src = SCM_SPI_CLK_SRC_XTAL,
	.clk_div_2mul = 20,
};

struct scm_spi_cfg spi_sl_cfg = {
	.role = SCM_SPI_ROLE_SLAVE,
	.mode = SCM_SPI_MODE_0,
	.data_io_format = SCM_SPI_DATA_IO_FORMAT_SINGLE,
	.bit_order = SCM_SPI_BIT_ORDER_MSB_FIRST,
	.clk_src = SCM_SPI_CLK_SRC_XTAL,
	.clk_div_2mul = 20,
};

struct scm_spi_cmd_cfg cmd_cfg = {
	.cmd = 0,
	.addr_io_format = SCM_SPI_ADDR_IO_SINGLE,
	.addr_len = SCM_SPI_ADDR_LEN_NONE,
	.addr = 0,
	.dummy_cycle = SCM_SPI_DUMMY_CYCLE_SINGLE_IO_8,
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

static void data_check(uint8_t *rx, uint8_t *tx, char *test_name,
		char *type, enum scm_spi_idx id, int test_cnt)
{
	if (memcmp(tx, rx, TEST_MSG_SIZE)) {
		printf("Data error!!!!! (%s)\n", test_name);
		hex_dump("TXBUF", tx, TEST_MSG_SIZE);
		hex_dump("RXBUF", rx, TEST_MSG_SIZE);
		udelay(1000 * 1000);

		assert(0);
	} else {
		printf("[%04d][%s]SPI%d RX: %s\n", test_cnt, type, id, (char *)rx);
		memset(rx, 0, TEST_BUF_SIZE);
	}
}

int spi_test(int dma_en, int io, enum scm_spi_idx ms_id, enum scm_spi_idx sl_id)
{
	int offset;
	int ret;
	int i;

	printf("************  SPI%d Master <-> SPI%d Slave (%s MODE,  %s IO) ************\n", ms_id, sl_id,
			dma_en == 1 ? "DMA" : "PIO", io_str[io]);

	spi_ms_cfg.dma_en = dma_en;
	spi_ms_cfg.data_io_format = io;
	spi_sl_cfg.dma_en = dma_en;
	spi_sl_cfg.data_io_format = io;
	cmd_cfg.dummy_cycle = io_dummy_cycle[io];

	if (io == SCM_SPI_DATA_IO_FORMAT_SINGLE) {
		offset = 2;
	} else if (io == SCM_SPI_DATA_IO_FORMAT_DUAL) {
		offset = 4;
	} else {
		offset = 8;
	}

	ret = scm_spi_init(ms_id);
	if (ret) {
		printk("SPI%d initialize failed %x\n", ms_id, ret);
		return -1;
	}

	ret = scm_spi_configure(ms_id, &spi_ms_cfg, NULL, NULL);
	if (ret) {
		printk("SPI%d initialize failed %x\n", ms_id, ret);
		return -1;
	}

	ret = scm_spi_init(sl_id);
	if (ret) {
		printk("SPI%d initialize failed %x\n", sl_id, ret);
		return -1;
	}

	ret = scm_spi_configure(sl_id, &spi_sl_cfg, NULL, NULL);
	if (ret) {
		printk("SPI%d initialize failed %x\n", sl_id, ret);
		return -1;
	}

	for (i = 0; i < TEST_COUNT; i++) {
		memset(ms_tx_buf, 0, TEST_BUF_SIZE);
		memset(sl_tx_buf, 0, TEST_BUF_SIZE);
		memset(ms_rx_buf, 0, TEST_BUF_SIZE);
		memset(sl_rx_buf, 0, TEST_BUF_SIZE);

		memcpy(&ms_tx_buf[offset], test_msg, TEST_MSG_SIZE);
		memcpy(sl_tx_buf, test_msg, TEST_MSG_SIZE);

		/* SPI master TX */
		ret = scm_spi_slave_set_rx_buf(sl_id, sl_rx_buf, TEST_MSG_SIZE);
		if (ret) {
			printk("SPI%d RX prepare failed %x\n", sl_id, ret);
			return -1;
		}

		ret = scm_spi_master_tx(ms_id, 0, ms_tx_buf, TEST_MSG_SIZE + offset, TEST_TIMEOUT);
		if (ret) {
			printk("SPI%d TX failed %x\n", ms_id, ret);
			return -1;
		}

		udelay(10000);

		data_check(sl_rx_buf, &ms_tx_buf[offset], "Master TX",
				"TX ", sl_id, i);

		/* SPI master RX */
		ret = scm_spi_slave_set_tx_buf(sl_id, sl_tx_buf, TEST_MSG_SIZE);
		if (ret) {
			printk("SPI%d TX prepare failed %x\n", sl_id, ret);
			return -1;
		}

		ret = scm_spi_master_rx(ms_id, 0, ms_rx_buf, TEST_MSG_SIZE + offset, TEST_TIMEOUT);
		if (ret) {
			printk("SPI%d TX failed %x\n", ms_id, ret);
			return -1;
		}

		udelay(10000);

		data_check(&ms_rx_buf[offset], sl_tx_buf, "Master RX",
				"RX ", ms_id, i);

		if (io == SCM_SPI_DATA_IO_FORMAT_SINGLE) {
			/* SPI master TRX */
			ret = scm_spi_slave_set_tx_rx_buf(sl_id, SCM_SPI_TRX_SAMETIME, sl_tx_buf,
					TEST_MSG_SIZE, sl_rx_buf, TEST_MSG_SIZE);
			if (ret) {
				printk("SPI%d TRX prepare failed %x\n", sl_id, ret);
				return -1;
			}

			ret = scm_spi_master_tx_rx(ms_id, 0, SCM_SPI_TRX_SAMETIME,
									   ms_tx_buf, TEST_MSG_SIZE + offset,
									   ms_rx_buf, TEST_MSG_SIZE + offset,
									   TEST_TIMEOUT);
			if (ret) {
				printk("SPI%d TRX failed %x\n", ms_id, ret);
				return -1;
			}

			data_check(sl_rx_buf, &ms_tx_buf[offset], "Master TRX",
					"TRX", sl_id, i);

			data_check(&ms_rx_buf[offset], sl_tx_buf, "Master TRX",
					"TRX", ms_id, i);

			/* SPI master TX and RX */
			ret = scm_spi_slave_set_tx_rx_buf(sl_id, SCM_SPI_TRX_RX_FIRST, sl_tx_buf,
					TEST_MSG_SIZE, sl_rx_buf, TEST_MSG_SIZE);
			if (ret) {
				printk("SPI%d TRX prepare failed %x\n", sl_id, ret);
				return -1;
			}

			ret = scm_spi_master_tx_rx(ms_id, 0, SCM_SPI_TRX_TX_FIRST,
									   ms_tx_buf, TEST_MSG_SIZE + offset,
									   ms_rx_buf, TEST_MSG_SIZE,
									   TEST_TIMEOUT);
			if (ret) {
				printk("SPI%d TRX failed %x\n", ms_id, ret);
				return -1;
			}

			data_check(sl_rx_buf, &ms_tx_buf[offset], "Master TX and RX",
					"T+R", sl_id, i);

			data_check(ms_rx_buf, sl_tx_buf, "Master TX and RX",
					"T+R", ms_id, i);

			/* SPI master RX and TX */
			ret = scm_spi_slave_set_tx_rx_buf(sl_id, SCM_SPI_TRX_TX_FIRST, sl_tx_buf,
					TEST_MSG_SIZE, sl_rx_buf, TEST_MSG_SIZE);
			if (ret) {
				printk("SPI%d TRX prepare failed %x\n", sl_id, ret);
				return -1;
			}

			memcpy(ms_tx_buf, test_msg, TEST_MSG_SIZE);
			ret = scm_spi_master_tx_rx(ms_id, 0, SCM_SPI_TRX_RX_FIRST,
									   ms_tx_buf, TEST_MSG_SIZE,
									   ms_rx_buf, TEST_MSG_SIZE + offset,
									   TEST_TIMEOUT);
			if (ret) {
				printk("SPI%d TRX failed %x\n", ms_id, ret);
				return -1;
			}

			data_check(sl_rx_buf, ms_tx_buf, "Master RX and TX",
					"R+T", sl_id, i);

			data_check(&ms_rx_buf[offset], sl_tx_buf, "Master RX and TX",
					"R+T", ms_id, i);
		}


		memcpy(ms_tx_buf, test_msg, TEST_MSG_SIZE);
		memcpy(sl_tx_buf, test_msg, TEST_MSG_SIZE);

		/* SPI master TX with command */
		ret = scm_spi_slave_set_rx_buf(sl_id, sl_rx_buf, TEST_MSG_SIZE);
		if (ret) {
			printk("SPI%d RX prepare failed %x\n", sl_id, ret);
			return -1;
		}

		ret = scm_spi_master_tx_with_cmd(ms_id, 0, &cmd_cfg, ms_tx_buf, TEST_MSG_SIZE, TEST_TIMEOUT);
		if (ret) {
			printk("SPI%d TX failed %x\n", ms_id, ret);
			return -1;
		}

		udelay(10000);


		data_check(sl_rx_buf, ms_tx_buf, "Master TX with command",
				"TXC", sl_id, i);

		/* SPI master RX with command */
		ret = scm_spi_slave_set_tx_buf(sl_id, sl_tx_buf, TEST_MSG_SIZE);
		if (ret) {
			printk("SPI%d TX prepare failed %x\n", sl_id, ret);
			return -1;
		}

		ret = scm_spi_master_rx_with_cmd(ms_id, 0, &cmd_cfg, ms_rx_buf, TEST_MSG_SIZE, TEST_TIMEOUT);
		if (ret) {
			printk("SPI%d TX failed %x\n", ms_id, ret);
			return -1;
		}

		udelay(10000);

		data_check(ms_rx_buf, sl_tx_buf, "Master RX with command",
				"RXC", ms_id, i);
	}

	ret = scm_spi_deinit(ms_id);
	if (ret) {
		printk("SPI%d deinitialize failed %x\n", ms_id, ret);
		return -1;
	}

	ret = scm_spi_deinit(sl_id);
	if (ret) {
		printk("SPI%d deinitialize failed %x\n", sl_id, ret);
		return -1;
	}

	return 0;
}

int main(void)
{
	ms_tx_buf = dma_malloc(TEST_BUF_SIZE);
	ms_rx_buf = dma_malloc(TEST_BUF_SIZE);
	sl_tx_buf = dma_malloc(TEST_BUF_SIZE);
	sl_rx_buf = dma_malloc(TEST_BUF_SIZE);

	if (spi_test(0, SCM_SPI_DATA_IO_FORMAT_SINGLE, SCM_SPI_IDX_1, SCM_SPI_IDX_2) < 0) {
		assert(0);
	}

	if (spi_test(0, SCM_SPI_DATA_IO_FORMAT_SINGLE, SCM_SPI_IDX_2, SCM_SPI_IDX_1) < 0) {
		assert(0);
	}

	if (spi_test(1, SCM_SPI_DATA_IO_FORMAT_SINGLE, SCM_SPI_IDX_1, SCM_SPI_IDX_2) < 0) {
		assert(0);
	}

	if (spi_test(1, SCM_SPI_DATA_IO_FORMAT_SINGLE, SCM_SPI_IDX_2, SCM_SPI_IDX_1) < 0) {
		assert(0);
	}

	if (spi_test(1, SCM_SPI_DATA_IO_FORMAT_DUAL, SCM_SPI_IDX_1, SCM_SPI_IDX_2) < 0) {
		assert(0);
	}

	if (spi_test(1, SCM_SPI_DATA_IO_FORMAT_DUAL, SCM_SPI_IDX_2, SCM_SPI_IDX_1) < 0) {
		assert(0);
	}

	if (spi_test(1, SCM_SPI_DATA_IO_FORMAT_QUAD, SCM_SPI_IDX_1, SCM_SPI_IDX_2) < 0) {
		assert(0);
	}

	if (spi_test(1, SCM_SPI_DATA_IO_FORMAT_QUAD, SCM_SPI_IDX_2, SCM_SPI_IDX_1) < 0) {
		assert(0);
	}

	dma_free(ms_tx_buf);
	dma_free(ms_rx_buf);
	dma_free(sl_tx_buf);
	dma_free(sl_rx_buf);

	return 0;
}

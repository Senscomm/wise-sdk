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
#include "scm_i2c.h"

#define TEST_COUNT		100

#define TEST_TIMEOUT	1000

#define TEST_MSG_SIZE	24

#define TEST_SLAVE_ADDR	0x5A

const char *test_msg = "[Xiaohu] I2C Sanity Test";

static uint8_t *ms_tx_buf;
static uint8_t *ms_rx_buf;
static uint8_t *sl_tx_buf;
static uint8_t *sl_rx_buf;

static struct scm_i2c_cfg i2c_ms_cfg = {
	.role = SCM_I2C_ROLE_MASTER,
	.addr_len = SCM_I2C_ADDR_LEN_7BIT,
	.bitrate = 400 * 1000,
	.slave_addr = 0,
	.dma_en = 0,
	.pull_up_en = 1,
};

static struct scm_i2c_cfg i2c_sl_cfg = {
	.role = SCM_I2C_ROLE_SLAVE,
	.addr_len = SCM_I2C_ADDR_LEN_7BIT,
	.bitrate = 400 * 1000,
	.slave_addr = TEST_SLAVE_ADDR,
	.dma_en = 0,
	.pull_up_en = 1,
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

int i2c_slave_notity(struct scm_i2c_event *event, void *ctx)
{
	enum scm_i2c_idx *sl_id = ctx;

	switch (event->type) {
	case SCM_I2C_EVENT_SLAVE_TX_REQUEST:
		scm_i2c_slave_tx(*sl_id, sl_tx_buf, TEST_MSG_SIZE);
	break;
	case SCM_I2C_EVENT_SLAVE_RX_REQUEST:
		scm_i2c_slave_rx(*sl_id, sl_rx_buf, TEST_MSG_SIZE);
	break;
	default:
	break;
	}

	return 0;
}

int i2c_test(int dma_en, enum scm_i2c_idx ms_id, enum scm_i2c_idx sl_id)
{
	int ret;
	int i;

	printf("************  I2C%d Master <-> I2C%d Slave (%s MODE) ************\n", ms_id, sl_id,
			dma_en == 1 ? "DMA" : "PIO");

	i2c_ms_cfg.dma_en = dma_en;
	i2c_sl_cfg.dma_en = dma_en;

	ret = scm_i2c_init(ms_id);
	if (ret) {
		printk("I2C%d initialize failed %x\n", ms_id, ret);
		return -1;
	}

	ret = scm_i2c_configure(ms_id, &i2c_ms_cfg, NULL, NULL);
	if (ret) {
		printk("I2C%d configure error %x\n", ms_id, ret);
		return -1;
	}

	ret = scm_i2c_init(sl_id);
	if (ret) {
		printk("I2C%d initialize failed %x\n", sl_id, ret);
		return -1;
	}

	ret = scm_i2c_configure(sl_id, &i2c_sl_cfg, i2c_slave_notity, &sl_id);
	if (ret) {
		printk("I2C%d configure error %x\n", ms_id, ret);
		return -1;
	}

	memcpy(ms_tx_buf, test_msg, TEST_MSG_SIZE);
	memset(ms_rx_buf, 0, TEST_MSG_SIZE + 1);
	memcpy(sl_tx_buf, test_msg, TEST_MSG_SIZE);
	memset(sl_rx_buf, 0, TEST_MSG_SIZE + 1);

	for (i = 0; i < TEST_COUNT; i++) {
		ret = scm_i2c_master_tx(ms_id, TEST_SLAVE_ADDR, ms_tx_buf, TEST_MSG_SIZE, TEST_TIMEOUT);
		if (ret) {
			printk("I2C%d TX failed %x\n", ms_id, ret);
			return -1;
		}

		udelay(10000);

		if (memcmp(ms_tx_buf, sl_rx_buf, TEST_MSG_SIZE)) {
			printf("Data error!!!!! (TX)\n");
			hex_dump("TXBUF", ms_tx_buf, TEST_MSG_SIZE);
			hex_dump("RXBUF", sl_rx_buf, TEST_MSG_SIZE);

			udelay(1000 * 1000);
			return -1;
		} else {
			printf("[%04d][TX ]I2C%d RX: %s\n", i, sl_id, (char *)sl_rx_buf);
			memset(sl_rx_buf, 0, TEST_MSG_SIZE + 1);
		}

		ret = scm_i2c_master_rx(ms_id, TEST_SLAVE_ADDR, ms_rx_buf, TEST_MSG_SIZE, TEST_TIMEOUT);
		if (ret) {
			printk("I2C%d TX failed %x\n", ms_id, ret);
			return -1;
		}

		udelay(10000);

		if (memcmp(ms_rx_buf, sl_tx_buf, TEST_MSG_SIZE)) {
			printf("Data error!!!!! (RX)\n");
			hex_dump("TXBUF", sl_tx_buf, TEST_MSG_SIZE);
			hex_dump("RXBUF", ms_rx_buf, TEST_MSG_SIZE);

			udelay(1000 * 1000);
			return -1;
		} else {
			printf("[%04d][RX ]I2C%d RX: %s\n", i, ms_id, (char *)ms_rx_buf);
			memset(ms_rx_buf, 0, TEST_MSG_SIZE + 1);
		}

		ret = scm_i2c_master_tx_rx(ms_id, TEST_SLAVE_ADDR,
				ms_tx_buf, TEST_MSG_SIZE,
				ms_rx_buf, TEST_MSG_SIZE,
				TEST_TIMEOUT);
		if (ret) {
			printk("I2C%d TX failed %x\n", ms_id, ret);
			return -1;
		}

		udelay(10000);

		if (memcmp(ms_tx_buf, sl_rx_buf, TEST_MSG_SIZE)) {
			printf("Data error!!!!! (TRX (M -> S))\n");
			hex_dump("TXBUF", ms_tx_buf, TEST_MSG_SIZE);
			hex_dump("RXBUF", sl_rx_buf, TEST_MSG_SIZE);

			udelay(1000 * 1000);
			return -1;
		} else {
			printf("[%04d][TRX]I2C%d RX: %s\n", i, sl_id, (char *)sl_rx_buf);
			memset(sl_rx_buf, 0, TEST_MSG_SIZE + 1);
		}

		if (memcmp(ms_rx_buf, sl_tx_buf, TEST_MSG_SIZE)) {
			printf("Data error!!!!! (TRX (S -> M))\n");
			hex_dump("TXBUF", sl_tx_buf, TEST_MSG_SIZE);
			hex_dump("RXBUF", ms_rx_buf, TEST_MSG_SIZE);

			udelay(1000 * 1000);
			return -1;
		} else {
			printf("[%04d][TRX]I2C%d RX: %s\n", i, ms_id, (char *)ms_rx_buf);
			memset(ms_rx_buf, 0, TEST_MSG_SIZE + 1);
		}
	}

	ret = scm_i2c_deinit(ms_id);
	if (ret) {
		printk("I2C%d deinitialize failed %x\n", ms_id, ret);
		return -1;
	}

	ret = scm_i2c_deinit(sl_id);
	if (ret) {
		printk("I2C%d deinitialize failed %x\n", sl_id, ret);
		return -1;
	}

	return 0;
}

int main(void)
{
	ms_tx_buf = dma_malloc(TEST_MSG_SIZE);
	ms_rx_buf = dma_malloc(TEST_MSG_SIZE + 1);
	sl_tx_buf = dma_malloc(TEST_MSG_SIZE);
	sl_rx_buf = dma_malloc(TEST_MSG_SIZE + 1);

	if (i2c_test(0, SCM_I2C_IDX_0, SCM_I2C_IDX_1) < 0) {
		assert(0);
	}

	if (i2c_test(0, SCM_I2C_IDX_1, SCM_I2C_IDX_0) < 0) {
		assert(0);
	}

	if (i2c_test(1, SCM_I2C_IDX_0, SCM_I2C_IDX_1) < 0) {
		assert(0);
	}

	if (i2c_test(1, SCM_I2C_IDX_1, SCM_I2C_IDX_0) < 0) {
		assert(0);
	}

	dma_free(ms_tx_buf);
	dma_free(ms_rx_buf);
	dma_free(sl_tx_buf);
	dma_free(sl_rx_buf);

	return 0;
}

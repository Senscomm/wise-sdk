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
#include "hal/console.h"
#include "cli.h"
#include "scm_i2c.h"

#define I2C_BUF_LEN	16

static uint8_t *i2cm_tx_buf;
static uint8_t *i2cm_rx_buf;
static uint8_t *i2cs_tx_buf;
static uint8_t *i2cs_rx_buf;

#ifndef CONFIG_USE_DMA_ALLOCATION
static uint8_t g_i2cm_tx_buf[I2C_BUF_LEN] __attribute__((section(".dma_buffer")));
static uint8_t g_i2cm_rx_buf[I2C_BUF_LEN] __attribute__((section(".dma_buffer")));
static uint8_t g_i2cs_tx_buf[I2C_BUF_LEN] __attribute__((section(".dma_buffer")));
static uint8_t g_i2cs_rx_buf[I2C_BUF_LEN] __attribute__((section(".dma_buffer")));
#endif

static enum scm_i2c_idx i2cm_idx = -1;
static enum scm_i2c_idx i2cs_idx = -1;

static void scm_cli_i2c_dump_buf(uint8_t *buf, int len)
{
	int i;

	for (i = 0; i < I2C_BUF_LEN; i++) {
		printf("%02x ", buf[i]);
	}
	printf("\n");
}

static void scm_cli_i2c_set_buf(int argc, char *argv[], uint8_t *buf)
{
	int i;

	for (i = 0; i < argc && i < I2C_BUF_LEN; i++) {
		buf[i] = strtol(argv[i], NULL, 16);
	}
	printf("buf set: ");
	scm_cli_i2c_dump_buf(buf, I2C_BUF_LEN);
}

static int scm_cli_i2cm_notify(struct scm_i2c_event *event, void *ctx)
{
	if (event->type != SCM_I2C_EVENT_MASTER_TRANS_CMPL) {
		assert(0);
	}

	printk("M:%d,%d\n",
		   event->data.master_trans_cmpl.tx_len,
		   event->data.master_trans_cmpl.rx_len);
	return 0;
}

static int scm_cli_i2cm_reset(int argc, char *argv[])
{
	int i2c_idx = 0;

	if (argc == 2) {
		i2c_idx = atoi(argv[1]);
	}

	scm_i2c_reset(i2c_idx);

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cm_probe(int argc, char *argv[])
{
	uint16_t addr_min = 0, addr_max = 0x7f;
	uint8_t status;
	int ret;

    if (argc > 1) {
	    addr_min = addr_max = strtol(argv[1], NULL, 16);
    }

	for (int i = addr_min; i <= addr_max; i++) {
		ret = scm_i2c_master_probe(i2cm_idx, i, &status);
		if (ret) {
			printf("i2c probe error %x\n", ret);
			break;
		}

		if ((i % 16) == 0) {
			printf("\n%02x: ", i);
		}

		if (status) {
			printf("%02x ", i);
		} else {
			printf("-- ");
		}

	}
	printf("\n");

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cm_init(int argc, char *argv[])
{
	struct scm_i2c_cfg cfg;
	int idx;
	int ret;

	if (argc < 5) {
		return CMD_RET_USAGE;
	}

	idx = atoi(argv[1]);

	memset(&cfg, 0, sizeof(cfg));
	cfg.role = SCM_I2C_ROLE_MASTER;
	cfg.dma_en = atoi(argv[2]);
	cfg.pull_up_en = atoi(argv[3]);;
	cfg.bitrate = atoi(argv[4]) * 1000;

	ret = scm_i2c_init(idx);
	if (ret) {
		printf("i2c init error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	ret = scm_i2c_configure(idx, &cfg, scm_cli_i2cm_notify, NULL);
	if (ret) {
		scm_i2c_deinit(idx);
		printf("i2c configure error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	i2cm_idx = idx;

#ifdef CONFIG_USE_DMA_ALLOCATION
	if (!i2cm_tx_buf) {
		i2cm_tx_buf = dma_malloc(I2C_BUF_LEN);
		i2cm_rx_buf = dma_malloc(I2C_BUF_LEN);
	}
#else
	i2cm_tx_buf = g_i2cm_tx_buf;
	i2cm_rx_buf = g_i2cm_rx_buf;
#endif

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cm_deinit(int argc, char *argv[])
{
	int ret;

	ret = scm_i2c_deinit(i2cm_idx);
	if (ret) {
		printf("i2c deinit error %x\n", ret);
		return CMD_RET_FAILURE;
	}
	i2cm_idx = -1;

#ifdef CONFIG_USE_DMA_ALLOCATION
	if (i2cm_tx_buf) {
		dma_free(i2cm_tx_buf);
		dma_free(i2cm_rx_buf);
		i2cm_tx_buf = NULL;
		i2cm_rx_buf = NULL;
	}
#endif

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cm_txbuf(int argc, char *argv[])
{
	if (!i2cm_tx_buf) {
		return CMD_RET_FAILURE;
	}

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	argc--;
	argv++;
	scm_cli_i2c_set_buf(argc, argv, i2cm_tx_buf);

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cm_rxbuf(int argc, char *argv[])
{
	if (!i2cm_rx_buf) {
		return CMD_RET_FAILURE;
	}

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	argc--;
	argv++;
	scm_cli_i2c_set_buf(argc, argv, i2cm_rx_buf);

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cm_tx(int argc, char *argv[])
{
	uint8_t slave_addr;
	int tx_len;
	uint32_t timeout;
	int ret;

	if (!i2cm_tx_buf) {
		return CMD_RET_FAILURE;
	}

	if (argc < 3) {
		return CMD_RET_USAGE;
	}

	slave_addr = strtol(argv[1], NULL, 16);
	tx_len = atoi(argv[2]);

	if (tx_len > I2C_BUF_LEN) {
		printf("Invalid len : %d (< 16)\n", tx_len);
		return CMD_RET_FAILURE;
	}

	if (argc == 4) {
		timeout = atoi(argv[3]);
		ret = scm_i2c_master_tx(i2cm_idx, slave_addr, i2cm_tx_buf, tx_len, timeout);
	} else {
		ret = scm_i2c_master_tx_async(i2cm_idx, slave_addr, i2cm_tx_buf, tx_len);
	}

	if (ret) {
		printf("i2c tx error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cm_rx(int argc, char *argv[])
{
	uint8_t slave_addr;
	int rx_len;
	uint32_t timeout;
	int ret;

	if (!i2cm_rx_buf) {
		return CMD_RET_FAILURE;
	}

	if (argc < 3) {
		return CMD_RET_USAGE;
	}

	slave_addr = strtol(argv[1], NULL, 16);
	rx_len = atoi(argv[2]);

	if (rx_len > I2C_BUF_LEN) {
		printf("Invalid len : %d (< 16)\n", rx_len);
		return CMD_RET_FAILURE;
	}

	if (argc == 4) {
		timeout = atoi(argv[3]);
		ret = scm_i2c_master_rx(i2cm_idx, slave_addr, i2cm_rx_buf, rx_len, timeout);
	} else {
		ret = scm_i2c_master_rx_async(i2cm_idx, slave_addr, i2cm_rx_buf, rx_len);
	}

	if (ret) {
		printf("i2c rx error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cm_tx_rx(int argc, char *argv[])
{
	uint8_t slave_addr;
	int tx_len;
	int rx_len;
	uint32_t timeout;
	int ret;

	if (!i2cm_tx_buf || !i2cm_rx_buf) {
		return CMD_RET_FAILURE;
	}

	if (argc < 4) {
		return CMD_RET_USAGE;
	}

	slave_addr = strtol(argv[1], NULL, 16);
	tx_len = atoi(argv[2]);
	rx_len = atoi(argv[3]);

	if (rx_len > I2C_BUF_LEN) {
		printf("Invalid rx len : %d (< 16)\n", rx_len);
		return CMD_RET_FAILURE;
	}

	if (tx_len > I2C_BUF_LEN) {
		printf("Invalid tx len : %d (< 16)\n", tx_len);
		return CMD_RET_FAILURE;
	}

	if (argc == 5) {
		timeout = atoi(argv[4]);
		ret = scm_i2c_master_tx_rx(i2cm_idx, slave_addr, i2cm_tx_buf, tx_len, i2cm_rx_buf, rx_len, timeout);
	} else {
		ret = scm_i2c_master_tx_rx_async(i2cm_idx, slave_addr, i2cm_tx_buf, tx_len, i2cm_rx_buf, rx_len);
	}

	if (ret) {
		printf("i2c tx rx error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cm_dump(int argc, char *argv[])
{
	if (!i2cm_tx_buf || !i2cm_rx_buf) {
		return CMD_RET_FAILURE;
	}

	printf("i2cm tx buf: ");
	scm_cli_i2c_dump_buf(i2cm_tx_buf, I2C_BUF_LEN);
	printf("i2cm rx buf: ");
	scm_cli_i2c_dump_buf(i2cm_rx_buf, I2C_BUF_LEN);

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cm_clrbuf(int argc, char *argv[])
{
	if (!i2cm_tx_buf || !i2cm_rx_buf) {
		return CMD_RET_FAILURE;
	}

	memset(i2cm_tx_buf, 0, I2C_BUF_LEN);
	memset(i2cm_rx_buf, 0, I2C_BUF_LEN);

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_cli_i2cm_cmd[] = {
	CMDENTRY(init, scm_cli_i2cm_init, "", ""),
	CMDENTRY(deinit, scm_cli_i2cm_deinit, "", ""),
	CMDENTRY(probe, scm_cli_i2cm_probe, "", ""),
	CMDENTRY(reset, scm_cli_i2cm_reset, "", ""),
	CMDENTRY(txbuf, scm_cli_i2cm_txbuf, "", ""),
	CMDENTRY(rxbuf, scm_cli_i2cm_rxbuf, "", ""),
	CMDENTRY(clrbuf, scm_cli_i2cm_clrbuf, "", ""),
	CMDENTRY(dump, scm_cli_i2cm_dump, "", ""),
	CMDENTRY(tx, scm_cli_i2cm_tx, "", ""),
	CMDENTRY(rx, scm_cli_i2cm_rx, "", ""),
	CMDENTRY(tr, scm_cli_i2cm_tx_rx, "", ""),
};

static int do_scm_cli_i2cm(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], scm_cli_i2cm_cmd, ARRAY_SIZE(scm_cli_i2cm_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(i2cm, do_scm_cli_i2cm,
	"I2C master test",
	"i2cm init idx dma(0|1) pull_up(0|1) bitrate(in Kbps)\n"
	"\t<dma>\n"
	"\tthis is operational parameter. if dma is included and set 1,\n"
	"\tI2C TX or RX use DMA." OR
	"i2cm probe <addr:hex>" OR
	"i2cm deinit" OR
	"i2cm txbuf <data:hex> <data:hex> ...." OR
	"i2cm rxbuf <data:hex> <data:hex> ...." OR
	"i2cm clrbuf" OR
	"i2cm dump" OR
	"i2cm tx slave_addr:hex tx_len <timeout>" OR
	"i2cm rx slave_addr:hex rx_len <timeout>" OR
	"i2cm tr slave_addr:hex tx_len rx_len <timeout>"
);


static int scm_cli_i2cs_notify(struct scm_i2c_event *event, void *ctx)
{
	switch (event->type) {
	case SCM_I2C_EVENT_SLAVE_TX_REQUEST:
		scm_i2c_slave_tx(i2cs_idx, i2cs_tx_buf, I2C_BUF_LEN);
	break;
	case SCM_I2C_EVENT_SLAVE_RX_REQUEST:
		scm_i2c_slave_rx(i2cs_idx, i2cs_rx_buf, I2C_BUF_LEN);
	break;
	case SCM_I2C_EVENT_SLAVE_TX_CMPL:
		printk("ST:%d\n", event->data.slave_tx_cmpl.len);
	break;
	case SCM_I2C_EVENT_SLAVE_RX_CMPL:
		printk("SR:%d\n", event->data.slave_rx_cmpl.len);
	break;
	default:
	break;
	}
	return 0;
}

static int scm_cli_i2cs_init(int argc, char *argv[])
{
	struct scm_i2c_cfg cfg;
	int idx;
	uint8_t slave_addr;
	int ret;

	if (argc < 6) {
		return CMD_RET_USAGE;
	}

	idx = atoi(argv[1]);
	slave_addr = strtol(argv[2], NULL, 16);

	if (idx >= SCM_I2C_IDX_GPIO) {
		return CMD_RET_FAILURE;
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.role = SCM_I2C_ROLE_SLAVE;
	cfg.slave_addr = slave_addr;
	cfg.dma_en = 0;
	cfg.dma_en = atoi(argv[3]);
	cfg.pull_up_en = atoi(argv[4]);
	cfg.bitrate = atoi(argv[5]) * 1000;

	ret = scm_i2c_init(idx);
	if (ret) {
		printf("i2c init error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	ret = scm_i2c_configure(idx, &cfg, scm_cli_i2cs_notify, NULL);
	if (ret) {
		scm_i2c_deinit(idx);
		printf("i2c configure error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	i2cs_idx = idx;

#ifdef CONFIG_USE_DMA_ALLOCATION
	if (!i2cs_tx_buf) {
		i2cs_tx_buf = dma_malloc(I2C_BUF_LEN);
		i2cs_rx_buf = dma_malloc(I2C_BUF_LEN);
	}
#else
	i2cs_tx_buf = g_i2cs_tx_buf;
	i2cs_rx_buf = g_i2cs_rx_buf;
#endif

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cs_deinit(int argc, char *argv[])
{
	int ret;

	ret = scm_i2c_deinit(i2cs_idx);
	if (ret) {
		printf("i2c deinit error %x\n", ret);
		return CMD_RET_FAILURE;
	}
	i2cs_idx = -1;

#ifdef CONFIG_USE_DMA_ALLOCATION
	if (i2cs_tx_buf) {
		dma_free(i2cs_tx_buf);
		dma_free(i2cs_rx_buf);
		i2cs_tx_buf = NULL;
		i2cs_rx_buf = NULL;
	}
#endif

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cs_txbuf(int argc, char *argv[])
{
	if (!i2cs_tx_buf) {
		return CMD_RET_FAILURE;
	}

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	argc--;
	argv++;
	scm_cli_i2c_set_buf(argc, argv, i2cs_tx_buf);

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cs_rxbuf(int argc, char *argv[])
{
	if (!i2cs_rx_buf) {
		return CMD_RET_FAILURE;
	}

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	argc--;
	argv++;
	scm_cli_i2c_set_buf(argc, argv, i2cs_rx_buf);

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cs_clrbuf(int argc, char *argv[])
{
	if (!i2cs_tx_buf || !i2cs_rx_buf) {
		return CMD_RET_FAILURE;
	}

	memset(i2cs_tx_buf, 0, I2C_BUF_LEN);
	memset(i2cs_rx_buf, 0, I2C_BUF_LEN);

	return CMD_RET_SUCCESS;
}

static int scm_cli_i2cs_dump(int argc, char *argv[])
{
	if (!i2cs_tx_buf || !i2cs_rx_buf) {
		return CMD_RET_FAILURE;
	}

	printf("i2cs tx buf: ");
	scm_cli_i2c_dump_buf(i2cs_tx_buf, I2C_BUF_LEN);
	printf("i2cs rx buf: ");
	scm_cli_i2c_dump_buf(i2cs_rx_buf, I2C_BUF_LEN);

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_cli_i2cs_cmd[] = {
	CMDENTRY(init, scm_cli_i2cs_init, "", ""),
	CMDENTRY(deinit, scm_cli_i2cs_deinit, "", ""),
	CMDENTRY(txbuf, scm_cli_i2cs_txbuf, "", ""),
	CMDENTRY(rxbuf, scm_cli_i2cs_rxbuf, "", ""),
	CMDENTRY(clrbuf, scm_cli_i2cs_clrbuf, "", ""),
	CMDENTRY(dump, scm_cli_i2cs_dump, "", ""),
};

static int do_scm_cli_i2cs(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], scm_cli_i2cs_cmd, ARRAY_SIZE(scm_cli_i2cs_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(i2cs, do_scm_cli_i2cs,
	"I2C slave test",
	"i2cs init idx slave_addr:hex dma(0|1) pull_up(0|1) bitrate(in Kbps)\n"
	"\t<dma>\n"
	"\tthis is operational parameter. if dma is included and set 1,\n"
	"\tI2C TX or RX use DMA." OR
	"i2cs deinit" OR
	"i2cs txbuf <data:hex> <data:hex> ...." OR
	"i2cs rxbuf <data:hex> <data:hex> ...." OR
	"i2cs clrbuf" OR
	"i2cs dump"
);

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
#include "scm_spi.h"

static uint8_t spi_ms_txbuf_set;
static uint8_t spi_sl_txbuf_set;

static uint8_t *spi_ms_txbuf;
static uint8_t *spi_ms_rxbuf;
static uint8_t *spi_sl_txbuf;
static uint8_t *spi_sl_rxbuf;

#ifndef CONFIG_USE_DMA_ALLOCATION
static uint8_t g_spi_ms_txbuf[SCM_SPI_TRANSFER_MAX_LEN] __attribute__((section(".dma_buffer")));
static uint8_t g_spi_ms_rxbuf[SCM_SPI_TRANSFER_MAX_LEN] __attribute__((section(".dma_buffer")));
static uint8_t g_spi_sl_txbuf[SCM_SPI_TRANSFER_MAX_LEN] __attribute__((section(".dma_buffer")));
static uint8_t g_spi_sl_rxbuf[SCM_SPI_TRANSFER_MAX_LEN] __attribute__((section(".dma_buffer")));
#endif

struct cli_spi_ctx {
    enum scm_spi_idx idx;
};

struct cli_spi_ctx g_spi_ctx;

static const char *active_mode[4] = {
    "active high, odd edge sampling",
    "active high, even edge sampling",
    "active low, odd edge sampling",
    "active low, even edge sampling",
};

static const char *io_prop[3] = {
    "single io",
    "dual io",
    "quad io",
};

static struct scm_spi_cfg scm_cli_spi_cfg = {
    .role = SCM_SPI_ROLE_MASTER,
    .mode = SCM_SPI_MODE_0,
    .data_io_format = SCM_SPI_DATA_IO_FORMAT_SINGLE,
    .bit_order = SCM_SPI_BIT_ORDER_MSB_FIRST,
    .slave_extra_dummy_cycle = SCM_SPI_DUMMY_CYCLE_NONE,
    .master_cs_bitmap = 0,
    .clk_src = SCM_SPI_CLK_SRC_XTAL,
    .clk_div_2mul = 20,
    .dma_en = 0,
};

static int scm_cli_spi_notify(struct scm_spi_event *event, void *ctx)
{
    struct cli_spi_ctx *spi_ctx = ctx;
    int ret;

    switch (event->type) {
        case SCM_SPI_EVENT_SLAVE_TX_REQUEST:
            ret = scm_spi_slave_set_tx_buf(spi_ctx->idx, spi_sl_txbuf, SCM_SPI_TRANSFER_MAX_LEN);
            if (ret) {
                printk("slave set tx buffer error\n");
            }
            break;
        case SCM_SPI_EVENT_SLAVE_RX_REQUEST:
            ret = scm_spi_slave_set_rx_buf(spi_ctx->idx, spi_sl_rxbuf, SCM_SPI_TRANSFER_MAX_LEN);
            if (ret) {
                printk("slave set rx buffer error\n");
            }
            break;
        case SCM_SPI_EVENT_SLAVE_USER_CMD:
            break;
        case SCM_SPI_EVENT_SLAVE_TRANS_CMPL:
            printk("slave transfer complete\n");
            printk("err       : %d\n", event->data.sl_cmpl.err);
            printk("rx amount : %d\n", event->data.sl_cmpl.rx_amount);
            printk("tx amount : %d\n", event->data.sl_cmpl.tx_amount);
            break;
        case SCM_SPI_EVENT_MASTER_TRANS_CMPL:
            printk("master transfer complete\n");
            break;
        default:
            printk("unknown event type : %d\n", event->type);
            break;
    }
    return 0;
}

static int scm_cli_spic_init(int argc, char *argv[])
{
    int idx;
    int ret;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    idx = (enum scm_spi_idx)atoi(argv[1]);
    ret = scm_spi_init(idx);
    if (ret) {
        printf("spi init error %x\n", ret);
        return CMD_RET_FAILURE;
    }

#ifdef CONFIG_USE_DMA_ALLOCATION
    if (!spi_ms_txbuf) {
        spi_ms_txbuf = dma_malloc(SCM_SPI_TRANSFER_MAX_LEN);
        spi_ms_rxbuf = dma_malloc(SCM_SPI_TRANSFER_MAX_LEN);
        spi_sl_txbuf = dma_malloc(SCM_SPI_TRANSFER_MAX_LEN);
        spi_sl_rxbuf = dma_malloc(SCM_SPI_TRANSFER_MAX_LEN);
    }
#else
    spi_ms_txbuf = g_spi_ms_txbuf;
    spi_ms_rxbuf = g_spi_ms_rxbuf;
    spi_sl_txbuf = g_spi_sl_txbuf;
    spi_sl_rxbuf = g_spi_sl_rxbuf;
#endif

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_deinit(int argc, char *argv[])
{
    int idx;
    int ret;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    idx = (enum scm_spi_idx)atoi(argv[1]);
    ret = scm_spi_deinit(idx);
    if (ret) {
        printf("spi deinit error %x\n", ret);
        return CMD_RET_FAILURE;
    }

#ifdef CONFIG_USE_DMA_ALLOCATION
    if (spi_ms_txbuf) {
        dma_free(spi_ms_txbuf);
        dma_free(spi_ms_rxbuf);
        dma_free(spi_sl_txbuf);
        dma_free(spi_sl_rxbuf);

        spi_ms_txbuf = NULL;
        spi_ms_rxbuf = NULL;
        spi_sl_txbuf = NULL;
        spi_sl_rxbuf = NULL;
    }
#endif

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_role(int argc, char *argv[])
{
    int role;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    role = atoi(argv[1]);

    if (role) {
        scm_cli_spi_cfg.role = SCM_SPI_ROLE_SLAVE;
    } else {
        scm_cli_spi_cfg.role = SCM_SPI_ROLE_MASTER;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_mode(int argc, char *argv[])
{
    int mode;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    mode = atoi(argv[1]);
    if (mode > SCM_SPI_MODE_3) {
        return CMD_RET_USAGE;
    }

    scm_cli_spi_cfg.mode = mode;

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_io_mode(int argc, char *argv[])
{
    int data_io_mode;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    data_io_mode = atoi(argv[1]);
    if (data_io_mode > SCM_SPI_DATA_IO_FORMAT_QUAD) {
        return CMD_RET_USAGE;
    }

    scm_cli_spi_cfg.data_io_format = data_io_mode;

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_dma(int argc, char *argv[])
{
    int en;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    en = atoi(argv[1]);

    if (en) {
        scm_cli_spi_cfg.dma_en = 1;
    } else {
        scm_cli_spi_cfg.dma_en = 0;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_slave_dummy_cycle(int argc, char *argv[])
{
    int cycle;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    cycle = atoi(argv[1]);
    if (cycle > 4) {
        return CMD_RET_USAGE;
    }

    scm_cli_spi_cfg.slave_extra_dummy_cycle = (enum scm_spi_dummy_cycle)cycle;

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_master_cs_bitmap(int argc, char *argv[])
{
    uint32_t bitmap;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    bitmap = strtoul(argv[1], NULL, 16);

    scm_cli_spi_cfg.master_cs_bitmap = bitmap;

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_clk_source(int argc, char *argv[])
{
    int clk_src;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    clk_src = atoi(argv[1]);
    if (clk_src > SCM_SPI_CLK_SRC_PLL) {
        return CMD_RET_USAGE;
    }

    scm_cli_spi_cfg.clk_src = clk_src;

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_clk_divide(int argc, char *argv[])
{
    uint8_t clk_div;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    clk_div = (uint8_t)atoi(argv[1]);

    scm_cli_spi_cfg.clk_div_2mul = clk_div;

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_bits(int argc, char *argv[])
{
    int bit_order;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    bit_order = atoi(argv[1]);
    if (bit_order > SCM_SPI_BIT_ORDER_LSB_FIRST) {
        return CMD_RET_USAGE;
    }

    scm_cli_spi_cfg.bit_order = bit_order;

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_show_configure(int argc, char *argv[])
{
    printf("role                   : %s\n", scm_cli_spi_cfg.role ? "slave" : "master");
    printf("mode                   : %s\n", active_mode[scm_cli_spi_cfg.mode]);
    printf("io format              : %s\n", io_prop[scm_cli_spi_cfg.data_io_format]);
    printf("bit order              : %s\n", scm_cli_spi_cfg.bit_order ? "lsb" : "msb");
    printf("slave extra dummy cycle: %d\n", scm_cli_spi_cfg.slave_extra_dummy_cycle);
    printf("master cs bitmap       : %x\n", scm_cli_spi_cfg.master_cs_bitmap);
    printf("clk source             : %s\n", scm_cli_spi_cfg.clk_src ? "pll" : "xtal");
    printf("clk div                : %d\n", scm_cli_spi_cfg.clk_div_2mul * 2);
    printf("dma                    : %s\n", scm_cli_spi_cfg.dma_en ? "true" : "false");

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_set_configure(int argc, char *argv[])
{
    enum scm_spi_idx idx;
    int ret;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);
    if (idx < SCM_SPI_IDX_MAX) {
        g_spi_ctx.idx = idx;
    }

    ret = scm_spi_configure(idx, &scm_cli_spi_cfg, scm_cli_spi_notify, &g_spi_ctx);
    if (ret) {
        printf("spi configure error %x\n", ret);
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_spic_reset(int argc, char *argv[])
{
    enum scm_spi_idx idx;
    int ret;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);

    ret = scm_spi_reset(idx);
    if (ret) {
        printf("spi reset error %x\n", ret);
    }

    return ret;
}

static const struct cli_cmd scm_cli_spic_cmd[] = {
    CMDENTRY(init, scm_cli_spic_init, "", ""),
    CMDENTRY(deinit, scm_cli_spic_deinit, "", ""),
    CMDENTRY(role, scm_cli_spic_role, "", ""),
    CMDENTRY(mode, scm_cli_spic_mode, "", ""),
    CMDENTRY(io, scm_cli_spic_io_mode, "", ""),
    CMDENTRY(dma, scm_cli_spic_dma, "", ""),
    CMDENTRY(dmyc, scm_cli_spic_slave_dummy_cycle, "", ""),
    CMDENTRY(csmap, scm_cli_spic_master_cs_bitmap, "", ""),
    CMDENTRY(clks, scm_cli_spic_clk_source, "", ""),
    CMDENTRY(clkd, scm_cli_spic_clk_divide, "", ""),
    CMDENTRY(bits, scm_cli_spic_bits, "", ""),
    CMDENTRY(show, scm_cli_spic_show_configure, "", ""),
    CMDENTRY(cfg, scm_cli_spic_set_configure, "", ""),
    CMDENTRY(rst, scm_cli_spic_reset, "", ""),
};

static int do_scm_cli_spi_config(int argc, char *argv[])
{
    const struct cli_cmd *cmd;

    argc--;
    argv++;

    cmd = cli_find_cmd(argv[0], scm_cli_spic_cmd, ARRAY_SIZE(scm_cli_spic_cmd));
    if (cmd == NULL)
        return CMD_RET_USAGE;

    return cmd->handler(argc, argv);
}

CMD(spic, do_scm_cli_spi_config,
        "CLI command for SPI configuration",
        "spic init <idx>" OR
        "spic deinit <idx>" OR
        "spic role <role> \n"
        "\t0: master\n"
        "\t1: slave" OR
        "spic mode <mode>\n"
        "\t0: active high, odd  edge\n"
        "\t1: active high, even edge\n"
        "\t2: active low,  odd  edge\n"
        "\t3: active low,  even edge" OR
        "spic io <format>" OR
        "\t0: single\n"
        "\t1: dual\n"
        "\t2: qaud" OR
        "spic dma <enable>\n"
        "\t0: disable\n"
        "\t1: enable" OR
        "spic dmyc <cycle>\n"
        "\tslave extra dummy cycle. value is valid up to 4\n"
"\tcycle count is vary depending on IO format" OR
"spic csmap <cs bitmap in hex>\n"
"\tgpio bitmap for chip-selects.\n"
"\tonly applicable to a master" OR
"spic clks <clock source>\n"
"\t0: XTAL\n"
"\t1: PLL" OR
"spic clkd <clock div value>\n"
"\tvalue times 2" OR
"spic bits <order>\n"
"\t0: MSB first\n"
"\t1: LSB first" OR
"spic show"
"\tshow configuration" OR
"spic cfg <idx>"
"\tsetup configuration" OR
"spic rst <idx>"
"\tspi reset"
);

static int scm_cli_spim_tx(int argc, char *argv[])
{
    enum scm_spi_idx idx;
    int slave;
    uint32_t timeout;
    int len;
    int ret;
    int i;

    if (!spi_ms_txbuf) {
        return CMD_RET_FAILURE;
    }

    if (argc < 4) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);
    slave = atoi(argv[2]);
    len = atoi(argv[3]);
    if (len > SCM_SPI_TRANSFER_MAX_LEN) {
        return CMD_RET_USAGE;
    }

    if (!spi_ms_txbuf_set) {
        for (i = 0; i < len; i++) {
            if (i >= 0xFF) {
                spi_ms_txbuf[i] = SCM_SPI_TRANSFER_MAX_LEN - (i + 1);
            } else {
                spi_ms_txbuf[i] = i;
            }
        }
    }

    if (argc == 5) {
        timeout = atoi(argv[4]);
        ret = scm_spi_master_tx(idx, slave, spi_ms_txbuf, len, timeout);
        if (ret) {
            printf("spi master tx error %x\n", ret);
            return CMD_RET_FAILURE;
        }
    } else {
        ret = scm_spi_master_tx_async(idx, slave, spi_ms_txbuf, len);
        if (ret) {
            printf("spi master tx error %x\n", ret);
            return CMD_RET_FAILURE;
        }
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_spim_rx(int argc, char *argv[])
{
    enum scm_spi_idx idx;
    int slave;
    uint32_t timeout;
    int len;
    int ret;

    if (!spi_ms_rxbuf) {
        return CMD_RET_FAILURE;
    }

    if (argc < 4) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);
    slave = atoi(argv[2]);
    len = atoi(argv[3]);

    if (argc == 5) {
        timeout = atoi(argv[4]);
        ret = scm_spi_master_rx(idx, slave, spi_ms_rxbuf, len, timeout);
        if (ret) {
            printf("spi master rx error %x\n", ret);
            return CMD_RET_FAILURE;
        }
    } else {
        ret = scm_spi_master_rx_async(idx, slave, spi_ms_rxbuf, len);
        if (ret) {
            printf("spi master rx error %x\n", ret);
            return CMD_RET_FAILURE;
        }
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_spim_tx_rx(int argc, char *argv[])
{
    enum scm_spi_idx idx;
    int slave;
    uint32_t timeout;
    int trx_mode;
    int tx_len;
    int rx_len;
    int i;
    int ret;

    if (!spi_ms_txbuf || !spi_ms_rxbuf) {
        return CMD_RET_FAILURE;
    }

    if (argc < 6) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);
    slave = atoi(argv[2]);
    trx_mode = atoi(argv[3]);
    if (trx_mode > SCM_SPI_TRX_RX_FIRST) {
        return CMD_RET_USAGE;
    }
    tx_len = atoi(argv[4]);
    rx_len = atoi(argv[5]);
    if (tx_len > SCM_SPI_TRANSFER_MAX_LEN ||
            rx_len > SCM_SPI_TRANSFER_MAX_LEN) {
        return CMD_RET_USAGE;
    }

    if (!spi_ms_txbuf_set) {
        for (i = 0; i < tx_len; i++) {
            spi_ms_txbuf[i] = i;
        }
    }

    if (argc == 7) {
        timeout = atoi(argv[6]);
        ret = scm_spi_master_tx_rx(idx, slave, trx_mode, spi_ms_txbuf, tx_len,
                spi_ms_rxbuf, rx_len, timeout);
        if (ret) {
            printf("spi master tx rx error %x\n", ret);
            return CMD_RET_FAILURE;
        }
    } else {
        ret = scm_spi_master_tx_rx_async(idx, slave, trx_mode, spi_ms_txbuf, tx_len,
                spi_ms_rxbuf, rx_len);
        if (ret) {
            printf("spi master tx rx error %x\n", ret);
            return CMD_RET_FAILURE;
        }
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_spim_tx_with_cmd(int argc, char *argv[])
{
    enum scm_spi_idx idx;
    int slave;
    struct scm_spi_cmd_cfg cmd_cfg;
    uint32_t timeout;
    int len;
    int i;
    int ret;

    if (!spi_ms_txbuf) {
        return CMD_RET_FAILURE;
    }


    if (argc < 9) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);
    slave = atoi(argv[2]);

    cmd_cfg.cmd = (uint8_t)strtol(argv[3], NULL , 16);
    cmd_cfg.addr_len= (enum scm_spi_addr_len)strtol(argv[4], NULL , 16);
    if (cmd_cfg.addr_len > SCM_SPI_ADDR_LEN_4BYTES) {
        return CMD_RET_USAGE;
    }
    cmd_cfg.addr_io_format = (enum scm_spi_addr_io_format)strtol(argv[5], NULL , 16);
    if (cmd_cfg.addr_io_format > SCM_SPI_ADDR_IO_SAME_DATA) {
        return CMD_RET_USAGE;
    }
    cmd_cfg.addr = (uint32_t)strtol(argv[6], NULL , 16);
    cmd_cfg.dummy_cycle = (enum scm_spi_dummy_cycle)strtol(argv[7], NULL , 16);
    if (cmd_cfg.dummy_cycle > SCM_SPI_DUMMY_CYCLE_QUAD_IO_8) {
        return CMD_RET_USAGE;
    }

    len = atoi(argv[8]);
    if (len > SCM_SPI_TRANSFER_MAX_LEN) {
        return CMD_RET_USAGE;
    }

    if (!spi_ms_txbuf_set) {
        for (i = 0; i < len; i++) {
            if (i >= 0xFF) {
                spi_ms_txbuf[i] = SCM_SPI_TRANSFER_MAX_LEN - (i + 1);
            } else {
                spi_ms_txbuf[i] = i;
            }
        }
    }

    if (argc == 10) {
        timeout = atoi(argv[9]);
        ret = scm_spi_master_tx_with_cmd(idx, slave, &cmd_cfg, spi_ms_txbuf, len, timeout);
        if (ret) {
            printf("spi master tx with cmd error %x\n", ret);
            return CMD_RET_FAILURE;
        }
    } else {
        ret = scm_spi_master_tx_with_cmd_async(idx, slave, &cmd_cfg, spi_ms_txbuf, len);
        if (ret) {
            printf("spi master tx with cmd error %x\n", ret);
            return CMD_RET_FAILURE;
        }
    }


    return CMD_RET_SUCCESS;
}

static int scm_cli_spim_rx_with_cmd(int argc, char *argv[])
{
    enum scm_spi_idx idx;
    int slave;
    struct scm_spi_cmd_cfg cmd_cfg;
    uint32_t timeout;
    int len;
    int ret;

    if (!spi_ms_rxbuf) {
        return CMD_RET_FAILURE;
    }

    if (argc < 9) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);
    slave = atoi(argv[2]);

    cmd_cfg.cmd = (uint8_t)strtol(argv[3], NULL , 16);
    cmd_cfg.addr_len= (enum scm_spi_addr_len)strtol(argv[4], NULL , 16);
    if (cmd_cfg.addr_len > SCM_SPI_ADDR_LEN_4BYTES) {
        return CMD_RET_USAGE;
    }
    cmd_cfg.addr_io_format = (enum scm_spi_addr_io_format)strtol(argv[5], NULL , 16);
    if (cmd_cfg.addr_io_format > SCM_SPI_ADDR_IO_SAME_DATA) {
        return CMD_RET_USAGE;
    }
    cmd_cfg.addr = (uint32_t)strtol(argv[6], NULL , 16);
    cmd_cfg.dummy_cycle = (enum scm_spi_dummy_cycle)strtol(argv[7], NULL , 16);
    if (cmd_cfg.dummy_cycle > SCM_SPI_DUMMY_CYCLE_QUAD_IO_8) {
        return CMD_RET_USAGE;
    }

    len = atoi(argv[8]);

    if (argc == 10) {
        timeout = atoi(argv[9]);
        ret = scm_spi_master_rx_with_cmd(idx, slave, &cmd_cfg, spi_ms_rxbuf, len, timeout);
        if (ret) {
            printf("spi master rx with cmd error %x\n", ret);
            return CMD_RET_FAILURE;
        }
    } else {
        ret = scm_spi_master_rx_with_cmd_async(idx, slave, &cmd_cfg, spi_ms_rxbuf, len);
        if (ret) {
            printf("spi master rx with cmd error %x\n", ret);
            return CMD_RET_FAILURE;
        }
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_spim_set_tx_buf(int argc, char *argv[])
{
    int i;

    if (!spi_ms_txbuf) {
        return CMD_RET_FAILURE;
    }

    for (i = 1; i < argc; i++) {
        spi_ms_txbuf[i - 1] = (uint32_t)strtol(argv[i], NULL , 16);
    }

    spi_ms_txbuf_set = 1;

    return CMD_RET_SUCCESS;
}

static int scm_cli_spim_clr_buf(int argc, char *argv[])
{
    if (!spi_ms_txbuf || !spi_ms_rxbuf) {
        return CMD_RET_FAILURE;
    }

    spi_ms_txbuf_set = 0;
    memset(spi_ms_txbuf, 0, SCM_SPI_TRANSFER_MAX_LEN);
    memset(spi_ms_rxbuf, 0, SCM_SPI_TRANSFER_MAX_LEN);

    return CMD_RET_SUCCESS;
}

static int scm_cli_spim_dump(int argc, char *argv[])
{
    int len;
    int i;

    if (!spi_ms_txbuf || !spi_ms_rxbuf) {
        return CMD_RET_FAILURE;
    }

    if (argc <  2) {
        return CMD_RET_USAGE;
    }

    len = atoi(argv[1]);
    if (len > SCM_SPI_TRANSFER_MAX_LEN) {
        return CMD_RET_USAGE;
    }

    printf("TX buffer\n");
    for (i = 0; i < len; i++) {
        if (i != 0 && (i % 16) == 0) {
            printf("\n%02x ", spi_ms_txbuf[i]);
        } else {
            printf("%02x ", spi_ms_txbuf[i]);
        }
    }
    printf("\n");
    printf("RX buffer\n");
    for (i = 0; i < len; i++) {
        if (i != 0 && (i % 16) == 0) {
            printf("\n%02x ", spi_ms_rxbuf[i]);
        } else {
            printf("%02x ", spi_ms_rxbuf[i]);
        }
    }
    printf("\n");

    return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_cli_spim_cmd[] = {
    CMDENTRY(tx, scm_cli_spim_tx, "", ""),
    CMDENTRY(rx, scm_cli_spim_rx, "", ""),
    CMDENTRY(trx, scm_cli_spim_tx_rx, "", ""),
    CMDENTRY(tx_cmd, scm_cli_spim_tx_with_cmd, "", ""),
    CMDENTRY(rx_cmd, scm_cli_spim_rx_with_cmd, "", ""),
    CMDENTRY(txbuf, scm_cli_spim_set_tx_buf, "", ""),
    CMDENTRY(clrbuf, scm_cli_spim_clr_buf, "", ""),
    CMDENTRY(dump, scm_cli_spim_dump, "", ""),
};

static int do_scm_cli_spi_master(int argc, char *argv[])
{
    const struct cli_cmd *cmd;

    argc--;
    argv++;

    cmd = cli_find_cmd(argv[0], scm_cli_spim_cmd, ARRAY_SIZE(scm_cli_spim_cmd));
    if (cmd == NULL)
        return CMD_RET_USAGE;

    return cmd->handler(argc, argv);
}

CMD(spim, do_scm_cli_spi_master,
        "SPI master transfer test",
        "spim tx <idx> <slave> <len> <timeout>\n"
        "\t<timeout>\n"
        "\tthis is operational parameter. if timeout is included in argument,\n"
        "\twait spi transfer complete(unit:ms)." OR
        "spim rx <idx> <slave> <len> <timeout>" OR
        "spim trx <idx> <slave> <txmode> <tx_len> <rx_len> <timeout>\n"
        "\t<trxmode>\n"
        "\t0:TX / RX same time\n"
        "\t1:TX and RX \n"
        "\t2:RX and TX" OR
        "spim tx_cmd <idx> <slave> <cmd:hex> <addr_len> <addr_format> <addr:hex> <dummy_cnt> <len> <timeout>\n"
        "\t<addr_len>\n"
        "\t0:no use address\n"
        "\t1:1bytes address\n"
        "\t2:2bytes address\n"
        "\t3:3bytes address\n"
        "\t4:4bytes address\n"
        "\t<addr_format>\n"
        "\t0:single io\n"
"\t1:same data io\n"
"\t<dummy_cnt>\n"
"\tvalid value range : 0 ~ 4.\n"
"\tSPI clock cycle after address phase, please refer API document." OR
"spim rx_cmd <idx> <slave> <cmd:hex> <addr_len> <addr_format> <addr:hex> <dummy_cnt> <len> <timeout>" OR
"spim txbuf <data:hex> <data:hex> ....\n"
"\tset data to tx buffer, argument max count =< 30.\n"
"\tif txbuf is not set by user, txbuf is filled increment data from 0" OR
"spim clrbuf" OR
"spim dump <len>\n"
"\ttxbuf and rxbuf dump"
);


static int scm_cli_spis_prepare_tx(int argc, char *argv[])
{
    enum scm_spi_idx idx;
    int len;
    int i;
    int ret;

    if (!spi_sl_txbuf) {
        return CMD_RET_FAILURE;
    }

    if (argc < 3) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);
    len = atoi(argv[2]);
    if (len > SCM_SPI_TRANSFER_MAX_LEN) {
        return CMD_RET_USAGE;
    }

    if (!spi_sl_txbuf_set) {
        for (i = 0; i < len; i++) {
            if (i >= 0xFF) {
                spi_sl_txbuf[i] = SCM_SPI_TRANSFER_MAX_LEN - (i + 1);
            } else {
                spi_sl_txbuf[i] = i;
            }
        }
    }

    ret = scm_spi_slave_set_tx_buf(idx, spi_sl_txbuf, len);
    if (ret) {
        printf("spi slave set tx buf error %x\n", ret);
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_spis_prepare_rx(int argc, char *argv[])
{
    enum scm_spi_idx idx;
    int len;
    int ret;

    if (!spi_sl_rxbuf) {
        return CMD_RET_FAILURE;
    }

    if (argc < 3) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);
    len = atoi(argv[2]);
    ret = scm_spi_slave_set_rx_buf(idx, spi_sl_rxbuf, len);
    if (ret) {
        printf("spi slave set rx buf error %x (%d)\n", ret, errno);
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_spis_prepare_tx_rx(int argc, char *argv[])
{
    enum scm_spi_idx idx;
    int trx_mode;
    int tx_len;
    int rx_len;
    int i;
    int ret;

    if (!spi_sl_txbuf || !spi_sl_rxbuf) {
        return CMD_RET_FAILURE;
    }

    if (argc < 5) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);
    trx_mode = atoi(argv[2]);
    if (trx_mode > SCM_SPI_TRX_RX_FIRST) {
        return CMD_RET_USAGE;
    }
    tx_len = atoi(argv[3]);
    rx_len = atoi(argv[4]);


    if (tx_len > SCM_SPI_TRANSFER_MAX_LEN ||
            rx_len > SCM_SPI_TRANSFER_MAX_LEN) {
        return CMD_RET_USAGE;
    }

    if (!spi_sl_txbuf_set) {
        for (i = 0; i < tx_len; i++) {
            if (i >= 0xFF) {
                spi_sl_txbuf[i] = SCM_SPI_TRANSFER_MAX_LEN - (i + 1);
            } else {
                spi_sl_txbuf[i] = i;
            }
        }
    }

    ret = scm_spi_slave_set_tx_rx_buf(idx, trx_mode, spi_sl_txbuf, tx_len, spi_sl_rxbuf, rx_len);
    if (ret) {
        printf("spi slave set tx rx buf error %x\n", ret);
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_spis_cancel(int argc, char *argv[])
{
    enum scm_spi_idx idx;
    int ret;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);
    ret = scm_spi_slave_cancel(idx);
    if (ret) {
        printf("spi slave cancel error %x\n", ret);
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_spis_set_user_state(int argc, char *argv[])
{
    enum scm_spi_idx idx;
    uint16_t user_state;
    int ret;

    if (argc < 3) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);

    user_state = (uint16_t)atoi(argv[2]);
    ret = scm_spi_slave_set_user_state(idx, user_state);
    if (ret) {
        printf("spi slave set user state error %x\n", ret);
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_spis_set_tx_buf(int argc, char *argv[])
{
    int i;

    if (!spi_sl_txbuf) {
        return CMD_RET_FAILURE;
    }

    for (i = 1; i < argc; i++) {
        spi_sl_txbuf[i - 1] = (uint32_t)strtol(argv[i], NULL , 16);
    }

    spi_sl_txbuf_set = 1;

    return CMD_RET_SUCCESS;
}

static int scm_cli_spis_clr_buf(int argc, char *argv[])
{
    if (!spi_sl_txbuf || !spi_sl_rxbuf) {
        return CMD_RET_FAILURE;
    }

    spi_sl_txbuf_set = 0;
    memset(spi_sl_txbuf, 0, SCM_SPI_TRANSFER_MAX_LEN);
    memset(spi_sl_rxbuf, 0, SCM_SPI_TRANSFER_MAX_LEN);

    return CMD_RET_SUCCESS;
}

static int scm_cli_spis_buf_dump(int argc, char *argv[])
{
    int len;
    int i;

    if (!spi_sl_txbuf || !spi_sl_rxbuf) {
        return CMD_RET_FAILURE;
    }

    if (argc <  2) {
        return CMD_RET_USAGE;
    }

    len = atoi(argv[1]);
    if (len > SCM_SPI_TRANSFER_MAX_LEN) {
        return CMD_RET_USAGE;
    }

    printf("TX buffer\n");
    for (i = 0; i < len; i++) {
        if (i != 0 && (i % 16) == 0) {
            printf("\n%02x ", spi_sl_txbuf[i]);
        } else {
            printf("%02x ", spi_sl_txbuf[i]);
        }
    }
    printf("\n");
    printf("RX buffer\n");
    for (i = 0; i < len; i++) {
        if (i != 0 && (i % 16) == 0) {
            printf("\n%02x ", spi_sl_rxbuf[i]);
        } else {
            printf("%02x ", spi_sl_rxbuf[i]);
        }
    }
    printf("\n");

    return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_cli_spis_cmd[] = {
    CMDENTRY(preptx, scm_cli_spis_prepare_tx, "", ""),
    CMDENTRY(preprx, scm_cli_spis_prepare_rx, "", ""),
    CMDENTRY(preptrx, scm_cli_spis_prepare_tx_rx, "", ""),
    CMDENTRY(cancel, scm_cli_spis_cancel, "", ""),
    CMDENTRY(ustate, scm_cli_spis_set_user_state, "", ""),
    CMDENTRY(txbuf, scm_cli_spis_set_tx_buf, "", ""),
    CMDENTRY(clrbuf, scm_cli_spis_clr_buf, "", ""),
    CMDENTRY(dump, scm_cli_spis_buf_dump, "", ""),
};

static int do_scm_cli_spi_slave(int argc, char *argv[])
{
    const struct cli_cmd *cmd;

    argc--;
    argv++;

    cmd = cli_find_cmd(argv[0], scm_cli_spis_cmd, ARRAY_SIZE(scm_cli_spis_cmd));
    if (cmd == NULL)
        return CMD_RET_USAGE;

    return cmd->handler(argc, argv);
}

CMD(spis, do_scm_cli_spi_slave,
        "SPI slave transfer test",
        "spis preptx <idx> <len>" OR
        "spis preprx <idx> <len>" OR
        "spis preptrx <idx> <txmode> <tx_len> <rx_len>\n"
        "\t0:TX / RX same time\n"
        "\t1:TX and RX \n"
        "\t2:RX and TX" OR
        "spis cancel <idx>" OR
        "spis ustate <idx> <state>" OR
        "spis txbuf <data:hex> <data:hex> ....\n"
        "\tset data to tx buffer, argument max count =< 30" OR
        "spis clrbuf" OR
        "spis dump <len>\n"
        "\ttxbuf and rxbuf dump"
   );

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
#include "cmsis_os.h"
#include "scm_uart.h"

static uint8_t uart_txbuf_set;
static uint8_t *uart_txbuf;
static uint8_t *uart_rxbuf;

#ifndef CONFIG_USE_DMA_ALLOCATION
static uint8_t g_uart_txbuf[SCM_UART_TRANSFER_MAX_LEN] __attribute__((section(".dma_buffer")));
static uint8_t g_uart_rxbuf[SCM_UART_TRANSFER_MAX_LEN] __attribute__((section(".dma_buffer")));
#endif

struct cli_uart_ctx {
    enum scm_uart_idx idx;
    osSemaphoreId_t completion;
};

static struct cli_uart_ctx g_uart_ctx[SCM_UART_IDX_MAX];

static struct scm_uart_cfg scm_cli_uart_cfg = {
    .baudrate = SCM_UART_BDR_115200,
    .data_bits = SCM_UART_DATA_BITS_8,
    .parity = SCM_UART_NO_PARITY,
    .stop_bits = SCM_UART_STOP_BIT_1,
    .dma_en = 0,
};

static int scm_cli_uart_notify(enum scm_uart_idx idx, struct scm_uart_event *event, void *ctx)
{
    struct cli_uart_ctx *uart_ctx = ctx;

    (void)idx;

    switch (event->type) {
        case SCM_UART_EVENT_TX_CMPL:
            printk("UART%d TX complete\n", uart_ctx->idx);
            if (event->err != SCM_UART_ERR_NO) {
                printk("Err : %02x\n", event->err);
            }
            break;
        case SCM_UART_EVENT_RX_CMPL:
            printk("UART%d RX complete\n", uart_ctx->idx);
            if (event->err != SCM_UART_ERR_NO) {
                printk("Err : %02x\n", event->err);
            }
            break;
        default:
            printk("unknown event type : %d\n", event->type);
            break;
    }
    osSemaphoreRelease(g_uart_ctx[idx].completion);
    return 0;
}

static int scm_cli_uart_init(int argc, char *argv[])
{
    int idx;
    int ret;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    scm_cli_uart_cfg.dma_en = 0;
    if (argc == 3) {
        scm_cli_uart_cfg.dma_en = atoi(argv[2]);
    }

    idx = (enum scm_uart_idx)atoi(argv[1]);
    g_uart_ctx[idx].idx = idx;
    g_uart_ctx[idx].completion = osSemaphoreNew(1, 0, NULL);
    if (!g_uart_ctx[idx].completion) {
        printf("osSemaphoreNew error\n");
        return CMD_RET_FAILURE;
    }

#ifdef CONFIG_USE_DMA_ALLOCATION
    if (!uart_txbuf) {
        uart_txbuf = dma_malloc(SCM_UART_TRANSFER_MAX_LEN);
        uart_rxbuf = dma_malloc(SCM_UART_TRANSFER_MAX_LEN);
        if ((uart_txbuf == NULL)  || (uart_rxbuf == NULL)) {
            dma_free(uart_txbuf);
            dma_free(uart_rxbuf);
            uart_txbuf = NULL;
            uart_rxbuf = NULL;
            printf("dma_malloc: error not enough dma memory\n");
            return CMD_RET_FAILURE;
        }
    }
#else
    uart_txbuf = g_uart_txbuf;
    uart_rxbuf = g_uart_rxbuf;
#endif

    ret = scm_uart_init(idx, &scm_cli_uart_cfg);
    if (ret) {
        printf("uart init: error %x\n", ret);
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_uart_deinit(int argc, char *argv[])
{
    int idx;
    int ret;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    idx = (enum scm_uart_idx)atoi(argv[1]);
    osSemaphoreDelete(g_uart_ctx[idx].completion);
    ret = scm_uart_deinit(idx);
    if (ret) {
        printf("uart deinit: error %x\n", ret);
        return CMD_RET_FAILURE;
    }

#ifdef CONFIG_USE_DMA_ALLOCATION
    if (uart_txbuf) {
        dma_free(uart_txbuf);
        dma_free(uart_rxbuf);
        uart_txbuf = NULL;
        uart_rxbuf = NULL;
    }
#endif

    return CMD_RET_SUCCESS;
}

static int scm_cli_uart_tx(int argc, char *argv[])
{
    enum scm_uart_idx idx;
    int timeout;
    int len;
    int ret;
    int i;

    if (!uart_txbuf) {
        return CMD_RET_FAILURE;
    }

    if (argc < 3) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);
    len = atoi(argv[2]);
    if (len > SCM_UART_TRANSFER_MAX_LEN) {
        printf("Error: the length[%u] > CONFIG_CLI_UARTBUF_MAXLEN[%u]\n",
            len, SCM_UART_TRANSFER_MAX_LEN);
        return CMD_RET_USAGE;
    }


    if (!uart_txbuf_set) {
        for (i = 0; i < len; i++) {
            uart_txbuf[i] = i + 0x30;
        }
    }

    if (argc == 4) {
        timeout = atoi(argv[3]);
        ret = scm_uart_tx(idx, uart_txbuf, len, timeout);
        if (ret) {
            printf("uart tx: error %x\n", ret);
            return CMD_RET_FAILURE;
        }
    } else {
        ret = scm_uart_tx_async(idx, uart_txbuf, len, scm_cli_uart_notify, &g_uart_ctx[idx]);
        if (ret) {
            printf("uart tx: error %x\n", ret);
            return CMD_RET_FAILURE;
        }
        osSemaphoreAcquire(g_uart_ctx[idx].completion, osWaitForever);
    }

    return CMD_RET_SUCCESS;
}

struct uart_cli_rx_arg {
    enum scm_uart_idx idx;
    bool async;
    int timeout;
    uint32_t len;
};

static void scm_cli_uart_rx_thread(void *argument)
{
    struct uart_cli_rx_arg *arg = argument;
    uint32_t len = arg->len;
    int ret;

    if (arg->async) {
        ret = scm_uart_rx_async(arg->idx, uart_rxbuf, len, scm_cli_uart_notify,
                &g_uart_ctx[arg->idx]);
        if (ret) {
            printf("uart rx: error %x\n", ret);
            goto exit;
        }
        osSemaphoreAcquire(g_uart_ctx[arg->idx].completion, osWaitForever);
    } else {
        ret = scm_uart_rx(arg->idx, uart_rxbuf, &len, arg->timeout);
        if (ret) {
            if (ret == WISE_ERR_TIMEOUT) {
                printf("uart rx: error %x with %d bytes received.\n", ret, len);
            } else {
                printf("uart rx: error %x\n", ret);
            }
            goto exit;
        }
        printf("uart rx: %d bytes received.\n", len);
    }

exit:
    free(arg);
    osThreadExit();
}

static int scm_cli_uart_rx(int argc, char *argv[])
{
    struct uart_cli_rx_arg *arg;
    osThreadId_t tid;

    if (!uart_rxbuf) {
        return CMD_RET_FAILURE;
    }

    if (argc < 3) {
        return CMD_RET_USAGE;
    }

    arg = zalloc(sizeof(*arg));

    arg->idx = atoi(argv[1]);
    arg->len = atoi(argv[2]);
    if (arg->len > SCM_UART_TRANSFER_MAX_LEN) {
        printf("Error: the length[%u] > CONFIG_CLI_UARTBUF_MAXLEN[%u]\n",
            arg->len, SCM_UART_TRANSFER_MAX_LEN);
        return CMD_RET_USAGE;
    }

    if (argc != 4) {
        arg->async = true;
    } else {
        arg->timeout = atoi(argv[3]);
    }

    tid = osThreadNew(scm_cli_uart_rx_thread, arg, NULL);
    if (tid == NULL) {
        printf("Cannot create an Rx thread.\n");
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_uart_reset(int argc, char *argv[])
{
    enum scm_uart_idx idx;
    int ret;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    idx = atoi(argv[1]);
    ret = scm_uart_reset(idx);
    if (ret) {
        printf("uart reset: error %x\n", ret);
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_uart_set_tx_buf(int argc, char *argv[])
{
    int i;

    if (!uart_txbuf) {
        return CMD_RET_FAILURE;
    }
    if (argc > (SCM_UART_TRANSFER_MAX_LEN + 1)) {
        printf("Error: the tx buf[%u] > CONFIG_CLI_UARTBUF_MAXLEN[%u]\n",
            argc, SCM_UART_TRANSFER_MAX_LEN);
        return CMD_RET_USAGE;
    }

    uart_txbuf_set = 1;

    for (i = 1; i < argc; i++) {
        uart_txbuf[i - 1] = (uint32_t)strtol(argv[i], NULL , 16);
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_uart_clr_buf(int argc, char *argv[])
{
    if (!uart_txbuf || !uart_rxbuf) {
        return CMD_RET_FAILURE;
    }

    uart_txbuf_set = 0;
    memset(uart_txbuf, 0, SCM_UART_TRANSFER_MAX_LEN);
    memset(uart_rxbuf, 0, SCM_UART_TRANSFER_MAX_LEN);

    return CMD_RET_SUCCESS;
}

static int scm_cli_uart_dump(int argc, char *argv[])
{
    int len;
    int i;

    if (!uart_txbuf || !uart_rxbuf) {
        return CMD_RET_FAILURE;
    }

    if (argc <  2) {
        return CMD_RET_USAGE;
    }

    len = atoi(argv[1]);
    if (len > SCM_UART_TRANSFER_MAX_LEN) {
        printf("Error: the length[%u] > CONFIG_CLI_UARTBUF_MAXLEN[%u]\n",
            len, SCM_UART_TRANSFER_MAX_LEN);
        return CMD_RET_USAGE;
    }

    printf("TX buffer\n");
    for (i = 0; i < len; i++) {
        if (i != 0 && (i % 16) == 0) {
            printf("\n%02x ", uart_txbuf[i]);
        } else {
            printf("%02x ", uart_txbuf[i]);
        }
    }
    printf("\n");
    printf("RX buffer\n");
    for (i = 0; i < len; i++) {
        if (i != 0 && (i % 16) == 0) {
            printf("\n%02x ", uart_rxbuf[i]);
        } else {
            printf("%02x ", uart_rxbuf[i]);
        }
    }
    printf("\n");

    return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_cli_uart_cmd[] = {
    CMDENTRY(init, scm_cli_uart_init, "", ""),
    CMDENTRY(deinit, scm_cli_uart_deinit, "", ""),
    CMDENTRY(tx, scm_cli_uart_tx, "", ""),
    CMDENTRY(rx, scm_cli_uart_rx, "", ""),
    CMDENTRY(reset, scm_cli_uart_reset, "", ""),
    CMDENTRY(txbuf, scm_cli_uart_set_tx_buf, "", ""),
    CMDENTRY(clrbuf, scm_cli_uart_clr_buf, "", ""),
    CMDENTRY(dump, scm_cli_uart_dump, "", ""),
};

static int do_scm_cli_uart(int argc, char *argv[])
{
    const struct cli_cmd *cmd;

    argc--;
    argv++;

    cmd = cli_find_cmd(argv[0], scm_cli_uart_cmd, ARRAY_SIZE(scm_cli_uart_cmd));
    if (cmd == NULL)
        return CMD_RET_USAGE;

    return cmd->handler(argc, argv);
}

CMD(uart, do_scm_cli_uart,
        "UART transfer test",
        "uart init <idx> <dma>\n"
        "\t<dma>\n"
        "\tthis is operational parameter. if dma is included and set 1,\n"
        "\tuart TX or RX use DMA." OR
        "uart deinit <idx>" OR
        "uart tx <idx> <len> <timeout>\n"
        "\t<timeout>\n"
        "\tthis is operational parameter. if timeout is included in argument,\n"
        "\twait uart transfer complete(unit:ms)." OR
        "uart rx <idx> <len> <timeout>\n"
        "\t<timeout>\n"
        "\tthis is operational parameter. if timeout is included in argument,\n"
        "\twait uart transfer complete(unit:ms)." OR
        "uart reset <idx>" OR
        "uart txbuf <data:hex> <data:hex> ....\n"
        "\tset data to tx buffer, argument max count =< 30.\n"
        "\tif txbuf is not set by user, txbuf is filled increment data from 0" OR
        "uart clrbuf" OR
        "uart dump <len>\n"
"\ttxbuf and rxbuf dump"
);

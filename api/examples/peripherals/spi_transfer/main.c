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

/*
 * This example demonstrates the use of the SPI in master and slave modes.
 *
 * [Hardware Setup]
 *
 * Connect SPI master and SPI slave pins on the board.
 * Default configuration for this demo is to use the EVB with the following pins.
 * 	SPI master using SCM_SPI_INDEX_2
 * 		CLK : GPIO 3
 * 		CS  : GPIO 2
 * 		DAT0: GPIO 4 (mosi)
 * 		DAT1: GPIO 5 (miso)
 * 		DAT2: GPIO 6 (wp)
 * 		DAT3: GPIO 7 (hold)
 * 	SPI slave using SCM_SPI_INDEX_1
 * 		CLK : GPIO 16
 * 		CS  : GPIO 15
 * 		DAT0: GPIO 17 (mosi)
 * 		DAT1: GPIO 18 (miso)
 * 		DAT2: GPIO 19 (wp)
 * 		DAT3: GPIO 20 (hold)
 *
 *
 * [Test]
 *
 * Test can check single, dual and quad IO transfer
 * Sequence
 * - SPI slave RX buffer prepare.
 * - SPI master TX data
 * - Confirm RX data in slave side
 *
 * - SPI slave TX buffer prepare.
 * - SPI master RX data
 * - Confirm RX data in master side
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "hal/kernel.h"

#include "cli.h"
#include "scm_spi.h"
#include "spi_test.h"
#include "cmsis_os.h"
#include "hal/console.h"

/* Structure to hold SPI event data for slave transaction completion */
struct scm_spi_event_slave_cmpl g_sl_cmpl_data;
osSemaphoreId_t g_spi_sem;  /* Semaphore ID for SPI synchronization */

/* Error and IO format strings for printing */
char *spi_err_str[3] = {
    "No Error",
    "Underrun",
    "Overrun",
};

char *spi_io_str[3] = {
    "Single IO",
    "Dual IO",
    "Quad IO",
};

int main(void)
{
    printf("SPI demo\n");

    spi_master_init();  /* Initialize SPI master */
    spi_slave_init();   /* Initialize SPI slave */

    /* Create a binary semaphore for SPI event synchronization */
    g_spi_sem = osSemaphoreNew(1, 0, NULL);

    return 0;
}

/* Common SPI callback */
static int spi_cb(struct scm_spi_event *event, void *ctx)
{
    switch (event->type) {
        case SCM_SPI_EVENT_SLAVE_TRANS_CMPL:
            printk("slave transfer complete\n");
            printk("err       : %d\n", event->data.sl_cmpl.err);
            printk("rx amount : %d\n", event->data.sl_cmpl.rx_amount);
            printk("tx amount : %d\n", event->data.sl_cmpl.tx_amount);

            memcpy(&g_sl_cmpl_data, &event->data.sl_cmpl, sizeof(struct scm_spi_event_slave_cmpl));
            osSemaphoreRelease(g_spi_sem);
            break;
        case SCM_SPI_EVENT_MASTER_TRANS_CMPL:
            printk("master transfer complete\n");
            break;
        case SCM_SPI_EVENT_SLAVE_TX_REQUEST:
        case SCM_SPI_EVENT_SLAVE_RX_REQUEST:
        case SCM_SPI_EVENT_SLAVE_USER_CMD:
            printk("event type : %d\n", event->type);
            break;
        default:
            printk("unknown event type : %d\n", event->type);
            break;
    }
    return 0;
}

static int wait_slave_complete(void)
{
    int ret;

    ret = osSemaphoreAcquire(g_spi_sem, 1000);

    return ret;
}

/* Function to perform SPI transfer tests */
int do_spi_transfer(int argc, char *argv[])
{
    enum scm_spi_data_io_format io_format;
    int count;
    int ret;
    int i;

    if (argc < 3) {
        return CMD_RET_USAGE;
    }

    io_format = (enum scm_spi_data_io_format)atoi(argv[1]);
    if (io_format > SCM_SPI_DATA_IO_FORMAT_QUAD) {
        printf("Err: IO format %d\n", io_format);
        return CMD_RET_USAGE;
    }

    count = atoi(argv[2]);
    if (count <= 0) {
        return CMD_RET_USAGE;
    }

    ret = spi_master_config(io_format, spi_cb);
    if (ret) {
        printf("Err: master configuration fail : %x\n",ret);
        return CMD_RET_FAILURE;
    }

    ret = spi_slave_config(io_format, spi_cb);
    if (ret) {
        printf("Err: slave configuration fail : %x\n",ret);
        return CMD_RET_FAILURE;
    }

    printf("Start SPI %s Test\n", spi_io_str[io_format]);

    for (i = 0; i < count; i++) {
        /* Set up and perform SPI transactions
         * This includes slave transmission preparation, master reception,
         * and then master transmission and slave reception
         * The process includes waiting for completion and error handling */
        ret = spi_slave_tx(i);
        if (ret) {
            printf("spi slave tx prepar err %x\n", ret);
            return CMD_RET_FAILURE;
        }

        ret = spi_master_rx(1000);
        if (ret) {
            printf("spi master rx err %x\n", ret);
            return CMD_RET_FAILURE;
        }

        ret = wait_slave_complete();
        if (ret != osOK) {
            printf("slave tx complete err: %d\n", ret);
            return CMD_RET_FAILURE;
        } else if (g_sl_cmpl_data.err) {
            printf("slave tx complete err: %s\n", spi_err_str[g_sl_cmpl_data.err]);
        }

        spi_master_recv_msg_print();

        ret = spi_slave_rx();
        if (ret) {
            printf("spi slave rx prepar err %x\n", ret);
            return CMD_RET_FAILURE;
        }

        spi_master_tx(i, 1000);
        if (ret) {
            printf("spi master tx err %x\n", ret);
            return CMD_RET_FAILURE;
        }

        ret = wait_slave_complete();
        if (ret != osOK) {
            printf("slave rx complete err: %s\n", ret);
            return CMD_RET_FAILURE;
        } else if (g_sl_cmpl_data.err) {
            printf("slave rx complete err: %s\n", spi_err_str[g_sl_cmpl_data.err]);
        }

        /* Must refer to rx_amount to get exact amount of data transffered. */
        spi_slave_recv_msg_print(g_sl_cmpl_data.rx_amount);

        osDelay(100); /* Delay between tests for stability */
    }

    return CMD_RET_SUCCESS;
}

/* Register 'spi_tr' command in the CLI for performing SPI transfer tests */
CMD(spi_tr, do_spi_transfer,
        "SPI transfer test",
        "spi_tr <io> <test count>\n"
        "\tio\n"
        "\t0:single io\n"
        "\t1:dual io\n"
        "\t2:quad io"
   );

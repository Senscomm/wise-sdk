/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef CONFIG_BLE_CLI_DTM

#ifdef CONFIG_NIMBLE_BLECLI
#error Need disable CONFIG_BLE_CLI_DTM, 'bt dtm-xxx' command is included in NIMBLE DEMO.
#endif

#include <stdio.h>
#include <stdlib.h>

#include <hal/kernel.h>
#include <hal/console.h>

#include "cli.h"
#include "nimble/hci_common.h"

extern int (*ble_ll_hci_dtm_tx_test_v2)(const uint8_t *cmdbuf, uint8_t len);
extern int (*ble_ll_hci_dtm_rx_test_v2)(const uint8_t *cmdbuf, uint8_t len);
extern int (*ble_ll_dtm_end_test)(uint8_t *rsp, uint8_t *rsplen);

static int do_bt_dtm_rx(int argc, char *argv[])
{
	uint8_t cmd_buf[32];
	struct ble_hci_le_rx_test_v2_cp *cmd = (struct ble_hci_le_rx_test_v2_cp *)cmd_buf;
	int rc;

	cmd->rx_chan = atoi(argv[1]);
	cmd->phy = atoi(argv[2]);
	rc = ble_ll_hci_dtm_rx_test_v2((const uint8_t *)cmd, sizeof(*cmd));

	printf("dtm rx : rc = %d\n", rc);

	return 0;
}

static int do_bt_dtm_tx(int argc, char *argv[])
{
	uint8_t cmd_buf[32];
	struct ble_hci_le_tx_test_v2_cp *cmd = (struct ble_hci_le_tx_test_v2_cp *)cmd_buf;
	int rc;

	cmd->tx_chan = atoi(argv[1]);
	cmd->test_data_len = atoi(argv[2]);
	cmd->payload = atoi(argv[3]);
	cmd->phy = atoi(argv[4]);
	rc = ble_ll_hci_dtm_tx_test_v2((const uint8_t *)cmd, sizeof(*cmd));

	printf("dtm tx : rc = %d\n", rc);

	return 0;
}

static int do_bt_dtm_end(int argc, char *argv[])
{
	uint8_t rsp_buf[32];
	uint8_t rsp_len;
	struct ble_hci_le_test_end_rp *rsp = (struct ble_hci_le_test_end_rp *)rsp_buf;
	int rc;

	rc = ble_ll_dtm_end_test(rsp_buf, &rsp_len);

	printf("dtm end: rc = %d, num=%d\n", rc, rsp->num_packets);

	return 0;
}

static const struct cli_cmd bt_dtm_cmd[] = {
	CMDENTRY(dtm-rx, do_bt_dtm_rx, "", ""),
	CMDENTRY(dtm-tx, do_bt_dtm_tx, "", ""),
	CMDENTRY(dtm-end, do_bt_dtm_end, "", ""),
};

static int do_bt_dtm(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], bt_dtm_cmd, ARRAY_SIZE(bt_dtm_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(bt, do_bt_dtm,
	"bt dtm test",
	"bt dtm-tx [channel] [len] [payload] [phy]" OR
	"bt dtm-rx [channel] [phy]" OR
	"bt dtm-end"
);

#endif /* CONFIG_BLE_CLI_DTM */

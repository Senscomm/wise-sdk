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

#ifdef CONFIG_BLE_CLI_CW

#include <stdio.h>
#include <stdlib.h>

#include <hal/kernel.h>
#include <hal/console.h>

#include "rf.h"
#include "cli.h"

static int do_bcw_start(int argc, char *argv[])
{
	int channel;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	channel = atoi(argv[1]);
	if (channel > 39) {
		return CMD_RET_USAGE;
	}

	rf_cw_start(channel);

	return CMD_RET_SUCCESS;
}

static int do_bcw_stop(int argc, char *argv[])
{
	rf_cw_stop();

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd bcw_cmd[] = {
	CMDENTRY(start, do_bcw_start, "", ""),
	CMDENTRY(stop, do_bcw_stop, "", ""),
};

static int do_bcw(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], bcw_cmd, ARRAY_SIZE(bcw_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(bcw, do_bcw,
		"BT continuous wave test",
		"bcw start <channel:0 ~ 39>" OR
		"bcw stop"
);

#endif

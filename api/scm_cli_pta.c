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
#include "cli.h"
#include "scm_pta.h"

#include "wise_err.h"

static char *pta_mode_str[] = {
	"FORCE MODE OFF",
	"FORCE MODE WIFI",
	"FORCE MODE BLE",
};

static int scm_cli_pta_get_force_mode(int argc, char *argv[])
{
	enum scm_pta_force_mode force_mode;
	int ret;

	ret = scm_pta_get_force_mode(&force_mode);
	if (ret != WISE_OK) {
		printf("failed get PTA force mode : %d\n", ret);
		return CMD_RET_FAILURE;
	}

	printf("Current PTA mode : %s\n", pta_mode_str[force_mode]);

	return CMD_RET_SUCCESS;
}

static int scm_cli_pta_set_force_mode(int argc, char *argv[])
{
	enum scm_pta_force_mode force_mode;
	int ret;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	force_mode = atoi(argv[1]);

	ret = scm_pta_set_force_mode(force_mode);
	if (ret != WISE_OK) {
		printf("failed set PTA force mode : %d\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_cli_pta_cmd[] = {
	CMDENTRY(get_force_mode, scm_cli_pta_get_force_mode, "", ""),
	CMDENTRY(set_force_mode, scm_cli_pta_set_force_mode, "", ""),
};

static int do_scm_cli_pta(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], scm_cli_pta_cmd, ARRAY_SIZE(scm_cli_pta_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(pta, do_scm_cli_pta,
		"CLI for PTA API test",
		"pta get_force_mode" OR
		"pta set_force_mode <mode>\n"
		"\t0: PTA force off\n"
		"\t1: PTA force WIFI\n"
		"\t2: PTA force BLE\n"
);

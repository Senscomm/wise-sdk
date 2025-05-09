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
#include "scm_crypto.h"

static int scm_cli_crypto_trng(int argc, char *argv[])
{
	uint8_t *buf;
	int len;
	int ret;
	int i;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	if (strncmp(argv[1], "read", 4)) {
		return CMD_RET_USAGE;
	}

	len = atoi(argv[2]);

	buf = malloc(len);
	if (!buf) {
		printf("no memory resource\n");
		return CMD_RET_FAILURE;
	}

	ret = scm_crypto_trng_read(buf, len);
	if (ret) {
		printf("crypto trng read error %x\n", ret);
		goto out;
	}

	printf("true random number\n");
	for (i = 0; i < len; i++) {
		if (i != 0 && (i % 16) == 0) {
			printf("\n0x%02x ", buf[i]);
		} else {
			printf("0x%02x ", buf[i]);
		}
	}
	printf("\n");

out:
	free(buf);

	if (ret) {
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_cli_crypto_cmd[] = {
	CMDENTRY(trng, scm_cli_crypto_trng, "", ""),
};

static int do_scm_cli_crypto(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], scm_cli_crypto_cmd, ARRAY_SIZE(scm_cli_crypto_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(crypto, do_scm_cli_crypto,
	"CLI for Crypto API test",
	"crypto trng read [len]"
);

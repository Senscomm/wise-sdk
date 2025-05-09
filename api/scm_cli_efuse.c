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
#include "scm_efuse.h"

/*
 * USAGE for eFuse buffer mode
 * eFuse buffer mode is needed for saving efuse data temporary.
 * We support 2 buffer mode.
 *  1. RAM buffer mode.
 *  2. FLASH buffer mode.
 *
 * efuse clear_buffer [bit offset] [bit length]
 * This command clear specific offset data in RAM or FLASH buffer mode.
 * This command is valid in RAM or FLASH buffer mode
 * ex) clear WIFI MAC address in flash buffer\n" \
 *  $ efuse set_mode 2\n" \
 *  $ efuse clear_buffer 576 48\n"
 *
 * efuse set_mode [mode]
 * This command selects one of three modes.
 * 0. efuse  mode      : This mode directly write or read efuse memory.
 * 1. RAM buffer mode  : This mode write or read RAM buffer same size of efuse memory.
 * 2. FLASH buffer mode: This mode write or read Flash memory buffer same size of efuse memory.
 * ex) write and read WIFI MAC address from flash buffer.
 *  $ efuse set_mode 2
 *  $ efuse write 576 48 64f947010203
 *  $ efuse read 576 48
 *
 * efuse get_mode
 * This command get current mode.
 *
 * efuse sync
 * This command save all buffer data to efuse (depend on current mode).
 * This command is valid in RAM or FLASH buffer mode
 * ex) data save from flash buffer to efuse
 *  $ efuse set_mode 2
 *  $ efuse sync
 *
 * efuse load
 * This command load all efuse data to buffer (depend on current mode).
 * This command is valid in RAM or FLASH buffer mode
 * ex) data load from efuse to flash buffer
 *  $ efuse set_mode 2
 *  $ efuse load
 *
 */

#define EFUSE_ROW_MAX		32
#define EFUSE_WORD_PER_ROW	4
#define EFUSE_TOTAL_BIT		(EFUSE_ROW_MAX * EFUSE_WORD_PER_ROW * 8)

static uint8_t buf[EFUSE_TOTAL_BIT / 8];

static int scm_cli_efuse_read(int argc, char *argv[])
{
	uint32_t bit_offset;
	uint32_t bit_count;
	uint8_t buf_len;
	int i;
	int ret;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	bit_offset = atoi(argv[1]);
	bit_count = atoi(argv[2]);

	printf(" bit_offset = %d, bit_count = %d\n", bit_offset, bit_count);

	ret = scm_efuse_read(bit_offset, bit_count, buf);
	if (ret) {
		printf("efuse read error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	buf_len = (bit_count / 8);
	if (bit_count % 8) {
		buf_len += 1;
	}

	printf(" value:\n");
	for (i = 0; i < buf_len; i++) {
		printf(" %03d: %02x\n", i, buf[i]);
	}

	return CMD_RET_SUCCESS;
}

static int scm_cli_efuse_write(int argc, char *argv[])
{
	uint32_t bit_offset;
	uint32_t bit_count;
	uint8_t arg_len;
	uint8_t buf_len;
	uint8_t nibble;
	int i;
	int j;
	int ret;

	if (argc != 4) {
		return CMD_RET_USAGE;
	}

	bit_offset = atoi(argv[1]);
	bit_count = atoi(argv[2]);

	printf(" bit_offset = %d, bit_count = %d\n", bit_offset, bit_count);

	arg_len = strlen(argv[3]);
	buf_len = arg_len / 2;
	nibble = arg_len % 2;
	if (nibble) {
		buf_len += 1;
	}

	/* value should be valid for the bit count */
	if (bit_count > (buf_len * 8)) {
		printf("bit count = %d, buf_len = %d\n", bit_count, (buf_len * 8));
		return CMD_RET_USAGE;
	}
	if (bit_count <= ((buf_len - 1) * 8)) {
		printf("bit count = %d, buf_len = %d\n", bit_count, (buf_len * 8));
		return CMD_RET_USAGE;
	}

	for (i = arg_len - 1, j = 0; i > 0; i -= 2, j++) {
		char b[3] = {0x00, 0x00, 0x00};
		memcpy(b, argv[3] + i - 1, 2);
		buf[j] = strtol(b, NULL, 16);
	}

	if (nibble) {
		char b[3] = {0x00, 0x00};
		b[0] = argv[3][0];
		buf[j] = strtol(b, NULL, 16);
	}

	for (i = 0; i < buf_len; i++) {
		printf(" %02d: %02x\n", i, buf[i]);
	}

	ret = scm_efuse_write(bit_offset, bit_count, buf);
	if (ret) {
		printf("efuse write error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

#ifdef CONFIG_EFUSE_BUFFER_MODE

static int scm_cli_efuse_clr_buf(int argc, char *argv[])
{
	uint32_t bit_offset;
	uint32_t bit_count;
	int ret;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	bit_offset = atoi(argv[1]);
	bit_count = atoi(argv[2]);

	printf(" bit_offset = %d, bit_count = %d\n", bit_offset, bit_count);

	ret = scm_efuse_clr_buffer(bit_offset, bit_count);
	if (ret) {
		printf("efuse clear buffer error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int scm_cli_efuse_set_mode(int argc, char *argv[])
{
	enum scm_efuse_mode mode;
	int ret;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	mode = (enum scm_efuse_mode)atoi(argv[1]);
	ret = scm_efuse_set_mode(mode);
	if (ret) {
		printf("efuse set mode error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int scm_cli_efuse_get_mode(int argc, char *argv[])
{
	enum scm_efuse_mode mode;
	int ret;

	ret = scm_efuse_get_mode(&mode);
	if (ret) {
		printf("efuse set mode error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	printf("efuse mode : %d\n", mode);

	return CMD_RET_SUCCESS;
}

static int scm_cli_efuse_sync(int argc, char *argv[])
{
	int ret;

	ret = scm_efuse_sync();
	if (ret) {
		printf("efuse set mode error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int scm_cli_efuse_load(int argc, char *argv[])
{
	int ret;

	ret = scm_efuse_load();
	if (ret) {
		printf("efuse set mode error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

#endif

static const struct cli_cmd scm_cli_efuse_cmd[] = {
	CMDENTRY(read, scm_cli_efuse_read, "", ""),
	CMDENTRY(write, scm_cli_efuse_write, "", ""),
#ifdef CONFIG_EFUSE_BUFFER_MODE
	CMDENTRY(clear_buffer, scm_cli_efuse_clr_buf, "", ""),
	CMDENTRY(set_mode, scm_cli_efuse_set_mode, "", ""),
	CMDENTRY(get_mode, scm_cli_efuse_get_mode, "", ""),
	CMDENTRY(sync, scm_cli_efuse_sync, "", ""),
	CMDENTRY(load, scm_cli_efuse_load, "", ""),
#endif
};

static int do_scm_cli_efuse(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], scm_cli_efuse_cmd, ARRAY_SIZE(scm_cli_efuse_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(efuse, do_scm_cli_efuse,
	"CLI for EFUSE API test",
	"efuse read  [bit offset] [bit length]" OR
	"efuse write [bit offset] [bit length] [value in hex]"
#ifdef CONFIG_EFUSE_BUFFER_MODE
	OR "efuse clear_buffer [bit offset] [bit length]" OR
	"efuse set_mode [mode] \n"
	"\t0 : EFUSE RAW mode\n"
	"\t1 : EFUSE RAM buffer mode\n"
	"\t2 : EFUSE FLASH buffer mode" OR
	"efuse get_mode" OR
	"efuse load" OR
	"efuse sync"
#endif
);

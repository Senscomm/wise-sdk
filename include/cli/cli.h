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

#ifndef __CLI_H__
#define __CLI_H__

#include <u-boot/linker-lists.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_MAX_ARGV 		32
#define CMD_MAX_CMDLINE_SIZE 	CONFIG_CMDLINE_MAX_LEN

#define CMD_RET_SUCCESS 	 0
#define	CMD_RET_FAILURE 	 1
#define CMD_RET_UNHANDLED 	 2
#define	CMD_RET_USAGE  		-1

struct cli_cmd {
	const char *name;
	int (*handler)(int argc, char *argv[]);
	const char *desc; /* Short description */
	const char *usage; /* usage */
};

#define CLI_CMD(_cmd_) \
	ll_entry_declare(struct cli_cmd, _cmd_, cmd)

#define _cli_cmd_start() ll_entry_start(struct cli_cmd, cmd)

#define _cli_cmd_end() ll_entry_end(struct cli_cmd, cmd)

#define CMD(cmd, fn, d, u)			\
	CLI_CMD(cmd) = {			\
		.name = #cmd,				\
		.handler = fn, 				\
		.desc = d,					\
		.usage = u,					\
	}

#define CMDENTRY(cmd, fn, d, u) \
	{							\
		.name = #cmd, 			\
		.handler = fn, 			\
		.desc = d, 				\
		.usage = u,				\
	}

#define OR "\n  or:  "

int cli_run_command(char *cmd);
const struct cli_cmd *cli_find_cmd(char *cmd, const struct cli_cmd *table, int len);
void cli_start(void);

int cli_parse_line(char **s, char *argv[]);
void cli_lock_cmd(const struct cli_cmd *cmd);
int cli_readline(const char *prompt, char *buffer, int blen);

#ifdef __cplusplus
}
#endif

#endif // __CLI_H__

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

#ifndef __SCM_CLI_H__
#define __SCM_CLI_H__

struct scm_cli {
	const char *name;
	int (*ops) (int argc, char *argv[]);
	const char *usage; /* usage */
	const char *desc; /* Short description */
};

#define SCM_CLI_CMD(_id_) \
	ll_entry_declare(struct scm_cli, _id_, cli)

#define scm_cli_cmd_start() ll_entry_start(struct scm_cli, cli)
#define scm_cli_cmd_end() ll_entry_end(struct scm_cli, cli)

#define SCM_CLI(cmd, fn, u, d)		\
	SCM_CLI_CMD(cmd) = {		\
		.name = #cmd,			\
		.ops = fn,	\
		.usage = u, \
		.desc = d, \
	}

#endif /* __SCM_CLI_H__ */

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
#include "scm_timer.h"

static uint8_t scm_cli_timer_ch[SCM_TIMER_IDX_MAX][SCM_TIMER_CH_MAX] = {
	{
		(0 << 4) + 0,
		(0 << 4) + 1,
		(0 << 4) + 2,
		(0 << 4) + 3,
	},
	{
		(1 << 4) + 0,
		(1 << 4) + 1,
		(1 << 4) + 2,
		(1 << 4) + 3,
	}
};

static int scm_cli_timer_notify(enum scm_timer_event_type type, void *ctx)
{
	uint8_t arg = *(uint32_t *)ctx;
	printk("timer event, id=%d, ch=%d\n", (arg >> 4), (arg & 0x0f));
	return 0;
}

static int scm_cli_timer_periodic(int argc, char *argv[])
{
	enum scm_timer_idx idx;
	struct scm_timer_cfg cfg;
	enum scm_timer_ch ch;
	int ret;

	if (argc != 4) {
		return CMD_RET_USAGE;
	}

	idx = (enum scm_timer_idx)atoi(argv[1]);
	ch = atoi(argv[2]);

	cfg.mode = SCM_TIMER_MODE_PERIODIC;
	cfg.intr_en = 1;
	cfg.data.periodic.duration = atoi(argv[3]);

	ret = scm_timer_configure(idx, ch, &cfg, scm_cli_timer_notify, &scm_cli_timer_ch[idx][ch]);
	if (ret) {
		printf("timer configure error = %x\n", ret);
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

static int scm_cli_timer_oneshot(int argc, char *argv[])
{
	enum scm_timer_idx idx;
	struct scm_timer_cfg cfg;
	enum scm_timer_ch ch;
	int ret;

	if (argc != 4) {
		return CMD_RET_USAGE;
	}

	idx = (enum scm_timer_idx)atoi(argv[1]);
	ch = atoi(argv[2]);

	cfg.mode = SCM_TIMER_MODE_ONESHOT;
	cfg.intr_en = 1;
	cfg.data.oneshot.duration = atoi(argv[3]);

	ret = scm_timer_configure(idx, ch, &cfg, scm_cli_timer_notify, &scm_cli_timer_ch[idx][ch]);
	if (ret) {
		printf("timer configure error = %x\n", ret);
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

static int scm_cli_timer_freerun(int argc, char *argv[])
{
	enum scm_timer_idx idx;
	struct scm_timer_cfg cfg;
	enum scm_timer_ch ch;
	int ret;

	if (argc != 4) {
		return CMD_RET_USAGE;
	}

	idx = (enum scm_timer_idx)atoi(argv[1]);
	ch = atoi(argv[2]);

	cfg.mode = SCM_TIMER_MODE_FREERUN;
	cfg.intr_en = 0;
	cfg.data.freerun.freq = atoi(argv[3]);

	ret = scm_timer_configure(idx, ch, &cfg, NULL, NULL);
	if (ret) {
		printf("timer configure error = %x\n", ret);
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

static int scm_cli_timer_pwm(int argc, char *argv[])
{
	enum scm_timer_idx idx;
	struct scm_timer_cfg cfg;
	enum scm_timer_ch ch;
	int ret;

	if (argc < 5) {
		return CMD_RET_USAGE;
	}

	idx = (enum scm_timer_idx)atoi(argv[1]);
	ch = atoi(argv[2]);

	cfg.mode = SCM_TIMER_MODE_PWM;
	cfg.intr_en = 0;
	cfg.data.pwm.high = atoi(argv[3]);
	cfg.data.pwm.low = atoi(argv[4]);
	if (argc == 6) {
		cfg.data.pwm.park = atoi(argv[5]);
	} else {
		cfg.data.pwm.park = 0;
	}

	ret = scm_timer_configure(idx, ch, &cfg, NULL, NULL);
	if (ret) {
		printf("timer configure error = %x\n", ret);
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

static int scm_cli_timer_start(int argc, char *argv[])
{
	enum scm_timer_idx idx;
	enum scm_timer_ch ch;
	int ret;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	idx = (enum scm_timer_idx)atoi(argv[1]);
	ch = atoi(argv[2]);
	ret = scm_timer_start(idx, ch);
	if (ret) {
		printf("timer start error = %x\n", ret);
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

static int scm_cli_timer_stop(int argc, char *argv[])
{
	enum scm_timer_idx idx;
	enum scm_timer_ch ch;
	int ret;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	idx = (enum scm_timer_idx)atoi(argv[1]);
	ch = atoi(argv[2]);
	ret = scm_timer_stop(idx, ch);
	if (ret) {
		printf("timer stop error = %x\n", ret);
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

static int scm_cli_timer_startm(int argc, char *argv[])
{
	enum scm_timer_idx idx;
	uint8_t chs = 0;
	int ret;

	if (argc < 3) {
		return CMD_RET_USAGE;
	}

	idx = (enum scm_timer_idx)atoi(argv[1]);
	for (int i = 2; i < argc; i++) {
		chs |= 1 << atoi(argv[i]);
	}

	ret = scm_timer_start_multi(idx, chs);
	if (ret) {
		printf("timer start multi error = %x\n", ret);
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

static int scm_cli_timer_stopm(int argc, char *argv[])
{
	enum scm_timer_idx idx;
	uint32_t chs = 0;
	int ret;

	if (argc < 3) {
		return CMD_RET_USAGE;
	}

	idx = (enum scm_timer_idx)atoi(argv[1]);
	for (int i = 2; i < argc; i++) {
		chs |= 1 << atoi(argv[i]);
	}

	ret = scm_timer_stop_multi(idx, chs);
	if (ret) {
		printf("timer stop multi error = %x\n", ret);
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

static int scm_cli_timer_value(int argc, char *argv[])
{
	enum scm_timer_idx idx;
	enum scm_timer_ch ch;
	uint32_t value;
	int ret;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	idx = (enum scm_timer_idx)atoi(argv[1]);
	ch = atoi(argv[2]);
	ret = scm_timer_value(idx, ch, &value);
	if (ret) {
		printf("timer value error = %x\n", ret);
		return CMD_RET_FAILURE;
	}
	printf("timer value [%d:%d] = 0x%08x (%u)\nj", idx, ch, value, value);

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_cli_timer_cmd[] = {
	CMDENTRY(periodic, scm_cli_timer_periodic, "", ""),
	CMDENTRY(oneshot, scm_cli_timer_oneshot, "", ""),
	CMDENTRY(freerun, scm_cli_timer_freerun, "", ""),
	CMDENTRY(pwm, scm_cli_timer_pwm, "", ""),
	CMDENTRY(start, scm_cli_timer_start, "", ""),
	CMDENTRY(stop, scm_cli_timer_stop, "", ""),
	CMDENTRY(startm, scm_cli_timer_startm, "", ""),
	CMDENTRY(stopm, scm_cli_timer_stopm, "", ""),
	CMDENTRY(value, scm_cli_timer_value, "", ""),
};

static int do_scm_cli_timer(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], scm_cli_timer_cmd, ARRAY_SIZE(scm_cli_timer_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(timer, do_scm_cli_timer,
	"CLI for TIMER API test",
	"timer periodic tid ch duration_usec" OR
	"timer oneshot  tid ch duration_usec" OR
	"timer freerun  tid ch freq_hz" OR
	"timer pwm      tid ch high_usec low_usec park" OR
	"timer start    tid ch" OR
	"timer stop     tid ch" OR
	"timer startm   tid [0] [1] [2] [3]" OR
	"timer stopm    tid [0] [1] [2] [3]" OR
	"timer value    tid ch"
);

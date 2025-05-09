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
#include "scm_adc.h"

/* Description of the test setup for ADC (Analog to Digital Converter). */
/*
 * This example test for ADC.
 *
 * [Hardware Setup]
 * Connect GPIO pins [4, 7, 0, 1] to the corresponding ADC channel [4, 5, 6, 7]
 */

int main(void)
{
	printf("ADC demo\n");

	return 0;
}

/* Function to read values from ADC channels */
static int scm_cli_adc_read(int argc, char *argv[])
{
	uint16_t buf[16];
	uint32_t len;
	enum scm_adc_channel ch;
	int i, ret;

	if (argc < 3) {
		return CMD_RET_USAGE;
	}

	ch = atoi(argv[1]);
	if (ch == SCM_ADC_DIFFER_CH_0_1	||
	    ch == SCM_ADC_DIFFER_CH_2_3 ||
		ch < SCM_ADC_SINGLE_CH_4) {
		return CMD_RET_USAGE;
	}
	if (ch > SCM_ADC_DIFFER_CH_6_7) {
		return CMD_RET_USAGE;
	}

	len = atoi(argv[2]);
	if (len > 16) {
		return CMD_RET_USAGE;
	}

	ret = scm_adc_read(ch, buf, len);
	if (ret) {
		printf("adc read error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	printf("ADC channel %d\n", ch);
	for (i = 0; i < len; i++) {
		if (i != 0 && (i % 8) == 0) {
			printf("\n%04x ", buf[i]);
		} else {
			printf("%04x ", buf[i]);
		}
	}
	printf("\n");

	return CMD_RET_SUCCESS;
}

/* Array of ADC commands for CLI */
static const struct cli_cmd scm_cli_adc_cmd[] = {
	CMDENTRY(read, scm_cli_adc_read, "", ""),
};

/* Function to execute ADC CLI commands */
static int do_scm_cli_adc(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	/* Find and execute the corresponding ADC command */
	cmd = cli_find_cmd(argv[0], scm_cli_adc_cmd, ARRAY_SIZE(scm_cli_adc_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

/* Register the ADC command in the CLI */
CMD(adc, do_scm_cli_adc,
		"ADC transfer test", /* Command description */
		"adc read <channel> <len>\n" /* Usage message */
		"\tchannel:support 4 ~ 7 ch\n" /* Details on channel parameter */
		"\tlen    :this appliactaion support max 16 sample\n" /* Details on length parameter */
);

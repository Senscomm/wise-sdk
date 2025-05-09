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
#include "scm_efuse.h"
/* Description of the test setup for ADC (Analog to Digital Converter). */
/*
 * This example test for ADC.
 *
 * [Hardware Setup]
 * Connect GPIO pins [4, 7, 0, 1] to the corresponding ADC channel [4, 5, 6, 7]
 */
#if 0
int main(void)
{
	printf("ADC demo\n");

	return 0;
}
#endif
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


int auxadc_set_linear_parameter(enum scm_adc_channel ch, s32 m, s32 n)
{
	uint8_t is_odd = (ch & 0x01);
	enum scm_efuse_mode mode;
	u32 v = 0;
	int ret = 0;

	ret = scm_efuse_get_mode(&mode);
	if (ret) {
		printf("efuse get mode error %x\n", ret);
		return -1;
	}
	if (mode == SCM_EFUSE_MODE_RAW) {
		printf("adc line cannot used in efuse raw mode, please set efuse_mode at first.\n");
		return -1;
	}

	v |= (m & 0x3FF);

	if (is_odd)
		v |= ((n & 0x7FF) << 10);
	else
		v |= ((n & 0x7FF) << 21);

	ret = scm_efuse_write(SCM_EFUSE_ADDR_AUXADC_LINEAR, SCM_EFUSE_SIZE_AUXADC_LINEAR, (const uint8_t *)&v);
	if (ret) {
		printf("efuse write error %x\n", ret);
		return -1;
	}

	printk("\nWrite efuse(0x%d) v=0x%x\n", SCM_EFUSE_ADDR_AUXADC_LINEAR, v);

	return 0;
}

/* Function to record linear parameter into efuse */
static int scm_cli_adc_save_line(int argc, char *argv[])
{
	int32_t m, n;
	enum scm_adc_channel ch;
	int ret;

	if (argc < 4) {
		printf("auxadc save line argc(%d)\n", argc);
		return CMD_RET_USAGE;
	}

	ch = atoi(argv[1]);
	printf("auxadc save line ch(%d)\n", ch);
	if (ch == SCM_ADC_DIFFER_CH_0_1	||
	    ch == SCM_ADC_DIFFER_CH_2_3 ||
		ch < SCM_ADC_SINGLE_CH_4) {
		return CMD_RET_USAGE;
	}
	if (ch > SCM_ADC_DIFFER_CH_6_7) {
		return CMD_RET_USAGE;
	}

	m = atoi(argv[2]);
	printf("auxadc save line m(%d)\n", m);
	if (m >= 512 || m < -512) {
		return CMD_RET_USAGE;
	}
	n = atoi(argv[3]);
	printf("auxadc save line n(%d)\n", n);
	if (n>= 1024 || n < -1024) {
		return CMD_RET_USAGE;
	}

	ret = auxadc_set_linear_parameter(ch, m, n);
	if (ret) {
		printf("auxadc save line error %x\n", ret);
		return CMD_RET_USAGE;
	}
	return CMD_RET_SUCCESS;
}


/* Function to calculate/regression linear parameter by 2 points */
static int scm_cli_adc_cal_line(int argc, char *argv[])
{
	uint32_t p1, p2;
	enum scm_adc_channel ch;
	double y1 = 0.3/3.3 * 4096; // ~372
	double y2 = 3.0/3.3 * 4096; // ~3724
	double x1 = 0, x2 = 0, m = 0, n = 0;

	if (argc < 4) {
		printf("auxadc reg line argc(%d)\n", argc);
		return CMD_RET_USAGE;
	}

	ch = atoi(argv[1]);
	printf("auxadc reg line ch(%d)\n", ch);
	if (ch == SCM_ADC_DIFFER_CH_0_1	||
	    ch == SCM_ADC_DIFFER_CH_2_3 ||
		ch < SCM_ADC_SINGLE_CH_4) {
		return CMD_RET_USAGE;
	}
	if (ch > SCM_ADC_DIFFER_CH_6_7) {
		return CMD_RET_USAGE;
	}

	p1 = atoi(argv[2]);
	x1 = (double) p1;
	if (p1 > 4095) {
		printf("auxadc reg line, invalid p1(%d)\n", p1);
		return CMD_RET_USAGE;
	}
	p2 = atoi(argv[3]);
	x2 = (double) p2;
	if (p2 > 4095) {
		printf("auxadc reg line, invalid p2(%d)\n", p2);
		return CMD_RET_USAGE;
	}
	if (p1 == p2) {
		printf("auxadc reg line, invalid p1=p2(%d)\n", p2);
		return CMD_RET_USAGE;
	}
	m = (y1-y2)/(x1-x2);
	m = (1.0 - m) * 10000;
	n = (x1*y2-x2*y1)/(x1-x2);
	n = n * 10;
	printf("The regression line (1-m)*10000 = %d, 10*n = %d\n",
		(unsigned int)m, (unsigned int)n);

	return CMD_RET_SUCCESS;
}

/* Array of ADC commands for CLI */
static const struct cli_cmd scm_cli_adc_cmd[] = {
	CMDENTRY(read, scm_cli_adc_read, "", ""),
	CMDENTRY(save_line, scm_cli_adc_save_line, "", ""),
	CMDENTRY(cal_line, scm_cli_adc_cal_line, "", ""),
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
		"\tlen    :this appliactaion support max 16 sample\n"  OR /* Details on length parameter */
		"adc save_line <channel> <m> <n>\n" /* Usage message */
		"\tchannel:support 4 ~ 7 ch\n" /* Details on channel parameter */
		"\tm	  :(1-slope)*10000, range: -512 ~ +511\n" /* Details on length parameter */
		"\tn	  :offset*10, range -1024 ~ +1023\n"  OR /* Details on length parameter */
		"adc cal_line <channel> <p1> <p2>\n" /* Usage message */
		"\tchannel:support 4 ~ 7 ch\n" /* Details on channel parameter */
		"\tp1	  :measure result for 0.3v (about 372),  range: 0~4095 \n" /* Details on length parameter */
		"\tp2	  :measure result for 3.0v (about 3724), range: 0~4095 \n" /* Details on length parameter */
);

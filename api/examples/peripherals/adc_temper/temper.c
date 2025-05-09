/*
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * This sample demonstrate temperature display using the ADC API and thermistor.
 *
 * [Hardware Setup]
 *
 * Solder the thermistor NCP15XH103F03RC and 30.06K resistor in series to GPIO 7.
 * GPIO 7 is ADC channel 5.
 */

#include <stdint.h>
#include <stdlib.h>
#include <hal/console.h>
#include <hal/kernel.h>
#include <math.h>

#include "scm_adc.h"
#include "temper.h"

#define REF_ADC_CNT			 16
#define INVALID_TEMPER_VLAUE 0xFF

struct temper_ctx {
	int adc_chan;
	float series_res;
	float ref_volt;
};

static struct temper_ctx g_temper_ctx = {
	.adc_chan = SCM_ADC_SINGLE_CH_5,
	.series_res = 30.06,
	.ref_volt = 3.3,
};

float r_ntc_table[] = {
	195.652, 184.917, 174.845, 165.391, 156.512, 148.171, 140.330, 132.958,
	126.021, 119.494, 113.347, 107.565, 102.116, 96.978, 92.132, 87.559,
	83.242, 79.166, 75.316, 71.677, 68.237, 64.991, 61.919, 59.011,
	56.258, 53.650, 51.178, 48.835, 46.613, 44.506, 42.506, 40.600,
	38.791, 37.073, 35.442, 33.892, 32.420, 31.020, 29.689, 28.423,
	27.219, 26.076, 24.988, 23.951, 22.963, 22.021, 21.123, 20.267,
	19.449, 18.670, 17.926, 17.214, 16.534, 15.886, 15.266, 14.674,
	14.108, 13.566, 13.049, 12.554, 12.080, 11.628, 11.195, 10.780,
	10.382, 10.000, 9.634, 9.283, 8.947, 8.624, 8.314, 8.018,
	7.734, 7.461, 7.199, 6.948, 6.707, 6.475, 6.253, 6.039,
	5.834, 5.636, 5.445, 5.262, 5.086, 4.917, 4.754, 4.597,
	4.446, 4.301, 4.161, 4.026, 3.896, 3.771, 3.651, 3.535,
	3.423, 3.315, 3.211, 3.111, 3.014, 2.922, 2.834, 2.748,
	2.666, 2.586, 2.509, 2.435, 2.364, 2.294, 2.227, 2.163,
	2.100, 2.040, 1.981, 1.924, 1.870, 1.817, 1.766, 1.716,
	1.668, 1.622, 1.578, 1.535, 1.493, 1.452, 1.413, 1.375,
	1.338, 1.303, 1.268, 1.234, 1.202, 1.170, 1.139, 1.110,
	1.081, 1.053, 1.026, 0.999, 0.974, 0.949, 0.925, 0.902,
	0.880, 0.858, 0.837, 0.816, 0.796, 0.777, 0.758, 0.740,
	0.722, 0.705, 0.688, 0.672, 0.656, 0.640, 0.625, 0.611,
	0.596, 0.583, 0.569, 0.556, 0.544, 0.531,
};

int temper_setup(int chan, float series_res, float ref_volt)
{
	g_temper_ctx.adc_chan = chan;
	g_temper_ctx.series_res = series_res;
	g_temper_ctx.ref_volt = ref_volt;

	return 0;
}

int temper_read(float *temperature)
{
	uint16_t buf[REF_ADC_CNT];
	float adc_avg = 0;
	float temper = INVALID_TEMPER_VLAUE;
	float v_ntc;
	float r_ntc;
	int i;
	int ret;

	ret = scm_adc_read(g_temper_ctx.adc_chan, buf, REF_ADC_CNT);
	if (ret) {
		printf("Read ADC failed : %x\n", ret);
		return -1;
	}

	for (i = 0; i < REF_ADC_CNT; i++) {
		adc_avg += buf[i];
	}
	adc_avg /= REF_ADC_CNT;

#if 0
	printf("adc_avg : [%f]\n", adc_avg);
#endif

	/* 1. Voltage Calculation */
	v_ntc = ((adc_avg / 4095) * g_temper_ctx.ref_volt);

	/* 2. Resistance Calculation */
	r_ntc = g_temper_ctx.series_res * (v_ntc / (g_temper_ctx.ref_volt - v_ntc));

	/* 3. Temperature Conversion */
	for (i = 0; i < sizeof(r_ntc_table) / sizeof(float); i++) {
		if (r_ntc <= r_ntc_table[i] && r_ntc >= r_ntc_table[i + 1]) {
			temper = -40 + i +
				(1 - ((r_ntc - r_ntc_table[i + 1]) /
				 (r_ntc_table[i] - r_ntc_table[i + 1])));
		}
	}

	if (temper == INVALID_TEMPER_VLAUE) {
		return -1;
	}


	*temperature = temper;

	return 0;
}

#ifdef CONFIG_CMDLINE

#include "cli.h"

int do_setup_temperature(int argc, char *argv[])
{
	int adc_chan;
	float series_res;
	float ref_volt;

	if (argc < 4) {
		return CMD_RET_USAGE;
	}

	adc_chan = atoi(argv[1]);
	series_res = atoi(argv[2]);
	ref_volt = atof(argv[3]);

	temper_setup(adc_chan, series_res, ref_volt);

	return CMD_RET_SUCCESS;
}

int do_read_temperature(int argc, char *arg[])
{
	float temperature;
	int ret;

	ret = temper_read(&temperature);
	if (ret) {
		return CMD_RET_FAILURE;
	}

	printf("temperature : [%f]\n", temperature);

	return CMD_RET_SUCCESS;
}


static const struct cli_cmd tmper_cmd[] = {
	CMDENTRY(setup, do_setup_temperature, "", ""),
	CMDENTRY(read, do_read_temperature, "", ""),
};

static int do_temperature(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], tmper_cmd, ARRAY_SIZE(tmper_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(temper, do_temperature,
	"temeperature demo",
	"temper setup <adc channel> <series resistor> <reference voltage>\n"
	"If there is no parameter, use default (5, 30.06, 3.3)" OR
	"temper read"
);

#endif

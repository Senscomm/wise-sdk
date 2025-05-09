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
#include "scm_gpio.h"

#define NUM_GPIO_PIN	25

int gpio_stats[NUM_GPIO_PIN];

static int scm_cli_gpio_notify(uint32_t pin, void *ctx)
{
	gpio_stats[pin]++;
	return 0;
}

static int scm_cli_gpio_configure(int argc, char *argv[])
{
	uint32_t pin;
	uint8_t property;
	int ret;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	pin = atoi(argv[1]);
	property = atoi(argv[2]);

	ret = scm_gpio_configure(pin, (enum scm_gpio_property)property);
	if (ret) {
		printf("gpio configure error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int scm_cli_gpio_write(int argc, char *argv[])
{
	uint32_t pin;
	uint8_t value;
	int ret;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	pin = atoi(argv[1]);
	value = atoi(argv[2]);

	printf("gpio write: pin=%d value=%d\n", pin, value);

	ret = scm_gpio_write(pin, value);
	if (ret) {
		printf("gpio write error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int scm_cli_gpio_read(int argc, char *argv[])
{
	uint32_t pin;
	uint8_t value;
	int ret;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	pin = atoi(argv[1]);

	ret = scm_gpio_read(pin, &value);
	if (ret) {
		printf("gpio read error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	printf("gpio read: pin=%d value=%d\n", pin, value);

	return CMD_RET_SUCCESS;
}

static int scm_cli_gpio_enable_interrupt(int argc, char *argv[])
{
	uint32_t pin;
	uint8_t type;
	int ret;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	pin = atoi(argv[1]);
	type = atoi(argv[2]);

	/* call API */
	ret = scm_gpio_enable_interrupt(pin, (enum scm_gpio_int_type)type, scm_cli_gpio_notify, NULL);
	if (ret) {
		printf("gpio enable interrupt error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int scm_cli_gpio_disable_interrupt(int argc, char *argv[])
{
	uint32_t pin;
	int ret;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	pin = atoi(argv[1]);

	/* call API */
	ret = scm_gpio_disable_interrupt(pin);
	if (ret) {
		printf("gpio disable interrupt error %x\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int scm_cli_gpio_stats(int argc, char *argv[])
{
	int i;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	if (strcmp(argv[1], "clear") == 0) {
		for (i = 0; i < NUM_GPIO_PIN; i++) {
			gpio_stats[i] = 0;
		}
		printf("GPIO stats cleared\n");
	} else if (strcmp(argv[1], "show") == 0) {

		printf("GPIO stats\n");
		for (i = 0; i < NUM_GPIO_PIN; i++) {
			printf("pin %02d: count %03d\n", i, gpio_stats[i]);
		}
	} else {
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_cli_gpio_cmd[] = {
	CMDENTRY(configure, scm_cli_gpio_configure, "", ""),
	CMDENTRY(write, scm_cli_gpio_write, "", ""),
	CMDENTRY(read, scm_cli_gpio_read, "", ""),
	CMDENTRY(enable_int, scm_cli_gpio_enable_interrupt, "", ""),
	CMDENTRY(disable_int, scm_cli_gpio_disable_interrupt, "", ""),
	CMDENTRY(stats, scm_cli_gpio_stats, "", ""),
};

static int do_scm_cli_gpio(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], scm_cli_gpio_cmd, ARRAY_SIZE(scm_cli_gpio_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(gpio, do_scm_cli_gpio,
	"CLI for GPIO API test",
	"gpio configure [pin] [property]\n"
		"\tproperty\n"
		"\t 0: output\n"
		"\t 1: input\n"
		"\t 2: input with pull up\n"
		"\t 3: input with pull down" OR
	"gpio write [pin] [value]" OR
	"gpio read [pin]" OR
	"gpio enable_int [pin] [type]\n"
		"\ttype\n"
		"\t 0: rising edge\n"
		"\t 1: falling edge\n"
		"\t 2: both edge" OR
	"gpio disable_int [pin]" OR
	"gpio stats [show/clear]"
);

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

/*
 * This example demonstrates the use of the I2C in master and slave modes.
 *
 * [Hardware Setup]
 *
 * Connect I2C master and I2C slave pins on the board.
 * Default configuration for this demo is to use the EVB with the following pins.
 *
 *
 * !!WARNING!!
 * Refer to <selected board>/board.c to confirm pin numbers for both I2C0 and I2C1.)
 *
 *
 * 	I2C master using SCM_I2C_INDEX_0
 * 		SCL: GPIO 15
 * 		SDA: GPIO 16
 * 	I2C slave using SCM_I2C_INDEX_1
 * 		SCL: GPIO  6
 * 		SDA: GPIO  7
 *
 * Alternatively, user can use two EVBs and connect
 *	one jumper between the SCL pins (GPIO 15 of master --- GPIO 6 of slave)
 *	one jumper between the SDA pins (GPIO 16 of master --- GPIO 7 of slave)
 *
 * [I2C message format]
 *
 * - WRITE with the offset
 * 		ST, ADDR(W), OFFSET_LOW, OFFSET_HIGH, DATA0, DATA1, ..., DATAN, SP
 * - READ with the offset
 * 		ST, ADDR(W), OFFSET_LOW, OFFSET_HIGH, ST, ADDR(R), DATA0, DATA1, ..., DATAN, SP
 * - READ without the offset
 * 		ST, ADDR(R), DATA0, DATA1, ..., DATAN, SP
 *
 * [Test]
 *
 * EEPROM master
 * - Using the test commands, EEPROM contents can be accessed
 * - It may work for a real EEPROM devices
 * EEPROM slave
 * - this demo simulates the EEPROM using the memory buffer
 *
 * After the booting, both EEPROM master and EEPROM slave are initialized.
 * Use "eeprom read|write|clear" to see the data contents of the slave simulation
 * Try "eeprom" to see the available command options
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "scm_i2c.h"
#include "eeprom.h"
#include "cli.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE( x ) ( sizeof( x ) / sizeof( x[ 0 ] ) )
#endif

#define EEPROM_TRANS_BUF_SIZE	32 /* Buffer size for EEPROM transactions */

int main(void)
{
	printf("I2C demo\n");

	/* Initialize I2C EEPROM master and slave for demo */
	eeprom_master_init();
	eeprom_slave_init();

	return 0;
}

/* Command handler to clear the EEPROM memory */
static int do_eeprom_clear(int argc, char *argv[])
{
	eeprom_clear();

	return CMD_RET_SUCCESS;
}

/* Command handler to write data to EEPROM */
static int do_eeprom_write(int argc, char *argv[])
{
	uint16_t addr;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	addr = strtoul(argv[1], NULL, 16);
	eeprom_write(addr, (uint8_t *)argv[2], strlen(argv[2]));

	return CMD_RET_SUCCESS;
}

/* Command handler to read data from EEPROM */
static int do_eeprom_read(int argc, char *argv[])
{
	uint16_t addr;
	uint8_t rx_buf[EEPROM_TRANS_BUF_SIZE];
	uint32_t rx_len;
	int ret;

	/* Read entire EEPROM memory if no arguments are provided */
	if (argc == 1) {
		rx_len = EEPROM_TRANS_BUF_SIZE; /* Set read length to buffer size */

		for (addr = 0; addr < EEPROM_MEMORY_SIZE; addr += EEPROM_TRANS_BUF_SIZE) {
			ret = eeprom_read(addr, rx_buf, rx_len);
			if (ret) {
				return CMD_RET_FAILURE;
			}

			for (int i = 0; i < rx_len; i++) {
				if ((i % 16) == 0) {
					printf("\n%03x: ", addr + i);
				}
				printf("%02x ", rx_buf[i]);
			}
		}
		printf("\n");

	} else if (argc == 3) { /* Read specified length from EEPROM if address and length are provided */
		addr = strtoul(argv[1], NULL, 16);
		rx_len = atoi(argv[2]);

		if (rx_len > EEPROM_TRANS_BUF_SIZE) {
			printf("cannot read more than %d bytes\n", EEPROM_TRANS_BUF_SIZE);
			return CMD_RET_USAGE;
		}

		ret = eeprom_read(addr, rx_buf, rx_len); /* Read from EEPROM */
		if (ret) {
			return CMD_RET_FAILURE;
		}

		for (int i = 0; i < rx_len; i++) {
			if ((i % 16) == 0) {
				printf("\n%03x: ", i);
			}
			printf("%02x ", rx_buf[i]);
		}
		printf("\n");
	} else {
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

/* Command handler to set the current EEPROM address */
static int do_eeprom_set_addr(int argc, char *argv[])
{
	uint16_t addr;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	addr = strtoul(argv[1], NULL, 16);
	eeprom_set_addr(addr);

	return CMD_RET_SUCCESS;
}

/* Command handler for continuous reading from EEPROM without setting address */
static int do_eeprom_readon(int argc, char *argv[])
{
	uint8_t rx_buf[EEPROM_TRANS_BUF_SIZE];
	uint32_t rx_len;
	int ret;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	rx_len = atoi(argv[1]);
	if (rx_len > EEPROM_TRANS_BUF_SIZE) {
		printf("length is too big : %d(<= %d)\n", rx_len, EEPROM_TRANS_BUF_SIZE);
	}

	ret = eeprom_readon(rx_buf, rx_len);
	if (ret) {
		return CMD_RET_FAILURE;
	}

	for (int i = 0; i < rx_len; i++) {
		if ((i % 16) == 0) {
			printf("\n%03x: ", i);
		}
		printf("%02x ", rx_buf[i]);
	}
	printf("\n");

	return CMD_RET_SUCCESS;
}

/* Command handler for testing EEPROM read and write operations */
static int do_eeprom_test(int argc, char *argv[])
{
	uint8_t tx_buf[EEPROM_TRANS_BUF_SIZE];
	uint8_t rx_buf[EEPROM_TRANS_BUF_SIZE];
	uint16_t addr;
	int test_count;
	int test_max = 10;
	int ret;

	if (argc == 2) {
		test_max = atoi(argv[1]);
	}

	/* Perform tests up to the maximum number specified */
	for (test_count = 0; test_count < test_max; test_count++) {
		printf("testing #%d\n", test_count);

		for (addr = 0; addr < EEPROM_MEMORY_SIZE; addr += EEPROM_TRANS_BUF_SIZE) {
			/* Fill the transmit buffer with random data */
			for (int i = 0; i < EEPROM_TRANS_BUF_SIZE; i++) {
				tx_buf[i] = rand() & 0xff;
			}

			ret = eeprom_write(addr, tx_buf, EEPROM_TRANS_BUF_SIZE);
			if (ret) {
				printf("eeprom write error\n");
				goto out;
			}

			ret = eeprom_read(addr, rx_buf, EEPROM_TRANS_BUF_SIZE);
			if (ret) {
				printf("eeprom read error\n");
				goto out;
			}

			/* Compare transmitted and received data */
			if (memcmp(tx_buf, rx_buf, EEPROM_TRANS_BUF_SIZE)) {
				printf("eeprom data mismatch\n");
				goto out;
			}
		}
	}
	printf("test passed\n");
	return CMD_RET_SUCCESS;

out:
	printf("test failed at %d\n", test_count);
	return CMD_RET_SUCCESS;
}

/* Array of supported EEPROM commands */
static const struct cli_cmd eeprom_cli_cmd[] = {
	CMDENTRY(clear, do_eeprom_clear, "", ""),
	CMDENTRY(write, do_eeprom_write, "", ""),
	CMDENTRY(read, do_eeprom_read, "", ""),
	CMDENTRY(addr, do_eeprom_set_addr, "", ""),
	CMDENTRY(readon, do_eeprom_readon, "", ""),
	CMDENTRY(test, do_eeprom_test, "", ""),
};

static int do_eeprom(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], eeprom_cli_cmd, ARRAY_SIZE(eeprom_cli_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

/* Register 'eeprom' command in the CLI */
CMD(eeprom, do_eeprom,
	"eeprom access",
	"eeprom clear" OR
	"eeprom write off_addr_hex string" OR
	"eeprom read [off_addr_hex len]" OR
	"eeprom addr off_addr_hex" OR
	"eeprom readon len" OR
	"eeprom test repeat"
);

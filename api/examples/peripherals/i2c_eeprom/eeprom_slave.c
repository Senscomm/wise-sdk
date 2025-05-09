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
#include <stdlib.h>
#include <string.h>
#include "scm_i2c.h"
#include "eeprom.h"

/* Definition for EEPROM using I2C slave interface */
#define EEPROM_I2C_SLAVE_IDX       SCM_I2C_IDX_1 /* Define the I2C slave index */
#define EEPROM_I2C_SLAVE_BUF_SIZE  (EEPROM_ADDR_LEN + 32) /* Define buffer size for I2C operations */


struct eeprom {
	uint8_t rx[EEPROM_I2C_SLAVE_BUF_SIZE]; /* Temporary buffer for I2C messages */
	uint8_t data[EEPROM_MEMORY_SIZE]; /* Simulation data as EEPROM memory */
	uint16_t offset; /* Current memory offset for read operations without offset information */
};

#ifdef CONFIG_USE_DMA_ALLOCATION
static struct eeprom *eeprom;
#else
static struct eeprom g_eeprom __attribute__((section(".dma_buffer")));
static struct eeprom *eeprom = &g_eeprom;
#endif

/* Callback function for handling I2C slave events */
static int eeprom_slave_notify(struct scm_i2c_event *event, void *ctx)
{
	int len;

	switch (event->type) {
	case SCM_I2C_EVENT_SLAVE_TX_REQUEST:
		/* Master requests data: set buffer pointer to current EEPROM data offset */
		scm_i2c_slave_tx(EEPROM_I2C_SLAVE_IDX, &eeprom->data[eeprom->offset], EEPROM_I2C_SLAVE_BUF_SIZE);
	break;
	case SCM_I2C_EVENT_SLAVE_RX_REQUEST:
		/* Master sends data: set buffer for receiving into temporary buffer */
		scm_i2c_slave_rx(EEPROM_I2C_SLAVE_IDX, eeprom->rx, EEPROM_I2C_SLAVE_BUF_SIZE);
	break;
	case SCM_I2C_EVENT_SLAVE_TX_CMPL:
		/* Transmission to master complete: update offset based on transmitted length */
		len = event->data.slave_tx_cmpl.len;
		if (len == 0) {
			return 0;
		}
		/* automatically increment the offset */
		if (eeprom->offset + len < EEPROM_MEMORY_SIZE) {
			eeprom->offset += len;
		}
	break;
	case SCM_I2C_EVENT_SLAVE_RX_CMPL:
		/* Reception from master complete: update EEPROM data and offset */
		len = event->data.slave_rx_cmpl.len;
		if (len == 0) {
			return 0;
		}
		if (len >= EEPROM_ADDR_LEN) {
			/* process eeprom address offset (first two bytes) */
			memcpy(&eeprom->offset, eeprom->rx, EEPROM_ADDR_LEN);
			len -= EEPROM_ADDR_LEN;
			/* process eeprom data (variable length) */
			if (len > 0) {
				if (eeprom->offset + len > EEPROM_MEMORY_SIZE) {
					len = EEPROM_MEMORY_SIZE - eeprom->offset;
				}
				memcpy(&eeprom->data[eeprom->offset], &eeprom->rx[EEPROM_ADDR_LEN], len);
				eeprom->offset += len; /* Update offset for next operations */
			}
		}
	break;
	default:
	break;
	}
	return 0;
}

/* Initialize the EEPROM as an I2C slave device */
int eeprom_slave_init(void)
{
	struct scm_i2c_cfg cfg;
	int ret;

	/* Initialize configuration structure */
	memset(&cfg, 0, sizeof(cfg));
	cfg.role = SCM_I2C_ROLE_SLAVE; /* Set device role to slave */
	cfg.bitrate = 100 * 1000; /* Set bitrate for I2C */
	cfg.dma_en = 0; /* DMA disabled for slave mode */
	cfg.pull_up_en = 1; /* Enable internal pull-up resistors */
	cfg.slave_addr = EEPROM_DEVICE_ADDR; /* Set slave address */

	/* Initialize I2C interface as a slave */
	ret = scm_i2c_init(EEPROM_I2C_SLAVE_IDX);
	if (ret) {
		printf("i2c init error %x\n", ret);
		return ret;
	}

	/* Configure I2C with specified settings */
	ret = scm_i2c_configure(EEPROM_I2C_SLAVE_IDX, &cfg, eeprom_slave_notify, NULL);
	if (ret) {
		printf("i2c configure error %x\n", ret);
		return ret;
	}

	/* Initialize EEPROM simulation: set offset to zero and fill data for demo */

#ifdef CONFIG_USE_DMA_ALLOCATION
	eeprom = dma_malloc(sizeof(struct eeprom));
#endif

	eeprom->offset = 0;
	for (int i = 0; i < EEPROM_MEMORY_SIZE; i++) {
		eeprom->data[i] = i;
	}

	return 0;
}

#include "cli.h"
/* Command line interface command to display the first 32 bytes of EEPROM data */
static int do_eeproms(int argc, char *argv[])
{
	int i;
	printf("offset=%d\n", eeprom->offset);
	for (i = 0; i < 32; i++) {
		printf("%02x ", eeprom->data[i]);
		if ((i % 16) == 15)
			printf("\n");
	}
	return CMD_RET_SUCCESS;
}

/* Register 'eeproms' command in CLI for displaying EEPROM status and data */
CMD(eeproms, do_eeproms, "eeproms show", "eeproms");

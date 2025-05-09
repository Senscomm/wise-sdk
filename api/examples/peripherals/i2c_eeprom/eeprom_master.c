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

/* EEPROM usage definitions */
#define EEPROM_I2C_MASTER_IDX       SCM_I2C_IDX_0 /* Define the I2C master index used for EEPROM */
#define EEPROM_I2C_MASTER_BUF_SIZE  (EEPROM_ADDR_LEN + 32) /* Define the buffer size for I2C operations */

/* DMA buffers for transmit and receive operations */
#ifdef CONFIG_USE_DMA_ALLOCATION
static uint8_t *tx_buf;
static uint8_t *rx_buf;
#else
static uint8_t tx_buf[EEPROM_I2C_MASTER_BUF_SIZE] __attribute__((section(".dma_buffer")));
static uint8_t rx_buf[EEPROM_I2C_MASTER_BUF_SIZE] __attribute__((section(".dma_buffer")));
#endif

/* Callback function for I2C events, currently unused */
static int eeprom_master_notify(struct scm_i2c_event *event, void *ctx)
{
	return 0;
}

/* Initialize I2C for EEPROM operations */
int eeprom_master_init(void)
{
	struct scm_i2c_cfg cfg; /* Configuration structure for I2C */
	int ret;

#ifdef CONFIG_USE_DMA_ALLOCATION
	tx_buf = dma_malloc(EEPROM_I2C_MASTER_BUF_SIZE);
	rx_buf = dma_malloc(EEPROM_I2C_MASTER_BUF_SIZE);
#endif

	memset(&cfg, 0, sizeof(cfg));
	cfg.role = SCM_I2C_ROLE_MASTER; /* Set role as I2C master */
	cfg.bitrate = 100 * 1000; /* Set bitrate for I2C */
	cfg.dma_en = 1; /* Enable DMA for I2C operations */
	cfg.pull_up_en = 1; /* Enable pull-up resistors */

	/* Initialize I2C with the given master index */
	ret = scm_i2c_init(EEPROM_I2C_MASTER_IDX);
	if (ret) {
		printf("i2c init error %x\n", ret);
		return ret;
	}

	/* Configure I2C with the specified settings */
	ret = scm_i2c_configure(EEPROM_I2C_MASTER_IDX, &cfg, eeprom_master_notify, NULL);
	if (ret) {
		printf("i2c configure error %x\n", ret);
		return ret;
	}

	return 0;
}

/* Clear EEPROM memory by setting all bytes to 0xFF */
int eeprom_clear(void)
{
	uint16_t addr;
	uint32_t tx_len;
	int ret;

	for (addr = 0; addr < EEPROM_MEMORY_SIZE; addr++) {
		memcpy(tx_buf, &addr, EEPROM_ADDR_LEN);
		memset(&tx_buf[EEPROM_ADDR_LEN], 0xff,  1);
		tx_len = EEPROM_ADDR_LEN + 1;
		ret = scm_i2c_master_tx(EEPROM_I2C_MASTER_IDX, EEPROM_DEVICE_ADDR, tx_buf, tx_len, 1000);
		if (ret) {
			printf("i2c master tx error %x\n", ret);
			break;
		}
	}
	return ret;
}

/* Write data to EEPROM at specified address */
int eeprom_write(uint16_t addr, uint8_t *buf, uint8_t len)
{
	uint32_t tx_len;
	int ret;

	if (len > EEPROM_I2C_MASTER_BUF_SIZE) {
		return -1;
	}

	memcpy(tx_buf, &addr, EEPROM_ADDR_LEN);
	memcpy(&tx_buf[EEPROM_ADDR_LEN], buf, len);
	tx_len = EEPROM_ADDR_LEN + len;

	/* Transmit data to EEPROM */
	ret = scm_i2c_master_tx(EEPROM_I2C_MASTER_IDX, EEPROM_DEVICE_ADDR, tx_buf, tx_len, 1000);
	if (ret) {
		printf("i2c master tx error %x\n", ret);
	}
	return ret;
}

/* Read data from EEPROM starting from specified address */
int eeprom_read(uint16_t addr, uint8_t *buf, uint32_t len)
{
	uint32_t tx_len;
	int ret;

	memcpy(tx_buf, &addr, EEPROM_ADDR_LEN);
	tx_len = EEPROM_ADDR_LEN;

	ret = scm_i2c_master_tx_rx(EEPROM_I2C_MASTER_IDX, EEPROM_DEVICE_ADDR,
							   tx_buf, tx_len, rx_buf, len, 1000);
	if (ret) {
		printf("i2c master tx rx error %x\n", ret);
	} else {
		 /* Copy received data to user buffer */
		memcpy(buf, rx_buf, len);
	}
	return ret;
}

/* Set EEPROM read/write address (used before a read or write operation) */
int eeprom_set_addr(uint16_t addr)
{
	uint32_t tx_len;
	int ret;

	memcpy(tx_buf, &addr, EEPROM_ADDR_LEN);
	tx_len = EEPROM_ADDR_LEN;

	ret = scm_i2c_master_tx(EEPROM_I2C_MASTER_IDX, EEPROM_DEVICE_ADDR, tx_buf, tx_len, 1000);
	if (ret) {
		printf("i2c master tx error %x\n", ret);
	}
	return ret;
}

/* Read data from EEPROM without setting address (continuous read) */
int eeprom_readon(uint8_t *buf, uint32_t len)
{
	int ret;

	ret = scm_i2c_master_rx(EEPROM_I2C_MASTER_IDX, EEPROM_DEVICE_ADDR, rx_buf, len, 1000);
	if (ret) {
		printf("i2c master rx error %x\n", ret);
	} else {
		memcpy(buf, rx_buf, len); /* Copy received data to user buffer */
	}
	return ret;
}

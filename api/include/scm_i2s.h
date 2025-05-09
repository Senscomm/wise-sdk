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

#ifndef __SCM_I2S_H__
#define __SCM_I2S_H__

#include <stdint.h>

#include "wise_err.h"

/**
 * I2S data format
 */
enum scm_i2s_fmt {
	SCM_I2S_FMT_I2S,
	SCM_I2S_FMT_LJ,
	SCM_I2S_FMT_RJ,
};

/**
 * I2S word length
 */
enum scm_i2s_wl {
	SCM_I2S_WL_16,
	SCM_I2S_WL_20,
	SCM_I2S_WL_24,
};

/**
 * I2S role
 */
enum scm_i2s_role {
	SCM_I2S_ROLE_MASTER,
	SCM_I2S_ROLE_SLAVE,
};

/**
 * I2S data direction
 */
enum scm_i2s_direction {
	SCM_I2S_RX 		= 0x1,
	SCM_I2S_TX 		= 0x2,
	SCM_I2S_BOTH 	= 0x3,
};

/**
 * I2S device configuration
 */
struct scm_i2s_cfg {
	enum scm_i2s_wl word_length;	/* word length in bits */
	enum scm_i2s_fmt format;		/* I2S data format */
	enum scm_i2s_role role;			/* Role on I2S bus */
	enum scm_i2s_direction dir;		/* I2S direction */
	uint32_t fs;					/* Sampling frequency, i.e., LRCLK frequency */
	int duration_per_block;			/* Duration in ms per an audio sample block */
	int number_of_blocks;			/* Number of audio sample blocks */
	int timeout;					/* Read, write timeout */
};

/**
 * @brief Initialize I2S
 */
int scm_i2s_init(void);

/**
 * @brief Deinitialize I2S
 */
int scm_i2s_deinit(void);

/**
 * @brief Configure I2S
 *
 * @param[in] cfg the cfg parameter
 */
int scm_i2s_configure(struct scm_i2s_cfg *cfg);

/**
 * @brief Read a block from an I2S stream
 */
int scm_i2s_read_block(uint8_t *buf, size_t *size);

/**
 * @brief Write a block into an I2S stream
 */
int scm_i2s_write_block(uint8_t *buf, size_t size);

/**
 * @brief Start an I2S stream
 */
int scm_i2s_start(enum scm_i2s_direction dir);

/**
 * @brief Stop an I2S stream
 */
int scm_i2s_stop(enum scm_i2s_direction dir);

/**
 * @brief Get a configured block buffer size
 */
int scm_i2s_get_block_buffer_size(struct scm_i2s_cfg *cfg);

#endif //__SCM_I2S_H__

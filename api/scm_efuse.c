/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <hal/types.h>
#include <hal/device.h>

#include "errno.h"
#include "scm_efuse.h"
#include "sys/ioctl.h"
#include "hal/efuse.h"

int scm_efuse_read(uint16_t bit_offset, uint16_t bit_count, uint8_t *val)
{
	int fd;
	int i;
	int ret;
	int result_bufidx, result_bit_offset;
	int start_bufidx, start_bit_offset;
	uint32_t efuse_val_32;
	uint8_t *efuse_val_ptr8;
	struct efuse_rw_data rw_data;

	fd = open("/dev/efuse", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	if (bit_count % 8) {
		memset(val, 0, (bit_count / 8) + 1);
	} else {
		memset(val, 0, (bit_count / 8));
	}

	rw_data.row = bit_offset / 32;
	rw_data.val = (u32 *)&efuse_val_32;
	start_bufidx = (bit_offset % 32) / 8;
	start_bit_offset = (bit_offset % 32) % 8;
	result_bufidx = 0;
	result_bit_offset = 0;

	ret = ioctl(fd, IOCTL_EFUSE_READ_ROW, &rw_data);
	if (ret < 0) {
		goto out;
	}
	efuse_val_ptr8 = (uint8_t *)rw_data.val;

	for (i = 0; i < bit_count; i++) {
		if (efuse_val_ptr8[start_bufidx] & (1 << start_bit_offset)) {
			val[result_bufidx] |= 1 << result_bit_offset;
		} else {
			val[result_bufidx] &= ~(1 << result_bit_offset);
		}

		result_bit_offset++;
		if (result_bit_offset == 8) {
			result_bufidx++;
			result_bit_offset = 0;
		}

		start_bit_offset++;
		if (start_bit_offset == 8) {
			start_bufidx++;
			if (start_bufidx == 4 && (i != (bit_count - 1))) {
				start_bufidx = 0;
				rw_data.row++;

				ret = ioctl(fd, IOCTL_EFUSE_READ_ROW, &rw_data);
				if (ret < 0) {
					goto out;
				}

				efuse_val_ptr8 = (uint8_t *)rw_data.val;
			}

			start_bit_offset = 0;
		}
	}

out:
	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_efuse_write(uint16_t bit_offset, uint16_t bit_count, const uint8_t *val)
{
	int fd;
	int i;
	int ret;
	int result_bufidx, result_bit_offset;
	int start_bufidx, start_bit_offset;
	int write_complete;
	uint32_t efuse_val_32;
	uint8_t *efuse_val_ptr8;
	struct efuse_rw_data rw_data;

	fd = open("/dev/efuse", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	rw_data.row = bit_offset / 32;
	rw_data.val = (u32 *)&efuse_val_32;
	start_bufidx = (bit_offset % 32) / 8;
	start_bit_offset = (bit_offset % 32) % 8;
	result_bufidx = 0;
	result_bit_offset = 0;
	write_complete = 0;

	ret = ioctl(fd, IOCTL_EFUSE_READ_ROW, &rw_data);
	if (ret < 0) {
		goto out;
	}
	efuse_val_ptr8 = (uint8_t *)rw_data.val;

	for (i = 0; i < bit_count; i++) {
		if (val[result_bufidx] & (1 << result_bit_offset)) {
			efuse_val_ptr8[start_bufidx] |= (1 << start_bit_offset);
		}

		result_bit_offset++;
		if (result_bit_offset == 8) {
			result_bufidx++;
			result_bit_offset = 0;
		}

		start_bit_offset++;
		if (start_bit_offset == 8) {
			start_bufidx++;
			if (start_bufidx == 4 && (i != (bit_count - 1))) {
				ret = ioctl(fd, IOCTL_EFUSE_WRITE_ROW, &rw_data);
				if (ret < 0) {
					goto out;
				}

				if (i + 1 == bit_count) {
					write_complete = 1;
					break;
				}

				rw_data.row++;
				ret = ioctl(fd, IOCTL_EFUSE_READ_ROW, &rw_data);
				if (ret < 0) {
					goto out;
				}


				efuse_val_ptr8 = (uint8_t *)rw_data.val;
				start_bufidx = 0;
			}

			start_bit_offset = 0;
		}
	}

	if (!write_complete) {
		ret = ioctl(fd, IOCTL_EFUSE_WRITE_ROW, &rw_data);
		if (ret < 0) {
			goto out;
		}
	}

out:
	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

#ifdef CONFIG_EFUSE_BUFFER_MODE

int scm_efuse_clr_buffer(uint16_t bit_offset, uint16_t bit_count)
{
	int fd;
	int i;
	int ret;
	int result_bufidx, result_bit_offset;
	int start_bufidx, start_bit_offset;
	int write_complete;
	uint32_t efuse_val_32;
	uint8_t *efuse_val_ptr8;
	uint32_t efuse_mode;
	struct efuse_rw_data rw_data;

	fd = open("/dev/efuse", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	ret = ioctl(fd, IOCTL_EFUSE_GET_MODE, &efuse_mode);
	if (ret < 0 || efuse_mode == SCM_EFUSE_MODE_RAW) {
		close(fd);
		return WISE_ERR_INVALID_STATE;
	}

	rw_data.row = bit_offset / 32;
	rw_data.val = (u32 *)&efuse_val_32;
	start_bufidx = (bit_offset % 32) / 8;
	start_bit_offset = (bit_offset % 32) % 8;
	result_bufidx = 0;
	result_bit_offset = 0;
	write_complete = 0;

	ret = ioctl(fd, IOCTL_EFUSE_READ_ROW, &rw_data);
	if (ret < 0) {
		goto out;
	}
	efuse_val_ptr8 = (uint8_t *)rw_data.val;

	for (i = 0; i < bit_count; i++) {
		efuse_val_ptr8[start_bufidx] &= ~(1 << start_bit_offset);

		result_bit_offset++;
		if (result_bit_offset == 8) {
			result_bufidx++;
			result_bit_offset = 0;
		}

		start_bit_offset++;
		if (start_bit_offset == 8) {
			start_bufidx++;
			if (start_bufidx == 4 && (i != (bit_count - 1))) {
				ret = ioctl(fd, IOCTL_EFUSE_WRITE_ROW, &rw_data);
				if (ret < 0) {
					goto out;
				}

				if (i + 1 == bit_count) {
					write_complete = 1;
					break;
				}

				rw_data.row++;
				ret = ioctl(fd, IOCTL_EFUSE_READ_ROW, &rw_data);
				if (ret < 0) {
					goto out;
				}


				efuse_val_ptr8 = (uint8_t *)rw_data.val;
				start_bufidx = 0;
			}

			start_bit_offset = 0;
		}
	}

	if (!write_complete) {
		ret = ioctl(fd, IOCTL_EFUSE_WRITE_ROW, &rw_data);
		if (ret < 0) {
			goto out;
		}
	}

out:
	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_efuse_set_mode(enum scm_efuse_mode mode)
{
	int fd;
	uint32_t efuse_mode;
	int ret;

	fd = open("/dev/efuse", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	efuse_mode = mode;

	ret = ioctl(fd, IOCTL_EFUSE_SET_MODE, &efuse_mode);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_efuse_get_mode(enum scm_efuse_mode *mode)
{
	int fd;
	uint32_t efuse_mode;
	int ret;

	fd = open("/dev/efuse", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	ret = ioctl(fd, IOCTL_EFUSE_GET_MODE, &efuse_mode);

	*mode = (enum scm_efuse_mode)efuse_mode;

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_efuse_sync(void)
{
	int fd;
	int ret;

	fd = open("/dev/efuse", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	ret = ioctl(fd, IOCTL_EFUSE_SYNC, NULL);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_efuse_load(void)
{
	int fd;
	int ret;

	fd = open("/dev/efuse", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	ret = ioctl(fd, IOCTL_EFUSE_LOAD, NULL);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

#endif

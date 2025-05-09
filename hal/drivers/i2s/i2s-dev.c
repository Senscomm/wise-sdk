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

#include <stdio.h>
#include <hal/init.h>
#include <hal/kernel.h>
#include <hal/pinctrl.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/i2s.h>
#include <hal/kmem.h>
#include <hal/dma.h>

#include "sys/ioctl.h"
#include "vfs.h"
#include "mmap.h"
#include "mem_slab.h"

static ssize_t i2s_block_read(struct device *dev, void *buf, size_t size)
{
	void *mem_block;
	size_t data_size;
	int ret;

	ret = i2s_buf_read(dev, &mem_block, &data_size);
	if (!ret) {
		const struct i2s_config *rx_cfg = i2s_config_get(dev, I2S_DIR_RX);
		data_size = min(data_size, size);
		memcpy((void *)buf, mem_block, data_size);
		/* Presumed to be configured otherwise the i2s_read() call
		 * would have failed.
		 */
		assert(rx_cfg);
		mem_slab_free(rx_cfg->mem_slab, mem_block);
		ret = data_size;
	}

	return ret;
}

ssize_t i2s_block_write(struct device *dev, void *buf, size_t size)
{
	const struct i2s_config *cfg;
	void *mem_block;
	int ret;

	cfg = i2s_config_get(dev, I2S_DIR_TX);
	if (!cfg) {
		return -EIO;
	}

	if (size > cfg->block_size) {
		size = cfg->block_size;
	}

	ret = mem_slab_alloc(cfg->mem_slab, &mem_block, osWaitForever);
	if (ret < 0) {
		return -ENOMEM;
	}

	memcpy(mem_block, (void *)buf, size);

	ret = i2s_buf_write(dev, mem_block, size);
	if (ret < 0) {
		mem_slab_free(cfg->mem_slab, mem_block);
        return ret;
	}

	return size;
}

int i2s_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct device *dev = file->f_priv;
	int ret = 0;

	switch (cmd) {
	case IOCTL_I2S_SET_CONFIG: {
		struct i2s_cfg_arg *cfg_arg = arg;
		ret = i2s_configure(dev, cfg_arg->dir, cfg_arg->cfg);
		break;
	}
	case IOCTL_I2S_GET_CONFIG: {
		struct i2s_cfg_arg *cfg_arg = arg;
        const struct i2s_config *i2s_cfg;
		i2s_cfg = i2s_config_get(dev, cfg_arg->dir);
        if (i2s_cfg) {
            memcpy(cfg_arg->cfg, i2s_cfg, sizeof(*cfg_arg->cfg));
        } else {
            ret = -EINVAL;
        }
		break;
	}
	case IOCTL_I2S_GET_STATE: {
		struct i2s_state_arg *state_arg = arg;
		ret = i2s_state_get(dev, state_arg->dir, &state_arg->state);
		break;
	}
	case IOCTL_I2S_TRIGGER: {
		struct i2s_trigger_arg *trigger_arg = arg;
		ret = i2s_trigger(dev, trigger_arg->dir, trigger_arg->cmd);
		break;
	}
	case IOCTL_I2S_BLOCK_READ: {
		struct i2s_block_rw_arg *block_rw_arg = arg;
		ret = i2s_block_read(dev, block_rw_arg->buf, block_rw_arg->size);
		if (ret > 0) {
			block_rw_arg->size = ret;
			ret = 0;
		}
		break;
    }
	case IOCTL_I2S_BLOCK_WRITE: {
		struct i2s_block_rw_arg *block_rw_arg = arg;
		ret = i2s_block_write(dev, block_rw_arg->buf, block_rw_arg->size);
		break;
    }
	default:
		ret = -EINVAL;
	}

	return ret;
}

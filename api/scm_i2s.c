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
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <cmsis_os.h>
#include "hal/i2s.h"
#include "hal/kmem.h"

#include "vfs.h"
#include "mmap.h"
#include "mem_slab.h"
#include "sys/ioctl.h"
#include "scm_i2s.h"

struct scm_i2s_data {
    int fd;
    char *dev_name;
    struct i2s_config cfg;
    uint8_t *slab_buf[2];
    struct mem_slab *mem_slab[2];
};

#define _dir(dir)	(dir == SCM_I2S_RX ? 0 : 1)

static struct scm_i2s_data i2s_data = {
    .fd = -1,
    .dev_name = "/dev/i2s",
};

static int scm_i2s_get_block_size(int duration_per_block, int fs, int word_length)
{
    uint32_t samples_per_block;
    uint32_t bytes_per_sample;
    int block_size;

    if (!duration_per_block) {
        return -1;
    }

    samples_per_block = (fs / (1000 / duration_per_block));
    samples_per_block *= 2; /* 2 channels (L/R) */

    /* XXX: platform specific?? */
    bytes_per_sample = (word_length == SCM_I2S_WL_16 ? 2 : 4);

    block_size = samples_per_block * bytes_per_sample;

    return block_size;
}

int scm_i2s_init(void)
{
    int fd;

    if (i2s_data.fd > 0) {
        return WISE_ERR_INVALID_STATE;
    }

    fd = open(i2s_data.dev_name, 0, 0);
    if (fd < 0) {
        return WISE_ERR_NOT_FOUND;
    }

    i2s_data.fd = fd;
    i2s_data.slab_buf[0] = NULL;
    i2s_data.mem_slab[0] = NULL;
    i2s_data.slab_buf[1] = NULL;
    i2s_data.mem_slab[1] = NULL;

    return WISE_OK;
}

int scm_i2s_deinit(void)
{
    if (i2s_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    close(i2s_data.fd);
    i2s_data.fd = -1;

    if (i2s_data.mem_slab[0] != NULL) {
        mem_slab_deinit(i2s_data.mem_slab[0]);
        free(i2s_data.mem_slab[0]);
        i2s_data.mem_slab[0] = NULL;
    }
    if (i2s_data.mem_slab[1] != NULL) {
        mem_slab_deinit(i2s_data.mem_slab[1]);
        free(i2s_data.mem_slab[1]);
        i2s_data.mem_slab[1] = NULL;
    }

    if (i2s_data.slab_buf[0] != NULL) {
        dma_free(i2s_data.slab_buf[0]);
        i2s_data.slab_buf[0] = NULL;
    }
    if (i2s_data.slab_buf[1] != NULL) {
        dma_free(i2s_data.slab_buf[1]);
        i2s_data.slab_buf[1] = NULL;
    }

    return WISE_OK;
}

static int scm_i2s_init_mem_slab(struct i2s_config *i2s, enum scm_i2s_direction dir,
        int block_size, int block_count)
{
    size_t bufsz;

    bufsz = block_count * block_size;

    if (i2s_data.slab_buf[_dir(dir)] != NULL) {
        dma_free(i2s_data.slab_buf[_dir(dir)]);
        i2s_data.slab_buf[_dir(dir)] = NULL;
    }

    i2s_data.slab_buf[_dir(dir)] = dma_malloc(bufsz);
    if (i2s_data.slab_buf[_dir(dir)] == NULL) {
        return -1;
    }
    memset(i2s_data.slab_buf[_dir(dir)], 0, bufsz);

    if (i2s_data.mem_slab[_dir(dir)] != NULL) {
        mem_slab_deinit(i2s_data.mem_slab[_dir(dir)]);
        free(i2s_data.mem_slab[_dir(dir)]);
        i2s_data.mem_slab[_dir(dir)] = NULL;
    }
    i2s_data.mem_slab[_dir(dir)] = zalloc(sizeof(struct mem_slab));
    i2s->mem_slab = i2s_data.mem_slab[_dir(dir)];
    if (mem_slab_init(i2s->mem_slab,  i2s_data.slab_buf[_dir(dir)], block_size, block_count)) {
        return -1;
    }

    return 0;
}

int scm_i2s_configure(struct scm_i2s_cfg *cfg)
{
    struct i2s_cfg_arg arg;
    struct i2s_state_arg state_arg;
    struct i2s_config *i2s;
    int block_size, block_count;

    if (i2s_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.cfg = i2s = &i2s_data.cfg;

    i2s->timeout = cfg->timeout;
    i2s->channels = 2;
    i2s->frame_clk_freq = cfg->fs;

    if (cfg->role == SCM_I2S_ROLE_MASTER) {
        i2s->options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER;
    } else {
        i2s->options = I2S_OPT_BIT_CLK_SLAVE | I2S_OPT_FRAME_CLK_SLAVE;
    }

    switch (cfg->word_length) {
    case SCM_I2S_WL_16:
        i2s->word_size = 16;
        break;
    case SCM_I2S_WL_20:
        i2s->word_size = 20;
        break;
    case SCM_I2S_WL_24:
        i2s->word_size = 24;
        break;
    default:
        return WISE_ERR_INVALID_ARG;
    }

    switch (cfg->format) {
    case SCM_I2S_FMT_I2S:
        i2s->format = I2S_FMT_DATA_FORMAT_I2S;
        break;
    case SCM_I2S_FMT_LJ:
        i2s->format = I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED;
        break;
    case SCM_I2S_FMT_RJ:
        i2s->format = I2S_FMT_DATA_FORMAT_RIGHT_JUSTIFIED;
        break;
    default:
        return WISE_ERR_INVALID_ARG;
    }

    if (cfg->duration_per_block == 0 || cfg->duration_per_block >= 1000) {
        return WISE_ERR_INVALID_ARG;
    }

    block_size = scm_i2s_get_block_size(cfg->duration_per_block, cfg->fs,
            cfg->word_length);
    block_count = cfg->number_of_blocks;

    i2s->block_size = block_size;

    if (cfg->dir & SCM_I2S_RX) {
        arg.dir = I2S_DIR_RX;
        state_arg.dir = I2S_DIR_RX;
        if (ioctl(i2s_data.fd, IOCTL_I2S_GET_STATE, &state_arg)) {
            return WISE_ERR_IOCTL;
        }
        if (state_arg.state != I2S_STATE_NOT_READY &&
            state_arg.state != I2S_STATE_READY) {
            return WISE_ERR_INVALID_STATE;
        }
        if (scm_i2s_init_mem_slab(i2s, SCM_I2S_RX, block_size, block_count)) {
            return WISE_ERR_NO_MEM;
        }
        if (ioctl(i2s_data.fd, IOCTL_I2S_SET_CONFIG, &arg)) {
            return WISE_ERR_IOCTL;
        }
    }

    if (cfg->dir & SCM_I2S_TX) {
        arg.dir = I2S_DIR_TX;
        state_arg.dir = I2S_DIR_TX;
        if (ioctl(i2s_data.fd, IOCTL_I2S_GET_STATE, &state_arg)) {
            return WISE_ERR_IOCTL;
        }
        if (state_arg.state != I2S_STATE_NOT_READY &&
            state_arg.state != I2S_STATE_READY) {
            return WISE_ERR_INVALID_STATE;
        }
        if (scm_i2s_init_mem_slab(i2s, SCM_I2S_TX, block_size, block_count)) {
            return WISE_ERR_NO_MEM;
        }
        if (ioctl(i2s_data.fd, IOCTL_I2S_SET_CONFIG, &arg)) {
            return WISE_ERR_IOCTL;
        }
    }

    return WISE_OK;
}

int scm_i2s_read_block(uint8_t *buf, size_t *size)
{
    struct i2s_block_rw_arg rw_arg;
    int ret = WISE_OK;

    if (i2s_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    rw_arg.buf = buf;
    rw_arg.size = *size;

    if (ioctl(i2s_data.fd, IOCTL_I2S_BLOCK_READ, &rw_arg) < 0) {
        if (errno == ENODATA) {
            /* I2S stream has not been enabled. */
            ret = WISE_ERR_NO_MEM;
        } else {
            ret = WISE_ERR_TIMEOUT;
        }
    }

    *size = rw_arg.size;

    return ret;
}

int scm_i2s_write_block(uint8_t *buf, size_t size)
{
    struct i2s_block_rw_arg rw_arg;
    int ret = WISE_OK;

    if (i2s_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    rw_arg.buf = buf;
    rw_arg.size = size;

    if (ioctl(i2s_data.fd, IOCTL_I2S_BLOCK_WRITE, &rw_arg) < 0) {
        ret = WISE_ERR_NO_MEM;
    }

    return ret;
}

int scm_i2s_start(enum scm_i2s_direction dir)
{
    struct i2s_trigger_arg arg;

    if (i2s_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.cmd = I2S_TRIGGER_START;

    if (dir & SCM_I2S_RX) {
        arg.dir = I2S_DIR_RX;
        if (ioctl(i2s_data.fd, IOCTL_I2S_TRIGGER, &arg)) {
            return WISE_ERR_IOCTL;
        }
    }

    if (dir & SCM_I2S_TX) {
        arg.dir = I2S_DIR_TX;
        if (ioctl(i2s_data.fd, IOCTL_I2S_TRIGGER, &arg)) {
            return WISE_ERR_IOCTL;
        }
    }

    return WISE_OK;
}

int scm_i2s_stop(enum scm_i2s_direction dir)
{
    struct i2s_trigger_arg arg;

    if (i2s_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    if (dir & SCM_I2S_RX) {
        arg.cmd = I2S_TRIGGER_DROP;
        arg.dir = I2S_DIR_RX;
        if (ioctl(i2s_data.fd, IOCTL_I2S_TRIGGER, &arg)) {
            return WISE_ERR_IOCTL;
        }
    }

    if (dir & SCM_I2S_TX) {
        arg.cmd = I2S_TRIGGER_DRAIN;
        arg.dir = I2S_DIR_TX;
        if (ioctl(i2s_data.fd, IOCTL_I2S_TRIGGER, &arg)) {
            return WISE_ERR_IOCTL;
        }
    }

    return WISE_OK;
}

int scm_i2s_get_block_buffer_size(struct scm_i2s_cfg *cfg)
{
    return scm_i2s_get_block_size(cfg->duration_per_block, cfg->fs,
            cfg->word_length);
}

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
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Reinhard Panhuber - rework to unmasked pointers
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "sdio-fifo.h"
#include <hal/unaligned.h>

// Suppress IAR warning
// Warning[Pa082]: undefined behavior: the order of volatile accesses is undefined in this statement
#if defined(__ICCARM__)
#pragma diag_suppress = Pa082
#endif

// implement mutex lock and unlock
#if CFG_FIFO_MUTEX

static inline void _ff_lock(sdio_fifo_mutex_t mutex)
{
    if (mutex) osMutexAcquire(mutex, osWaitForever);
}

static inline void _ff_unlock(sdio_fifo_mutex_t mutex)
{
    if (mutex) osMutexRelease(mutex);
}

#else

#define _ff_lock(_mutex)
#define _ff_unlock(_mutex)

#endif

bool sdio_fifo_config(sdio_fifo_t *f, void *buffer, uint16_t depth)
{
    if (depth > 0x8000) return false; // Maximum depth is 2^15 items

    _ff_lock(f->mutex_wr);
    _ff_lock(f->mutex_rd);

    f->buffer       = (uint8_t *)buffer;
    f->depth        = depth;

    f->rd_idx = f->wr_idx = f->len_waste = 0;

    _ff_unlock(f->mutex_wr);
    _ff_unlock(f->mutex_rd);

    return true;
}

bool sdio_fifo_config_nolock(sdio_fifo_t *f, void *buffer, uint16_t depth)
{
	if (depth > 0x8000) return false; // Maximum depth is 2^15 items

	f->buffer		= (uint8_t *)buffer;
	f->depth		= depth;

	f->rd_idx = f->wr_idx = f->len_waste = 0;

	return true;
}

#define BOUNDARY_INDX(f) ((f)->depth - CONFIG_SDIO_FIFO_MIN_LINEAR_SIZE)
#define INDX_TO_ADDR(f, idx) (((f)->buffer) + idx)
#define space(s, e, t, h) ({     \
    uint16_t __e;                \
    if (h < t)                   \
        __e = t - h;             \
    else                         \
        __e = (e - s) - (h - t); \
    __e;                         \
})

uint8_t *sdio_fifo_array_get_write_buf(sdio_fifo_t *f)
{
    int16_t w = f->wr_idx;
    int16_t r = f->rd_idx;

    if ((w >= r) && (f->depth - w < CONFIG_SDIO_FIFO_MIN_LINEAR_SIZE))
        f->wr_idx = 0;

    return (f->buffer + f->wr_idx);;
}

uint8_t *sdio_fifo_array_get_write_buf_locked(sdio_fifo_t *f)
{
    uint8_t *write_buf;

    _ff_lock(f->mutex_wr);
    write_buf = sdio_fifo_array_get_write_buf(f);
    _ff_unlock(f->mutex_wr);

    return write_buf;
}

uint8_t *sdio_fifo_array_get_read_buf(sdio_fifo_t *f)
{
    uint16_t r = f->rd_idx;

    if (r > BOUNDARY_INDX(f)) {
        if (!f->len_waste) {
            f->len_waste = f->depth - r;
            SDIO_LOG3("%s update len_waste = %d \n", __func__, f->len_waste);
        }
        return &f->buffer[0];
    }

    return &f->buffer[r];
}

uint8_t *sdio_fifo_array_get_read_buf_locked(sdio_fifo_t *f)
{
    uint8_t *read_buf;

    _ff_lock(f->mutex_rd);
    read_buf = sdio_fifo_array_get_read_buf(f);
    _ff_unlock(f->mutex_rd);

    return read_buf;
}

bool sdio_fifo_array_linear_space_available(sdio_fifo_t *f)
{
    uint16_t spaces;

    _ff_lock(f->mutex_wr);
    _ff_lock(f->mutex_rd);

    uint16_t r = f->rd_idx;
    uint16_t w = f->wr_idx;
    SDIO_LOG3("%s w = 0x%X r = 0x%X \n", __func__, INDX_TO_ADDR(f, w), INDX_TO_ADDR(f, r));

    // 0 ... r ... w ....end
    // 0 ... w ... r ....end

    spaces = space(0, f->depth, r, w);

    if (spaces < CONFIG_SDIO_FIFO_MIN_LINEAR_SIZE) {
        SDIO_LOG2("c1: spaces not enough");
        _ff_unlock(f->mutex_wr);
        _ff_unlock(f->mutex_rd);

        return false;
    }

    if (f->depth - w < CONFIG_SDIO_FIFO_MIN_LINEAR_SIZE) {
        spaces = space(0, f->depth, r, 0);
        if (spaces < CONFIG_SDIO_FIFO_MIN_LINEAR_SIZE) {
            SDIO_LOG2("c2: spaces not enough");
            _ff_unlock(f->mutex_wr);
            _ff_unlock(f->mutex_rd);

            return false;
        } else if (f->depth - r < CONFIG_SDIO_FIFO_MIN_LINEAR_SIZE) {
            SDIO_ASSERT(0);
        }
    }

    _ff_unlock(f->mutex_wr);
    _ff_unlock(f->mutex_rd);
    return true;
}

bool sdio_fifo_array_space_available(sdio_fifo_t *f, uint16_t need)
{
    bool ret = true;
    uint16_t spaces;

    _ff_lock(f->mutex_wr);
    _ff_lock(f->mutex_rd);

    uint16_t r = f->rd_idx;
    uint16_t w = f->wr_idx;

    // 0 ... r ... w ....end
    // 0 ... w ... r ....end

    spaces = space(0, f->depth, r, w);

    if (spaces < need) {
        SDIO_LOG2("spaces not enough");
        ret = false;
    }
    _ff_unlock(f->mutex_wr);
    _ff_unlock(f->mutex_rd);
    return ret;
}

void sdio_fifo_array_linearize(sdio_fifo_t *f, sdio_fifo_buffer_info_t* pfinfo)
{
    if (((uint8_t *)pfinfo->ptr_lin) > INDX_TO_ADDR(f, BOUNDARY_INDX(f))) {
        // pre list contains a wrap packet
        if (f->len_waste) {
            assert(((uint32_t)(pfinfo->ptr_lin + f->len_waste) >= (uint32_t) &f->buffer[f->depth]));
            pfinfo->ptr_lin = (pfinfo->ptr_lin + f->len_waste) - f->depth;

            SDIO_LOG2("%s already wrap = %d pfinfo->ptr_lin = 0x%X\n",
               __func__, f->len_waste, pfinfo->ptr_lin);
        } else {  // !f->len_waste
            f->len_waste = &f->buffer[f->depth] - (uint8_t *)pfinfo->ptr_lin;

            SDIO_LOG2("%s just cross boundary len_waste = %d pfinfo->ptr_lin = 0x%X\n",
                __func__, f->len_waste, pfinfo->ptr_lin);

            pfinfo->ptr_lin = &f->buffer[0];
        }
     }
}

void sdio_fifo_array_get_read_linear_info(sdio_fifo_t *f, sdio_fifo_buffer_info_t *info)
{
    uint16_t r = f->rd_idx;

    if (r > BOUNDARY_INDX(f)) {
        info->ptr_lin = &f->buffer[0];

        if (!f->len_waste) {
            f->len_waste = f->depth - r;
            SDIO_LOG3("%s update len_waste = %d \n", __func__, f->len_waste);
        }
    } else {
        info->ptr_lin = &f->buffer[r];
    }

    info->len_wrap = 0;
    info->ptr_wrap = NULL;

    SDIO_LOG3("%s len_waste = %d w:0x%X rd_idx:0x%X r:0x%X G:0x%X n:%d now_r_ptr:0x%X\n", __func__, f->len_waste, INDX_TO_ADDR(f, f->wr_idx), INDX_TO_ADDR(f, f->rd_idx), INDX_TO_ADDR(f, r), INDX_TO_ADDR(f, BOUNDARY_INDX(f)), cnt, info->ptr_lin);
}

__ilm__ static void sdio_fifo_array_advance_write_pointer(sdio_fifo_t *f, uint16_t n)
{
    uint16_t w = f->wr_idx, r = f->rd_idx;

    f->wr_idx = f->wr_idx + n;
    if (f->wr_idx > (f->depth - CONFIG_SDIO_FIFO_MIN_LINEAR_SIZE)) {
        f->wr_idx = 0;
    }

    // two cases
    // 0 ...r ... w ...depth
    // 0 ... w ... r ...depth
    w >= r ? assert(w < f->depth) : assert(r - w >= CONFIG_SDIO_FIFO_MIN_LINEAR_SIZE);
    SDIO_LOG2("%s w:0x%X n:%d, wr_idx:0x%X, rd_idx:0x%X, depth %X\n", __func__, INDX_TO_ADDR(f, w), n, INDX_TO_ADDR(f, f->wr_idx), INDX_TO_ADDR(f, f->rd_idx), INDX_TO_ADDR(f, f->depth));
}

__ilm__ void sdio_fifo_array_advance_write_locked(sdio_fifo_t *f, uint16_t n)
{
    _ff_lock(f->mutex_wr);
    sdio_fifo_array_advance_write_pointer(f, n);
    _ff_unlock(f->mutex_wr);
}

void sdio_fifo_array_advance_read(sdio_fifo_t *f, uint16_t n)
{
    uint16_t r = f->rd_idx;
    uint16_t w = f->wr_idx;

    // update r first
    r = r + n;

    if (r > BOUNDARY_INDX(f)) {
        /*
         * case 1: w has start from 0 and already move several packets
         * 0 ... w ...r ...end
         *
         * case 2: r == w means empty
         *
         */

        f->len_waste > 0 ? assert(r + f->len_waste >= f->depth) : assert(r < f->depth);
        if (r >= w) {
            if (f->len_waste) {
                r            = r + f->len_waste - f->depth;
                f->len_waste = 0;
            } else {
                r = 0;
            }
            /*
             * It's empty case
             * must update w = 0 here or it will get not enough in sdio_fifo_array_linear_space_available()
             */
            if (w > BOUNDARY_INDX(f)) {
                f->wr_idx = 0;
            }
        }
    }

    f->rd_idx = r;

    SDIO_LOG3("%s  n:%d rd_idx:0x%X wr_idx:0x%X waste_n = %d depth = 0x%X\n", __func__, n, INDX_TO_ADDR(f, f->rd_idx), INDX_TO_ADDR(f, f->wr_idx), f->len_waste, INDX_TO_ADDR(f, f->depth));
}

void sdio_fifo_array_advance_read_locked(sdio_fifo_t *f, uint16_t n)
{
    _ff_lock(f->mutex_rd);
    _ff_lock(f->mutex_wr);
    sdio_fifo_array_advance_read(f, n);
    _ff_unlock(f->mutex_rd);
    _ff_unlock(f->mutex_wr);
}

uint16_t sdio_fifo_array_get_read_idx(sdio_fifo_t *f)
{
	uint16_t rd_idx;

	_ff_lock(f->mutex_rd);
	rd_idx = f->rd_idx;
	_ff_unlock(f->mutex_rd);

	return rd_idx;
}

void sdio_fifo_array_reset(sdio_fifo_t *f)
{
	_ff_lock(f->mutex_rd);
	_ff_lock(f->mutex_wr);
	f->rd_idx = f->wr_idx = f->len_waste = 0;
	_ff_unlock(f->mutex_rd);
	_ff_unlock(f->mutex_wr);
}

#ifdef CONFIG_CMD_SDIO
void sdio_fifo_show_status(sdio_fifo_t *f)
{
	printk("rd_idx:%u wr_idx:%u waste_n:%u\n", f->rd_idx, f->wr_idx, f->len_waste);
}
#endif

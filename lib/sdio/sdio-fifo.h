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

#ifndef _SDIO_FIFO_H_
#define _SDIO_FIFO_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <hal/console.h>
#include <cmsis_os.h>

// Due to the use of unmasked pointers, this FIFO does not suffer from losing
// one item slice. Furthermore, write and read operations are completely
// decoupled as write and read functions do not modify a common state. Henceforth,
// writing or reading from the FIFO within an ISR is safe as long as no other
// process (thread or ISR) interferes.
// Also, this FIFO is ready to be used in combination with a DMA as the write and
// read pointers can be updated from within a DMA ISR. Overflows are detectable
// within a certain number (see tu_fifo_overflow()).

// mutex is only needed for RTOS
// for OS None, we don't get preempted
#if SDIO_FIFO_DBG
#define SDIO_LOG1 printk
#define SDIO_LOG2 printk
#define SDIO_LOG3 printk
#else
#define SDIO_LOG1(...)
#define SDIO_LOG2(...)
#define SDIO_LOG3(...)
#endif
#define SDIO_ASSERT assert

#define CFG_FIFO_MUTEX 1

#if CFG_FIFO_MUTEX
#define sdio_fifo_mutex_t osMutexId_t
#endif

typedef struct
{
    uint8_t *buffer;    ///< buffer pointer
    uint16_t depth;     ///< max items

    volatile uint16_t wr_idx; ///< write pointer
    volatile uint16_t rd_idx; ///< read pointer
    uint16_t          len_waste;

#if CFG_FIFO_MUTEX
    sdio_fifo_mutex_t mutex_wr;
    sdio_fifo_mutex_t mutex_rd;
#endif

} sdio_fifo_t;

typedef struct
{
    uint16_t len_lin;  ///< linear length in item size
    uint16_t len_wrap; ///< wrapped length in item size
    void    *ptr_lin;  ///< linear part start pointer
    void    *ptr_wrap; ///< wrapped part start pointer
    void    *priv_data;///< private data
    uint8_t  pre_len;   ///< pre length in item size
    uint8_t  pad_len;   ///< padded length in item size
} sdio_fifo_buffer_info_t;

#if CFG_FIFO_MUTEX
static inline void sdio_fifo_config_mutex(sdio_fifo_t *f, sdio_fifo_mutex_t write_mutex_hdl, sdio_fifo_mutex_t read_mutex_hdl)
{
    f->mutex_wr = write_mutex_hdl;
    f->mutex_rd = read_mutex_hdl;
}
#endif

static inline uint16_t sdio_fifo_depth(sdio_fifo_t *f)
{
    return f->depth;
}

bool     sdio_fifo_config(sdio_fifo_t *f, void *buffer, uint16_t depth);
bool     sdio_fifo_config_nolock(sdio_fifo_t *f, void *buffer, uint16_t depth);
uint8_t *sdio_fifo_array_get_write_buf(sdio_fifo_t *f);
uint8_t *sdio_fifo_array_get_write_buf_locked(sdio_fifo_t *f);
uint8_t *sdio_fifo_array_get_read_buf_locked(sdio_fifo_t *f);
bool     sdio_fifo_array_linear_space_available(sdio_fifo_t *f);
bool     sdio_fifo_array_space_available(sdio_fifo_t *f, uint16_t need);
void     sdio_fifo_array_linearize(sdio_fifo_t *f, sdio_fifo_buffer_info_t* pfinfo);
void     sdio_fifo_array_get_read_linear_info(sdio_fifo_t *f, sdio_fifo_buffer_info_t *info);
void     sdio_fifo_array_advance_write_locked(sdio_fifo_t *f, uint16_t n);
void     sdio_fifo_array_advance_read_locked(sdio_fifo_t *f, uint16_t n);
uint16_t sdio_fifo_array_get_read_idx(sdio_fifo_t *f);
void     sdio_fifo_array_reset(sdio_fifo_t *f);

#ifdef __cplusplus
}
#endif

#endif /* _SDIO_FIFO_H_ */

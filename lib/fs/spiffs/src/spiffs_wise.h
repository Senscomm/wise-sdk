/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __SPIFFS_WISE_H__
#define __SPIFFS_WISE_H__

#include <stdio.h>
#include <stdint.h>

#ifdef __WISE__
#include "hal/kernel.h"
#include "hal/console.h"

extern void spiffs_lock(void *);
extern void spiffs_unlock(void *);

#define SPIFFS_LOCK(fs)		spiffs_lock(fs)
#define SPIFFS_UNLOCK(fs)	spiffs_unlock(fs)

#else

#define SPIFFS_LOCK(fs)
#define SPIFFS_UNLOCK(fs)

#endif /* __WISE__ */

#define ASSERT(c, m) assert(c)

#ifdef DEBUG
#define SPIFFS_DBG(_f, ...) 		printk(_f, ## __VA_ARGS__)
#define SPIFFS_API_DBG(_f, ...) 	printf(_f, ## __VA_ARGS__)
#define SPIFFS_GC_DBG(_f, ...) 		printk(_f, ## __VA_ARGS__)
#define SPIFFS_CACHE_DBG(_f, ...) 	printk(_f, ## __VA_ARGS__)
#define SPIFFS_CHECK_DBG(_f, ...) 	printk(_f, ## __VA_ARGS__)
#endif

typedef int s32_t;
typedef unsigned u32_t;
typedef int16_t s16_t;
typedef uint16_t u16_t;
typedef int8_t s8_t;
typedef uint8_t u8_t;

#endif /* __SPIFFS_WISE_H__ */

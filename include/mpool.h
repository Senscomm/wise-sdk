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

#ifndef MPOOL_H
#define MPOOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#define POOL_BITSIZE(s) (s / 8 + (s % 8 != 0))

#define	POOL_ALLOC(name, type, size) \
	struct name##_bitmap \
	{ \
		uint8_t inusedcount; \
		uint8_t inused[POOL_BITSIZE(size)]; \
	} __packed; \
	typedef struct name##Pool_s \
	{ \
		type pool[size]; \
		struct name##_bitmap bitmap; \
	} name##Pool_t; \
	name##Pool_t name##Pool = {0};

#define POOL_COUNT(p) (p.bitmap.inusedcount)
#define POOL_BITMAP_INUSED_ARRAY(p, x) (p.bitmap.inused[x])
#define POOL_NOT_IN_USE(p, idx) ((p.bitmap.inused[idx >> 3] & (1 << (idx % 8))) == 0)
#define POOL_EQUAL(p, pf, idx) (pf == &(p.pool[idx]))
#define POOL_SET_IN_USE(p, idx) { \
	p.bitmap.inusedcount++; \
	p.bitmap.inused[(idx >> 3)] |= (1 << (idx % 8)); \
}
#define POOL_SET_NOT_IN_USE(p, idx) { \
	p.bitmap.inused[(idx >> 3)] &= ~(1 << (idx % 8)); \
	p.bitmap.inusedcount--; \
}
#define POOL_ADDR(p, idx) (&(p.pool[idx]))

#define PoolMalloc(p, s, pr) { \
	vTaskSuspendAll(); \
	if ((POOL_COUNT(p) < s)) \
	{ \
		int empty_pl = s; \
		int i = 0, j = 0; \
		\
		for (i = 0; i < POOL_BITSIZE(s); i++) { \
			uint8_t bitc; \
			if (i == (POOL_BITSIZE(s) - 1)) \
				bitc = ((s % 8) == 0)? 8 : (s % 8); \
			else \
				bitc = 8; \
			\
			if (POOL_BITMAP_INUSED_ARRAY(p, i) == ((2^bitc) - 1)) \
				continue; \
			\
			for (j = 0; j < bitc; j++) { \
				int empty_tmp = (i << 3) + j; \
				if (POOL_NOT_IN_USE(p, empty_tmp)) { \
					empty_pl = empty_tmp; \
					goto pl_found; \
				} \
			} \
		} \
pl_found: \
		if (empty_pl < s) { \
			pr = POOL_ADDR(p, empty_pl); \
			memset(pr, 0, sizeof(*pr)); \
			POOL_SET_IN_USE(p, empty_pl); \
		} \
	} \
	( void ) xTaskResumeAll(); \
}

#define PoolFree(p, s, pr) \
	if ((uint32_t) pr >= (uint32_t) POOL_ADDR(p, 0) \
			&& (uint32_t)pr <= (uint32_t)POOL_ADDR(p, s-1)) { \
		uint8_t plidx; \
		configASSERT((((uint32_t) pr - (uint32_t) POOL_ADDR(p, 0)) % sizeof(*pr) == 0)); \
		plidx = ((uint32_t) pr - (uint32_t) POOL_ADDR(p, 0))/sizeof(*pr); \
		vTaskSuspendAll(); \
		POOL_SET_NOT_IN_USE(p, plidx); \
		( void ) xTaskResumeAll(); \
	} else { \
		vPortFree( pr ); \
	}

#ifdef __cplusplus
}
#endif

#endif /* MPOOL_H */

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

#ifndef __KERNEL_H__
#define __KERNEL_H__

#define __KERNEL__

#include <stddef.h>
#include <bitfield.h>
#include <hal/compiler.h>
#include <hal/rom.h>
#include <hal/bitops.h>
#include <hal/io.h>
#include <hal/irq.h>
#include <errno.h>
#include <assert.h>
#include <strings.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif


#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define FIELD_SIZE(t, f) (sizeof(((t *)0)->f))

#ifndef min
#define min(x, y) 	((x) > (y)? (y): (x))
#endif
#ifndef max
#define max(x, y) 	((x) > (y)? (x): (y))
#endif

#define container_of(ptr, type, member) ({		\
	const typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) );})

#ifndef roundup
#define roundup(x, y) ( \
{ \
	const typeof(y) __y = y; \
	(((x) + (__y - 1)) / __y) * __y; \
} \
)
#endif

#ifndef roundup
#define rounddown(x, y) ( \
{ \
	typeof(x) __x = (x); \
	__x - ( __x % (y)); \
} \
)
#endif

#define PAGE_SIZE		4096
#define __bf_shft(x)		(__builtin_ffsll(x) - 1)

#define IS_ERR(p)		((p) == NULL)
#define IS_ERR_OR_NULL(p)	IS_ERR(p)

#define IS_ALIGNED(x, a)	(((x) & ((typeof(x))(a) - 1)) == 0)
#define PTR_IS_ALIGNED(p, a) 	((((unsigned long)(p)) & ((a)-1)) == 0)

#define GENMASK(h, l)		bitmask(l, h)
#define GENMASK64(h, l)		bitmask64(l, h)
#define FIELD_PREP(_mask, _val)	((typeof(_mask))(_val) << __bf_shft(_mask) & (_mask))
#define FIELD_GET(_mask, _reg)	((typeof(_mask))(((_reg) & (_mask)) >> __bf_shft(_mask)))

#define upper_32_bits(n)	((u32)(((n) >> 16) >> 16))
#define lower_32_bits(n)	((u32) (n))

#define spin_lock_irqsave(l, f) { (void) f; }
#define spin_unlock_irqrestore(l, f) { (void) f; }

#define EXPORT_SYMBOL_GPL(f)

#define DIV_ROUND_UP(x, y)   	(((x) + (y) - 1)/(y))
#define BITS_TO_LONGS(nr)    	DIV_ROUND_UP(nr, BITS_PER_LONG)
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define BITS_PER_LONG					(sizeof(long) * CHAR_BIT)
#define BITMAP_LAST_WORD_MASK(n)		(~0UL >> (BITS_PER_LONG - (n)))


#define ffz(x)			__ffs(~(x))

static inline int __fls(unsigned int x)
{
	return x ? sizeof(x) * 8 - __builtin_clz(x): 0;
}

static inline int ilog2(u32 n)
{
	return __fls(n) - 1;
}

static inline unsigned long __ffs(unsigned long word)
{
	int num = 0;

	if ((word & 0xffff) == 0) {
		num += 16;
		word >>= 16;
	}
	if ((word & 0xff) == 0) {
		num += 8;
		word >>= 8;
	}
	if ((word & 0xf) == 0) {
		num += 4;
		word >>= 4;
	}
	if ((word & 0x3) == 0) {
		num += 2;
		word >>= 2;
	}
	if ((word & 0x1) == 0)
		num += 1;
	return num;
}

static inline unsigned long find_first_zero_bit(const unsigned long *addr,
						unsigned long size)
{
	unsigned long idx;

	for (idx = 0; idx * BITS_PER_LONG < size; idx++) {
		if (addr[idx] != ~0UL)
			return min(idx * BITS_PER_LONG + ffz(addr[idx]), size);
	}

	return size;
}

/*
 * Find the first set bit in a memory region.
 */
static inline unsigned long find_first_bit(const unsigned long *addr, unsigned long size)
{
  unsigned long idx;

  for (idx = 0; idx * BITS_PER_LONG < size; idx++) {
    if (addr[idx])
      return min(idx * BITS_PER_LONG + __ffs(addr[idx]), size);
  }

  return size;
}

static inline void set_bit(unsigned int nr, volatile unsigned long *p)
{
	p += BIT_WORD(nr);
	*p |= BIT_MASK(nr);
}

static inline void clear_bit(unsigned int nr, volatile unsigned long *p)
{
	p += BIT_WORD(nr);
	*p &= ~BIT_MASK(nr);
}

#ifdef __cplusplus
}
#endif

#include <soc.h>

#endif

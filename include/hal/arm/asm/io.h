/*
 * Copyright (c) 2020-2024 Senscomm Semiconductor Co., Ltd. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __ARM_IO_H__
#define __ARM_IO_H__

#include <asm/barriers.h>

#if 0
#if CONFIG_SYS_ARM_ARCH == 7
#define isb(x) __asm__ __volatile__ ("isb " #x : : : "memory")
#define dsb(x) __asm__ __volatile__ ("dsb " #x : : : "memory")
#define dmb(x) __asm__ __volatile__ ("dmb " #x : : : "memory")

#elif CONFIG_SYS_ARM_ARCH == 6
#define isb(x) __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" \
                                    : : "r" (0) : "memory")
#define dsb(x) __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" \
                                    : : "r" (0) : "memory")
#define dmb(x) __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 5" \
				    : : "r" (0) : "memory")
#else
#define isb(x) __asm__ __volatile__ ("" : : : "memory")
#define dsb(x) __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" \
                                    : : "r" (0) : "memory")
#define dmb(x) __asm__ __volatile__ ("" : : : "memory")
#endif
#endif


#if CONFIG_SYS_ARM_ARCH >= 6
#define rmb()		dsb()
#define wmb()		dsb()
#else
#define rmb()		__asm__ __volatile__("": : :"memory")
#define wmb()		__asm__ __volatile__("": : :"memory")
#endif

#define __iormb()	rmb()
#define __iowmb()	wmb()

#define readl(a) \
	({u32 __v = *((volatile u32 *)(a)); __iormb(); __v; })

#define readw(a) \
	({u16 __v = *((volatile u16 *)(a)); __iormb(); __v; })

#define readb(a) \
	({u8 __v = *((volatile u8 *)(a)); __iormb(); __v; })

#define writel(v, a) \
	({u32 __v = v; __iowmb(); *((volatile u32 *)(a)) = __v; })

#define writew(v, a) \
	({u16 __v = v; __iowmb(); *((volatile u16 *)(a)) = __v; })

#define writeb(v, a) \
	({u8  __v = v; __iowmb(); *((volatile u8 *)(a)) = __v; })


#endif /* __ARM_IO_H__ */

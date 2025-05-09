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
#ifndef __WISE_IO_H__
#define __WISE_IO_H__

#include <hal/types.h>
#include <hal/kernel.h>
#ifndef CONFIG_SANDBOX
#include <asm/io.h>
#endif

#define __iomem

#define barrier() __asm__ __volatile__("": : :"memory")

#ifndef readl
#define readl(a) \
	({u32 __v = *((volatile u32 *)(a)); barrier(); __v; })
#endif

#ifndef readw
#define readw(a) \
	({u16 __v = *((volatile u16 *)(a)); barrier(); __v; })
#endif

#ifndef readb
#define readb(a) \
	({u8 __v = *((volatile u8 *)(a)); barrier(); __v; })
#endif

#ifndef writel
#define writel(v, a) \
	({u32 __v = v; barrier(); *((volatile u32 *)(a)) = __v; })
#endif

#ifndef writew
#define writew(v, a) \
	({u16 __v = v; barrier(); *((volatile u16 *)(a)) = __v; })
#endif

#ifndef writeb
#define writeb(v, a) \
	({u8  __v = v; barrier(); *((volatile u8 *)(a)) = __v; })
#endif


void memset_io(void __iomem *to, int c, size_t len);
void memcpy_toio(void __iomem *to, const void *from, size_t len);
void memcpy_fromio(void * to, void __iomem *from, size_t len);

#endif /* __WISE_IO_H__ */

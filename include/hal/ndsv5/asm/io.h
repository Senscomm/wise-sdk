/*
 * Copyright (c) 2021-2024 Senscomm Semiconductor Co., Ltd. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __NDSV5_IO_H__
#define __NDSV5_IO_H__

#include <nds_intrinsic.h>

/* Access to device regions is guaranteed to be strictly-ordered */
#ifdef CONFIG_DEVICE_REGION
#define __iormb()
#define __iowmb()
#else
#define __iormb()	__nds__fence(FENCE_IR, FENCE_IORW)
#define __iowmb()	__nds__fence(FENCE_OW, FENCE_IORW)
#endif

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

#endif /* __NDSV5_IO_H__ */

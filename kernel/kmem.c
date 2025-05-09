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

#include <string.h>
#include <stdio.h>

#include <FreeRTOS/FreeRTOS.h>
#include "cmsis_os.h"
#include "malloc.h"

#include <cli.h>
#include <hal/kmem.h>

static void conv_to_str(char *out, u32 in) __maybe_unused;
static void conv_to_str(char *out, u32 in)
{
	int i;
	u8 b;
	for (i = 0; i < 8; i++) {
		b = (in >> (4 * (7 - i))) & 0xf;
		*(out + i) = b < 10 ? '0' + b : 'a' + (b - 10);
	}
}

static void *_kmalloc(size_t size)
{
#ifdef CONFIG_MEM_HEAP_DEBUG
#if CONFIG_MEM_HEAP_DEBUG_FUNCNAMELEN < 14
#error "FUNCNAMELEN is too small."
#endif
	u32 ra = (u32)__builtin_return_address(0);
	char caller[14];

	memset(caller, 0, sizeof(caller));
	caller[0] = 'k';
	caller[1] = 'm';
	caller[2] = '-';
	conv_to_str(caller + 3, ra);
	return pvPortMallocDbg(size, caller);
#else
	return pvPortMalloc(size);
#endif
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(kmalloc, &kmalloc, &_kmalloc);
#else
__func_tab__ void *(*kmalloc)(size_t size) = _kmalloc;
#endif

void *kzalloc(size_t size)
{
	char *buf = kmalloc(size);

	memset(buf, 0, size);

	return buf;
}

void _kfree(void *ptr);

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(kfree, &kfree, &_kfree);
#else
__func_tab__ void (*kfree)(void *ptr) = _kfree;
#endif

void _kfree(void *ptr)
{
	vPortFree(ptr);
}

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
void *dma_kmalloc(size_t size)
{
#ifdef CONFIG_MEM_HEAP_DEBUG
#if CONFIG_MEM_HEAP_DEBUG_FUNCNAMELEN < 14
#error "FUNCNAMELEN is too small."
#endif
	u32 ra = (u32)__builtin_return_address(0);
	char caller[14];

	memset(caller, 0, sizeof(caller));
	caller[0] = 'k';
	caller[1] = 'm';
	caller[2] = '-';
	conv_to_str(caller + 3, ra);
	return pvPortMallocDMA(size, caller);
#else
	return pvPortMallocDMA(size);
#endif
}

void dma_kfree(void *ptr)
{
	vPortFree(ptr);
}
#endif

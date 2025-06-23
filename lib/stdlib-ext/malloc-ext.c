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

#include <string.h>
#include <stdio.h>
#include <FreeRTOS/FreeRTOS.h>

#include <hal/compiler.h>
#include <hal/console.h>

#ifdef CONFIG_MEM_HEAP_DEBUG

#define xstr(s) str(s)
#define str(s) #s
#define FMT "%-" xstr(CONFIG_MEM_HEAP_DEBUG_FUNCNAMELEN+4) "s"

void os_free(void *ptr);

void *os_malloc_dbg(size_t size, const char *func_name)
{
	void *p = NULL;
	p = pvPortMallocDbg(size, func_name);
	return p;
}

void *os_calloc_dbg(size_t nmemb, size_t size, const char *func_name)
{
	void *p;

	if ((p = os_malloc_dbg(nmemb * size, func_name)) == NULL)
		return NULL;
	memset(p, 0, nmemb * size);

	return p;
}

void *os_zalloc_dbg(size_t n, const char *func_name)
{
	void *p = os_malloc_dbg(n, func_name);
	if (!p)
		return NULL;
	memset(p, 0, n);

	return p;
}

void *os_realloc_dbg(void *ptr, size_t size, const char *func_name)
{
	void *n;

	if (ptr && size == 0) {
		os_free(ptr);
		return NULL;
	} else if (ptr == NULL) {
		return os_malloc_dbg(size, func_name);
	}

	n = os_malloc_dbg(size, func_name);
	if (!n)
		return NULL;

	memcpy(n, ptr, size);
	os_free(ptr);
	return n;
}


#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
void *os_dma_malloc_dbg(size_t size, const char *func_name)
{
	void *p = NULL;
	p = pvPortMallocDMA(size, func_name);
	return p;
}

void os_dma_free_dbg(void *ptr)
{
	vPortFree(ptr);
}
#endif

void *__wrap_malloc(size_t sz)
{
    return os_malloc_dbg(sz, __func__);
}

void __wrap_free(void *ptr)
{
    os_free(ptr);
}

void *__wrap_realloc(void *ptr, size_t sz)
{
    return os_realloc_dbg(ptr, sz, __func__);
}

void *__wrap_calloc(size_t n, size_t sz)
{
    return os_calloc_dbg(n, sz, __func__);
}

#else

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
void *os_dma_malloc(size_t size)
{
	void *p = NULL;
	p = pvPortMallocDMA(size);
	return p;
}

void os_dma_free(void *ptr)
{
	vPortFree(ptr);
}
#endif

extern void *os_malloc(size_t);
extern void os_free(void *);
extern void *os_calloc(size_t, size_t);
extern void *os_realloc(void *, size_t);

void *__wrap_malloc(size_t sz)
{
    return os_malloc(sz);
}

void __wrap_free(void *ptr)
{
    os_free(ptr);
}

void *__wrap_realloc(void *ptr, size_t sz)
{
    return os_realloc(ptr, sz);
}

void *__wrap_calloc(size_t n, size_t sz)
{
    return os_calloc(n, sz);
}

#endif /* CONFIG_MEM_HEAP_DEBUG */

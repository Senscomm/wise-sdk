#ifndef __WISE_STDLIB_H__
#define __WISE_STDLIB_H__

#include_next <stdlib.h>

#ifndef __USE_NATIVE_HEADER__

int os_system(const char *);

#define system os_system

#ifdef CONFIG_MEM_HEAP_DEBUG

void os_free(void *);
void *os_malloc_dbg(size_t size, const char *func_name);
void *os_calloc_dbg(size_t nmemb, size_t size, const char *func_name_);
void *os_zalloc_dbg(size_t n, const char *func_name);
void *os_realloc_dbg(void *ptr, size_t size, const char *func_name);

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
void *os_dma_malloc_dbg(size_t size, const char *func_name);
void os_dma_free_dbg(void *, const char *func_name);
#endif

#undef malloc
#define malloc(s) os_malloc_dbg(s, __func__)

#undef calloc
#define calloc(n, s) os_calloc_dbg(n, s, __func__)

#define mbedtls_calloc(n, s) os_calloc_dbg(n, s, __func__)

#undef zalloc
#define zalloc(s) os_zalloc_dbg(s, __func__)

#undef realloc
#define realloc(p,s) os_realloc_dbg(p, s, __func__)

#define free   os_free

#define os_malloc(s) os_malloc_dbg(s, __func__)
#define os_zalloc(s) os_zalloc_dbg(s, __func__)
#define os_calloc(n, s) os_calloc_dbg(n, s, __func__)
#define os_realloc(p, s) os_realloc_dbg(p ,s, __func__)

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
#define dma_malloc(s) os_dma_malloc_dbg(s, __func__)
#define dma_free(p) os_dma_free_dbg(p, __func__)
#define os_dma_malloc(s) os_dma_malloc_dbg(s, __func__)
#define os_dma_free(p) os_dma_free_dbg(p, __func__)
#endif

#else

void os_free(void *);
void *os_malloc(size_t);
void *os_zalloc(size_t);
void *os_calloc(size_t, size_t);
void *os_realloc(void *, size_t);
#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
void *os_dma_malloc(size_t);
void *os_dma_free(void *);
#endif

#define malloc os_malloc
#define free   os_free
#define calloc os_calloc
#define zalloc os_zalloc
#define realloc os_realloc
#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
#define dma_malloc os_dma_malloc
#define dma_free os_dma_free
#endif

#endif /* CONFIG_MEM_HEAP_DEBUG */
float os_strtof(const char *str, char **endptr);
double os_strtod(const char *str, char **endptr);
double os_atof(const char *nptr);

#undef strtof
#define strtof   os_strtof
#undef strtod
#define strtod   os_strtod
#undef atof
#define atof     os_atof

#endif /* __USE_NATIVE_HEADER__ */

#endif

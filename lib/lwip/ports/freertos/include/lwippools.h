/**
 * @file
 * lwIP custom memory pools (do not use in application code)
 */

#include <freebsd/mbuf.h>

/*
 * A list of internal pools used by net80211.
 *
 * LWIP_MEMPOOL(pool_name, number_elements, element_size, pool_description)
 *     creates a pool name MEMP_pool_name. description is used in stats.c
 */

#ifdef CONFIG_IEEE80211_HOSTED
/*
 * Some condition buffer* will be located in d25
 * Separate to different sections and start with 32-byte alignment address is a better solution
 */
#define MEM_32ALIGN_SIZE(size) (((size) + 32 - 1U) & ~(32-1U))
LWIP_MEMPOOL_SECTION(MBUF_CACHE, MEMP_NUM_MBUF_CACHE, MEM_32ALIGN_SIZE(__MSIZE__), "MBUF_CACHE", .buffer_cache)
LWIP_MEMPOOL_SECTION(MBUF_CHUNK, MEMP_NUM_MBUF_CHUNK, MEM_32ALIGN_SIZE(__MCLBYTES__), "MBUF_CHUNK", .buffer_chunk)
LWIP_MEMPOOL_SECTION(MBUF_EXT_NODE, MEMP_NUM_MBUF_DYNA_EXT, MEM_32ALIGN_SIZE(sizeof(struct mbuf3)), "MBUF_EXT_NODE", .buffer_ext)
#else
/* LWIP_MEM_ALIGN_SIZE: 4-byte alignment */
LWIP_MEMPOOL_SECTION(MBUF_CACHE, MEMP_NUM_MBUF_CACHE, LWIP_MEM_ALIGN_SIZE(__MSIZE__), "MBUF_CACHE", .buffer_cache)
LWIP_MEMPOOL_SECTION(MBUF_CHUNK, MEMP_NUM_MBUF_CHUNK, LWIP_MEM_ALIGN_SIZE(__MCLBYTES__), "MBUF_CHUNK", .buffer_chunk)
LWIP_MEMPOOL_SECTION(MBUF_EXT_NODE, MEMP_NUM_MBUF_DYNA_EXT, LWIP_MEM_ALIGN_SIZE(sizeof(struct mbuf3)), "MBUF_EXT_NODE", .buffer_ext)
#endif

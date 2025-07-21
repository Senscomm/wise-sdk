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
/*
 * xmem.c - LWIP memory pool customization
 */
/*
 * NB: We allow pool of zero size. In that case, we resort to malloc/free
 */

/**
 * @file
 * Dynamic pool memory manager
 *
 * lwIP has dedicated pools for many structures (netconn, protocol control blocks,
 * packet buffers, ...). All these pools are managed here.
 *
 * @defgroup mempool Memory pools
 * @ingroup infrastructure
 * Custom memory pools

 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "hal/rom.h"

#include "lwip/opt.h"
#include "lwip/memp.h"
#include "lwip/sys.h"
#include "lwip/stats.h"

#include <string.h>

/* Make sure we include everything we need for size calculation required by memp_std.h */
#include "lwip/pbuf.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/altcp.h"
#include "lwip/ip4_frag.h"
#include "lwip/netbuf.h"
#include "lwip/api.h"
#include "lwip/priv/tcpip_priv.h"
#include "lwip/priv/api_msg.h"
#ifdef __WISE__
#include <sys/socket.h>
#else
#include "lwip/sockets.h"
#endif /* __WISE__ */
#include "lwip/priv/sockets_priv.h"
#include "lwip/netifapi.h"
#include "lwip/etharp.h"
#include "lwip/igmp.h"
#include "lwip/timeouts.h"
/* needed by default MEMP_NUM_SYS_TIMEOUT */
#include "netif/ppp/ppp_opts.h"
#ifdef __WISE__
#include <netdb.h>
#else
#include "lwip/netdb.h"
#endif
#include "lwip/dns.h"
#include "lwip/priv/nd6_priv.h"
#include "lwip/ip6_frag.h"
#include "lwip/mld6.h"

/* Keep memp indices same whether specific features are enabled or not so that
 * lwIP configurations can be adjusted without breaking prebuilt libraries accessing
 * specific memory pools.
 */
static const struct memp_desc memp_dummy = {0};

#define LWIP_MEMPOOL_SECTION(name,num,size,desc,s) LWIP_MEMPOOL_DECLARE_SECTION(name,num,size,desc,s)
#define LWIP_MEMPOOL(name,num,size,desc) LWIP_MEMPOOL_DECLARE_SECTION(name,num,size,desc,CONFIG_MEM_MEMP_DEFAULT_SECTION)
#define LWIP_MEMPOOL_DUMMY(name)
#include "lwip/priv/memp_std.h"

const struct memp_desc *const memp_pools[MEMP_MAX] = {
#define LWIP_MEMPOOL_SECTION(name,num,size,desc,s) &memp_ ## name,
#define LWIP_MEMPOOL(name,num,size,desc) &memp_ ## name,
#define LWIP_MEMPOOL_DUMMY(name) &memp_dummy,
#include "lwip/priv/memp_std.h"
};

#ifdef LWIP_HOOK_FILENAME
#include LWIP_HOOK_FILENAME
#endif

#if MEMP_MEM_MALLOC && MEMP_OVERFLOW_CHECK >= 2
#undef MEMP_OVERFLOW_CHECK
/* MEMP_OVERFLOW_CHECK >= 2 does not work with MEMP_MEM_MALLOC, use 1 instead */
#define MEMP_OVERFLOW_CHECK 1
#endif

#if MEMP_SANITY_CHECK && !MEMP_MEM_MALLOC
/**
 * Check that memp-lists don't form a circle, using "Floyd's cycle-finding algorithm".
 */
static int memp_sanity(const struct memp_desc *desc)
{
	struct memp *t, *h;

	t = *desc->tab;
	if (t != NULL) {
		for (h = t->next; (t != NULL) && (h != NULL); t = t->next,
			     h = ((h->next != NULL) ? h->next->next : NULL)) {
			if (t == h) {
				return 0;
			}
		}
	}

	return 1;
}
#endif /* MEMP_SANITY_CHECK && !MEMP_MEM_MALLOC */

#if MEMP_OVERFLOW_CHECK
/**
 * Check if a memp element was victim of an overflow
 * (e.g. the restricted area after it has been altered)
 *
 * @param p the memp element to check
 * @param desc the pool p comes from
 */
static void
memp_overflow_check_element_overflow(struct memp *p, const struct memp_desc *desc)
{
#if MEM_SANITY_REGION_AFTER_ALIGNED > 0
	u16_t k;
	u8_t *m;
	if (!desc->num)
		return;
	m = (u8_t *)p + MEMP_SIZE + desc->size;
	for (k = 0; k < MEM_SANITY_REGION_AFTER_ALIGNED; k++) {
		if (m[k] != 0xcd) {
			char errstr[128] = "detected memp overflow in pool ";
			strcat(errstr, desc->desc);
			LWIP_ASSERT(errstr, 0);
		}
	}
#else /* MEM_SANITY_REGION_AFTER_ALIGNED > 0 */
	LWIP_UNUSED_ARG(p);
	LWIP_UNUSED_ARG(desc);
#endif /* MEM_SANITY_REGION_AFTER_ALIGNED > 0 */
}

/**
 * Check if a memp element was victim of an underflow
 * (e.g. the restricted area before it has been altered)
 *
 * @param p the memp element to check
 * @param desc the pool p comes from
 */
static void
memp_overflow_check_element_underflow(struct memp *p, const struct memp_desc *desc)
{
#if MEM_SANITY_REGION_BEFORE_ALIGNED > 0
	u16_t k;
	u8_t *m;
	if (!desc->num)
		return;
	m = (u8_t *)p + MEMP_SIZE - MEM_SANITY_REGION_BEFORE_ALIGNED;
	for (k = 0; k < MEM_SANITY_REGION_BEFORE_ALIGNED; k++) {
		if (m[k] != 0xcd) {
			char errstr[128] = "detected memp underflow in pool ";
			strcat(errstr, desc->desc);
			LWIP_ASSERT(errstr, 0);
		}
	}
#else /* MEM_SANITY_REGION_BEFORE_ALIGNED > 0 */
	LWIP_UNUSED_ARG(p);
	LWIP_UNUSED_ARG(desc);
#endif /* MEM_SANITY_REGION_BEFORE_ALIGNED > 0 */
}

/**
 * Initialize the restricted area of on memp element.
 */
static void
memp_overflow_init_element(struct memp *p, const struct memp_desc *desc)
{
#if MEM_SANITY_REGION_BEFORE_ALIGNED > 0 || MEM_SANITY_REGION_AFTER_ALIGNED > 0
	u8_t *m;
#if MEM_SANITY_REGION_BEFORE_ALIGNED > 0
	m = (u8_t *)p + MEMP_SIZE - MEM_SANITY_REGION_BEFORE_ALIGNED;
	memset(m, 0xcd, MEM_SANITY_REGION_BEFORE_ALIGNED);
#endif
#if MEM_SANITY_REGION_AFTER_ALIGNED > 0
	m = (u8_t *)p + MEMP_SIZE + desc->size;
	memset(m, 0xcd, MEM_SANITY_REGION_AFTER_ALIGNED);
#endif
#else /* MEM_SANITY_REGION_BEFORE_ALIGNED > 0 || MEM_SANITY_REGION_AFTER_ALIGNED > 0 */
	LWIP_UNUSED_ARG(p);
	LWIP_UNUSED_ARG(desc);
#endif /* MEM_SANITY_REGION_BEFORE_ALIGNED > 0 || MEM_SANITY_REGION_AFTER_ALIGNED > 0 */
}

#if MEMP_OVERFLOW_CHECK >= 2
/**
 * Do an overflow check for all elements in every pool.
 *
 * @see memp_overflow_check_element for a description of the check
 */
static void
memp_overflow_check_all(void)
{
	u16_t i, j;
	struct memp *p;
	SYS_ARCH_DECL_PROTECT(old_level);
	SYS_ARCH_PROTECT(old_level);

	for (i = 0; i < MEMP_MAX; ++i) {
		p = (struct memp *)LWIP_MEM_ALIGN(memp_pools[i]->base);
		for (j = 0; j < memp_pools[i]->num; ++j) {
			memp_overflow_check_element_overflow(p, memp_pools[i]);
			memp_overflow_check_element_underflow(p, memp_pools[i]);
			p = LWIP_ALIGNMENT_CAST(struct memp *, ((u8_t *)p + MEMP_SIZE + memp_pools[i]->size + MEM_SANITY_REGION_AFTER_ALIGNED));
		}
	}
	SYS_ARCH_UNPROTECT(old_level);
}
#endif /* MEMP_OVERFLOW_CHECK >= 2 */
#endif /* MEMP_OVERFLOW_CHECK */

#if MEMP_OWNER_CHECK
/**
 * Check owners of used memp blocks.
 *
 * @param desc the pool where to check
 */
void
memp_check_owners(const char *desc)
{
	u16_t i, j, k;
	struct memp *p;

	if (desc == NULL || desc == &memp_dummy) {
		return;
	}

	for (i = 0; i < MEMP_MAX; ++i) {
		if (!strcmp(memp_pools[i]->desc, desc)) {
			p = (struct memp *)LWIP_MEM_ALIGN(memp_pools[i]->base);
			k = 0;
			for (j = 0; j < memp_pools[i]->num; ++j) {
				if (p->func != NULL)
					printf("[%3d] %32s,\t%5d,\t0x%x\n", ++k, p->func, p->line, (u32_t)((u8_t *)p + MEMP_SIZE));
#if MEMP_OVERFLOW_CHECK
				p = LWIP_ALIGNMENT_CAST(struct memp *, ((u8_t *)p + MEMP_SIZE + memp_pools[i]->size + MEM_SANITY_REGION_AFTER_ALIGNED));
#else
				p = LWIP_ALIGNMENT_CAST(struct memp *, ((u8_t *)p + MEMP_SIZE + memp_pools[i]->size));
#endif
			}
			break;
		}
	}

}
#endif

#ifdef CONFIG_MEMP_STATS_LOCAL

struct stats_ lwip_stats;

void
stats_init(void)
{
}

#endif

/**
 * Initialize custom memory pool.
 * Related functions: memp_malloc_pool, memp_free_pool
 *
 * @param desc pool to initialize
 */
void memp_init_pool(const struct memp_desc *desc)
{
#if MEMP_MEM_MALLOC
	LWIP_UNUSED_ARG(desc);
#else
	int i, objsize;
	struct memp *memp;

	/* Ignore the dummy */
	if (desc == &memp_dummy)
		return;

	*desc->tab = NULL;
	memp = (struct memp *)LWIP_MEM_ALIGN(desc->base);

	objsize = MEMP_SIZE + desc->size;
#if MEMP_OVERFLOW_CHECK
	objsize += MEM_SANITY_REGION_AFTER_ALIGNED;
#endif

#if MEMP_MEM_INIT
	/* force memset on pool memory */
	memset(memp, 0, (size_t)desc->num * objsize);
#endif
	/* create a linked list of memp elements */
	for (i = 0; i < desc->num; ++i) {
#if MEMP_OWNER_CHECK
		memp->func = NULL;
		memp->line = 0;
#endif
		memp->next = *desc->tab;
		*desc->tab = memp;
#if MEMP_OVERFLOW_CHECK
		memp_overflow_init_element(memp, desc);
#endif /* MEMP_OVERFLOW_CHECK */
		/* cast through void* to get rid of alignment warnings */
		memp = (struct memp *)(void *)((u8_t *)memp + objsize);
	}

	desc->stats->avail = desc->num;

#endif /* !MEMP_MEM_MALLOC */

#if MEMP_STATS && (defined(LWIP_DEBUG) || LWIP_STATS_DISPLAY)
	desc->stats->name  = desc->desc;
#endif /* MEMP_STATS && (defined(LWIP_DEBUG) || LWIP_STATS_DISPLAY) */
}

void memp_init_pool_region(const struct memp_desc *desc, uint16_t reg_num)
{
#if MEMP_MEM_MALLOC
	LWIP_UNUSED_ARG(desc);
#else
	int i, objsize;
	struct memp *memp;

	/* Ignore the dummy. */
	if (desc == &memp_dummy)
		return;

	*desc->tab = NULL;
	memp = (struct memp *)LWIP_MEM_ALIGN(desc->base);

	objsize = MEMP_SIZE + desc->size;

#if MEMP_MEM_INIT
	/* force memset on pool memory */
	memset(memp, 0, (size_t)reg_num * objsize);
#endif
	/* create a linked list of memp elements */
	for (i = 0; i < reg_num; ++i) {
#if MEMP_OWNER_CHECK
		memp->func = NULL;
		memp->line = 0;
#endif
		memp->next = *desc->tab;
		*desc->tab = memp;
		/* cast through void* to get rid of alignment warnings */
		memp = (struct memp *)(void *)((u8_t *)memp + objsize);
	}
#endif /* !MEMP_MEM_MALLOC */
}

/**
 * Initializes lwIP built-in pools.
 * Related functions: memp_malloc, memp_free
 *
 * Carves out memp_memory into linked lists for each pool-type.
 */
void
memp_init(void)
{
	u16_t i;

	/* for every pool: */
	for (i = 0; i < LWIP_ARRAYSIZE(memp_pools); i++) {
		memp_init_pool(memp_pools[i]);
#if LWIP_STATS && MEMP_STATS
		lwip_stats.memp[i] = memp_pools[i]->stats;
#endif
	}

#if MEMP_OVERFLOW_CHECK >= 2
	/* check everything a first time to see if it worked */
	memp_overflow_check_all();
#endif /* MEMP_OVERFLOW_CHECK >= 2 */
}

#if !MEMP_OVERFLOW_CHECK && !MEMP_OWNER_CHECK
__ilm__ static void *do_memp_malloc_pool(const struct memp_desc *desc)
{
	struct memp *memp;
	SYS_ARCH_DECL_PROTECT(flags);

	/* Ignore the dummy. */
	if (desc == &memp_dummy)
		return NULL;
#if MEMP_MEM_MALLOC
	memp = (struct memp *) mem_malloc(MEMP_SIZE + MEMP_ALIGN_SIZE(desc->size));
	SYS_ARCH_PROTECT(flags);
#else
	/* Zero-size descriptor */
	if (desc->num == 0) {
		memp = (struct memp *) mem_malloc(MEMP_SIZE + MEMP_ALIGN_SIZE(desc->size));
		SYS_ARCH_PROTECT(flags);
	} else {
		SYS_ARCH_PROTECT(flags);
		if ((memp = *desc->tab) != NULL)
			*desc->tab = memp->next;
	}
#endif

	if (memp != NULL) {
		LWIP_ASSERT("memp_malloc: memp properly aligned",
			    ((mem_ptr_t)memp % MEM_ALIGNMENT) == 0);

		desc->stats->used++;
		if (desc->stats->used > desc->stats->max) {
			desc->stats->max = desc->stats->used;
		}

		/* cast through u8_t* to get rid of alignment warnings */
		SYS_ARCH_UNPROTECT(flags);
		return ((u8_t *)memp + MEMP_SIZE);
	}

#if MEMP_STATS
	desc->stats->err++;
#endif
	SYS_ARCH_UNPROTECT(flags);
	LWIP_DEBUGF(MEMP_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
		    ("memp_malloc: out of memory in pool %s\n", desc->desc));
	return NULL;
}
#else
static void *do_memp_malloc_pool_fn(const struct memp_desc *desc,
				    const char *func, const int line)

{
	struct memp *memp;
	SYS_ARCH_DECL_PROTECT(flags);

	/* Ignore the dummy. */
	if (desc == &memp_dummy)
		return NULL;
#if MEMP_MEM_MALLOC
	memp = (struct memp *) mem_malloc(MEMP_SIZE + MEMP_ALIGN_SIZE(desc->size));
	SYS_ARCH_PROTECT(flags);
#else
	/* Zero-size descriptor */
	if (desc->num == 0) {
		memp = (struct memp *) mem_malloc(MEMP_SIZE + MEMP_ALIGN_SIZE(desc->size));
		SYS_ARCH_PROTECT(flags);
	} else {
		SYS_ARCH_PROTECT(flags);
		if ((memp = *desc->tab) != NULL)
			*desc->tab = memp->next;
	}
#endif

	if (memp != NULL) {
		memp->func = func;
		memp->line = line;
#if MEMP_MEM_MALLOC
		memp_overflow_init_element(memp, desc);
#else
#if MEMP_OVERFLOW_CHECK == 1
		memp_overflow_check_element_overflow(memp, desc);
		memp_overflow_check_element_underflow(memp, desc);
#endif /* MEMP_OVERFLOW_CHECK */
		memp->next = NULL;
#endif /* MEMP_MEM_MALLOC */

		LWIP_ASSERT("memp_malloc: memp properly aligned",
			    ((mem_ptr_t)memp % MEM_ALIGNMENT) == 0);

		desc->stats->used++;
		if (desc->stats->used > desc->stats->max) {
			desc->stats->max = desc->stats->used;
		}

		SYS_ARCH_UNPROTECT(flags);
		/* cast through u8_t* to get rid of alignment warnings */
		return ((u8_t *)memp + MEMP_SIZE);
	}

	desc->stats->err++;

	SYS_ARCH_UNPROTECT(flags);
	LWIP_DEBUGF(MEMP_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
		    ("memp_malloc: out of memory in pool %s\n", desc->desc));
	return NULL;
}
#endif

/**
 * Get an element from a custom pool.
 *
 * @param desc the pool to get an element from
 *
 * @return a pointer to the allocated memory or a NULL pointer on error
 */
__ilm__ void *
#if !MEMP_OVERFLOW_CHECK && !MEMP_OWNER_CHECK
memp_malloc_pool(const struct memp_desc *desc)
#else
memp_malloc_pool_fn(const struct memp_desc *desc, const char *func, const int line)
#endif
{
	LWIP_ASSERT("invalid pool desc", desc != NULL);
	if (desc == NULL) {
		return NULL;
	}

#if !MEMP_OVERFLOW_CHECK && !MEMP_OWNER_CHECK
	return do_memp_malloc_pool(desc);
#else
	return do_memp_malloc_pool_fn(desc, func, line);
#endif
}

/**
 * Get an element from a specific pool.
 *
 * @param type the pool to get an element from
 *
 * @return a pointer to the allocated memory or a NULL pointer on error
 */
#if !MEMP_OVERFLOW_CHECK && !MEMP_OWNER_CHECK

#if MEMP_OVERFLOW_CHECK || MEMP_OWNER_CHECK
#ifdef CONFIG_LINK_TO_ROM
#error "MEMP checks are not allowed."
#endif
#endif

static void *_memp_malloc(memp_t type);

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(memp_malloc, &memp_malloc, &_memp_malloc);
#else
__func_tab__  void *
(*memp_malloc)(memp_t type) = _memp_malloc;
#endif
__ilm__ static void *_memp_malloc(memp_t type)
{
	void *memp;
	LWIP_ERROR("memp_malloc: type < MEMP_MAX", (type < MEMP_MAX), return NULL;);

#if MEMP_OVERFLOW_CHECK >= 2
	memp_overflow_check_all();
#endif /* MEMP_OVERFLOW_CHECK >= 2 */

	memp = do_memp_malloc_pool(memp_pools[type]);
	return memp;
}
#else
void *memp_malloc_fn(memp_t type, const char *func, const int line)
{
	void *memp;
	LWIP_ERROR("memp_malloc: type < MEMP_MAX", (type < MEMP_MAX), return NULL;);

#if MEMP_OVERFLOW_CHECK >= 2
	memp_overflow_check_all();
#endif /* MEMP_OVERFLOW_CHECK >= 2 */

	memp = do_memp_malloc_pool_fn(memp_pools[type], func, line);
	return memp;
}
#endif

__ilm__ static void
do_memp_free_pool(const struct memp_desc *desc, void *mem)
{
	struct memp *memp;
	SYS_ARCH_DECL_PROTECT(old_level);

	/* Ignore the dummy. */
	if (desc == &memp_dummy)
		return;

	LWIP_ASSERT("memp_free: mem properly aligned",
		    ((mem_ptr_t)mem % MEM_ALIGNMENT) == 0);

	/* cast through void* to get rid of alignment warnings */
	memp = (struct memp *)(void *)((u8_t *)mem - MEMP_SIZE);

	SYS_ARCH_PROTECT(old_level);

#if MEMP_OVERFLOW_CHECK == 1
	memp_overflow_check_element_overflow(memp, desc);
	memp_overflow_check_element_underflow(memp, desc);
#endif /* MEMP_OVERFLOW_CHECK */

#if MEMP_OWNER_CHECK
  memp->func = NULL;
  memp->line = 0;
#endif

	desc->stats->used--;

#if MEMP_MEM_MALLOC
	LWIP_UNUSED_ARG(desc);
	SYS_ARCH_UNPROTECT(old_level);
	mem_free(memp);
#else /* MEMP_MEM_MALLOC */
	if (desc->num == 0) {
		SYS_ARCH_UNPROTECT(old_level);
		mem_free(memp);
#if MEMP_SANITY_CHECK
		LWIP_ASSERT("memp sanity", memp_sanity(desc));
#endif /* MEMP_SANITY_CHECK */
		return;
	}
	memp->next = *desc->tab;
	*desc->tab = memp;

#if MEMP_SANITY_CHECK
	LWIP_ASSERT("memp sanity", memp_sanity(desc));
#endif /* MEMP_SANITY_CHECK */

	SYS_ARCH_UNPROTECT(old_level);
#endif /* !MEMP_MEM_MALLOC */
}

/**
 * Put a custom pool element back into its pool.
 *
 * @param desc the pool where to put mem
 * @param mem the memp element to free
 */
__ilm__ void
memp_free_pool(const struct memp_desc *desc, void *mem)
{
	LWIP_ASSERT("invalid pool desc", desc != NULL);
	if ((desc == NULL) || (mem == NULL)) {
		return;
	}

	do_memp_free_pool(desc, mem);
}


/**
 * Put an element back into its pool.
 *
 * @param type the pool where to put mem
 * @param mem the memp element to free
 */

static void _memp_free(memp_t type, void *mem);
#ifdef CONFIG_LINK_TO_ROM
PROVIDE(memp_free, &memp_free, &_memp_free);
#else
__func_tab__  void
(*memp_free)(memp_t type, void *mem) = _memp_free;
#endif /* CONFIG_LINK_TO_ROM */

__ilm__ static void _memp_free(memp_t type, void *mem)
{
#ifdef LWIP_HOOK_MEMP_AVAILABLE
	struct memp *old_first;
#endif

	LWIP_ERROR("memp_free: type < MEMP_MAX", (type < MEMP_MAX), return;);

	if (mem == NULL) {
		return;
	}

#if MEMP_OVERFLOW_CHECK >= 2
	memp_overflow_check_all();
#endif /* MEMP_OVERFLOW_CHECK >= 2 */

#ifdef LWIP_HOOK_MEMP_AVAILABLE
	old_first = *memp_pools[type]->tab;
#endif

	do_memp_free_pool(memp_pools[type], mem);

#ifdef LWIP_HOOK_MEMP_AVAILABLE
	if (old_first == NULL) {
		LWIP_HOOK_MEMP_AVAILABLE(type);
	}
#endif
}

/**
 * Get the number of available elements in a pool.
 *
 * @param type the pool where from get available number
 */
mem_size_t memp_available(memp_t type)
{

	const struct memp_desc *desc = memp_pools[type];
	if (desc != &memp_dummy && (desc->stats->avail > desc->stats->used))
		return (desc->stats->avail - desc->stats->used);
	else
		return 0;
}

#if defined(__WISE__)

#if MEMP_MEM_MALLOC
#error "memp_num and memp_size not available."
#endif

/**
 * Resets a lwIP built-in pool to its original state.
 * Related functions: memp_malloc, memp_free
 *
 * Carves out memp_memory into linked lists for each pool-type.
 */
void
memp_reset(memp_t type)
{
	const struct memp_desc *desc;

	assert(type < MEMP_MAX);

	desc = memp_pools[type];

	memp_init_pool(desc);
}

/**
 * Resets Special region of a lwIP built-in pool to its original state.
 * Related functions: memp_malloc, memp_free
 *
 * Carves out memp_memory into linked lists for each pool-type.
 */
void
memp_reset_region(memp_t type, uint16_t reg_num)
{
	const struct memp_desc *desc;

	assert(type < MEMP_MAX);

	desc = memp_pools[type];

	memp_init_pool_region(desc, reg_num);
}

/**
 * Get the total number of elements in a pool.
 *
 * @desc desc of the pool where from get total number
 */

u16_t memp_num(memp_t type)
{
	const struct memp_desc *desc;

	assert(type < MEMP_MAX);

	desc = memp_pools[type];

	if (desc == &memp_dummy)
		return 0;

	return desc->num;
}

/**
 * Get the size of an element in a pool.
 *
 * @desc desc of the pool where from get element size
 */

u16_t memp_size(memp_t type)
{
	const struct memp_desc *desc;

	assert(type < MEMP_MAX);

	desc = memp_pools[type];

	if (desc == &memp_dummy)
		return 0;

	return desc->size;
}

int memp_is_dummy(memp_t type)
{
	const struct memp_desc *desc;

	assert(type < MEMP_MAX);

	desc = memp_pools[type];

	if (desc == &memp_dummy)
		return 1;

	return 0;
}

#endif

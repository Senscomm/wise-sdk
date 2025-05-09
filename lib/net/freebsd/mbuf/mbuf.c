/*
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Copyright 2004, Marcus Overhagen. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */


#include <stdint.h>
#include <string.h>
#include <hal/kernel.h>
#include <hal/console.h>

#include "mbuf.h"
#include "kernel.h"

#include "lwip/pbuf.h"
#include "lwip/memp.h"
#include "lwip/stats.h"

#include "hal/types.h"

int MSIZE 			= __MSIZE__;
int MLEN			= __MLEN__;
int MHLEN			= __MHLEN__;
int MINCLSIZE		= __MINCLSIZE__;
int MCLBYTES		= __MCLBYTES__;
int MLEN2			= __MLEN2__;
int MHLEN2			= __MHLEN2__;
int max_linkhdr		= 16;
int max_protohdr	= (40 + 20);
int max_hdr			= (16 + 40 + 20); /* max_linkhdr + max_protohdr */
int max_datalen		= (__MHLEN__ - (16 + 40 + 20));

#ifndef CONFIG_LINK_TO_ROM

int
construct_mbuf(struct mbuf *memoryBuffer, short type, int flags)
{
	memoryBuffer->m_next = NULL;
	memoryBuffer->m_nextpkt = NULL;
	memoryBuffer->m_len = 0;
	memoryBuffer->m_flags = flags;
	memoryBuffer->m_type = type;

	if (flags & M_PKTHDR) {
		memoryBuffer->m_data = memoryBuffer->m_pktdat;
		memset(&memoryBuffer->m_pkthdr, 0, sizeof(memoryBuffer->m_pkthdr));
		SLIST_INIT(&memoryBuffer->m_pkthdr.tags);
	} else
		memoryBuffer->m_data = memoryBuffer->m_dat;

	if (flags & M_FIXED)
		memoryBuffer->m_data = memoryBuffer->m_ext.ext_buf;

	return 0;
}

int
construct_ext_cluster_mbuf(struct mbuf *memoryBuffer, int how, int size)
{
	struct mbuf2 *m = (struct mbuf2 *)memoryBuffer;
	(void)how;

	if (size != MHLEN2) {
		printk("unsupported size");
		return B_BAD_VALUE;
	}

	m->m_ext.ext_buf = m->m_pktdat;
	m->m_data = m->m_ext.ext_buf;
	m->m_flags |= M_EXT;
	m->m_ext.ext_size = size;
	m->m_ext.ext_type = EXT_CLUSTER;

	return 0;
}

int
construct_ext_mbuf(struct mbuf *memoryBuffer, int how)
{
	if (memoryBuffer->m_flags & M_FIXED)
		return 0;

	return construct_ext_cluster_mbuf(memoryBuffer, how, MHLEN2);
}

int
construct_pkt_mbuf(int how, struct mbuf *memoryBuffer, short type, int flags)
{
	construct_mbuf(memoryBuffer, type, flags);
	if (construct_ext_mbuf(memoryBuffer, how) < 0)
		return -1;
	return 0;
}

/*
 * Same as m_dup() except this will only use MBUF cache.
 */

static struct mbuf *
_m_dup2(struct mbuf *m0, int how);

__romfunc__ struct mbuf *
(*m_dup2)(struct mbuf *m0, int how) = _m_dup2;

static struct mbuf *
_m_dup2(struct mbuf *m0, int how)
{
	const struct mbuf *n;
	struct mbuf *m;
	int hdrm = M_LEADINGSPACE(m0);
	int off;
	int len = m_length(m0, NULL);

	m = m_gethdr(how, MT_DATA);
	if (m) {
		m_dup_pkthdr(m, m0, how);
		m->m_data += hdrm;
		m->m_len = m0->m_len;
		for (n = m0, off = 0; n != NULL; n = n->m_next) {
			hdrm = M_LEADINGSPACE(n);
			m_copyback(m, off, n->m_len, n->m_data, hdrm);
			off += n->m_len;
		}
		m_fixhdr(m);
	}

	if (len != m_length(m, NULL)) {
		m_freem(m);
		return NULL;
	}

	return m;
}

static struct mbuf *
_m_frompbuf(struct pbuf *p0, int mhdrm);

__romfunc__ struct mbuf *
(*m_frompbuf)(struct pbuf *p0, int mhdrm) = _m_frompbuf;

static struct mbuf *
_m_frompbuf(struct pbuf *p0, int mhdrm)
{
	const struct pbuf *p;
	struct mbuf *m;
	int phdrm;
	int off;
	int len = p0->tot_len;

	phdrm = (size_t)(p0->payload - (void *)p0)
			- sizeof(struct pbuf);
	phdrm = LWIP_MEM_ALIGN_SIZE(phdrm);
	if (!(phdrm < MHLEN))
		return NULL;

	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m) {
		m->m_data += phdrm;
		m->m_len = MHLEN - phdrm;
		for (p = p0, off = 0; p != NULL; p = p->next) {
			m_copyback(m, off, p->len, p->payload, mhdrm);
			off += p->len;
		}
		if (!m->m_next)
			m->m_len = m->m_pkthdr.len;
		m_fixhdr(m);

		if (len != m_length(m, NULL)) {
			m_freem(m);
			return NULL;
		}
	}
	return m;
}

static struct pbuf *
_m_topbuf(struct mbuf *mb);

__romfunc__ struct pbuf *
(*m_topbuf)(struct mbuf *mb) = _m_topbuf;

static struct pbuf *
_m_topbuf(struct mbuf *mb)
{
	struct pbuf *p, *q;
	int len, totlen, offset;

  	/*
	 * Obtain the size of the packet and put it into
	 * the "totlen" variable.
	*/
  	totlen = mb->m_pkthdr.len;

#if ETH_PAD_SIZE
  	totlen += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

	/* We allocate a pbuf chain of pbufs from the pool. */
	p = pbuf_alloc(PBUF_RAW, totlen, PBUF_POOL);

#if 0
	KASSERT(p != NULL, ("m_topbuf, pbuf_alloc failed\n"));
#endif

	if (p != NULL) {
#if ETH_PAD_SIZE
    		pbuf_remove_header(p, ETH_PAD_SIZE); /* drop the padding word */
#endif
		/*
		 * We iterate over the pbuf chain until we have read the entire
		 * packet into the pbuf.
		 */
		offset = 0;
		for (q = p; q != NULL; q = q->next) {
			/*
			 * Read enough bytes to fill this pbuf in the chain.
			 * The available space in the pbuf is given
			 * by the q->len variable.
			 */
			len = min(totlen, q->len);
			m_copydata(mb, offset, len, q->payload);
			offset += len;
			totlen -= len;
		}

		KASSERT(totlen == 0, ("m_tobuf, not all data copied %d", totlen));
	}
	m_freem(mb);

	return p;
}

static int
_m_fragnum(struct mbuf *m);

__romfunc__ int
(*m_fragnum)(struct mbuf *m) = _m_fragnum;

static int
_m_fragnum(struct mbuf *m)
{
	int num;
	struct mbuf *f;

	for (num = 0, f = m; f; f = f->m_next)
		num++;

	return num;
}


static struct mbuf *
_m_get(int how, short type);

__romfunc__ struct mbuf *
(*m_get)(int how, short type) = _m_get;

static struct mbuf *
_m_get(int how, short type)
{
	return __m_get(how, type, 0);
}


static struct mbuf *
_m_gethdr(int how, short type);

__romfunc__ struct mbuf *
(*m_gethdr)(int how, short type) = _m_gethdr;

static struct mbuf *
_m_gethdr(int how, short type)
{
	return __m_get(how, type, M_PKTHDR);
}

static struct mbuf *
_m_getext(int how, short type);

__romfunc__ struct mbuf *
(*m_getext)(int how, short type) = _m_getext;

static struct mbuf *
_m_getext(int how, short type)
{
	return __m_get(how, type, M_FIXED | M_EXT);
}

static struct mbuf *
_m_getexthdr(int how, short type);

__romfunc__ struct mbuf *
(*m_getexthdr)(int how, short type) = _m_getexthdr;

static struct mbuf *
_m_getexthdr(int how, short type)
{
	return __m_get(how, type, M_PKTHDR | M_FIXED | M_EXT);
}

static int
_m_clget(struct mbuf *memoryBuffer, int how);

__romfunc__ int
(*m_clget)(struct mbuf *memoryBuffer, int how) = _m_clget;

static int
_m_clget(struct mbuf *memoryBuffer, int how)
{
	if (!(memoryBuffer->m_flags & M_FIXED))
		memoryBuffer->m_ext.ext_buf = NULL;
	/* called checks for errors by looking for M_EXT */
	construct_ext_mbuf(memoryBuffer, how);
	return memoryBuffer->m_flags & M_EXT;
}

static void
_m_freem(struct mbuf *memoryBuffer);

__romfunc__ void
(*m_freem)(struct mbuf *memoryBuffer) = _m_freem;

static void
_m_freem(struct mbuf *memoryBuffer)
{
	while (memoryBuffer)
		memoryBuffer = m_free(memoryBuffer);
}

/*
 * Configure a provided mbuf to refer to the provided external storage buffer.
 *
 * Arguments:
 *    memoryBuffer  The existing mbuf to which to attach the provided buffer.
 *    buffer The address of the provided external storage buffer.
 *    size   The size of the provided buffer.
 *    freef  A pointer to a routine that is responsible for freeing the
 *           provided external storage buffer.
 *    args   A pointer to an argument structure (of any type) to be passed
 *           to the provided freef routine (may be NULL).
 *    flags  Any other flags to be passed to the provided mbuf.
 *    type   The type that the external storage buffer should be
 *           labeled with.
 *
 * Returns:
 *    Nothing.
 */

static void
_m_dyna_extadd(struct mbuf *memoryBuffer, caddr_t buffer, u_int size,
	m_ext_free_t freef, uint32_t arg1, uint32_t arg2, int flags, int type);

__romfunc__ void
(*m_dyna_extadd)(struct mbuf *memoryBuffer, caddr_t buffer, u_int size,
	m_ext_free_t freef, uint32_t arg1, uint32_t arg2, int flags,
	int type) = _m_dyna_extadd;

static void
_m_dyna_extadd(struct mbuf *memoryBuffer, caddr_t buffer, u_int size,
	m_ext_free_t freef, uint32_t arg1, uint32_t arg2, int flags, int type)
{
	struct mbuf3 *mbuf = (struct mbuf3 *)memoryBuffer;

	mbuf->m_flags |= (M_EXT | flags);
	mbuf->m_ext.ext_buf = buffer;
	mbuf->m_data = mbuf->m_ext.ext_buf;
	mbuf->m_ext.ext_size = size;
	mbuf->m_ext.ext_free = freef;
	mbuf->m_ext.ext_arg1 = arg1;
	mbuf->m_ext.ext_arg2 = arg2;
	mbuf->m_ext.ext_type = type;
	mbuf->m_len = size;
}

#endif /* CONFIG_LINK_TO_ROM */

extern int
construct_mbuf(struct mbuf *memoryBuffer, short type, int flags);
extern int
construct_ext_cluster_mbuf(struct mbuf *memoryBuffer, int how, int size);
extern int
construct_ext_mbuf(struct mbuf *memoryBuffer, int how);
extern int
construct_pkt_mbuf(int how, struct mbuf *memoryBuffer, short type, int flags);

static struct mbuf *
_m_getcl(int how, short type, int flags);

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(m_getcl, &m_getcl, &_m_getcl);
#else
__func_tab__ struct mbuf *
(*m_getcl)(int how, short type, int flags) = _m_getcl;
#endif

__ilm__ static struct mbuf *
_m_getcl(int how, short type, int flags)
{
	struct mbuf *memoryBuffer;

	if (flags & M_HEAP)
		memoryBuffer = (struct mbuf *)malloc(sizeof(struct mbuf2));
	else
		memoryBuffer = (struct mbuf *)memp_malloc(MEMP_MBUF_CHUNK);

	if (memoryBuffer == NULL)
		return NULL;

	if (construct_pkt_mbuf(how, memoryBuffer, type, flags) < 0) {
		if (flags & M_HEAP)
			free(memoryBuffer);
		else
			memp_free(MEMP_MBUF_CHUNK, memoryBuffer);
		return NULL;
	}

	return memoryBuffer;
}

static struct mbuf *
___m_get(int how, short type, int flags);

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(__m_get, &__m_get, &___m_get);
#else
__func_tab__ struct mbuf *
(*__m_get)(int how, short type, int flags) = ___m_get;
#endif

__ilm__ static struct mbuf *
___m_get(int how, short type, int flags)
{
	struct mbuf *memoryBuffer =
		(flags & M_EXT ? (struct mbuf *)memp_malloc(MEMP_MBUF_EXT_NODE)
		 : (struct mbuf *)memp_malloc(MEMP_MBUF_CACHE));
	if (memoryBuffer == NULL)
		return NULL;

	construct_mbuf(memoryBuffer, type, flags);

	return memoryBuffer;
}

static int
_m_free_ext(struct mbuf *memoryBuffer);

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(m_free_ext, &m_free_ext, &_m_free_ext);
#else
__func_tab__ int
(*m_free_ext)(struct mbuf *memoryBuffer) = _m_free_ext;
#endif

__ilm__ static int
_m_free_ext(struct mbuf *memoryBuffer)
{
	/*
	if (m->m_ext.ref_count != NULL)
		printk("unsupported");
	*/

	switch (memoryBuffer->m_ext.ext_type) {
	case EXT_CLUSTER:
		memp_free(MEMP_MBUF_CHUNK, memoryBuffer);
		memoryBuffer->m_ext.ext_buf = NULL;
		break;
	case EXT_IPCRING:
		if (memoryBuffer->m_ext.ext_free)
			memoryBuffer->m_ext.ext_free(memoryBuffer);
		memoryBuffer->m_ext.ext_buf = NULL;
		memp_free(MEMP_MBUF_EXT_NODE, memoryBuffer);
		break;
	default:
		printk("unknown type(%d)\n", memoryBuffer->m_ext.ext_type);
		return -1;

	}
	return 0;
}

static struct mbuf *
_m_free(struct mbuf *memoryBuffer);

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(m_free, &m_free, &_m_free);
#else
__func_tab__ struct mbuf *
(*m_free)(struct mbuf *memoryBuffer) = _m_free;
#endif

__ilm__ static struct mbuf *
_m_free(struct mbuf *memoryBuffer)
{
	struct mbuf *next = memoryBuffer->m_next;

	if (memoryBuffer->m_flags & M_PKTHDR)
		m_tag_delete_chain(memoryBuffer, NULL);
	if (memoryBuffer->m_flags & M_HEAP)
		free(memoryBuffer);
	else if (memoryBuffer->m_flags & M_EXT) {
		if (m_free_ext(memoryBuffer) < 0)
			return NULL;
	} else if (!(memoryBuffer->m_flags & M_PROTO11))
		memp_free(MEMP_MBUF_CACHE, memoryBuffer);

	return next;
}

#if __WISE__
#ifdef CONFIG_ARM
#include <cache.h>
/* Cache management in case mbuf pool is placed in cacheable memory */
#define LWIP_CACHE_ALIGN(addr) (void *)((mem_ptr_t)(addr) & ~(mem_ptr_t)(DCACHE_SIZE-1))
void
_m_cache_op(void *ptr, size_t size, CACHE_OP op)
{
	void *start = ptr, *end;
	u32 flags;

	local_irq_save(flags);
	if (!size) {
		struct mbuf *memoryBuffer = (struct mbuf *)ptr;
		if (memoryBuffer->m_flags & M_EXT)
			end = start + MCLBYTES;
		else
			end = start + MSIZE;
	} else
		end = start + size;

	start = LWIP_CACHE_ALIGN(ptr);
	while (start < end) {
		switch (op) {
		case CACHE_OP_INVALIDATE:
			armv7a_dcache_invalidate_va(start);
			break;
		case CACHE_OP_CLEAN:
			armv7a_dcache_clean_va(start);
			break;
		case CACHE_OP_CLEAN_INVALIDATE:
			armv7a_dcache_clean_invalidate_va(start);
			break;
		}
		start += DCACHE_SIZE;
	}
	local_irq_restore(flags);
}
#else
void
_m_cache_op(void *ptr, size_t size, CACHE_OP op)
{
}
#endif
#endif

struct pbuf *
m_topbuf_nofreem(struct mbuf *mb)
{
	struct pbuf *p, *q;
	int len, totlen, offset;

  	/*
	 * Obtain the size of the packet and put it into
	 * the "totlen" variable.
	*/
  	totlen = mb->m_pkthdr.len;

#if ETH_PAD_SIZE
  	totlen += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

	/* We allocate a pbuf chain of pbufs from the pool. */
	p = pbuf_alloc(PBUF_RAW, totlen, PBUF_POOL);

#if 0
	KASSERT(p != NULL, ("m_topbuf, pbuf_alloc failed\n"));
#endif

	if (p != NULL) {
#if ETH_PAD_SIZE
    		pbuf_remove_header(p, ETH_PAD_SIZE); /* drop the padding word */
#endif
		/*
		 * We iterate over the pbuf chain until we have read the entire
		 * packet into the pbuf.
		 */
		offset = 0;
		for (q = p; q != NULL; q = q->next) {
			/*
			 * Read enough bytes to fill this pbuf in the chain.
			 * The available space in the pbuf is given
			 * by the q->len variable.
			 */
			len = min(totlen, q->len);
			m_copydata(mb, offset, len, q->payload);
			offset += len;
			totlen -= len;
		}

		KASSERT(totlen == 0, ("m_tobuf, not all data copied %d", totlen));
	}

	return p;
}

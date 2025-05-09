/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_MBUF_H_
#define _FBSD_COMPAT_SYS_MBUF_H_


#include <sys/queue.h>
#include "compat_types.h"
#include "systm.h"
#include "compat_param.h"

#define __MLEN__		((int)(__MSIZE__ - sizeof(struct m_hdr)))
#define __MHLEN__		((int)(__MLEN__ - sizeof(struct pkthdr)))
#define __MINCLSIZE__	(__MHLEN__ + 1)

#define MBTOM(how)	(how)

#define M_NOWAIT	0x0001
#define M_WAITOK	0x0002
#define M_FROMHEAP	0x0004
#define M_ZERO		0x0100

#define M_DONTWAIT	M_NOWAIT
#define M_TRYWAIT	M_WAITOK
#define M_WAIT		M_WAITOK

#define MT_DATA		1

#define M_EXT		0x00000001
#define M_PKTHDR	0x00000002
#define M_RDONLY	0x00000008
#define M_PROTO1	0x00000010
#define M_PROTO2	0x00000020
#define M_PROTO3	0x00000040
#define M_PROTO4	0x00000080
#define M_PROTO5	0x00000100
#define M_BCAST		0x00000200
#define M_MCAST		0x00000400
#define M_FRAG		0x00000800
#define M_FIRSTFRAG	0x00001000
#define M_LASTFRAG	0x00002000
#define M_VLANTAG	0x00010000
#define M_PROTO6	0x00080000
#define M_PROTO7	0x00100000
#define M_PROTO8	0x00200000
#define M_HEAP		0x00400000 /* not for DMA, but just for messaging */
#define M_FIXED		0x00800000 /* for fixed type of mbuf (M_FIXED) */
#define M_TEST		0x01000000
#define M_TEST_F	0x02000000
#define M_TEST_M	0x04000000
#define M_TEST_E	0x08000000
#define M_TEST_EXT	(M_TEST_F | M_TEST_M | M_TEST_E)
#define M_TEST_E	0x08000000
#define M_PROTO9	0x10000000
#define M_PROTO10	0x20000000
#define M_PROTO11	0x40000000

#define M_COPYFLAGS (M_PKTHDR | M_RDONLY | M_BCAST | M_MCAST | M_FRAG \
	| M_FIRSTFRAG | M_LASTFRAG | M_VLANTAG | M_PROTO1 | M_PROTO2 \
	| M_PROTO3 | M_PROTO4 | M_PROTO5 | M_PROTO6 | M_PROTO7 | M_PROTO8 | M_PROTO9 | M_PROTO10)
	// Flags preserved when copying m_pkthdr

#define M_MOVE_PKTHDR(to, from)	m_move_pkthdr((to), (from))
#define MGET(m, how, type)		((m) = m_get((how), (type)))
#define MGETHDR(m, how, type)	((m) = m_gethdr((how), (type)))
#define MCLGET(m, how)			m_clget((m), (how))

#define mtod(m, type)	((type)((m)->m_data))
#define	mtodo(m, o)	((void *)(((m)->m_data) + (o)))
#define mget_ifp(m)	((m)->m_pkthdr.rcvif)

// Check if the supplied mbuf has a packet header, or else panic.
#define M_ASSERTPKTHDR(m) KASSERT(m != NULL && m->m_flags & M_PKTHDR, \
	("%s: no mbuf packet header!", __func__))

#define MBUF_CHECKSLEEP(how) do { } while (0)

#define MTAG_PERSISTENT	0x800

#define EXT_CLUSTER		1		// 2048 bytes
#define EXT_FIXED		2		// custom ext_buf permanently attached by wlan driver
#define EXT_IPCRING		3		// data in IPC receive ring
#define EXT_NET_DRV		100		// custom ext_buf provided by net driver

#define CSUM_IP			0x0001
#define CSUM_TCP		0x0002
#define CSUM_UDP		0x0004
#define CSUM_TSO		0x0020
#define CSUM_IP_CHECKED		0x0100
#define CSUM_IP_VALID		0x0200
#define CSUM_DATA_VALID		0x0400
#define CSUM_PSEUDO_HDR		0x0800
#define CSUM_DELAY_DATA		(CSUM_TCP | CSUM_UDP)

extern int MSIZE;
extern int MLEN;
extern int MHLEN;
extern int MINCLSIZE;
extern int MCLBYTES;
extern int MLEN2;
extern int MHLEN2;
extern int max_linkhdr;
extern int max_protohdr;
extern int max_hdr;
extern int max_datalen;




struct m_hdr {
	struct mbuf*	mh_next;
	struct mbuf*	mh_nextpkt;
	caddr_t		mh_data;
	int		mh_len;
	int		mh_flags;
	short		mh_type;
};

struct pkthdr {
	struct ifnet*	rcvif;
	int		len;
	int		csum_flags;
	int		csum_data;
	uint16_t	tso_segsz;
	uint16_t	ether_vtag;
	SLIST_HEAD(packet_tags, m_tag)	tags;
};

struct m_tag {
	SLIST_ENTRY(m_tag)	m_tag_link;		// List of packet tags
	u_int16_t		m_tag_id;		// Tag ID
	u_int16_t		m_tag_len;		// Length of data
	u_int32_t		m_tag_cookie;	// ABI/Module ID
	void			(*m_tag_free)(struct m_tag*);
};

/*
 * Description of external storage mapped into mbuf; valid only if M_EXT is
 * set.
 */
typedef void m_ext_free_t(struct mbuf *);
struct ext {
	/*
	 * Regular M_EXT mbuf:
	 * o ext_buf always points to the external buffer.
	 * o ext_free (below) and two optional arguments
	 *   ext_arg1 and ext_arg2 store the free context for
	 *   the external storage.
	 */
	caddr_t		ext_buf;  /* start of buffer */
	unsigned int	ext_size; /* size of buffer */
	int		ext_type; /* type of external storage */
	/*
	 * Free method and optional argument pointer
	 */
	m_ext_free_t	*ext_free;
	uint32_t		ext_arg1;
	uint32_t		ext_arg2;

};

struct mbuf {
	struct m_hdr m_hdr;
	union {
		struct {
			struct pkthdr	MH_pkthdr;
			union {
				struct ext	MH_ext;
				char		MH_databuf[__MHLEN__];
			} MH_dat;
		} MH;
		char M_databuf[__MLEN__];
	} M_dat;
};

/* A new structure for cluster type (EXT_CLUSTER) mbuf */
#define __MLEN2__		((int)(__MCLBYTES__ - sizeof(struct m_hdr)))
#define __MHLEN2__		((int)((__MLEN2__ - sizeof(struct pkthdr)) - sizeof(struct ext)))

struct mbuf2 {
	struct m_hdr m_hdr;
	union {
		struct {
			struct pkthdr	MH_pkthdr;
			struct {
				struct ext	MH_ext;
				char		MH_databuf[__MHLEN2__];
			} MH_dat;
		} MH;
		char M_databuf[__MLEN2__];
	} M_dat;
};

/* A new structure for fixed ext type (EXT_FIXED) mbuf */
/* for dynamic ext type (such as EXT_NET_DRV) mbuf as well */
struct mbuf3 {
	struct m_hdr m_hdr;
	union {
		struct {
			struct pkthdr	MH_pkthdr;
			struct {
				struct ext	MH_ext;
				char		MH_databuf[0];
			} MH_dat;
		} MH;
		char M_databuf[0];
	} M_dat;
};

#define m_next		m_hdr.mh_next
#define m_len		m_hdr.mh_len
#define m_data		m_hdr.mh_data
#define m_type		m_hdr.mh_type
#define m_flags		m_hdr.mh_flags
#define m_nextpkt	m_hdr.mh_nextpkt
#define m_act		m_nextpkt
#define m_pkthdr	M_dat.MH.MH_pkthdr
#define m_ext		M_dat.MH.MH_dat.MH_ext
#define m_pktdat	M_dat.MH.MH_dat.MH_databuf
#define m_dat		M_dat.M_databuf

extern struct mbuf*	(*m_get)(int, short);
extern struct mbuf *(*__m_get)(int how, short type, int flags);
extern struct mbuf*	(*m_gethdr)(int, short);
extern struct mbuf*	(*m_getcl)(int, short, int);
extern int			(*m_clget)(struct mbuf*, int);
extern void			(*m_dyna_extadd)(struct mbuf *memoryBuffer, caddr_t buffer, u_int size,
						m_ext_free_t freef, uint32_t arg1, uint32_t arg2, int flags, int type);
extern struct mbuf*	(*m_getext)(int how, short type);
extern struct mbuf*	(*m_getexthdr)(int how, short type);
extern struct mbuf*	(*m_free)(struct mbuf*);
extern void 		(*m_freem)(struct mbuf *memoryBuffer);
extern int 			(*m_free_ext)(struct mbuf *memoryBuffer);
extern struct mbuf *(*m_dup2)(struct mbuf *m0, int how);
extern int			(*m_fragnum)(struct mbuf *m);

void				m_catpkt(struct mbuf *m, struct mbuf *n);
void				m_adj(struct mbuf*, int);
void				m_align(struct mbuf*, int);
int					m_append(struct mbuf*, int, c_caddr_t);
void				m_cat(struct mbuf*, struct mbuf*);

struct mbuf*		m_collapse(struct mbuf*, int, int);
void				m_copyback(struct mbuf*, int, int, caddr_t, int);

void 		m_copydata(const struct mbuf *m, int off, int len, caddr_t cp);

struct mbuf*		m_copypacket(struct mbuf*, int);
struct mbuf*		m_defrag(struct mbuf*, int);
struct mbuf*		m_devget(char*, int, int, struct ifnet*,
						void(*) (char*, caddr_t, u_int));

struct mbuf*		m_dup(struct mbuf*, int);


int					m_dup_pkthdr(struct mbuf*, struct mbuf*, int);
void				m_demote_pkthdr(struct mbuf *m);
void				m_demote(struct mbuf *m0, int all, int flags);
u_int				m_fixhdr(struct mbuf*);

u_int				m_length(struct mbuf*, struct mbuf**);
void				m_move_pkthdr(struct mbuf*, struct mbuf*);
struct mbuf*		m_prepend(struct mbuf*, int, int);

struct mbuf*		m_pulldown(struct mbuf*, int, int, int*);

struct mbuf*		m_pullup(struct mbuf*, int);
struct mbuf*		m_split(struct mbuf*, int, int);
struct mbuf*		m_unshare(struct mbuf*, int);


#ifdef __WISE__
typedef enum {
	CACHE_OP_INVALIDATE,
	CACHE_OP_CLEAN,
	CACHE_OP_CLEAN_INVALIDATE,
	CACHE_OP_FLUSH = CACHE_OP_CLEAN_INVALIDATE,
} CACHE_OP;

#ifdef CONFIG_ARM
#define	m_cache_op(ptr, size, op) _m_cache_op(ptr, size, op);
#else
#define	m_cache_op(ptr, size, op)
#endif
void 				_m_cache_op(void *ptr, size_t size, CACHE_OP op);

#endif

struct m_tag*		m_tag_alloc(u_int32_t, int, int, int);

void				m_tag_delete(struct mbuf*, struct m_tag*);
void				m_tag_delete_chain(struct mbuf*, struct m_tag*);
void				m_tag_free_default(struct m_tag*);

struct m_tag*	m_tag_locate(struct mbuf*, u_int32_t, int, struct m_tag*);

extern struct pbuf*	(*m_topbuf)(struct mbuf *);
struct pbuf* m_topbuf_nofreem(struct mbuf *mb);

struct m_tag*		m_tag_copy(struct m_tag*, int);
int					m_tag_copy_chain(struct mbuf*, struct mbuf*, int);
void				m_tag_delete_nonpersistent(struct mbuf*);

__ilm__ static inline void
m_tag_setup(struct m_tag* tagPointer, u_int32_t cookie, int type, int length)
{
	tagPointer->m_tag_id = type;
	tagPointer->m_tag_len = length;
	tagPointer->m_tag_cookie = cookie;
}


__ilm__ static inline void
m_tag_free(struct m_tag* tag)
{
	(*tag->m_tag_free)(tag);
}


static inline void
m_tag_prepend(struct mbuf* memoryBuffer, struct m_tag* tag)
{
	SLIST_INSERT_HEAD(&memoryBuffer->m_pkthdr.tags, tag, m_tag_link);
}


__ilm__ static inline void
m_tag_unlink(struct mbuf* memoryBuffer, struct m_tag* tag)
{
	SLIST_REMOVE(&memoryBuffer->m_pkthdr.tags, tag, m_tag, m_tag_link);
}


#include "mbuf-fbsd.h"

#endif	/* _FBSD_COMPAT_SYS_MBUF_H_ */

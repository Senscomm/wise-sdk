#ifndef __WISE_SELECT_H__
#define __WISE_SELECT_H__

#ifndef FD_SET

#define FD_SETSIZE 	64

#define NFDBITS (sizeof(unsigned long) * 8) /* bits per mask */

typedef struct fd_set {
	unsigned long fds_bits[(FD_SETSIZE+NFDBITS-1)/NFDBITS];
} fd_set;

#define FD_SET(n, p)   ((p)->fds_bits[(n)/NFDBITS] |= (1L << ((n) % NFDBITS)))
#define FD_CLR(n, p)   ((p)->fds_bits[(n)/NFDBITS] &= ~(1L << ((n) % NFDBITS)))
#define FD_ISSET(n, p) ((p)->fds_bits[(n)/NFDBITS] & (1L << ((n) % NFDBITS)))
#define FD_ZERO(p)     memset((void *)p, 0, sizeof(*p))

#endif /* FD_SET */

#endif /* __WISE_SELECT_H__ */

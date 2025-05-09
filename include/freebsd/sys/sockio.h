/*
 * Copyright 2007-2009 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_SOCKIO_H_
#define _FBSD_COMPAT_SYS_SOCKIO_H_

#ifdef __USE_NATIVE_HEADER__

#include <sys/sockio.h>

#else

#include <sys/ioccom.h>

/* Socket ioctl's. */
#define	SIOCSHIWAT	 _IOW('s',  0, int)		/* set high watermark */
#define	SIOCGHIWAT	 _IOR('s',  1, int)		/* get high watermark */
#define	SIOCSLOWAT	 _IOW('s',  2, int)		/* set low watermark */
#define	SIOCGLOWAT	 _IOR('s',  3, int)		/* get low watermark */
#define	SIOCATMARK	 _IOR('s',  7, int)		/* at oob mark? */
#define	SIOCSPGRP	 _IOW('s',  8, int)		/* set process group */
#define	SIOCGPGRP	 _IOR('s',  9, int)		/* get process group */

/*	SIOCADDRT	 _IOW('r', 10, struct ortentry)	4.3BSD */
/*	SIOCDELRT	 _IOW('r', 11, struct ortentry)	4.3BSD */
/*	SIOCGETVIFCNT	_IOWR('r', 15, struct sioc_vif_req) get vif pkt cnt */
/*	SIOCGETSGCNT	_IOWR('r', 16, struct sioc_sg_req) get s,g pkt cnt */

#define	SIOCSIFADDR	 _IOW('i', 12, struct ifreq)	/* set ifnet address */
/*	OSIOCGIFADDR	_IOWR('i', 13, struct ifreq)	4.3BSD */
#define	SIOCGIFADDR	_IOWR('i', 33, struct ifreq)	/* get ifnet address */
#define	SIOCSIFDSTADDR	 _IOW('i', 14, struct ifreq)	/* set p-p address */
/*	OSIOCGIFDSTADDR	_IOWR('i', 15, struct ifreq)	4.3BSD */
#define	SIOCGIFDSTADDR	_IOWR('i', 34, struct ifreq)	/* get p-p address */
#define	SIOCSIFFLAGS	 _IOW('i', 16, struct ifreq)	/* set ifnet flags */
#define	SIOCGIFFLAGS	_IOWR('i', 17, struct ifreq)	/* get ifnet flags */
/*	OSIOCGIFBRDADDR	_IOWR('i', 18, struct ifreq)	4.3BSD */
#define	SIOCGIFBRDADDR	_IOWR('i', 35, struct ifreq)	/* get broadcast addr */
#define	SIOCSIFBRDADDR	 _IOW('i', 19, struct ifreq)	/* set broadcast addr */
#ifndef __WISE__
/*	OSIOCGIFCONF	_IOWR('i', 20, struct ifconf)	4.3BSD */
/*	SIOCGIFCONF	_IOWR('i', 36, struct ifconf)	get ifnet list */
#else
#define	SIOCGIFGWADDR	_IOWR('i', 20, struct ifreq)	/* get gateway addr */
#define	SIOCSIFGWADDR	 _IOW('i', 36, struct ifreq)	/* set gateway addr */
#endif
/*	OSIOCGIFNETMASK	_IOWR('i', 21, struct ifreq)	4.3BSD */
#define	SIOCGIFNETMASK	_IOWR('i', 37, struct ifreq)	/* get net addr mask */
#define	SIOCSIFNETMASK	 _IOW('i', 22, struct ifreq)	/* set net addr mask */
#define	SIOCGIFMETRIC	_IOWR('i', 23, struct ifreq)	/* get IF metric */
#define	SIOCSIFMETRIC	 _IOW('i', 24, struct ifreq)	/* set IF metric */
#define	SIOCDIFADDR	 _IOW('i', 25, struct ifreq)	/* delete IF addr */
/*	OSIOCAIFADDR	 _IOW('i', 26, struct oifaliasreq) FreeBSD 9.x */
/*	SIOCALIFADDR	 _IOW('i', 27, struct if_laddrreq) KAME */
/*	SIOCGLIFADDR	_IOWR('i', 28, struct if_laddrreq) KAME */
/*	SIOCDLIFADDR	 _IOW('i', 29, struct if_laddrreq) KAME */
#define	SIOCSIFCAP	 _IOW('i', 30, struct ifreq)	/* set IF features */
#define	SIOCGIFCAP	_IOWR('i', 31, struct ifreq)	/* get IF features */
#define	SIOCGIFINDEX	_IOWR('i', 32, struct ifreq)	/* get IF index */
#define	SIOCGIFMAC	_IOWR('i', 38, struct ifreq)	/* get IF MAC label */
#define	SIOCSIFMAC	 _IOW('i', 39, struct ifreq)	/* set IF MAC label */
#define	SIOCSIFNAME	 _IOW('i', 40, struct ifreq)	/* set IF name */
#define	SIOCSIFDESCR	 _IOW('i', 41, struct ifreq)	/* set ifnet descr */
#define	SIOCGIFDESCR	_IOWR('i', 42, struct ifreq)	/* get ifnet descr */
/*	SIOCAIFADDR	 _IOW('i', 43, struct ifaliasreq)  add/chg IF alias */

#define	SIOCADDMULTI	 _IOW('i', 49, struct ifreq)	/* add m'cast addr */
#define	SIOCDELMULTI	 _IOW('i', 50, struct ifreq)	/* del m'cast addr */
#define	SIOCGIFMTU	_IOWR('i', 51, struct ifreq)	/* get IF mtu */
#define	SIOCSIFMTU	 _IOW('i', 52, struct ifreq)	/* set IF mtu */
#define	SIOCGIFPHYS	_IOWR('i', 53, struct ifreq)	/* get IF wire */
#define	SIOCSIFPHYS	 _IOW('i', 54, struct ifreq)	/* set IF wire */
#define	SIOCSIFMEDIA	_IOWR('i', 55, struct ifreq)	/* set net media */
#define	SIOCGIFMEDIA	_IOWR('i', 56, struct ifmediareq) /* get net media */

#define	SIOCSIFGENERIC	 _IOW('i', 57, struct ifreq)	/* generic IF set op */
#define	SIOCGIFGENERIC	_IOWR('i', 58, struct ifreq)	/* generic IF get op */

#define	SIOCGIFSTATUS	_IOWR('i', 59, struct ifreq) 	/* get IF status */
#define	SIOCSIFLLADDR	 _IOW('i', 60, struct ifreq)	/* set linklevel addr */
#define	SIOCGI2C	_IOWR('i', 61, struct ifreq)	/* get I2C data  */
#define	SIOCGIFHWADDR	_IOWR('i', 62, struct ifreq)	/* get hardware lladdr */
#define	SIOCSIFHWADDR	_IOWR('i', 63, struct ifreq)	/* set hardware lladdr */

/*	SIOCSIFPHYADDR	 _IOW('i', 70, struct ifaliasreq) set gif address */
#define	SIOCGIFPSRCADDR	_IOWR('i', 71, struct ifreq)	/* get gif psrc addr */
#define	SIOCGIFPDSTADDR	_IOWR('i', 72, struct ifreq)	/* get gif pdst addr */
#define	SIOCDIFPHYADDR	 _IOW('i', 73, struct ifreq)	/* delete gif addrs */
/*	SIOCSLIFPHYADDR	 _IOW('i', 74, struct if_laddrreq) KAME */
/*	SIOCGLIFPHYADDR	_IOWR('i', 75, struct if_laddrreq) KAME */

#define	SIOCGPRIVATE_0	_IOWR('i', 80, struct ifreq)	/* device private 0 */
#define	SIOCGPRIVATE_1	_IOWR('i', 81, struct ifreq)	/* device private 1 */

#ifdef __WISE__
#define SIOCGLINKSTATE	SIOCGPRIVATE_0			/* get IF link state */
#endif

#define	SIOCSIFVNET	_IOWR('i', 90, struct ifreq)	/* move IF jail/vnet */
#define	SIOCSIFRVNET	_IOWR('i', 91, struct ifreq)	/* reclaim vnet IF */

#define	SIOCGIFFIB	_IOWR('i', 92, struct ifreq)	/* get IF fib */
#define	SIOCSIFFIB	 _IOW('i', 93, struct ifreq)	/* set IF fib */

#define	SIOCGTUNFIB	_IOWR('i', 94, struct ifreq)	/* get tunnel fib */
#define	SIOCSTUNFIB	 _IOW('i', 95, struct ifreq)	/* set tunnel fib */

#define SIOCSDRVSPEC	_IOW('i', 123, struct ifdrv)    /* set driver-specific
								  parameters */
#define	SIOCGDRVSPEC	_IOWR('i', 123, struct ifdrv)   /* get driver-specific
								  parameters */

#define	SIOCIFCREATE	_IOWR('i', 122, struct ifreq)	/* create clone if */
#define	SIOCIFCREATE2	_IOWR('i', 124, struct ifreq)	/* create clone if */
#define	SIOCIFDESTROY	 _IOW('i', 121, struct ifreq)	/* destroy clone if */
/*	SIOCIFGCLONERS	_IOWR('i', 120, struct if_clonereq) get cloners */

/*	SIOCAIFGROUP	 _IOW('i', 135, struct ifgroupreq) add an ifgroup */
/*	SIOCGIFGROUP	_IOWR('i', 136, struct ifgroupreq) get ifgroups */
/*	SIOCDIFGROUP	 _IOW('i', 137, struct ifgroupreq) delete ifgroup */
/*	SIOCGIFGMEMB	_IOWR('i', 138, struct ifgroupreq) get members */
/*	SIOCGIFXMEDIA	_IOWR('i', 139, struct ifmediareq) get net xmedia */

/*	SIOCGIFRSSKEY	_IOWR('i', 150, struct ifrsskey) get RSS key */
/*	SIOCGIFRSSHASH	_IOWR('i', 151, struct ifrsshash) get the current RSS
							type/func settings */
/* Defines for poor glibc2.0 users, the feature check is done at runtime */
#if !defined(SIOCSIFTXQLEN)
#define	SIOCGIFTXQLEN	_IOWR('i', 160, struct ifreq)	/* get IF queue length */
#define	SIOCSIFTXQLEN	_IOW('i', 161, struct ifreq)	/* set IF queue length */
#endif


#endif /* __USE_NATIVE_HEADER__ */

#endif

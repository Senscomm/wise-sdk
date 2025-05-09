/*
 * Copyright 2006-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _WISE_NET_IF_H
#define _WISE_NET_IF_H

#ifdef __USE_NATIVE_HEADER__

#include_next <net/if.h>

#else
#include <netinet/in.h>
#include <sys/socket.h>

#ifndef IFNAMSIZ
#define IFNAMSIZ 	16
#endif

/* this macro shows whether if statistics located in if_data
* originally these statistics should be in if_data, but in ROM
* the macro CONFIG_SUPPORT_IF_STATS is disabled, so the size and
* offset of `if_data` would be changed if CONFIG_SUPPORT_IF_STATS
* enabled outside ROM
*/
#define CONFIG_STATS_IN_IF_DATA		0

/*
 * Structure describing information about an interface
 * which may be of interest to management entities.
 */
struct if_data {
	/* generic interface information */
	uint8_t	ifi_type;			/* ethernet, tokenring, etc */
	uint8_t	ifi_addrlen;		/* media address length */
	uint8_t	ifi_hdrlen;			/* media header length */
	uint8_t	ifi_link_state;		/* current link state */
	uint32_t	ifi_mtu;			/* maximum transmission unit */
	uint32_t	ifi_metric;			/* routing metric (external only) */
#if CONFIG_STATS_IN_IF_DATA
	/* volatile statistics */
	uint64_t	ifi_ipackets;		/* packets received on interface */
	uint64_t	ifi_ierrors;		/* input errors on interface */
	uint64_t	ifi_opackets;		/* packets sent on interface */
	uint64_t	ifi_oerrors;		/* output errors on interface */
	uint64_t	ifi_ibytes;			/* total number of octets received */
	uint64_t	ifi_obytes;			/* total number of octets sent */
	uint64_t	ifi_omcasts;		/* packets sent via multicast */
	uint64_t	ifi_iqdrops;		/* dropped on input, this interface */
	uint64_t	ifi_oqdrops;		/* dropped on output, this interface */
#else
	uint32_t dummy;
#endif
};


/*
 * Message format for use in obtaining information about interfaces
 * from getkerninfo and the routing socket
 * For the new, extensible interface see struct if_msghdrl below.
 */
struct if_msghdr {
	u_short	ifm_msglen;	/* to skip over non-understood messages */
	u_char	ifm_version;	/* future binary compatibility */
	u_char	ifm_type;	/* message type */
	int	ifm_addrs;	/* like rtm_addrs */
	int	ifm_flags;	/* value of if_flags */
	u_short	ifm_index;	/* index for associated ifp */
	struct	if_data ifm_data;/* statistics and other data about if */
};

/*
 * Message format announcing the arrival or departure of a network interface.
 */
struct if_announcemsghdr {
	u_short	ifan_msglen;	/* to skip over non-understood messages */
	u_char	ifan_version;	/* future binary compatibility */
	u_char	ifan_type;	/* message type */
	u_short	ifan_index;	/* index for associated ifp */
	char	ifan_name[IFNAMSIZ]; /* if name, e.g. "en0" */
	u_short	ifan_what;	/* what type of announcement */
};

#define	IFAN_ARRIVAL	0	/* interface arrival */
#define	IFAN_DEPARTURE	1	/* interface departure */

/*
 * Buffer with length to be used in SIOCGIFDESCR/SIOCSIFDESCR requests
 */
struct ifreq_buffer {
	size_t	length;
	void	*buffer;
};

/*
 * Interface request structure used for socket
 * ioctl's.  All interface ioctl's must have parameter
 * definitions which begin with ifr_name.  The
 * remainder may be interface specific.
 */
struct	ifreq {
	char	ifr_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	union {
		struct	sockaddr ifru_addr;
		struct	sockaddr ifru_dstaddr;
		struct	sockaddr ifru_broadaddr;
		struct	sockaddr ifru_netmask;
#ifdef __WISE__
		struct	sockaddr ifru_gateway;
#endif
		struct	sockaddr ifru_hwaddr;
		struct	ifreq_buffer ifru_buffer;
		short	ifru_flags[2];
#ifdef __WISE__
		short	ifru_linkstate;
#endif
		short	ifru_index;
		int	ifru_jid;
		int	ifru_metric;
		int	ifru_mtu;
		int	ifru_phys;
		int	ifru_media;
		caddr_t	ifru_data;
		int	ifru_cap[2];
		u_int	ifru_fib;
		u_int	ifru_txqlen;
		u_char	ifru_vlan_pcp;
	} ifr_ifru;
#define	ifr_addr	ifr_ifru.ifru_addr	/* address */
#define	ifr_dstaddr	ifr_ifru.ifru_dstaddr	/* other end of p-to-p link */
#define	ifr_broadaddr	ifr_ifru.ifru_broadaddr	/* broadcast address */
#define	ifr_netmask	ifr_ifru.ifru_netmask	/* netmask */
#ifdef __WISE__
#define	ifr_gateway	ifr_ifru.ifru_gateway	/* gateway */
#endif
#define	ifr_hwaddr	ifr_ifru.ifru_hwaddr	/* MAC address */
#define	ifr_buffer	ifr_ifru.ifru_buffer	/* user supplied buffer with its length */
#define	ifr_flags	ifr_ifru.ifru_flags[0]	/* flags (low 16 bits) */
#define	ifr_flagshigh	ifr_ifru.ifru_flags[1]	/* flags (high 16 bits) */
#ifdef __WISE__
#define ifr_linkstate	ifr_ifru.ifru_linkstate /* link state */
#endif
#define	ifr_jid		ifr_ifru.ifru_jid	/* jail/vnet */
#define	ifr_metric	ifr_ifru.ifru_metric	/* metric */
#define	ifr_mtu		ifr_ifru.ifru_mtu	/* mtu */
#define ifr_phys	ifr_ifru.ifru_phys	/* physical wire */
#define ifr_media	ifr_ifru.ifru_media	/* physical media */
#define	ifr_data	ifr_ifru.ifru_data	/* for use by interface */
#define	ifr_reqcap	ifr_ifru.ifru_cap[0]	/* requested capabilities */
#define	ifr_curcap	ifr_ifru.ifru_cap[1]	/* current capabilities */
#define	ifr_index	ifr_ifru.ifru_index	/* interface index */
#define	ifr_fib		ifr_ifru.ifru_fib	/* interface fib */
#define	ifr_txqlen	ifr_ifru.ifru_txqlen	/* length of tx queue */
#define	ifr_vlan_pcp	ifr_ifru.ifru_vlan_pcp	/* VLAN priority */
};

#define	_SIZEOF_ADDR_IFREQ(ifr) \
	((ifr).ifr_addr.sa_len > sizeof(struct sockaddr) ? \
	 (sizeof(struct ifreq) - sizeof(struct sockaddr) + \
	  (ifr).ifr_addr.sa_len) : sizeof(struct ifreq))

struct ifreq6 {
	char ifr_name[IFNAMSIZ];
	struct sockaddr_in6 ifr6_addr;
	u_int ifr6_prefixlen;
	u_char ifr6_ifindex;
	u_char ifr6_add;
};

/* used with SIOCGIFMEDIA */
struct ifmediareq {
	char			ifm_name[IFNAMSIZ];
	int			ifm_current;
	int			ifm_mask;
	int			ifm_status;
	int			ifm_active;
	int			ifm_count;
	int*			ifm_ulist;
};

/* interface flags */
#define IFF_UP				0x0001
#define IFF_BROADCAST			0x0002	/* valid broadcast address */
#define IFF_LOOPBACK			0x0008
#define IFF_POINTOPOINT			0x0010	/* point-to-point link */
#define IFF_NOARP			0x0040	/* no address resolution */
#define IFF_AUTOUP			0x0080	/* auto dial */
#define IFF_PROMISC			0x0100	/* receive all packets */
#define IFF_ALLMULTI			0x0200	/* receive all multicast packets */
#define IFF_SIMPLEX			0x0800	/* doesn't receive own transmissions */
#define IFF_LINK			0x1000	/* has link */
#define IFF_AUTO_CONFIGURED		0x2000
#define IFF_CONFIGURING			0x4000
#define IFF_MULTICAST			0x8000	/* supports multicast */

/* flags set internally only: */
#define IFF_CANTCHANGE \
	(IFF_BROADCAST|IFF_POINTOPOINT|IFF_DRV_RUNNING|IFF_DRV_OACTIVE|\
	 IFF_SIMPLEX|IFF_MULTICAST|IFF_ALLMULTI|IFF_PROMISC)

/* interface alias flags */
#define IFAF_AUTO_CONFIGURED		0x0001	/* has been automatically configured */
#define IFAF_CONFIGURING		0x0002	/* auto configuration in progress */

#define IFSTATMAX 				256
struct ifstat {
	char ifs_name[IFNAMSIZ];
	char ascii[IFSTATMAX + 1];
};

/* used with SIOCGIFCOUNT, and SIOCGIFCONF */
struct ifconf {
	int				ifc_len;	/* size of buffer */
	union {
		void*		ifc_buf;
		struct ifreq* ifc_req;
		int			ifc_value;
	};
};

/* POSIX definitions follow */

struct if_nameindex {
	unsigned		if_index;	/* positive interface index */
	char*			if_name;	/* interface name, ie. "loopback" */
};


#ifdef __cplusplus
extern "C" {
#endif

/* Convert an interface name to an index, and vice versa.  */
unsigned int os_if_nametoindex (const char *__ifname);
char *os_if_indextoname (unsigned int __ifindex, char *__ifname);
struct if_nameindex* os_if_nameindex(void);
void os_if_freenameindex(struct if_nameindex* interfaceArray);
unsigned char os_if_nametoflags(char *);

#ifdef __cplusplus
}
#endif

#define if_nametoindex os_if_nametoindex
#define if_indextoname os_if_indextoname
#define if_nameindex()   os_if_nameindex()
#define if_freenameindex os_if_freenameindex
#define if_nametoflags   os_if_nametoflags

#endif /* __USE_NATIVE_HEADER__ */

#endif	/* _WISE_NET_IF_H */

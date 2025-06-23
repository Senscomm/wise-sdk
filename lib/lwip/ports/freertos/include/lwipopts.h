/**
 * @file
 *
 * lwIP Options Configuration
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
#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#include "lwip/debug.h"

#include "cmsis_os.h"
#include "generated/kconfig2lwipopt.h"

/* Fixup */
#define _roundup(x, y) ((((x) + (y) - 1)/(y)) * (y))

#define TCP_WND				\
	(CONFIG_TCP_WND_FACTOR * TCP_MSS)
#define TCP_SND_BUF			\
	(CONFIG_TCP_SND_BUF_FACTOR * TCP_MSS)
#define TCP_SND_QUEUELEN						\
	(CONFIG_TCP_SND_QUEUELEN_FACTOR * _roundup(TCP_SND_BUF, TCP_MSS))

/* PBUF_LINK_HLEN */

/*
   -----------------------------------------------
   ---------- Platform specific locking ----------
   -----------------------------------------------
*/

/**
 * SYS_LIGHTWEIGHT_PROT==1: if you want inter-task protection for certain
 * critical regions during buffer allocation, deallocation and memory
 * allocation and deallocation.
 */
//#define SYS_LIGHTWEIGHT_PROT            0

/**
 * NO_SYS==1: Provides VERY minimal functionality. Otherwise,
 * use lwIP facilities.
 */
#ifndef CONFIG_LWIP
#define NO_SYS                          1
#else
#define NO_SYS                          0
#endif

/*
   ------------------------------------
   ---------- Memory options ----------
   ------------------------------------
*/

/**
 * MEM_ALIGNMENT: should be set to the alignment of the CPU
 *    4 byte alignment -> #define MEM_ALIGNMENT 4
 *    2 byte alignment -> #define MEM_ALIGNMENT 2
 */
//#define MEM_ALIGNMENT                   4U

/**
 * MEM_SIZE: the size of the heap memory. If the application will send
 * a lot of data that needs to be copied, this should be set high.
 */
//#define MEM_LIBC_MALLOC			1

//#define MEMP_USE_CUSTOM_POOLS           1

/*
   ------------------------------------------------
   ---------- Internal Memory Pool Sizes ----------
   ------------------------------------------------
*/
/**
 * MEMP_NUM_PBUF: the number of memp struct pbufs (used for PBUF_ROM and PBUF_REF).
 * If the application sends a lot of data out of ROM (or other static memory),
 * this should be set high.
 */
//#define MEMP_NUM_PBUF                   16

/**
 * MEMP_NUM_RAW_PCB: Number of raw connection PCBs
 * (requires the LWIP_RAW option)
 */
//#define MEMP_NUM_RAW_PCB                4

/**
 * MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
 * per active UDP "connection".
 * (requires the LWIP_UDP option)
 */
//#define MEMP_NUM_UDP_PCB                4

/**
 * MEMP_NUM_TCP_PCB: the number of simulatenously active TCP connections.
 * (requires the LWIP_TCP option)
 */
//#define MEMP_NUM_TCP_PCB                4

/**
 * MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP connections.
 * (requires the LWIP_TCP option)
 */
//#define MEMP_NUM_TCP_PCB_LISTEN         4

/**
 * MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP segments.
 * (requires the LWIP_TCP option)
 */
//#define MEMP_NUM_TCP_SEG                16

/**
 * MEMP_NUM_REASSDATA: the number of simultaneously IP packets queued for
 * reassembly (whole packets, not fragments!)
 */
//#define MEMP_NUM_REASSDATA              4

/**
 * MEMP_NUM_ARP_QUEUE: the number of simulateously queued outgoing
 * packets (pbufs) that are waiting for an ARP request (to resolve
 * their destination address) to finish.
 * (requires the ARP_QUEUEING option)
 */
//#define MEMP_NUM_ARP_QUEUE              2

/**
 * MEMP_NUM_SYS_TIMEOUT: the number of simulateously active timeouts.
 * (requires NO_SYS==0)
 */
/* FIXME: let MEMP_NUM_SYS_TIMEOUT defined by opt.h */
/*#define MEMP_NUM_SYS_TIMEOUT            3*/

/* XXX: More to be added in the future. */
#define MEMP_NUM_APP_TIMEOUTS	(MEMP_NUM_MDNS_TIMERS + MEMP_NUM_SNTP_TIMERS)

/**
 * MEMP_NUM_NETBUF: the number of struct netbufs.
 * (only needed if you use the sequential API, like api_lib.c)
 */
//#define MEMP_NUM_NETBUF                 8

/**
 * MEMP_NUM_NETCONN: the number of struct netconns.
 * (only needed if you use the sequential API, like api_lib.c)
 */

#if defined(__WISE__)

#if (MEMP_NUM_RAW_PCB != 0 \
        && MEMP_NUM_UDP_PCB  != 0 \
        && MEMP_NUM_TCP_PCB  != 0 \
        && MEMP_NUM_TCP_PCB_LISTEN  != 0)
#define MEMP_NUM_NETCONN				\
	(MEMP_NUM_RAW_PCB + MEMP_NUM_UDP_PCB +		\
	 MEMP_NUM_TCP_PCB + MEMP_NUM_TCP_PCB_LISTEN)
#else /* at least one is 0, i.e., infinite. */
#define MEMP_NUM_NETCONN    (0)
#endif
#else

#define MEMP_NUM_NETCONN				\
	(MEMP_NUM_RAW_PCB + MEMP_NUM_UDP_PCB +		\
	 MEMP_NUM_TCP_PCB + MEMP_NUM_TCP_PCB_LISTEN)

#endif

/**
 * MEMP_NUM_TCPIP_MSG_API: the number of struct tcpip_msg, which are used
 * for callback/timeout API communication.
 * (only needed if you use tcpip.c)
 */
//#define MEMP_NUM_TCPIP_MSG_API          8

/**
 * MEMP_NUM_TCPIP_MSG_INPKT: the number of struct tcpip_msg, which are used
 * for incoming packets.
 * (only needed if you use tcpip.c)
 */
//#define MEMP_NUM_TCPIP_MSG_INPKT        8

/**
 * PBUF_POOL_SIZE: the number of buffers in the pbuf pool.
 */
//#define PBUF_POOL_SIZE                  8

/**
 * MEMP_NUM_MBUF_CACHE: the number of buffers in the mbuf cache pool.
 * (required by net80211)
 */
//#define MEMP_NUM_MBUF_CACHE            14

/**
 * MEMP_NUM_MBUF_CHUNK: the number of buffers in the mbuf chunk pool.
 * (required by net80211)
 */
//#define MEMP_NUM_MBUF_CHUNK            14

/*
   ---------------------------------
   ---------- ARP options ----------
   ---------------------------------
*/
/**
 * LWIP_ARP==1: Enable ARP functionality.
 */
//#define LWIP_ARP                        1

/*
   --------------------------------
   ---------- IP options ----------
   --------------------------------
*/
/**
 * IP_FORWARD==1: Enables the ability to forward IP packets across network
 * interfaces. If you are going to run lwIP on a device with only one network
 * interface, define this to 0.
 */
#define IP_FORWARD                      0

/**
 * IP_OPTIONS: Defines the behavior for IP options.
 *      IP_OPTIONS_ALLOWED==0: All packets with IP options are dropped.
 *      IP_OPTIONS_ALLOWED==1: IP options are allowed (but not parsed).
 */
//#define IP_OPTIONS_ALLOWED              1

/**
 * IP_REASSEMBLY==1: Reassemble incoming fragmented IP packets. Note that
 * this option does not affect outgoing packet sizes, which can be controlled
 * via IP_FRAG.
 */
//#define IP_REASSEMBLY                   1

/**
 * IP_FRAG==1: Fragment outgoing IP packets if their size exceeds MTU. Note
 * that this option does not affect incoming packet sizes, which can be
 * controlled via IP_REASSEMBLY.
 */
//#define IP_FRAG                         1

/**
 * IP_REASS_MAXAGE: Maximum time (in multiples of IP_TMR_INTERVAL - so seconds, normally)
 * a fragmented IP packet waits for all fragments to arrive. If not all fragments arrived
 * in this time, the whole packet is discarded.
 */
//#define IP_REASS_MAXAGE                 3

/**
 * IP_REASS_MAX_PBUFS: Total maximum amount of pbufs waiting to be reassembled.
 * Since the received pbufs are enqueued, be sure to configure
 * PBUF_POOL_SIZE > IP_REASS_MAX_PBUFS so that the stack is still able to receive
 * packets even if the maximum amount of fragments is enqueued for reassembly!
 */
//#define IP_REASS_MAX_PBUFS              4

/**
 * IP_FRAG_USES_STATIC_BUF==1: Use a static MTU-sized buffer for IP
 * fragmentation. Otherwise pbufs are allocated and reference the original
    * packet data to be fragmented.
*/
#define IP_FRAG_USES_STATIC_BUF         0

/**
 * IP_DEFAULT_TTL: Default value for Time-To-Live used by transport layers.
 */
//#define IP_DEFAULT_TTL                  255

/*
   ----------------------------------
   ---------- ICMP options ----------
   ----------------------------------
*/
/**
 * LWIP_ICMP==1: Enable ICMP module inside the IP stack.
 * Be careful, disable that make your product non-compliant to RFC1122
 */
//#define LWIP_ICMP                       1

/*
   ---------------------------------
   ---------- RAW options ----------
   ---------------------------------
*/
/**
 * LWIP_RAW==1: Enable application layer to hook into the IP layer itself.
 */
//#define LWIP_RAW                        1

/*
   ----------------------------------
   ---------- DHCP options ----------
   ----------------------------------
*/
/**
 * LWIP_DHCP==1: Enable DHCP module.
 */
//#define LWIP_DHCP                       1
#define DHCP_CREATE_RAND_XID		1
#define LWIP_RAND			rand
/*
   ------------------------------------
   ---------- AUTOIP options ----------
   ------------------------------------
*/
/**
 * LWIP_AUTOIP==1: Enable AUTOIP module.
 */
//#define LWIP_AUTOIP                     1
//#define LWIP_DHCP_AUTOIP_COOP			1

/*
   ----------------------------------
   ---------- SNMP options ----------
   ----------------------------------
*/
/**
 * LWIP_SNMP==1: Turn on SNMP module. UDP must be available for SNMP
 * transport.
 */
#define LWIP_SNMP                       0

/*
   ----------------------------------
   ---------- IGMP options ----------
   ----------------------------------
*/
/**
 * LWIP_IGMP==1: Turn on IGMP module.
 */
//#define LWIP_IGMP                       0

/*
   ----------------------------------
   ---------- DNS options -----------
   ----------------------------------
*/
/**
 * LWIP_DNS==1: Turn on DNS module. UDP must be available for DNS
 * transport.
 */
//#define LWIP_DNS                        1

/* FIXME: check if this is allowable */
#define DNS_RANDTXID
//#define LWIP_DNS_SECURE					0

/*
   ---------------------------------
   ---------- UDP options ----------
   ---------------------------------
*/
/**
 * LWIP_UDP==1: Turn on UDP.
 */
//#define LWIP_UDP                        1

/*
   ---------------------------------
   ---------- TCP options ----------
   ---------------------------------
*/
/**
 * LWIP_TCP==1: Turn on TCP.
 */
//#define LWIP_TCP                        1

#define LWIP_LISTEN_BACKLOG             0

/*
   ----------------------------------
   ---------- Pbuf options ----------
   ----------------------------------
*/

#define PBUF_TRANSPORT_HLEN 20
#if CONFIG_LWIP_IPV6
#define PBUF_IP_HLEN        40
#else
#define PBUF_IP_HLEN        20
#endif

/**
 * PBUF_LINK_HLEN: the number of bytes that should be allocated for a
 * link level header. The default is 14, the standard value for
 * Ethernet.
 */
#define PBUF_LINK_HLEN                  (14 + ETH_PAD_SIZE)

#if 0
/**
 * PBUF_LINK_ENCAPSULATION_HLEN: the number of bytes that should be allocated
 * for an additional encapsulation header before ethernet headers (e.g. 802.11)
 */
#define PBUF_CUSTOM_HLEN			(0)
#define PBUF_IEEE80211_HW_HLEN			(16 + 20 + 12)
#define PBUF_IEEE80211_MAC_HLEN			(26)
#define PBUF_IEEE80211_PRIVACY_HLEN		(8)
#define PBUF_IEEE80211_LLC_LEN			(8)

#define PBUF_LINK_ENCAPSULATION_HLEN				 \
	(PBUF_CUSTOM_HLEN					 \
	 + PBUF_IEEE80211_HW_HLEN				 \
	 + PBUF_IEEE80211_MAC_HLEN				 \
	 + PBUF_IEEE80211_PRIVACY_HLEN				 \
	 + PBUF_IEEE80211_LLC_LEN				 \
	 - PBUF_LINK_HLEN)
#endif

#undef PBUF_POOL_BUFSIZE
/**
 * PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool. The default is
 * designed to accommodate single full size TCP frame in one pbuf, including
 * TCP_MSS, IP header, and link header.
*
 */
#define PBUF_POOL_BUFSIZE               LWIP_MEM_ALIGN_SIZE(TCP_MSS+PBUF_IP_HLEN+PBUF_TRANSPORT_HLEN+PBUF_LINK_HLEN)

/*
   ------------------------------------------------
   ---------- Network Interfaces options ----------
   ------------------------------------------------
*/
/**
 * LWIP_NETIF_API==1: Support netif api (in netifapi.c)
 */
//#define LWIP_NETIF_API                  1

/*
   ------------------------------------
   ---------- LOOPIF options ----------
   ------------------------------------
*/
/**
 * LWIP_HAVE_LOOPIF==1: Support loop interface (127.0.0.1) and loopif.c
 */
//#define LWIP_HAVE_LOOPIF                1

/*
   ----------------------------------------------
   ---------- Sequential layer options ----------
   ----------------------------------------------
*/

/**
 * LWIP_NETCONN==1: Enable Netconn API (require to use api_lib.c)
 */
//#define LWIP_NETCONN                    1

/*
   ------------------------------------
   ---------- Socket options ----------
   ------------------------------------
*/
/**
 * SO_REUSE==1: Enable SO_REUSEADDR option.
 */
//#define SO_REUSE                        1

/**
 * LWIP_SO_RCVBUF==1: Enable SO_RCVBUF processing.
 */
//#define LWIP_SO_RCVBUF                  1

/**
 * LWIP_SOCKET==1: Enable Socket API (require to use sockets.c)
 */
//#define LWIP_SOCKET                     1
//#define LWIP_SO_RCVTIMEO		1
//#define LWIP_SOCKET_OFFSET 		32 /* FIXME: large enough? */

/**
 * LWIP_LINK==1: Enable AF_LINK
 * LWIP_HOOK_UNKNOWN_ETH_PROTOCOL
 *
 */
//#define LWIP_LINK 			1
#define LWIP_HOOK_FILENAME "ether.h"
#define LWIP_HOOK_UNKNOWN_ETH_PROTOCOL(pbuf, netif) eth_input_all(pbuf, netif)

/**
 * The definitions below are to use sys/socket.h instead of lwip/sockets.h.
 */
#if defined(__WISE__)

#undef LWIP_SOCKET
#define LWIP_SOCKET 			0
#define LWIP_POSIX_SOCKETS_IO_NAMES	0
#define LWIP_SOCKET_SELECT		0
#define LWIP_SOCKET_POLL		0

#define SA_FAMILY_T_DEFINED
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS

#endif

/*
   ----------------------------------------
   ---------- Statistics options ----------
   ----------------------------------------
*/
/**
 * LWIP_STATS==1: Enable statistics collection in lwip_stats.
 */
//#define LWIP_STATS                      1
//#define LWIP_STATS_DISPLAY              1
/**
 * NETIF_STATS==1: Enable netif stats.
 */
//#define NETIF_STATS                     0
/*
   ---------------------------------
   ---------- PPP options ----------
   ---------------------------------
*/
/**
 * PPP_SUPPORT==1: Enable PPP.
 */
#define PPP_SUPPORT                     0



/*
   ---------------------------------------
   ---------- Thread options ----------
   ---------------------------------------
*/

//#define LWIP_TCPIP_CORE_    0
//#define LWIP_RTOS_CHECK_CORE_LOCKING 	0

#if !NO_SYS

#ifdef __cplusplus
extern "C" {
#endif

/* XXX: these are required even when LWIP_TCPIP_CORE_LOCKING is not defined.
 */
void sys_check_core_locking(void);
void sys_mark_tcpip_thread(void);
int sys_is_tcpip_context(void);
int sys_is_core_locked(void);
#define LWIP_ASSERT_CORE_LOCKED()  sys_check_core_locking()
#define LWIP_MARK_TCPIP_THREAD()   sys_mark_tcpip_thread()
#ifdef __WISE__
/* XXX: to allow callbacks in tcpip thread context.
 */
#define LWIP_IS_TCPIP_CONTEXT()    sys_is_tcpip_context()
#define LWIP_IS_CORE_LOCKED()      sys_is_core_locked()
#define LWIP_IS_MBOX_FEASIBLE()    !(LWIP_IS_TCPIP_CONTEXT() || LWIP_IS_CORE_LOCKED())
#endif

#if LWIP_TCPIP_CORE_LOCKING
/* #define LWIP_LOCK_LAST_HOLDER_DEBUG */
#ifdef LWIP_LOCK_LAST_HOLDER_DEBUG
void sys_lock_tcpip_core(const char *, const int);
void sys_unlock_tcpip_core(const char *, const int);
const char *sys_lock_current_holder_fn(void);
int sys_lock_current_holder_ln(void);
const char *sys_lock_last_holder_fn(void);
int sys_lock_last_holder_ln(void);
#else
void sys_lock_tcpip_core(void);
void sys_unlock_tcpip_core(void);
#endif
#ifdef LWIP_LOCK_LAST_HOLDER_DEBUG
#define LOCK_TCPIP_CORE()          sys_lock_tcpip_core(__func__, __LINE__)
#define UNLOCK_TCPIP_CORE()        sys_unlock_tcpip_core(__func__, __LINE__)
#else
#define LOCK_TCPIP_CORE()          sys_lock_tcpip_core()
#define UNLOCK_TCPIP_CORE()        sys_unlock_tcpip_core()
#endif
#else /* LWIP_TCPIP_CORE_LOCKING */
#if !LWIP_NETCONN_SEM_PER_THREAD
#error "LWIP_NETCONN_SEM_PER_THREAD must be enabled!"
#endif
#define LOCK_TCPIP_CORE()
#define UNLOCK_TCPIP_CORE()
#endif /* LWIP_TCPIP_CORE_LOCKING */

#ifdef __cplusplus
}
#endif

#endif /* NO_SYS */

#if defined(__WISE__)

/* For SYS_LIGHTWEIGHT_PROT */

#include "hal/irq.h"
#define SYS_ARCH_DECL_PROTECT(lev)	unsigned long lev
#define SYS_ARCH_PROTECT(lev)		local_irq_save(lev)
#define SYS_ARCH_UNPROTECT(lev)		local_irq_restore(lev)

#endif


//#define DEFAULT_THREAD_STACKSIZE	(CONFIG_DEFAULT_STACK_SIZE)
//#define DEFAULT_THREAD_PRIO             (osPriorityNormal)

//#define TCPIP_THREAD_STACKSIZE		(2 * DEFAULT_THREAD_STACKSIZE)
//#define TCPIP_THREAD_PRIO		DEFAULT_THREAD_PRIO

//#define TCPIP_MBOX_SIZE                 4
//#define DEFAULT_RAW_RECVMBOX_SIZE       4 /*1*/
//#define DEFAULT_UDP_RECVMBOX_SIZE       1
//#define DEFAULT_TCP_RECVMBOX_SIZE       1
//#define DEFAULT_ACCEPTMBOX_SIZE         1

/*
   ----------------------------------------
   ---------- Debug options ----------
   ----------------------------------------
*/
#define LWIP_ERRNO_STDINCLUDE
//#define MEMP_OVERFLOW_CHECK             0

/* On demand timer protocol definition */
#ifdef CONFIG_LWIP_TIMERS_ONDEMAND

#define SCM_LWIP_ETHARP_TIMERS_ONDEMAND           LWIP_ARP
#define SCM_LWIP_IGMP_TIMERS_ONDEMAND             LWIP_IGMP
#define SCM_LWIP_DNS_TIMERS_ONDEMAND              LWIP_DNS
#define SCM_LWIP_DHCP_FINE_TIMERS_ONDEMAND        LWIP_DHCP
#define SCM_LWIP_IP4_REASSEMBLY_TIMERS_ONDEMAND   IP_REASSEMBLY
#define SCM_LWIP_IP6_REASSEMBLY_TIMERS_ONDEMAND   LWIP_IPV6_REASS
#define SCM_LWIP_MLD6_TIMERS_ONDEMAND             LWIP_IPV6_MLD

#else

#define SCM_LWIP_ETHARP_TIMERS_ONDEMAND           0
#define SCM_LWIP_IGMP_TIMERS_ONDEMAND             0
#define SCM_LWIP_DNS_TIMERS_ONDEMAND              0
#define SCM_LWIP_DHCP_FINE_TIMERS_ONDEMAND        0
#define SCM_LWIP_IP4_REASSEMBLY_TIMERS_ONDEMAND   0
#define SCM_LWIP_IP6_REASSEMBLY_TIMERS_ONDEMAND   0
#define SCM_LWIP_MLD6_TIMERS_ONDEMAND             0

#endif /* CONFIG_LWIP_TIMERS_ONDEMAND */

#endif /* LWIP_LWIPOPTS_H */

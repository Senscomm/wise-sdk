ccflags-$(CONFIG_SUPPORT_GCOV_LIB_LWIP) += -fprofile-arcs -ftest-coverage

ccflags-y += -I$(src)/src/include -I$(src)/ports/freertos/include -I$(srctree)/lib -I$(srctree)/include/FreeRTOS -I$(srctree)/lib/net80211
ccflags-y += -D__WISE__

ifdef CONFIG_LWIP

ccflags-$(CONFIG_LWIP_DEBUG_ENABLE) += -DLWIP_DEBUG 

ifdef CONFIG_LWIP_ASSERT_VERBOSITY
ccflags-y += -DASSERT_VERBOSITY=$(CONFIG_LWIP_ASSERT_VERBOSITY)
endif

ifeq ($(CONFIG_LWIP_ASSERT_VERBOSITY)$(CONFIG_LWP_DEBUG_ENABLE),)
ccflags-y += -Wno-unused-but-set-variable -Wno-unused-variable
endif

obj-y += ports/freertos/sys_arch.o
obj-y += ports/freertos/xnet.o

ifeq ($(CONFIG_WISE_SOCKET),y)
obj-y += ports/freertos/xsocket.o
obj-y += ports/freertos/xnetdb.o
else
obj-y += src/api/sockets.o
obj-y += src/api/netdb.o
endif
obj-y += src/api/api_lib.o
obj-y += src/api/api_msg.o
obj-y += src/api/err.o
obj-y += src/api/if_api.o
obj-y += src/api/netbuf.o
obj-y += src/api/netifapi.o
obj-y += src/api/tcpip.o
obj-y += src/core/altcp.o
obj-y += src/core/altcp_alloc.o
obj-y += src/core/altcp_tcp.o
obj-y += src/core/def.o
obj-y += src/core/dns.o
obj-y += src/core/inet_chksum.o
obj-y += src/core/init.o
obj-y += src/core/ip.o
obj-y += src/core/mem.o
#obj-y += src/core/memp.o
obj-y += ports/freertos/xmemp.o
obj-y += src/core/netif.o
obj-y += src/core/pbuf.o
obj-y += src/core/raw.o
obj-y += src/core/stats.o
obj-y += src/core/sys.o
obj-y += src/core/tcp.o
obj-y += src/core/tcp_in.o
obj-y += src/core/tcp_out.o
obj-y += src/core/timeouts.o
obj-y += src/core/udp.o
obj-y += src/core/ipv4/acd.o
obj-y += src/core/ipv4/autoip.o
obj-y += src/core/ipv4/dhcp.o
obj-y += src/core/ipv4/etharp.o
obj-y += src/core/ipv4/icmp.o
obj-y += src/core/ipv4/igmp.o
obj-y += src/core/ipv4/ip4_addr.o
obj-y += src/core/ipv4/ip4.o
obj-y += src/core/ipv4/ip4_frag.o
obj-y += src/core/ipv6/dhcp6.o
obj-y += src/core/ipv6/ethip6.o
obj-y += src/core/ipv6/icmp6.o
obj-y += src/core/ipv6/inet6.o
obj-y += src/core/ipv6/ip6_addr.o
obj-y += src/core/ipv6/ip6.o
obj-y += src/core/ipv6/ip6_frag.o
obj-y += src/core/ipv6/mld6.o
obj-y += src/core/ipv6/nd6.o
obj-y += src/netif/ethernet.o
obj-y += src/netif/ethernetif.o
obj-y += src/netif/routeif.o

else

obj-$(CONFIG_NET80211) += ports/freertos/xnet.o
obj-$(CONFIG_NET80211) += src/core/init.o
obj-$(CONFIG_NET80211) += src/core/mem.o
#obj-$(CONFIG_NET80211) += src/core/memp.o
obj-$(CONFIG_NET80211) += ports/freertos/xmemp.o
obj-$(CONFIG_NET80211) += src/core/pbuf.o

endif #CONFIG_LWIP

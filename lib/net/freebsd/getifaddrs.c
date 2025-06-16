/*
 * Copyright 2025-2027 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "lwip/netif.h"
#include "hal/console.h"

//#define DEBUG_GETIFADDRS

static int
getifaddr4(struct ifaddrs **head, const char *netifname, int fd) {
	struct ifreq ifr;
	struct ifaddrs *new_ifa = calloc(1, sizeof(struct ifaddrs));

	if (!new_ifa)
		return -1;

	new_ifa->ifa_name = strdup(netifname);

	 if (!new_ifa->ifa_name) {
		free(new_ifa);
		return -1;
	 }

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, netifname, IFNAMSIZ);

	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)
			goto skip;

	new_ifa->ifa_flags = ifr.ifr_flags;

#ifdef DEBUG_GETIFADDRS
	printf("[INFO] Flags: 0x%x\n", new_ifa->ifa_flags);
#endif

	if (ioctl(fd, SIOCGIFADDR, &ifr) == 0 && ifr.ifr_addr.sa_family == AF_INET) {
		struct sockaddr_in *addr = malloc(sizeof(struct sockaddr_in));

		if (!addr)
			return -1;

		memcpy(addr, &ifr.ifr_addr, sizeof(struct sockaddr_in));
		new_ifa->ifa_addr = (struct sockaddr *)addr;
#ifdef DEBUG_GETIFADDRS
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
		printf("[INFO] IPv4 address: %s\n", ip);
#endif
	} else {
		goto skip;
	}

	if (ioctl(fd, SIOCGIFNETMASK, &ifr) == 0) {
		struct sockaddr_in *netmask = malloc(sizeof(struct sockaddr_in));

		if (!netmask)
			return -1;

		memcpy(netmask, &ifr.ifr_netmask, sizeof(struct sockaddr_in));
		new_ifa->ifa_netmask = (struct sockaddr *)netmask;
#ifdef DEBUG_GETIFADDRS
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &netmask->sin_addr, ip, sizeof(ip));
		printf("[INFO] Netmask: %s\n", ip);
#endif
	} else {
		goto skip;
	}

	if (new_ifa->ifa_flags & IFF_BROADCAST) {
		if (ioctl(fd, SIOCGIFBRDADDR, &ifr) == 0) {
			struct sockaddr_in *broad = malloc(sizeof(struct sockaddr_in));
			if (!broad) return -1;
			memcpy(broad, &ifr.ifr_broadaddr, sizeof(struct sockaddr_in));
			new_ifa->ifa_broadaddr = (struct sockaddr *)broad;
#ifdef DEBUG_GETIFADDRS
			char ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &broad->sin_addr, ip, sizeof(ip));
			printf("[INFO] Broadcast address: %s\n", ip);
#endif
		} else {
			goto skip;
		}
	} else if (new_ifa->ifa_flags & IFF_POINTOPOINT) {
		if (ioctl(fd, SIOCGIFDSTADDR, &ifr) == 0) {
			struct sockaddr_in *dst = malloc(sizeof(struct sockaddr_in));

			if (!dst)
				return -1;

			memcpy(dst, &ifr.ifr_dstaddr, sizeof(struct sockaddr_in));
			new_ifa->ifa_dstaddr = (struct sockaddr *)dst;
#ifdef DEBUG_GETIFADDRS
			char ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &dst->sin_addr, ip, sizeof(ip));
			printf("[INFO] Destination address: %s\n", ip);
#endif
		} else {
			goto skip;
		}
	}

	new_ifa->ifa_next = *head;
	*head = new_ifa;
	return 0;

skip:
#ifdef DEBUG_GETIFADDRS
	printf("[WARN] Skipping interface: %s due to incomplete info\n", new_ifa->ifa_name);
#endif
	if (new_ifa->ifa_name) free(new_ifa->ifa_name);
	if (new_ifa->ifa_addr) free(new_ifa->ifa_addr);
	if (new_ifa->ifa_netmask) free(new_ifa->ifa_netmask);
	if (new_ifa->ifa_flags & IFF_BROADCAST && new_ifa->ifa_broadaddr)
		free(new_ifa->ifa_broadaddr);
	else if (new_ifa->ifa_flags & IFF_POINTOPOINT && new_ifa->ifa_dstaddr)
		free(new_ifa->ifa_dstaddr);
	if (new_ifa->ifa_data) free(new_ifa->ifa_data);
	free(new_ifa);
		return -1;

}

#ifdef CONFIG_LWIP_IPV6

static int getifaddr6(struct ifaddrs **head, const char *netifname, int fd)
{
	struct ifreq6 ifr6;
	struct ifaddrs *new_ifa;
	int index;

	for (index = 0; index < LWIP_IPV6_NUM_ADDRESSES; index++) {
		memset(&ifr6, 0, sizeof(ifr6));
		strncpy(ifr6.ifr_name, netifname, IFNAMSIZ);
		ifr6.ifr6_addr.sin6_family = AF_INET6;
		ifr6.ifr6_ifindex = index;

		if (ioctl(fd, SIOCGIFADDR, &ifr6) == 0) {
			if (ifr6.ifr6_addr.sin6_len == 0) {
				continue;
			}
			struct sockaddr_in6 *sa6 = malloc(sizeof(struct sockaddr_in6));

			if (!sa6)
				return -1;

			memcpy(sa6, &ifr6.ifr6_addr, sizeof(struct sockaddr_in6));

			new_ifa = calloc(1, sizeof(struct ifaddrs));
			if (!new_ifa)
				return -1;

			new_ifa->ifa_name = strdup(netifname);
			new_ifa->ifa_addr = (struct sockaddr *)sa6;

#ifdef DEBUG_GETIFADDRS
			char ip6[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &sa6->sin6_addr, ip6, sizeof(ip6));
			printf("[INFO] IPv6 address: %s\n", ip6);
#endif
			new_ifa->ifa_next = *head;
			*head = new_ifa;
		}
	}

	return 0;
}

#endif

int
getifaddrs(struct ifaddrs **ifa)
{
	int fd, i = 1;
	struct ifaddrs *head = NULL;
	char netifname[IFNAMSIZ];

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		return -1;

	*ifa = NULL;

	do {
		if (!if_indextoname(i++, netifname)) {
			if (i > 1)
				break;
			else {
				printf("No net i/f available.\n");
				goto fail;
			}
		}

		if (if_nametoflags(netifname) & NETIF_FLAG_ROUTE) {
			/* Skip interface for internal use */
			continue;
		}

		if (getifaddr4(&head, netifname, fd) < 0)
			continue;

#ifdef CONFIG_LWIP_IPV6
		if (getifaddr6(&head, netifname, fd) < 0)
			continue;
#endif

	} while (1);


	close(fd);
	*ifa = head;
	return 0;

fail:
	printf("%s: fail \n", __func__);
	close(fd);
	while (head) {
		struct ifaddrs *next = head->ifa_next;
		if (head->ifa_name)
			free(head->ifa_name);
		if (head->ifa_addr)
			free(head->ifa_addr);
		if (head->ifa_netmask)
			free(head->ifa_netmask);
		if (head->ifa_flags & IFF_BROADCAST && head->ifa_broadaddr)
			free(head->ifa_broadaddr);
		else if (head->ifa_flags & IFF_POINTOPOINT && head->ifa_dstaddr)
			free(head->ifa_dstaddr);
		if (head->ifa_data) free(head->ifa_data);
		free(head);
		head = next;
	}
	return -1;

}

void
freeifaddrs(struct ifaddrs *ifap)
{
	while (ifap) {
		struct ifaddrs *next = ifap->ifa_next;

		if (ifap->ifa_name) {
			free(ifap->ifa_name);
			ifap->ifa_name = NULL;
		}
		if (ifap->ifa_addr) free(ifap->ifa_addr);
		if (ifap->ifa_netmask) free(ifap->ifa_netmask);
		if (ifap->ifa_flags & IFF_BROADCAST && ifap->ifa_broadaddr)
			free(ifap->ifa_broadaddr);
		else if (ifap->ifa_flags & IFF_POINTOPOINT && ifap->ifa_dstaddr)
			free(ifap->ifa_dstaddr);
		if (ifap->ifa_data) free(ifap->ifa_data);

		free(ifap);
		ifap = next;
	}
}

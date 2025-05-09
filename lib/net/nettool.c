/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#if defined (CONFIG_CMD_NETTOOL) && defined(CONFIG_N22_ONLY)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/task.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "lwip/ip_addr.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/ip6.h"

#include <cmsis_os.h>

#include "cli.h"

#define MC_UDP_PORT 				6789

uint16_t tcp_port;
uint16_t tcp_timeout;
uint16_t udp_port;
uint16_t udp_timeout;

static const char *payload = "Msg from Xiaohu";

#define MULTICAST_IPV4_ADDR 		"224.0.1.129"
#define CONFIG_EXAMPLES_IGMP_GRPADDR 0xE0000181

#define	INADDR_ANY		((in_addr_t)0x00000000)

static int create_multicast_ipv4_socket(void)
{
	struct ip_mreq imreq = { 0 };
	struct sockaddr_in saddr = { 0 };
	int sock = -1;
	int err = 0;

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock < 0) {
		printf("[MC] Failed to create socket. Error %d\n", errno);
		return -1;
	}

	/* Bind the socket to any address */
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(MC_UDP_PORT);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
	if (err < 0) {
		printf("[MC] Failed to bind socket. Error %d\n", errno);
		goto err;
	}

	/** this is also a listening socket, so add it to the multicast
	 *  group for listening...
	 */
	imreq.imr_interface.s_addr = htonl(INADDR_ANY); // ipaddr_any

	/* Add a socket to the IPV4 multicast group */
	//imreq.imr_multiaddr.s_addr = htonl(CONFIG_EXAMPLES_IGMP_GRPADDR);
	imreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_IPV4_ADDR);

	/* Add membership for joining */
	err = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
							 &imreq, sizeof(struct ip_mreq));
	if (err < 0) {
		printf("[MC] Failed to set IP_ADD_MEMBERSHIP. Error %d\n", errno);
		goto err;
	}

	/* TTL small ttl = 3*/
	/* if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))) */

	return sock;
err:
	close(sock);
	return -1;
}


static void mcast_example_task(void *pvParameters)
{
	int sock;
	int err = 1;
	char recvbuf[100];
	struct sockaddr_storage raddr;
	int len;

	sock = create_multicast_ipv4_socket();
	if (sock < 0) {
		osThreadExit();
		return;
	}

	/* All set, socket is configured for receiving */
	printf("[MC] create IPv4 multicast addr(%s) port (%d)\n", MULTICAST_IPV4_ADDR, MC_UDP_PORT);
	while (err > 0) {
		fd_set rfds;

		/* Set timeout as 20 sec --> retry */
		struct timeval tv = {
			.tv_sec = 20,
			.tv_usec = 0,
		};

		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);

		int s = select(sock + 1, &rfds, NULL, NULL, &tv);
		if (s < 0) {
			printf("[MC] Select failed: errno %d\n", errno);
			err = -1;
		break;
		} else if (s > 0) {
			if (FD_ISSET(sock, &rfds)) {
				socklen_t socklen = sizeof(raddr);

				len = recvfrom(sock, recvbuf, sizeof(recvbuf)-1, 0,
					(struct sockaddr *)&raddr, &socklen);
				if (len < 0) {
					printf("[MC] multicast recvfrom failed: errno %d \n", errno);
					err = -1;
					break;
				}
				recvbuf[len] = 0;
				printf("[MC] RX %s\n", recvbuf);
			}
		} else { /* s == 0 */
			break;
		}
	}

	printf("[MC] Shutting down MC socket & task\n");
	shutdown(sock, 0);
	close(sock);

	osThreadExit();
	return;
}

static void udp_server_task(void *arg)
{
	int err;
	char rx_buffer[100];
	char *addr_str = NULL;
	int ip_protocol = 0;
	struct sockaddr_storage dest_addr;
	socklen_t socklen;
	struct timeval timeout;
	int addr_family = (int)arg;

	if (addr_family == AF_INET) {
		struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
		memset(&dest_addr, 0, sizeof(dest_addr));
		dest_addr_ip4->sin_len = sizeof(struct sockaddr_in);
		dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
		dest_addr_ip4->sin_family = AF_INET;
		dest_addr_ip4->sin_port = htons(udp_port);
		ip_protocol = IPPROTO_IP;
	}
#if LWIP_IPV6
	else if (addr_family == AF_INET6) {
		struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
		bzero(&dest_addr_ip6->sin6_addr.__in6_u, sizeof(dest_addr_ip6->sin6_addr.__in6_u));
		dest_addr_ip6->sin6_len = sizeof(struct sockaddr_in6);
		dest_addr_ip6->sin6_family = AF_INET6;
		dest_addr_ip6->sin6_port = htons(udp_port);
		dest_addr_ip6->sin6_scope_id = 0;
		ip_protocol = IPPROTO_IPV6;
	}
#endif

	int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
	if (sock < 0) {
		printf("[UDP] Unable to create socket: errno %d\n", errno);
		osThreadExit();
		return;
	}

#if LWIP_IPV4 && LWIP_IPV6
	if (addr_family == AF_INET6) {
		/* Note that by default IPV6 binds to both protocols, it is must be disabled
		 * if both protocols used at the same time.
		 */
		int opt = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
	}
#endif

	/* Set timeout --> if no packet --> close */
	timeout.tv_sec = udp_timeout;
	timeout.tv_usec = 0;
	setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

	err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (err < 0) {
		printf("[UDP] Socket unable to bind: errno %d\n", errno);
		goto CLEAN_UP;
	}
	printf("[UDP] Socket[%s] bound, port %u\n",
		addr_family == AF_INET ? "IPv4" : "IPv6", udp_port);

	while (1) {
		printf("[UDP] Waiting for data...\n");
		struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
		socklen = sizeof(source_addr);

		int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

		/* Error occurred during receiving */
		if (len < 0) {
			printf("[UDP] recvfrom failed: errno %d\n", errno);
			break;
		}
		/* received closing msg (no data) */
		else if (len == 0) {
			printf("[UDP] recvfrom closing msg\n");
			break;
		}
		/* Data received */
		else {
			/* Get the sender's ip address as string */
			if (source_addr.ss_family == PF_INET) {
				addr_str = inet_ntoa(((struct sockaddr_in *)&source_addr)->sin_addr);
			}
#if LWIP_IPV6
			else if (source_addr.ss_family == PF_INET6) {
				addr_str = inet6_ntoa(((struct sockaddr_in6 *)&source_addr)->sin6_addr);
			}
#endif
			/* Null-terminate whatever we received and treat like a string... */
			rx_buffer[len] = 0;
			printf("[UDP] client [%s:%u] Received %d bytes: %s\n",
				addr_str ? addr_str : "", ntohs(((struct sockaddr_in *)&source_addr)->sin_port), len, rx_buffer);
		}
	}

CLEAN_UP:
	if (sock != -1) {
		printf("[UDP] Shutting down socket...\n");
		shutdown(sock, 0);
		close(sock);
	}

	osThreadExit();
}

static void tcp_server_task(void *arg)
{
	int len;
	char rx_buffer[100];
	int ip_protocol = 0;
	struct sockaddr_storage dest_addr;
	int opt = 1;
	int err;
	int listen_sock;
	struct timeval timeout;
	socklen_t addr_len;
	int addr_family = (int)arg;
	struct sockaddr_storage source_addr;

	if (addr_family == AF_INET) {
		struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
		dest_addr_ip4->sin_len = sizeof(struct sockaddr_in);
		dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
		dest_addr_ip4->sin_family = AF_INET;
		dest_addr_ip4->sin_port = htons(tcp_port);
		ip_protocol = IPPROTO_IP;
	}
#if LWIP_IPV6
	else if (addr_family == AF_INET6) {
		struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
		bzero(&dest_addr_ip6->sin6_addr.__in6_u, sizeof(dest_addr_ip6->sin6_addr.__in6_u));
		dest_addr_ip6->sin6_len = sizeof(struct sockaddr_in6);
		dest_addr_ip6->sin6_family = AF_INET6;
		dest_addr_ip6->sin6_port = htons(tcp_port);
		dest_addr_ip6->sin6_scope_id = 0;
		ip_protocol = IPPROTO_IPV6;
	}
#endif

	listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
	if (listen_sock < 0) {
		printf("[TCP] Unable to create socket: errno %d\n", errno);
		osThreadExit();
		return;
	}

	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#if LWIP_IPV4 && LWIP_IPV6
	/* Note that by default IPV6 binds to both protocols, it is must be disabled
	 * if both protocols used at the same time.
	 */
	setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

	/* Set timeout --> if no connect --> close */
	timeout.tv_sec = tcp_timeout;
	timeout.tv_usec = 0;
	setsockopt (listen_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (err != 0) {
		printf("[TCP] Socket unable to bind: errno %d\n", errno);
		goto CLEAN_UP;
	}
	printf("[TCP] Socket[%s] bound, listen port %d\n",
		addr_family == AF_INET ? "IPv4" : "IPv6", tcp_port);

	err = listen(listen_sock, 1);
	if (err != 0) {
		printf("[TCP] Error occurred during listen: errno %d\n", errno);
		goto CLEAN_UP;
	}

	addr_len = sizeof(source_addr);
	int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
	if (sock < 0) {
		printf("[TCP] Unable to accept connection: errno %d\n", errno);
		goto CLEAN_UP;
	}

	/* Set timeout --> if no packet --> close */
	setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	printf("[TCP] Connection accepted client port[%u] -- receiving\n",
		ntohs(((struct sockaddr_in *)&source_addr)->sin_port));

	do {
		len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
		if (len < 0) {
			printf("[TCP] Error occurred during receiving: errno %d\n", errno);
		} else if (len == 0) {
			printf("[TCP] Connection closed\n");
		} else {
			rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
			printf("[TCP] Received %d bytes: %s\n", len, rx_buffer);
		}
	} while (len > 0);

	shutdown(sock, 0);
	close(sock);

CLEAN_UP:
	close(listen_sock);
	osThreadExit();
}

/**************************************************************************/
int
do_nettool_tcpserver(int argc, char **argv)
{
	int ret = CMD_RET_FAILURE;
	osThreadAttr_t attr = {
		.name 		= "tcp_server",
		.stack_size = 2048,
		.priority 	= osPriorityNormal,
	};

	if (argc != 3) {
		goto done;
	}
	tcp_port = atoi(argv[1]);
	tcp_timeout = atoi(argv[2]);

	if ((tcp_timeout == 0) || (tcp_timeout > 300)) {
		printf("tcp bind/close timeout should be 1-300\n");
		goto done;
	}

#if LWIP_IPV4
	if (osThreadNew(tcp_server_task, (void*)AF_INET, &attr) == NULL) {
		printf("%s: failed to create tcp server task\n", __func__);
		goto done;
	}
#endif
#if LWIP_IPV6
	osDelay(100);
	if (osThreadNew(tcp_server_task, (void*)AF_INET6, &attr) == NULL) {
		printf("%s: failed to create tcp server task\n", __func__);
		goto done;
	}
#endif

	ret = CMD_RET_SUCCESS;
done:
	return ret;
}

void tcp_client(const char *server_ip, uint16_t port, uint8_t bind_local, uint16_t localport)
{
	int err;
	int addr_family = 0;
	int ip_protocol = 0;
	struct sockaddr_storage dest_addr;
	struct sockaddr_storage bind_addr;

#if LWIP_IPV6
	ip_addr_t addr;
	int is_ipv6 = 0;
	if (ipaddr_aton(server_ip, &addr) && IP_IS_V6(&addr) &&
		!ip6_addr_isipv4mappedipv6(ip_2_ip6(&addr))) {
		struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
		is_ipv6 = 1;
		dest_addr_ip6->sin6_len = sizeof(struct sockaddr_in6);
		inet6_aton(server_ip, &dest_addr_ip6->sin6_addr);
		dest_addr_ip6->sin6_family = AF_INET6;
		dest_addr_ip6->sin6_port = htons(port);
		dest_addr_ip6->sin6_scope_id = 0;
		addr_family = AF_INET6;
		ip_protocol = IPPROTO_IPV6;
	} else
#endif /* LWIP_IPV6 */
	{
		struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
		dest_addr_ip4->sin_len = sizeof(struct sockaddr_in);
		dest_addr_ip4->sin_addr.s_addr = inet_addr(server_ip);
		if (dest_addr_ip4->sin_addr.s_addr == INADDR_NONE) {
			printf("Invalid IP %s\n", server_ip);
			return;
		}
		dest_addr_ip4->sin_family = AF_INET;
		dest_addr_ip4->sin_port = htons(port);
		addr_family = AF_INET;
		ip_protocol = IPPROTO_IP;
	}

	int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
	if (sock < 0) {
		printf("Unable to create socket: errno %d\n", errno);
		return;
	}
	if (bind_local) {
#if LWIP_IPV6
		if (is_ipv6) {
			struct sockaddr_in6 *bind_addr_ip6 = (struct sockaddr_in6 *)&bind_addr;
			bind_addr_ip6->sin6_len = sizeof(struct sockaddr_in6);
			bzero(&bind_addr_ip6->sin6_addr.__in6_u, sizeof(bind_addr_ip6->sin6_addr.__in6_u));
			bind_addr_ip6->sin6_family = AF_INET6;
			bind_addr_ip6->sin6_port = htons(localport);
			bind_addr_ip6->sin6_scope_id = 0;
		} else
#endif /* LWIP_IPV6 */
		{
			struct sockaddr_in *bind_addr_ip4 = (struct sockaddr_in *)&bind_addr;
			bind_addr_ip4->sin_len = sizeof(struct sockaddr_in);
			bind_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
			bind_addr_ip4->sin_family = AF_INET;
			bind_addr_ip4->sin_port = htons(localport);
		}

		err = bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
		if (err != 0) {
			printf("Socket unable to bind: errno %d\n", errno);
			goto CLEAN_UP;
		}
		printf("Bind to local port %u\n", localport);
	}

	printf("Socket created, connecting to %s:%d\n", server_ip, port);

	err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in));
	if (err != 0) {
		printf("Socket unable to connect: errno %d\n", errno);
		goto CLEAN_UP;
	}
	printf("Successfully connected\n");

	err = send(sock, payload, strlen(payload), 0);
	if (err < 0) {
		printf("Error occurred during sending: errno %d\n", errno);
		goto CLEAN_UP;
	}

CLEAN_UP:
	if (sock != -1) {
		printf("Shutting down socket and restarting...\n");
		shutdown(sock, 0);
		close(sock);
	}
}

int
do_nettool_tcpclient(int argc, char **argv)
{
	uint16_t localport = 0;
	if ((argc != 3) && (argc != 4)) {
		return CMD_RET_FAILURE;
	}
	if (argc == 4) {
		localport = atoi(argv[3]);
	}

	tcp_client(argv[1], atoi(argv[2]), (argc == 4), localport);

	return CMD_RET_SUCCESS;
}
/**************************************************************************/
int
do_nettool_udpserver(int argc, char **argv)
{
	int ret = CMD_RET_FAILURE;
	osThreadAttr_t attr = {
		.name 		= "udp_server",
		.stack_size = 1024 * 4,
		.priority 	= osPriorityNormal,
	};

	if (argc != 3) {
		goto done;
	}
	udp_port = atoi(argv[1]);
	udp_timeout = atoi(argv[2]);

	if ((udp_timeout == 0) || (udp_timeout > 300)) {
		printf("udp close timeout should be 1-300\n");
		goto done;
	}
#if LWIP_IPV4
	if (osThreadNew(udp_server_task, (void*)AF_INET, &attr) == NULL) {
		printf("%s: failed to create udp server task\n", __func__);
		goto done;
	}
#endif
#if LWIP_IPV6
	osDelay(100);
	if (osThreadNew(udp_server_task, (void*)AF_INET6, &attr) == NULL) {
		printf("%s: failed to create udp server task\n", __func__);
		goto done;
	}
#endif

	ret = CMD_RET_SUCCESS;
done:
	return ret;
}

void udp_client(const char *server_ip, uint16_t port, uint8_t bind_local, uint16_t localport)
{
	int err;
	int addr_family = 0;
	int ip_protocol = 0;
	struct sockaddr_storage dest_addr;
	struct sockaddr_storage bind_addr;

#if LWIP_IPV6
	ip_addr_t addr;
	int is_ipv6 = 0;
	if (ipaddr_aton(server_ip, &addr) && IP_IS_V6(&addr) &&
		!ip6_addr_isipv4mappedipv6(ip_2_ip6(&addr))) {
		struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
		is_ipv6 = 1;
		dest_addr_ip6->sin6_len = sizeof(struct sockaddr_in6);
		inet6_aton(server_ip, &dest_addr_ip6->sin6_addr);
		dest_addr_ip6->sin6_family = AF_INET6;
		dest_addr_ip6->sin6_port = htons(port);
		dest_addr_ip6->sin6_scope_id = 0;
		addr_family = AF_INET6;
		ip_protocol = IPPROTO_IPV6;
	} else
#endif /* LWIP_IPV6 */
	{
		struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
		dest_addr_ip4->sin_len = sizeof(struct sockaddr_in);
		dest_addr_ip4->sin_addr.s_addr = inet_addr(server_ip);
		if (dest_addr_ip4->sin_addr.s_addr == INADDR_NONE) {
			printf("Invalid IP %s\n", server_ip);
			return;
		}
		dest_addr_ip4->sin_family = AF_INET;
		dest_addr_ip4->sin_port = htons(port);
		addr_family = AF_INET;
		ip_protocol = IPPROTO_IP;
	}

	int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
	if (sock < 0) {
		printf("Unable to create socket: errno %d\n", errno);
		return;
	}
	if (bind_local) {
#if LWIP_IPV6
		if (is_ipv6) {
			struct sockaddr_in6 *bind_addr_ip6 = (struct sockaddr_in6 *)&bind_addr;
			bind_addr_ip6->sin6_len = sizeof(struct sockaddr_in6);
			bzero(&bind_addr_ip6->sin6_addr.__in6_u, sizeof(bind_addr_ip6->sin6_addr.__in6_u));
			bind_addr_ip6->sin6_family = AF_INET6;
			bind_addr_ip6->sin6_port = htons(localport);
			bind_addr_ip6->sin6_scope_id = 0;
		} else
#endif /* LWIP_IPV6 */
		{
			struct sockaddr_in *bind_addr_ip4 = (struct sockaddr_in *)&bind_addr;
			bind_addr_ip4->sin_len = sizeof(struct sockaddr_in);
			bind_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
			bind_addr_ip4->sin_family = AF_INET;
			bind_addr_ip4->sin_port = htons(localport);
		}

		err = bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
		if (err != 0) {
			printf("Socket unable to bind: errno %d\n", errno);
			goto CLEAN_UP;
		}
		printf("Bind to local port %u\n", localport);
	}

	printf("Socket created, sending to %s:%d\n", server_ip, port);

	err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (err < 0) {
		printf("Error occurred during sending: errno %d\n", errno);
		goto CLEAN_UP;
	}
	printf("Message sent\n");

CLEAN_UP:
	if (sock != -1) {
		printf("Shutting down socket and restarting...\n");
		shutdown(sock, 0);
		close(sock);
	}
}

int
do_nettool_udpclient(int argc, char **argv)
{
	uint16_t localport = 0;
	if ((argc != 3) && (argc != 4)) {
		return CMD_RET_FAILURE;
	}
	if (argc == 4) {
		localport = atoi(argv[3]);
	}

	udp_client(argv[1], atoi(argv[2]), (argc == 4), localport);

	return CMD_RET_SUCCESS;
}
/**************************************************************************/
int
do_nettool_mc(int argc, char **argv)
{
	osThreadAttr_t attr = {
		.name 		= "mcast_task",
		.stack_size = 2048,
		.priority 	= osPriorityNormal,
	};

	if (osThreadNew(mcast_example_task, NULL, &attr) == NULL) {
		printf("%s: failed to create mcast_task\n", __func__);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

/**************************************************************************/

static const struct cli_cmd nettool_cmd[] = {
	CMDENTRY(tcp_server, do_nettool_tcpserver, "<localport> <timeout>[1-300sec]", ""),
	CMDENTRY(udp_server, do_nettool_udpserver, "<localport> <timeout>[1-300sec]", ""),
	CMDENTRY(tcp_client, do_nettool_tcpclient, "<serverip> <serverport> [localport]", ""),
	CMDENTRY(udp_client, do_nettool_udpclient, "<serverip> <serverport> [localport]", ""),
	CMDENTRY(mc, do_nettool_mc, "", ""),
};

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
static int do_nettool(int argc, char *argv[])
{
	int ret;
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], nettool_cmd, ARRAY_SIZE(nettool_cmd));
	if (cmd == NULL) {
		printf("fail to find CLI command\n");
		return CMD_RET_USAGE;
	}

	ret = cmd->handler(argc, argv);
	if (ret != CMD_RET_SUCCESS) {
		printf("%s FAIL (%d)\n", cmd->name,  ret);
		printf("Usage: %s %s %s\n", "nettool", cmd->name, cmd->usage);

		if (cmd->desc != NULL)
			printf("ex: %s %s %s\n", "nettool", cmd->name, cmd->desc);
	}
	return ret;
}

CMD(nettool, do_nettool,
	"CLI commands for NET Tool",
	"nettool tcp_server" OR
	"nettool tcp_client" OR
	"nettool udp_server" OR
	"nettool udp_client" OR
	"nettool mc"
);
#endif

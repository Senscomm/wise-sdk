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

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "hal/kernel.h"
#include "hal/timer.h"

#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/ip6.h"


#if LWIP_RAW == 0
#error "LWIP_RAW should be defined to be 1"
#endif

/** ping identifier - must fit on a u16_t */
#ifndef PING_ID
#define PING_ID        0xAFAF
#endif

#define TT(x) (x)/1000, (x) % 1000

struct ping_context {
	u16 seqno;

	/* Statistics */
	unsigned start;
	unsigned end;
	unsigned sent;
	unsigned received;
	struct {
		unsigned min;
		unsigned max;
		unsigned avg;
	} rtt;
};

/** Prepare a echo ICMP request */
static void
ping_prepare_echo( struct icmp_echo_hdr *iecho, u16_t len, u16 seqno, bool ipv6)
{
	size_t i;
	size_t data_len = len - sizeof(struct icmp_echo_hdr);
#if LWIP_IPV6
	int icmp_type_req = (ipv6 == true) ? ICMP6_TYPE_EREQ : ICMP_ECHO;
#else
	int icmp_type_req = ICMP_ECHO;
#endif

	ICMPH_TYPE_SET(iecho, icmp_type_req);
	ICMPH_CODE_SET(iecho, 0);
	iecho->chksum = 0;
	iecho->id     = PING_ID;
	iecho->seqno  = lwip_htons(seqno);

	/* fill the additional data buffer with some data */
	for(i = 0; i < data_len; i++) {
		((char*)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)i;
	}

	/* icmpv6 does not need this checksum */
	if (!ipv6)
		iecho->chksum = inet_chksum(iecho, len);
}

static void *xsin_addr(void *ptr)
{
	struct sockaddr *sa = ptr;

	return (sa->sa_family == AF_INET) ?
		ptr + offsetof(struct sockaddr_in, sin_addr) :
#if LWIP_IPV6
		ptr + offsetof(struct sockaddr_in6, sin6_addr)
#else
		ptr
#endif
		;

}

static struct addrinfo *resolv_hostname(const char *hostname,
					const char *service,
					int family, int socktype,
					int flags)
{
	struct addrinfo hint = {
		.ai_flags = AI_CANONNAME | flags,
		.ai_family = family,
		.ai_socktype = socktype,
	};
	struct addrinfo *ai;
	int ret;

	ret = getaddrinfo(hostname, service, &hint, &ai);
	if (ret != 0)
		return NULL;

	return ai;
}

#define HASH_TABLE_SIZE 6

static int ping_hash_function(int sequence)
{
    return sequence % HASH_TABLE_SIZE;
}

struct ping_hash_ctx_t {
	u16 seqno;
	unsigned sent_tick;
	bool replied;
};

static void dump_ping_statistics(struct ping_context *ctx, char *hostname, struct ping_hash_ctx_t *ping_hash_ctx)
{
	u8 i, j, min_idx;
	u16 seqno;
	bool replied;
	/* here we should dump all the left un-replied seqno
	* and seqno in the hash table need to be sorted for dumping
	*/
	for (i = 0; i < HASH_TABLE_SIZE; i++) {
		min_idx = i;
		for (j = i + 1; j < HASH_TABLE_SIZE; j++) {
			if (ping_hash_ctx[min_idx].seqno > ping_hash_ctx[j].seqno)
				min_idx = j;
		}
		/* swap the seqno and replied info according to the sorted result */
		if (min_idx != i) {
			seqno = ping_hash_ctx[i].seqno;
			replied = ping_hash_ctx[i].replied;
			ping_hash_ctx[i].seqno = ping_hash_ctx[min_idx].seqno;
			ping_hash_ctx[i].replied = ping_hash_ctx[min_idx].replied;
			ping_hash_ctx[min_idx].seqno = seqno;
			ping_hash_ctx[min_idx].replied = replied;
		}
		if ((ping_hash_ctx[i].replied == false) && (ping_hash_ctx[i].seqno != 0))
			printf("Request timeout: icmp_seq %d timeout\n", ping_hash_ctx[i].seqno);
	}
	printf("\n--- %s ping statistics ---\n", hostname);
	printf("%d packets transmitted, %d received, %f%% packet loss, time %u ms\n",
		   ctx->sent, ctx->received,
		   ctx->sent > 0 ?
		   ((float)ctx->sent-(float)ctx->received)*100/ctx->sent: 0,
		   tick_to_us(ctx->end - ctx->start)/1000);

	printf("rtt min/avg/max = %u.%03u/%u.%03u/%u.%03u ms\n",
		   TT(ctx->rtt.min),
		   TT(ctx->rtt.avg),
		   TT(ctx->rtt.max));
}

#include "cli.h"

static int ping(int argc, char *argv[])
{
	struct sockaddr_storage from;
	struct addrinfo *ai = NULL;
	struct timeval timeout;
	struct icmp_echo_hdr *iecho = NULL;	/* icmpv4 and icmpv6 have exactly the same format of hdr */
	char *hostname, *buf = NULL;
#if LWIP_IPV6
	char dst[INET6_ADDRSTRLEN];
#else
	char dst[INET_ADDRSTRLEN];
#endif
	int opt, cnt = 0, flood = 0, interval = 1000;
	int quiet = 0, ttl = 0, retry = 0;
	int size = sizeof(*iecho) + 56;
	int family = AF_INET, proto = IPPROTO_ICMP;
	int sock = -1, ret;
	double to;
	char *ep;
	int pause;
	int c = -1;
	unsigned send_delay = 0, stamp = 0;
	struct ping_hash_ctx_t ping_hash_ctx[HASH_TABLE_SIZE] = {0};
	ip_addr_t addr __maybe_unused;
	bool is_ipv6 = false;
	int hdr_offset = 0, echo_type_resp = 0;

	struct ping_context ctx = {
		.seqno = 0,
		.sent = 0,
		.received = 0,
		.rtt = { .min = INT_MAX, .max = 0, .avg = 0,},
	};

	optind = 0;
	while ((opt = getopt(argc, argv, "46c:fi:I:qs:t:W:w:r")) != -1) {
		switch (opt) {
		case '4':
			break;
		case '6':
			family = AF_INET6;
			proto = IP6_NEXTH_ICMP6;
			is_ipv6 = true;
			break;
		case 'c':
			cnt = atoi(optarg);
			break;
		case 'f':
			flood = 1;
			break;
		case 'i':
			to = strtod(optarg, &ep) * 1000.0;
			if (*ep || ep == optarg || to > (double)INT_MAX)
				printf("invalid timing interval: `%s'", optarg);
			interval = (int)to;
			break;
		case 's':
			size = atoi(optarg) + sizeof(*iecho);
			break;
		case 't':
			ttl = atoi(optarg);
			break;
		case 'q':
			quiet = 1;
            break;
		case 'r':
			retry = 1;
		}
	}

	if (optind >= argc) {
		return CMD_RET_USAGE;
	}

	hostname = argv[optind];

#if LWIP_IPV6
	if (ipaddr_aton(hostname, &addr) && IP_IS_V6(&addr) &&
		!ip6_addr_isipv4mappedipv6(ip_2_ip6(&addr))) {
		printf("ICMP6\n");
		family = AF_INET6;
		proto = IP6_NEXTH_ICMP6;
		is_ipv6 = true;
	}
#endif /* LWIP_IPV6 */

	ai = resolv_hostname(hostname, NULL, family, 0, 0);
	if (ai == NULL ||
		inet_ntop(ai->ai_family, xsin_addr(ai->ai_addr), dst, sizeof(dst)) < 0) {
		printf("Unknown host %s\n", hostname);
		goto fail;
	}

	/* Buffers */
	iecho = malloc(size);
	if (iecho == NULL) {
		printf("failed to allocate ping echo buffer\n");
		goto fail;
	}
	buf = malloc(size);
	if (buf == NULL) {
		printf("failed to allocate receive buffer\n");
		goto fail;
	}

	/* Socket */
	sock = socket(family, SOCK_RAW, proto);
	if (sock < 0) {
		printf("socket error\n");
		goto fail;
	}

	if (flood) {
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;
	} else {
		/* here we should make sure the timeout value set to RCVTIMEO is not 0,
		 * otherwise the `recvfrom` function will be blocked forever
		 */
		if (interval == 0) interval = 1000;
		timeout.tv_sec = interval / 1000;
		timeout.tv_usec = (interval % 1000) * 1000;
	}
	ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	if (ret < 0) {
		printf("setsokopt(SO_RCVTIMEO) error\n");
		goto fail;
	}
	if (ttl > 0 && family == AF_INET) {
		ret = setsockopt(sock, proto, IP_TTL, &ttl, sizeof(ttl));
		if (ret < 0) {
			printf("setsokopt(TTL) error\n");
			goto fail;
		}
	}

	printf("PING %s (%s) %d(%d) bytes of data.\n", hostname, dst,
		   (int) (size - sizeof(*iecho)),
		   (int) (size - sizeof(*iecho) + 28));

	ctx.seqno = 0;
	ctx.start = hal_timer_value();

	do {
		int len;
		long long fromlen = sizeof(from);

		struct ip_hdr *iphdr;
		struct icmp_echo_hdr *echo_reply;
		unsigned t, rtt;

		t = hal_timer_value();
		/* for the normal case, send_delay would be 0, but for the case of noisy env, we need to delay
		 * some time to make sending echo much slower
		 */
		if ((send_delay > 0) && tick_to_us(t - stamp)/1000 < send_delay) {
			goto wait_reply;
		} else {
			send_delay = 0;
			stamp = 0;
		}

		ping_prepare_echo(iecho, (u16_t) size, ++ctx.seqno, is_ipv6);
		int idx = ping_hash_function(ctx.seqno);
		ping_hash_ctx[idx].sent_tick = t;
		/* if a new request field will be filled but orininal one without replied, the original request timeout */
		if (ctx.seqno > HASH_TABLE_SIZE && !ping_hash_ctx[idx].replied)
			printf("Request timeout: icmp_seq %d timeout\n", ping_hash_ctx[idx].seqno);
		else
			ping_hash_ctx[idx].replied = false;
		ping_hash_ctx[idx].seqno = ctx.seqno;

		ret = sendto(sock, iecho, size, 0, (void *) ai->ai_addr, ai->ai_addrlen);
		if (ret < 0) {
			printf("sendto error: %d, %s\n", errno, strerror(errno));
            if (retry) {
			    pause = 1000;
			    continue;
            }
			/* link layer issue happens if ping request has been successfully sent,
			 * in this case, we should still output the ping statistics info
			 */
			if (ctx.sent > 0) {
				dump_ping_statistics(&ctx, hostname, ping_hash_ctx);
			}
			goto fail;
		}

		ctx.sent++;

wait_reply:
		len = recvfrom(sock, buf, size, 0, (struct sockaddr *) &from,
					   (socklen_t *) &fromlen);

		if (len < 0 || len < sizeof(struct ip_hdr) + sizeof(struct icmp_echo_hdr) ||
			inet_ntop(from.ss_family, xsin_addr(&from), dst, sizeof(dst)) < 0) {
			pause = 10;
			if (cnt > 0 && ctx.sent >= cnt)
				break;
			else
				continue;
		}

#if LWIP_IPV6
		if (is_ipv6) {
			hdr_offset = IP6_HLEN;
			echo_type_resp = ICMP6_TYPE_EREP;
		} else
#endif
		{
			iphdr = (struct ip_hdr *)buf;
			hdr_offset = IPH_HL(iphdr) * 4;
			echo_type_resp = ICMP_ER;
		}

		echo_reply = (struct icmp_echo_hdr *)(buf + hdr_offset);
		int idx_reply = ping_hash_function(ntohs(echo_reply->seqno));
		if ((ICMPH_TYPE(echo_reply) != echo_type_resp)
			/* If the delay of an echo reply exceeds `HASH_TABLE_SIZE` seconds (i.e., sent_seqno - received_seqno >= HASH_TABLE_SIZE),
			 * it indicates that the packet has been delayed for at least `HASH_TABLE_SIZE` seconds and a hash collision might have occurred.
			 * Due to such collisions, the hash index of the current sent sequence number could match that of the stale echo reply,
			 * leading to inaccurate Round-Trip Time (RTT) calculations.
			 * To address this issue, two strategies are considered:
			 * 1. Ignore echo replies with excessive delays.
			 * 2. Increase the size of the hash table.
			 * Here, we opt to disregard echo replies that have been delayed for more than `HASH_TABLE_SIZE` seconds,
			 * ensuring the accuracy of RTT computations.
			 */
			|| (ctx.seqno - ntohs(echo_reply->seqno) >= HASH_TABLE_SIZE)
			/* If the flag is TRUE, this indicates a duplicate ICMP sequence echo reply,
			 * in this case, ignore the packet to prevent double counting.
			 */
			|| (ping_hash_ctx[idx_reply].replied == true)) {
			/* XXX: Still need to check timeout, i.e., interval.
			 *      Otherwise, it will never send another ping request
			 *      even after the previous ping request has been lost
			 *      while the other ping in the opposite direction keeps
			 *      feeding ping requests.
			 */
			unsigned elapsed = tick_to_us(hal_timer_value() - t);
			if (elapsed > interval * 1000) {
				pause = 10;
				continue;
			}
			goto wait_reply;
		}

		ping_hash_ctx[idx_reply].replied = true;

		rtt = tick_to_us(hal_timer_value() - ping_hash_ctx[idx_reply].sent_tick); /* microsecond */
		ctx.rtt.min = min(ctx.rtt.min, rtt);
		ctx.rtt.max = max(ctx.rtt.max, rtt);
		ctx.rtt.avg += rtt;

		ctx.received++;

		if (!quiet) {
			if (!flood)
#if LWIP_IPV6
				printf("%d bytes from %s (%s): icmp_seq=%d  time=%u.%03u ms\n",
					   len,
					   ai->ai_canonname, dst,
					   ntohs(echo_reply->seqno),
					   TT(rtt));
#else
				printf("%d bytes from %s (%s): icmp_seq=%d ttl=%d time=%u.%03u ms\n",
					   len,
					   ai->ai_canonname, dst,
					   ntohs(echo_reply->seqno), iphdr->_ttl,
					   TT(rtt));
#endif
		}

		if (cnt > 0 && ctx.sent >= cnt)
			break;

		pause = (interval - (int)rtt / 1000);
		if (pause <= 0)
		       pause = 10;

		/* set send_delay according to the rtt, which indicates the current env clean or not */
		if (send_delay == 0) {
			send_delay = (interval - (int)rtt / 1000) < 0 ? rtt/1000 : 0;
			stamp = hal_timer_value();
		}
	} while ((c = getchar_timeout(pause)) < 0);

	ctx.end = hal_timer_value();
	ctx.rtt.avg /= ctx.sent;

	dump_ping_statistics(&ctx, hostname, ping_hash_ctx);

	ret = CMD_RET_SUCCESS;
	goto out;

 fail:
	ret = CMD_RET_FAILURE;
 out:
	if (iecho)
		free(iecho);
	if (buf)
		free(buf);
	if (ai)
		freeaddrinfo(ai);
	if (sock >= 0)
		close(sock);
	if (c >= 0)
		ungetc(c, stdin);

	return ret;
}



CMD(ping, ping,
	"send ICMP ECHO_REQUEST to network hosts",
	"ping [OPTIONS] destination\n"
	"Options:\n"
    "-4, -6      Force IPv4 or IPv6\n"
    "-c CNT      Send CNT many packets (default 0, 0 = infinite)\n"
    "-f          Flood (. on send, backspace on receive, to show packet drops)\n"
    "-i TIME     Interval between packets (default 1, need root for < .2)\n"
#if 0
    "-I IFACE/IP Source interface or address\n"
#endif
    "-q          Quiet (stops after one returns true if host is alive)\n"
    "-s SIZE     Data SIZE in bytes (default 56)\n"
    "-t TTL      Set Time To Live (IPv4 only)\n"
#if 0
    "-W SEC      Seconds to wait for response after -c (default 10)\n"
    "-w SEC      Exit after this many seconds"
#endif
	);

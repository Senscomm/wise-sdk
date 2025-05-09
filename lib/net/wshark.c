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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ethernet.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <hal/types.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <freebsd/if_types.h>
#include <freebsd/if_dl.h>

#include <common/ieee802_11_defs.h>
#include <common/ieee802_11_common.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_radiotap.h>

#include <cmsis_os.h>

#define __TYPES_H__
#include <hal/unaligned.h>

char *ieee80211_frame_type(u16 fc)
{
	u16 stype = WLAN_FC_GET_STYPE(fc);

#define FC2STR(x) case WLAN_FC_STYPE_##x: return #x;

	switch (WLAN_FC_GET_TYPE(fc)) {
	case WLAN_FC_TYPE_MGMT:
		switch (stype) {
		FC2STR(ASSOC_REQ);
		FC2STR(ASSOC_RESP);
		FC2STR(REASSOC_REQ);
		FC2STR(REASSOC_RESP)
		FC2STR(PROBE_REQ);
		FC2STR(PROBE_RESP);
		FC2STR(BEACON);
		FC2STR(ATIM);
		FC2STR(DISASSOC);
		FC2STR(AUTH);
		FC2STR(DEAUTH);
		FC2STR(ACTION);
		}
		break;
	case WLAN_FC_TYPE_CTRL:
		switch (stype) {
		FC2STR(PSPOLL);
		FC2STR(RTS);
		FC2STR(CTS);
		FC2STR(ACK);
		FC2STR(CFEND);
		FC2STR(CFENDACK);
		}
		break;
	case WLAN_FC_TYPE_DATA:
		switch (stype) {
		FC2STR(DATA);
		FC2STR(DATA_CFACK);
		FC2STR(DATA_CFPOLL);
		FC2STR(DATA_CFACKPOLL);
		FC2STR(NULLFUNC);
		FC2STR(CFACK);
		FC2STR(CFPOLL);
		FC2STR(CFACKPOLL);
		FC2STR(QOS_DATA);
		FC2STR(QOS_DATA_CFACK);
		FC2STR(QOS_DATA_CFPOLL);
		FC2STR(QOS_DATA_CFACKPOLL);
		FC2STR(QOS_NULL);
		FC2STR(QOS_CFPOLL);
		FC2STR(QOS_CFACKPOLL);
		}
		break;
	}
	return "Unknown";
#undef FC2STR
}

#ifndef roundup2
#define roundup2(x, y)	(((x) + ((y) - 1)) & (~((y) - 1)))
#endif /* roundup2 */

/*
 * Return the offset of the specified item in the radiotap
 * header description.  If the item is not present or is not
 * known -1 is returned.
 */
static int
radiotap_offset(struct ieee80211_radiotap_header *rh, int item)
{
	static const struct {
		size_t	align, width;
	} items[] = {
		[IEEE80211_RADIOTAP_TSFT] = {
			.align	= sizeof(uint64_t),
			.width	= sizeof(uint64_t),
		},
		[IEEE80211_RADIOTAP_FLAGS] = {
			.align	= sizeof(uint8_t),
			.width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_RATE] = {
			.align	= sizeof(uint8_t),
			.width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_CHANNEL] = {
			.align	= sizeof(uint16_t),
			.width	= 2*sizeof(uint16_t),
		},
		[IEEE80211_RADIOTAP_FHSS] = {
			.align	= sizeof(uint16_t),
			.width	= sizeof(uint16_t),
		},
		[IEEE80211_RADIOTAP_DBM_ANTSIGNAL] = {
			.align	= sizeof(uint8_t),
			.width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_DBM_ANTNOISE] = {
			.align	= sizeof(uint8_t),
			.width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_LOCK_QUALITY] = {
			.align	= sizeof(uint16_t),
			.width	= sizeof(uint16_t),
		},
		[IEEE80211_RADIOTAP_TX_ATTENUATION] = {
			.align	= sizeof(uint16_t),
			.width	= sizeof(uint16_t),
		},
		[IEEE80211_RADIOTAP_DB_TX_ATTENUATION] = {
			.align	= sizeof(uint16_t),
			.width	= sizeof(uint16_t),
		},
		[IEEE80211_RADIOTAP_DBM_TX_POWER] = {
			.align	= sizeof(uint8_t),
			.width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_ANTENNA] = {
			.align	= sizeof(uint8_t),
			.width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_DB_ANTSIGNAL] = {
			.align	= sizeof(uint8_t),
			.width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_DB_ANTNOISE] = {
			.align	= sizeof(uint8_t),
			.width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_RX_FLAGS] = {
			.align = sizeof(uint16_t),
			.width = sizeof(uint16_t),
		},
		[IEEE80211_RADIOTAP_MCS] = {
			.align = sizeof(uint8_t),
			.width = 3 * sizeof(uint8_t),
		},
	};
	uint32_t present = le32toh(rh->it_present);
	int off, i;

	off = sizeof(struct ieee80211_radiotap_header);
	for (i = 0; i < IEEE80211_RADIOTAP_EXT; i++) {
		if ((present & (1<<i)) == 0)
			continue;
		if (items[i].align == 0) {
			/* NB: unidentified element, don't guess */
			printf("%s: unknown item %d\n", __func__, i);
			return -1;
		}
		off = roundup2(off, items[i].align);
		if (i == item) {
			if (off + items[i].width > le16toh(rh->it_len)) {
				/* NB: item does not fit in header data */
				printf("%s: item %d not in header data, "
				       "off %d width %zu len %d\n", __func__, i,
				       off, items[i].width, le16toh(rh->it_len));
				return -1;
			}
			return off;
		}
		off += items[i].width;
	}
	return -1;
}

static void *radiotap_item(struct ieee80211_radiotap_header *rh, int item)
{
	int offset = radiotap_offset(rh, item);

	if (offset == -1)
		return NULL;
	return ((void *)rh) + offset;
}

static uint16_t ieee80211_ht_mcs2rate(uint8_t mcs)
{
	switch (mcs) {
	case 0: return 6500;
	case 1: return 1300;
	case 2: return 19500;
	case 3: return 26000;
	case 4: return 39000;
	case 5: return 52000;
	case 6: return 58500;
	case 7: return 65000;
	default: return 6500;
	}
}


static int num_pkt = 0;

static void ieee80211_dump_frame(unsigned char *buf, int len)
{
	struct ieee80211_radiotap_header *rh;
	struct ieee80211_hdr *hdr;
	uint16_t fc, type, stype, seq, rhlen, freq = 0;
	uint8_t *pos, mcs = 0;
	int rate = 0, crc_bad = 0, bandwidth = 0;
	int rssi = -100;
	struct timeval tv;

	gettimeofday(&tv, NULL);

	rh = (struct ieee80211_radiotap_header *) buf;
	rhlen = ieee80211_get_radiotap_len((void *) rh);

	if ((pos = radiotap_item(rh, IEEE80211_RADIOTAP_FLAGS))) {
		crc_bad = *pos & IEEE80211_RADIOTAP_F_BADFCS;
	}
	if ((pos = radiotap_item(rh, IEEE80211_RADIOTAP_CHANNEL))) {
		freq = get_unaligned_le16(pos);
	}
	if ((pos = radiotap_item(rh, IEEE80211_RADIOTAP_RATE))) {
		rate = pos[0] * 500; /* Kbps */
	} else if ((pos = radiotap_item(rh, IEEE80211_RADIOTAP_MCS))) {
		if (pos[0] & IEEE80211_RADIOTAP_MCS_HAVE_BW)
			bandwidth = pos[1] & 0x3;
#if 0
		if (pos[0] & IEEE80211_RADIOTAP_MCS_HAVE_GI)
			sgi = (pos[1] >> 2) & 1;
#endif
		if (pos[0] & IEEE80211_RADIOTAP_MCS_HAVE_MCS)
			mcs = pos[2];
	}
	if ((pos = radiotap_item(rh, IEEE80211_RADIOTAP_DBM_ANTSIGNAL))) {
		if (pos[0] & 0x80)
			rssi = pos[0] - 256;
		else
			rssi = pos[0];
	}

	hdr = (struct ieee80211_hdr *) (buf + rhlen);
	fc = get_unaligned_le16(&hdr->frame_control);
	seq = get_unaligned_le16(&hdr->seq_ctrl);
	type = WLAN_FC_GET_TYPE(fc);
	stype = WLAN_FC_GET_STYPE(fc);

	printf("%-6d %6d.%06d ", num_pkt++, tv.tv_sec, tv.tv_usec);
	printf(MACSTR" "MACSTR,  MAC2STR(hdr->addr1), MAC2STR(hdr->addr2));

	if (!rate)
		rate = ieee80211_ht_mcs2rate(mcs);

	printf(" %2d.%1d %2d %4hhd", rate/1000, (rate%1000)/100,
	       (bandwidth + 1) * 20, rssi);

	printf(" %4d %4d.%-2d %6d .%c%c%c%c%c%c%c%c  %s",
	       len,
		   IEEE80211_HAS_SEQ(type<<2, stype<<4) ? WLAN_GET_SEQ_SEQ(seq) : 0,
           IEEE80211_HAS_SEQ(type<<2, stype<<4) ? WLAN_GET_SEQ_FRAG(seq) : 0,
           freq,
	       (fc & WLAN_FC_ISWEP) ?    'p' : '.',
	       (fc & WLAN_FC_MOREFRAG) ? 'm' : '.',
	       (fc & WLAN_FC_PWRMGT) ?   'P' : '.',
	       (fc & WLAN_FC_RETRY) ?    'R' : '.',
	       (fc & WLAN_FC_MOREDATA) ? 'M' : '.',
	       (fc & WLAN_FC_FROMDS) ?   'F' : '.',
	       (fc & WLAN_FC_TODS) ?     'T' : '.',
	       crc_bad?                  'C' : '.',
	       ieee80211_frame_type(fc));

	if (crc_bad)
		goto out;

	if (type == WLAN_FC_TYPE_MGMT) {
		struct ieee80211_mgmt *mgmt = (void *) hdr;
		struct ieee802_11_elems elems;

		switch (stype) {
		case WLAN_FC_STYPE_BEACON:
			len -= (offsetof(struct ieee80211_mgmt, u.beacon.variable) + rhlen);
			ieee802_11_parse_elems(mgmt->u.beacon.variable,
					       len, &elems, 0);
			if (elems.ssid) {
				char ssid[SSID_MAX_LEN+1];
				memcpy(ssid, elems.ssid, elems.ssid_len);
				ssid[elems.ssid_len] = '\0';
				printf(" (ssid=%s)", ssid);
			}
		}
	}
 out:
	printf("\n");
}

#include <cli.h>

typedef struct wshark_ctx {
	osThreadId_t ws_tid;
	char ifname[32];
	bool daemon;
	bool exit;
} wshark_ctx_t;

static wshark_ctx_t ws_ctx = {0,};

void wshark_loop(void *arg)
{
	int sock = -1;
	int maxfds, ret = -1;
	struct sockaddr_dl ll;
	fd_set rdfds;
	unsigned char *buf = NULL;
	wshark_ctx_t *ctx = arg;
	int wait_ms = 1000;
	struct timeval tv;

	sock = socket(AF_LINK, SOCK_RAW, htons(ETH_P_ALL));
	if (sock < 0) {
		printf("socket: %s\n", strerror(errno));
		goto out;
	}

	memset(&ll, 0, sizeof(ll));
	ll.sdl_family = AF_LINK;
	ll.sdl_type = IFT_ETHER;
	if ((ll.sdl_index = if_nametoindex(ctx->ifname)) == 0) {
		printf("if_nametoindex: %s\n", errno);
		goto out;
	}
	if (bind(sock, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
		printf("bind: %s\n", strerror(errno));
		goto out;
	}

	buf = malloc(2048);
	if (buf == NULL) {
		printf("wiseshar: not enough memory\n");
		goto out;
	}
	maxfds = sock + 1;
	num_pkt = 0;

	/* Heading */
	printf("\x1b[2;48r");

	printf("\x1b[1;1H\x1b[2J\x1b[7m");
	printf("%-6s %-17s %-17s %-17s %4s %2s %4s %4s %-7s %-6s %-9s  %-30s",
	       "No.", "Time", "RA", "TA", "Rate", "BW", "RSSI", "LEN", "SEQ", "FREQ", "Flags", "Info");
	printf("\x1b[0m\n");

	while (!ctx->exit) {
		FD_ZERO(&rdfds);
		if (!ctx->daemon) {
			FD_SET(STDIN_FILENO, &rdfds);
		}
		FD_SET(sock, &rdfds);

		tv.tv_sec = wait_ms / 1000;
		tv.tv_usec = (wait_ms % 1000) * 1000;
		/* Wait until any i/o takes place or timeout */
		ret = select(maxfds, &rdfds, NULL, NULL, &tv);
		if (ret <= 0)
			continue;
		if (!ctx->daemon && FD_ISSET(STDIN_FILENO, &rdfds)) {
			break;
		}
		if (FD_ISSET(sock, &rdfds)) {
			ssize_t len;
			len = recvfrom(sock, buf, 2048, 0, NULL, NULL);
			if (len < 0) {
				printf("recvfrom: %s\n", strerror(errno));
				break;
			}
			ieee80211_dump_frame(buf, len);
		}
	}

 out:
	if (sock >= 0)
		close(sock);
	if (buf)
		free(buf);

	printf("\x1b[s\x1b[;r");
	printf("\x1b[u");

	if (ctx->daemon) {
		ctx->ws_tid = NULL;
		osThreadExit();
	}
}

static int wshark(int argc, char *argv[])
{
	int c;
	char *ifname = NULL;
	bool daemon = false, kill = false;
	osThreadAttr_t attr = {
		.name = "wshark",
		.stack_size = 2048,
		.priority = osPriorityNormal,
	};
	wshark_ctx_t *ctx = &ws_ctx;

	while ((c = getopt(argc, argv, "i:dk")) != -1) {
		switch (c) {
		case 'i':
			ifname = optarg;
			break;
		case 'd':
			daemon = true;
			break;
		case 'k':
			kill = true;
			break;
		default:
			break;
		}
	}

	if (!ifname)
		return CMD_RET_USAGE;

	if (daemon && ctx->ws_tid != NULL) {
		printf("wshark daemon is already running (pid = %d)\n",
			   ctx->ws_tid);
		return CMD_RET_FAILURE;
	}

	if (kill && ctx->ws_tid == NULL) {
		printf("No wshark daemon is running\n");
		return CMD_RET_FAILURE;
	}

	if (kill) {
		ctx->exit = true;
		return CMD_RET_SUCCESS;
	}

	memset(ctx->ifname, 0, sizeof(ctx->ifname));
	strncpy(ctx->ifname, ifname, sizeof(ctx->ifname));
	ctx->exit = false;
	ctx->daemon = daemon;

	if (daemon) {
		ctx->ws_tid = osThreadNew(wshark_loop, ctx, &attr);
		if (!ctx->ws_tid) {
			printf("Could not start wshark daemon\n");
		}
	} else {
		wshark_loop(ctx);
	}

	return CMD_RET_SUCCESS;
}
CMD(wshark, wshark, "frame monitor", "wshark -i <interface> (-d | -k for daemon)");

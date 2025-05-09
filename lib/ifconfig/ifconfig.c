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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>

#include "compat_param.h"
#include "compat_if.h"
#include "if_dl.h"
#include "if_media.h"
#include "ethernet.h"
#include "route.h"
#include "libifconfig.h"
#include <cli.h>
#ifdef CONFIG_LWIP_IPV6
#include "lwip/opt.h"
#include "lwip/ip6_addr.h"
#endif

#define IFCONFIG_CALL(libfunc, ret, ...) do {				\
	ifconfig_handle_t *h = ifconfig_open();				\
	if (ifconfig_ ## libfunc(h, ifname, ##__VA_ARGS__) < 0) {	\
		ifconfig_close(h);					\
		return ret;						\
	}								\
	ifconfig_close(h);						\
} while (0)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#endif /* ARRAY_SIZE */

static const char *ifname;

#ifdef CONFIG_IFCONFIG_WIRELESS

#include <net80211/ieee80211_ioctl.h>
#include <net80211/ieee80211_wise.h>

#ifndef IEEE80211_FIXED_RATE_NONE
#define IEEE80211_FIXED_RATE_NONE	0xff
#endif
#ifndef IEEE80211_NODE_AUTH
#define	IEEE80211_NODE_AUTH	0x000001	/* authorized for data */
#define	IEEE80211_NODE_QOS	0x000002	/* QoS enabled */
#define	IEEE80211_NODE_ERP	0x000004	/* ERP enabled */
#define	IEEE80211_NODE_PWR_MGT	0x000010	/* power save mode enabled */
#define	IEEE80211_NODE_AREF	0x000020	/* authentication ref held */
#define	IEEE80211_NODE_HT	0x000040	/* HT enabled */
#define	IEEE80211_NODE_HTCOMPAT	0x000080	/* HT setup w/ vendor OUI's */
#define	IEEE80211_NODE_WPS	0x000100	/* WPS association */
#define	IEEE80211_NODE_TSN	0x000200	/* TSN association */
#define	IEEE80211_NODE_AMPDU_RX	0x000400	/* AMPDU rx enabled */
#define	IEEE80211_NODE_AMPDU_TX	0x000800	/* AMPDU tx enabled */
#define	IEEE80211_NODE_MIMO_PS	0x001000	/* MIMO power save enabled */
#define	IEEE80211_NODE_MIMO_RTS	0x002000	/* send RTS in MIMO PS */
#define	IEEE80211_NODE_RIFS	0x004000	/* RIFS enabled */
#define	IEEE80211_NODE_SGI20	0x008000	/* Short GI in HT20 enabled */
#define	IEEE80211_NODE_SGI40	0x010000	/* Short GI in HT40 enabled */
#define	IEEE80211_NODE_ASSOCID	0x020000	/* xmit requires associd */
#define	IEEE80211_NODE_AMSDU_RX	0x040000	/* AMSDU rx enabled */
#define	IEEE80211_NODE_AMSDU_TX	0x080000	/* AMSDU tx enabled */
#define	IEEE80211_NODE_VHT	0x100000	/* VHT enabled */
#endif /* IEEE80211_NODE_AUTH */

#define	MAXCHAN	(13 * 6)		/* 2GHz band (B/G/N20/N40) */
#define	MAXCOL	78
static	int col;
static	char spacer;

static void LINE_INIT(char c) __unused;
static void LINE_BREAK(void);
static void LINE_CHECK(const char *fmt, ...) __maybe_unused;

static const char *modename[IEEE80211_MODE_MAX] __maybe_unused = {
	[IEEE80211_MODE_AUTO]	  = "auto",
#ifdef CONFIG_IEEE80211_MODE_11A
	[IEEE80211_MODE_11A]	  = "11a",
#endif
	[IEEE80211_MODE_11B]	  = "11b",
	[IEEE80211_MODE_11G]	  = "11g",
#ifdef CONFIG_IEEE80211_MODE_FH
	[IEEE80211_MODE_FH]	  = "fh",
#endif
#ifdef CONFIG_IEEE80211_MODE_TURBO_A
	[IEEE80211_MODE_TURBO_A]  = "turboA",
#endif
#ifdef CONFIG_IEEE80211_MODE_TURBO_G
	[IEEE80211_MODE_TURBO_G]  = "turboG",
#endif
#ifdef CONFIG_IEEE80211_MODE_STURBO_A
	[IEEE80211_MODE_STURBO_A] = "sturbo",
#endif
#ifdef CONFIG_IEEE80211_MODE_11NA
	[IEEE80211_MODE_11NA]	  = "11na",
#endif
	[IEEE80211_MODE_11NG]	  = "11ng",
#ifdef CONFIG_IEEE80211_MODE_HALF
	[IEEE80211_MODE_HALF]	  = "half",
#endif
#ifdef CONFIG_IEEE80211_MODE_QUARTER
#ifdef CONFIG_IEEE80211_MODE_QUARTER
	[IEEE80211_MODE_QUARTER]  = "quarter",
#endif
#endif
#ifdef CONFIG_IEEE80211_MODE_VHT_2GHZ
	[IEEE80211_MODE_VHT_2GHZ] = "11acg",
#endif
#ifdef CONFIG_IEEE80211_MODE_VHT_5GHZ
	[IEEE80211_MODE_VHT_5GHZ] = "11ac",
#endif
#ifdef CONFIG_IEEE80211_MODE_HE_2GHZ
	[IEEE80211_MODE_HE_2GHZ] = "11axg",
#endif
#ifdef CONFIG_IEEE80211_MODE_HE_5GHZ
	[IEEE80211_MODE_HE_5GHZ] = "11ax",
#endif
};

static struct ieee80211req_chaninfo *chaninfo;
static struct ieee80211_txparams_req txparams;
static int gottxparams __maybe_unused = 0;
static struct ieee80211_channel curchan;
static int gotcurchan = 0;
static struct ifmediareq *ifmr = NULL;
static int htconf = 0;
static int gothtconf = 0;
static int verbose = 1;
static const char *mesh_linkstate_string(uint8_t state);

#endif /* CONFIG_IFCONFIG_WIRELESS */

static int INET_rresolve(char *, size_t, struct sockaddr_in *, int);
static void INET_reserror(char *);
static char *INET_print(unsigned char *);
static char *INET_sprint(struct sockaddr *, int);
static int INET_input(char *, struct sockaddr *);

/* This structure defines protocol families and their handlers. */
struct aftype {
	char *name;
	char *(*print)(unsigned char *);
	char *(*sprint)(struct sockaddr *, int numeric);
	int (*input)(char *bufp, struct sockaddr *);
	void (*herror)(char *text);
};

struct aftype inet_aftype = {
	"inet",
	INET_print,
	INET_sprint,
	INET_input,
	INET_reserror
};

#ifdef CONFIG_LWIP_IPV6
static char *INET6_print(unsigned char *ptr);
static char *INET6_sprint(struct sockaddr *sap, int numeric);
static int INET6_input(char *bufp, struct sockaddr *sap);
static void INET6_reserror(char *text);

struct aftype inet6_aftype = {
    "inet6",
    INET6_print,
    INET6_sprint,
    INET6_input,
    INET6_reserror
};
#endif
static char *pr_ether(unsigned char *ptr);
static int in_ether(char *bufp, struct sockaddr *sap);

/* This structure defines hardware protocols and their handlers. */
static struct hwtype {
	char *name;
	int alen;
	char *(*print) (unsigned char *);
	int (*input) (char *, struct sockaddr *);
} ether_hwtype = {
	"ether",
	ETHER_ADDR_LEN,
	pr_ether,
	in_ether
};

/* Like strncpy but make sure the resulting string is always 0 terminated. */
static char *safe_strncpy(char *dst, const char *src, size_t size)
{
	dst[size-1] = '\0';
	return strncpy(dst,src,size-1);
}

static int INET_resolve(char *name, struct sockaddr_in *sin)
{
	/* Grmpf. -FvK */
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	/* Default is special, meaning 0.0.0.0. */
	if (!strcmp(name, "default")) {
		sin->sin_addr.s_addr = INADDR_ANY;
		return (1);
	}
	/* Look to see if it's a dotted quad. */
	if (inet_aton(name, &sin->sin_addr)) {
		return 0;
	}
	return -1;
}

/* numeric: & 0x8000: default instead of *,
 *	    & 0x4000: host instead of net,
 *	    & 0x0fff: don't resolve
 */
static int INET_rresolve(char *name, size_t len, struct sockaddr_in *sin,
			 int numeric)
{
	u_int32_t ad;

	/* Grmpf. -FvK */
	if (sin->sin_family != AF_INET) {
		errno = EAFNOSUPPORT;
		return (-1);
	}
	ad = sin->sin_addr.s_addr;
	if (ad == INADDR_ANY) {
		if ((numeric & 0x0FFF) == 0) {
			if (numeric & 0x8000)
				safe_strncpy(name, "default", len);
			else
				safe_strncpy(name, "*", len);
			return (0);
		}
	}

	if (!(numeric & 0x0FFF)) {
		/* host dns - not supported */
		errno = EAFNOSUPPORT;
		return (-1);
	}

	safe_strncpy(name, inet_ntoa(sin->sin_addr), len);
	return (0);
}

static void INET_reserror(char *text)
{
	printf("%s", text);
}

/* Display an Internet socket address. */
static char *INET_print(unsigned char *ptr)
{
	return (inet_ntoa((*(struct in_addr *)ptr)));
}

/* Display an Internet socket address. */
static char *INET_sprint(struct sockaddr *sap, int numeric)
{
	static char buff[128];

	if (sap->sa_family == 0xFFFF || sap->sa_family == 0)
		return safe_strncpy(buff, "[NONE SET]", sizeof(buff));

	if (INET_rresolve(buff, sizeof(buff), (struct sockaddr_in *)sap,
			  numeric) != 0)
		return (NULL);

	return (buff);
}

static int INET_input(char *bufp, struct sockaddr *sap)
{
	return (INET_resolve(bufp, (struct sockaddr_in *) sap));
}

#ifdef CONFIG_LWIP_IPV6
#ifndef IN6_IS_ADDR_V4MAPPED
#define IN6_IS_ADDR_V4MAPPED(a) \
	((((__const uint32_t *) (a))[0] == 0) \
	&& (((__const uint32_t *) (a))[1] == 0) \
	&& (((__const uint32_t *) (a))[2] == htonl (0xffff)))
#endif

char * fix_v4_address(char *buf, struct in6_addr *in6)
{
	if (IN6_IS_ADDR_V4MAPPED(in6->s6_addr)) {
			char *s =strchr(buf, '.');
			if (s) {
				while (s > buf && *s != ':')
					--s;
				if (*s == ':') ++s;
				else s = NULL;
			}
			if (s) return s;
	}
	return buf;
}

static int INET6_resolve(char *name, struct sockaddr_in6 *sin6)
{
	struct addrinfo req, *ai;
	int s;

	memset (&req, '\0', sizeof req);
	req.ai_family = AF_INET6;
	if ((s = getaddrinfo(name, NULL, &req, &ai))) {
		fprintf(stderr, "getaddrinfo: %s: %d\n", name, s);
		return -1;
	}
	memcpy(sin6, ai->ai_addr, sizeof(struct sockaddr_in6));

	freeaddrinfo(ai);

	return (0);
}

#ifndef IN6_IS_ADDR_UNSPECIFIED
#define IN6_IS_ADDR_UNSPECIFIED(a) \
        (((uint32_t *) (a))[0] == 0 && ((uint32_t *) (a))[1] == 0 && \
         ((uint32_t *) (a))[2] == 0 && ((uint32_t *) (a))[3] == 0)
#endif

static int INET6_rresolve(char *name, struct sockaddr_in6 *sin6, int numeric)
{
	if (sin6->sin6_family != AF_INET6) {
#ifdef DEBUG
		fprintf(stderr, _("rresolve: unsupported address family %d !\n"),
			sin6->sin6_family);
#endif
		errno = EAFNOSUPPORT;
		return (-1);
	}
	if (numeric & 0x7FFF) {
		inet_ntop( AF_INET6, &sin6->sin6_addr, name, 80);
		return (0);
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		if (numeric & 0x8000)
			strcpy(name, "default");
		else
			strcpy(name, "[::]");
		return (0);
	}

	return (0);
}

/* Display an Internet socket address. */
static char *INET6_print(unsigned char *ptr)
{
	static char name[80];

	inet_ntop(AF_INET6, (struct in6_addr *) ptr, name, 80);
	return fix_v4_address(name, (struct in6_addr *)ptr);
}

/* Display an Internet socket address. */
/* dirty! struct sockaddr usually doesn't suffer for inet6 addresses, fst. */
static char *INET6_sprint(struct sockaddr *sap, int numeric)
{
	static char buff[128];

	if (sap->sa_family == 0xFFFF || sap->sa_family == 0)
		return safe_strncpy(buff, "[NONE SET]", sizeof(buff));
	if (INET6_rresolve(buff, (struct sockaddr_in6 *) sap, numeric) != 0)
		return safe_strncpy(buff, "[UNKNOWN]", sizeof(buff));
	return (fix_v4_address(buff, &((struct sockaddr_in6 *)sap)->sin6_addr));
}

static int INET6_input(char *bufp, struct sockaddr *sap)
{
	return (INET6_resolve(bufp, (struct sockaddr_in6 *) sap));
}

static void INET6_reserror(char *text)
{
	printf("%s", text);
}
#endif

static char *pr_ether(unsigned char *ptr)
{
	return ether_ntoa((const struct ether_addr *) ptr);
}

static int in_ether(char *bufp, struct sockaddr *sap)
{
	struct ether_addr *ether = ether_aton(bufp);

	if (ether) {
		memcpy(sap->sa_data, ether, sizeof(*ether));
		sap->sa_len = sizeof(*ether);
		assert(sizeof(*ether) == ETHER_ADDR_LEN);
		return 0;
	}
	return -1;
}

#ifdef CONFIG_IFCONFIG_WIRELESS

static void LINE_INIT(char c)
{
	spacer = c;
	col = (c == '\t') ? 8: 1;
}

static void LINE_BREAK(void)
{
	if (spacer != '\t') {
		printf("\n");
		spacer = '\t';
	}
	col = 8;		/* 8-col tab */
}

static void LINE_CHECK(const char *fmt, ...)
{
	char buf[80];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf+1, sizeof(buf)-1, fmt, ap);
	va_end(ap);
	col += 1+n;
	if (col > MAXCOL) {
		LINE_BREAK();
		col += n;
	}
	buf[0] = spacer;
	printf("%s", buf);
	spacer = ' ';
}

/*
 * Print a value a la the %b format of the kernel's printf
 */
void printb(const char *s, unsigned v, const char *bits)
{
	int i, any = 0;
	char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
	if (bits) {
		bits++;
		putchar('<');
		while ((i = *bits++) != '\0') {
			if (v & (1 << (i-1))) {
				if (any)
					putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					putchar(c);
			} else
				for (; *bits > 32; bits++)
					;
		}
		putchar('>');
	}
}

static
void printie(const char* tag, const uint8_t *ie, size_t ielen, int maxlen)
{
	printf("%s", tag);
	if (verbose) {
		maxlen -= strlen(tag)+2;
		if (2*ielen > maxlen)
			maxlen--;
		printf("<");
		for (; ielen > 0; ie++, ielen--) {
			if (maxlen-- <= 0)
				break;
			printf("%02x", *ie);
		}
		if (ielen != 0)
			printf("-");
		printf(">");
	}
}

#include <hal/unaligned.h>
#define LE_READ_2(p)	get_unaligned_le16(p)
#define LE_READ_4(p)	get_unaligned_le32(p)

/*
 * NB: The decoding routines assume a properly formatted ie
 *     which should be safe as the kernel only retains them
 *     if they parse ok.
 */

static void printwmeparam(const char *tag, const u_int8_t *ie, size_t ielen,
			  int maxlen)
{
#define	MS(_v, _f)	(((_v) & _f) >> _f##_S)
	static const char *acnames[] = { "BE", "BK", "VO", "VI" };
	const struct ieee80211_wme_param *wme =
	    (const struct ieee80211_wme_param *) ie;
	int i;

	printf("%s", tag);
	if (!verbose)
		return;
	printf("<qosinfo 0x%x", wme->param_qosInfo);
	ie += offsetof(struct ieee80211_wme_param, params_acParams);
	for (i = 0; i < WME_NUM_AC; i++) {
		const struct ieee80211_wme_acparams *ac;
		ac = &wme->params_acParams[i];
		printf(" %s[%saifsn %u cwmin %u cwmax %u txop %u]",
		       acnames[i],
		       MS(ac->acp_aci_aifsn, WME_PARAM_ACM) ? "acm " : "",
		       MS(ac->acp_aci_aifsn, WME_PARAM_AIFSN),
		       MS(ac->acp_logcwminmax, WME_PARAM_LOGCWMIN),
		       MS(ac->acp_logcwminmax, WME_PARAM_LOGCWMAX),
		       LE_READ_2(&ac->acp_txop));
	}
	printf(">");
#undef MS
}

static
void printwmeinfo(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
	printf("%s", tag);
	if (verbose) {
		const struct ieee80211_wme_info *wme;
		wme = (const struct ieee80211_wme_info *) ie;
		printf("<version 0x%x info 0x%x>",
		       wme->wme_version, wme->wme_info);
	}
}

static
void printvhtcap(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
	printf("%s", tag);
	if (verbose) {
		const struct ieee80211_ie_vhtcap *vhtcap =
			(const struct ieee80211_ie_vhtcap *) ie;
		uint32_t vhtcap_info = LE_READ_4(&vhtcap->vht_cap_info);

		printf("<cap 0x%08x", (unsigned int)vhtcap_info);
		printf(" rx_mcs_map 0x%x",
		       LE_READ_2(&vhtcap->supp_mcs.rx_mcs_map));
		printf(" rx_highest %d",
		       LE_READ_2(&vhtcap->supp_mcs.rx_highest) & 0x1fff);
		printf(" tx_mcs_map 0x%x",
		       LE_READ_2(&vhtcap->supp_mcs.tx_mcs_map));
		printf(" tx_highest %d",
		       LE_READ_2(&vhtcap->supp_mcs.tx_highest) & 0x1fff);
		printf(">");
	}
}

static void printvhtinfo(const char *tag, const u_int8_t *ie, size_t ielen,
			 int maxlen)
{
	printf("%s", tag);
	if (verbose) {
		const struct ieee80211_ie_vht_operation *vhtinfo;
		vhtinfo = (const struct ieee80211_ie_vht_operation *) ie;
		printf("<chw %d freq1_idx %d freq2_idx %d basic_mcs_set 0x%04x>",
		       vhtinfo->chan_width,
		       vhtinfo->center_freq_seg1_idx,
		       vhtinfo->center_freq_seg2_idx,
		       LE_READ_2(&vhtinfo->basic_mcs_set));
	}
}

static void printvhtpwrenv(const char *tag, const u_int8_t *ie, size_t ielen,
			   int maxlen)
{
	printf("%s", tag);
	static const char *txpwrmap[] = {"20", "40", "80", "160",};

	if (verbose) {
		const struct ieee80211_ie_vht_txpwrenv *vhtpwr =
		    (const struct ieee80211_ie_vht_txpwrenv *) ie;
		int i, n;
		const char *sep = "";

		/* Get count; trim at ielen */
		n = (vhtpwr->tx_info &
		    IEEE80211_VHT_TXPWRENV_INFO_COUNT_MASK) + 1;
		/* Trim at ielen */
		if (n > ielen - 3)
			n = ielen - 3;
		printf("<tx_info 0x%02x pwr:[", vhtpwr->tx_info);
		for (i = 0; i < n; i++) {
			printf("%s%s:%.2f", sep, txpwrmap[i],
			    ((float) ((int8_t) ie[i+3])) / 2.0);
			sep = " ";
		}

		printf("]>");
	}
}

static void printhtcap(const char *tag, const u_int8_t *ie, size_t ielen,
		       int maxlen)
{
	printf("%s", tag);
	if (verbose) {
		const struct ieee80211_ie_htcap *htcap =
			(const struct ieee80211_ie_htcap *) ie;
		const char *sep;
		int i, j;

		printf("<cap 0x%x param 0x%x",
		       LE_READ_2(&htcap->hc_cap), htcap->hc_param);
		printf(" mcsset[");
		sep = "";
		for (i = 0; i < IEEE80211_HTRATE_MAXSIZE; i++)
			if (isset(htcap->hc_mcsset, i)) {
				for (j = i+1; j < IEEE80211_HTRATE_MAXSIZE; j++)
					if (isclr(htcap->hc_mcsset, j))
						break;
				j--;
				if (i == j)
					printf("%s%u", sep, i);
				else
					printf("%s%u-%u", sep, i, j);
				i += j-i;
				sep = ",";
			}
		printf("] extcap 0x%x txbf 0x%x antenna 0x%x>",
		       LE_READ_2(&htcap->hc_extcap),
		       (unsigned int)LE_READ_4(&htcap->hc_txbf),
		       htcap->hc_antenna);
	}
}

static
void printhtinfo(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
	printf("%s", tag);
	if (verbose) {
		const struct ieee80211_ie_htinfo *htinfo =
			(const struct ieee80211_ie_htinfo *) ie;
		const char *sep;
		int i, j;

		printf("<ctl %u, %x,%x,%x,%x", htinfo->hi_ctrlchannel,
		       htinfo->hi_byte1, htinfo->hi_byte2, htinfo->hi_byte3,
		       LE_READ_2(&htinfo->hi_byte45));
		printf(" basicmcs[");
		sep = "";
		for (i = 0; i < IEEE80211_HTRATE_MAXSIZE; i++)
			if (isset(htinfo->hi_basicmcsset, i)) {
				for (j = i+1; j < IEEE80211_HTRATE_MAXSIZE; j++)
					if (isclr(htinfo->hi_basicmcsset, j))
						break;
				j--;
				if (i == j)
					printf("%s%u", sep, i);
				else
					printf("%s%u-%u", sep, i, j);
				i += j-i;
				sep = ",";
			}
		printf("]>");
	}
}

#if 0
static
void printathie(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{

	printf("%s", tag);
	if (verbose) {
		const struct ieee80211_ath_ie *ath =
			(const struct ieee80211_ath_ie *)ie;

		printf("<");
		if (ath->ath_capability & ATHEROS_CAP_TURBO_PRIME)
			printf("DTURBO,");
		if (ath->ath_capability & ATHEROS_CAP_COMPRESSION)
			printf("COMP,");
		if (ath->ath_capability & ATHEROS_CAP_FAST_FRAME)
			printf("FF,");
		if (ath->ath_capability & ATHEROS_CAP_XR)
			printf("XR,");
		if (ath->ath_capability & ATHEROS_CAP_AR)
			printf("AR,");
		if (ath->ath_capability & ATHEROS_CAP_BURST)
			printf("BURST,");
		if (ath->ath_capability & ATHEROS_CAP_WME)
			printf("WME,");
		if (ath->ath_capability & ATHEROS_CAP_BOOST)
			printf("BOOST,");
		printf("0x%x>", LE_READ_2(ath->ath_defkeyix));
	}
}
#endif

static
void printmeshconf(const char *tag, const uint8_t *ie, size_t ielen, int maxlen)
{
#if 0
	printf("%s", tag);
	if (verbose) {
		const struct ieee80211_meshconf_ie *mconf =
			(const struct ieee80211_meshconf_ie *)ie;
		printf("<PATH:");
		if (mconf->conf_pselid == IEEE80211_MESHCONF_PATH_HWMP)
			printf("HWMP");
		else
			printf("UNKNOWN");
		printf(" LINK:");
		if (mconf->conf_pmetid == IEEE80211_MESHCONF_METRIC_AIRTIME)
			printf("AIRTIME");
		else
			printf("UNKNOWN");
		printf(" CONGESTION:");
		if (mconf->conf_ccid == IEEE80211_MESHCONF_CC_DISABLED)
			printf("DISABLED");
		else
			printf("UNKNOWN");
		printf(" SYNC:");
		if (mconf->conf_syncid == IEEE80211_MESHCONF_SYNC_NEIGHOFF)
			printf("NEIGHOFF");
		else
			printf("UNKNOWN");
		printf(" AUTH:");
		if (mconf->conf_authid == IEEE80211_MESHCONF_AUTH_DISABLED)
			printf("DISABLED");
		else
			printf("UNKNOWN");
		printf(" FORM:0x%x CAPS:0x%x>", mconf->conf_form,
		    mconf->conf_cap);
	}
#endif
}


#if 0
static void
printbssload(const char *tag, const uint8_t *ie, size_t ielen, int maxlen)
{
	printf("%s", tag);
	if (verbose) {
		const struct ieee80211_bss_load_ie *bssload =
		    (const struct ieee80211_bss_load_ie *) ie;
		printf("<sta count %d, chan load %d, aac %d>",
		    LE_READ_2(&bssload->sta_count),
		    bssload->chan_load,
		    bssload->aac);
	}
}

static
void printapchanrep(const char *tag, const u_int8_t *ie, size_t ielen,
		    int maxlen)
{
	printf("%s", tag);
	if (verbose) {
		const struct ieee80211_ap_chan_report_ie *ap =
		    (const struct ieee80211_ap_chan_report_ie *) ie;
		const char *sep = "";
		int i;

		printf("<class %u, chan:[", ap->i_class);

		for (i = 3; i < ielen; i++) {
			printf("%s%u", sep, ie[i]);
			sep = ",";
		}
		printf("]>");
	}
}
#endif

static const char *wpa_cipher(const u_int8_t *sel)
{
#define	WPA_SEL(x)	(((x)<<24)|WPA_OUI)
	u_int32_t w = LE_READ_4(sel);

	switch (w) {
	case WPA_SEL(WPA_CSE_NULL):
		return "NONE";
	case WPA_SEL(WPA_CSE_WEP40):
		return "WEP40";
	case WPA_SEL(WPA_CSE_WEP104):
		return "WEP104";
	case WPA_SEL(WPA_CSE_TKIP):
		return "TKIP";
	case WPA_SEL(WPA_CSE_CCMP):
		return "AES-CCMP";
	}
	return "?";		/* NB: so 1<< is discarded */
#undef WPA_SEL
}

static const char *wpa_keymgmt(const u_int8_t *sel)
{
#define	WPA_SEL(x)	(((x)<<24)|WPA_OUI)
	u_int32_t w = LE_READ_4(sel);

	switch (w) {
	case WPA_SEL(WPA_ASE_8021X_UNSPEC):
		return "8021X-UNSPEC";
	case WPA_SEL(WPA_ASE_8021X_PSK):
		return "8021X-PSK";
	case WPA_SEL(WPA_ASE_NONE):
		return "NONE";
	}
	return "?";
#undef WPA_SEL
}

static
void printwpaie(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
	u_int8_t len = ie[1];

	printf("%s", tag);
	if (verbose) {
		const char *sep;
		int n;

		ie += 6, len -= 4;		/* NB: len is payload only */

		printf("<v%u", LE_READ_2(ie));
		ie += 2, len -= 2;

		printf(" mc:%s", wpa_cipher(ie));
		ie += 4, len -= 4;

		/* unicast ciphers */
		n = LE_READ_2(ie);
		ie += 2, len -= 2;
		sep = " uc:";
		for (; n > 0; n--) {
			printf("%s%s", sep, wpa_cipher(ie));
			ie += 4, len -= 4;
			sep = "+";
		}

		/* key management algorithms */
		n = LE_READ_2(ie);
		ie += 2, len -= 2;
		sep = " km:";
		for (; n > 0; n--) {
			printf("%s%s", sep, wpa_keymgmt(ie));
			ie += 4, len -= 4;
			sep = "+";
		}

		if (len > 2)		/* optional capabilities */
			printf(", caps 0x%x", LE_READ_2(ie));
		printf(">");
	}
}

static const char *rsn_cipher(const u_int8_t *sel)
{
#define	RSN_SEL(x)	(((x)<<24)|RSN_OUI)
	u_int32_t w = LE_READ_4(sel);

	switch (w) {
	case RSN_SEL(RSN_CSE_NULL):
		return "NONE";
	case RSN_SEL(RSN_CSE_WEP40):
		return "WEP40";
	case RSN_SEL(RSN_CSE_WEP104):
		return "WEP104";
	case RSN_SEL(RSN_CSE_TKIP):
		return "TKIP";
	case RSN_SEL(RSN_CSE_CCMP):
		return "AES-CCMP";
	case RSN_SEL(RSN_CSE_WRAP):
		return "AES-OCB";
	}
	return "?";
#undef WPA_SEL
}

static const char *rsn_keymgmt(const u_int8_t *sel)
{
#define	RSN_SEL(x)	(((x)<<24)|RSN_OUI)
	u_int32_t w = LE_READ_4(sel);

	switch (w) {
	case RSN_SEL(RSN_ASE_8021X_UNSPEC):
		return "8021X-UNSPEC";
	case RSN_SEL(RSN_ASE_8021X_PSK):
		return "8021X-PSK";
	case RSN_SEL(RSN_ASE_NONE):
		return "NONE";
	}
	return "?";
#undef RSN_SEL
}

static
void printrsnie(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
	printf("%s", tag);
	if (verbose) {
		const char *sep;
		int n;

		ie += 2, ielen -= 2;

		printf("<v%u", LE_READ_2(ie));
		ie += 2, ielen -= 2;

		printf(" mc:%s", rsn_cipher(ie));
		ie += 4, ielen -= 4;

		/* unicast ciphers */
		n = LE_READ_2(ie);
		ie += 2, ielen -= 2;
		sep = " uc:";
		for (; n > 0; n--) {
			printf("%s%s", sep, rsn_cipher(ie));
			ie += 4, ielen -= 4;
			sep = "+";
		}

		/* key management algorithms */
		n = LE_READ_2(ie);
		ie += 2, ielen -= 2;
		sep = " km:";
		for (; n > 0; n--) {
			printf("%s%s", sep, rsn_keymgmt(ie));
			ie += 4, ielen -= 4;
			sep = "+";
		}

		if (ielen > 2)		/* optional capabilities */
			printf(", caps 0x%x", LE_READ_2(ie));
		/* XXXPMKID */
		printf(">");
	}
}

/* XXX move to a public include file */
#define IEEE80211_WPS_DEV_PASS_ID	0x1012
#define IEEE80211_WPS_SELECTED_REG	0x1041
#define IEEE80211_WPS_SETUP_STATE	0x1044
#define IEEE80211_WPS_UUID_E		0x1047
#define IEEE80211_WPS_VERSION		0x104a

#define BE_READ_2(p)	get_unaligned_be16(p)

static
void printwpsie(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
	u_int8_t len = ie[1];

	printf("%s", tag);
	if (verbose) {
		static const char *dev_pass_id[] = {
			"D",	/* Default (PIN) */
			"U",	/* User-specified */
			"M",	/* Machine-specified */
			"K",	/* Rekey */
			"P",	/* PushButton */
			"R"	/* Registrar-specified */
		};
		int n;

		ie +=6, len -= 4;		/* NB: len is payload only */

		/* WPS IE in Beacon and Probe Resp frames have different fields */
		printf("<");
		while (len) {
			uint16_t tlv_type = BE_READ_2(ie);
			uint16_t tlv_len  = BE_READ_2(ie + 2);

			/* some devices broadcast invalid WPS frames */
			if (tlv_len > len) {
				printf("bad frame length tlv_type=0x%02x "
				    "tlv_len=%d len=%d", tlv_type, tlv_len,
				    len);
				break;
			}

			ie += 4, len -= 4;

			switch (tlv_type) {
			case IEEE80211_WPS_VERSION:
				printf("v:%d.%d", *ie >> 4, *ie & 0xf);
				break;
			case IEEE80211_WPS_SETUP_STATE:
				/* Only 1 and 2 are valid */
				if (*ie == 0 || *ie >= 3)
					printf(" state:B");
				else
					printf(" st:%s", *ie == 1 ? "N" : "C");
				break;
			case IEEE80211_WPS_SELECTED_REG:
				printf(" sel:%s", *ie ? "T" : "F");
				break;
			case IEEE80211_WPS_DEV_PASS_ID:
				n = LE_READ_2(ie);
				if (n < nitems(dev_pass_id))
					printf(" dpi:%s", dev_pass_id[n]);
				break;
			case IEEE80211_WPS_UUID_E:
				printf(" uuid-e:");
				for (n = 0; n < (tlv_len - 1); n++)
					printf("%02x-", ie[n]);
				printf("%02x", ie[n]);
				break;
			}
			ie += tlv_len, len -= tlv_len;
		}
		printf(">");
	}
}

static
void printtdmaie(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
#if 0
	printf("%s", tag);
	if (verbose && ielen >= sizeof(struct ieee80211_tdma_param)) {
		const struct ieee80211_tdma_param *tdma =
		   (const struct ieee80211_tdma_param *) ie;

		/* XXX tstamp */
		printf("<v%u slot:%u slotcnt:%u slotlen:%u bintval:%u inuse:0x%x>",
		    tdma->tdma_version, tdma->tdma_slot, tdma->tdma_slotcnt,
		    LE_READ_2(&tdma->tdma_slotlen), tdma->tdma_bintval,
		    tdma->tdma_inuse[0]);
	}
#endif
}

/*
 * Copy the ssid string contents into buf, truncating to fit.  If the
 * ssid is entirely printable then just copy intact.  Otherwise convert
 * to hexadecimal.  If the result is truncated then replace the last
 * three characters with "...".
 */
static
int copy_essid(char buf[], size_t bufsize, const u_int8_t *essid,
	       size_t essid_len)
{
	const u_int8_t *p;
	size_t maxlen;
	u_int i;

	if (essid_len > bufsize)
		maxlen = bufsize;
	else
		maxlen = essid_len;
	/* determine printable or not */
	for (i = 0, p = essid; i < maxlen; i++, p++) {
		if (*p < ' ' || *p > 0x7e)
			break;
	}
	if (i != maxlen) {		/* not printable, print as hex */
		if (bufsize < 3)
			return 0;
		strlcpy(buf, "0x", bufsize);
		bufsize -= 2;
		p = essid;
		for (i = 0; i < maxlen && bufsize >= 2; i++) {
			sprintf(&buf[2+2*i], "%02x", p[i]);
			bufsize -= 2;
		}
		if (i != essid_len)
			memcpy(&buf[2+2*i-3], "...", 3);
	} else {			/* printable, truncate as needed */
		memcpy(buf, essid, maxlen);
		if (maxlen != essid_len)
			memcpy(&buf[maxlen-3], "...", 3);
	}
	return maxlen;
}

static
void printssid(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
	char ssid[2*IEEE80211_NWID_LEN+1];

	printf("%s<%.*s>", tag, copy_essid(ssid, maxlen, ie+2, ie[1]), ssid);
}

static
void printrates(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
	const char *sep;
	int i;

	printf("%s", tag);
	sep = "<";
	for (i = 2; i < ielen; i++) {
		printf("%s%s%d", sep,
		       ie[i] & IEEE80211_RATE_BASIC ? "B" : "",
		       ie[i] & IEEE80211_RATE_VAL);
		sep = ",";
	}
	printf(">");
}

static
void printcountry(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
	const struct ieee80211_country_ie *cie =
		(const struct ieee80211_country_ie *) ie;
	int i, nbands, schan, nchan;

	printf("%s<%c%c%c", tag, cie->cc[0], cie->cc[1], cie->cc[2]);
	nbands = (cie->len - 3) / sizeof(cie->band[0]);
	for (i = 0; i < nbands; i++) {
		schan = cie->band[i].schan;
		nchan = cie->band[i].nchan;
		if (nchan != 1)
			printf(" %u-%u,%u", schan, schan + nchan-1,
			    cie->band[i].maxtxpwr);
		else
			printf(" %u,%u", schan, cie->band[i].maxtxpwr);
	}
	printf(">");
}

static __inline int iswpaoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WPA_OUI_TYPE<<24)|WPA_OUI);
}

static __inline int iswmeinfo(const u_int8_t *frm)
{
	return frm[1] > 5 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI) &&
		frm[6] == WME_INFO_OUI_SUBTYPE;
}

static __inline int iswmeparam(const u_int8_t *frm)
{
	return frm[1] > 5 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI) &&
		frm[6] == WME_PARAM_OUI_SUBTYPE;
}

static __inline int isatherosoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((ATH_OUI_TYPE<<24)|ATH_OUI);
}

static __inline int istdmaoui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((TDMA_OUI_TYPE<<24)|TDMA_OUI);
}

static __inline int iswpsoui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WPS_OUI_TYPE<<24)|WPA_OUI);
}

static const char *iename(int elemid)
{
	switch (elemid) {
	case IEEE80211_ELEMID_FHPARMS:	return " FHPARMS";
	case IEEE80211_ELEMID_CFPARMS:	return " CFPARMS";
	case IEEE80211_ELEMID_TIM:	return " TIM";
	case IEEE80211_ELEMID_IBSSPARMS:return " IBSSPARMS";
#if 0
	case IEEE80211_ELEMID_BSSLOAD:	return " BSSLOAD";
#endif
	case IEEE80211_ELEMID_CHALLENGE:return " CHALLENGE";
	case IEEE80211_ELEMID_PWRCNSTR:	return " PWRCNSTR";
	case IEEE80211_ELEMID_PWRCAP:	return " PWRCAP";
	case IEEE80211_ELEMID_TPCREQ:	return " TPCREQ";
	case IEEE80211_ELEMID_TPCREP:	return " TPCREP";
	case IEEE80211_ELEMID_SUPPCHAN:	return " SUPPCHAN";
	case IEEE80211_ELEMID_CSA:	return " CSA";
	case IEEE80211_ELEMID_MEASREQ:	return " MEASREQ";
	case IEEE80211_ELEMID_MEASREP:	return " MEASREP";
	case IEEE80211_ELEMID_QUIET:	return " QUIET";
	case IEEE80211_ELEMID_IBSSDFS:	return " IBSSDFS";
	case IEEE80211_ELEMID_TPC:	return " TPC";
	case IEEE80211_ELEMID_CCKM:	return " CCKM";
	}
	return " ???";
}

static void printies(const u_int8_t *vp, int ielen, int maxcols)
{
	while (ielen > 0) {
		switch (vp[0]) {
		case IEEE80211_ELEMID_SSID:
			if (verbose)
				printssid(" SSID", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_RATES:
		case IEEE80211_ELEMID_XRATES:
			if (verbose)
				printrates(vp[0] == IEEE80211_ELEMID_RATES ?
				    " RATES" : " XRATES", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_DSPARMS:
			if (verbose)
				printf(" DSPARMS<%u>", vp[2]);
			break;
		case IEEE80211_ELEMID_COUNTRY:
			if (verbose)
				printcountry(" COUNTRY", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_ERP:
			if (verbose)
				printf(" ERP<0x%x>", vp[2]);
			break;
		case IEEE80211_ELEMID_VENDOR:
			if (iswpaoui(vp))
				printwpaie(" WPA", vp, 2+vp[1], maxcols);
			else if (iswmeinfo(vp))
				printwmeinfo(" WME", vp, 2+vp[1], maxcols);
			else if (iswmeparam(vp))
				printwmeparam(" WME", vp, 2+vp[1], maxcols);
#if 0
			else if (isatherosoui(vp))
				printathie(" ATH", vp, 2+vp[1], maxcols);
#endif
			else if (iswpsoui(vp))
				printwpsie(" WPS", vp, 2+vp[1], maxcols);
			else if (istdmaoui(vp))
				printtdmaie(" TDMA", vp, 2+vp[1], maxcols);
			else if (verbose)
				printie(" VEN", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_RSN:
			printrsnie(" RSN", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_HTCAP:
			printhtcap(" HTCAP", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_HTINFO:
			if (verbose)
				printhtinfo(" HTINFO", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_MESHID:
			if (verbose)
				printssid(" MESHID", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_MESHCONF:
			printmeshconf(" MESHCONF", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_VHT_CAP:
			printvhtcap(" VHTCAP", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_VHT_OPMODE:
			printvhtinfo(" VHTOPMODE", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_VHT_PWR_ENV:
			printvhtpwrenv(" VHTPWRENV", vp, 2+vp[1], maxcols);
			break;
#if 0
		case IEEE80211_ELEMID_BSSLOAD:
			printbssload(" BSSLOAD", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_APCHANREP:
			printapchanrep(" APCHANREP", vp, 2+vp[1], maxcols);
			break;
#endif
		default:
			if (verbose)
				printie(iename(vp[0]), vp, 2+vp[1], maxcols);
			break;
		}
		ielen -= 2+vp[1];
		vp += 2+vp[1];
	}
}

static void printmimo(const struct ieee80211_mimo_info *mi)
{
	int i;
	int r = 0;

	for (i = 0; i < IEEE80211_MAX_CHAINS; i++) {
		if (mi->ch[i].rssi != 0) {
			r = 1;
			break;
		}
	}

	/* NB: don't muddy display unless there's something to show */
	if (r == 0)
		return;

#if (CONFIG_MIMO_CHAINS == 1)
	printf(" (rssi %.1f nf %d)",
	    mi->ch[0].rssi[0] / 2.0,
	    mi->ch[0].noise[0]);
#elif  (CONFIG_MIMO_CHAINS == 2)
	printf(" (rssi %.1f:%.1f nf %d:%d)",
	    mi->ch[0].rssi[0] / 2.0,
	    mi->ch[1].rssi[0] / 2.0,
	    mi->ch[0].noise[0],
	    mi->ch[1].noise[0]);
#else
	/* XXX TODO: ignore EVM; secondary channels for now */
	printf(" (rssi %.1f:%.1f:%.1f:%.1f nf %d:%d:%d:%d)",
	    mi->ch[0].rssi[0] / 2.0,
	    mi->ch[1].rssi[0] / 2.0,
	    mi->ch[2].rssi[0] / 2.0,
	    mi->ch[3].rssi[0] / 2.0,
	    mi->ch[0].noise[0],
	    mi->ch[1].noise[0],
	    mi->ch[2].noise[0],
	    mi->ch[3].noise[0]);
#endif
}
static const char *get_string(const char *val, const char *sep, u_int8_t *buf,
			      int *lenp)
{
	int len;
	int hexstr;
	u_int8_t *p;

	len = *lenp;
	p = buf;
	hexstr = (val[0] == '0' && tolower((u_char)val[1]) == 'x');
	if (hexstr)
		val += 2;
	for (;;) {
		if (*val == '\0')
			break;
		if (sep != NULL && strchr(sep, *val) != NULL) {
			val++;
			break;
		}
		if (hexstr) {
			if (!isxdigit((u_char)val[0])) {
				printf("bad hexadecimal digits");
				return NULL;
			}
			if (!isxdigit((u_char)val[1])) {
				printf("odd count hexadecimal digits");
				return NULL;
			}
		}
		if (p >= buf + len) {
			if (hexstr)
				printf("hexadecimal digits too long");
			else
				printf("string too long");
			return NULL;
		}
		if (hexstr) {
#define	tohex(x)	(isdigit(x) ? (x) - '0' : tolower(x) - 'a' + 10)
			*p++ = (tohex((u_char)val[0]) << 4) |
			    tohex((u_char)val[1]);
#undef tohex
			val += 2;
		} else
			*p++ = *val++;
	}
	len = p - buf;
	/* The string "-" is treated as the empty string. */
	if (!hexstr && len == 1 && buf[0] == '-') {
		len = 0;
		memset(buf, 0, *lenp);
	} else if (len < *lenp)
		memset(p, 0, *lenp - len);
	*lenp = len;
	return val;
}


static int isanyarg(const char *arg)
{
	return (strncmp(arg, "-", 1) == 0 ||
	    strncasecmp(arg, "any", 3) == 0 || strncasecmp(arg, "off", 3) == 0);
}

static int isundefarg(const char *arg)
{
	return (strcmp(arg, "-") == 0 || strncasecmp(arg, "undef", 5) == 0);
}

/* States*/
#define NAMING	0
#define GOTONE	1
#define GOTTWO	2
#define RESET	3
/* Inputs */
#define	DIGIT	(4*0)
#define	END	(4*1)
#define DELIM	(4*2)
#define LETTER	(4*3)

void link_addr(const char *addr, struct sockaddr_dl *sdl)
{
	char *cp = (char *)sdl->sdl_data;
	char *cplim = sdl->sdl_len + (char *)sdl;
	int byte = 0, state = NAMING, new = 0;

	bzero((char *)&sdl->sdl_family, sdl->sdl_len - 1);
	sdl->sdl_family = AF_LINK;
	do {
		state &= ~LETTER;
		if ((*addr >= '0') && (*addr <= '9')) {
			new = *addr - '0';
		} else if ((*addr >= 'a') && (*addr <= 'f')) {
			new = *addr - 'a' + 10;
		} else if ((*addr >= 'A') && (*addr <= 'F')) {
			new = *addr - 'A' + 10;
		} else if (*addr == 0) {
			state |= END;
		} else if (state == NAMING &&
			   (((*addr >= 'A') && (*addr <= 'Z')) ||
			   ((*addr >= 'a') && (*addr <= 'z'))))
			state |= LETTER;
		else
			state |= DELIM;
		addr++;
		switch (state /* | INPUT */) {
		case NAMING | DIGIT:
		case NAMING | LETTER:
			*cp++ = addr[-1];
			continue;
		case NAMING | DELIM:
			state = RESET;
			sdl->sdl_nlen = (uint8_t)(cp - (char *)sdl->sdl_data);
			continue;
		case GOTTWO | DIGIT:
			*cp++ = byte;
			/* FALLTHROUGH */
		case RESET | DIGIT:
			state = GOTONE;
			byte = new;
			continue;
		case GOTONE | DIGIT:
			state = GOTTWO;
			byte = new + (byte << 4);
			continue;
		default: /* | DELIM */
			state = RESET;
			*cp++ = byte;
			byte = 0;
			continue;
		case GOTONE | END:
		case GOTTWO | END:
			*cp++ = byte;
			/* FALLTHROUGH */
		case RESET | END:
			break;
		}
		break;
	} while (cp < cplim);
	sdl->sdl_alen = (uint8_t)(cp - (char *)LLADDR(sdl));
	new = cp - (char *)sdl;
	if (new > sizeof(*sdl))
		sdl->sdl_len = new;
	return;
}
static int gethtconf(void)
{
	if (gothtconf)
		return CMD_RET_SUCCESS;

	IFCONFIG_CALL(get80211val, CMD_RET_FAILURE,
			IEEE80211_IOC_HTCONF,
			0, &htconf);

	gothtconf = 1;

	return CMD_RET_SUCCESS;
}

struct ifmediareq *ifmedia_getstate(void)
{
	/*static struct ifmediareq *ifmr = NULL;*/
	int *mwords;

	if (ifmr == NULL) {
		ifmr = (struct ifmediareq *)malloc(sizeof(struct ifmediareq));
		if (ifmr == NULL)
			printf("[%s, %d] malloc failed\n", __func__, __LINE__);

		(void) memset(ifmr, 0, sizeof(struct ifmediareq));

		ifmr->ifm_count = 0;
		ifmr->ifm_ulist = NULL;

		/*
		 * We must go through the motions of reading all
		 * supported media because we need to know both
		 * the current media type and the top-level type.
		 */

		IFCONFIG_CALL(get_media, NULL, ifmr);

		if (ifmr->ifm_count == 0) {
			printf("[%s, %d] %s: no media types?\n",
					__func__, __LINE__, ifname);
			return NULL;
		}

		mwords = (int *)malloc(ifmr->ifm_count * sizeof(int));
		if (mwords == NULL)
			printf("[%s, %d] malloc failed\n",
					__func__, __LINE__);

		ifmr->ifm_ulist = mwords;

		IFCONFIG_CALL(get_media, NULL, ifmr);
	}

	return ifmr;
}

/*
 * Collect channel info from the kernel.  We use this (mostly)
 * to handle mapping between frequency and IEEE channel number.
 */
static int getchaninfo(void)
{
	int space;
	if (chaninfo != NULL)
		return CMD_RET_SUCCESS;
	space = IEEE80211_CHANINFO_SIZE(MAXCHAN);
	chaninfo = malloc(space);
	if (chaninfo == NULL) {
		printf("[%s, %d] no space for channel list (size:%d)\n",
				__func__, __LINE__, space);
		return CMD_RET_FAILURE;
	}
	memset(chaninfo, 0, space);
#ifdef CONFIG_NET80211_IOC_CHANINFO
	IFCONFIG_CALL(get80211, CMD_RET_FAILURE,
			IEEE80211_IOC_CHANINFO,
			chaninfo,
			space);
#endif
#if 0
	printf("[%s, %d] ic_nchans:%d\n", __func__, __LINE__, chaninfo->ic_nchans);
	for (int i = 0; i < chaninfo->ic_nchans; i++)
		printf("ch.%d] flags:0x%x, freq:%d, ieee:%d\n",
				i,
				(unsigned int)chaninfo->ic_chans[i].ic_flags,
				chaninfo->ic_chans[i].ic_freq,
				chaninfo->ic_chans[i].ic_ieee
				);
#endif
	ifmr = ifmedia_getstate();
	(void)gethtconf();

	return CMD_RET_SUCCESS;
}

/*
 * Given the channel at index i with attributes from,
 * check if there is a channel with attributes to in
 * the channel table.  With suitable attributes this
 * allows the caller to look for promotion; e.g. from
 * 11b > 11g.
 */
static int canpromote(int i, int from, int to)
{
	const struct ieee80211_channel *fc = &chaninfo->ic_chans[i];
	u_int j;

	if ((fc->ic_flags & from) != from)
		return i;
	/* NB: quick check exploiting ordering of chans w/ same frequency */
	if (i+1 < chaninfo->ic_nchans &&
	    chaninfo->ic_chans[i+1].ic_freq == fc->ic_freq &&
	    (chaninfo->ic_chans[i+1].ic_flags & to) == to)
		return i+1;
	/* brute force search in case channel list is not ordered */
	for (j = 0; j < chaninfo->ic_nchans; j++) {
		const struct ieee80211_channel *tc = &chaninfo->ic_chans[j];
		if (j != i &&
		    tc->ic_freq == fc->ic_freq && (tc->ic_flags & to) == to)
		return j;
	}
	return i;
}

/*
 * Handle channel promotion.  When a channel is specified with
 * only a frequency we want to promote it to the ``best'' channel
 * available.  The channel list has separate entries for 11b, 11g,
 * 11a, and 11n[ga] channels so specifying a frequency w/o any
 * attributes requires we upgrade, e.g. from 11b -> 11g.  This
 * gets complicated when the channel is specified on the same
 * command line with a media request that constrains the available
 * channel list (e.g. mode 11a); we want to honor that to avoid
 * confusing behaviour.
 */
/*
 * XXX VHT
 */
static int promote(int i)
{
	/*
	 * Query the current mode of the interface in case it's
	 * constrained (e.g. to 11a).  We must do this carefully
	 * as there may be a pending ifmedia request in which case
	 * asking the kernel will give us the wrong answer.  This
	 * is an unfortunate side-effect of the way ifconfig is
	 * structure for modularity (yech).
	 *
	 * NB: ifmr is actually setup in getchaninfo (above); we
	 *     assume it's called coincident with to this call so
	 *     we have a ``current setting''; otherwise we must pass
	 *     the socket descriptor down to here so we can make
	 *     the ifmedia_getstate call ourselves.
	 */
	int chanmode = ifmr != NULL ? IFM_MODE(ifmr->ifm_current) : IFM_AUTO;

	/* when ambiguous promote to ``best'' */
	/* NB: we arbitrarily pick HT40+ over HT40- */
	if (chanmode != IFM_IEEE80211_11B)
		i = canpromote(i, IEEE80211_CHAN_B, IEEE80211_CHAN_G);
	if (chanmode != IFM_IEEE80211_11G && (htconf & 1)) {
		i = canpromote(i, IEEE80211_CHAN_G,
			IEEE80211_CHAN_G | IEEE80211_CHAN_HT20);
		if (htconf & 2) {
			i = canpromote(i, IEEE80211_CHAN_G,
				IEEE80211_CHAN_G | IEEE80211_CHAN_HT40D);
			i = canpromote(i, IEEE80211_CHAN_G,
				IEEE80211_CHAN_G | IEEE80211_CHAN_HT40U);
		}
	}
	if (chanmode != IFM_IEEE80211_11A && (htconf & 1)) {
		i = canpromote(i, IEEE80211_CHAN_A,
			IEEE80211_CHAN_A | IEEE80211_CHAN_HT20);
		if (htconf & 2) {
			i = canpromote(i, IEEE80211_CHAN_A,
				IEEE80211_CHAN_A | IEEE80211_CHAN_HT40D);
			i = canpromote(i, IEEE80211_CHAN_A,
				IEEE80211_CHAN_A | IEEE80211_CHAN_HT40U);
		}
	}
	return i;
}

static void mapfreq(struct ieee80211_channel *chan, int freq, int flags)
{
	u_int i;

	for (i = 0; i < chaninfo->ic_nchans; i++) {
		const struct ieee80211_channel *c = &chaninfo->ic_chans[i];

		if (c->ic_freq == freq && (c->ic_flags & flags) == flags) {
			if (flags == 0) {
				/* when ambiguous promote to ``best'' */
				c = &chaninfo->ic_chans[promote(i)];
			}
			*chan = *c;
			return;
		}
	}
	printf("unknown/undefined frequency %u/0x%x", freq, flags);
}

static void mapchan(struct ieee80211_channel *chan, int ieee, int flags)
{
	u_int i;

	for (i = 0; i < chaninfo->ic_nchans; i++) {
		const struct ieee80211_channel *c = &chaninfo->ic_chans[i];

		if (c->ic_ieee == ieee && (c->ic_flags & flags) == flags) {
			if (flags == 0) {
				/* when ambiguous promote to ``best'' */
				c = &chaninfo->ic_chans[promote(i)];
			}
			*chan = *c;
			return;
		}
	}
	printf("unknown/undefined channel number %d flags 0x%x", ieee, flags);
}

static int ieee80211_mhz2ieee(int freq, int flags)
{
	struct ieee80211_channel chan;
	mapfreq(&chan, freq, flags);
	return chan.ic_ieee;
}


static int set80211ssid(const char *val)
{
	int ssid, len;
	u_int8_t data[IEEE80211_NWID_LEN];

	ssid = 0;
	len = strlen(val);
	if (len > 2 && isdigit((int)val[0]) && val[1] == ':') {
		ssid = atoi(val)-1;
		val += 2;
	}

	bzero(data, sizeof(data));
	len = sizeof(data);
	if (get_string(val, NULL, data, &len) == NULL) {
		printf("set80211ssid : get_string failed\n");
		return CMD_RET_FAILURE;
	}

	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
		      IEEE80211_IOC_SSID,
		      ssid, len, data);

	return CMD_RET_SUCCESS;
}

/*
 * Parse a channel specification for attributes/flags.
 * The syntax is:
 *	freq/xx		channel width (5,10,20,40,40+,40-)
 *	freq:mode	channel mode (a,b,g,h,n,t,s,d)
 *
 * These can be combined in either order; e.g. 2437:ng/40.
 * Modes are case insensitive.
 *
 * The result is not validated here; it's assumed to be
 * checked against the channel table fetched from the kernel.
 */
static int getchannelflags(const char *val, int freq)
{
#define	_CHAN_HT	0x80000000
	const char *cp;
	int flags;
	int is_vht = 0;

	flags = 0;

	cp = strchr(val, ':');
	if (cp != NULL) {
		for (cp++; isalpha((int) *cp); cp++) {
			/* accept mixed case */
			int c = *cp;
			if (isupper(c))
				c = tolower(c);
			switch (c) {
			case 'a':		/* 802.11a */
				flags |= IEEE80211_CHAN_A;
				break;
			case 'b':		/* 802.11b */
				flags |= IEEE80211_CHAN_B;
				break;
			case 'g':		/* 802.11g */
				flags |= IEEE80211_CHAN_G;
				break;
			case 'v':		/* vht: 802.11ac */
				is_vht = 1;
				/* Fallthrough */
			case 'h':		/* ht = 802.11n */
			case 'n':		/* 802.11n */
				flags |= _CHAN_HT;	/* NB: private */
				break;
			case 'd':		/* dt = Atheros Dynamic Turbo */
				flags |= IEEE80211_CHAN_TURBO;
				break;
			case 't':		/* ht, dt, st, t */
				/* dt and unadorned t specify Dynamic Turbo */
				if ((flags & (IEEE80211_CHAN_STURBO|_CHAN_HT)) == 0)
					flags |= IEEE80211_CHAN_TURBO;
				break;
			case 's':		/* st = Atheros Static Turbo */
				flags |= IEEE80211_CHAN_STURBO;
				break;
			default:
				printf("%s: Invalid channel attribute %c\n",
				    val, *cp);
			}
		}
	}
	cp = strchr(val, '/');
	if (cp != NULL) {
		char *ep;
		u_long cw = strtoul(cp+1, &ep, 10);

		switch (cw) {
		case 5:
			flags |= IEEE80211_CHAN_QUARTER;
			break;
		case 10:
			flags |= IEEE80211_CHAN_HALF;
			break;
		case 20:
			/* NB: this may be removed below */
			flags |= IEEE80211_CHAN_HT20;
			break;
		case 40:
		case 80:
		case 160:
			/* Handle the 80/160 VHT flag */
			if (cw == 80)
				flags |= IEEE80211_CHAN_VHT80;
			else if (cw == 160)
				flags |= IEEE80211_CHAN_VHT160;

			/* Fallthrough */
			if (ep != NULL && *ep == '+')
				flags |= IEEE80211_CHAN_HT40U;
			else if (ep != NULL && *ep == '-')
				flags |= IEEE80211_CHAN_HT40D;
			break;
		default:
			printf("%s: Invalid channel width\n", val);
		}
	}

	/*
	 * Cleanup specifications.
	 */
	if ((flags & _CHAN_HT) == 0) {
		/*
		 * If user specified freq/20 or freq/40 quietly remove
		 * HT cw attributes depending on channel use.  To give
		 * an explicit 20/40 width for an HT channel you must
		 * indicate it is an HT channel since all HT channels
		 * are also usable for legacy operation; e.g. freq:n/40.
		 */
		flags &= ~IEEE80211_CHAN_HT;
		flags &= ~IEEE80211_CHAN_VHT;
	} else {
		/*
		 * Remove private indicator that this is an HT channel
		 * and if no explicit channel width has been given
		 * provide the default settings.
		 */
		flags &= ~_CHAN_HT;
		if ((flags & IEEE80211_CHAN_HT) == 0) {
			struct ieee80211_channel chan;
			/*
			 * Consult the channel list to see if we can use
			 * HT40+ or HT40- (if both the map routines choose).
			 */
			if (freq > 255)
				mapfreq(&chan, freq, 0);
			else
				mapchan(&chan, freq, 0);
			flags |= (chan.ic_flags & IEEE80211_CHAN_HT);
		}

		/*
		 * If VHT is enabled, then also set the VHT flag and the
		 * relevant channel up/down.
		 */
		if (is_vht && (flags & IEEE80211_CHAN_HT)) {
			/*
			 * XXX yes, maybe we should just have VHT, and reuse
			 * HT20/HT40U/HT40D
			 */
			if (flags & IEEE80211_CHAN_VHT80)
				;
			else if (flags & IEEE80211_CHAN_HT20)
				flags |= IEEE80211_CHAN_VHT20;
			else if (flags & IEEE80211_CHAN_HT40U)
				flags |= IEEE80211_CHAN_VHT40U;
			else if (flags & IEEE80211_CHAN_HT40D)
				flags |= IEEE80211_CHAN_VHT40D;
		}
	}
	return flags;
#undef _CHAN_HT
}

static void getchannel(struct ieee80211_channel *chan, const char *val)
{
	int v, flags;
	char *eptr;

	memset(chan, 0, sizeof(*chan));
	if (isanyarg(val)) {
		chan->ic_freq = IEEE80211_CHAN_ANY;
		return;
	}
	(void)getchaninfo();
	errno = 0;
	v = strtol(val, &eptr, 10);
	if (val[0] == '\0' || val == eptr || errno == ERANGE ||
	    /* channel may be suffixed with nothing, :flag, or /width */
	    (eptr[0] != '\0' && eptr[0] != ':' && eptr[0] != '/'))
		printf("invalid channel specification%s",
		    errno == ERANGE ? " (out of range)" : "");
	flags = getchannelflags(val, v);
	if (v > 255) {		/* treat as frequency */
		mapfreq(chan, v, flags);
	} else {
		mapchan(chan, v, flags);
	}
}

static const struct ieee80211_channel *getcurchan(void)
{
	if (gotcurchan)
		return &curchan;

	IFCONFIG_CALL(get80211, NULL,
			IEEE80211_IOC_CURCHAN,
			&curchan, sizeof(curchan));
	gotcurchan = 1;
	return &curchan;
}

static int getmaxrate(const uint8_t rates[15], uint8_t nrates)
{
	int i, maxrate = -1;

	for (i = 0; i < nrates; i++) {
		int rate = rates[i] & IEEE80211_RATE_VAL;
		if (rate > maxrate)
			maxrate = rate;
	}
	return maxrate / 2;
}

static const char *getcaps(int capinfo)
{
	static char capstring[32];
	char *cp = capstring;

	if (capinfo & IEEE80211_CAPINFO_ESS)
		*cp++ = 'E';
	if (capinfo & IEEE80211_CAPINFO_IBSS)
		*cp++ = 'I';
	if (capinfo & IEEE80211_CAPINFO_CF_POLLABLE)
		*cp++ = 'c';
	if (capinfo & IEEE80211_CAPINFO_CF_POLLREQ)
		*cp++ = 'C';
	if (capinfo & IEEE80211_CAPINFO_PRIVACY)
		*cp++ = 'P';
	if (capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE)
		*cp++ = 'S';
	if (capinfo & IEEE80211_CAPINFO_PBCC)
		*cp++ = 'B';
	if (capinfo & IEEE80211_CAPINFO_CHNL_AGILITY)
		*cp++ = 'A';
	if (capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME)
		*cp++ = 's';
	if (capinfo & IEEE80211_CAPINFO_RSN)
		*cp++ = 'R';
	if (capinfo & IEEE80211_CAPINFO_DSSSOFDM)
		*cp++ = 'D';
	*cp = '\0';
	return capstring;
}

static const char *getflags(int flags)
{
	static char flagstring[32];
	char *cp = flagstring;

	if (flags & IEEE80211_NODE_AUTH)
		*cp++ = 'A';
	if (flags & IEEE80211_NODE_QOS)
		*cp++ = 'Q';
	if (flags & IEEE80211_NODE_ERP)
		*cp++ = 'E';
	if (flags & IEEE80211_NODE_PWR_MGT)
		*cp++ = 'P';
	if (flags & IEEE80211_NODE_HT) {
		*cp++ = 'H';
		if (flags & IEEE80211_NODE_HTCOMPAT)
			*cp++ = '+';
	}
	if (flags & IEEE80211_NODE_VHT)
		*cp++ = 'V';
	if (flags & IEEE80211_NODE_WPS)
		*cp++ = 'W';
	if (flags & IEEE80211_NODE_TSN)
		*cp++ = 'N';
	if (flags & IEEE80211_NODE_AMPDU_TX)
		*cp++ = 'T';
	if (flags & IEEE80211_NODE_AMPDU_RX)
		*cp++ = 'R';
	if (flags & IEEE80211_NODE_MIMO_PS) {
		*cp++ = 'M';
		if (flags & IEEE80211_NODE_MIMO_RTS)
			*cp++ = '+';
	}
	if (flags & IEEE80211_NODE_RIFS)
		*cp++ = 'I';
	if (flags & IEEE80211_NODE_SGI40) {
		*cp++ = 'S';
		if (flags & IEEE80211_NODE_SGI20)
			*cp++ = '+';
	} else if (flags & IEEE80211_NODE_SGI20)
		*cp++ = 's';
	if (flags & IEEE80211_NODE_AMSDU_TX)
		*cp++ = 't';
	if (flags & IEEE80211_NODE_AMSDU_RX)
		*cp++ = 'r';
	*cp = '\0';
	return flagstring;
}

#ifdef CONFIG_NET80211_IOC_CHANLIST
static
const char *get_chaninfo(const struct ieee80211_channel *c, int precise,
			 char buf[], size_t bsize)
{
	buf[0] = '\0';
	if (IEEE80211_IS_CHAN_FHSS(c))
		strlcat(buf, " FHSS", bsize);
	if (IEEE80211_IS_CHAN_A(c))
		strlcat(buf, " 11a", bsize);
	else if (IEEE80211_IS_CHAN_ANYG(c))
		strlcat(buf, " 11g", bsize);
	else if (IEEE80211_IS_CHAN_B(c))
		strlcat(buf, " 11b", bsize);
	if (IEEE80211_IS_CHAN_HALF(c))
		strlcat(buf, "/10MHz", bsize);
	if (IEEE80211_IS_CHAN_QUARTER(c))
		strlcat(buf, "/5MHz", bsize);
	if (IEEE80211_IS_CHAN_TURBO(c))
		strlcat(buf, " Turbo", bsize);
	if (precise) {
		/* XXX should make VHT80U, VHT80D */
		if (IEEE80211_IS_CHAN_VHT80(c) &&
		    IEEE80211_IS_CHAN_HT40D(c))
			strlcat(buf, " vht/80-", bsize);
		else if (IEEE80211_IS_CHAN_VHT80(c) &&
		    IEEE80211_IS_CHAN_HT40U(c))
			strlcat(buf, " vht/80+", bsize);
		else if (IEEE80211_IS_CHAN_VHT80(c))
			strlcat(buf, " vht/80", bsize);
		else if (IEEE80211_IS_CHAN_VHT40D(c))
			strlcat(buf, " vht/40-", bsize);
		else if (IEEE80211_IS_CHAN_VHT40U(c))
			strlcat(buf, " vht/40+", bsize);
		else if (IEEE80211_IS_CHAN_VHT20(c))
			strlcat(buf, " vht/20", bsize);
		else if (IEEE80211_IS_CHAN_HT20(c))
			strlcat(buf, " ht/20", bsize);
		else if (IEEE80211_IS_CHAN_HT40D(c))
			strlcat(buf, " ht/40-", bsize);
		else if (IEEE80211_IS_CHAN_HT40U(c))
			strlcat(buf, " ht/40+", bsize);
	} else {
		if (IEEE80211_IS_CHAN_VHT(c))
			strlcat(buf, " vht", bsize);
		else if (IEEE80211_IS_CHAN_HT(c))
			strlcat(buf, " ht", bsize);
	}
	return buf;
}

static void print_chaninfo(const struct ieee80211_channel *c, int verb)
{
	char buf[14];

	if (verb)
		printf("Channel %3u : %u%c%c%c%c%c MHz%-14.14s",
		    ieee80211_mhz2ieee(c->ic_freq, c->ic_flags), c->ic_freq,
		    IEEE80211_IS_CHAN_PASSIVE(c) ? '*' : ' ',
		    IEEE80211_IS_CHAN_DFS(c) ? 'D' : ' ',
		    IEEE80211_IS_CHAN_RADAR(c) ? 'R' : ' ',
		    IEEE80211_IS_CHAN_CWINT(c) ? 'I' : ' ',
		    IEEE80211_IS_CHAN_CACDONE(c) ? 'C' : ' ',
		    get_chaninfo(c, verb, buf, sizeof(buf)));
	else
		printf("Channel %3u : %u%c MHz%-14.14s",
		    ieee80211_mhz2ieee(c->ic_freq, c->ic_flags), c->ic_freq,
		    IEEE80211_IS_CHAN_PASSIVE(c) ? '*' : ' ',
		    get_chaninfo(c, verb, buf, sizeof(buf)));

}

static int chanpref(const struct ieee80211_channel *c)
{
	if (IEEE80211_IS_CHAN_VHT160(c))
		return 80;
	if (IEEE80211_IS_CHAN_VHT80_80(c))
		return 75;
	if (IEEE80211_IS_CHAN_VHT80(c))
		return 70;
	if (IEEE80211_IS_CHAN_VHT40(c))
		return 60;
	if (IEEE80211_IS_CHAN_VHT20(c))
		return 50;
	if (IEEE80211_IS_CHAN_HT40(c))
		return 40;
	if (IEEE80211_IS_CHAN_HT20(c))
		return 30;
	if (IEEE80211_IS_CHAN_HALF(c))
		return 10;
	if (IEEE80211_IS_CHAN_QUARTER(c))
		return 5;
	if (IEEE80211_IS_CHAN_TURBO(c))
		return 25;
	if (IEEE80211_IS_CHAN_A(c))
		return 20;
	if (IEEE80211_IS_CHAN_G(c))
		return 20;
	if (IEEE80211_IS_CHAN_B(c))
		return 15;
	if (IEEE80211_IS_CHAN_PUREG(c))
		return 15;
	return 0;
}

static int print_channels(const struct ieee80211req_chaninfo *chans,
			  int allchans, int verb)
{
	struct ieee80211req_chaninfo *achans;
	uint8_t reported[IEEE80211_CHAN_BYTES];
	const struct ieee80211_channel *c;
	int i, half;
	int space;

	space = IEEE80211_CHANINFO_SPACE(chans);
	achans = malloc(space);
	if (achans == NULL) {
		printf("[%s, %d] no space for active channel list, size:%d\n",
				__func__, __LINE__, space);
		return CMD_RET_FAILURE;
	}
	achans->ic_nchans = 0;
	memset(reported, 0, sizeof(reported));
	if (!allchans) {
		struct ieee80211req_chanlist active;

		IFCONFIG_CALL(get80211, CMD_RET_FAILURE,
				IEEE80211_IOC_CHANLIST,
				&active,
				sizeof(active));
		for (i = 0; i < chans->ic_nchans; i++) {
			c = &chans->ic_chans[i];
			if (!isset(active.ic_channels, c->ic_ieee))
				continue;
			/*
			 * Suppress compatible duplicates unless
			 * verbose.  The kernel gives us it's
			 * complete channel list which has separate
			 * entries for 11g/11b and 11a/turbo.
			 */
			if (isset(reported, c->ic_ieee) && !verb) {
				/* XXX we assume duplicates are adjacent */
				achans->ic_chans[achans->ic_nchans-1] = *c;
			} else {
				achans->ic_chans[achans->ic_nchans++] = *c;
				setbit(reported, c->ic_ieee);
			}
		}
	} else {
		for (i = 0; i < chans->ic_nchans; i++) {
			c = &chans->ic_chans[i];
			if (!c->ic_freq)
				continue;
			/* suppress duplicates as above */
			if (isset(reported, c->ic_ieee) && !verb) {
				/* XXX we assume duplicates are adjacent */
				struct ieee80211_channel *a =
				    &achans->ic_chans[achans->ic_nchans-1];
				if (chanpref(c) > chanpref(a))
					*a = *c;
			} else {
				achans->ic_chans[achans->ic_nchans++] = *c;
				setbit(reported, c->ic_ieee);
			}
		}
	}
	half = achans->ic_nchans / 2;
	if (achans->ic_nchans % 2)
		half++;

	for (i = 0; i < achans->ic_nchans / 2; i++) {
		print_chaninfo(&achans->ic_chans[i], verb);
		print_chaninfo(&achans->ic_chans[half+i], verb);
		printf("\n");
	}
	if (achans->ic_nchans % 2) {
		print_chaninfo(&achans->ic_chans[i], verb);
		printf("\n");
	}
	free(achans);

	return CMD_RET_SUCCESS;
}
#else
/* FIXME: weird dependencies here */
static void print_chaninfo(const struct ieee80211_channel *c, int verb)
{
}
static int print_channels(const struct ieee80211req_chaninfo *chans,
			  int allchans, int verb)
{
	return CMD_RET_SUCCESS;
}
#endif

static int list_channels(int allchans)
{
#ifdef CONFIG_NET80211_IOC_CHANLIST
	getchaninfo();
	return print_channels(chaninfo, allchans, verbose);
#else
	return CMD_RET_FAILURE;
#endif
}


#define SCAN_LIST_BUF_SIZE (24*1024)

static int list_scan(void)
{
	uint8_t *buf;
	char ssid[IEEE80211_NWID_LEN+1];
	const uint8_t *cp;
	int len, ssidmax, idlen;
	bool verbose = false;

	buf = malloc(24*1024);
	if (buf == NULL)
		return CMD_RET_FAILURE;

	IFCONFIG_CALL(get80211len, CMD_RET_FAILURE,
			IEEE80211_IOC_SCAN_RESULTS,
			buf, SCAN_LIST_BUF_SIZE, &len);
	if (len < sizeof(struct ieee80211req_scan_result)) {
		free(buf);
		return CMD_RET_FAILURE;
	}

	getchaninfo();

	ssidmax = verbose ? IEEE80211_NWID_LEN : 14;
	printf("%-*.*s  %-17.17s  %4s %4s   %-7s  %3s %4s\n"
		, ssidmax, ssidmax, "SSID/MESH ID"
		, "BSSID"
		, "CHAN"
		, "RATE"
		, " S:N"
		, "INT"
		, "CAPS"
	);
	cp = buf;
	do {
		const struct ieee80211req_scan_result *sr;
		const uint8_t *vp, *idp;

		sr = (const struct ieee80211req_scan_result *) cp;
		vp = cp + sr->isr_ie_off;
		if (sr->isr_meshid_len) {
			idp = vp + sr->isr_ssid_len;
			idlen = sr->isr_meshid_len;
		} else {
			idp = vp;
			idlen = sr->isr_ssid_len;
		}
		printf("%-*.*s  %s  %3d  %3dM %4d:%-4d %4d %-4.4s"
			, ssidmax
			  , copy_essid(ssid, ssidmax, idp, idlen)
			  , ssid
			, ether_ntoa((const struct ether_addr *) sr->isr_bssid)
			, ieee80211_mhz2ieee(sr->isr_freq, sr->isr_flags)
			, getmaxrate(sr->isr_rates, sr->isr_nrates)
			, (sr->isr_rssi/2)+sr->isr_noise, sr->isr_noise
			, sr->isr_intval
			, getcaps(sr->isr_capinfo)
		);
		printies(vp + sr->isr_ssid_len + sr->isr_meshid_len,
		    sr->isr_ie_len, 24);
		printf("\n");
		cp += sr->isr_len, len -= sr->isr_len;
	} while (len >= sizeof(struct ieee80211req_scan_result));

	free(buf);
	return CMD_RET_SUCCESS;
}

static int scan_and_wait(void)
{
	struct ieee80211_scan_req sr;
	char buf[sizeof(struct if_announcemsghdr)];
	struct if_announcemsghdr *ifan;
	struct rt_msghdr *rtm;
	int sroute;

	sroute = socket(PF_ROUTE, SOCK_RAW, 0);
	if (sroute < 0) {
		printf("error:socket(PF_ROUTE,SOCK_RAW)\n");
		return CMD_RET_FAILURE;
	}
	memset(&sr, 0, sizeof(sr));
	sr.sr_flags = IEEE80211_IOC_SCAN_ACTIVE
		    | IEEE80211_IOC_SCAN_BGSCAN
		    | IEEE80211_IOC_SCAN_NOPICK
		    | IEEE80211_IOC_SCAN_ONCE;
	sr.sr_duration = IEEE80211_IOC_SCAN_FOREVER;
	sr.sr_nssid = 0;

	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_SCAN_REQ,
			0, sizeof(sr), &sr);
	do {
		if (read(sroute, buf, sizeof(buf)) < 0) {
			printf("error:read(PF_ROUTE)\n");
			break;
		}
		rtm = (struct rt_msghdr *) buf;
		if (rtm->rtm_version != RTM_VERSION)
			break;
		ifan = (struct if_announcemsghdr *) rtm;
	} while (rtm->rtm_type != RTM_IEEE80211 ||
	    ifan->ifan_what != RTM_IEEE80211_SCAN);
	close(sroute);

	return CMD_RET_SUCCESS;
}

static int scan_cancel(void)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_SCAN_CANCEL,
			0, 0, NULL);
	return CMD_RET_SUCCESS;
}



static int getac(const char *ac)
{
	if (strcasecmp(ac, "ac_be") == 0 || strcasecmp(ac, "be") == 0)
		return WME_AC_BE;
	if (strcasecmp(ac, "ac_bk") == 0 || strcasecmp(ac, "bk") == 0)
		return WME_AC_BK;
	if (strcasecmp(ac, "ac_vi") == 0 || strcasecmp(ac, "vi") == 0)
		return WME_AC_VI;
	if (strcasecmp(ac, "ac_vo") == 0 || strcasecmp(ac, "vo") == 0)
		return WME_AC_VO;
	printf("unknown wme access class %s", ac);
	return WME_AC_BE;
}

/*
 * Parse an optional trailing specification of which netbands
 * to apply a parameter to.  This is basically the same syntax
 * as used for channels but you can concatenate to specify
 * multiple.  For example:
 *	14:abg		apply to 11a, 11b, and 11g
 *	6:ht		apply to 11na and 11ng
 * We don't make a big effort to catch silly things; this is
 * really a convenience mechanism.
 */
static int getmodeflags(const char *val)
{
	const char *cp;
	int flags;

	flags = 0;

	cp = strchr(val, ':');
	if (cp != NULL) {
		for (cp++; isalpha((int) *cp); cp++) {
			/* accept mixed case */
			int c = *cp;
			if (isupper(c))
				c = tolower(c);
			switch (c) {
			case 'a':		/* 802.11a */
				flags |= IEEE80211_CHAN_A;
				break;
			case 'b':		/* 802.11b */
				flags |= IEEE80211_CHAN_B;
				break;
			case 'g':		/* 802.11g */
				flags |= IEEE80211_CHAN_G;
				break;
			case 'n':		/* 802.11n */
				flags |= IEEE80211_CHAN_HT;
				break;
			case 'd':		/* dt = Atheros Dynamic Turbo */
				flags |= IEEE80211_CHAN_TURBO;
				break;
			case 't':		/* ht, dt, st, t */
				/* dt and unadorned t specify Dynamic Turbo */
				if ((flags & (IEEE80211_CHAN_STURBO|IEEE80211_CHAN_HT)) == 0)
					flags |= IEEE80211_CHAN_TURBO;
				break;
			case 's':		/* st = Atheros Static Turbo */
				flags |= IEEE80211_CHAN_STURBO;
				break;
			case 'h':		/* 1/2-width channels */
				flags |= IEEE80211_CHAN_HALF;
				break;
			case 'q':		/* 1/4-width channels */
				flags |= IEEE80211_CHAN_QUARTER;
				break;
			case 'v':
				/* XXX set HT too? */
				flags |= IEEE80211_CHAN_VHT;
				break;
			default:
				printf("%s: Invalid mode attribute %c\n",
				    val, *cp);
			}
		}
	}
	return flags;
}

#define	IEEE80211_CHAN_HTA	(IEEE80211_CHAN_HT|IEEE80211_CHAN_5GHZ)
#define	IEEE80211_CHAN_HTG	(IEEE80211_CHAN_HT|IEEE80211_CHAN_2GHZ)

#ifndef CONFIG_IEEE80211_MODE_11A
#define IEEE80211_MODE_11A 	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_11NA
#define IEEE80211_MODE_11NA	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_11A
#define IEEE80211_MODE_11A	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_FH
#define IEEE80211_MODE_FH	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_TURBO_A
#define IEEE80211_MODE_TURBO_A	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_TURBO_G
#define IEEE80211_MODE_TURBO_G	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_STURBO_A
#define IEEE80211_MODE_STURBO_A	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_11NA
#define IEEE80211_MODE_11NA	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_HALF
#define IEEE80211_MODE_HALF	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_QUARTER
#define IEEE80211_MODE_QUARTER	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_VHT_2GHZ
#define IEEE80211_MODE_VHT_2GHZ	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_VHT_5GHZ
#define IEEE80211_MODE_VHT_5GHZ	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_HE_2GHZ
#define IEEE80211_MODE_HE_2GHZ	IEEE80211_MODE_AUTO
#endif
#ifndef CONFIG_IEEE80211_MODE_HE_5GHZ
#define IEEE80211_MODE_HE_5GHZ	IEEE80211_MODE_AUTO
#endif

#define	_APPLY(_flags, _base, _param, _v) do {				\
    if (_flags & IEEE80211_CHAN_HT) {					\
	    if ((_flags & (IEEE80211_CHAN_5GHZ|IEEE80211_CHAN_2GHZ)) == 0) {\
		    _base.params[IEEE80211_MODE_11NA]._param = _v;	\
		    _base.params[IEEE80211_MODE_11NG]._param = _v;	\
	    } else if (_flags & IEEE80211_CHAN_5GHZ)			\
		    _base.params[IEEE80211_MODE_11NA]._param = _v;	\
	    else							\
		    _base.params[IEEE80211_MODE_11NG]._param = _v;	\
    }									\
    if (_flags & IEEE80211_CHAN_TURBO) {				\
	    if ((_flags & (IEEE80211_CHAN_5GHZ|IEEE80211_CHAN_2GHZ)) == 0) {\
		    _base.params[IEEE80211_MODE_TURBO_A]._param = _v;	\
		    _base.params[IEEE80211_MODE_TURBO_G]._param = _v;	\
	    } else if (_flags & IEEE80211_CHAN_5GHZ)			\
		    _base.params[IEEE80211_MODE_TURBO_A]._param = _v;	\
	    else							\
		    _base.params[IEEE80211_MODE_TURBO_G]._param = _v;	\
    }									\
    if (_flags & IEEE80211_CHAN_STURBO)					\
	    _base.params[IEEE80211_MODE_STURBO_A]._param = _v;		\
    if ((_flags & IEEE80211_CHAN_A) == IEEE80211_CHAN_A)		\
	    _base.params[IEEE80211_MODE_11A]._param = _v;		\
    if ((_flags & IEEE80211_CHAN_G) == IEEE80211_CHAN_G)		\
	    _base.params[IEEE80211_MODE_11G]._param = _v;		\
    if ((_flags & IEEE80211_CHAN_B) == IEEE80211_CHAN_B)		\
	    _base.params[IEEE80211_MODE_11B]._param = _v;		\
    if (_flags & IEEE80211_CHAN_HALF)					\
	    _base.params[IEEE80211_MODE_HALF]._param = _v;		\
    if (_flags & IEEE80211_CHAN_QUARTER)				\
	    _base.params[IEEE80211_MODE_QUARTER]._param = _v;		\
} while (0)

#define	_APPLY1(_flags, _base, _param, _v) do {				\
    if (_flags & IEEE80211_CHAN_HT) {					\
	    if (_flags & IEEE80211_CHAN_5GHZ)				\
		    _base.params[IEEE80211_MODE_11NA]._param = _v;	\
	    else							\
		    _base.params[IEEE80211_MODE_11NG]._param = _v;	\
    } else if ((_flags & IEEE80211_CHAN_108A) == IEEE80211_CHAN_108A)	\
	    _base.params[IEEE80211_MODE_TURBO_A]._param = _v;		\
    else if ((_flags & IEEE80211_CHAN_108G) == IEEE80211_CHAN_108G)	\
	    _base.params[IEEE80211_MODE_TURBO_G]._param = _v;		\
    else if ((_flags & IEEE80211_CHAN_ST) == IEEE80211_CHAN_ST)		\
	    _base.params[IEEE80211_MODE_STURBO_A]._param = _v;		\
    else if (_flags & IEEE80211_CHAN_HALF)				\
	    _base.params[IEEE80211_MODE_HALF]._param = _v;		\
    else if (_flags & IEEE80211_CHAN_QUARTER)				\
	    _base.params[IEEE80211_MODE_QUARTER]._param = _v;		\
    else if ((_flags & IEEE80211_CHAN_A) == IEEE80211_CHAN_A)		\
	    _base.params[IEEE80211_MODE_11A]._param = _v;		\
    else if ((_flags & IEEE80211_CHAN_G) == IEEE80211_CHAN_G)		\
	    _base.params[IEEE80211_MODE_11G]._param = _v;		\
    else if ((_flags & IEEE80211_CHAN_B) == IEEE80211_CHAN_B)		\
	    _base.params[IEEE80211_MODE_11B]._param = _v;		\
} while (0)

#define	_APPLY_RATE(_flags, _base, _param, _v) do {			\
    if (_flags & IEEE80211_CHAN_HT) {					\
	(_v) = (_v / 2) | IEEE80211_RATE_MCS;				\
    }									\
    _APPLY(_flags, _base, _param, _v);					\
} while (0)
#define	_APPLY_RATE1(_flags, _base, _param, _v) do {			\
    if (_flags & IEEE80211_CHAN_HT) {					\
	(_v) = (_v / 2) | IEEE80211_RATE_MCS;				\
    }									\
    _APPLY1(_flags, _base, _param, _v);					\
} while (0)

static int gettxparams(void)
{
#ifdef CONFIG_NET80211_IOC_TXPARAMS
	if (gottxparams)
		return CMD_RET_SUCCESS;

	IFCONFIG_CALL(get80211, CMD_RET_FAILURE,
			IEEE80211_IOC_TXPARAMS,
			&txparams,
			sizeof(txparams));
	gottxparams = 1;

	return CMD_RET_SUCCESS;
#else
	return CMD_RET_FAILURE;
#endif
}

static enum ieee80211_opmode get80211opmode()
{
	struct ifmediareq ifmr;

	(void) memset(&ifmr, 0, sizeof(ifmr));

	/* FIXME : error should be propagated upward ! */
	IFCONFIG_CALL(get_media, IEEE80211_M_STA,
			&ifmr);
	if (ifmr.ifm_current & IFM_IEEE80211_ADHOC) {
		if (ifmr.ifm_current & IFM_FLAG0)
			return IEEE80211_M_AHDEMO;
		else
			return IEEE80211_M_IBSS;
	}
	if (ifmr.ifm_current & IFM_IEEE80211_HOSTAP)
		return IEEE80211_M_HOSTAP;
	if (ifmr.ifm_current & IFM_IEEE80211_IBSS)
		return IEEE80211_M_IBSS;
	if (ifmr.ifm_current & IFM_IEEE80211_MONITOR)
		return IEEE80211_M_MONITOR;
	if (ifmr.ifm_current & IFM_IEEE80211_MBSS)
		return IEEE80211_M_MBSS;
	else
		return IEEE80211_M_STA;
}

static int gettxseq(const struct ieee80211req_sta_info *si)
{
	int i, txseq;

	if ((si->isi_state & IEEE80211_NODE_QOS) == 0)
		return si->isi_txseqs[0];
	/* XXX not right but usually what folks want */
	txseq = 0;
	for (i = 0; i < IEEE80211_TID_SIZE; i++)
		if (si->isi_txseqs[i] > txseq)
			txseq = si->isi_txseqs[i];
	return txseq;
}

static int getrxseq(const struct ieee80211req_sta_info *si)
{
	int i, rxseq;

	if ((si->isi_state & IEEE80211_NODE_QOS) == 0)
		return si->isi_rxseqs[0];
	/* XXX not right but usually what folks want */
	rxseq = 0;
	for (i = 0; i < IEEE80211_TID_SIZE; i++)
		if (si->isi_rxseqs[i] > rxseq)
			rxseq = si->isi_rxseqs[i];
	return rxseq;
}

static int list_stations(void)
{
	union {
		struct ieee80211req_sta_req req;
		uint8_t buf[/*24*/4*1024];
	} *u;
	enum ieee80211_opmode opmode = get80211opmode();
	const uint8_t *cp;
	int len;

	/* broadcast address =>'s get all stations */
	u = malloc(sizeof(*u));
	if (!u)
		return -1;

	(void) memset(u->req.is_u.macaddr, 0xff, IEEE80211_ADDR_LEN);
	if (opmode == IEEE80211_M_STA) {
		/*
		 * Get information about the associated AP.
		 */
		IFCONFIG_CALL(get80211, CMD_RET_FAILURE,
			      IEEE80211_IOC_BSSID,
			      u->req.is_u.macaddr,
			      IEEE80211_ADDR_LEN);
	}
	IFCONFIG_CALL(get80211len, CMD_RET_FAILURE,
		      IEEE80211_IOC_STA_INFO,
		      u, sizeof(*u), &len);
	if (len < sizeof(struct ieee80211req_sta_info))
		return CMD_RET_FAILURE;

	getchaninfo();

	if (opmode == IEEE80211_M_MBSS)
		printf("%-17.17s %4s %5s %5s %7s %4s %4s %4s %6s %6s\n"
			, "ADDR"
			, "CHAN"
			, "LOCAL"
			, "PEER"
			, "STATE"
			, "RATE"
			, "RSSI"
			, "IDLE"
			, "TXSEQ"
			, "RXSEQ"
		);
	else
		printf("%-17.17s %4s %4s %4s %4s %4s %6s %6s %4s %-7s\n"
			, "ADDR"
			, "AID"
			, "CHAN"
			, "RATE"
			, "RSSI"
			, "IDLE"
			, "TXSEQ"
			, "RXSEQ"
			, "CAPS"
			, "FLAG"
		);
	cp = (const uint8_t *) u->req.info;
	do {
		const struct ieee80211req_sta_info *si;

		si = (const struct ieee80211req_sta_info *) cp;
		if (si->isi_len < sizeof(*si))
			break;
		if (opmode == IEEE80211_M_MBSS)
			printf("%s %4d %5x %5x %7.7s %3dM %4.1f %4d %6d %6d"
				, ether_ntoa((const struct ether_addr*)
					     si->isi_macaddr)
			       , ieee80211_mhz2ieee(si->isi_freq,
						    si->isi_flags)
				, si->isi_localid
				, si->isi_peerid
				, mesh_linkstate_string(si->isi_peerstate)
				, si->isi_txmbps/2
				, si->isi_rssi/1.
				, si->isi_inact
				, gettxseq(si)
				, getrxseq(si)
			);
		else
			printf("%s %4u %4d %3dM %4.1f %4d %6d %6d %-4.4s %-7.7s"
				, ether_ntoa((const struct ether_addr*)
					     si->isi_macaddr)
				, IEEE80211_AID(si->isi_associd)
				, ieee80211_mhz2ieee(si->isi_freq,
						     si->isi_flags)
				, si->isi_txmbps/2
				, si->isi_rssi/1.
				, si->isi_inact
				, gettxseq(si)
				, getrxseq(si)
				, getcaps(si->isi_capinfo)
				, getflags(si->isi_state)
			);
		printies(cp + si->isi_ie_off, si->isi_ie_len, 24);
		printmimo(&si->isi_mimo);
		printf("\n");
		cp += si->isi_len, len -= si->isi_len;
	} while (len >= sizeof(struct ieee80211req_sta_info));

	free(u);
	return CMD_RET_SUCCESS;
}

static void print_txpow(const struct ieee80211_channel *c)
{
	printf("Channel %3u : %u MHz %3.1f reg %2d  ",
	       c->ic_ieee, c->ic_freq,
	       c->ic_maxpower/2., c->ic_maxregpower);
}

static void print_txpow_verbose(const struct ieee80211_channel *c)
{
	print_chaninfo(c, 1);
	printf("min %4.1f dBm  max %3.1f dBm  reg %2d dBm",
	       c->ic_minpower/2., c->ic_maxpower/2., c->ic_maxregpower);
	/* indicate where regulatory cap limits power use */
	if (c->ic_maxpower > 2*c->ic_maxregpower)
		printf(" <");
}

static int list_txpow(void)
{
	struct ieee80211req_chaninfo *achans;
	uint8_t reported[IEEE80211_CHAN_BYTES];
	struct ieee80211_channel *c, *prev;
	int i, half;
	int space;

	getchaninfo();
	space = IEEE80211_CHANINFO_SPACE(chaninfo);
	achans = malloc(space);
	if (achans == NULL) {
		printf("[%s, %d] no space for active channel list, size:%d\n",
				__func__, __LINE__, space);
		return CMD_RET_FAILURE;
	}
	achans->ic_nchans = 0;
	memset(reported, 0, sizeof(reported));
	for (i = 0; i < chaninfo->ic_nchans; i++) {
		c = &chaninfo->ic_chans[i];
		/* suppress duplicates as above */
		if (isset(reported, c->ic_ieee) && !verbose) {
			/* XXX we assume duplicates are adjacent */
			assert(achans->ic_nchans > 0);
			prev = &achans->ic_chans[achans->ic_nchans-1];
			/* display highest power on channel */
			if (c->ic_maxpower > prev->ic_maxpower)
				*prev = *c;
		} else {
			achans->ic_chans[achans->ic_nchans++] = *c;
			setbit(reported, c->ic_ieee);
		}
	}
	if (!verbose) {
		half = achans->ic_nchans / 2;
		if (achans->ic_nchans % 2)
			half++;

		for (i = 0; i < achans->ic_nchans / 2; i++) {
			print_txpow(&achans->ic_chans[i]);
			print_txpow(&achans->ic_chans[half+i]);
			printf("\n");
		}
		if (achans->ic_nchans % 2) {
			print_txpow(&achans->ic_chans[i]);
			printf("\n");
		}
	} else {
		for (i = 0; i < achans->ic_nchans; i++) {
			print_txpow_verbose(&achans->ic_chans[i]);
			printf("\n");
		}
	}
	free(achans);

	return CMD_RET_SUCCESS;
}

static int list_keys(void)
{
	printf("Not supported yet\n");
	return CMD_RET_SUCCESS;
}

static int getdevcaps(struct ieee80211_devcaps_req *dc)
{
	IFCONFIG_CALL(get80211, CMD_RET_FAILURE,
			IEEE80211_IOC_DEVCAPS,
			dc,
			IEEE80211_DEVCAPS_SPACE(dc));

	return CMD_RET_SUCCESS;
}

static int list_capabilities(void)
{
	struct ieee80211_devcaps_req *dc;

	if (verbose)
		dc = malloc(IEEE80211_DEVCAPS_SIZE(MAXCHAN));
	else
		dc = malloc(IEEE80211_DEVCAPS_SIZE(1));
	if (dc == NULL)
		printf("[%s, %d] no space for device capabilities\n",
				__func__, __LINE__);
	dc->dc_chaninfo.ic_nchans = verbose ? MAXCHAN : 1;
	if (getdevcaps(dc)) {
		free(dc);
		return CMD_RET_FAILURE;
	}
	printb("drivercaps", dc->dc_drivercaps, IEEE80211_C_BITS);
	if (dc->dc_cryptocaps != 0 || verbose) {
		putchar('\n');
		printb("cryptocaps", dc->dc_cryptocaps, IEEE80211_CRYPTO_BITS);
	}
	if (dc->dc_htcaps != 0 || verbose) {
		putchar('\n');
		printb("htcaps", dc->dc_htcaps, IEEE80211_HTCAP_BITS);
	}
#if 0
	if (dc->dc_vhtcaps != 0 || verbose) {
		putchar('\n');
		printb("vhtcaps", dc->dc_vhtcaps, IEEE80211_VHTCAP_BITS);
	}
#endif

	putchar('\n');
	if (verbose) {
		if (chaninfo != NULL)
			free(chaninfo);
		chaninfo = &dc->dc_chaninfo;	/* XXX */
		if (print_channels(&dc->dc_chaninfo, 1/*allchans*/, verbose)) {
			free(dc);
			chaninfo = NULL;
			return CMD_RET_FAILURE;
		}
		chaninfo = NULL;
	}
	free(dc);

	return CMD_RET_SUCCESS;
}

static int get80211wme(int param, int ac, int *val)
{
	IFCONFIG_CALL(get80211val, -1, param, ac, val);
	return 0;
}

static void list_wme_aci(const char *tag, int ac)
{
	int val;

	printf("\t%s", tag);

	/* show WME BSS parameters */
	if (get80211wme(IEEE80211_IOC_WME_CWMIN, ac, &val) != -1)
		printf(" cwmin %2u", val);
	if (get80211wme(IEEE80211_IOC_WME_CWMAX, ac, &val) != -1)
		printf(" cwmax %2u", val);
	if (get80211wme(IEEE80211_IOC_WME_AIFS, ac, &val) != -1)
		printf(" aifs %2u", val);
	if (get80211wme(IEEE80211_IOC_WME_TXOPLIMIT, ac, &val) != -1)
		printf(" txopLimit %3u", val);
	if (get80211wme(IEEE80211_IOC_WME_ACM, ac, &val) != -1) {
		if (val)
			printf(" acm");
		else if (verbose)
			printf(" -acm");
	}
	/* !BSS only */
	if ((ac & IEEE80211_WMEPARAM_BSS) == 0) {
		if (get80211wme(IEEE80211_IOC_WME_ACKPOLICY, ac, &val) != -1) {
			if (!val)
				printf(" -ack");
			else if (verbose)
				printf(" ack");
		}
	}
	printf("\n");
}

static int list_wme(void)
{
	static const char *acnames[] = { "AC_BE", "AC_BK", "AC_VI", "AC_VO" };
	int ac;

	if (verbose) {
		/* display both BSS and local settings */
		for (ac = WME_AC_BE; ac <= WME_AC_VO; ac++) {
	again:
			if (ac & IEEE80211_WMEPARAM_BSS)
				list_wme_aci("     ", ac);
			else
				list_wme_aci(acnames[ac], ac);
			if ((ac & IEEE80211_WMEPARAM_BSS) == 0) {
				ac |= IEEE80211_WMEPARAM_BSS;
				goto again;
			} else
				ac &= ~IEEE80211_WMEPARAM_BSS;
		}
	} else {
		/* display only channel settings */
		for (ac = WME_AC_BE; ac <= WME_AC_VO; ac++)
			list_wme_aci(acnames[ac], ac);
	}

	return CMD_RET_SUCCESS;
}

static int list_roam(void)
{
#ifdef __not_yet__
	const struct ieee80211_roamparam *rp;
	int mode;

	getroam(s);
	for (mode = IEEE80211_MODE_AUTO + 1; mode < IEEE80211_MODE_MAX; mode++) {
		rp = &roamparams.params[mode];
		if (rp->rssi == 0 && rp->rate == 0)
			continue;
		if (mode == IEEE80211_MODE_11NA || mode == IEEE80211_MODE_11NG) {
			if (rp->rssi & 1)
				LINE_CHECK("roam:%-7.7s rssi %2u.5dBm  MCS %2u    ",
				    modename[mode], rp->rssi/2,
				    rp->rate &~ IEEE80211_RATE_MCS);
			else
				LINE_CHECK("roam:%-7.7s rssi %4udBm  MCS %2u    ",
				    modename[mode], rp->rssi/2,
				    rp->rate &~ IEEE80211_RATE_MCS);
		} else {
			if (rp->rssi & 1)
				LINE_CHECK("roam:%-7.7s rssi %2u.5dBm rate %2u Mb/s",
				    modename[mode], rp->rssi/2, rp->rate/2);
			else
				LINE_CHECK("roam:%-7.7s rssi %4udBm rate %2u Mb/s",
				    modename[mode], rp->rssi/2, rp->rate/2);
		}
	}
#else
	printf("Not supported yet\n");
	return CMD_RET_SUCCESS;
#endif
}

static int list_txparams(void)
{
#ifdef CONFIG_NET80211_IOC_TXPARAMS
	const struct ieee80211_txparam *tp;
	int mode;

	gettxparams();
	for (mode = IEEE80211_MODE_AUTO + 1; mode < IEEE80211_MODE_MAX; mode++) {
		tp = &txparams.params[mode];
		if (tp->mgmtrate == 0 && tp->mcastrate == 0)
			continue;
		if (
#ifdef CONFIG_IEEE80211_MODE_11NA
		    mode == IEEE80211_MODE_11NA ||
#endif
		    mode == IEEE80211_MODE_11NG) {
			if (tp->ucastrate == IEEE80211_FIXED_RATE_NONE)
				LINE_CHECK("%-7.7s ucast NONE    mgmt %2u MCS  "
				    "mcast %2u MCS  maxretry %u",
				    modename[mode],
				    tp->mgmtrate &~ IEEE80211_RATE_MCS,
				    tp->mcastrate &~ IEEE80211_RATE_MCS,
				    tp->maxretry);
			else
				LINE_CHECK("%-7.7s ucast %2u MCS  mgmt %2u MCS  "
				    "mcast %2u MCS  maxretry %u",
				    modename[mode],
				    tp->ucastrate &~ IEEE80211_RATE_MCS,
				    tp->mgmtrate &~ IEEE80211_RATE_MCS,
				    tp->mcastrate &~ IEEE80211_RATE_MCS,
				    tp->maxretry);
		} else {
			if (tp->ucastrate == IEEE80211_FIXED_RATE_NONE)
				LINE_CHECK("%-7.7s ucast NONE    mgmt %2u Mb/s "
				    "mcast %2u Mb/s maxretry %u",
				    modename[mode],
				    tp->mgmtrate/2,
				    tp->mcastrate/2, tp->maxretry);
			else
				LINE_CHECK("%-7.7s ucast %2u Mb/s mgmt %2u Mb/s "
				    "mcast %2u Mb/s maxretry %u",
				    modename[mode],
				    tp->ucastrate/2, tp->mgmtrate/2,
				    tp->mcastrate/2, tp->maxretry);
		}
	}
	printf("\n");

	return CMD_RET_SUCCESS;
#else
	return CMD_RET_FAILURE;
#endif
}

#ifdef __not_yet__
static void printpolicy(int policy)
{
	switch (policy) {
	case IEEE80211_MACCMD_POLICY_OPEN:
		printf("policy: open\n");
		break;
	case IEEE80211_MACCMD_POLICY_ALLOW:
		printf("policy: allow\n");
		break;
	case IEEE80211_MACCMD_POLICY_DENY:
		printf("policy: deny\n");
		break;
	case IEEE80211_MACCMD_POLICY_RADIUS:
		printf("policy: radius\n");
		break;
	default:
		printf("policy: unknown (%u)\n", policy);
		break;
	}
}
#endif

static int list_mac(void)
{
#ifdef CONFIG_NET80211_IOC_MAC
	struct ieee80211req ireq;
	struct ieee80211req_maclist *acllist;
	int i, nacls, policy, len;
	uint8_t *data;
	char c;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strlcpy(ireq.i_name, name, sizeof(ireq.i_name)); /* XXX ?? */
	ireq.i_type = IEEE80211_IOC_MACCMD;
	ireq.i_val = IEEE80211_MACCMD_POLICY;
	if (ioctl(s, SIOCG80211, &ireq) < 0) {
		if (errno == EINVAL) {
			printf("No acl policy loaded\n");
			return;
		}
		err(1, "unable to get mac policy");
	}
	policy = ireq.i_val;
	if (policy == IEEE80211_MACCMD_POLICY_OPEN) {
		c = '*';
	} else if (policy == IEEE80211_MACCMD_POLICY_ALLOW) {
		c = '+';
	} else if (policy == IEEE80211_MACCMD_POLICY_DENY) {
		c = '-';
	} else if (policy == IEEE80211_MACCMD_POLICY_RADIUS) {
		c = 'r';		/* NB: should never have entries */
	} else {
		printf("policy: unknown (%u)\n", policy);
		c = '?';
	}
	if (verbose || c == '?')
		printpolicy(policy);

	ireq.i_val = IEEE80211_MACCMD_LIST;
	ireq.i_len = 0;
	if (ioctl(s, SIOCG80211, &ireq) < 0)
		err(1, "unable to get mac acl list size");
	if (ireq.i_len == 0) {		/* NB: no acls */
		if (!(verbose || c == '?'))
			printpolicy(policy);
		return;
	}
	len = ireq.i_len;

	data = malloc(len);
	if (data == NULL)
		err(1, "out of memory for acl list");

	ireq.i_data = data;
	if (ioctl(s, SIOCG80211, &ireq) < 0)
		err(1, "unable to get mac acl list");
	nacls = len / sizeof(*acllist);
	acllist = (struct ieee80211req_maclist *) data;
	for (i = 0; i < nacls; i++)
		printf("%c%s\n", c, ether_ntoa(
			(const struct ether_addr *) acllist[i].ml_macaddr));
	free(data);
#else
	return CMD_RET_FAILURE;
#endif
}

#ifdef __not_yet__
static void print_regdomain(const struct ieee80211_regdomain *reg, int verb)
{
	if ((reg->regdomain != 0 &&
	    reg->regdomain != reg->country) || verb) {
		const struct regdomain *rd =
		    lib80211_regdomain_findbysku(getregdata(), reg->regdomain);
		if (rd == NULL)
			LINE_CHECK("regdomain %d", reg->regdomain);
		else
			LINE_CHECK("regdomain %s", rd->name);
	}
	if (reg->country != 0 || verb) {
		const struct country *cc =
		    lib80211_country_findbycc(getregdata(), reg->country);
		if (cc == NULL)
			LINE_CHECK("country %d", reg->country);
		else
			LINE_CHECK("country %s", cc->isoname);
	}
	if (reg->location == 'I')
		LINE_CHECK("indoor");
	else if (reg->location == 'O')
		LINE_CHECK("outdoor");
	else if (verb)
		LINE_CHECK("anywhere");
	if (reg->ecm)
		LINE_CHECK("ecm");
	else if (verb)
		LINE_CHECK("-ecm");
}
#endif

static int list_regdomain(int channelsalso)
{
#ifdef __not_yet__
	getregdomain(s);
	if (channelsalso) {
		getchaninfo(s);
		spacer = ':';
		print_regdomain(&regdomain, 1);
		LINE_BREAK();
		print_channels(s, chaninfo, 1/*allchans*/, 1/*verbose*/);
	} else
		print_regdomain(&regdomain, verbose);
#else
	printf("Not supported yet\n");
	return CMD_RET_SUCCESS;
#endif
}

static int list_countries(void)
{
#ifdef __not_yet__
	struct regdata *rdp = getregdata();
	const struct country *cp;
	const struct regdomain *dp;
	int i;

	i = 0;
	printf("\nCountry codes:\n");
	LIST_FOREACH(cp, &rdp->countries, next) {
		printf("%2s %-15.15s%s", cp->isoname,
		    cp->name, ((i+1)%4) == 0 ? "\n" : " ");
		i++;
	}
	i = 0;
	printf("\nRegulatory domains:\n");
	LIST_FOREACH(dp, &rdp->domains, next) {
		printf("%-15.15s%s", dp->name, ((i+1)%4) == 0 ? "\n" : " ");
		i++;
	}
	printf("\n");
#else
	printf("Not supported yet\n");
	return CMD_RET_SUCCESS;
#endif
}


static int list_mesh(void)
{
#ifdef __not_yet__
	struct ieee80211req ireq;
	struct ieee80211req_mesh_route routes[128];
	struct ieee80211req_mesh_route *rt;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strlcpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_type = IEEE80211_IOC_MESH_RTCMD;
	ireq.i_val = IEEE80211_MESH_RTCMD_LIST;
	ireq.i_data = &routes;
	ireq.i_len = sizeof(routes);
	if (ioctl(s, SIOCG80211, &ireq) < 0)
	 	err(1, "unable to get the Mesh routing table");

	printf("%-17.17s %-17.17s %4s %4s %4s %6s %s\n"
		, "DEST"
		, "NEXT HOP"
		, "HOPS"
		, "METRIC"
		, "LIFETIME"
		, "MSEQ"
		, "FLAGS");

	for (rt = &routes[0]; rt - &routes[0] < ireq.i_len / sizeof(*rt); rt++){
		printf("%s ",
		    ether_ntoa((const struct ether_addr *)rt->imr_dest));
		printf("%s %4u   %4u   %6u %6u    %c%c\n",
			ether_ntoa((const struct ether_addr *)rt->imr_nexthop),
			rt->imr_nhops, rt->imr_metric, rt->imr_lifetime,
			rt->imr_lastmseq,
			(rt->imr_flags & IEEE80211_MESHRT_FLAGS_DISCOVER) ?
			    'D' :
			(rt->imr_flags & IEEE80211_MESHRT_FLAGS_VALID) ?
			    'V' : '!',
			(rt->imr_flags & IEEE80211_MESHRT_FLAGS_PROXY) ?
			    'P' :
			(rt->imr_flags & IEEE80211_MESHRT_FLAGS_GATE) ?
			    'G' :' ');
	}
#else
	printf("Not supported yet\n");
	return CMD_RET_SUCCESS;
#endif
}

static const char * mesh_linkstate_string(uint8_t state)
{
	static const char *state_names[] = {
	    [0] = "IDLE",
	    [1] = "OPEN-TX",
	    [2] = "OPEN-RX",
	    [3] = "CONF-RX",
	    [4] = "ESTAB",
	    [5] = "HOLDING",
	};

	if (state >= nitems(state_names)) {
		static char buf[10];
		snprintf(buf, sizeof(buf), "#%u", state);
		return buf;
	} else
		return state_names[state];
}

static int set80211channel(const char *val)
{
	struct ieee80211_channel chan;

	getchannel(&chan, val);

#if 0
	printf("selch] flags:0x%x, freq:%d, ieee:%d\n",
			(unsigned int)chan.ic_flags,
			chan.ic_freq,
			chan.ic_ieee
			);
#endif

	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_CURCHAN,
			0, sizeof(chan), &chan);

	return CMD_RET_SUCCESS;
}

static int set80211authmode(const char *val)
{
	int	mode;

	if (strcasecmp(val, "none") == 0) {
		mode = IEEE80211_AUTH_NONE;
	} else if (strcasecmp(val, "open") == 0) {
		mode = IEEE80211_AUTH_OPEN;
	} else if (strcasecmp(val, "shared") == 0) {
		mode = IEEE80211_AUTH_SHARED;
	} else if (strcasecmp(val, "8021x") == 0) {
		mode = IEEE80211_AUTH_8021X;
	} else if (strcasecmp(val, "wpa") == 0) {
		mode = IEEE80211_AUTH_WPA;
	} else {
		printf("unknown authmode");
		return CMD_RET_USAGE;
	}

	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_AUTHMODE,
			mode, 0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211powersave(const char *val, int flag)
{
	int mode;

	mode = (flag) ? IEEE80211_POWERSAVE_ON : IEEE80211_POWERSAVE_OFF;
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_POWERSAVE,
			mode, 0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211pspoll(const char *val)
{
	int	mode;

	if (strcasecmp(val, "tx") == 0)
		mode = IEEE80211_POWERSAVE_PSP;
	else
		mode = IEEE80211_POWERSAVE_PSP_CAM;

	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_POWERSAVE,
			mode, 0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211powersavesleep(const char *val)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_POWERSAVESLEEP,
			atoi(val), 0, NULL);

	return CMD_RET_SUCCESS;
}

#ifdef CONFIG_IFCONFIG_HE_LTF_GI

static int set80211heltfgi(char *val[], int flag)
{
    uint32_t ltf_gi = 0;

    if (flag) {
        ltf_gi |= ((atoi(val[0]) << IEEE80211_IOC_HE_LTF_SHIFT) & IEEE80211_IOC_HE_LTF_MASK);
        ltf_gi |= ((atoi(val[1]) << IEEE80211_IOC_HE_GI_SHIFT) & IEEE80211_IOC_HE_GI_MASK);
    }

    IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
        IEEE80211_IOC_HE_LTFGI,
        0, sizeof(ltf_gi), &ltf_gi);

    return CMD_RET_SUCCESS;
}
#endif
#ifdef CONFIG_IFCONFIG_WME_FIX_AC
static int set80211wmeac(char *val[], int flag)
{
    uint32_t wme_ac = 0;

    if (flag) {
        wme_ac = atoi(val[0]);
    }

    IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
        IEEE80211_IOC_WME_FIX_AC,
        0, sizeof(wme_ac), &wme_ac);

    return CMD_RET_SUCCESS;
}
#endif

static int set80211rtsthreshold(const char *val)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_RTSTHRESHOLD,
			isundefarg(val) ? IEEE80211_RTS_MAX : atoi(val),
			0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211protmode(const char *val)
{
	int	mode;

	if (strcasecmp(val, "off") == 0) {
		mode = IEEE80211_PROTMODE_OFF;
	} else if (strcasecmp(val, "cts") == 0) {
		mode = IEEE80211_PROTMODE_CTS;
	} else if (strncasecmp(val, "rtscts", 3) == 0) {
		mode = IEEE80211_PROTMODE_RTSCTS;
	} else {
		printf("unknown protection mode");
		return CMD_RET_USAGE;
	}

	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_PROTMODE,
			mode, 0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211txpower(const char *val)
{
	double v = atof(val);
	int txpow;

	txpow = (int) (2*v);
	if (txpow != 2*v)
		printf("invalid tx power (must be .5 dBm units)");

	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_TXPOWER,
			txpow, 0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211bssid(const char *val)
{
	if (!isanyarg(val)) {
#if 1
		struct sockaddr_dl sdl = {0, };
		struct ether_addr *ether = ether_aton(val);

		if (!ether) {
			printf("malformed link-level address");
			return -1;
		}
		sdl.sdl_len = sizeof(sdl);
		sdl.sdl_family = AF_LINK;
		sdl.sdl_alen = IEEE80211_ADDR_LEN;
		memcpy(sdl.sdl_data, ether, IEEE80211_ADDR_LEN);
#else
		char *temp;
		struct sockaddr_dl sdl;

		temp = malloc(strlen(val) + 2); /* ':' and '\0' */
		if (temp == NULL) {
			printf("malloc failed");
			return CMD_RET_FAILURE;
		}
		temp[0] = ':';
		strcpy(temp + 1, val);
		sdl.sdl_len = sizeof(sdl);
		link_addr(temp, &sdl);
		free(temp);
		if (sdl.sdl_alen != IEEE80211_ADDR_LEN)
			printf("malformed link-level address");
#endif
		IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
				IEEE80211_IOC_BSSID,
				0, IEEE80211_ADDR_LEN,
				LLADDR(&sdl));
	} else {
		uint8_t zerobssid[IEEE80211_ADDR_LEN];
		memset(zerobssid, 0, sizeof(zerobssid));
		IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
				IEEE80211_IOC_BSSID,
				0, IEEE80211_ADDR_LEN,
				zerobssid);
	}

	return CMD_RET_SUCCESS;
}

static int set80211scan(const char *val)
{
	int ret;

	bool cancel = (val[0] == '-' ? true : false);

	if (cancel)
		ret = scan_cancel();
	else {
		ret = scan_and_wait();
		if (ret == CMD_RET_SUCCESS)
			return list_scan();
	}

	return ret;
}

static int set80211cwmin(const char *ac, const char *val, int bss)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_WME_CWMIN,
			atoi(val),
			getac(ac)|(bss ? IEEE80211_WMEPARAM_BSS : 0),
			NULL);

	return CMD_RET_SUCCESS;
}

static int set80211cwmax(const char *ac, const char *val, int bss)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_WME_CWMAX,
			atoi(val),
			getac(ac)|(bss ? IEEE80211_WMEPARAM_BSS : 0),
			NULL);

	return CMD_RET_SUCCESS;
}

static int set80211aifs(const char *ac, const char *val, int bss)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_WME_AIFS,
			atoi(val),
			getac(ac)|(bss ? IEEE80211_WMEPARAM_BSS : 0),
			NULL);

	return CMD_RET_SUCCESS;
}

static int set80211txoplimit(const char *ac, const char *val, int bss)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_WME_TXOPLIMIT,
			atoi(val),
			getac(ac)|(bss ? IEEE80211_WMEPARAM_BSS : 0),
			NULL);

	return CMD_RET_SUCCESS;
}

static int set80211ack(const char *ac, int flag)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_WME_ACKPOLICY,
			flag, getac(ac), NULL);

	return CMD_RET_SUCCESS;
}

static int set80211dtimperiod(const char *val)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_DTIM_PERIOD,
			atoi(val), 0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211bintval(const char *val)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_BEACON_INTERVAL,
			atoi(val), 0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211txparams(void *arg)
{
#ifdef CONFIG_NET80211_IOC_TXPARAMS
	struct ieee80211_txparams_req *txp = arg;

	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_TXPARAMS,
			0, sizeof(*txp), txp);

	return CMD_RET_SUCCESS;
#else
	return CMD_RET_FAILURE;
#endif
}

static int set80211fragthreshold(const char *val)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_FRAGTHRESHOLD,
			isundefarg(val) ? IEEE80211_FRAG_MAX : atoi(val),
			0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211burst(const char *val, int flag)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_BURST,
			flag, 0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211bmissthreshold(const char *val)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_BMISSTHRESHOLD,
			isundefarg(val) ? IEEE80211_HWBMISS_MAX : atoi(val),
			0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211shortgi(const char *val, int flag)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_SHORTGI,
			flag ? (IEEE80211_HTCAP_SHORTGI20 | IEEE80211_HTCAP_SHORTGI40) : 0,
			0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211ampdu(const char *val, int flag)
{
	int ampdu;

	IFCONFIG_CALL(get80211val, CMD_RET_FAILURE,
			IEEE80211_IOC_AMPDU, 0, &ampdu);
	if (flag < 0) {
		flag = -flag;
		ampdu &= ~flag;
	} else
		ampdu |= flag;

	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_AMPDU,
			ampdu, 0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211ampdulimit(const char *val)
{
	int v;

	switch (atoi(val)) {
	case 8:
	case 8*1024:
		v = IEEE80211_HTCAP_MAXRXAMPDU_8K;
		break;
	case 16:
	case 16*1024:
		v = IEEE80211_HTCAP_MAXRXAMPDU_16K;
		break;
	case 32:
	case 32*1024:
		v = IEEE80211_HTCAP_MAXRXAMPDU_32K;
		break;
	case 64:
	case 64*1024:
		v = IEEE80211_HTCAP_MAXRXAMPDU_64K;
		break;
	default:
		printf("invalid A-MPDU limit %s\n", val);
		return CMD_RET_USAGE;
	}

	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_AMPDU_LIMIT,
			v, 0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211ampdudensity(const char *val)
{
	int v;

	if (isanyarg(val) || strcasecmp(val, "na") == 0)
		v = IEEE80211_HTCAP_MPDUDENSITY_NA;
	else
		switch ((int)(atof(val)*4)) {
		case 0:
			v = IEEE80211_HTCAP_MPDUDENSITY_NA;
			break;
		case 1:
			v = IEEE80211_HTCAP_MPDUDENSITY_025;
			break;
		case 2:
			v = IEEE80211_HTCAP_MPDUDENSITY_05;
			break;
		case 4:
			v = IEEE80211_HTCAP_MPDUDENSITY_1;
			break;
		case 8:
			v = IEEE80211_HTCAP_MPDUDENSITY_2;
			break;
		case 16:
			v = IEEE80211_HTCAP_MPDUDENSITY_4;
			break;
		case 32:
			v = IEEE80211_HTCAP_MPDUDENSITY_8;
			break;
		case 64:
			v = IEEE80211_HTCAP_MPDUDENSITY_16;
			break;
		default:
			printf("invalid A-MPDU density %s\n", val);
			return CMD_RET_FAILURE;
		}

	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_AMPDU_DENSITY,
			v, 0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211inact(const char *val, int flag)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_INACTIVITY,
			flag, 0, NULL);

	return CMD_RET_SUCCESS;
}

static int set80211htconf(const char *val, int flag)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_HTCONF,
			flag, 0, NULL);
	htconf = flag;

	return CMD_RET_SUCCESS;
}

static int set80211rifs(const char *val, int flag)
{
	IFCONFIG_CALL(set80211, CMD_RET_FAILURE,
			IEEE80211_IOC_RIFS,
			flag, 0, NULL);

	return CMD_RET_SUCCESS;
}

#endif /* CONFIG_IFCONFIG_WIRELESS */

static struct {
	int flags;
	const char *str;
} iff_flagstr[]= {
	{ IFF_UP, "UP" },
	{ IFF_BROADCAST, "BROADCAST" },
	{ IFF_DEBUG, "DEBUG" },
	{ IFF_LOOPBACK, "LOOPBACK" },
	{ IFF_POINTOPOINT, "POINTTOPOINT" },
	{ IFF_LINK, "RUNNING" },
	{ IFF_NOARP, "NOARP" },
	{ IFF_PROMISC, "PROMISC" },
	{ IFF_ALLMULTI, "ALLMULTI" },
	{ IFF_MULTICAST, "MULTICAST" },
};

static void get_ifflags_string(char *buf, int flags, int linkstate)
{
	int i;
	char comma = '<';

	if (linkstate == LINK_STATE_UP)
		flags |= IFF_LINK;

	buf += sprintf(buf, "flags=%d", flags);
	for (i = 0; i < ARRAY_SIZE(iff_flagstr); i++) {
		if (flags & iff_flagstr[i].flags) {
			buf += sprintf(buf, "%c%s", comma, iff_flagstr[i].str);
			comma = ',';
		}
	}
	sprintf(buf, ">");
}

int do_ifconfig_status(void)
{
#define NETIF_FLAG_ROUTE 0x80U
	int i = 1; //NETIF index starts from 1
	char netifname[IFNAMSIZ];
	const char *_ifname = ifname;
	int mtu, metric, flag, linkstate, txqlen;
	struct sockaddr_in addr;
	struct sockaddr_in netmask;
	struct sockaddr_in broadaddr;
	struct sockaddr hwaddr;
	static char flags[200];
	struct aftype *ap;
	struct hwtype *hw = &ether_hwtype;
	struct ifstat *stats = NULL;
#ifdef CONFIG_LWIP_IPV6
	struct sockaddr_in6 addr6;
	ip6_addr_t ip6addr;
	int plen, index;
#endif

	do {
		if (!if_indextoname(i++, netifname)) {
			if (i > 1)
				break;
			else {
				printf("No net i/f available.\n");
				return CMD_RET_FAILURE;
			}
		}
		if (1 && if_nametoflags(netifname) & NETIF_FLAG_ROUTE) {
			/* Skip interface for internal use */
			continue;
		}
		if (_ifname && strncmp(_ifname, netifname, IFNAMSIZ-1))
			continue;

		ifname = netifname;
		ap = &inet_aftype;

		IFCONFIG_CALL(get_mtu, CMD_RET_FAILURE, &mtu);
		IFCONFIG_CALL(get_metric, CMD_RET_FAILURE, &metric);
		IFCONFIG_CALL(get_flags, CMD_RET_FAILURE, &flag);
		IFCONFIG_CALL(get_linkstate, CMD_RET_FAILURE, &linkstate);
		IFCONFIG_CALL(get_txqlen, CMD_RET_FAILURE, &txqlen);
		IFCONFIG_CALL(get_addr, CMD_RET_FAILURE, (void *) &addr);
		IFCONFIG_CALL(get_broadaddr, CMD_RET_FAILURE, (void *) &broadaddr);
		IFCONFIG_CALL(get_netmask, CMD_RET_FAILURE, (void *) &netmask);
		IFCONFIG_CALL(get_hwaddr, CMD_RET_FAILURE, &hwaddr);

		get_ifflags_string(flags, flag, linkstate);
		printf("%s: %s  mtu %d  metric %d\n", ifname, flags, mtu, metric);
		printf("\t%s %s", ap->name, ap->sprint((struct sockaddr *)&addr, 1));
		printf("  netmask %s", ap->sprint((struct sockaddr *)&netmask, 1));
		if (flag & IFF_BROADCAST) {
			printf("  broadcast %s", ap->sprint((struct sockaddr *)&broadaddr, 1));
		}
#ifdef CONFIG_LWIP_IPV6
		plen = 64;
		ap = &inet6_aftype;

		for (index = 0; index < LWIP_IPV6_NUM_ADDRESSES; index++) {
			IFCONFIG_CALL(get_ip6addr, CMD_RET_FAILURE, (void *) &addr6, index);
			if (addr6.sin6_len == 0) {
				continue;
			}

			inet6_addr_to_ip6addr(&ip6addr, &((addr6).sin6_addr));
			printf("\n\t%s %s  prefixlen %d", ap->name,
				ap->sprint((struct sockaddr *)&addr6, 1),
				plen);
			printf("  scopeid 0x%x", addr6.sin6_scope_id);

			memset(flags, 0, sizeof(flags));
			flags[0] = '<';
			if (ip6_addr_isglobal(&ip6addr))
				strcat(flags, "global,");
			if (ip6_addr_islinklocal(&ip6addr))
				strcat(flags, "link,");
			if (ip6_addr_issitelocal(&ip6addr))
				strcat(flags, "site,");
			if (ip6_addr_isloopback(&ip6addr))
				strcat(flags, "host,");

			if (flags[strlen(flags)-1] == ',')
				flags[strlen(flags)-1] = '>';
			else
				flags[strlen(flags)-1] = 0;
			printf("%s", flags);
		}
#endif
		printf("\n\t%s %s", hw->name, hw->print((unsigned char *)hwaddr.sa_data));
		if (txqlen != -1)
			printf("  txqueuelen %d", txqlen);
		printf("\n");

		stats = malloc(sizeof(*stats));
		if (stats) {
			IFCONFIG_CALL(get_stats, CMD_RET_FAILURE, stats);
			printf("%s", stats->ascii);
			free(stats);
		}
		printf("\n");
	} while (1);

	return CMD_RET_SUCCESS;
#undef NETIF_FLAGS_ROUTE
}

int do_ifconfig_addr(int argc, char *argv[])
{
	char host[128];
	struct sockaddr sa;
	struct aftype *ap = &inet_aftype;

	safe_strncpy(host, argv[0], (sizeof host));
	if (ap->input(host, &sa) < 0) {
		if (ap->herror)
			ap->herror(host);
		return CMD_RET_FAILURE;
	}

	IFCONFIG_CALL(set_addr, CMD_RET_FAILURE, sa);
	return CMD_RET_SUCCESS;
}

int do_ifconfig_mtu(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;

	IFCONFIG_CALL(set_mtu, CMD_RET_FAILURE, atoi(argv[1]));
	return CMD_RET_SUCCESS;
}

int do_ifconfig_metric(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;

	IFCONFIG_CALL(set_metric, CMD_RET_FAILURE, atoi(argv[1]));
	return CMD_RET_SUCCESS;
}

int do_ifconfig_netmask(int argc, char *argv[])
{
	char host[128];
	struct sockaddr sa;
	struct aftype *ap = &inet_aftype;

	if (argc < 2)
		return CMD_RET_USAGE;

	safe_strncpy(host, argv[1], (sizeof host));
	if (ap->input(host, &sa) < 0) {
		if (ap->herror)
			ap->herror(host);

		return CMD_RET_FAILURE;
	}

	IFCONFIG_CALL(set_netmask, CMD_RET_FAILURE, sa);
	return CMD_RET_SUCCESS;
}

int do_ifconfig_hwaddr(int argc, char *argv[])
{
	char host[128];
	struct sockaddr sa;
	struct hwtype *hw = &ether_hwtype;

	if (argc < 2)
		return CMD_RET_USAGE;

	safe_strncpy(host, argv[1], (sizeof host));
	if (hw->input(host, &sa) < 0) {
		return CMD_RET_FAILURE;
	}

	IFCONFIG_CALL(set_hwaddr, CMD_RET_FAILURE,
			sa);

	return CMD_RET_SUCCESS;
}

int do_ifconfig_up(int argc, char *argv[])
{
	IFCONFIG_CALL(set_flags, CMD_RET_FAILURE, IFF_UP);
	return CMD_RET_SUCCESS;
}

int do_ifconfig_down(int argc, char *argv[])
{
	IFCONFIG_CALL(set_flags, CMD_RET_FAILURE, -IFF_UP);
	return CMD_RET_SUCCESS;
}

int do_ifconfig_promisc(int argc, char *argv[])
{
	unsigned ifflags = IFF_PROMISC;

	if (argv[0][0] == '-')
		ifflags = -IFF_PROMISC;

	IFCONFIG_CALL(set_flags, CMD_RET_FAILURE, ifflags);
	return CMD_RET_SUCCESS;
}

int do_ifconfig_monitor(int argc, char *argv[])
{
	ifconfig_handle_t *h;
	struct ifmediareq ifm = {{0}, };
	struct ifreq ifr = {{0}, };
	unsigned ifflags = IFF_PROMISC;
	int mode = IFM_IEEE80211_MONITOR, ret;

#define CALL(f)					\
	if ((ret = f))				\
		goto fail;

	h = ifconfig_open();
	if (argv[0][0] == '-') {
		ifflags = -IFF_PROMISC;
		mode = 0; /* STA */

		CALL(ifconfig_set_flags(h, ifname, ifflags));
		CALL(ifconfig_get_media(h, ifname, &ifm));
		ifr.ifr_media = ifm.ifm_current &~ IFM_OMASK;
		ifr.ifr_media |= mode;
		CALL(ifconfig_set_media(h, ifname, (struct ifmediareq *) &ifr));
	}else
	{
		CALL(ifconfig_get_media(h, ifname, &ifm));
		ifr.ifr_media = ifm.ifm_current &~ IFM_OMASK;
		ifr.ifr_media |= mode;
		CALL(ifconfig_set_media(h, ifname, (struct ifmediareq *) &ifr));
		CALL(ifconfig_set_flags(h, ifname, ifflags));
	}
	ifconfig_close(h);
	return 0;
 fail:
 	ifconfig_close(h);
	return CMD_RET_FAILURE;
}

#ifdef CONFIG_IFCONFIG_WIRELESS
int do_ifconfig_ssid(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	return set80211ssid(argv[1]);
}

int do_ifconfig_channel(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	return set80211channel(argv[1]);
}

int do_ifconfig_authmode(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	return set80211authmode(argv[1]);
}

int do_ifconfig_powersave(int argc, char *argv[])
{
	int flag = (argv[0][0] == '-' ? 0 : 1);
	return set80211powersave(argv[0], flag);
}

int do_ifconfig_pspoll(int argc, char *argv[])
{
	/* if no additional parameter, do default command without pspoll tx */
	if (argc < 2)
		return set80211pspoll("off");

	return set80211pspoll(argv[1]);
}

int do_ifconfig_powersavesleep(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	return set80211powersavesleep(argv[1]);
}

#ifdef CONFIG_IFCONFIG_HE_LTF_GI
int do_ifconfig_he_ltf_gi(int argc, char *argv[])
{
	int flag = (argv[0][0] == '-' ? 0 : 1);

    if (flag && argc < 3)
        return CMD_RET_USAGE;

    if (!flag && argc > 1)
        return CMD_RET_USAGE;

    return set80211heltfgi(&argv[1], flag);
}
#endif

#ifdef CONFIG_IFCONFIG_WME_FIX_AC
int do_ifconfig_wme_fixed_ac(int argc, char *argv[])
{
    int flag = (argv[0][0] == '-' ? 0 : 1);

    if (flag && argc < 2)
        return CMD_RET_USAGE;
    if (!flag && argc > 1)
        return CMD_RET_USAGE;
    return set80211wmeac(&argv[1], flag);
}
#endif

int do_ifconfig_rtsthreshold(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	return set80211rtsthreshold(argv[1]);
}

int do_ifconfig_protmode(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	return set80211protmode(argv[1]);
}

int do_ifconfig_txpower(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	return set80211txpower(argv[1]);
}

int do_ifconfig_bssid(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	return set80211bssid(argv[1]);
}

int do_ifconfig_scan(int argc, char *argv[])
{
	if (argc > 1)
		return CMD_RET_USAGE;
	return set80211scan(argv[0]);
}

int do_ifconfig_cwmin(int argc, char *argv[])
{
	int bss = (strncmp(argv[0], "bss:", 4) ? 0 : 1);

	if (argc < 3)
		return CMD_RET_USAGE;
	return set80211cwmin(argv[1], argv[2], bss);
}

int do_ifconfig_cwmax(int argc, char *argv[])
{
	int bss = (strncmp(argv[0], "bss:", 4) ? 0 : 1);

	if (argc < 3)
		return CMD_RET_USAGE;
	return set80211cwmax(argv[1], argv[2], bss);
}

int do_ifconfig_aifs(int argc, char *argv[])
{
	int bss = (strncmp(argv[0], "bss:", 4) ? 0 : 1);

	if (argc < 3)
		return CMD_RET_USAGE;
	return set80211aifs(argv[1], argv[2], bss);
}

int do_ifconfig_txoplimit(int argc, char *argv[])
{
	int bss = (strncmp(argv[0], "bss:", 4) ? 0 : 1);

	if (argc < 3)
		return CMD_RET_USAGE;
	return set80211txoplimit(argv[1], argv[2], bss);
}

int do_ifconfig_ack(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;

	int flag = (argv[0][0] == '-' ? 0 : 1);
	return set80211ack(argv[1], flag);
}

int do_ifconfig_dtimperiod(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	return set80211dtimperiod(argv[1]);
}

int do_ifconfig_bintval(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	return set80211bintval(argv[1]);
}

int do_ifconfig_maxretry(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;

	int v = atoi(argv[1]), flags;

	flags = getmodeflags(argv[1]);
	if (gettxparams())
		return CMD_RET_FAILURE;
	if (flags == 0) {		/* NB: no flags => current channel */
		flags = getcurchan()->ic_flags;
		_APPLY1(flags, txparams, maxretry, v);
	} else
		_APPLY(flags, txparams, maxretry, v);
	return set80211txparams(&txparams);
}

int do_ifconfig_fragthreshold(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	return set80211fragthreshold(argv[1]);
}

int do_ifconfig_burst(int argc, char *argv[])
{
	int flag = (argv[0][0] == '-' ? 0 : 1);
	return set80211burst(argv[0], flag);
}

int do_ifconfig_bmissreshold(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	return set80211bmissthreshold(argv[1]);
}

int do_ifconfig_shortgi(int argc, char *argv[])
{
	int flag = (argv[0][0] == '-' ? 0 : 1);
	return set80211shortgi(argv[0], flag);
}

int do_ifconfig_ampdu(int argc, char *argv[])
{
	int flag;

	if (!strncmp(argv[0], "ampdurx", 6))
		flag = 2;
	else if (!strncmp(argv[0], "-ampdurx", 7))
		flag = -2;
	else if (!strncmp(argv[0], "ampdutx", 6))
		flag = 1;
	else if (!strncmp(argv[0], "-ampdutx", 7))
		flag = -1;
	else if (!strncmp(argv[0], "ampdu", 4))
		flag = 3;
	else if (!strncmp(argv[0], "-ampdu", 5))
		flag = -3;
	else
		return CMD_RET_USAGE;

	return set80211ampdu(argv[0], flag);
}

int do_ifconfig_ampdulimit(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;

	return set80211ampdulimit(argv[1]);
}

int do_ifconfig_ampdudensity(int argc, char *argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;

	return set80211ampdudensity(argv[1]);
}

int do_ifconfig_inact(int argc, char *argv[])
{
	int flag = (argv[0][0] == '-' ? 0 : 1);
	return set80211inact(argv[0], flag);
}

int do_ifconfig_ht(int argc, char *argv[])
{
	int flag;

	if (!strncmp(argv[0], "ht20", 4))
		flag = 1;
	else if (!strncmp(argv[0], "-ht20", 5))
		flag = 0;
	else if (!strncmp(argv[0], "ht40", 4))
		flag = 3;
	else if (!strncmp(argv[0], "-ht40", 5))
		flag = 0;
	else if (!strncmp(argv[0], "ht", 2))
		flag = 3;
	else if (!strncmp(argv[0], "-ht", 3))
		flag = 0;
	else
		return CMD_RET_USAGE;

	return set80211htconf(argv[0], flag);
}

#ifdef CONFIG_LWIP_IPV6
/*
 * ifconfig <interface> inet6 add <ipv6address>/<prefixlength>
 * ifconfig <interface> inet6 del <ipv6address>/<prefixlength>
 */
int do_ifconfig_inet6(int argc, char *argv[])
{
	char host[128];
	char *cp;
	uint8_t add;
	uint32_t prefixlen;
	struct sockaddr_in6 sa;
	struct aftype *ap = &inet6_aftype;

	if (argc < 3)
		return CMD_RET_USAGE;

	if (!strcmp(argv[1], "add")) {
		add = 1;
	} else if (!strcmp(argv[1], "del")) {
		add = 0;
	} else {
		return CMD_RET_FAILURE;
	}

	safe_strncpy(host, argv[2], (sizeof host));
	if (ap->input(host, (struct sockaddr *)&sa) < 0) {
		if (ap->herror)
			ap->herror(host);
		return CMD_RET_FAILURE;
	}

	if ((cp = strchr(argv[2], '/'))) {
		prefixlen = atoi(cp + 1);
		if ((prefixlen < 0) || (prefixlen > 128))
			return CMD_RET_FAILURE;
	} else {
		prefixlen = 128;
	}

	IFCONFIG_CALL(set_ip6addr, CMD_RET_FAILURE, sa, prefixlen, add);
	return CMD_RET_SUCCESS;
}
#endif

int do_ifconfig_rifs(int argc, char *argv[])
{
	int flag = (argv[0][0] == '-' ? 0 : 1);
	return set80211rifs(argv[0], flag);
}

static int list_channels_1(void)
{
	return list_channels(1);
}

static int list_channels_0(void)
{
	return list_channels(0);
}

static int list_regdomain_1(void)
{
	return list_regdomain(1);
}

const static struct {
	const char *item;
	int (*show)(void);
} list_tab[] = {
	{"sta", list_stations},
	{"scan", list_scan},
	{"ap", list_scan},
	{"chan", list_channels_1} ,
	{"freq", list_channels_1},
	{"active", list_channels_0},
	{"keys", list_keys},
	{"caps", list_capabilities},
	{"mac", list_mac},
	{"wme", list_wme},
	{"wmm", list_wme},
	{"txpow", list_txpow},
	{"roam", list_roam},
	{"txparam", list_txparams},
	{"txparm", list_txparams},
	{"regdomain", list_regdomain_1},
	{"countries", list_countries},
	{"mesh", list_mesh},
};

int do_ifconfig_list(int argc, char *argv[])
{
#define	iseq(a,b)	(strncasecmp(a,b,strlen(a)) == 0)
	int i;

	if (argc > 1) {
		for (i = 0; i < ARRAY_SIZE(list_tab); i++) {
			if (iseq(argv[1], list_tab[i].item))
				return list_tab[i].show();
		}
		printf("Don't know how to list %s for %s\n", argv[1], ifname);
	}
	for (i = 0; i < ARRAY_SIZE(list_tab); i++) {
		printf("ifconfig <interface> list %s\n", list_tab[i].item);
	}
	return 0;
#undef iseq
}

#endif /* CONFIG_IFCONFIG_WIRELESS */

static const struct cli_cmd ifconfig_cmd[] = {
	CMDENTRY(mtu, do_ifconfig_mtu, "", (char *) -1),
	CMDENTRY(metric, do_ifconfig_metric, "", (char *) -1),
	CMDENTRY(netmask, do_ifconfig_netmask, "", (char *) -1),
	CMDENTRY(hw, do_ifconfig_hwaddr, "", "<mac addr>"),
	CMDENTRY(up, do_ifconfig_up, "", ""),
#ifdef CONFIG_LWIP_IPV6
	CMDENTRY(inet6, do_ifconfig_inet6, "", "add/del <ipv6 addr>/<prefix length>"),
#endif
	CMDENTRY(down, do_ifconfig_down, "", ""),
	CMDENTRY(promisc, do_ifconfig_promisc, "", ""),
	CMDENTRY(-promisc, do_ifconfig_promisc, "", ""),
	CMDENTRY(monitor, do_ifconfig_monitor, "", ""),
	CMDENTRY(-monitor, do_ifconfig_monitor, "", ""),
#ifdef CONFIG_IFCONFIG_SSID
	CMDENTRY(ssid, do_ifconfig_ssid, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_CHANNEL
	CMDENTRY(channel, do_ifconfig_channel, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_AUTHMODE
	CMDENTRY(authmode, do_ifconfig_authmode, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_POWERSAVE
	CMDENTRY(powersave, do_ifconfig_powersave, "", ""),
	CMDENTRY(-powersave, do_ifconfig_powersave, "", ""),
	CMDENTRY(powersavesleep, do_ifconfig_powersavesleep, "", "<sleep>"),
	/* Set WiFi PS_POLL : toggle command */
	CMDENTRY(pspoll, do_ifconfig_pspoll, "", ""),
#endif
#ifdef CONFIG_IFCONFIG_HE_LTF_GI
    CMDENTRY(heltfgi, do_ifconfig_he_ltf_gi, "", ""),
	CMDENTRY(-heltfgi, do_ifconfig_he_ltf_gi, "", ""),
#endif
#ifdef CONFIG_IFCONFIG_WME_FIX_AC
		CMDENTRY(wme, do_ifconfig_wme_fixed_ac, "", ""),
		CMDENTRY(-wme, do_ifconfig_wme_fixed_ac, "", ""),
#endif
#ifdef CONFIG_IFCONFIG_RTSTHRESHOLD
	CMDENTRY(rtsthreshold, do_ifconfig_rtsthreshold, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_PROTMODE
	CMDENTRY(protmode, do_ifconfig_protmode, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_TXPOWER
	CMDENTRY(txpower, do_ifconfig_txpower, "", ""),
#endif
#ifdef CONFIG_IFCONFIG_BSSID
	CMDENTRY(bssid, do_ifconfig_bssid, "", (char *) -1),
	CMDENTRY(ap, do_ifconfig_bssid, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_SCAN
	CMDENTRY(scan, do_ifconfig_scan, "", ""),
	CMDENTRY(-scan, do_ifconfig_scan, "", ""),
#endif
#ifdef CONFIG_IFCONFIG_WMM
	CMDENTRY(cwmin, do_ifconfig_cwmin, "", (char *) -1),
	CMDENTRY(cwmax, do_ifconfig_cwmax, "", (char *) -1),
	CMDENTRY(aifs, do_ifconfig_aifs, "", (char *) -1),
	CMDENTRY(txoplimit, do_ifconfig_txoplimit, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_ACK
	CMDENTRY(ack, do_ifconfig_ack, "", ""),
	CMDENTRY(-ack, do_ifconfig_ack, "", ""),
#endif
#ifdef CONFIG_IFCONFIG_WMM
	CMDENTRY(bss:cwmin, do_ifconfig_cwmin, "", (char *) -1),
	CMDENTRY(bss:cwmax, do_ifconfig_cwmax, "", (char *) -1),
	CMDENTRY(bss:aifs, do_ifconfig_aifs, "", (char *) -1),
	CMDENTRY(bss:txoplimit, do_ifconfig_txoplimit, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_DTIMPERIOD
	CMDENTRY(dtimperiod, do_ifconfig_dtimperiod, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_BINTVAL
	CMDENTRY(bintval, do_ifconfig_bintval, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_MAXRETRY
	CMDENTRY(maxretry, do_ifconfig_maxretry, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_FRAGTHRESHOLD
	CMDENTRY(fragthreshold, do_ifconfig_fragthreshold, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_BURST
	CMDENTRY(burst, do_ifconfig_burst, "", ""),
	CMDENTRY(-burst, do_ifconfig_burst, "", ""),
#endif
#ifdef CONFIG_IFCONFIG_BMISSTHRESHOLD
	CMDENTRY(bmissthreshold, do_ifconfig_bmissreshold, "", (char *) -1),
	CMDENTRY(bmiss, do_ifconfig_bmissreshold, "", ""),
#endif
#ifdef CONFIG_IFCONFIG_SHORTGI
	CMDENTRY(shortgi, do_ifconfig_shortgi, "", ""),
	CMDENTRY(-shortgi, do_ifconfig_shortgi, "", ""),
#endif
#ifdef CONFIG_IFCONFIG_AMPDU
	CMDENTRY(ampdu, do_ifconfig_ampdu, "", ""),
	CMDENTRY(-ampdu, do_ifconfig_ampdu, "", ""),
	CMDENTRY(ampdurx, do_ifconfig_ampdu, "", ""),
	CMDENTRY(-ampdurx, do_ifconfig_ampdu, "", ""),
	CMDENTRY(ampdutx, do_ifconfig_ampdu, "", ""),
	CMDENTRY(-ampdutx, do_ifconfig_ampdu, "", ""),
	CMDENTRY(ampdulimit, do_ifconfig_ampdulimit, "", (char *) -1),
	CMDENTRY(ampdudensity, do_ifconfig_ampdudensity, "", (char *) -1),
#endif
#ifdef CONFIG_IFCONFIG_INACT
	CMDENTRY(inact, do_ifconfig_inact, "", ""),
	CMDENTRY(-inact, do_ifconfig_inact, "", ""),
#endif
#ifdef CONFIG_IFCONFIG_HT
	CMDENTRY(ht, do_ifconfig_ht, "", ""),
	CMDENTRY(-ht, do_ifconfig_ht, "", ""),
	CMDENTRY(ht20, do_ifconfig_ht, "", ""),
	CMDENTRY(-ht20, do_ifconfig_ht, "", ""),
	CMDENTRY(ht40, do_ifconfig_ht, "", ""),
	CMDENTRY(-ht40, do_ifconfig_ht, "", ""),
#endif
#ifdef CONFIG_IFCONFIG_RIFS
	CMDENTRY(rifs, do_ifconfig_rifs, "", ""),
	CMDENTRY(-rifs, do_ifconfig_rifs, "", ""),
#endif
#ifdef CONFIG_IFCONFIG_LIST
	CMDENTRY(list, do_ifconfig_list, "",
		 "<sta|scan|chan|active|keys|caps|wme|mac|txpow|roam|txparam|redoamin|countries|mesh>"),
#endif
};

static void ifconfig_help_cmd(const struct cli_cmd *cmd)
{
	char *bracket[2] = {"[", "]"};
	const char *usage;

	if (cmd->usage != (char *) -1) {
		bracket[0] = bracket[1] = "";
		usage = cmd->usage;
	} else
		usage = cmd->name;

	printf("\tifconfig <interface> %s %s%s%s\n", cmd->name,
	       bracket[0], usage, bracket[1]);
}

static int ifconfig_help(int argc, char *argv[])
{
	const struct cli_cmd *cmd = ll_entry_get(struct cli_cmd, ifconfig, cmd);
	int ncmd = ARRAY_SIZE(ifconfig_cmd);

	printf("%s - %s\nUsage:\n", cmd->name, cmd->desc);
	if (argc == 1) {
		printf("\tifconfig <interface> <ip addr>\n");
		for (cmd = ifconfig_cmd; cmd < ifconfig_cmd + ncmd; cmd++)
			ifconfig_help_cmd(cmd);
	}
        /* printf("\tRefer to https://www.freebsd.org/cgi/man.cgi?ifconfig for more information\n"); */
	return 0;
}

static void ifconfig_cleanup(void)
{
#ifdef CONFIG_IFCONFIG_WIRELESS
	if (chaninfo) {
		free(chaninfo);
		chaninfo = NULL;
	}
	if (ifmr) {
		if (ifmr->ifm_ulist) {
			free(ifmr->ifm_ulist);
			ifmr->ifm_ulist = NULL;
		}
		free(ifmr);
		ifmr = NULL;
	}
#endif /* CONFIG_IFCONFIG_WIRELESS */

}

static int do_ifconfig(int argc, char *argv[])
{
	const struct cli_cmd *cmd;
	int ret = CMD_RET_SUCCESS;

	if (argc == 2 && strcmp(argv[1], "help") == 0)
		return ifconfig_help(argc-1, &argv[1]);

	argc--, argv++;
	ifname = (argc == 0) ? NULL : *argv;

	argc--, argv++;
	if (argc <= 0)
		return do_ifconfig_status();

	cmd = cli_find_cmd(argv[0], ifconfig_cmd, ARRAY_SIZE(ifconfig_cmd));
	if (cmd) {
		ret = cmd->handler(argc, argv);
		goto out;
	}
	if (do_ifconfig_addr(argc, argv) == CMD_RET_FAILURE)
		ret = CMD_RET_USAGE;

 out:
	ifconfig_cleanup();
	return ret;
}

CMD(ifconfig, do_ifconfig, "configure network interfaces", NULL);

#ifdef IEEE80211_MODE_11A
#undef IEEE80211_MODE_11A
#endif

#ifdef IEEE80211_MODE_FH
#undef IEEE80211_MODE_FH
#endif

#ifdef IEEE80211_MODE_TURBO_A
#undef IEEE80211_MODE_TURBO_A
#endif

#ifdef IEEE80211_MODE_TURBO_G
#undef IEEE80211_MODE_TURBO_G
#endif

#ifdef IEEE80211_MODE_STURBO_A
#undef IEEE80211_MODE_STURBO_A
#endif

#ifdef IEEE80211_MODE_11NA
#undef IEEE80211_MODE_11NA
#endif

#ifdef IEEE80211_MODE_HALF
#undef IEEE80211_MODE_HALF
#endif

#ifdef IEEE80211_MODE_QUARTER
#undef IEEE80211_MODE_QUARTER
#endif

#ifdef IEEE80211_MODE_VHT_2GHZ
#undef IEEE80211_MODE_VHT_2GHZ
#endif

#ifdef IEEE80211_MODE_VHT_5GHZ
#undef IEEE80211_MODE_VHT_5GHZ
#endif

#ifdef IEEE80211_MODE_HE_2GHZ
#undef IEEE80211_MODE_HE_2GHZ
#endif

#ifdef IEEE80211_MODE_HE_5GHZ
#undef IEEE80211_MODE_HE_5GHZ
#endif

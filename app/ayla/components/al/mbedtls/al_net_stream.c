/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <string.h>
#include <ayla/utypes.h>
#include <ayla/endian.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <al/al_clock.h>
#include <al/al_net_addr.h>
#include <al/al_net_stream.h>
#include <al/al_os_lock.h>
#include <al/al_os_mem.h>
#include <al/al_random.h>
#include <platform/pfm_ada_thread.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/platform.h>
#include <mbedtls/debug.h>
#if MBEDTLS_VERSION_MAJOR >= 3
#include <psa/crypto.h>
#endif
#ifndef AYLA_SCM_SUPPORT
#include <lwip/sockets.h>
#endif
#include <platform/pfm_net_socket.h>
#include "pfm_certs.h"
#include "pfm_rng.h"

#ifdef AYLA_SCM_SUPPORT
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <lwip/errno.h>

#define lwip_send        send
#define lwip_recv        recv
#define lwip_socket      socket
#define lwip_bind        bind
#define lwip_connect     connect
#define lwip_listen      listen
#define lwip_accept      accept
#define lwip_close       close
#define lwip_fcntl       fcntl
#define lwip_getsockname getsockname
#define lwip_setsockopt  setsockopt
#endif

#define PFM_NET_TLS_SEND_BUF_LEN	AL_NET_STREAM_WRITE_BUF_LEN
#define PFM_NET_TLS_RECV_BUF_LEN	800	/* buffer for receiving */
#define PFM_NET_TLS_RECV_CLR_BUF_LEN	600	/* buffer for cleartext rx */

#define PFM_NET_STREAM_TLS_TIMEOUT	20000	/* TLS handshake timeout (ms) */

#define PFM_NET_KEEP_ALIVE_ENABLE	1	/* enable TCP keep-alive */
#define PFM_NET_KEEP_ALIVE_IDLE		300	/* seconds of idle before KA */
#define PFM_NET_KEEP_ALIVE_INTVL	30	/* then send every 30 s */
#define PFM_NET_KEEP_ALIVE_COUNT	6	/* 6 times before closing */

/*
 * Do not allow TLS 1.0 or TLS 1.1 to be used.
 * This check is to prevent accidental mbedTLS errors.
 */
#if defined(MBEDTLS_SSL_PROTO_TLS1) || defined(MBEDTLS_SSL_PROTO_TLS1_1)
#error mbedTLS config must disable TLS 1.0 and TLS 1.1 support.  use TLS >= 1.2
#error Do not define MBEDTLS_SSL_PROTO_TLS1 or MBEDTLS_SSL_PROTO_TLS1_1.
#endif

/*
 * Require certificate time check.
 */
#ifndef MBEDTLS_HAVE_TIME_DATE
#error mbedTLS config must enable TLS certificate expiry check.
#error define MBEDTLS_HAVE_TIME_DATE in the mbedtls config header file.
#endif

#if CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC	/* ESP32 only */

/*
 * Custom buffer allocation size range.
 *
 * To reduce fragmentation and to reduce chances of allocation failure, we
 * reserve a single 16KB+ receive buffer for mbedtls.
 *
 * It was found experimentally that mbedtls allocates a receive buffer
 * of 16717 bytes, presumably 16384 bytes plus overhead.
 * We will reserve just one of these allocations after it is first allocated.
 *
 * In case the size changes in mbedtls, allow for a range of such allocations
 * and save the one that is used first.
 */
#define PFM_NET_STREAM_RES_BUF_MIN	16384
#define PFM_NET_STREAM_RES_BUF_MAX	17000

static void *pfm_net_stream_buf;	/* reserved buffer for mbedtls */
static size_t pfm_net_stream_buf_size;
static u8 pfm_net_stream_buf_in_use;
#endif

struct al_net_stream {
	enum al_net_stream_type type;
	mbedtls_ssl_context *tls;	/* mbedtls handle */
#ifdef TODO /* may make reconnection more efficient with session ID */
	mbedtls_ssl_session *sess_id;	/* TODO session ID to resume */
#endif
	u8 connected;
	u8 tls_handshake_complete;
	u8 recv_paused;
	u8 rx_close;		/* TCP closed by peer */
	u8 ref_count;		/* reference count */
	u16 send_len;		/* valid length of data in send buffer */
	u16 recv_len;		/* valid length of data in recv buffer */
	u16 recv_clr_len;	/* valid length of data in recv_clr_buf */
	u16 recv_paused_len;	/* amount of recv data remaining while paused */
	u16 sent_len;		/* amount to report sent in sent callback */
	enum al_err error;	/* rx/tx error occurred - need reconnect */

	struct pfm_net_socket pfm_sock;
	struct callback recv_continue_callback;
	struct callback sent_callback;

	void *cb_arg;
	enum al_err (*conn_cb)(void *arg, struct al_net_stream *, enum al_err);
	enum al_err (*recv_cb)(void *arg, struct al_net_stream *,
		void *buf, size_t);
	void (*sent_cb)(void *arg, struct al_net_stream *, size_t);
	void (*err_cb)(void *arg, enum al_err);
	void (*accept_cb)(void *arg, struct al_net_stream *,
	    const struct al_net_addr *peer_addr, u16 peer_port);

	char send_buf[PFM_NET_TLS_SEND_BUF_LEN];
	char recv_buf[PFM_NET_TLS_RECV_BUF_LEN];
	char *recv_clr_buf;
};

static mbedtls_x509_crt pfm_net_stream_ca_cert_list;
static mbedtls_x509_crt *pfm_net_stream_ca_certs;
static mbedtls_ssl_config pfm_net_stream_tls_conf;

static void pfm_net_stream_recv_cb(void *arg);
static void pfm_net_stream_accept_event(struct al_net_stream *, u8 flags);

static void pfm_net_stream_log(const char *fmt, ...) ADA_ATTRIB_FORMAT(1, 2);

static void pfm_net_stream_log(const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_put_va(MOD_LOG_SSL, fmt, args);
	ADA_VA_END(args);
}

static void pfm_net_stream_hold(struct al_net_stream *ps)
{
	ps->ref_count++;
	ASSERT(ps->ref_count);
}

static void pfm_net_stream_release(struct al_net_stream *ps)
{
	ASSERT(ps->ref_count);
	ps->ref_count--;
	if (!ps->ref_count) {
		al_os_mem_free(ps);
	}
}

static enum al_err pfm_al_err_from_errno(int errno_in)
{
	static const struct {
		u8 lwip_err;
		u8 al_err;
	} err_table[] = {
		{ENOMEM,	AL_ERR_ALLOC},
		{ENOBUFS,	AL_ERR_BUF},
		{EWOULDBLOCK,	AL_ERR_TIMEOUT},
		{EHOSTUNREACH,	AL_ERR_NOTCONN},
		{EINPROGRESS,	AL_ERR_IN_PROGRESS},
		{EINVAL,	AL_ERR_INVAL_VAL},
		{EADDRINUSE,	AL_ERR_BUSY},
		{EALREADY,	AL_ERR_INVAL_STATE},
		{EISCONN,	AL_ERR_BUSY},
		{ENOTCONN,	AL_ERR_CLSD},
		{ECONNABORTED,	AL_ERR_CLSD},
		{ECONNRESET,	AL_ERR_CLSD},
		{EIO,		AL_ERR_INVAL_VAL},
	};
	unsigned int i;

	for (i = 0; i < ARRAY_LEN(err_table); i++) {
		if (err_table[i].lwip_err == errno_in) {
			return (enum al_err)err_table[i].al_err;
		}
	}
	return AL_ERR_ERR;
}

#ifdef MBEDTLS_DEBUG_C /* may be enabled in mbedTLS */
static void pfm_net_stream_mbedtls_dbg(void *ctx, int level,
		const char *file, int line, const char *msg)
{
	const char *cp;

	cp = strrchr(file, '/');
	if (cp) {
		file = cp + 1;
	}
	pfm_net_stream_log(LOG_DEBUG "mbedtls %s:%04d: |%d| %s",
	    file, line, level, msg);
}

#ifdef PFM_NET_STREAM_HANDSHAKE_DEBUG
/*
 * Filter the verbose TLS logs to show just the client hello random bytes and
 * master secret.  These can be reformatted and given to Wireshark to
 * decrypt the TLS session.
 */
static void pfm_net_stream_mbedtls_handshake_dbg(void *ctx, int level,
		const char *file, int line, const char *msg)
{
	static u8 log_lines;

	if (!log_lines) {
		if (strstr(msg, "dumping 'client hello, random bytes' (32 ")) {
			log_lines = 3;
		} else if (strstr(msg, "dumping 'master secret' (48 ")) {
			log_lines = 4;
		} else {
			return;
		}
	}
	log_lines--;
	pfm_net_stream_mbedtls_dbg(ctx, level, file, line, msg);
}
#endif
#endif

#if CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC
/*
 * Custom allocation for mbedtls.
 * Reserve a 16 KB buffer for use to avoid being unable to allocate it due
 * to fragmentation.
 */
static void *pfm_net_stream_calloc(size_t count, size_t size)
{
	void *ptr;
	size_t len = count * size;

	/*
	 * If this request can be satisfied by the reserved buffer,
	 * give it that buffer.
	 */
	if (!pfm_net_stream_buf_in_use &&
	    len >= PFM_NET_STREAM_RES_BUF_MIN &&
	    len <= pfm_net_stream_buf_size) {
		pfm_net_stream_buf_in_use = 1;
		memset(pfm_net_stream_buf, 0, len);
		ptr = pfm_net_stream_buf;
	} else {
		ptr = calloc(count, size);

		/*
		 * If we haven't gotten the reserved buffer yet and this buffer
		 * would work, set it as the reserved buffer and mark it in use.
		 */
		if (ptr && !pfm_net_stream_buf &&
		    (len >= PFM_NET_STREAM_RES_BUF_MIN &&
		    len <= PFM_NET_STREAM_RES_BUF_MAX)) {
			pfm_net_stream_buf = ptr;
			pfm_net_stream_buf_size = len;
			pfm_net_stream_buf_in_use = 1;
		}
	}
	return ptr;
}

static void pfm_net_stream_free(void *ptr)
{
	if (!ptr) {
		return;
	}

	/*
	 * If freeing the reserved buffer, mark it free but don't free to heap.
	 */
	if (ptr == pfm_net_stream_buf) {
		ASSERT(pfm_net_stream_buf_in_use);
		pfm_net_stream_buf_in_use = 0;
		return;
	}
	free(ptr);
}
#endif

/*
 * Certificate validation hook.  Used only for logging issues.
 * We could override time checks, but won't.
 */
static int pfm_net_stream_cert_valid(void *data, mbedtls_x509_crt *crt,
		int depth, uint32_t *flags)
{
	if (*flags) {
		pfm_net_stream_log(LOG_WARN "%s: bad cert flags %#x",
		    __func__, (unsigned int)*flags);
	}
	return 0;
}

#ifdef MBEDTLS_PLATFORM_TIME_ALT
/*
 * Optionally provide AL time if configured in mbedtls_config.h.
 */
static mbedtls_time_t pfm_net_stream_time(mbedtls_time_t *time_p)
{
	u32 time;

	time = al_clock_get(NULL);
	if (time_p) {
		*time_p = time;
	}
	return (mbedtls_time_t)time;
}
#endif

/*
 * Perform first-time initialization.
 * Note: this is not MT-safe.
 */
static void pfm_net_stream_init(void)
{
	mbedtls_ssl_config *conf = &pfm_net_stream_tls_conf;
	mbedtls_ctr_drbg_context *rng;
	int rc;

	if (pfm_net_stream_ca_certs) {
		return;
	}

#if MBEDTLS_VERSION_MAJOR >= 3
	psa_crypto_init();
#endif

#ifdef MBEDTLS_PLATFORM_TIME_ALT
	mbedtls_platform_set_time(pfm_net_stream_time);
#endif

	/*
	 * Load CA certs.
	 */
	pfm_net_stream_ca_certs = &pfm_net_stream_ca_cert_list;
	mbedtls_x509_crt_init(pfm_net_stream_ca_certs);
	rc = mbedtls_x509_crt_parse(pfm_net_stream_ca_certs,
	    (unsigned char *)LINKER_TEXT_START(ca_certs_der_txt),
	   LINKER_TEXT_SIZE(ca_certs_der_txt));
	ASSERT(!rc);

	/*
	 * Set default config.
	 */
	mbedtls_ssl_config_init(conf);
	rc = mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_CLIENT,
	    MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
	ASSERT(!rc);

	mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(conf, pfm_net_stream_ca_certs, NULL);
	mbedtls_ssl_conf_verify(conf, pfm_net_stream_cert_valid, NULL);

	/*
	 * Init random number generator.
	 */
	rng = pfm_rng_mbedtls_drbg();
	ASSERT(rng);
	mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, rng);

#ifdef MBEDTLS_DEBUG_C
#ifdef PFM_NET_STREAM_HANDSHAKE_DEBUG
	mbedtls_debug_set_threshold(4); /* verbose debug */
	mbedtls_ssl_conf_dbg(conf, pfm_net_stream_mbedtls_handshake_dbg, NULL);
#else
	mbedtls_debug_set_threshold(3); /* informational debug */
	mbedtls_ssl_conf_dbg(conf, pfm_net_stream_mbedtls_dbg, NULL);
#endif
#endif

#if CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC
	mbedtls_platform_set_calloc_free(pfm_net_stream_calloc,
	    pfm_net_stream_free);
#else
#ifdef AYLA_ESP32_SUPPORT
	pfm_net_stream_log(LOG_WARN
	    "%s: menuconfig CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC recommended",
	    __func__);
#endif
#endif
}

static int pfm_net_stream_tls_init(struct al_net_stream *ps)
{
	mbedtls_ssl_context *tls = ps->tls;;
	int rc;

	pfm_net_stream_init();
	mbedtls_ssl_init(tls);
	rc = mbedtls_ssl_setup(tls, &pfm_net_stream_tls_conf);
	if (rc) {
		pfm_net_stream_log(LOG_ERR "%s: ssl_setup failed rc -%#x",
		    __func__, -rc);
		return -1;
	}
	return 0;
}

static void pfm_net_stream_setsockopt(int sock, int level, int optname,
		int optval)
{
	int rc;

	rc = lwip_setsockopt(sock, level, optname, &optval, sizeof(optval));
	if (rc < 0) {
		rc = errno;
		pfm_net_stream_log(LOG_WARN
		    "sock=%d, setsockopt %d %d failed err %d",
		    sock, level, optname, rc);
	}
}

/*
 * Asynchronous callback handler to report bytes sent.
 */
static void pfm_net_stream_sent_cb(void *arg)
{
	struct al_net_stream *ps = arg;
	size_t len;

	len = ps->sent_len;
	ps->sent_len = 0;
	if (ps->sent_cb) {
		ps->sent_cb(ps->cb_arg, ps, len);
	}
}

/*
 * Report bytes sent.
 */
static void pfm_net_stream_sent_report(struct al_net_stream *ps, size_t len)
{
	if (!len) {
		return;
	}
	ps->sent_len += (u16)len;
	pfm_callback_pend(&ps->sent_callback);
}

static struct al_net_stream *pfm_net_stream_new(enum al_net_stream_type type,
		int sock)
{
	struct al_net_stream *ps;
	int rc;
	size_t len;

	len = sizeof(*ps);
	if (type == AL_NET_STREAM_TLS) {
		len += sizeof(mbedtls_ssl_context);
		len += PFM_NET_TLS_RECV_CLR_BUF_LEN;
	}
	ps = al_os_mem_calloc(len);
	if (!ps) {
		goto out;
	}
	ps->type = type;
	if (type == AL_NET_STREAM_TLS) {
		ps->tls = (mbedtls_ssl_context *)(ps + 1);
		ps->recv_clr_buf = (char *)(ps->tls + 1);
		if (pfm_net_stream_tls_init(ps)) {
			goto free;
		}
	}
	rc = lwip_fcntl(sock, F_SETFL, O_NONBLOCK);
	if (rc < 0) {
		pfm_net_stream_log(LOG_ERR "sock=%d, fcntl failed err %d",
		    sock, errno);
		lwip_close(sock);
		goto free;
	}
	pfm_net_stream_setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
	    PFM_NET_KEEP_ALIVE_ENABLE);
	pfm_net_stream_setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE,
	    PFM_NET_KEEP_ALIVE_IDLE);
	pfm_net_stream_setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL,
	    PFM_NET_KEEP_ALIVE_INTVL);
	pfm_net_stream_setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT,
	    PFM_NET_KEEP_ALIVE_COUNT);
	ps->pfm_sock.sock = sock;

	callback_init(&ps->recv_continue_callback, pfm_net_stream_recv_cb, ps);
	callback_init(&ps->sent_callback, pfm_net_stream_sent_cb, ps);
	pfm_net_stream_hold(ps);
	pfm_net_stream_log(LOG_DEBUG "sock=%d, socket created",
	    ps->pfm_sock.sock);
	return ps;
free:
	al_os_mem_free(ps);
out:
	lwip_close(sock);
	return NULL;
}

struct al_net_stream *al_net_stream_new(enum al_net_stream_type type)
{
	int sock;

	switch (type) {
	case AL_NET_STREAM_TLS:
	case AL_NET_STREAM_TCP:
		sock = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock < 0) {
			pfm_net_stream_log(LOG_ERR "%s: socket failed err %d",
			    __func__, errno);
			return NULL;
		}
		break;
	default:
		return NULL;	/* unsupported type */
	}
	return pfm_net_stream_new(type, sock);
}

enum al_err al_net_stream_close(struct al_net_stream *ps)
{
	int rc;

	ps->rx_close = 1;
	ps->connected = 0;
	ps->conn_cb = NULL;
	ps->recv_cb = NULL;
	ps->sent_cb = NULL;
	ps->err_cb = NULL;
	if (ps->tls) {
		mbedtls_ssl_free(ps->tls);
		ps->tls = NULL;
	}
	pfm_net_socket_select_remove(&ps->pfm_sock);
	rc = lwip_close(ps->pfm_sock.sock);
	if (rc < 0) {
		rc = errno;
		pfm_net_stream_log(LOG_ERR "sock=%d, close err %d",
		    ps->pfm_sock.sock, rc);
		/* fall through to close in spite of error */
	} else {
		pfm_net_stream_log(LOG_DEBUG "sock=%d, socket closed",
		    ps->pfm_sock.sock);
	}
	pfm_net_stream_release(ps);
	return AL_ERR_OK;
}

void al_net_stream_set_arg(struct al_net_stream *ps, void *arg)
{
	ps->cb_arg = arg;
}

/*
 * Set error status, but don't issue err_cb callback.
 */
static void pfm_net_stream_error_set(struct al_net_stream *ps, enum al_err err)
{
	if (ps->error) {
		return;
	}
	ps->error = err;
	ps->pfm_sock.select_flags = 0;
}

/*
 * Set error status, and issue err_cb callback.
 */
static void pfm_net_stream_error(struct al_net_stream *ps, enum al_err err)
{
	if (ps->error) {
		return;
	}
	pfm_net_stream_error_set(ps, err);
	if (ps->err_cb) {
		ps->err_cb(ps->cb_arg, err);
	}
}

/*
 * Perform next step of TLS handshake.
 * Return 0 if complete.
 * Return 1 if handshake should continue.
 * Return negative mbedtls error otherwise.
 */
static int pfm_net_stream_tls_handshake(struct al_net_stream *ps)
{
	int rc;
	enum al_err al_err;

	rc = mbedtls_ssl_handshake(ps->tls);
	switch (rc) {
	case 0:
		ps->tls_handshake_complete = 1;
		al_err = AL_ERR_OK;
		break;
	case MBEDTLS_ERR_SSL_WANT_READ:
		ps->pfm_sock.select_flags |= PFM_NETF_READ;
		return 1;
	case MBEDTLS_ERR_SSL_WANT_WRITE:
		ps->pfm_sock.select_flags |= PFM_NETF_WRITE;
		return 1;
	case MBEDTLS_ERR_X509_CERT_VERIFY_FAILED:
		al_err = AL_ERR_CERT_EXP;
		break;
	default:
		pfm_net_stream_log(LOG_ERR "sock=%d, handshake rc -%#x",
		    ps->pfm_sock.sock, -rc);
		al_err = AL_ERR_CLSD;
		break;
	}
	if (ps->conn_cb) {
		pfm_net_stream_log(LOG_DEBUG "sock=%d, handshaked, err %d",
		    ps->pfm_sock.sock, al_err);
		ps->conn_cb(ps->cb_arg, ps, al_err);
	}
	return rc;
}

/*
 * Receive encrypted data for TLS.
 */
static ssize_t pfm_net_stream_recv_tls(struct al_net_stream *ps,
		void *buf, size_t len)
{
	ssize_t rc;

	rc = mbedtls_ssl_read(ps->tls, buf, len);
	if (rc < 0) {
		switch (rc) {
		case MBEDTLS_ERR_SSL_WANT_READ:
			ps->pfm_sock.select_flags |= PFM_NETF_READ;
			break;
		case MBEDTLS_ERR_SSL_WANT_WRITE:
			ps->pfm_sock.select_flags |= PFM_NETF_WRITE;
			break;
		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			pfm_net_stream_log(LOG_DEBUG
			    "sock=%d, tls_read close notify",
			    ps->pfm_sock.sock);
			rc = MBEDTLS_ERR_SSL_CONN_EOF;
			break;
		case MBEDTLS_ERR_SSL_CONN_EOF:
			pfm_net_stream_log(LOG_DEBUG
			    "sock=%d, tls_read SSL_CONN_EOF",
			    ps->pfm_sock.sock);
			break;
		default:
			pfm_net_stream_log(LOG_DEBUG
			    "sock=%d, tls_read rc -%#x", ps->pfm_sock.sock,
			    -rc);
			rc = -1;
			break;
		}
	} else {
		pfm_net_stream_log(LOG_DEBUG2 "sock=%d, tls_read %u bytes",
		    ps->pfm_sock.sock, rc);
	}
	return rc;
}

static ssize_t pfm_net_stream_recv_tcp(struct al_net_stream *ps,
		void *buf, size_t len)
{
	ssize_t rc;
	int err;
#ifdef AYLA_STRERROR_R_SUPPORT
	char err_buf[40];
#endif

	if (!ps->connected) {
		return -ENOTCONN;
	}

	rc = lwip_recv(ps->pfm_sock.sock, buf, len, 0);
	if (rc < 0) {
		err = errno;
		switch (err) {
		case EWOULDBLOCK:
			ps->pfm_sock.select_flags |= PFM_NETF_READ;
			return -err;
		case ENOTCONN:
		case ECONNRESET:
			rc = 0;
			break;
		default:
#ifdef AYLA_STRERROR_R_SUPPORT
			strerror_r(err, err_buf, sizeof(err_buf));
			pfm_net_stream_log(LOG_DEBUG
			    "sock=%d, lwip_recv err %d: %s", ps->pfm_sock.sock,
			    err, err_buf);
#else
			pfm_net_stream_log(LOG_DEBUG
			    "sock=%d, lwip_recv err %d", ps->pfm_sock.sock,
			    err);
#endif
			return -err;
		}
	}
	if (!rc) {
		pfm_net_stream_log(LOG_DEBUG "sock=%d, remote closed",
		    ps->pfm_sock.sock);
		ps->rx_close = 1;
	} else {
		pfm_net_stream_log(LOG_DEBUG2 "sock=%d, lwip_recv %u bytes",
		    ps->pfm_sock.sock, rc);
	}
	return rc;
}

/*
 * Handle reception of data.
 * This is from a select callback on the ADA thread.
 *
 * For TLS, ciphertext is received into the receive buffer, then mbedtls is
 * called to process it so we can deliver it.  The ciphertext could be
 * non-payload-related, such as a handshake or shutdown message.
 */
static void pfm_net_stream_recv(struct al_net_stream *ps)
{
	size_t len;
	size_t tlen;
	ssize_t rc;
	void *buf;
	enum al_err al_err;
	size_t rx_data;		/* amount of TCP or TLS received in one loop */

	/*
	 * Loop while able to receive anything from TCP or TLS.
	 * For TLS, the raw data will be picked up by BIO callbacks.
	 * Even if there is no TLS data to deliver, receive all TCP data that
	 * fits in the buffer.
	 */
	do {
		rx_data = 0;

		/*
		 * Receive raw data from TCP.
		 */
		if (sizeof(ps->recv_buf) <= ps->recv_len) {
			ps->pfm_sock.select_flags &= ~PFM_NETF_READ;
			len = 0;
		} else {
			len = sizeof(ps->recv_buf) - ps->recv_len;
		}
		if (len) {
			rc = pfm_net_stream_recv_tcp(ps,
			    ps->recv_buf + ps->recv_len, len);
			if (rc <= 0) {
				if (rc == 0) {
					goto closed;
				} else if (rc == -EWOULDBLOCK) {
					rc = 0;
				} else {
					al_err = AL_ERR_ABRT;
					goto err_cb;
				}
			}
			ps->recv_len += rc;
			rx_data += rc;
			ASSERT(rc <= len);
		}
		if (ps->recv_paused) {
			break;
		}

		/*
		 * Receive or handle ciphertext in TLS.
		 * This may access recv_buf.
		 */
		if (ps->tls) {
			if (!ps->tls_handshake_complete) {
				rc = pfm_net_stream_tls_handshake(ps);
				if (rc >= 0) {
#ifdef AYLA_SCM_SUPPORT
                    /* Can't stay indefinitely here because it will result in
                     * watchdog expiry.
                     * Get out now and come back after select will allow it.
                     */
                    break;
#else
					rx_data = 1;
					continue;
#endif
				}
			}
			ASSERT(PFM_NET_TLS_RECV_CLR_BUF_LEN >=
			    ps->recv_clr_len);
			tlen = PFM_NET_TLS_RECV_CLR_BUF_LEN - ps->recv_clr_len;
			if (tlen) {
				rc = pfm_net_stream_recv_tls(ps,
				    ps->recv_clr_buf + ps->recv_clr_len, tlen);
				if (rc < 0) {
					if (rc == MBEDTLS_ERR_SSL_CONN_EOF) {
						goto closed;
					}
					rc = 0;
				}
				ASSERT(rc <= tlen);
				ps->recv_clr_len += rc;
			}
			buf = ps->recv_clr_buf;
			len = ps->recv_clr_len;
		} else {
			buf = ps->recv_buf;
			len = ps->recv_len;
		}
		ps->recv_paused_len = len;

		/*
		 * Deliver the data via receive callback.
		 */
		if (!ps->recv_cb) {
			goto closed;
		}
		if (len) {
			rx_data = 1;
			(void)ps->recv_cb(ps->cb_arg, ps, buf, len);
		}
		if (!ps->tls && ps->rx_close) {	/* TCP connection closed */
			goto closed;
		}

		/*
		 * Remove the data actually consumed.
		 * If not all of the data was consumed, pause the stream.
		 * Move the remaining bytes up in the buffer.
		 */
		if (len && len == ps->recv_paused_len) {
			pfm_net_stream_log(LOG_DEBUG
			    "sock=%d, no data consumed", ps->pfm_sock.sock);
			/* fall through to pause */
		}
		if (len < ps->recv_paused_len) {
			pfm_net_stream_log(LOG_ERR
			    "sock=%d, flow control err len %zu paused len %u",
			    ps->pfm_sock.sock, len, ps->recv_paused_len);
			ps->recv_paused_len = len;
			/* fall through to remove data from buffer */
		}
		tlen = len - ps->recv_paused_len;	/* length consumed */
		if (buf == ps->recv_buf) {		/* TCP buf */
			ASSERT(tlen <= sizeof(ps->recv_buf));
			ASSERT(tlen <= ps->recv_len);
			memmove(ps->recv_buf,
			    ps->recv_buf + tlen,
			    ps->recv_len - tlen);
			ps->recv_len -= tlen;
		} else {
			ASSERT(tlen <= PFM_NET_TLS_RECV_CLR_BUF_LEN);
			ASSERT(tlen <= ps->recv_clr_len);
			memmove(ps->recv_clr_buf,
			    ps->recv_clr_buf + tlen,
			    ps->recv_clr_len - tlen);
			ps->recv_clr_len -= tlen;
		}
		if (ps->recv_paused_len) {
			ps->recv_paused = 1;
			ps->pfm_sock.select_flags &= ~PFM_NETF_READ;
			break;
		}
	} while (rx_data);
	return;

closed:
	ps->rx_close = 1;
	al_err = AL_ERR_CLSD;
err_cb:
	pfm_net_stream_error(ps, al_err);
}

static void pfm_net_stream_recv_cb(void *arg)
{
	struct al_net_stream *ps = arg;

	pfm_net_stream_recv(ps);
}

static ssize_t pfm_net_stream_send_tls(struct al_net_stream *ps,
		const void *buf, size_t len)
{
	ssize_t rc;

	pfm_net_stream_log(LOG_DEBUG2 "sock=%d, ssl_write %u bytes",
		ps->pfm_sock.sock, len);
	rc = mbedtls_ssl_write(ps->tls, buf, len);
	if (rc < 0) {
		if (rc == MBEDTLS_ERR_SSL_WANT_READ) {
			ps->pfm_sock.select_flags |= PFM_NETF_READ;
			return rc;
		}
		if (rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
			ps->pfm_sock.select_flags |= PFM_NETF_WRITE;
			return rc;
		}
		pfm_net_stream_log(LOG_ERR "sock=%d, ssl_write rc -%#x",
		    ps->pfm_sock.sock, -rc);
		pfm_net_stream_error_set(ps, AL_ERR_CLSD);
	} else if (rc != len) {
		pfm_net_stream_log(LOG_DEBUG2 "sock=%d, ssl_write %u/%u bytes",
			ps->pfm_sock.sock, rc, len);
	}
	return rc;
}

static ssize_t pfm_net_stream_send_tcp(struct al_net_stream *ps,
		const void *buf, size_t len)
{
	ssize_t rc;
	int err;
#ifdef AYLA_STRERROR_R_SUPPORT
	char err_buf[40];
#endif

	if (!ps->connected) {
		return -ENOTCONN;
	}

	rc = lwip_send(ps->pfm_sock.sock, buf, len, 0);
	if (rc < 0) {
		err = errno;
		if (err == EWOULDBLOCK) {
			ps->pfm_sock.select_flags |= PFM_NETF_WRITE;
			return -err;
		}
#ifdef AYLA_STRERROR_R_SUPPORT
		strerror_r(err, err_buf, sizeof(err_buf));
		pfm_net_stream_log(LOG_ERR "sock=%d, lwip_send err %d %s",
		    ps->pfm_sock.sock, err, err_buf);
#else
		pfm_net_stream_log(LOG_ERR "sock=%d, lwip_send err %d",
		    ps->pfm_sock.sock, err);
#endif

		pfm_net_stream_error_set(ps, AL_ERR_CLSD);
		return -err;
	} else {
		pfm_net_stream_log(LOG_DEBUG2 "sock=%d, lwip_send %u bytes",
		    ps->pfm_sock.sock, rc);
	}
	return rc;
}

/*
 * Handle deferred send of data.
 */
static void pfm_net_stream_send(struct al_net_stream *ps)
{
	ssize_t len;
	ssize_t rc;

	ps->pfm_sock.select_flags &= ~PFM_NETF_WRITE;
	if (!ps->send_len) {
		return;
	}
	if (ps->tls) {
		rc = pfm_net_stream_send_tls(ps, ps->send_buf, ps->send_len);
	} else {
		rc = pfm_net_stream_send_tcp(ps, ps->send_buf, ps->send_len);
	}
	if (rc <= 0) {
		return;
	}
	ASSERT(rc <= ps->send_len);
	len = ps->send_len - rc;	/* remaining length to send */
	if (len) {
		memmove(ps->send_buf, ps->send_buf + rc, len);
	}
	ps->send_len = len;
	if (ps->send_len) {
		ps->pfm_sock.select_flags |= PFM_NETF_WRITE;
	}
	pfm_net_stream_sent_report(ps, rc);
}

/*
 * Callback from MBEDTLS BIO to send encrypted data to the lower-level stream.
 * This is non-blocking and may send less than requested.
 */
static int pfm_net_stream_bio_send(void *ctx,
		const unsigned char *buf, size_t len)
{
	struct al_net_stream *ps = ctx;
	ssize_t rc;

	if (!ps->connected) {
		return MBEDTLS_ERR_SSL_CONN_EOF;
	}
	rc = pfm_net_stream_send_tcp(ps, buf, len);
	if (rc < 0) {
		switch (-rc) {
		case EWOULDBLOCK:
			ps->pfm_sock.select_flags |= PFM_NETF_WRITE;
			return MBEDTLS_ERR_SSL_WANT_WRITE;
		case ENETDOWN:
		case ENETUNREACH:
		case ENETRESET:
		case ECONNABORTED:
		case ECONNRESET:
		case ENOTCONN:
		case EHOSTDOWN:
		case EHOSTUNREACH:
			return MBEDTLS_ERR_SSL_CONN_EOF;
		default:
			pfm_net_stream_log(LOG_ERR "%s: send len %zu err %u",
			    __func__, len, rc);
			return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
		}
	}
	return rc;
}

/*
 * Callback from MBEDTLS BIO to get ciphertext data from the buffer and
 * receive more if possible.
 */
static int pfm_net_stream_bio_recv(void *ctx, unsigned char *buf, size_t len)
{
	struct al_net_stream *ps = ctx;
	size_t tlen;
	int rc;

	if (ps->recv_len) {
		if (len > ps->recv_len) {
			len = ps->recv_len;
		}
		memcpy(buf, ps->recv_buf, len);
		if (len < ps->recv_len) {
			tlen = ps->recv_len - len;
			memmove(ps->recv_buf, ps->recv_buf + len, tlen);
		}
		ps->recv_len -= len;
		ps->pfm_sock.select_flags |= PFM_NETF_READ;
		return len;
	}
	if (!ps->connected || ps->rx_close) {
		pfm_net_stream_log(LOG_DEBUG2 "%s: conn %u close %u EOF",
		    __func__, ps->connected, ps->rx_close);
		return MBEDTLS_ERR_SSL_CONN_EOF;
	}
	rc = pfm_net_stream_recv_tcp(ps, buf, len);
	if (rc < 0) {
		if (rc == -EWOULDBLOCK) {
			return MBEDTLS_ERR_SSL_WANT_READ;
		}
		pfm_net_stream_log(LOG_ERR "%s: rc -%#x", __func__, -rc);
		return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
	}
	if (!rc) {
		pfm_net_stream_log(LOG_DEBUG "%s: closed rc EOF", __func__);
		return MBEDTLS_ERR_SSL_CONN_EOF;
	}
	return rc;
}

static void pfm_net_stream_set_connected(struct al_net_stream *ps)
{
	if (ps->connected) {
		return;
	}
	if (ps->error) {
		return;
	}
	ps->connected = 1;
	if (ps->tls) {
		pfm_net_stream_log(LOG_DEBUG "sock=%d, handshaking",
		    ps->pfm_sock.sock);
		mbedtls_ssl_set_bio(ps->tls, ps, pfm_net_stream_bio_send,
		    pfm_net_stream_bio_recv, NULL);
		ps->tls_handshake_complete = 0;
		pfm_net_stream_tls_handshake(ps);
	} else {
		if (ps->conn_cb) {
			pfm_net_stream_log(LOG_DEBUG "sock=%d, connected",
			    ps->pfm_sock.sock);
			ps->conn_cb(ps->cb_arg, ps, AL_ERR_OK);
		}
	}
}

static void pfm_net_stream_except(struct al_net_stream *ps)
{
	int err;
	socklen_t optlen = sizeof(err);

	if (getsockopt(ps->pfm_sock.sock, SOL_SOCKET, SO_ERROR,
	    &err, &optlen)) {
		pfm_net_stream_log(LOG_DEBUG2 "sock=%d, sock %d err %d",
		    ps->pfm_sock.sock, ps->pfm_sock.sock, err);
		return;
	}
	pfm_net_stream_set_connected(ps);
}

/*
 * Select callback indicating connection completed or socket is readable
 * or writable.
 */
static void pfm_net_stream_select_cb(void *arg, u8 flags)
{
	struct al_net_stream *ps = arg;

	pfm_net_stream_hold(ps);
	if (ps->accept_cb) {
		pfm_net_stream_accept_event(ps, flags);
		pfm_net_stream_release(ps);
		return;
	}
	if (flags & PFM_NETF_EXCEPT) {
		pfm_net_stream_except(ps);
	}
	if (flags & (PFM_NETF_READ | PFM_NETF_WRITE)) {
		pfm_net_stream_set_connected(ps);
	}
	if (flags & PFM_NETF_READ) {
		pfm_net_stream_recv(ps);
	}
	if (flags & PFM_NETF_WRITE) {
		pfm_net_stream_send(ps);
	}
	pfm_net_stream_release(ps);
}

/*
 * Handle accept event on a listening socket.
 */
static void pfm_net_stream_accept_event(struct al_net_stream *listen_ps,
		u8 flags)
{
	struct al_net_stream *ps;
	int sock;
	int rc;
	struct sockaddr_in sa;
	socklen_t sa_len = sizeof(sa);
	struct al_net_addr addr;
	u16 port = 0;
	enum al_err err;

	sock = lwip_accept(listen_ps->pfm_sock.sock,
	    (struct sockaddr *)&sa, &sa_len);
	if (sock < 0) {
		rc = errno;
		err = pfm_al_err_from_errno(rc);
		pfm_net_stream_log(LOG_ERR
		    "%s: accept failed errno %d al_err %u",
		    __func__, rc, err);
		return;
	}

	ps = pfm_net_stream_new(listen_ps->type, sock);	/* frees sock on err */
	if (!ps) {
		pfm_net_stream_log(LOG_DEBUG2 "%s: stream alloc failed",
		    __func__);
		return;
	}
	memset(&addr, 0, sizeof(addr));
	if (sa.sin_family == AF_INET) {
		al_net_addr_set_ipv4(&addr, ntohl(sa.sin_addr.s_addr));
		port = ntohs(sa.sin_port);
	}
	listen_ps->accept_cb(listen_ps->cb_arg, ps, &addr, port);

	ps->pfm_sock.select_flags =
	    PFM_NETF_READ | PFM_NETF_WRITE | PFM_NETF_EXCEPT;
	ps->pfm_sock.select_cb = pfm_net_stream_select_cb;
	ps->pfm_sock.arg = ps;
	pfm_net_socket_select_add(&ps->pfm_sock);
}

/*
 * Connect for TCP.
 */
static enum al_err pfm_net_stream_connect_tcp(struct al_net_stream *ps,
	struct al_net_addr *peer_addr, u16 port)
{
	int rc;
	struct sockaddr_in sa;
	enum al_err err;

	memset(&sa, 0, sizeof(sa));
	sa.sin_len = sizeof(sa);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(al_net_addr_get_ipv4(peer_addr));
	sa.sin_port = htons(port);

	rc = lwip_connect(ps->pfm_sock.sock,
	    (struct sockaddr *)&sa, sizeof(sa));
	if (rc < 0) {
		rc = errno;
		if (rc == EINPROGRESS) {
			return AL_ERR_OK;
		}
		err = pfm_al_err_from_errno(rc);
		pfm_net_stream_log(LOG_ERR
		    "sock=%d, connect failed errno %d al_err %u",
		    ps->pfm_sock.sock, rc, err);
		return err;
	}
	pfm_net_stream_log(LOG_DEBUG "sock=%d, connecting", ps->pfm_sock.sock);
	return AL_ERR_OK;
}

/*
 * Connect for TLS.
 */
static enum al_err pfm_net_stream_connect_tls(struct al_net_stream *ps,
	const char *hostname, struct al_net_addr *peer_addr, u16 port)
{
	enum al_err err;
	int rc;

	rc = mbedtls_ssl_set_hostname(ps->tls, hostname);
	if (rc) {
		pfm_net_stream_log(LOG_ERR "sock=%d, set hostname rc -%#x",
		    ps->pfm_sock.sock, -rc);
		return AL_ERR_ERR;
	}
	err = pfm_net_stream_connect_tcp(ps, peer_addr, port);
	return err;
}

/*
 * Connect.
 */
enum al_err al_net_stream_connect(struct al_net_stream *ps,
	const char *hostname, struct al_net_addr *peer_addr, u16 port,
	enum al_err (*conn_cb)(void *arg, struct al_net_stream *,
	enum al_err err))
{
	enum al_err err;

	ps->conn_cb = conn_cb;
	ps->error = AL_ERR_OK;

	if (ps->type == AL_NET_STREAM_TLS) {
		err = pfm_net_stream_connect_tls(ps, hostname, peer_addr, port);
	} else {
		err = pfm_net_stream_connect_tcp(ps, peer_addr, port);
	}
	if (err) {
		return err;
	}

	ps->pfm_sock.select_flags =
	    PFM_NETF_READ | PFM_NETF_WRITE | PFM_NETF_EXCEPT;
	ps->pfm_sock.select_cb = pfm_net_stream_select_cb;
	ps->pfm_sock.arg = ps;
	pfm_net_socket_select_add(&ps->pfm_sock);
	return AL_ERR_OK;
}

/*
 * Listen.
 */
enum al_err al_net_stream_listen(struct al_net_stream *ps,
	const struct al_net_addr *local_addr, u16 local_port, int backlog,
	void (*accept_cb)(void *arg, struct al_net_stream *,
	    const struct al_net_addr *peer_addr, u16 peer_port))
{
	struct sockaddr_in sa;
	enum al_err err;
	int rc;

	if (ps->type != AL_NET_STREAM_TCP) {
		return AL_ERR_INVAL_TYPE;
	}
	ps->accept_cb = accept_cb;
	ps->error = AL_ERR_OK;

	memset(&sa, 0, sizeof(sa));
	sa.sin_len = sizeof(sa);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(al_net_addr_get_ipv4(local_addr));
	sa.sin_port = htons(local_port);

	rc = lwip_bind(ps->pfm_sock.sock, (struct sockaddr *)&sa, sizeof(sa));
	if (rc < 0) {
		rc = errno;
		err = pfm_al_err_from_errno(rc);
		pfm_net_stream_log(LOG_ERR "%s: bind failed errno %d al_err %u",
		    __func__, rc, err);
		return err;
	}

	ps->pfm_sock.select_flags = PFM_NETF_READ | PFM_NETF_EXCEPT;
	ps->pfm_sock.select_cb = pfm_net_stream_select_cb;
	ps->pfm_sock.arg = ps;
	pfm_net_socket_select_add(&ps->pfm_sock);

	rc = lwip_listen(ps->pfm_sock.sock, backlog);
	if (rc < 0) {
		rc = errno;
		pfm_net_socket_select_remove(&ps->pfm_sock);
		err = pfm_al_err_from_errno(rc);
		pfm_net_stream_log(LOG_ERR
		    "%s: listen failed errno %d al_err %u",
		    __func__, rc, err);
		return err;
	}
	return AL_ERR_OK;
}

int al_net_stream_is_established(struct al_net_stream *ps)
{
	return ps->connected;
}

void al_net_stream_continue_recv(struct al_net_stream *ps)
{
	ps->recv_paused = 0;
	ps->pfm_sock.select_flags |= PFM_NETF_READ;
	pfm_callback_pend(&ps->recv_continue_callback);
}

void al_net_stream_set_recv_cb(struct al_net_stream *ps,
	enum al_err (*recv_cb)(void *, struct al_net_stream *,
	void *, size_t))
{
	ps->recv_cb = recv_cb;
}

void al_net_stream_recved(struct al_net_stream *ps, size_t len)
{
	ASSERT(len <= ps->recv_paused_len);
	ps->recv_paused_len -= len;
}

enum al_err al_net_stream_write(struct al_net_stream *ps,
	const void *buf, size_t len)
{
	ssize_t rc;

	if (!ps->connected) {
		pfm_net_stream_log(LOG_DEBUG "sock=%d, not connected",
		    ps->pfm_sock.sock);
		return AL_ERR_NOTCONN;
	}
	if (ps->error) {
		pfm_net_stream_log(LOG_DEBUG "sock=%d, returning %u",
		    ps->pfm_sock.sock, ps->error);
		return ps->error;
	}
	if (len > sizeof(ps->send_buf)) {
		return AL_ERR_LEN;
	}

	/*
	 * If any send data is being buffered, we return AL_ERR_BUF.
	 *
	 * This is due to this restriction from mbedtls for mbedtls_ssl_write():
	 *	When this function returns MBEDTLS_ERR_SSL_WANT_WRITE/READ,
	 *	it must be called later with the same arguments, until it
	 *	returns a positive value.
	 *
	 * We violate that by sending a different buffer but with the
	 * same length and contents.
	 *
	 * The first such write encrypts and buffers the contents
	 * internally in mbedtls.
	 */
	if (ps->send_len) {
		return AL_ERR_BUF;
	}
	if (ps->tls) {
		rc = pfm_net_stream_send_tls(ps, buf, len);
		if (rc < 0) {
			pfm_net_stream_log(LOG_DEBUG
			    "sock=%d, send_tls rc -%#x", ps->pfm_sock.sock,
			    -rc);
			switch (rc) {
			case MBEDTLS_ERR_SSL_WANT_READ:
			case MBEDTLS_ERR_SSL_WANT_WRITE:
				break;
			case MBEDTLS_ERR_SSL_CONN_EOF:
				return AL_ERR_CLSD;
			default:
				return AL_ERR_ERR;
			}
			rc = 0;
		}
	} else {
		rc = pfm_net_stream_send_tcp(ps, buf, len);
		if (rc < 0) {
			switch (-rc) {
			case EINPROGRESS:
				break;
			case ENETDOWN:
			case ENETUNREACH:
			case ENETRESET:
			case ECONNABORTED:
			case ECONNRESET:
			case ENOTCONN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				pfm_net_stream_log(LOG_DEBUG2
				    "sock=%d, send_tcp errno %d (closed)",
				    ps->pfm_sock.sock, -rc);
				return AL_ERR_CLSD;
			default:
				pfm_net_stream_log(LOG_ERR
				    "sock=%d, send_tcp errno %d",
				    ps->pfm_sock.sock, -rc);
				return AL_ERR_ERR;
			}
			rc = 0;
		}
	}
	pfm_net_stream_sent_report(ps, rc);
	if (rc >= len) {
		return AL_ERR_OK;
	}

	/*
	 * Buffer the unsent portion.
	 */
	memcpy(ps->send_buf, (const u8 *)buf + rc, len - rc);
	ps->send_len = len - rc;
	if (ps->send_len) {
		ps->pfm_sock.select_flags |= PFM_NETF_WRITE;
	}
	return AL_ERR_OK;
}

enum al_err al_net_stream_output(struct al_net_stream *ps)
{
	if (ps->error) {
		return ps->error;
	}
	pfm_net_stream_send(ps);
	return ps->error;
}

void al_net_stream_set_sent_cb(struct al_net_stream *ps,
	void(*sent_cb)(void *, struct al_net_stream *, size_t))
{
	ps->sent_cb = sent_cb;
}

void al_net_stream_set_err_cb(struct al_net_stream *ps,
	void (*err_cb)(void *arg, enum al_err err))
{
	ps->err_cb = err_cb;
}

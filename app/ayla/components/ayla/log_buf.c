/*
 * Copyright 2011-2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdio.h>	/* for snprintf */

#include <ayla/utypes.h>
#include <ayla/endian.h>
#include <ayla/assert.h>
#include <ayla/log.h>
#include <ayla/clock.h>
#include <al/al_os_mem.h>
#include <al/al_os_lock.h>

#define LOG_BUF_DEF_SIZE	4096	/* default size for log buffer */

#define LOG_MSG_HEAD_MIN_COPY	2	/* sufficient to copy event from head */
#define LOG_SNAP_MAGIC	0x12345789	/* magic number */

u8 log_snap_saved;
#ifdef AYLA_LOG_SNAPSHOTS
static u8 log_snap_to_save;
#endif

/*
 * Log buffer structure for in memory log and snapshots saved to flash.
 *
 * The in and out offsets refer to where to write a new entry and the
 * offset of the oldest entry. If the two offsets are equal, out has reached
 * in and the buffer is empty. When making room in the buffer for new data,
 * at least one byte is always left unused to prevent in and out from being
 * equal when there is data in the buffer.
 *
 * The buffer wraps, so the data for the entry at the end of the buffer will
 * typically have a portion of its data at the end of the log buffer and the
 * remainder of its data at the beginning of the log buffer.
 *
 * Note: This structure is persisted to flash with snapshots. Changing it will
 * affect backward compatibility with previously saved snapshots.
 */
struct log_buf {
	u32	magic;		/* magic number for storage verification */
	u16	len;		/* buffer length */
	u16	in;		/* offset of where to write */
	u16	out;		/* offset of oldest entry */
	u16	count;		/* number of messages in buffer */
	u32	base_seq;	/* sequence number of oldest - 1 */
	u32	time;		/* time of snapshot */
	char    *buf;
};

/*
 * Context for reading log buffers or snapshots.
 */
struct log_buf_ctxt {
	struct log_buf *log;	/* log_buf or snapshot */
	u8	will_send;	/* true if logs will be sent to the cloud */
	u32	seq;		/* next sequence for reading */
};

static struct log_buf log_buf;
static u8 ada_log_buf_enabled;
static struct al_lock *log_buf_mutex;
static u32 log_buf_client_seq;	/* seq of last message sendable to cloud */
static u16 log_buf_client_size; /* bytes of logs in buf to be sent to cloud */
static void (*log_buf_notify)(int push); /* notify for log_client */

static void log_buf_get_bytes(struct log_buf *log, void *dst, u16 off, u16 cnt);
#ifdef AYLA_LOG_SNAPSHOTS
static struct log_buf_ctxt *log_buf_snapshot_open(unsigned int snapshot);
#endif

/*
 * Log events.
 * This is for disabling logs to the service between events, where
 * an event is a wakeup of the client thread.
 */
static u8 curr_event;
static u8 used_event;
static u8 log_buf_events[LOG_BUF_EVENT_MAX];

/*
 * Advance an offset pointer by a specified amount with wrap around
 * at the end of the buffer.
 */
static inline u16 log_buf_advance(u16 offset, u16 amount, u16 buf_len)
{
	return (offset + amount) % buf_len;
}

/*
 * Return the amount of currently unused space in the log buffer. Always
 * reserve one unused byte so in and out will only be equal when
 * the buffer is empty.
 */
static inline u16 log_buf_unused(struct log_buf *log)
{
	return (log->len + log->out - 1 - log->in) % log->len;
}

void log_buf_init(void)
{
	struct log_buf *log = &log_buf;
	size_t len;
	char *buf;

	if (log_buf_mutex) {
		return;
	}
	len = al_log_snap_size_get();
	if (!len) {
		len = LOG_BUF_DEF_SIZE;
	}
	if (len > sizeof(struct log_buf)) {
		len -= sizeof(struct log_buf);
	}
	if (len > MAX_U16) {
		len = MAX_U16;
	}
#ifdef AYLA_LOG_BUF_ALIGN4
	len &= ~3;
#endif
	buf = al_log_buf_alloc(len);
	ASSERT(buf);
	log_buf_mutex = al_os_lock_create();
	ada_log_buf_enabled = 1;
	log->buf = buf;
	log->len = len;
	log->magic = LOG_SNAP_MAGIC;
}

static void log_buf_lock(void)
{
	al_os_lock_lock(log_buf_mutex);
}

static void log_buf_unlock(void)
{
	al_os_lock_unlock(log_buf_mutex);
}

void log_buf_notify_set(void (*notify)(int push))
{
	log_buf_notify = notify;
}

/*
 * Return non-zero if the log message can be sent to the cloud.
 */
static u8 log_buf_log_can_send(struct log_msg_head *head)
{
	if (head->mod_nr & LOG_MOD_NOSEND) {
		return 0;
	}
	if (log_buf_events[head->event] & LOG_EVENT_NOSEND) {
		return 0;
	}
	return 1;
}

/*
 * Make room for message.
 * This removes the oldest full messages from the log until there is room.
 * Returns zero on success.
 * Called with lock held.
 */
static int log_buf_make_room(size_t req_len)
{
	struct log_msg_head head;
	struct log_buf *log = &log_buf;
	size_t tlen;
	u16 out;
	u16 room;

	if (req_len > log->len - 1) {
		return -1;	/* request is invalid */
	}
	out = log->out;
	room = log_buf_unused(log);
	while (room < req_len && out != log->in) {
		log_buf_get_bytes(log, &head, out, sizeof(head));
		tlen = head.len + sizeof(head) + sizeof(struct log_msg_tail);
		if (log_buf_log_can_send(&head)) {
			if (log_buf_client_size >= tlen) {
				log_buf_client_size -= tlen;
			}
		}
		room += tlen;
		out = log_buf_advance(out, tlen, log->len);
		log->count--;
		log->base_seq++;
	}
	log->out = out;
	return room < req_len;
}

#ifdef AYLA_LOG_BUF_ALIGN4
/*
 * Put data into the buffer using 32-bit aligned writes. Writes are cached
 * and flushed to the buffer in 32-bit chunks. The caller must ensure
 * that records are a multiple of 4 bytes in length to be sure the record
 * is fully written to the buffer. A record may be written using multiple
 * calls as long as the total length is a multiple of 4.
 */
static void log_buf_put_bytes(const void *data, size_t len)
{
	struct log_buf *log = &log_buf;
	u16 in;
	size_t rlen;
	static u32 write_buffer;
	static u8 wb_off;
	char *wb = (char *)&write_buffer;

	in = log->in;
	while (len > 0) {
		rlen = 4 - wb_off;
		if (rlen > len) {
			rlen = len;
		}
		memcpy(wb + wb_off, data, rlen);
		len -= rlen;
		data = (char *)data + rlen;
		wb_off += rlen;
		if (wb_off >= 4) {
			*(u32 *)&log->buf[in] = write_buffer;
			in = log_buf_advance(in, 4, log->len);
			wb_off = 0;
		}
	}
	log->in = in;
}

/*
 * Read data from the buffer using 32-bit aligned reads.
 */
static void log_buf_get_bytes(struct log_buf *log, void *dst, u16 off, u16 cnt)
{
	char *tgt = dst;
	u32 read_buffer;
	char *rb = (char *)&read_buffer;
	u8 rb_off = off & 0x3;	/* offset in 4 byte word */
	size_t rlen;

	off -= rb_off;	/* back up to 32-bit word boundary */
	while (cnt) {
		read_buffer = *(u32 *)&log->buf[off];
		rlen = 4 - rb_off;
		if (rlen > cnt) {
			rlen = cnt;
		}
		memcpy(tgt, rb + rb_off, rlen);
		cnt -= rlen;
		tgt += rlen;
		off = log_buf_advance(off, 4, log->len);
		rb_off = 0;
	}
}
#else

/*
 * Put data into the buffer, handling buffer wrap, if necessary.
 * Called with lock held.
 */
static void log_buf_put_bytes(const void *data, size_t len)
{
	struct log_buf *log = &log_buf;
	u16 in;
	size_t rlen;

	in = log->in;
	while (len > 0) {
		if (log->out > in) {
			rlen = log->out - 1 - in;
		} else {
			rlen = log->len - in;
		}
		if (len < rlen) {
			rlen = len;
		}
		ASSERT(in < log->len);
		ASSERT(in + rlen <= log->len);
		memcpy(log->buf + in, data, rlen);
		len -= rlen;
		data = (char *)data + rlen;
		in = log_buf_advance(in, rlen, log->len);
	}
	log->in = in;
}

/*
 * Get bytes from offset in buffer, allowing for wrap, without moving pointers.
 */
static void log_buf_get_bytes(struct log_buf *log, void *dst, u16 off, u16 cnt)
{
	char *tgt;
	u16 tmp;

	tgt = dst;
	tmp = log->len - off;
	if (cnt < tmp) {
		tmp = cnt;
	}
	memcpy(tgt, log->buf + off, tmp);
	cnt -= tmp;
	if (cnt) {
		memcpy(tgt + tmp, log->buf, cnt);
	}
}
#endif

/*
 * Put a log message into the log buffer.
 * This may not be called from an interrupt routine.
 */
void log_buf_put(u8 mod_nr, enum log_sev sev, u32 mtime,
		u32 time, u32 msec, const char *msg, size_t len)
{
	struct log_buf *log = &log_buf;
	struct log_msg_head head;
	struct log_msg_tail tail;
	size_t size;
#ifdef AYLA_LOG_BUF_ALIGN4
	size_t padded_size;
	size_t padding;
	size_t padded_len;
	static u32 pad;
#endif

	if (!ada_log_buf_enabled) {
		return;
	}
#ifdef AYLA_LOG_BUF_ALIGN4
	/* insure padded message length will be less than MAX_U8 */
	if (len > (MAX_U8 - 3)) {
		len = MAX_U8 - 3;
	}
#else
	if (len > MAX_U8) {
		len = MAX_U8;
	}
#endif
	size = len + sizeof(head) + sizeof(tail);
#ifdef AYLA_LOG_BUF_ALIGN4
	padded_size = (size + 3) & ~0x3;
	padding = padded_size - size;
	size = padded_size;
	padded_len = len + padding;
#endif

	log_buf_lock();
	if (log_buf_make_room(size)) {
		log_buf_unlock();
		return;
	}

	head.magic = LOG_V2_MAGIC;
	head.resvd[0] = 0;
	head.mod_nr = mod_nr;
	head.sev = sev;
#ifdef AYLA_LOG_BUF_ALIGN4
	head.len = (u8)padded_len;
	tail.len = (u8)padded_len;
#else
	head.len = (u8)len;
	tail.len = (u8)len;
#endif
	head.event = curr_event;
	used_event = 1;
	head.mtime = mtime;
	head.time = time;
	head.msec = msec;

	log_buf_put_bytes(&head, sizeof(head));
	log_buf_put_bytes(msg, len);
#ifdef AYLA_LOG_BUF_ALIGN4
	if (padding) {
		log_buf_put_bytes(&pad, padding);
	}
#endif
	log_buf_put_bytes(&tail, sizeof(tail));
	log->count++;

	if (log_buf_log_can_send(&head) && log_buf_notify) {
		log_buf_client_size += size;
		log_buf_client_seq = log->base_seq + log->count;

		/*
		 * Notify log client.  Indicate push if pending data is
		 * more than 3/4 of the log buffer.
		 * This heuristic doesn't address the issue where the buffer
		 * is mostly occupied by logs that will not be sent.
		 */
		log_buf_notify(log_buf_client_size > ((log->len * 3) / 4));
	}
	log_buf_unlock();
}

struct log_buf_ctxt *log_buf_open(unsigned int snapshot)
{
	struct log_buf *log = &log_buf;
	struct log_buf_ctxt *ctxt;

	if (snapshot) {
#ifdef AYLA_LOG_SNAPSHOTS
		return log_buf_snapshot_open(snapshot);
#else
		return NULL;
#endif
	}

	ctxt = al_os_mem_calloc(sizeof(*ctxt));
	if (!ctxt) {
		return NULL;
	}
	ctxt->log = log;
	ctxt->seq = log->base_seq;
	return ctxt;
}

void log_buf_ctxt_will_send(struct log_buf_ctxt *ctxt)
{
	ctxt->will_send = 1;	/* mark this context as the log client */
}

void log_buf_close(struct log_buf_ctxt *ctxt)
{
	struct log_buf *log = ctxt->log;

	if (log != &log_buf) {
		al_os_mem_free(log->buf);
	}
	al_os_mem_free(ctxt);
}

/*
 * See log_buf_get_next.
 */
static ssize_t log_buf_get_next_no_lock(struct log_buf_ctxt *ctxt, void *buf,
    size_t len)
{
	struct log_msg_head head;
	struct log_msg_tail *tail;
	struct log_buf *log = ctxt->log;
	u16 next;
	u32 seq;
	size_t clen;

	if (!log->len) {
		return 0;
	}

	/*
	 * Skip older messages.
	 */
	next = log->out;
	seq = log->base_seq;
	while (next != log->in && clock_gt(ctxt->seq, seq)) {
		log_buf_get_bytes(log, &head, next, LOG_MSG_HEAD_MIN_COPY);
		if (head.magic != LOG_V2_MAGIC) {
			return -2;
		}
		clen = head.len + sizeof(head) + sizeof(*tail);
		next = log_buf_advance(next, clen, log->len);
		seq++;
	}
	if (next == log->in) {
		ctxt->seq = seq;
		return 0;
	}

	log_buf_get_bytes(log, &head, next, sizeof(head));
	if (head.magic != LOG_V2_MAGIC) {
		return -2;
	}
	clen = sizeof(head) + head.len + sizeof(*tail);

	/*
	 * For the log client, skip any messages we should not send to
	 * the service because the event was itself involved in logging
	 * to the service.
	 */
	while (ctxt->will_send && !log_buf_log_can_send(&head)) {
		next = log_buf_advance(next, clen, log->len);
		seq++;
		if (next == log->in) {
			ctxt->seq = seq;
			return 0;
		}
		log_buf_get_bytes(log, &head, next, sizeof(head));
		if (head.magic != LOG_V2_MAGIC) {
			return -2;
		}
		clen = head.len + sizeof(head) + sizeof(*tail);
	}

	if (clen >= len) {
		return -1;
	}
	log_buf_get_bytes(log, buf, next, clen);
	tail = (struct log_msg_tail *)((char *)buf + sizeof(head) + head.len);
	if (head.len != tail->len) {
		return -2;
	}
	ctxt->seq = seq + 1;
	clen -= sizeof(*tail);
	((char *)buf)[clen++] = '\0';	/* null terminate message string */
	return clen;
}

ssize_t log_buf_get_next(struct log_buf_ctxt *ctxt, void *buf, size_t len)
{
	size_t clen;
	u8 is_active_log;
	u8 enable = 0;

	if (len <= 0 || !ctxt) {
		return 0;
	}

	is_active_log = ctxt->log == &log_buf;
	if (is_active_log) {
		/* acquire lock and disable logging */
		log_buf_lock();
		enable = ada_log_buf_enabled;
		ada_log_buf_enabled = 0;
	}

	clen = log_buf_get_next_no_lock(ctxt, buf, len);

	if (is_active_log) {
		ada_log_buf_enabled = enable;
		log_buf_unlock();
	}
	return clen;
}

u32 log_buf_seq_get(struct log_buf_ctxt *ctxt)
{
	return ctxt->seq;
}

void log_buf_seq_set(struct log_buf_ctxt *ctxt, u32 seq)
{
	ctxt->seq = seq;
}

void log_buf_seq_set_tail(struct log_buf_ctxt *ctxt, u16 lines)
{
	struct log_buf *log = ctxt->log;

	if (lines >= log->count) {
		ctxt->seq = log->base_seq;
		return;
	}
	ctxt->seq = log->base_seq + log->count - lines;
}

void log_buf_seq_set_end(struct log_buf_ctxt *ctxt)
{
	struct log_buf *log = ctxt->log;

	ctxt->seq = log->base_seq + log->count;
}

int log_buf_has_more(struct log_buf_ctxt *ctxt)
{
	struct log_buf *log = ctxt->log;
	u32 latest_seq;

	if (log->out == log->in) {
		return 0;
	}
	if (ctxt->will_send) {
		if (!log_buf_client_size) {
			return 0;
		}
		latest_seq = log_buf_client_seq;
	} else {
		latest_seq = log->base_seq + log->count;
	}
	return clock_gt(latest_seq, ctxt->seq);
}

/*
 * Create a new event. Called in the client thread.
 * This is for disabling logs to the service between events, where
 * an event is a wakeup of the client thread.
 *
 * Each log entry is associated with an event number, and each event number
 * has its own flag value that says whether it should be logged to the
 * service or not.
 */
void log_buf_new_event(void)
{
	if (used_event) {
		curr_event = (curr_event + 1) % LOG_BUF_EVENT_MAX;
		used_event = 0;
	}
	log_buf_events[curr_event] = LOG_EVENT_SEND;
}

/*
 * Modify event mask of the corresponding event.
 */
void log_buf_send_event(u8 event_mask)
{
	log_buf_events[curr_event] = event_mask;
}

#ifndef AYLA_LOG_SNAPSHOTS

/*
 * Stubs for snapshots.
 */
void log_save(void)
{
}

/*
 * Erase log snapshots.
 */
int log_snap_erase(void)
{
	return 0;
}

/*
 * Get info on saved log snapshots.
 */
int log_snap_stat(struct log_snap *snap, unsigned int count)
{
	return 0;
}

int log_snap_count(size_t *max, int *auto_overwrite)
{
	*max = 0;
	*auto_overwrite = 0;
	return 0;
}

/*
 * Show snapshot on console.
 */
void log_snap_show(unsigned long snapshot, int time_only)
{
}

#else /* AYLA_LOG_SNAPSHOTS */

/*
 * Save log to flash if there's room.
 *
 * Snapshots may be FIFO or may be limited to when flash area fills, depending
 * on lower-layer.
 *
 * This function runs on the shallow interrupt stack.
 */
void log_save(void)
{
	struct log_buf *log = &log_buf;
	int rc;
	int snapshot;

	if (!ada_log_buf_enabled) {
		return;
	}
	if (!log_snap_to_save) {
		log_snap_stat(NULL, 0);		/* finds oldest snapshot */
		if (!log_snap_to_save) {
			log_snap_to_save = 1;
		}
	}
	snapshot = log_snap_to_save;

	ada_log_buf_enabled = 0;
	log->time = clock_utc();
	rc = al_log_snap_save(snapshot, log, offsetof(struct log_buf, buf),
	    log->buf, log->len);
	ada_log_buf_enabled = 1;

	if (rc) {
		log_warn(LOG_MOD_DEFAULT, "log_save: save rc %d", rc);
		return;
	}
	log_info("log_save: saved snapshot %u", snapshot);
	log_snap_saved++;
}

/*
 * Erase all log snapshots.
 */
int log_snap_erase(void)
{
	int rc;

	rc = al_log_snap_erase(0);
	if (rc < 0) {
		log_err(LOG_MOD_DEFAULT, "log snap erase err rc %d", rc);
	}
	return rc;
}

/*
 * Decide whether saved log snapshot buffer is valid.
 */
static int log_snap_is_valid(struct log_buf *log)
{
	if (log->magic != LOG_SNAP_MAGIC) {
		return 0;
	}
	if (log->in > log->len || log->out > log->len) {
		return 0;
	}
	return 1;
}

/*
 * Open a snapshot for reading.
 */
static struct log_buf_ctxt *log_buf_snapshot_open(unsigned int snapshot)
{
	struct log_buf_ctxt *ctxt;
	struct log_buf *log;
	char *buf = NULL;
	int rc;

	ctxt = al_os_mem_calloc(sizeof(*ctxt) + sizeof(*log));
	if (!ctxt) {
		log_put(LOG_ERR "log_buf: snapshot %u alloc failed",
		    snapshot);
		return NULL;
	}

	log = (struct log_buf *)(ctxt + 1);
	ctxt->log = log;

	rc = al_log_snap_read(snapshot, 0, log, offsetof(struct log_buf, buf));
	if (rc < 0) {
		goto err;
	}
	if (!log_snap_is_valid(log)) {
		log_put(LOG_ERR "log_buf: snapshot %u not valid",
		    snapshot);
		goto err;
	}

	buf = al_os_mem_calloc(log->len);
	if (!buf) {
		log_put(LOG_ERR "log_buf: snapshot %u buffer alloc failed",
		    snapshot);
		goto err;
	}

	rc = al_log_snap_read(snapshot, offsetof(struct log_buf, buf), buf,
	    log->len);
	if (rc < 0) {
		log_put(LOG_ERR "log_buf: snapshot %u buf read failed",
		    snapshot);
		goto err;
	}

	log->buf = buf;

	ctxt->seq = log->base_seq;
	return ctxt;

err:
	al_os_mem_free(buf);
	al_os_mem_free(ctxt);
	return NULL;
}

/*
 * Get info on saved log snapshots.
 */
int log_snap_stat(struct log_snap *snap, unsigned int count)
{
	struct log_buf buf;
	struct log_buf *log;
	unsigned int i;
	ssize_t len;
	u32 oldest = MAX_U32;
	int valid = 0;

	for (i = 1;
	    (len = al_log_snap_read(i, 0, &buf, sizeof(buf))) > 0; i++) {
		if (len < sizeof(buf)) {
			log_put(LOG_ERR "log_snap_snap %u short read len %zd",
			    i, len);
			continue;
		}
		log = &buf;
		if (log_snap_is_valid(log)) {
			valid++;
		} else {
			log->time = 0;
			log->len = 0;
		}
		if (log->time < oldest) {
			oldest = log->time;
			log_snap_to_save = i;
		}
		if (i < count) {
			snap->size = log->len;
			snap->time = log->time;
			snap++;
		}
	}
	return valid;
}

int log_snap_count(size_t *max, int *auto_overwrite)
{
	int count;

	count = log_snap_stat(NULL, 0);
	log_snap_saved = count;
	*max = al_log_snap_space(sizeof(struct log_buf), auto_overwrite);
	return count;
}

/*
 * Show log status.
 * Used at boot to warn of saved snapshots, and if there's no space left.
 */
void log_snap_status(void)
{
	size_t max;
	int count;
	int auto_overwrite;

	count = log_snap_count(&max, &auto_overwrite);
	if (count) {
		log_warn(LOG_MOD_DEFAULT, "log: %d fault log snapshots saved%s",
		    count, ((count < max) || auto_overwrite) ?
		    "" : ", no space left");
	}
}

/*
 * Show snapshot on console.
 */
void log_snap_show(unsigned long snapshot, int time_only)
{
	struct log_buf_ctxt *ctxt;
	struct log_buf *log;
	char clock_buf[CLOCK_FMT_LEN];
	char buf[LOG_ENTRY_MAX_SZ];
	struct log_msg_head *head = (struct log_msg_head *)buf;
	const char *msg = buf + sizeof(*head);
	ssize_t len;
	const char *subsys;
	u32 day;
	u32 last_day = 0;
	char sev;

	ctxt = log_buf_open(snapshot);
	if (!ctxt) {
		printcli("snapshot %lu open failed", snapshot);
		return;
	}
	log = ctxt->log;

	if (snapshot) {
		clock_fmt(clock_buf, sizeof(clock_buf), log->time);
		last_day = log->time / (60 * 60 * 24);	/* convert to day */
		printcli("snapshot %lu saved %s", snapshot, clock_buf);
	}
	if (time_only) {
		goto out;
	}

	while ((len = log_buf_get_next(ctxt, buf, sizeof(buf))) > 0) {
		sev = '?';
		if (head->sev < LOG_SEV_LIMIT) {
			sev = log_sev_chars[head->sev];
		}

		subsys = log_mod_get_name(head->mod_nr);
		if (!subsys) {
			subsys = "?";
		}

		/*
		 * Show full date once a day, otherwise just show hh:mm:ss + ms.
		 */
		day = head->time / (60 * 60 * 24);
		clock_fmt(clock_buf, sizeof(clock_buf), head->time);
		printcli("snap %lu: %s.%3.3u %c %s: %s",
		    snapshot,
		    &clock_buf[day == last_day ? CLOCK_FMT_TIME : 0],
		    head->msec, sev, subsys, msg);
		last_day = day;
	}
	if (len < 0) {
		printcli("%s: garbled snapshot", __func__);
	}
out:
	log_buf_close(ctxt);
}

#endif /* AYLA_LOG_SNAPSHOTS */

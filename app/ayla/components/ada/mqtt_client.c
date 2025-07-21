/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ayla/callback.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/clock.h>
#include <ayla/tlv.h>
#include <ayla/timer.h>
#include <ayla/ipaddr_fmt.h>

#include <al/al_utypes.h>
#include <al/al_clock.h>
#include <al/al_os_lock.h>
#include <al/al_os_mem.h>

#include <al/al_net_dns.h>
#include <al/al_net_stream.h>
#include "client_lock.h"
#include "client_timer.h"
#include "mqtt_client.h"
#include "mqtt_packet.h"

#define MQTT_CLIENT_LOG_DEBUG		/* define for more logging */

/*
 * Macro to make logging easier
 */
#define MQTT_CLIENT_LOG(_mc, _level, _format, ...) \
	mqtt_client_log(_mc, _level, "%s: " _format, \
	__func__, ##__VA_ARGS__)

#ifdef MQTT_CLIENT_LOG_DEBUG
#define MQTT_CLIENT_DEBUG MQTT_CLIENT_LOG
#else
#define MQTT_CLIENT_DEBUG(_mc, _level, _format, ...) do { } while (0)
#endif

#define MAX_MQTT_PACKET_ID 65535
#define MAX_SUB_TOPICS 5
#define MQTT_SUB_FAILED_CODE 0x80
#define MQTT_CLIENT_BUF_LEN 416 /* Max header len. Saw 386 on connect. */
#define MQTT_CLIENT_RETRY_MAX 3
#define MQTT_CLIENT_DNS_WAIT 10000
#define MQTT_CLIENT_NET_WAIT 50000
#define MQTT_CLIENT_CONN_WAIT 10000
#define MQTT_CLIENT_PING_RESP_WAIT 20000
#define MQTT_CLIENT_SUB_WAIT 10000
#define MQTT_CLIENT_UNSUB_WAIT 10000
#define MQTT_CLIENT_MSG_WAIT 10000
#define MQTT_CLIENT_PUB_SEND_WAIT 5000
#define MQTT_CLIENT_PUB_RECV_WAIT 10000
#define MQTT_CLIENT_PUBREL_WAIT 10000

#define MQTT_CLIENT_CONACK_LEN 4
#define MQTT_CLIENT_PUBACK_LEN 4
#define MQTT_CLIENT_PUBREC_LEN 4
#define MQTT_CLIENT_PUBREL_LEN 4
#define MQTT_CLIENT_PUBCOMP_LEN 4
#define MQTT_CLIENT_SUBACK_LEN 5
#define MQTT_CLIENT_UNSUBACK_LEN 4
#define MQTT_CLIENT_PINGRESP 2

/*
 * Extract fields from the MQTT header byte.
 * Not using bitfields to avoid ordering issues.
 */
#define MQTT_HEADER_RETAIN_GET(byte)	((byte) & BIT(0))
#define MQTT_HEADER_QOS_GET(byte)	(((byte) >> 1) & 3)
#define MQTT_HEADER_DUP_GET(byte)	(((byte) >> 3) & 1)
#define MQTT_HEADER_TYPE_GET(byte) \
	((enum mqtt_packet_msg_type)(((byte) >> 4) & 0xf))

/*
 * Fields filled in by deserializer.
 */
struct mqtt_client_msg {
	enum mqtt_client_qos qos;
	u8 retained;
	u8 dup;
	u16 id;
	void *payload;
	size_t payloadlen;
};

struct mqtt_client_msg_handles {
	const char *topic_filter;
	struct timer sub_timer;
	struct timer unsub_timer;
	struct mqtt_client *mc;
	u8 sub_outstanding;
	u8 unsub_outstanding;
	u8 is_valid;
	u16 sub_id;
	u16 unsub_id;
	size_t (*sub_handle)(struct mqtt_client *mc, void *arg,
		const void *payload, size_t len, size_t total_len);
	void (*suback_cb)(struct mqtt_client *mc, void *arg,
		enum al_err err);
	void (*unsuback_cb)(struct mqtt_client *mc, void *arg,
		enum al_err err);
	enum mqtt_client_qos sub_qos;
};

struct mqtt_client_data_info {
	char data[AL_NET_STREAM_WRITE_BUF_LEN];
	size_t len;
	struct mqtt_client_data_info *next;
};

struct mqtt_client_publish_info {
	u8 pub_outstanding;
	u8 pub_retry;
	u16 pub_id;
	u32 total_len;
	u32 sent_len;
	enum mqtt_client_qos pub_qos;
	void (*pub_completed)(struct mqtt_client *mc, void *arg,
		enum al_err err);
	struct timer pub_timer;
	const char *pub_topic;
	struct mqtt_client *mc;
	struct al_lock *data_lock;
	struct mqtt_client_data_info *queue_head;
	struct mqtt_client_data_info *queue_tail;
	struct mqtt_client_publish_info *next;
};

enum mqtt_client_connect_state {
	MQTT_DISCONNECTED,
	MQTT_DISCONNECTING,
	MQTT_CONNECTING,
	MQTT_CONNECTED,
};

struct mqtt_client {
	/* pfm mqtt */
	struct mqtt_packet_connect_data condata;
	unsigned int next_packetid;
	enum mqtt_client_connect_state conn_state;
	u16 mqtt_id;
	u8 ping_outstanding;
	u8 msg_is_repeat;
	u8 msg_recv_outstanding;
	u8 msg_index;
	u16 msg_id;
	enum mqtt_client_qos msg_qos;
	size_t msg_payload_total_len;
	size_t msg_payload_recv_len;
	struct mqtt_client_publish_info *pub_list;

	void *arg;

	void (*connack_cb)(struct mqtt_client *mc,
		enum mqtt_client_conn_err err, void *arg);
	void (*err_cb)(struct mqtt_client *mc,
		enum al_err err, void *arg);
	void (*disconnected_cb)(struct mqtt_client *mc, void *arg);
	void (*sent_cb)(void *arg);

	/* Message handlers are indexed by subscription topic */
	struct mqtt_client_msg_handles msg_hdls[MAX_SUB_TOPICS];

	/* mqtt client */
	struct al_net_stream *pcb;	/* connecting stream */
	char host[80];			/* server host name or IP address */
	u16 host_port;
	u8 ssl_enable;
	u8 retries;
	u8 paused;
	struct al_net_addr host_addr;	/* server ip address */
	enum mod_log_id mod_log_id;
	struct timer mc_timer;
	struct timer mc_dns_timer;
	struct timer mc_keepalive_timer;
	struct timer mc_msg_timer;
	struct al_net_dns_req dns_req;
	unsigned char mqtt_header[MQTT_CLIENT_BUF_LEN];
	size_t disconnect_len;		/* pending send length of disconnect */
};

static u16 gv_mqtt_id_count;

static const char *mqtt_client_msg_types[] = MQTT_PACKET_MSG_TYPES;

static void mqtt_client_getdnshostip(struct mqtt_client *mc);
static void mqtt_client_info_reset(struct mqtt_client *mc);
static void mqtt_client_msg_qos_handle(struct mqtt_client *mc);
static void mqtt_client_err(struct mqtt_client *mc, enum al_err err);
static void mqtt_client_log(struct mqtt_client *, const char *level,
	const char *fmt, ...) ADA_ATTRIB_FORMAT(3, 4);

static const char *mqtt_client_msg_type(enum mqtt_packet_msg_type type)
{
	const char *name = NULL;

	if ((unsigned)type < ARRAY_LEN(mqtt_client_msg_types)) {
		name = mqtt_client_msg_types[type];
	}
	if (!name) {
		name = "unknown";
	}
	return name;
}

/*
 * Prints log messages for the mod_log_id of this http_client.
 */
static void mqtt_client_log(struct mqtt_client *mc,
	const char *level, const char *fmt, ...)
{
	ADA_VA_LIST args;
	char buf[LOG_LINE];
	enum log_mod_id mod = (enum log_mod_id) MOD_LOG_MQTT;

	if (mc) {
		snprintf(buf, sizeof(buf), "%cmc (id=%d) %s", level[0],
		    mc->mqtt_id, fmt);
		fmt = buf;
	} else {
		snprintf(buf, sizeof(buf), "%cmc (unknown) %s",
		    level[0], fmt);
		fmt = buf;
	}
	ADA_VA_START(args, fmt);
	log_put_va((u8)mod, fmt, args);
	ADA_VA_END(args);
}

static void mqtt_client_timers_cancel(struct mqtt_client *mc)
{
	ASSERT(client_locked);
	client_timer_cancel(&mc->mc_timer);
	client_timer_cancel(&mc->mc_dns_timer);
	client_timer_cancel(&mc->mc_keepalive_timer);
	client_timer_cancel(&mc->mc_msg_timer);
}

static void mqtt_client_close(struct mqtt_client *mc)
{
	enum al_err err;
	struct al_net_stream *pcb;

	ASSERT(client_locked);
	pcb = mc->pcb;

	if (!pcb) {
		return;
	}
	err = (enum al_err)al_net_stream_close(pcb);
	if (err != AL_ERR_OK) {
		MQTT_CLIENT_LOG(mc, LOG_ERR,
		    "stream close error: %s", al_err_string(err));
	}
	mc->pcb = NULL;
}

/*
 * Abort connection.  Set disconnected but retain PCB.
 */
void mqtt_client_abort(struct mqtt_client *mc)
{
	struct al_net_stream *pcb = mc->pcb;

	ASSERT(client_locked);
	if (pcb) {
		al_net_stream_set_sent_cb(pcb, NULL);
		al_net_stream_set_recv_cb(pcb, NULL);
		al_net_stream_set_err_cb(pcb, NULL);
	}
	mqtt_client_info_reset(mc);
}

/*
 * Close PCB and call the connack_cb
 */
static void mqtt_client_abort_connack(struct mqtt_client *mc,
	enum mqtt_client_conn_err err)
{
	mqtt_client_abort(mc);
	if (mc->connack_cb) {
		mc->connack_cb(mc, err, mc->arg);
	}
}

/*
 * Close PCB and call the err_cb
 */
static void mqtt_client_abort_err(struct mqtt_client *mc,
	enum al_err err)
{
	MQTT_CLIENT_LOG(mc, LOG_ERR, "err %d", err);
	mqtt_client_abort(mc);
	if (mc->err_cb) {
		mc->err_cb(mc, err, mc->arg);
	}
}

static unsigned int mqtt_client_get_next_packetid(
	struct mqtt_client *mc)
{
	if (mc->next_packetid == MAX_MQTT_PACKET_ID) {
		mc->next_packetid = 1;
	} else {
		mc->next_packetid++;
	}

	return mc->next_packetid;
}

static void mqtt_client_reset_message_handle(
	struct mqtt_client_msg_handles *handle)
{
	client_timer_cancel(&handle->sub_timer);
	client_timer_cancel(&handle->unsub_timer);

	al_os_mem_free((void *)handle->topic_filter);
	memset(handle, 0, sizeof(struct mqtt_client_msg_handles));
}

static void mqtt_client_reset_msg_info(struct mqtt_client *mc)
{
	mc->msg_id = 0;
	mc->msg_is_repeat = 0;
	mc->msg_index = MAX_SUB_TOPICS;
	mc->msg_payload_recv_len = 0;
	mc->msg_payload_total_len = 0;
	mc->msg_qos = MQTT_QOS_0;
	mc->msg_recv_outstanding = 0;
}

/*
 * Check for match of topic to a name in the topic_filter.
 *
 * Assume topic filter and name is in correct format
 * # can only be at end
 * + and # can only be next to separator
 */
static char mqtt_client_is_topic_matched(const char *topic_filter,
	const char *topic_name)
{
	int len = strlen(topic_name);
	const char *curf = topic_filter;
	const char *curn = topic_name;
	const char *curn_end = curn + len;
	const char *nextpos;

	while (*curf && curn < curn_end) {
		if (*curn == '/' && *curf != '/') {
			break;
		}

		if (*curf != '+' && *curf != '#' && *curf != *curn) {
			break;
		}

		/* skip until we meet the next separator, or end of string */
		if (*curf == '+') {
			nextpos = curn + 1;
			while (nextpos < curn_end && *nextpos != '/') {
				nextpos = ++curn + 1;
			}
		} else if (*curf == '#') {
			curn = curn_end - 1;/* skip until end of string */
		}
		curf++;
		curn++;
	};

	return (curn == curn_end) && (*curf == '\0');
}

static int mqtt_client_message_filter(struct mqtt_client *mc,
	const char *topic_name)
{
	int i;
	const char *topic_filter;

	for (i = 0; i < MAX_SUB_TOPICS; i++) {
		topic_filter = mc->msg_hdls[i].topic_filter;
		if (topic_filter != 0 &&
		    (!strcmp(topic_name, topic_filter) ||
		    mqtt_client_is_topic_matched(topic_filter, topic_name))) {
			break;
		}
	}
	return i;
}

static enum al_err mqtt_client_add_data_to_pub(
	struct mqtt_client_publish_info *pub_info, const void *buf, u32 len)
{
	struct mqtt_client_data_info *data_info;
	enum al_err ret_err;
	char *p_dst;
	size_t cp_size;

	al_os_lock_lock(pub_info->data_lock);
	while (len) {
		data_info = pub_info->queue_tail;
		if (NULL != data_info && \
		    data_info->len < sizeof(data_info->data)) {
			cp_size = sizeof(data_info->data) - data_info->len;
			if (cp_size > len) {
				cp_size = len;
			}
			p_dst = data_info->data + data_info->len;
		} else {
			data_info =
			    (struct mqtt_client_data_info *)al_os_mem_calloc(
			    sizeof(struct mqtt_client_data_info));
			if (NULL == data_info) {
				ret_err = AL_ERR_BUF;
				goto on_exit;
			}
			cp_size = (sizeof(data_info->data) > len) ? \
			    (len) : (sizeof(data_info->data));
			p_dst = data_info->data;
			if (NULL == pub_info->queue_head) {
				pub_info->queue_head = data_info;
				pub_info->queue_tail = data_info;
			} else {
				pub_info->queue_tail->next = data_info;
				pub_info->queue_tail = data_info;
			}
		}
		memcpy(p_dst, buf, cp_size);
		data_info->len += cp_size;
		buf = (const char *)buf + cp_size;
		len -= cp_size;
	}
	ret_err = AL_ERR_OK;
on_exit:
	al_os_lock_unlock(pub_info->data_lock);
	return ret_err;
}

static struct mqtt_client_data_info *mqtt_client_pub_get_data(
	struct mqtt_client_publish_info *pub_info)
{
	struct mqtt_client_data_info *node;

	ASSERT(pub_info);
	al_os_lock_lock(pub_info->data_lock);
	node = pub_info->queue_head;
	if (node) {
		pub_info->queue_head = node->next;
		if (pub_info->queue_head == NULL) {
			pub_info->queue_tail = NULL;
		}
		node->next = NULL;
	}
	al_os_lock_unlock(pub_info->data_lock);
	return node;
}

static void mqtt_client_free_data_list(
	struct mqtt_client_publish_info *pub_info)
{
	struct mqtt_client_data_info *data_info;

	while (1) {
		data_info = mqtt_client_pub_get_data(pub_info);
		if (!data_info) {
			break;
		}
		al_os_mem_free(data_info);
	}
}

static void mqtt_client_add_pub_info_to_list(
	struct mqtt_client_publish_info **pub_list,
	struct mqtt_client_publish_info *pub_info)
{
	struct mqtt_client_publish_info *node;

	if (*pub_list) {
		for (node = *pub_list; 1; node = node->next) {
			if (node->next == NULL) {
				node->next = pub_info;
				break;
			}
		}
	} else {
		*pub_list = pub_info;
	}
}

static void mqtt_client_remove_pub_info_from_list(
	struct mqtt_client_publish_info **pub_list,
	struct mqtt_client_publish_info *pub_info)
{
	struct mqtt_client_publish_info *head;
	struct mqtt_client_publish_info *node;

	for (head = *pub_list, node = head->next; head;
	    node = node->next, head = head->next) {
		if (head == pub_info) {
			head = node;
			break;
		}
	}
	*pub_list = head;
}

static struct mqtt_client_publish_info *mqtt_client_get_outstanding_pub_info(
	struct mqtt_client_publish_info *pub_list)
{
	struct mqtt_client_publish_info *node;

	if (pub_list) {
		for (node = pub_list; node; node = node->next) {
			if (node->pub_outstanding == 1) {
				break;
			}
		}
	} else {
		node = NULL;
	}

	return node;
}

static struct mqtt_client_publish_info *mqtt_client_get_pub_info_by_id(
	struct mqtt_client_publish_info *pub_list, u16 pub_id)
{
	struct mqtt_client_publish_info *node;

	if (pub_list) {
		for (node = pub_list; node; node = node->next) {
			if (node->pub_id == pub_id) {
				break;
			}
		}
	} else {
		node = NULL;
	}

	return node;
}

static struct mqtt_client_publish_info *mqtt_client_pub_info_new(
	struct mqtt_client *mc)
{
	struct mqtt_client_publish_info *pub_info;

	pub_info = al_os_mem_calloc(sizeof(*pub_info));
	if (!pub_info) {
		return NULL;
	}
	mqtt_client_add_pub_info_to_list(&mc->pub_list, pub_info);
	return pub_info;
}

static void mqtt_client_pub_info_free(struct mqtt_client *mc,
	struct mqtt_client_publish_info *pub_info)
{
	mqtt_client_remove_pub_info_from_list(&mc->pub_list, pub_info);

	al_os_mem_free((void *)pub_info->pub_topic);

	if (pub_info->queue_head) {
		mqtt_client_free_data_list(pub_info);
	}
	al_os_lock_destroy(pub_info->data_lock);
	al_os_mem_free(pub_info);
}

static void mqtt_client_publish_complete(struct mqtt_client *mc,
		struct mqtt_client_publish_info *pub_info, enum al_err err)
{
	void (*pub_completed)(struct mqtt_client *mc, void *arg,
		enum al_err err);

	pub_completed = pub_info->pub_completed;
	client_timer_cancel(&pub_info->pub_timer);
	mqtt_client_pub_info_free(mc, pub_info);
	if (pub_completed) {
		pub_completed(mc, mc->arg, err);
	}
}

static void mqtt_client_cleanup_pub_list(struct mqtt_client *mc)
{
	struct mqtt_client_publish_info *head = mc->pub_list;

	while (head) {
		client_timer_cancel(&head->pub_timer);
		mqtt_client_remove_pub_info_from_list(&mc->pub_list, head);

		al_os_mem_free((void *)head->pub_topic);

		if (head->queue_head) {
			mqtt_client_free_data_list(head);
		}
		al_os_lock_destroy(head->data_lock);
		al_os_mem_free(head);
		head = mc->pub_list;
	}
}

/*
 * Low level function used to send data on the stream to the server.
 */
static enum al_err mqtt_client_send_buf(struct mqtt_client *mc,
	const void *buf, u16 len)
{
	enum al_err err;

	if (!mc->pcb) {
		return AL_ERR_CLSD;
	}
	err = al_net_stream_write(mc->pcb, buf, len);
	if (err) {
		if (err == AL_ERR_BUF) {
			return err;
		}
		MQTT_CLIENT_LOG(mc, LOG_ERR, "len %u err %d", len, err);
	}
	return err;
}

/*
 * Interface for the client to complete the send request.
 * This should only be called from the send callback when it has completed the
 * final send.
 */
static void mqtt_client_send_complete(struct mqtt_client *mc)
{
	if (!mc->pcb) {
		return;
	}
	al_net_stream_output(mc->pcb);
}

static void mqtt_client_connect_packet_send(struct mqtt_client *mc)
{
	int len;

	memset(mc->mqtt_header, 0, sizeof(mc->mqtt_header));
	len = mqtt_packet_serialize_connect(mc->mqtt_header,
	    sizeof(mc->mqtt_header), &mc->condata);

	mc->retries++;
	client_timer_set(&mc->mc_timer, MQTT_CLIENT_CONN_WAIT);
	mqtt_client_send_buf(mc, mc->mqtt_header, len);
	mqtt_client_send_complete(mc);
}

#ifdef MQTT_CLIENT_LOG_DEBUG
static const char *mqtt_client_connack_string(int connack_code)
{
	switch (connack_code) {
	case 0:
		return "Connection Accepted.";
	case 1:
		return "Connection Refused: unacceptable protocol version.";
	case 2:
		return "Connection Refused: identifier rejected.";
	case 3:
		return "Connection Refused: broker unavailable.";
	case 4:
		return "Connection Refused: bad user name or password.";
	case 5:
		return "Connection Refused: not authorised.";
	default:
		return "Connection Refused: unknown reason.";
	}
}
#endif

static size_t mqtt_client_connack_handle(struct mqtt_client *mc,
	u8 *buf, size_t len)
{
	int rc;
	u8 session_present = 0;
	u8 connack_code = 0;

	client_timer_cancel(&mc->mc_timer);
	rc = mqtt_packet_deserialize_connack(&session_present, &connack_code,
	    buf, len);
	if (rc == 1) {
		MQTT_CLIENT_DEBUG(mc, LOG_DEBUG,
		    "session_present = %d, %s", session_present,
		    mqtt_client_connack_string(connack_code));

		mc->retries = 0;
		if (!connack_code) {
			if (mc->condata.keep_alive) {
				client_timer_set(&mc->mc_keepalive_timer,
				    mc->condata.keep_alive * 1000);
			}
			mc->conn_state = MQTT_CONNECTED;
		} else {
			mc->conn_state = MQTT_DISCONNECTED;
			mqtt_client_abort(mc);
		}

		if (mc->connack_cb) {
			mc->connack_cb(mc,
			    (enum mqtt_client_conn_err)connack_code, mc->arg);
		}
	} else {
		mqtt_client_connect_packet_send(mc);
	}
	return MQTT_CLIENT_CONACK_LEN;
}


static void mqtt_client_publish_ack_send(struct mqtt_client *mc,
	enum mqtt_packet_msg_type type, u16 packet_id)
{
	int len;

	memset(mc->mqtt_header, 0, sizeof(mc->mqtt_header));
	len = mqtt_packet_serialize_ack(mc->mqtt_header,
	    sizeof(mc->mqtt_header), type, 0, packet_id);

	mqtt_client_send_buf(mc, mc->mqtt_header, len);
	mqtt_client_send_complete(mc);
}

static size_t mqtt_client_message_handle(struct mqtt_client *mc,
	u8 *buf, size_t len)
{
	int rc;
	char *topic_name = NULL;
	struct mqtt_client_msg msg;
	int int_qos;
	int mqtt_header_len;
	int index;
	int current_len;
	size_t consume_len;

	rc = mqtt_packet_deserialize_publish(&msg.dup, &int_qos, &msg.retained,
	    &msg.id, &topic_name, (unsigned char **)&msg.payload,
	    (int *)&msg.payloadlen, buf, len);

	if (rc == 1) {
		mqtt_header_len = (u8 *)msg.payload - buf;
		if (msg.payloadlen + mqtt_header_len >= len) {
			/* take all of buffer */
			current_len = len - mqtt_header_len;
		} else {
			/* take up only one message */
			current_len = msg.payloadlen;
		}
		/* limit what we deliver to what the upper level can buffer */
		if (current_len > MQTT_CLIENT_SUB_BUF_LEN) {
			current_len = MQTT_CLIENT_SUB_BUF_LEN;
		}

		MQTT_CLIENT_DEBUG(mc, LOG_DEBUG,
		    "id=%u, len=%u, qos=%d, payloadlen=%u, "
		    "mqtt_header_len=%d, current_len=%d",
		    msg.id, len, int_qos, msg.payloadlen, mqtt_header_len,
		    current_len);

		index = mqtt_client_message_filter(mc, topic_name);
		MQTT_CLIENT_DEBUG(mc, LOG_DEBUG, "index=%d, "
		    "mc->msg_id=%d, msg.id=%d", index, mc->msg_id, msg.id);
		if ((mc->msg_id == msg.id && int_qos != MQTT_QOS_0)) {
			mc->msg_is_repeat = 1;
		}

		if (index == MAX_SUB_TOPICS) {
			MQTT_CLIENT_LOG(mc, LOG_ERR, "msg ID mismatch");
		} else if (mc->msg_hdls[index].sub_handle) {
			current_len = mc->msg_hdls[index].sub_handle(mc,
			    mc->arg,
			    msg.payload, current_len,
			    msg.payloadlen);
		} else {
			/* drop message */
		}

		consume_len = current_len + mqtt_header_len;
		mc->msg_id = msg.id;
		mc->msg_qos = (enum mqtt_client_qos)int_qos;
		if (msg.payloadlen > current_len) {
			mc->msg_recv_outstanding = 1;
			mc->msg_index = index;
			mc->msg_payload_total_len = msg.payloadlen;
			mc->msg_payload_recv_len = current_len;
			client_timer_set(&mc->mc_msg_timer,
			    MQTT_CLIENT_MSG_WAIT);
		} else {
			mqtt_client_msg_qos_handle(mc);
		}
	} else {
		consume_len = len;
	}
	al_os_mem_free(topic_name);
	return consume_len;
}

static size_t mqtt_client_pubrel_handle(struct mqtt_client *mc,
	u8 *buf, size_t len)
{
	int rc;
	u16 pub_id = 0;
	u8 packet_type;
	u8 dup;

	rc = mqtt_packet_deserialize_ack(&packet_type, &dup, &pub_id, buf, len);

	if (rc == 1 && pub_id == mc->msg_id) {
		mqtt_client_publish_ack_send(mc, MQTT_PTYPE_PUBCOMP,
		    mc->msg_id);
		client_timer_cancel(&mc->mc_msg_timer);
		mqtt_client_reset_msg_info(mc);
	}
	return MQTT_CLIENT_PUBREL_LEN;
}


static void mqtt_client_ping_req_send(struct mqtt_client *mc)
{
	int len;

	mqtt_client_log(mc, LOG_DEBUG, "%s", __func__);

	memset(mc->mqtt_header, 0, sizeof(mc->mqtt_header));
	len = mqtt_packet_serialize_pingreq(mc->mqtt_header,
	    sizeof(mc->mqtt_header));

	client_timer_set(&mc->mc_keepalive_timer, MQTT_CLIENT_PING_RESP_WAIT);
	mqtt_client_send_buf(mc, mc->mqtt_header, len);
	mqtt_client_send_complete(mc);
	mc->ping_outstanding = 1;
}

static size_t mqtt_client_ping_resp_handle(struct mqtt_client *mc)
{
	mc->ping_outstanding = 0;
	client_timer_set(&mc->mc_keepalive_timer,
	    mc->condata.keep_alive * 1000);
	return MQTT_CLIENT_PINGRESP;
}

static void mqtt_client_disconnected(struct mqtt_client *mc)
{
	mqtt_client_abort(mc);
	mqtt_client_log(mc, LOG_DEBUG2, "%s: disconnected_cb %p",
	    __func__, mc->disconnected_cb);
	if (mc->disconnected_cb) {
		mc->disconnected_cb(mc, mc->arg);
	}
	mqtt_client_close(mc);
}

static void mqtt_client_disconnect_packet_send(struct mqtt_client *mc)
{
	int len;
	enum al_err err;

	memset(mc->mqtt_header, 0, sizeof(mc->mqtt_header));
	len = mqtt_packet_serialize_disconnect(mc->mqtt_header,
	    sizeof(mc->mqtt_header));

	mc->disconnect_len = len;
	err = mqtt_client_send_buf(mc, mc->mqtt_header, len);
	if (err) {
		mc->disconnect_len = 0;
		mqtt_client_disconnected(mc);
		return;
	}
	mqtt_client_send_complete(mc);
}

static u8 mqtt_client_sub_id_match(struct mqtt_client *mc, u16 id)
{
	u8 index;

	for (index = 0; index < MAX_SUB_TOPICS; index++) {
		if (mc->msg_hdls[index].sub_id == id) {
			break;
		}
	}

	return index;
}

static u8 mqtt_client_unsub_id_match(struct mqtt_client *mc, u16 id)
{
	u8 index;

	for (index = 0; index < MAX_SUB_TOPICS; index++) {
		if (mc->msg_hdls[index].unsub_id == id) {
			break;
		}
	}

	return index;
}

static void mqtt_client_subscribe_packet_send(struct mqtt_client *mc,
	struct mqtt_client_msg_handles *message_handle)
{
	int len;
	int qos = message_handle->sub_qos;

	memset(mc->mqtt_header, 0, sizeof(mc->mqtt_header));
	len = mqtt_packet_serialize_subscribe(mc->mqtt_header,
	    sizeof(mc->mqtt_header), 0, message_handle->sub_id, 1,
	    &message_handle->topic_filter, &qos);

	client_timer_set(&message_handle->sub_timer, MQTT_CLIENT_SUB_WAIT);
	mqtt_client_send_buf(mc, mc->mqtt_header, len);
	mqtt_client_send_complete(mc);
}

static size_t mqtt_client_suback_handle(struct mqtt_client *mc,
	u8 *buf, size_t len)
{
	int rc;
	u16 suback_id = 0;
	int count;
	int granted_qos;
	u8 index;
	enum al_err err;

	rc = mqtt_packet_deserialize_suback(&suback_id, 1, &count,
	    &granted_qos, buf, len);
	index = mqtt_client_sub_id_match(mc, suback_id);
	err = (granted_qos == MQTT_SUB_FAILED_CODE) ? AL_ERR_ERR : AL_ERR_OK;

	if (rc == 1 && index < MAX_SUB_TOPICS) {
		mc->msg_hdls[index].sub_outstanding = 0;
		mc->msg_hdls[index].is_valid = 1;
		client_timer_cancel(&mc->msg_hdls[index].sub_timer);
		if (mc->msg_hdls[index].suback_cb) {
			mc->msg_hdls[index].suback_cb(mc, mc->arg, err);
		}
	}
	return MQTT_CLIENT_SUBACK_LEN;
}

static void mqtt_client_unsubscribe_packet_send(struct mqtt_client *mc,
	struct mqtt_client_msg_handles *message_handle)
{
	int len;

	memset(mc->mqtt_header, 0, sizeof(mc->mqtt_header));
	len = mqtt_packet_serialize_unsubscribe(mc->mqtt_header,
	    sizeof(mc->mqtt_header), 0, message_handle->unsub_id, 1,
	    &message_handle->topic_filter);

	client_timer_set(&message_handle->unsub_timer,
	    MQTT_CLIENT_UNSUB_WAIT);
	mqtt_client_send_buf(mc, mc->mqtt_header, len);
	mqtt_client_send_complete(mc);
}

static size_t mqtt_client_unsuback_handle(struct mqtt_client *mc,
	u8 *buf, size_t len)
{
	int rc;
	u16 unsuback_id = 0;
	u8 index;

	rc = mqtt_packet_deserialize_unsuback(&unsuback_id, buf, len);
	index = mqtt_client_unsub_id_match(mc, unsuback_id);

	if (rc == 1 && index < MAX_SUB_TOPICS) {
		mc->msg_hdls[index].is_valid = 0;
		mc->msg_hdls[index].unsub_outstanding = 0;
		if (mc->msg_hdls[index].unsuback_cb) {
			mc->msg_hdls[index].unsuback_cb(mc, mc->arg,
			    AL_ERR_OK);
		}
		mqtt_client_reset_message_handle(&mc->msg_hdls[index]);
	}
	return MQTT_CLIENT_UNSUBACK_LEN;
}

static void mqtt_client_publish_header_packet_send(struct mqtt_client *mc,
	const char *topic, int qos, int total_len, u16 pub_id)
{
	int len = 0;

	memset(mc->mqtt_header, 0, sizeof(mc->mqtt_header));
	len = mqtt_packet_serialize_publish_header(mc->mqtt_header,
	    sizeof(mc->mqtt_header), 0, qos, 0, pub_id,
	    topic, total_len);

	mqtt_client_send_buf(mc, mc->mqtt_header, len);
	mqtt_client_send_complete(mc);
}

static size_t mqtt_client_puback_handle(struct mqtt_client *mc,
	u8 *buf, size_t len)
{
	int rc;
	u16 pub_id = 0;
	u8 packet_type;
	u8 dup;
	struct mqtt_client_publish_info *pub_info = NULL;

	rc = mqtt_packet_deserialize_ack(&packet_type, &dup, &pub_id, buf, len);
	if (rc == 1) {
		pub_info = mqtt_client_get_pub_info_by_id(mc->pub_list, pub_id);
	}

	if (pub_info) {
		mqtt_client_publish_complete(mc, pub_info, AL_ERR_OK);
	}
	return MQTT_CLIENT_PUBACK_LEN;
}

static size_t mqtt_client_pubrec_handle(struct mqtt_client *mc,
	u8 *buf, size_t len)
{
	int rc;
	u16 pub_id = 0;
	u8 packet_type;
	u8 dup;
	struct mqtt_client_publish_info *pub_info = NULL;

	rc = mqtt_packet_deserialize_ack(&packet_type, &dup, &pub_id, buf, len);

	if (rc == 1) {
		pub_info = mqtt_client_get_pub_info_by_id(mc->pub_list, pub_id);
	}

	if (pub_info) {
		mqtt_client_publish_ack_send(mc, MQTT_PTYPE_PUBREL, pub_id);
		client_timer_set(&mc->mc_msg_timer, MQTT_CLIENT_PUB_RECV_WAIT);
	}
	return MQTT_CLIENT_PUBREC_LEN;
}

static size_t mqtt_client_pubcomp_handle(struct mqtt_client *mc,
	u8 *buf, size_t len)
{
	int rc;
	u16 pub_id = 0;
	u8 packet_type;
	u8 dup;
	struct mqtt_client_publish_info *pub_info = NULL;

	rc = mqtt_packet_deserialize_ack(&packet_type, &dup, &pub_id, buf, len);

	if (rc == 1) {
		pub_info = mqtt_client_get_pub_info_by_id(mc->pub_list, pub_id);
	}

	if (pub_info) {
		client_timer_cancel(&mc->mc_msg_timer);
		mqtt_client_publish_complete(mc, pub_info, AL_ERR_OK);
	}
	return MQTT_CLIENT_PUBCOMP_LEN;
}

static void mqtt_client_disconnected_sent_cb(void *arg,
	struct al_net_stream *pcb, size_t len)
{
	struct mqtt_client *mc = arg;

	client_lock();
	if (len > mc->disconnect_len) {
		len = mc->disconnect_len;
	}
	mc->disconnect_len -= len;
	if (!mc->disconnect_len) {
		mqtt_client_disconnected(mc);
	}
	client_unlock();
}

static void mqtt_client_dns_timeout(struct timer *arg)
{
	struct mqtt_client *mc =
	    CONTAINER_OF(struct mqtt_client, mc_dns_timer, arg);

	if (++mc->retries <= MQTT_CLIENT_RETRY_MAX) {
		MQTT_CLIENT_DEBUG(mc, LOG_DEBUG, "retry %u", mc->retries);
		mqtt_client_getdnshostip(mc);
	} else {
		mqtt_client_abort_connack(mc, MQTT_CONN_ERR_TIMEOUT);
	}
}

static void mqtt_client_net_timeout(struct timer *arg)
{
	struct mqtt_client *mc =
	    CONTAINER_OF(struct mqtt_client, mc_timer, arg);

	MQTT_CLIENT_DEBUG(mc, LOG_DEBUG, "timeout");
	mqtt_client_abort_connack(mc, MQTT_CONN_ERR_TIMEOUT);
}


static void mqtt_client_conn_timeout(struct timer *arg)
{
	struct mqtt_client *mc =
	    CONTAINER_OF(struct mqtt_client, mc_timer, arg);

	if (mc->retries <= MQTT_CLIENT_RETRY_MAX) {
		MQTT_CLIENT_DEBUG(mc, LOG_DEBUG, "retry %u", mc->retries);
		mqtt_client_connect_packet_send(mc);
	} else {
		mqtt_client_abort_connack(mc, MQTT_CONN_ERR_TIMEOUT);
	}
	MQTT_CLIENT_DEBUG(mc, LOG_DEBUG, "timeout");
}

static void mqtt_client_keepalive_timeout(struct timer *arg)
{
	struct mqtt_client *mc =
	    CONTAINER_OF(struct mqtt_client, mc_keepalive_timer, arg);
	struct mqtt_client_publish_info *pub_info;

	if (mc->condata.keep_alive == 0) {
		return;
	}

	/* publish is outstanding, ping next keepalive time */
	pub_info = mqtt_client_get_outstanding_pub_info(mc->pub_list);
	if (pub_info) {
		client_timer_set(&mc->mc_keepalive_timer,
		    mc->condata.keep_alive * 1000);
		return;
	}

	if (mc->ping_outstanding) {
		MQTT_CLIENT_LOG(mc, LOG_ERR, "ping response timeout");
		mqtt_client_abort_err(mc, AL_ERR_CLSD);
	} else {
		mqtt_client_ping_req_send(mc);
	}
}

static void mqtt_client_subscribe_timeout(struct timer *arg)
{
	struct mqtt_client_msg_handles *message_handle =
	    CONTAINER_OF(struct mqtt_client_msg_handles, sub_timer, arg);
	struct mqtt_client *mc = message_handle->mc;

	MQTT_CLIENT_DEBUG(mc, LOG_DEBUG, "timeout");
	if (message_handle->suback_cb) {
		message_handle->suback_cb(mc, mc->arg, AL_ERR_TIMEOUT);
	}
	mqtt_client_reset_message_handle(message_handle);
}

static void mqtt_client_unsubscribe_timeout(struct timer *arg)
{
	struct mqtt_client_msg_handles *message_handle =
	    CONTAINER_OF(struct mqtt_client_msg_handles, unsub_timer, arg);
	struct mqtt_client *mc = message_handle->mc;

	message_handle->unsub_id = 0;

	MQTT_CLIENT_DEBUG(mc, LOG_DEBUG, "timeout");
	if (message_handle->unsuback_cb) {
		message_handle->unsuback_cb(mc, mc->arg, AL_ERR_TIMEOUT);
	}
}

static void mqtt_client_msg_timeout(struct timer *arg)
{
	struct mqtt_client *mc =
	    CONTAINER_OF(struct mqtt_client, mc_msg_timer, arg);

	MQTT_CLIENT_DEBUG(mc, LOG_DEBUG, "timeout");
	mqtt_client_err(mc, AL_ERR_TIMEOUT);
}

static void mqtt_client_pub_retry(struct mqtt_client *mc,
	struct mqtt_client_publish_info *pub_info)
{
	struct mqtt_client_data_info *data_info;

	mqtt_client_publish_header_packet_send(mc, pub_info->pub_topic,
	    pub_info->pub_qos, pub_info->total_len, pub_info->pub_id);

	for (data_info = pub_info->queue_head; data_info;
	    data_info = data_info->next) {
		mqtt_client_send_buf(mc, data_info->data,
		    data_info->len);
	}
	mqtt_client_send_complete(mc);
	client_timer_set(&pub_info->pub_timer, MQTT_CLIENT_PUB_RECV_WAIT);
}

static void mqtt_client_publish_timeout(struct timer *arg)
{
	struct mqtt_client_publish_info *pub_info =
	    CONTAINER_OF(struct mqtt_client_publish_info, pub_timer, arg);
	struct mqtt_client *mc = pub_info->mc;

	MQTT_CLIENT_DEBUG(mc, LOG_DEBUG, "timeout");

	if (pub_info->pub_outstanding == 1) {
		goto final;
	}

	if (pub_info->pub_retry < MQTT_CLIENT_RETRY_MAX) {
		mqtt_client_pub_retry(mc, pub_info);
		pub_info->pub_retry++;
		return;
	}

final:
	mqtt_client_publish_complete(mc, pub_info, AL_ERR_TIMEOUT);
}

static void mqtt_client_msg_qos_handle(struct mqtt_client *mc)
{
	switch (mc->msg_qos) {
	case MQTT_QOS_0:
		client_timer_cancel(&mc->mc_msg_timer);
		mqtt_client_reset_msg_info(mc);
		break;
	case MQTT_QOS_1:
		mqtt_client_publish_ack_send(mc, MQTT_PTYPE_PUBACK,
		    mc->msg_id);
		client_timer_cancel(&mc->mc_msg_timer);
		mqtt_client_reset_msg_info(mc);
		break;
	case MQTT_QOS_2:
		mqtt_client_publish_ack_send(mc, MQTT_PTYPE_PUBREC,
		    mc->msg_id);
		client_timer_set(&mc->mc_msg_timer,
		    MQTT_CLIENT_PUBREL_WAIT);
		break;
	default:
		break;
	}
}

static size_t mqtt_client_message_recv_continue(struct mqtt_client *mc,
	u8 *buf, size_t len)
{
	size_t consume_len;
	size_t recv_len;

	if (mc->msg_payload_recv_len + len > mc->msg_payload_total_len) {
		consume_len = mc->msg_payload_total_len -
		    mc->msg_payload_recv_len;
	} else {
		consume_len = len;
	}

	client_timer_set(&mc->mc_msg_timer, MQTT_CLIENT_MSG_WAIT);
	if (mc->msg_index == MAX_SUB_TOPICS || mc->msg_is_repeat == 1) {
		goto final;
	}

	if (mc->msg_hdls[mc->msg_index].sub_handle) {
		/* limit what we deliver to what the upper level can buffer */
		if (consume_len > MQTT_CLIENT_SUB_BUF_LEN) {
			consume_len = MQTT_CLIENT_SUB_BUF_LEN;
		}
		recv_len = mc->msg_hdls[mc->msg_index].sub_handle(mc, mc->arg,
		    buf, consume_len, mc->msg_payload_total_len);
		if (recv_len < consume_len) {
			consume_len = recv_len;
			mc->paused = 1;
		}
	}

final:
	mc->msg_payload_recv_len += consume_len;
	if (mc->msg_payload_total_len == mc->msg_payload_recv_len) {
		mqtt_client_msg_qos_handle(mc);
		mc->msg_recv_outstanding = 0;
	}
	return consume_len;
}

/*
 * Callback from AL stream to receive data.
 */
static enum al_err mqtt_client_tcp_recv(void *arg,
	struct al_net_stream *pcb, void *payload, size_t len)
{
	struct mqtt_client *mc = arg;
	u8 header_byte;
	enum mqtt_packet_msg_type packet_type;
	size_t consume_len = 0;
	size_t recv_len;
	u8 *mqtt_data = payload;

	client_lock();
packet_analysis:
	if (mc->paused) {
		goto out;
	}
	if (mc->msg_recv_outstanding) {
		recv_len = mqtt_client_message_recv_continue(mc,
		    mqtt_data + consume_len, len - consume_len);
		consume_len += recv_len;
		if (mc->paused) {
			goto out;
		}
		if (mc->msg_recv_outstanding) {
			if (consume_len < len) {
				goto packet_analysis;
			}
			goto out;
		}
	}

	ASSERT(consume_len <= len);

	if (consume_len == len) {
		goto out;
	}

	header_byte = mqtt_data[consume_len];
	packet_type = MQTT_HEADER_TYPE_GET(header_byte);

	if (packet_type != MQTT_PTYPE_PINGRESP) {
		mqtt_client_log(mc, LOG_DEBUG2, "%s: packet type %u %s",
		    __func__, packet_type, mqtt_client_msg_type(packet_type));
	}

	switch (packet_type) {
	case MQTT_PTYPE_CONNACK:
		consume_len += mqtt_client_connack_handle(mc,
		    &mqtt_data[consume_len], len - consume_len);
		break;

	case MQTT_PTYPE_PUBLISH:
		consume_len += mqtt_client_message_handle(mc,
		    &mqtt_data[consume_len], len - consume_len);
		break;

	case MQTT_PTYPE_PUBACK:
		consume_len += mqtt_client_puback_handle(mc,
		    &mqtt_data[consume_len], len - consume_len);
		break;

	case MQTT_PTYPE_PUBREC:
		consume_len += mqtt_client_pubrec_handle(mc,
		    &mqtt_data[consume_len], len - consume_len);
		break;

	case MQTT_PTYPE_PUBREL:
		consume_len += mqtt_client_pubrel_handle(mc,
		    &mqtt_data[consume_len], len - consume_len);
		break;

	case MQTT_PTYPE_PUBCOMP:
		consume_len += mqtt_client_pubcomp_handle(mc,
		    &mqtt_data[consume_len], len - consume_len);
		break;

	case MQTT_PTYPE_SUBACK:
		consume_len += mqtt_client_suback_handle(mc,
		    &mqtt_data[consume_len], len - consume_len);
		break;

	case MQTT_PTYPE_UNSUBACK:
		consume_len += mqtt_client_unsuback_handle(mc,
		    &mqtt_data[consume_len], len - consume_len);
		break;

	case MQTT_PTYPE_PINGRESP:
		consume_len += mqtt_client_ping_resp_handle(mc);
		break;

	default:
		mqtt_client_log(mc, LOG_ERR,
		    "%s: unexpected packet type %u - aborting connection",
		    __func__, packet_type);
		mqtt_client_err(mc, AL_ERR_ABRT);
		goto unlock;
	}

	if (consume_len < len) {
		goto packet_analysis;
	}
out:
	al_net_stream_recved(pcb, consume_len);
unlock:
	client_unlock();

	return AL_ERR_OK;
}

void mqtt_client_pause(struct mqtt_client *mc)
{
	mc->paused = 1;
}

void mqtt_client_continue(struct mqtt_client *mc)
{
	if (!mc->paused) {
		return;
	}
	mc->paused = 0;
	if (!mc->pcb) {
		return;
	}
	al_net_stream_continue_recv(mc->pcb);
}

/*
 * Report error and abort connection.
 */
static void mqtt_client_err(struct mqtt_client *mc, enum al_err err)
{
	if (mc->conn_state == MQTT_CONNECTING) {
		mqtt_client_abort_connack(mc, MQTT_CONN_ERR_STREAM);
	} else {
		mqtt_client_abort_err(mc, err);
	}
}

/*
 * Callback from al_stream when connection fails or gets reset.
 */
static void mqtt_client_err_cb(void *arg, enum al_err err)
{
	struct mqtt_client *mc = arg;

	client_lock();
	if (mc->conn_state == MQTT_DISCONNECTING && err == AL_ERR_CLSD) {
		mqtt_client_disconnected(mc);
	} else {
		mqtt_client_err(mc, err);
	}
	client_unlock();
}

/*
 * Callback from al_stream when more data may be sent.
 */
static void mqtt_client_sent_cb(void *arg, struct al_net_stream *pcb,
    size_t len)
{
	struct mqtt_client *mc = arg;

	if (!mc->sent_cb || mc->conn_state != MQTT_CONNECTED) {
		return;
	}
	client_lock();
	mc->sent_cb(mc->arg);
	client_unlock();
}

/*
 * Callback from mqtt_client_connect when TCP is connected
 */
static enum al_err mqtt_client_network_connected(void *arg,
    struct al_net_stream *pcb, enum al_err err)
{
	struct mqtt_client *mc = arg;

	client_lock();
	client_timer_cancel(&mc->mc_timer);

	if (err != AL_ERR_OK) {
		MQTT_CLIENT_LOG(mc, LOG_ERR, "err %d", err);
		mqtt_client_abort_connack(mc, MQTT_CONN_ERR_STREAM);
	} else {
		MQTT_CLIENT_DEBUG(mc, LOG_DEBUG, "pcb %p", pcb);
		mc->retries = 0;
		timer_init(&mc->mc_timer, mqtt_client_conn_timeout);
		mqtt_client_connect_packet_send(mc);
	}
	client_unlock();
	return err;
}

/*
 * Establish connection.
 */
static void mqtt_client_connect_int(struct mqtt_client *mc)
{
	struct al_net_stream *pcb;
	enum al_err err;
	u32 host_addr;

	host_addr = al_net_addr_get_ipv4(&mc->host_addr);
	if (!host_addr) {
		MQTT_CLIENT_LOG(mc, LOG_WARN, "null ip");
		mqtt_client_abort_connack(mc, MQTT_CONN_ERR_DNS_REQ);
		return;
	}

	mqtt_client_close(mc);

	pcb = al_net_stream_new(
	    mc->ssl_enable ? AL_NET_STREAM_TLS : AL_NET_STREAM_TCP);
	if (!pcb) {
		MQTT_CLIENT_LOG(mc, LOG_WARN, "cannot alloc PCB");
		mqtt_client_abort_connack(mc, MQTT_CONN_ERR_STREAM);
		return;
	}
	mc->pcb = pcb;
	mc->paused = 0;

	al_net_stream_set_arg(pcb, mc);
	al_net_stream_set_recv_cb(pcb, mqtt_client_tcp_recv);
	al_net_stream_set_err_cb(pcb, mqtt_client_err_cb);
	al_net_stream_set_sent_cb(mc->pcb, mqtt_client_sent_cb);

	err = al_net_stream_connect(pcb,
	    mc->host, &mc->host_addr, mc->host_port,
	    mqtt_client_network_connected);
	if (err != AL_ERR_OK) {
		MQTT_CLIENT_LOG(mc, LOG_WARN, "err %d", err);
		mqtt_client_abort_connack(mc, MQTT_CONN_ERR_STREAM);
		return;
	}

	client_timer_set(&mc->mc_timer, MQTT_CLIENT_NET_WAIT);
	return;
}

/*
 * DNS resolved callback.
 */
static void mqtt_client_dns_cb(struct al_net_dns_req *req)
{
	struct mqtt_client *mc =
	    CONTAINER_OF(struct mqtt_client, dns_req, req);
	const char *name = req->hostname;
	u32 addr;
	char buf[20];

	client_lock();

	/*
	 * Client may have changed hosts or state while DNS was outstanding.
	 */
	addr = al_net_addr_get_ipv4(&req->addr);
	if (req->error || !addr) {
		al_net_addr_set_ipv4(&mc->host_addr, 0);
		MQTT_CLIENT_LOG(mc, LOG_WARN, "host %s failed", name);
		mqtt_client_abort_connack(mc, MQTT_CONN_ERR_DNS_REQ);
		goto unlock;
	}
	if (al_net_addr_get_ipv4(&mc->host_addr) != addr) {
		mqtt_client_log(mc, LOG_INFO, "DNS: host %s at %s",
		    name,
		    ipaddr_fmt_ipv4_to_str(al_net_addr_get_ipv4(&req->addr),
		    buf, sizeof(buf)));
		mc->host_addr = req->addr;
	}
	mqtt_client_connect_int(mc);
unlock:
	client_unlock();
}

/*
 * Get the IP address of the DNS
 */
static void mqtt_client_getdnshostip(struct mqtt_client *mc)
{
	struct al_net_dns_req *req = &mc->dns_req;
	enum al_err err;

	req->hostname = mc->host;
	req->callback = mqtt_client_dns_cb;

	err = al_dns_req_ipv4_start(req);
	if (AL_ERR_OK != err) {
		client_timer_set(&mc->mc_dns_timer, MQTT_CLIENT_DNS_WAIT);
	}
}

static void mqtt_client_start(struct mqtt_client *mc)
{
	if (mc->host[0] != '\0') {
		mqtt_client_getdnshostip(mc);
	} else {
		mqtt_client_abort_connack(mc, MQTT_CONN_ERR_DNS_REQ);
	}
}

static void mqtt_client_info_reset(struct mqtt_client *mc)
{
	int i;

	/* all mqtt client timers cancel and free alloc*/
	mqtt_client_timers_cancel(mc);
	for (i = 0; i < MAX_SUB_TOPICS; i++) {
		mqtt_client_reset_message_handle(&mc->msg_hdls[i]);
	}
	mqtt_client_cleanup_pub_list(mc);

	/* reset mqtt client info */
	mc->retries = 0;
	mc->conn_state = MQTT_DISCONNECTED;
	mc->ping_outstanding = 0;
	mc->condata.cleansession = 1;
	mc->condata.keep_alive = MQTT_CLIENT_DEFAULT_KEEPALIVE;
	mc->next_packetid = 1;
	mc->mod_log_id = MOD_LOG_MQTT;
	memset(&mc->host_addr, 0, sizeof(struct al_net_addr));
	memset(&mc->dns_req, 0, sizeof(struct al_net_dns_req));
	mqtt_client_reset_msg_info(mc);
}

void mqtt_packet_set_host_addr(struct mqtt_client *mc,
	struct al_net_addr *addr)
{
	memcpy(&mc->host_addr, addr, sizeof(struct al_net_addr));
}

struct mqtt_client *mqtt_client_new(void)
{
	struct mqtt_client *mc;

	mc = al_os_mem_calloc(sizeof(*mc));
	if (!mc) {
		return NULL;
	}
	mc->condata.cleansession = 1;
	mc->condata.keep_alive = MQTT_CLIENT_DEFAULT_KEEPALIVE;
	mc->next_packetid = 1;
	mc->mod_log_id = MOD_LOG_MQTT;
	mqtt_client_reset_msg_info(mc);

	timer_init(&mc->mc_timer, mqtt_client_net_timeout);
	timer_init(&mc->mc_dns_timer, mqtt_client_dns_timeout);
	timer_init(&mc->mc_keepalive_timer, mqtt_client_keepalive_timeout);
	timer_init(&mc->mc_msg_timer, mqtt_client_msg_timeout);

	return mc;
}

int mqtt_client_free(struct mqtt_client *mc)
{
	if (!mc) {
		return 0;
	}

	if (mc->conn_state != MQTT_DISCONNECTED) {
		return -1;
	}
	mqtt_client_close(mc);
	mqtt_client_timers_cancel(mc);
	al_os_mem_free((void *)mc->condata.client_id);
	al_os_mem_free((void *)mc->condata.username);
	al_os_mem_free((void *)mc->condata.password);

	al_os_mem_free(mc);
	return 0;
}

/*
 * Allocate and set dynamic string pointer.
 */
static enum al_err mqtt_client_string_set(const char **dest, const char *sp)
{
	size_t len;
	char *dp;

	al_os_mem_free((void *)*dest);
	*dest = NULL;
	if (!sp) {
		return AL_ERR_OK;
	}
	len = strlen(sp);
	dp = al_os_mem_alloc(len + 1);
	if (!dp) {
		return AL_ERR_ALLOC;
	}
	memcpy(dp, sp, len + 1);
	*dest = dp;
	return AL_ERR_OK;
}

int mqtt_client_connect(struct mqtt_client *mc,
	const char *host, u16 port,
	int ssl_enable, const char *client_id,
	void (*connack_cb)(struct mqtt_client *mc,
	enum mqtt_client_conn_err err, void *arg),
	void (*err_cb)(struct mqtt_client *mc,
	enum al_err err, void *arg))
{
	int len;
	u32 host_addr;
	enum al_err err;

	ASSERT(client_locked);

	if (mc->conn_state != MQTT_DISCONNECTED) {
		MQTT_CLIENT_LOG(mc, LOG_WARN, "mqtt is not disconnected");
		return -1;
	}

	len = snprintf(mc->host, sizeof(mc->host), "%s", host);
	if (len >= sizeof(mc->host)) {
		MQTT_CLIENT_LOG(mc, LOG_ERR, "host name truncated");
		return -1;
	}
	mc->host_port = port;
	mc->ssl_enable = ssl_enable;
	mc->mqtt_id = gv_mqtt_id_count++;

	err = mqtt_client_string_set(&mc->condata.client_id, client_id);
	if (err) {
		return -1;
	}

	mc->connack_cb = connack_cb;
	mc->err_cb = err_cb;
	mc->retries = 0;
	mc->conn_state = MQTT_CONNECTING;

	host_addr = al_net_addr_get_ipv4(&mc->host_addr);
	if (host_addr) {
		mqtt_client_connect_int(mc);
	} else {
		mqtt_client_start(mc);
	}
	return 0;
}


void mqtt_client_set_arg(struct mqtt_client *mc, void *arg)
{
	mc->arg = arg;
}

enum al_err mqtt_client_set_conn_account(struct mqtt_client *mc,
	const char *username, const char *password)
{
	enum al_err err;

	err = mqtt_client_string_set(&mc->condata.username, username);
	if (err) {
		return err;
	}
	err = mqtt_client_string_set(&mc->condata.password, password);
	return err;
}

int mqtt_client_set_conn_keepalive(struct mqtt_client *mc,
	u16 keepalive)
{
	mc->condata.keep_alive = keepalive;
	return 0;
}

int mqtt_client_disconnect(struct mqtt_client *mc,
	void (*disconnected_cb)(struct mqtt_client *mc, void *arg))
{
	ASSERT(client_locked);

	mc->disconnected_cb = disconnected_cb;

	switch (mc->conn_state) {
	case MQTT_DISCONNECTED:
		MQTT_CLIENT_LOG(mc, LOG_WARN, "already disconnected");
		return -1;
	case MQTT_DISCONNECTING:
		MQTT_CLIENT_LOG(mc, LOG_WARN, "already disconnecting");
		return -1;
	case MQTT_CONNECTING:
		MQTT_CLIENT_LOG(mc, LOG_DEBUG, "disconnect while connecting");
		mqtt_client_disconnected(mc);
		break;
	case MQTT_CONNECTED:
		MQTT_CLIENT_LOG(mc, LOG_DEBUG, "disconnecting");
		mc->conn_state = MQTT_DISCONNECTING;
		al_net_stream_set_sent_cb(mc->pcb,
		    mqtt_client_disconnected_sent_cb);
		mqtt_client_disconnect_packet_send(mc);
		break;
	}

	return 0;
}

int mqtt_client_subscribe_topic(struct mqtt_client *mc,
	const char *topic,
	enum mqtt_client_qos qos,
	size_t (*sub_handle)(struct mqtt_client *mc,
		void *arg, const void *payload, size_t len, size_t total_len),
	void (*suback_cb)(struct mqtt_client *mc,
		void *arg, enum al_err err))
{
	u8 index;

	ASSERT(client_locked);

	if (mc->conn_state != MQTT_CONNECTED) {
		MQTT_CLIENT_LOG(mc, LOG_ERR, "not connected");
		return -1;
	}

	for (index = 0; index < MAX_SUB_TOPICS; index++) {
		if (mc->msg_hdls[index].topic_filter &&
		    !strcmp(mc->msg_hdls[index].topic_filter, topic)) {
			if (mc->msg_hdls[index].sub_outstanding == 1) {
				MQTT_CLIENT_LOG(mc, LOG_ERR,
				    "same topic is outstanding");
				return -1;
			} else {
				mqtt_client_reset_message_handle(
				    &mc->msg_hdls[index]);
			}
		}
	}

	for (index = 0; index < MAX_SUB_TOPICS; index++) {
		if (!mc->msg_hdls[index].topic_filter) {
			break;
		}
	}

	if (index == MAX_SUB_TOPICS) {
		MQTT_CLIENT_LOG(mc, LOG_ERR, "topic lists is full");
		return -1;
	}

	mc->msg_hdls[index].mc = mc;
	if (mqtt_client_string_set(&mc->msg_hdls[index].topic_filter, topic)) {
		MQTT_CLIENT_LOG(mc, LOG_ERR, "topic alloc failed");
		return -1;
	}
	mc->msg_hdls[index].sub_outstanding = 1;
	mc->msg_hdls[index].sub_qos = qos;
	mc->msg_hdls[index].sub_handle = sub_handle;
	mc->msg_hdls[index].suback_cb = suback_cb;
	mc->msg_hdls[index].sub_id = mqtt_client_get_next_packetid(mc);
	timer_init(&mc->msg_hdls[index].sub_timer,
	    mqtt_client_subscribe_timeout);

	mqtt_client_subscribe_packet_send(mc, &mc->msg_hdls[index]);
	return 0;
}

int mqtt_client_unsubscribe_topic(struct mqtt_client *mc,
	const char *topic,
	void (*unsuback_cb)(struct mqtt_client *mc, void *arg,
	enum al_err err))
{
	u8 index;

	if (mc->conn_state != MQTT_CONNECTED) {
		MQTT_CLIENT_LOG(mc, LOG_ERR, "mqtt is not connected");
		return -1;
	}

	for (index = 0; index < MAX_SUB_TOPICS; index++) {
		if (mc->msg_hdls[index].topic_filter &&
		    !strcmp(mc->msg_hdls[index].topic_filter, topic)) {
			if (mc->msg_hdls[index].sub_outstanding == 1 ||
			    mc->msg_hdls[index].unsub_outstanding == 1 ||
			    mc->msg_hdls[index].is_valid == 0) {
				return -1;
			} else {
				break;
			}
		}
	}

	if (index == MAX_SUB_TOPICS) {
		MQTT_CLIENT_LOG(mc, LOG_ERR, "topic not matched");
		return -1;
	}

	mc->msg_hdls[index].unsub_outstanding = 1;
	mc->msg_hdls[index].unsuback_cb = unsuback_cb;
	mc->msg_hdls[index].unsub_id = mqtt_client_get_next_packetid(mc);
	timer_init(&mc->msg_hdls[index].unsub_timer,
	    mqtt_client_unsubscribe_timeout);
	mqtt_client_unsubscribe_packet_send(mc, &mc->msg_hdls[index]);
	return 0;
}

enum al_err mqtt_client_publish_topic_header(struct mqtt_client *mc,
	const char *topic, enum mqtt_client_qos qos, u32 total_len,
	void (*pub_completed)(struct mqtt_client *mc,
	void *arg, enum al_err err))
{
	struct mqtt_client_publish_info *pub_info;
	struct mqtt_client_publish_info *outstanding_pub_info;
	enum al_err err;

	ASSERT(client_locked);

	if (mc->conn_state != MQTT_CONNECTED) {
		MQTT_CLIENT_LOG(mc, LOG_ERR, "mqtt is not connected");
		return AL_ERR_NOTCONN;
	}

	outstanding_pub_info =
	    mqtt_client_get_outstanding_pub_info(mc->pub_list);
	if (outstanding_pub_info) {
		MQTT_CLIENT_LOG(mc, LOG_WARN,
		    "last publish is in progress");
		return AL_ERR_IN_PROGRESS;
	}

	pub_info = mqtt_client_pub_info_new(mc);
	if (!pub_info) {
		mqtt_client_log(mc, LOG_ERR, "pub_info alloc failed");
		return AL_ERR_ALLOC;
	}
	pub_info->pub_outstanding = (total_len > 0) ? 1 : 0;
	pub_info->pub_id = mqtt_client_get_next_packetid(mc);
	pub_info->total_len = total_len;
	pub_info->pub_qos = qos;
	pub_info->pub_completed = pub_completed;
	pub_info->mc = mc;

	err = mqtt_client_string_set(&pub_info->pub_topic, topic);
	if (err) {
		mqtt_client_log(mc, LOG_ERR, "pub_info topic alloc failed");
		mqtt_client_pub_info_free(mc, pub_info);
		return err;
	}
	pub_info->data_lock = al_os_lock_create();
	if (!pub_info->data_lock) {
		mqtt_client_log(mc, LOG_ERR, "pub_info lock alloc failed");
		mqtt_client_pub_info_free(mc, pub_info);
		return err;
	}
	timer_init(&pub_info->pub_timer, mqtt_client_publish_timeout);

	mqtt_client_publish_header_packet_send(mc, pub_info->pub_topic,
	    pub_info->pub_qos, pub_info->total_len, pub_info->pub_id);

	if (pub_info->pub_outstanding == 0) {
		if (pub_info->pub_qos == MQTT_QOS_0) {
			mqtt_client_publish_complete(mc, pub_info, AL_ERR_OK);
		} else {
			client_timer_set(&pub_info->pub_timer,
			    MQTT_CLIENT_PUB_RECV_WAIT);
		}
	} else {
		client_timer_set(&pub_info->pub_timer,
		    MQTT_CLIENT_PUB_SEND_WAIT);
	}
	return AL_ERR_OK;
}

enum al_err mqtt_client_publish_payload(struct mqtt_client *mc,
	const void *payload, u32 len)
{
	struct mqtt_client_publish_info *pub_info;
	enum al_err err;

	ASSERT(client_locked);

	if (mc->conn_state != MQTT_CONNECTED) {
		MQTT_CLIENT_LOG(mc, LOG_ERR, "mqtt is not connected");
		return AL_ERR_NOTCONN;
	}

	pub_info = mqtt_client_get_outstanding_pub_info(mc->pub_list);
	if (pub_info) {
		if (mqtt_client_add_data_to_pub(pub_info, payload, len)) {
			MQTT_CLIENT_LOG(mc, LOG_ERR, "pub buf alloc failed");
			return AL_ERR_ALLOC;
		}
	} else {
		MQTT_CLIENT_LOG(mc, LOG_WARN, "no publish is in progress");
		return AL_ERR_ERR;
	}

	err = mqtt_client_send_buf(mc, payload, len);
	if (err) {
		return err;
	}
	mqtt_client_send_complete(mc);
	pub_info->sent_len += len;
	if (pub_info->sent_len >= pub_info->total_len) {
		if (pub_info->sent_len > pub_info->total_len) {
			MQTT_CLIENT_LOG(mc, LOG_ERR,
			    "sent_len %lu gt total_len %lu",
			    pub_info->sent_len, pub_info->total_len);
		}
		pub_info->pub_outstanding = 0;
		if (pub_info->pub_qos == MQTT_QOS_0) {
			mqtt_client_publish_complete(mc, pub_info, AL_ERR_OK);
		} else {
			client_timer_set(&pub_info->pub_timer,
			    MQTT_CLIENT_PUB_RECV_WAIT);
		}
	} else {
		client_timer_set(&pub_info->pub_timer,
		    MQTT_CLIENT_PUB_SEND_WAIT);
	}
	return AL_ERR_OK;
}

void mqtt_client_log_set(struct mqtt_client *mc, enum mod_log_id mod_nr)
{
	mc->mod_log_id = mod_nr;
}

void mqtt_client_set_sent_cb(struct mqtt_client *mc, void (*sent_cb)(void *arg))
{
	mc->sent_cb = sent_cb;
}

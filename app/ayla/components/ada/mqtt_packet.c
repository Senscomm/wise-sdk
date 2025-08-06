/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <string.h>
#include <ayla/assert.h>
#include <al/al_os_mem.h>
#include "MQTTPacket.h"
#include "mqtt_packet.h"

/*
 * Fill in MQTTString from normal C string.
 * MQTTString can have either the NUL-terminated cstring set without a length,
 * or an unterminated data area with a length.
 * Using the latter to limit strlen() calls.
 */
static void mqtt_packet_string_set(MQTTString *ps, const char *val)
{
	ps->cstring = NULL;
	ps->lenstring.data = (char *)val;
	ps->lenstring.len = val ? strlen(val) : 0;
}

/*
 * Copy MQTTString to a dynamically-allocated C string.
 * MQTTString can have either the NUL-terminated cstring set without a length,
 * or an unterminated data area with a length.
 */
static char *mqtt_packet_string_get(MQTTString *sp)
{
	int len;
	char *dp;
	char *data;

	if (sp->cstring) {
		data = sp->cstring;
		len = strlen(data);
	} else {
		data = sp->lenstring.data;
		len = sp->lenstring.len;
	}
	if (!data) {
		return NULL;
	}
	dp = al_os_mem_alloc(len + 1);
	if (!dp) {
		return NULL;
	}
	memcpy(dp, data, len);
	dp[len] = '\0';
	return dp;
}

/*
 * Allocate and set an array of MQTTString.
 */
static MQTTString *mqtt_packet_strings_alloc(unsigned int count,
	const char **strings)
{
	MQTTString *sp;
	unsigned int i;

	sp = al_os_mem_calloc(sizeof(*sp) * count);
	if (!sp) {
		return NULL;
	}
	for (i = 0; i < count; i++) {
		mqtt_packet_string_set(&sp[i], strings[i]);
	}
	return sp;
}

int mqtt_packet_serialize_ack(unsigned char *buf, int buflen,
	unsigned char type,
	unsigned char dup, unsigned short packetid)
{
	return MQTTSerialize_ack(buf, buflen, type, dup, packetid);
}

int mqtt_packet_deserialize_ack(unsigned char *packettype, unsigned char *dup,
	unsigned short *packetid, unsigned char *buf, int buflen)
{
	return MQTTDeserialize_ack(packettype, dup, packetid, buf, buflen);
}

/*
 * Note returns 1 on success.
 */
int mqtt_packet_serialize_connect(unsigned char *buf, int buflen,
	struct mqtt_packet_connect_data *options)
{
	MQTTPacket_connectData conn = MQTTPacket_connectData_initializer;

	mqtt_packet_string_set(&conn.username, options->username);
	mqtt_packet_string_set(&conn.password, options->password);
	mqtt_packet_string_set(&conn.clientID, options->client_id);
	conn.keepAliveInterval = options->keep_alive;
	conn.willFlag = options->will_flag;
	mqtt_packet_string_set(&conn.will.topicName, options->will.topic);
	mqtt_packet_string_set(&conn.will.message, options->will.message);
	conn.will.retained = options->will.retained;

	return MQTTSerialize_connect(buf, buflen, &conn);
}

int mqtt_packet_deserialize_connack(unsigned char *sessionPresent,
	unsigned char *connack_rc, unsigned char *buf, int buflen)
{
	return MQTTDeserialize_connack(sessionPresent, connack_rc,
	    buf, buflen);
}

int mqtt_packet_serialize_disconnect(unsigned char *buf, int buflen)
{
	return MQTTSerialize_disconnect(buf, buflen);
}

int mqtt_packet_serialize_pingreq(unsigned char *buf, int buflen)
{
	return MQTTSerialize_pingreq(buf, buflen);
}

int mqtt_packet_serialize_publish(unsigned char *buf, int buflen,
	unsigned char dup, int qos, unsigned char retained,
	unsigned short packetid, const char *topic_name,
	unsigned char *payload, int payloadlen)
{
	MQTTString topic;

	mqtt_packet_string_set(&topic, topic_name);
	return MQTTSerialize_publish(buf, buflen, dup, qos, retained,
	    packetid, topic,
	    payload, payloadlen);
}

static int mqtt_packet_serialize_publish_length(int qos,
	const char *topic_name, int payload_len)
{
	int len;

	len = 2 + strlen(topic_name) + payload_len;
	if (qos > 0) {
		len += 2; /* packetid */
	}
	return len;
}

int mqtt_packet_serialize_publish_header(unsigned char *buf, int buflen,
	unsigned char dup, int qos, unsigned char retained,
	unsigned short packetid, const char *topic_name,
	int payload_len)
{
	unsigned char *ptr = buf;
	MQTTHeader header = {0};
	int rem_len = 0;
	int rc = 0;

	rem_len = mqtt_packet_serialize_publish_length(qos,
	    topic_name, payload_len);

	header.bits.type = PUBLISH;
	header.bits.dup = dup;
	header.bits.qos = qos;
	header.bits.retain = retained;
	writeChar(&ptr, header.byte); /* write header */

	ptr += MQTTPacket_encode(ptr, rem_len); /* write remaining length */

	writeCString(&ptr, topic_name);

	if (qos > 0) {
		writeInt(&ptr, packetid);
	}

	rc = ptr - buf;

	return rc;
}

int mqtt_packet_deserialize_publish(unsigned char *dup, int *qos,
	unsigned char *retained, unsigned short *packetid,
	char **topic_name, unsigned char **payload,
	int *payloadlen, unsigned char *buf, int buflen)
{
	MQTTString topic;
	int err;

	err = MQTTDeserialize_publish(dup, qos, retained, packetid,
	    &topic, payload, payloadlen,
	    buf, buflen);
	if (err != 1) {		/* 1 is success */
		return err;
	}
	*topic_name = mqtt_packet_string_get(&topic);
	if (!*topic_name) {
		return -1;
	}
	return err;
}

int mqtt_packet_serialize_subscribe(unsigned char *buf,
	int buflen, unsigned char dup, unsigned short packetid,
	int count, const char **topic_filters,
	int requestedQoSs[])
{
	MQTTString *topics;
	int rc;

	topics = mqtt_packet_strings_alloc(count, topic_filters);
	if (!topics) {
		return -1;
	}
	rc = MQTTSerialize_subscribe(buf, buflen, dup, packetid,
	    count, topics, requestedQoSs);
	al_os_mem_free(topics);
	return rc;
}

int mqtt_packet_deserialize_suback(unsigned short *packetid,
	int maxcount, int *count, int grantedQoSs[],
	unsigned char *buf, int len)
{
	return MQTTDeserialize_suback(packetid, maxcount, count,
	    grantedQoSs, buf, len);
}

int mqtt_packet_serialize_unsubscribe(unsigned char *buf, int buflen,
	unsigned char dup, unsigned short packetid,
	int count, const char **topic_filters)
{
	MQTTString *topics;
	int rc;

	topics = mqtt_packet_strings_alloc(count, topic_filters);
	if (!topics) {
		return -1;
	}
	rc = MQTTSerialize_unsubscribe(buf, buflen, dup, packetid,
	    count, topics);
	al_os_mem_free(topics);
	return rc;
}

int mqtt_packet_deserialize_unsuback(unsigned short *packetid,
	unsigned char *buf, int len)
{
	return MQTTDeserialize_unsuback(packetid, buf, len);
}

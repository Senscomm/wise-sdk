/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_MQTT_PACKET_H__
#define __AYLA_ADA_MQTT_PACKET_H__

#include <al/al_utypes.h>

/**
 * @file
 * Platform MQTT packet Interfaces
 */

/**
 * Type of mqtt message
 */
enum mqtt_packet_msg_type {
	MQTT_PTYPE_CONNECT = 1,		/**< connect */
	MQTT_PTYPE_CONNACK = 2,		/**< connack */
	MQTT_PTYPE_PUBLISH = 3,		/**< publish */
	MQTT_PTYPE_PUBACK = 4,		/**< puback */
	MQTT_PTYPE_PUBREC = 5,		/**< pubrec */
	MQTT_PTYPE_PUBREL = 6,		/**< pubrel */
	MQTT_PTYPE_PUBCOMP = 7,		/**< pubcomp */
	MQTT_PTYPE_SUBSCRIBE = 8,	/**< subscribe */
	MQTT_PTYPE_SUBACK = 9,		/**< suback */
	MQTT_PTYPE_UNSUBSCRIBE = 10,	/**< unsubscribe */
	MQTT_PTYPE_UNSUBACK = 11,	/**< unsuback */
	MQTT_PTYPE_PINGREQ = 12,	/**< pingreq */
	MQTT_PTYPE_PINGRESP = 13,	/**< pingresp */
	MQTT_PTYPE_DISCONNECT = 14,	/**< disconnect */
};

#define MQTT_PACKET_MSG_TYPES { \
	[MQTT_PTYPE_CONNECT] =		"connect", \
	[MQTT_PTYPE_CONNACK] =		"connack", \
	[MQTT_PTYPE_PUBLISH] =		"publish", \
	[MQTT_PTYPE_PUBACK] =		"puback", \
	[MQTT_PTYPE_PUBREC] =		"pubrec", \
	[MQTT_PTYPE_PUBREL] =		"pubrel", \
	[MQTT_PTYPE_PUBCOMP] =		"pubcomp", \
	[MQTT_PTYPE_SUBSCRIBE] =	"subscribe", \
	[MQTT_PTYPE_SUBACK] =		"suback", \
	[MQTT_PTYPE_UNSUBSCRIBE] =	"unsubscribe", \
	[MQTT_PTYPE_UNSUBACK] =		"unsuback", \
	[MQTT_PTYPE_PINGREQ] =		"pingreq", \
	[MQTT_PTYPE_PINGRESP] =		"pingresp", \
	[MQTT_PTYPE_DISCONNECT] =	"disconnect", \
}

/**
 * MQTT "Last Will and Testament" (LWT) settings for the connect packet.
 */
struct mqtt_packet_will_options {
	const char *topic;
	/** The LWT topic to which the LWT message will be published. */
	const char *message;		/** The LWT payload. */

	/**
	 * The retained flag for the LWT message.
	 */
	unsigned char retained;

	/**
	 * The quality of service setting for the LWT message.
	 */
	char qos;
};

/**
 * MQTT connect data.
 */
struct mqtt_packet_connect_data {
	const char *client_id;
	unsigned short keep_alive;
	unsigned char cleansession;
	unsigned char will_flag;
	struct mqtt_packet_will_options will;
	const char *username;
	const char *password;
};

/*
 * Serializes the ack packet into the supplied buffer.
 *
 * \param buf the buffer into which the packet will be serialized.
 * \param buflen the length in bytes of the supplied buffer.
 * \param type the MQTT packet type.
 * \param dup the MQTT dup flag.
 * \param packetid the MQTT packet identifier.
 * \returns serialized length, or error if zero.
 */
int mqtt_packet_serialize_ack(unsigned char *buf, int buflen,
	unsigned char type,
	unsigned char dup, unsigned short packetid);

/*
 * Deserializes the supplied (wire) buffer into an ack.
 *
 * \param packettype returned integer - the MQTT packet type.
 * \param dup returned integer - the MQTT dup flag.
 * \param packetid returned integer - the MQTT packet identifier.
 * \param buf the raw buffer data, of the correct length determined by
 * the remaining length field.
 * \param buflen the length in bytes of the data in the supplied buffer.
 * \returns error code.  1 is success, 0 is failure.
 */
int mqtt_packet_deserialize_ack(unsigned char *packettype, unsigned char *dup,
	unsigned short *packetid, unsigned char *buf, int buflen);

/*
 * Serializes the connect options into the buffer.
 *
 * \param buf the buffer into which the packet will be serialized.
 * \param buflen the length in bytes of the supplied buffer.
 * \param options the options to be used to build the connect packet.
 * \returns serialized length, or error if 0.
 */
int mqtt_packet_serialize_connect(unsigned char *buf, int buflen,
	struct mqtt_packet_connect_data *options);

/*
 * Deserializes the supplied (wire) buffer into connack data - return code.
 *
 * \param sessionPresent the session present flag returned only for MQTT 3.1.1.
 * \param connack_rc returned integer value of the connack return code.
 * \param buf the raw buffer data, of the correct length determined by
 * the remaining length field.
 * \param buflen the length in bytes of the data in the supplied buffer.
 * \returns error code.  1 is success, 0 is failure.
 */
int mqtt_packet_deserialize_connack(unsigned char *sessionPresent,
	unsigned char *connack_rc, unsigned char *buf, int buflen);

/*
 * Serializes a disconnect packet into the supplied buffer.
 *
 * \param buf the buffer into which the packet will be serialized.
 * \param buflen the length in bytes of the supplied buffer, to avoid overruns.
 * \returns serialized length, or error if 0.
 */
int mqtt_packet_serialize_disconnect(unsigned char *buf, int buflen);

/*
 * Serializes a pingreq packet into the supplied buffer.
 *
 * \param buf the buffer into which the packet will be serialized.
 * \param buflen the length in bytes of the supplied buffer, to avoid overruns.
 * \returns serialized length, or error if 0.
 */
int mqtt_packet_serialize_pingreq(unsigned char *buf, int buflen);

/*
 * Serializes the supplied publish data into the supplied buffer.
 *
 * \param buf the buffer into which the packet will be serialized.
 * \param buflen the length in bytes of the supplied buffer.
 * \param dup integer - the MQTT dup flag.
 * \param qos integer - the MQTT QoS value.
 * \param retained integer - the MQTT retained flag.
 * \param packetid integer - the MQTT packet identifier.
 * \param topic_name - the MQTT topic in the publish.
 * \param payload byte buffer - the MQTT publish payload.
 * \param payloadlen integer - the length of the MQTT payload.
 * \returns the length of the serialized data.  <= 0 indicates error.
 */
int mqtt_packet_serialize_publish(unsigned char *buf, int buflen,
	unsigned char dup, int qos, unsigned char retained,
	unsigned short packetid, const char *topic_name,
	unsigned char *payload, int payloadlen);

/*
 * Serializes the supplied publish header data into the supplied buffer.
 *
 * \param buf the buffer into which the packet will be serialized.
 * \param buflen the length in bytes of the supplied buffer.
 * \param dup integer - the MQTT dup flag.
 * \param qos integer - the MQTT QoS value.
 * \param retained integer - the MQTT retained flag.
 * \param packetid integer - the MQTT packet identifier.
 * \param topic_name - the MQTT topic in the publish.
 * \param payloadlen integer - the length of the MQTT publish payload.
 * \returns the length of the serialized data.  <= 0 indicates error.
 */
int mqtt_packet_serialize_publish_header(unsigned char *buf, int buflen,
	unsigned char dup, int qos, unsigned char retained,
	unsigned short packetid, const char *topic_name,
	int payloadlen);

/*
 * Deserializes the supplied (wire) buffer into publish data.
 *
 * \param dup returned integer - the MQTT dup flag.
 * \param qos returned integer - the MQTT QoS value.
 * \param retained returned integer - the MQTT retained flag.
 * \param packetid returned integer - the MQTT packet identifier.
 * \param retained integer - the MQTT retained flag.
 * \param topic_name - dynamic value of the MQTT topic in the publish.
 * \param payload returned byte buffer - the MQTT publish payload.
 * \param payloadlen returned integer - the length of the MQTT payload.
 * \param buf the raw buffer data, of the correct length determined by the
 * remaining length field.
 * \param buflen the length in bytes of the data in the supplied buffer.
 * \returns error code.  1 is success.
 */
int mqtt_packet_deserialize_publish(unsigned char *dup, int *qos,
	unsigned char *retained, unsigned short *packetid,
	char **topic_namep, unsigned char **payload,
	int *payloadlen, unsigned char *buf, int buflen);


/*
 * Serializes the supplied subscribe data into the supplied buffer.
 *
 * \param buf the buffer into which the packet will be serialized.
 * \param buflen the length in bytes of the supplied buffer.
 * \param dup integer - the MQTT dup flag.
 * \param packetid integer - the MQTT packet identifier.
 * \param count - number of members in the topicFilters and reqQos arrays.
 * \param topicFilters - array of topic filter names.
 * \param requestedQoSs - array of requested QoS.
 * \returns the length of the serialized data.  <= 0 indicates error.
 */
int mqtt_packet_serialize_subscribe(unsigned char *buf,
	int buflen, unsigned char dup, unsigned short packetid,
	int count, const char **topic_filters,
	int requestedQoSs[]);

/*
 * Deserializes the supplied (wire) buffer into suback data.
 *
 * \param packetid returned integer - the MQTT packet identifier.
 * \param maxcount - the maximum number of members allowed
 * in the grantedQoSs array.
 * \param count returned integer - number of members in the grantedQoSs array.
 * \param grantedQoSs returned array of integers - the granted
 * qualities of service.
 * \param buf the raw buffer data, of the correct length determined by
 * the remaining length field.
 * \param buflen the length in bytes of the data in the supplied buffer.
 * \returns error code.  1 is success, 0 is failure.
 */
int mqtt_packet_deserialize_suback(unsigned short *packetid,
	int maxcount, int *count, int grantedQoSs[],
	unsigned char *buf, int len);

/*
 * Serializes the supplied unsubscribe data into the supplied buffer.
 *
 * \param buf the raw buffer data, of the correct length determined by
 * the remaining length field.
 * \param buflen the length in bytes of the data in the supplied buffer.
 * \param dup integer - the MQTT dup flag.
 * \param packetid integer - the MQTT packet identifier.
 * \param count - number of members in the topicFilters array.
 * \param topic_filters - array of topic filter names.
 * \returns the length of the serialized data.  <= 0 indicates error.
 */
int mqtt_packet_serialize_unsubscribe(unsigned char *buf, int buflen,
	unsigned char dup, unsigned short packetid,
	int count, const char **topic_filters);

/*
 * Deserializes the supplied (wire) buffer into unsuback data.
 *
 * \param packetid returned integer - the MQTT packet identifier.
 * \param buf the raw buffer data, of the correct length determined by
 * the remaining length field.
 * \param len the length in bytes of the data in the supplied buffer.
 * \returns error code.  1 is success, 0 is failure.
 */
int mqtt_packet_deserialize_unsuback(unsigned short *packetid,
	unsigned char *buf, int len);

#endif /* __AYLA_ADA_MQTT_PACKET_H__ */

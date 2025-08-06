/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADB_H__
#define __AYLA_ADB_H__

#define ADB_PASSKEY_MAX	999999	/* 6 digit decimal number */

#define ADB_GATT_MTU_DEFAULT	23		/* per BLE spec */
#define ADB_GATT_ATT_MAX_LEN	512		/* per BLE spec */
#define ADB_NOTIFY_OVERHEAD	3		/* opcode (1) + handle (2) */

/**
 * Attribute access errors defined by the Bluetooth spec
 */
enum adb_att_err {
	ADB_ATT_SUCCESS = 0x00,
	ADB_ATT_INVALID_HANDLE = 0x01,
	ADB_ATT_READ_NOT_PERMITTED = 0x02,
	ADB_ATT_WRITE_NOT_PERMITTED = 0x03,
	ADB_ATT_INVALID_PDU = 0x04,
	ADB_ATT_INSUFFICIENT_AUTHEN = 0x05,
	ADB_ATT_REQ_NOT_SUPPORTED = 0x06,
	ADB_ATT_INVALID_OFFSET = 0x07,
	ADB_ATT_INSUFFICIENT_AUTHOR = 0x08,
	ADB_ATT_PREPARE_QUEUE_FULL = 0x09,
	ADB_ATT_ATTR_NOT_FOUND = 0x0a,
	ADB_ATT_ATTR_NOT_LONG = 0x0b,
	ADB_ATT_INSUFFICIENT_KEY_SZ = 0x0c,
	ADB_ATT_INVALID_ATTR_VALUE_LEN = 0x0d,
	ADB_ATT_UNLIKELY = 0x0e,
	ADB_ATT_INSUFFICIENT_ENC = 0x0f,
	ADB_ATT_UNSUPPORTED_GROUP = 0x10,
	ADB_ATT_INSUFFICIENT_RES = 0x11,
	ADB_ATT_ERR_MAX = 0x12,
};

/**
 * IDs for primitive GATT attribute types.
 */
enum adb_attr_type {
	ADB_ATTR_NONE = 0,	/**< used as end of table marker */
	ADB_ATTR_SERVICE,	/**< a service */
	ADB_ATTR_CHR,		/**< a characteristic */
	ADB_ATTR_DESC,		/**< a descriptor */
};

/**
 * Event IDs for connection events.
 */
enum adb_conn_event {
	ADB_CONN_UP,		/**< a new connection has been established */
	ADB_CONN_DOWN,		/**< connection has gone down */
};

/**
 * Event IDs for ADB events.
 */
enum adb_event {
	ADB_EV_WIFI_PROVISION_START,	/**< wifi provisioning has started */
	ADB_EV_WIFI_PROVISION_STOP,	/**< wifi provisioning has stopped */
};

/**
 * Modes of pairing supported.
 */
enum adb_pairing_mode {
	ADB_PM_DISABLED,		/**< no pairing allowed */
	ADB_PM_NO_PASSKEY,	/**< no passkey, no mitm protection */
	ADB_PM_CONFIGURED_PASSKEY,	/**< require configured passkey */
	ADB_PM_RANDOM_PASSKEY,	/**< random passkey, requires a display */
	ADB_PM_AYLA_PASSKEY,	/**< require LAN IP key generated passkey */
};

/**
 * Structure used as the building block for defining attribute tables that
 * define Bluetooth GATT services.
 *
 * Note: The ADB_SERVICE macro is provided to simplify declaration and
 * initialization of an instance of this structure.
 *
 * A GATT service is defined by a table of GATT attributes arranged in a
 * specific way, as defined by the Bluetooth specification. A service is
 * defined by a hierarchy of service, characteristics, and descriptors
 * organized as a table.
 *
 *	service
 *	characteristic
 *	descriptor (optional, 0 or more per characteristic)
 *	...
 *	characteristic
 *	descriptor
 *	...
 */
struct adb_attr {
	enum adb_attr_type type;	/*!< type of this attribute */
	const char *name;		/*!< a short name for logging */
	const void *uuid;		/*!< platform specific UUID */
	const u32 access;		/*!< access flags */
	void *info;			/*!< type specific info */
	enum adb_att_err (*read_cb)(u16 conn,
	    const struct adb_attr *attr,
	    u8 *buf, u16 *length);	/*!< read callback */
	enum adb_att_err (*write_cb)(u16 conn,
	    const struct adb_attr *attr,
	    u8 *buf, u16 length);	/*!< write callback */
	enum adb_att_err (*subscribe_cb)(u16 conn, u8 notify,
	    u8 indicate);		/*!< subscribe callback */
	u8 (*read_redact_cb)
	    (int offset, u8 c);	/*!< read redactor function */
	u8 (*write_redact_cb)
	    (int offset, u8 c);	/*!< write redactor function */
};

/**
 * GATT service declaration macro.
 *
 * \param svc_name is a short name for logging.
 * \param svc_uuid is a pointer to the UUID for the service.
 * \param svc_info is a pointer to adb_service_info for the service.
 */
#define ADB_SERVICE(svc_name, svc_uuid, svc_info)	\
{							\
	.type = ADB_ATTR_SERVICE,			\
	.name = (svc_name),				\
	.uuid = (svc_uuid),				\
	.info = (svc_info),				\
}

/**
 * GATT characteristic declaration macro.
 *
 * \param chr_name is a short name for logging.
 * \param chr_uuid is a pointer to the UUID for the characteristic.
 * \param chr_access is access flags for the characteristic.
 * \param chr_read_cb is the read callback function.
 * \param chr_write_cb is the write callback function.
 * \param chr_info is a pointer to adb_chr_info for the characteristic.
 */
#define ADB_CHR(chr_name, chr_uuid, chr_access, chr_read_cb, \
    chr_write_cb, chr_info)				\
{							\
	.type = ADB_ATTR_CHR,				\
	.name = (chr_name),				\
	.uuid = (chr_uuid),				\
	.access = (chr_access),				\
	.read_cb = (chr_read_cb),			\
	.write_cb = (chr_write_cb),			\
	.info = (chr_info),				\
}

/**
 * GATT characteristic declaration macro with subscription callback.
 *
 * \param chr_name is a short name for logging.
 * \param chr_uuid is a pointer to the UUID for the characteristic.
 * \param chr_access is access flags for the characteristic.
 * \param chr_read_cb is the read callback function.
 * \param chr_write_cb is the write callback function.
 * \param chr_subscribe_cb is the subscribe callback function.
 * \param chr_info is a pointer to adb_chr_info for the characteristic.
 */
#define ADB_CHR_SUB(chr_name, chr_uuid, chr_access, chr_read_cb, \
    chr_write_cb, chr_subscribe_cb, chr_info)		\
{							\
	.type = ADB_ATTR_CHR,				\
	.name = (chr_name),				\
	.uuid = (chr_uuid),				\
	.access = (chr_access),				\
	.read_cb = (chr_read_cb),			\
	.write_cb = (chr_write_cb),			\
	.subscribe_cb = (chr_subscribe_cb),		\
	.info = (chr_info),				\
}

/**
 * GATT characteristic declaration macro with redaction callbacks.
 *
 * Redaction callbacks provide for redacting sensitive data like passwords
 * from log messages when dumping read and write buffers.
 *
 * \param chr_name is a short name for logging.
 * \param chr_uuid is a pointer to the UUID for the characteristic.
 * \param chr_access is access flags for the characteristic.
 * \param chr_read_cb is the read callback function.
 * \param chr_write_cb is the write callback function.
 * \param chr_info is a pointer to adb_chr_info for the characteristic.
 * \param chr_rd_redact_cb is the read redaction callback function.
 * \param chr_wr_redact_cb is the write redaction callback function.
 */
#define ADB_CHR_REDACT(chr_name, chr_uuid, chr_access, chr_read_cb, \
    chr_write_cb, chr_info, chr_rd_redact_cb, chr_wr_redact_cb) \
{							\
	.type = ADB_ATTR_CHR,				\
	.name = (chr_name),				\
	.uuid = (chr_uuid),				\
	.access = (chr_access),				\
	.read_cb = (chr_read_cb),			\
	.write_cb = (chr_write_cb),			\
	.info = (chr_info),				\
	.read_redact_cb = (chr_rd_redact_cb),		\
	.write_redact_cb = (chr_wr_redact_cb),		\
}

/**
 * GATT descriptor declaration macro.
 *
 * \param desc_name is a short name for logging.
 * \param desc_uuid is a pointer to the UUID for the descriptor.
 * \param desc_access is access flags for the descriptor.
 * \param desc_read_cb is the read callback function.
 * \param desc_write_cb is the write callback function.
 * \param desc_info is a pointer to adb_chr_info for the descriptor.
 */
#define ADB_DESC(desc_name, desc_uuid, desc_access, desc_read_cb, \
    desc_write_cb, desc_info)				\
{							\
	.type = ADB_ATTR_DESC,				\
	.name = (desc_name),				\
	.uuid = (const ble_uuid_any_t *)(desc_uuid),	\
	.access = (desc_access),			\
	.read_cb = (desc_read_cb),			\
	.write_cb = (desc_write_cb),			\
	.info = (desc_info),				\
}

/**
 * Structure used to hold data specific to Bluetooth GATT characteristic
 * attributes.
 */
struct adb_chr_info {
	void *pfm_arg;		/*!< platform specific info */
	void *value;		/*!< pointer to value storage location */
	u16 handle;		/*!< handle assigned at registration */
	u16 max_val_len;	/*!< value storage location size in bytes */
	u16 value_length;	/*!< current value length in bytes */
	u8 notify_mask;		/*!< connections to notify */
	u8 indicate_mask;	/*!< connections to indicate */
};

/**
 * Structure used to hold data specific to Bluetooth GATT characteristic
 * attributes.
 */
struct adb_service_info {
	void *pfm_arg;		/*!< platform specific info */
	u16 handle;		/*!< handle assigned at registration */
	u8 is_primary:1;	/*!< indicate if service is primary */
};

/**
 * Service attribute table end macro.
 *
 * Used to mark the end of a service's attribute table.
 */
#define ADB_SERVICE_END()				\
{							\
	.type = ADB_ATTR_NONE,				\
}

/**
 * Log a message to the Bluetooth category.
 *
 * \param fmt is a C format string.
 */
void adb_log(const char *fmt, ...);

/**
 * Dump a buffer to the Bluetooth category at DEBUG2 level.
 *
 * \param prefix is a short prefix to include in each log line.
 * \param buf is a pointer to the buffer to be dumped.
 * \param length is the number of bytes to dump.
 * \param redact_cb is a callback for redaction or NULL, if not needed
 */
void adb_dump_log(const char *prefix, const u8 *buf, u16 length,
    u8 (*redact_cb)(int offset, u8 c));

/**
 * Print a Bluetooth device address into a buffer.
 *
 * \param addr is a pointer to the six byte Bluetooth address.
 * \param buf is the buffer to snprint to.
 * \param length is the length of buf.
 */
void adb_snprint_addr(const u8 *addr, char *buf, size_t length);

/**
 * Set the pairing mode.
 *
 * \param mode is the pairing mode to configure.
 * \param duration is the duration in seconds to enable this mode, 0 means
 * no time limit.
 */
void adb_pairing_mode_set(enum adb_pairing_mode mode, u16 duration);

/**
 * Get the current pairing mode.
 *
 * \returns current pairing mode.
 */
enum adb_pairing_mode adb_pairing_mode_get(void);

/**
 * Get the name of the current pairing mode.
 *
 * \returns name of current pairing mode.
 */
const char *adb_pairing_mode_name_get(void);

/**
 * Register a handler for connection events.
 *
 * \param handler is the event handler function pointer.
 * \param arg is an opaque pointer that is passed to the handler.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adb_conn_event_register(void (*handler)(enum adb_conn_event event,
    u16 conn, void *arg), void *arg);

/**
 * Deregister a connection event handler.
 *
 * \param handler is the event handler function pointer.
 */
void adb_conn_event_deregister(void (*handler)(enum adb_conn_event event,
    u16 conn, void *arg));

/**
 * Register a handler for adb events.
 *
 * \param handler is the event handler function pointer.
 */
enum ada_err adb_event_register(void (*handler)(enum adb_event event));

/**
 * Notify ADB event handler of an event.
 *
 * \param event is the event type.
 */
void adb_event_notify(enum adb_event event);

/**
 * Send a connection event notification to registered handlers.
 *
 * \param event is the id of the event being notified.
 * \param conn is the connection handle the event relates to.
 */
void adb_conn_event_notify(enum adb_conn_event event, u16 conn);

/**
 * Generic read callback function to read a value from the value location
 * specified when defining the characteristic.
 *
 * \param conn is the connection making the request.
 * \param attr is the attribute being read.
 * \param buf is a pointer to a buffer to read value into.
 * \param length is a pointer to the length, set to the length of the buffer
 * on input and the length of the data read on return.
 * \returns ADB_ATT_SUCCESS if successful, otherwise, an error.
 */
enum adb_att_err adb_chr_read(u16 conn, const struct adb_attr *attr, u8 *buf,
    u16 *length);

/**
 * Generic read callback function to read a string value from the value location
 * specified when defining the characteristic.
 *
 * The nul at the end of the string is not included in the data copied
 * to the buffer.
 *
 * \param conn is the connection making the request.
 * \param attr is the attribute being read.
 * \param buf is a pointer to a buffer to read value into.
 * \param length is a pointer to the length, set to the length of the buffer
 * on input and the length of the data read on return.
 * \returns ADB_ATT_SUCCESS if successful, otherwise, an error.
 */
enum adb_att_err adb_chr_read_str(u16 conn, const struct adb_attr *attr,
    u8 *buf, u16 *length);

/**
 * Generic write callback function to write a value to the value location
 * specified when defining the characteristic.
 *
 * Only as much data as fits will be written to the value.
 *
 * \param conn is the connection making the request.
 * \param attr is the attribute being written.
 * \param buf is a pointer to a buffer containing the data to write.
 * \param length of the data in the buffer.
 * \returns ADB_ATT_SUCCESS if successful, otherwise, an error.
 */
enum adb_att_err adb_chr_write(u16 conn, const struct adb_attr *attr, u8 *buf,
    u16 length);

/**
 * Generic write callback function to write a string value to the value
 * location specified when defining the characteristic.
 *
 * Only as much data as fits will be written to the value. A nul value will
 * always be included at the end of the string that is written.
 *
 * \param conn is the connection making the request.
 * \param attr is the attribute being written.
 * \param buf is a pointer to a buffer containing the data to write.
 * \param length of the data in the buffer.
 * \returns ADB_ATT_SUCCESS if successful, otherwise, an error.
 */
enum adb_att_err adb_chr_write_str(u16 conn, const struct adb_attr *attr,
    u8 *buf, u16 length);

#endif

/*
 * Copyright 2015-2016 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_SPROP_H__
#define __AYLA_ADA_SPROP_H__

#include <ada/prop.h>

#define SPROP_NAME_MAX_LEN      PROP_NAME_LEN
#define SPROP_TABLE_ENTRIES     10
#define SPROP_DEF_FILECHUNK_SIZE	512

struct ada_sprop {
	const char *name;
	enum ayla_tlv_type type;
	void *val;
	size_t val_len;
	ssize_t (*get)(struct ada_sprop *, void *buf, size_t len);
	enum ada_err (*set)(struct ada_sprop *, const void *buf, size_t len);
	u8 send_req;	/* flag indicating to send once connected to cloud */
	u8 source_mask;	/* mask indicating which node did the most recent set */
	char *ack_id;	/**< ack id of property, if non-NULL */
	struct prop_dp_meta *metadata; /**< metadata */
};

#ifdef AYLA_FILE_PROP_SUPPORT
enum file_dp_state {
	FD_IDLE = 0,	/* nothing to do */
	FD_CREATE,	/* send datapoint create */
	FD_SEND,	/* send datapoint value */
	FD_RECV,	/* request and receive datapoint value */
	FD_FETCHED,	/* send datapoint fetched opcode */
	FD_ABORT,	/* abort the datapoint operation */
};

struct file_dp {
	enum file_dp_state state;
	struct ada_sprop *sprop;	/* associated property entry */
	u32 next_off;		/* next offset to transfer */
	u32 tot_len;		/* total length expected (if known) */
	size_t chunk_size;		/* max chunk size for file transfer */
	u8 aborted;		/* 1 if last op for this prop was aborted */
	void *val_buf;		/* pointer to the next chunk */
	char loc[PROP_LOC_LEN];	/* location / data point key */
	/* callback to handle incoming data */
	size_t (*file_get)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len);
	/* callback to handle outgoing data */
	enum ada_err (*file_set)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len, u8 eof);
	/* optional callback to indicate completion status */
	void (*file_result)(struct ada_sprop *sprop, enum ada_err err);
	struct prop_dp_meta *metadata; /**< metadata */
	u8 meta_count;		/**< metadata item count */
};

/*
 * Initialize a file property struct.
 * (*file_get) is used to retrieve file chunks for file upload.
 * (*file_set) is used to send file property chunks during file download.
 * (*file_result) is an optional callback providing completion status.
 * chunk_size is the maximum allowed size of file chunk for transfer.
 */
void ada_sprop_file_init_v2(struct file_dp *dp,
	size_t (*file_get)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len),
	enum ada_err (*file_set)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len, u8 eof),
	void (*file_result)(struct ada_sprop *sprop, enum ada_err err),
	size_t chunk_size);

/*
 * Initialize a file property struct.
 * This is a deprecated interface for source compatibility.
 * New code should sue ada_sprop_file_init_v2() or ada_sprop_file_alloc().
 */
void ada_sprop_file_init(struct file_dp *dp,
	size_t (*file_get)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len),
	enum ada_err (*file_set)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len, u8 eof),
	size_t chunk_size);

/*
 * Allocate and initialize a file property struct.
 * (*file_get) is used to retrieve file chunks for file upload.
 * (*file_set) is used to send file property chunks during file download.
 * (*file_result) is an optional callback providing completion status.
 * chunk_size is the maximum allowed size of file chunk for transfer.
 */
enum ada_err ada_sprop_file_alloc(const char *prop_name,
	size_t (*file_get)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len),
	enum ada_err (*file_set)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len, u8 eof),
	void (*file_result)(struct ada_sprop *sprop, enum ada_err err),
	size_t chunk_size);

/*
 * Begin file or message property upload.
 */
enum ada_err ada_sprop_file_start_send(const char *name, size_t len);

/*
 * Begin file or message property upload with metadata.
 */
enum ada_err ada_sprop_file_start_send_with_meta(const char *name,
		size_t len, struct prop_dp_meta *meta, u8 meta_count);

/*
 * Begin file or message property download.
 */
enum ada_err ada_sprop_file_start_recv(const char *name, const void *buf,
				size_t len, u32 off);

/*
 * Abort ongoing file transfer.
 */
void ada_sprop_file_abort(void);

/*
 * Mark file datapoint fetched.
 */
enum ada_err ada_sprop_file_fetched(const char *name);
#endif

/*
 * Get an ATLV_INT or ATLV_CENTS type property from the
 * sprop structure.
 */
ssize_t ada_sprop_get_int(struct ada_sprop *, void *buf, size_t len);

/*
 * Get an ATLV_UINT type property from the sprop structure.
 */
ssize_t ada_sprop_get_uint(struct ada_sprop *, void *buf, size_t len);

/*
 * Get an ATLV_BOOL type property from the sprop structure.
 */
ssize_t ada_sprop_get_bool(struct ada_sprop *, void *buf, size_t len);

/*
 * Get an ATLV_UTF8 type property from the sprop structure.
 */
ssize_t ada_sprop_get_string(struct ada_sprop *, void *buf, size_t len);

/*
 * Set an ATLV_INT or ATLV_CENTS property value to the
 * value in *buf.
 */
enum ada_err ada_sprop_set_int(struct ada_sprop *, const void *buf, size_t len);

/*
 * Set an ATLV_UINT property value to the value in *buf.
 */
enum ada_err ada_sprop_set_uint(struct ada_sprop *,
				const void *buf, size_t len);

/*
 * Set an ATLV_BOOL property value to the value in *buf.
 */
enum ada_err ada_sprop_set_bool(struct ada_sprop *,
				const void *buf, size_t len);

/*
 * Set an ATLV_UTF8 property value to the value in *buf.
 */
enum ada_err ada_sprop_set_string(struct ada_sprop *,
				const void *buf, size_t len);

/*
 * Send property update.
 */
enum ada_err ada_sprop_send(struct ada_sprop *);

/**
 * Send the locally updated property to ADS server with metadata.
 * \param prop a pointer to struct ada_sprop.
 * \param metadata a pointer to struct prop_dp_meta.
 * \param count is number of metadata key value pair.
 * \returns 0 on success.
 */
enum ada_err ada_sprop_send_with_meta(struct ada_sprop *prop,
		struct prop_dp_meta *metadata, u8 count);

/*
 * Send a property update by name.
 */
enum ada_err ada_sprop_send_by_name(const char *);

/**
 * Send the property to ADS server by name with metadata.
 * \param name is the name of property to be sent.
 * \param metadata a pointer to struct prop_dp_meta.
 * \param count is number of metadata key value pair.
 * \returns 0 on success.
 */
enum ada_err ada_sprop_send_by_name_with_meta(const char *name,
			struct prop_dp_meta *metadata, u8 count);

/*
 * Send a property to specific destinations.
 *
 * The send is an echo if for a to-device property (one with a set function).
 *
 * The current value is sent to connected destinations specified in dest_mask.
 *
 * If the cloud is not connected, a flag is set to cause an echo to be sent
 * when the cloud is next connected.
 */
enum ada_err ada_sprop_send_to(struct ada_sprop *sprop, u8 dest_mask);

/*
 * Send a property to specific destinations by name.
 * See ada_sprop_send_to().
 */
enum ada_err ada_sprop_send_to_by_name(const char *name, u8 dest_mask);

/*
 * Echo a property to specified destinations.
 *
 * The send is an echo if for a to-device property (one with a set function).
 *
 * The dest_mask parameter is a bitmask of destinations to be included,
 * and can usually be ~0 (all bits set), which will echo the value to the
 * cloud service (ADS) and all connected LANs.
 *
 * The source of the latest set is automatically excluded from the echo.
 *
 * If the cloud is not connected and the echo is going to the cloud,
 * a flag is set to cause an echo to be sent when the cloud is next connected.
 *
 * Note that when the auto-sync option is selected in the LAN settings portion
 * of the device's template, echoing is handled by the agent, and the use of
 * this API is unnecessary.
 */
enum ada_err ada_sprop_send_echo(struct ada_sprop *sprop, u8 dest_mask);

/*
 * Send echo to specified destinations only.
 * See ada_sprop_send_echo().
 */
enum ada_err ada_sprop_send_echo_by_name(const char *name, u8 dests);

/*
 * Register a table of properties to the generic prop_mgr.
 */
enum ada_err ada_sprop_mgr_register(char *name, struct ada_sprop *table,
		unsigned int entries);

/*
 * Mask of currently-connected destinations.
 */
extern u8 ada_sprop_dest_mask;

/**
 * Send a property ack to ADS server.
 *
 * \param sprop a pointer to struct ada_sprop.
 * \param status is 0 for success, 1 for error.
 * \param ack_message is a customer-defined ack code.
 * \returns 0 on success.
 */
enum ada_err ada_sprop_send_ack(struct ada_sprop *sprop, u8 status,
	int ack_message);

#endif /* __AYLA_ADA_SPROP_H__ */

/*
 * Copyright 2014-2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PROP_MGR_H__
#define __AYLA_PROP_MGR_H__

#include <ayla/nameval.h>

enum prop_mgr_event {
	PME_TIME,		/* time parameters updated */
	PME_PROP_SET,		/* property value was set (and maybe changed) */
	PME_NOTIFY,		/* ANS notification received */
	PME_ECHO_FAIL,		/* echo of property failed */
	PME_TIMEOUT,		/* timeout waiting for property manager */
	PME_FILE_DONE,		/* file transfer complete */
	PME_DISCONNECT,		/* disconnected from dest specified by arg */
	PME_GET_INPUT_START,	/* GET of all to-device properties starting */
	PME_GET_INPUT_END,	/* GET of all to-device properties completed */
	PME_LAN_STATE_CHANGE,	/* LAN state change. Note: for Ayla use only */
};

/*
 * Optional event callback arg.
 */
union prop_mgr_event_arg {
	char *prop_name;		/* property name */
	void (*continue_recv)(void);	/* continue receiving data */
};

struct prop;
struct prop_recvd;

/*
 * Structure that a handler of properties uses to register with
 * the Ayla Device Client.
 */
struct prop_mgr {
	const char *name;

	/*
	 * Receive property value from ADS or app.
	 * This should be NULL if metadata is supported.
	 */
	enum ada_err (*prop_recv)(const char *name, enum ayla_tlv_type type,
			const void *val, size_t len,
			size_t *offset, u8 src, void *cb_arg);

	/*
	 * Receive property value with metadata from ADS or app.
	 */
	enum ada_err (*prop_meta_recv)(const char *name,
			enum ayla_tlv_type type,
			const void *val, size_t len,
			size_t *offset, u8 src, void *cb_arg,
			const char *ack_id, struct prop_dp_meta *metadata);

	/*
	 * Callback to report success/failure of property post to ADS/apps.
	 * status will be PROP_CB_DONE on success.
	 * fail_mask contains a bitmask of failed destinations.
	 */
	void (*send_done)(enum prop_cb_status, u8 fail_mask, void *cb_arg);

	/*
	 * ADS/app wants to fetch a value of a property.
	 * Function must call (*get_cb) with the value of the property.
	 */
	enum ada_err (*get_val)(const char *name,
	    enum ada_err (*get_cb)(struct prop *, void *arg, enum ada_err),
	    void *arg);

	/*
	 * ADC reports a change in it's connectivity. Bit 0 is ADS, others
	 * are mobile clients.
	 */
	void (*connect_status)(u8 mask);

	/*
	 * ADC reports an event to all property managers.
	 */
	void (*event)(enum prop_mgr_event, const void *arg);
};

/*
 * Property manager uses this to register itself as a handler of properties.
 */
void ada_prop_mgr_register(const struct prop_mgr *mgr);

/*
 * Report connectivity status to all property managers.
 */
void prop_mgr_connect_sts(u8 mask);

/*
 * Send event to all interested property managers.
 */
void prop_mgr_event(enum prop_mgr_event, void *);

/*
 * Get the prop currently being sent.
 */
struct prop *prop_mgr_get_send_prop(void);

/*
 * Property manager reports it's ready to receive data.
 */
void ada_prop_mgr_ready(const struct prop_mgr *);

/**
 * Post a property to ADS/app. (*prop_mgr_done) is called when operation is
 * done.
 *
 * \param prop is the property to be sent. The caller must set all relevant
 * fields in the prop structure prior to calling this function.
 *
 * \returns 0 on success.
 */
enum ada_err ada_prop_mgr_prop_send(struct prop *prop);

/**
 * Post a property to ADS/app. (*send_cb) is called when operation is done.
 *
 * \param mgr is property manager.
 * \param prop is the property to be sent.
 * \param dest_mask is the mask of destination.
 * \param cb_arg is the argument for callback.
 *
 * \returns 0 on success.
 */
enum ada_err ada_prop_mgr_send(const struct prop_mgr *, struct prop *,
			u8 dest_mask, void *cb_arg);

/*
 * Indicate to agent that property recv is complete.
 * After a property manager's prop_recv() handler returns AE_BUF, receive is
 * paused until this is called.
 *
 * The src_mask argument indicates the source of the recv request.
 */
void ada_prop_mgr_recv_done(u8 src_mask);

#ifdef AYLA_FILE_PROP_SUPPORT
/*
 * Put a file property upload to S3 server.
 */
enum ada_err ada_prop_mgr_dp_put(const struct prop_mgr *pm, struct prop *prop,
	u32 off, size_t tot_len, u8 eof, void *cb_arg);

/*
 * Do a file property download from S3 server.
 */
enum ada_err ada_prop_mgr_dp_get(const struct prop_mgr *pm, struct prop *prop,
	const char *location, u32 off, size_t max_chunk_size,
	enum ada_err (*prop_mgr_dp_process_cb)(const char *location, u32 off,
	    void *buf, size_t len, u8 eof), void *cb_arg);

/*
 * Mark a file property datapoint fetched.
 */
enum ada_err ada_prop_mgr_dp_fetched(const struct prop_mgr *pm,
	struct prop *prop, const char *location, void *cb_arg);

/*
 * Abort an ongoing file transfer.
 */
void ada_prop_mgr_dp_abort(const char *location);
#endif

/*
 * Get a property from a property manager.
 * On success, eventually calls (*get_cb) with the value of the property.
 * Returns 0 on success.
 */
enum ada_err ada_prop_mgr_get(const char *name,
		enum ada_err (*get_cb)(struct prop *, void *arg, enum ada_err),
		void *arg);

/*
 * Set a prop using a property manager including ACK ID and metadata.
 */
enum ada_err ada_prop_mgr_meta_set(const char *name, enum ayla_tlv_type type,
			const void *val, size_t val_len,
			size_t *offset, u8 src, void *set_arg,
			const char *ack_id, struct prop_dp_meta *metadata);

/*
 * Set a prop using a property manager.
 * For source compatibility with old host programs.
 */
enum ada_err ada_prop_mgr_set(const char *name, enum ayla_tlv_type type,
			const void *val, size_t val_len,
			size_t *offset, u8 src, void *set_arg);

/*
 * Request a property value from ADS.
 * A NULL name gets all to-device properties.
 */
enum ada_err ada_prop_mgr_request(const char *name);

/*
 * Request a property value from ADS.
 * A NULL name gets all to-device properties.
 * This version supplies a callback which should free the property structure.
 */
enum ada_err ada_prop_mgr_request_get(const char *name,
		void (*cb)(struct prop *prop));

extern const struct name_val prop_types[];

/*
 * Internal initialization call.
 */
void prop_mgr_init(void);

extern const char ada_prop_test_cli_help[];

/*
 * Enable property test for given seconds.
 *
 */
void ada_prop_test_cli(int, char **);

extern struct prop_recvd prop_recvd;

#endif /* __AYLA_PROP_MGR___ */

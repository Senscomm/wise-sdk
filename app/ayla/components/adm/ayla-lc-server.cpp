/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 */
#include <string.h>
#include <stdio.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <protocols/interaction_model/StatusCode.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app/EventLogging.h>
#include <app/InteractionModelEngine.h>
#include <app/util/attribute-storage.h>
#include <app/AttributeAccessInterface.h>
#include <app/data-model/Nullable.h>
#include <app/reporting/reporting.h>
#include <app/util/af.h>
#include <app/util/util.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/conf.h>
#include <ayla/clock.h>
#include <ayla/timer.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/wifi_error.h>
#include <ada/err.h>
#include <ada/ada_conf.h>
#include <ada/prop.h>
#include <ada/client.h>
#include <ada/local_control.h>
#include <adm/adm.h>
#include <adm/adm_ayla_base.h>
#include <adm/adm_local_control.h>
#include <sys/queue.h>
#include <al/al_os_mem.h>
#include <al/al_os_lock.h>
#include <al/al_os_thread.h>
#include <ayla/callback.h>
#include "client_timer.h"
#include "client_lock.h"

#define ADM_LC_MAILBOXES	3
#define ADM_LC_MTU		512
#define ADM_LC_MAX_RETRY	10

struct adm_outbox {
	size_t len;
	u8 *buf;
};

static struct generic_session *sessions[ADM_LC_MAILBOXES];
static struct adm_outbox outboxes[ADM_LC_MAILBOXES];
static struct timer ka_timer[ADM_LC_MAILBOXES];

extern "C" void adm_log(const char *fmt, ...);

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::AylaLocalControl;
using namespace chip::app::Clusters::AylaLocalControl::Attributes;
using namespace chip::Protocols::InteractionModel;

/*
 * Out message queue data struct.
 */
struct out_msg {
	u8 outbox;
	u8 count;
	char *msg;
	STAILQ_ENTRY(out_msg) list; /* list linkage for queue */
};

/* declares queue head struct */
STAILQ_HEAD(out_msg_queue, out_msg);

/* Init queue head */
static struct out_msg_queue lc_out_msg_queue =
	STAILQ_HEAD_INITIALIZER(lc_out_msg_queue);

static struct al_lock *lc_queue_lock;

static struct callback lc_msg_callback;

static enum ada_err (*adm_ext_attr_cb)(
		u16 ep_id, u16 cls_id, u16 attr_id,
		enum adm_attr_rw_type type, u8 *buff, u16 len);

extern "C" void adm_ext_attr_cb_register(enum ada_err (*callback)(
		u16 ep_id, u16 cls_id, u16 attr_id,
		enum adm_attr_rw_type type, u8 *buff, u16 len))
{
	adm_ext_attr_cb = callback;
}

static enum ada_err lc_msg_write(struct out_msg *qmsg)
{
	AttributeId id;
	enum ada_err err;

	ASSERT(qmsg);
	switch (qmsg->outbox) {
	case 0:
		id = Outbox0::Id;
		break;
	case 1:
		id = Outbox1::Id;
		break;
	case 2:
		id = Outbox2::Id;
		break;
	default:
		ASSERT_NOTREACHED();
	}

	err = adm_write_string(0, AylaLocalControl::Id, id, qmsg->msg);
	if (err != AE_OK) {
		adm_log(LOG_DEBUG "%s: adm_write_string write %s ret %d",
		    __func__, qmsg->msg, err);
	} else {
		adm_log(LOG_DEBUG "%s: adm_write_string write %s ret OK",
		    __func__, qmsg->msg);
	}
	return err;
}

static void lc_msg_cont_cb(void *arg)
{
	struct out_msg *qmsg;
	enum ada_err err;

	al_os_lock_lock(lc_queue_lock);
	qmsg = STAILQ_FIRST(&lc_out_msg_queue);
	if (!qmsg) {
		al_os_lock_unlock(lc_queue_lock);
		return;
	}

	err = lc_msg_write(qmsg);
	if (err == AE_OK) {
		STAILQ_REMOVE_HEAD(&lc_out_msg_queue, list);
		al_os_mem_free(qmsg->msg);
		al_os_mem_free(qmsg);
	} else {
		if (err != AE_BUSY) {
			qmsg->count++;
		}
		if (qmsg->count >= ADM_LC_MAX_RETRY) {
			adm_log(LOG_WARN "%s: Remove msg after %d times",
			    __func__, qmsg->count);
			STAILQ_REMOVE_HEAD(&lc_out_msg_queue, list);
			al_os_mem_free(qmsg->msg);
			al_os_mem_free(qmsg);
		}
	}
	al_os_lock_unlock(lc_queue_lock);

	client_callback_pend(&lc_msg_callback);
}

static enum ada_err lc_mbox_init(void)
{
	static bool mbox_init_flag;
	if (mbox_init_flag) {
		adm_log(LOG_DEBUG "%s: already init", __func__);
		return AE_OK;
	}

	/* Create a lock for queue op */
	lc_queue_lock = al_os_lock_create();
	if (!lc_queue_lock) {
		adm_log(LOG_ERR "%s: create lc_queue_lock failed", __func__);
		return AE_ALLOC;
	}

	callback_init(&lc_msg_callback, lc_msg_cont_cb, NULL);

	mbox_init_flag = true;
	return AE_OK;
}

static void lc_queue_free(u8 mailbox)
{
	struct out_msg *qmsg;
	adm_log(LOG_INFO "%s: free outbox%u queue items", __func__, mailbox);
	al_os_lock_lock(lc_queue_lock);
	STAILQ_FOREACH(qmsg, &lc_out_msg_queue, list) {
		if (qmsg->outbox == mailbox) {
			STAILQ_REMOVE(&lc_out_msg_queue, qmsg, out_msg, list);
		}
	}
	al_os_lock_unlock(lc_queue_lock);
}

static void lc_outbox_queue_add(struct out_msg *msg)
{
	al_os_lock_lock(lc_queue_lock);
	STAILQ_INSERT_TAIL(&lc_out_msg_queue, msg, list);
	al_os_lock_unlock(lc_queue_lock);
	client_callback_pend(&lc_msg_callback);
}

static enum ada_err lc_msg_add(u8 outbox, const char *str, u16 length)
{
	char *buf;
	struct out_msg *qmsg;

	if (!lc_queue_lock) {
		lc_queue_lock = al_os_lock_create();
		if (!lc_queue_lock) {
			return AE_ALLOC;
		}
		callback_init(&lc_msg_callback, lc_msg_cont_cb, NULL);
	}

	buf = (char *)al_os_mem_alloc(length + 1);
	if (!buf) {
		adm_log(LOG_ERR "%s: malloc %u failed", __func__, length + 1);
		return AE_ALLOC;
	}
	qmsg = (struct out_msg *)al_os_mem_alloc(sizeof(struct out_msg));
	if (!qmsg) {
		adm_log(LOG_ERR "%s: malloc %u failed",
		    __func__, sizeof(struct out_msg));
		al_os_mem_free(buf);
		return AE_ALLOC;
	}

	memcpy(buf, str, length);
	buf[length] = '\0';
	qmsg->outbox = outbox;
	qmsg->count = 0;
	qmsg->msg = buf;

	lc_outbox_queue_add(qmsg);
	return AE_OK;
}

extern "C" enum ada_err adm_mbox_svc_msg_tx(struct generic_session *gs, u8 *msg,
    u16 length)
{
	u8 mailbox;
	enum ada_err err;

	ASSERT(gs);
	ASSERT(msg);
	ASSERT(length);

	mailbox = (u32)gs->ctxt;
	adm_log(LOG_DEBUG2 "%s: mailbox %u, msg %s",
	    __func__, mailbox, (char *)msg);
	log_dump(MOD_LOG_CLIENT, LOG_SEV_DEBUG2, "out: ", msg, length, NULL);

	err = lc_msg_add(mailbox, (const char *)msg, length);
	if (err != AE_OK) {
		adm_log(LOG_ERR "%s: lc_msg_add ret %d", __func__, err);
		return err;
	}

	return AE_OK;
}

extern "C" enum ada_err adm_local_control_mtu_get(
    struct generic_session *gs, u32 *mtu)
{
	*mtu = ADM_LC_MTU;
	return AE_OK;
}

/*
 * Close the underlying mailbox but leave it to the caller to free the
 * session.
 */
extern "C" enum ada_err adm_mbox_svc_session_close(struct generic_session *gs)
{
	u8 mailbox = (u32)gs->ctxt;
	u8 *buf;

	adm_log(LOG_DEBUG "%s: close session outbox%u", __func__, mailbox);

	ASSERT(mailbox < ARRAY_LEN(sessions));

	sessions[mailbox] = NULL;

	buf = outboxes[mailbox].buf;
	if (buf) {
		outboxes[mailbox].buf = NULL;
		outboxes[mailbox].len = 0;
		free(buf);
	}

	lc_queue_free(mailbox);

	return AE_OK;
}

extern "C" void adm_mbox_keep_alive_timeout(struct timer *timer)
{
	u8 mailbox = (timer - &ka_timer[0]) / sizeof(ka_timer[0]);
	if (!sessions[mailbox]) {
		adm_log(LOG_DEBUG "%s: skip, session outbox%u closed",
		    __func__, mailbox);
		return;
	}
	client_unlock();
	lctrl_session_down(sessions[mailbox]);
	client_lock();
	adm_mbox_svc_session_close(sessions[mailbox]);
}

extern "C" enum ada_err adm_mbox_keep_alive(struct generic_session *gs, u32 period)
{
	u8 mailbox = (u32)gs->ctxt;
	/* Start keep alive timer with new period(seconds) */
	client_timer_set(&ka_timer[mailbox], period * 1000);
	return AE_OK;
}

namespace {

class AylaLocalControlAccess : public AttributeAccessInterface
{
public:
	AylaLocalControlAccess() : AttributeAccessInterface(
	    Optional<EndpointId>::Missing(), AylaLocalControl::Id) {}

	CHIP_ERROR Read(const ConcreteReadAttributePath &aPath,
	    AttributeValueEncoder &aEncoder) override;
	CHIP_ERROR Write(const ConcreteDataAttributePath &aPath,
	    AttributeValueDecoder &aDecoder) override;

private:
	CHIP_ERROR ReadInbox(uint8_t mailbox, AttributeValueEncoder &aEncoder);
	CHIP_ERROR ReadOutbox(uint8_t mailbox, AttributeValueEncoder &aEncoder);
	CHIP_ERROR WriteMailboxInuse(uint8_t mailbox,
	    AttributeValueDecoder &aDecoder);
	CHIP_ERROR WriteInbox(uint8_t mailbox, AttributeValueDecoder &aDecoder);
};

static AylaLocalControlAccess gAylaLocalControlAttrAccess;

CHIP_ERROR AylaLocalControlAccess::ReadInbox(uint8_t mailbox,
    AttributeValueEncoder &aEncoder)
{
	struct generic_session *gs = sessions[mailbox];
	CHIP_ERROR chip_err;

	if (gs && gs->rx_buffer && gs->rx_length) {
		chip_err = aEncoder.Encode(
		    chip::CharSpan((char *)gs->rx_buffer, gs->rx_length));
	} else {
		chip_err = aEncoder.Encode(chip::CharSpan());
	}

	if (chip_err != CHIP_NO_ERROR) {
		adm_log(LOG_ERR "%s: Encode inbox%u error",
		    __func__, mailbox);
	}

	return chip_err;
}

CHIP_ERROR AylaLocalControlAccess::ReadOutbox(uint8_t mailbox,
    AttributeValueEncoder &aEncoder)
{
	u8 *buf;
	size_t length;
	CHIP_ERROR chip_err;

	buf = outboxes[mailbox].buf;
	outboxes[mailbox].buf = NULL;
	length = outboxes[mailbox].len;
	outboxes[mailbox].len = 0;

	if (buf) {
		chip_err = aEncoder.Encode(chip::CharSpan((char *)buf, length));
		free(buf);
	} else {
		chip_err = aEncoder.Encode(chip::CharSpan());
	}

	if (chip_err != CHIP_NO_ERROR) {
		adm_log(LOG_ERR "%s: Encode outbox%u error",
		    __func__, mailbox);
	}

	return chip_err;
}

CHIP_ERROR AylaLocalControlAccess::Read(const ConcreteReadAttributePath &aPath,
    AttributeValueEncoder &aEncoder)
{
	CHIP_ERROR chip_err = CHIP_NO_ERROR;
	uint16_t mailbox_mask;
	int i;

	if (aPath.mClusterId != AylaLocalControl::Id)
	{
		return CHIP_ERROR_INVALID_ARGUMENT;
	}

	switch (aPath.mAttributeId) {
	case MailboxMask::Id:
		mailbox_mask = 0;
		for (i = 0; i < ARRAY_LEN(sessions); i++) {
			if (!sessions[i]) {
				mailbox_mask |= 1 << i;
			}
		}
		adm_log(LOG_DEBUG "%s: MailboxMask 0x%x",
		    __func__, mailbox_mask);
		chip_err = aEncoder.Encode(mailbox_mask);
		break;
	case Mailbox0InUse::Id:
	        chip_err = aEncoder.Encode(sessions[0] != NULL);
		break;
	case Mailbox1InUse::Id:
	        chip_err = aEncoder.Encode(sessions[1] != NULL);
		break;
	case Mailbox2InUse::Id:
	        chip_err = aEncoder.Encode(sessions[2] != NULL);
		break;
	case Inbox0::Id:
		chip_err = ReadInbox(0, aEncoder);
		break;
	case Outbox0::Id:
		chip_err = ReadOutbox(0, aEncoder);
		break;
	case Inbox1::Id:
		chip_err = ReadInbox(1, aEncoder);
		break;
	case Outbox1::Id:
		chip_err = ReadOutbox(1, aEncoder);
		break;
	case Inbox2::Id:
		chip_err = ReadInbox(2, aEncoder);
		break;
	case Outbox2::Id:
		chip_err = ReadOutbox(2, aEncoder);
		break;
	default:
		/* not readable, return nothing */
		break;
	}

	if (chip_err != CHIP_NO_ERROR) {
		adm_log(LOG_ERR "LC %s Attribute 0x%X failed: %s",
		    __func__, aPath.mAttributeId, ErrorStr(chip_err));
	}

	return chip_err;
}

CHIP_ERROR AylaLocalControlAccess::WriteMailboxInuse(uint8_t mailbox,
    AttributeValueDecoder &aDecoder)
{
	struct generic_session *gs = sessions[mailbox];
	bool in_use;
	CHIP_ERROR chip_err;
	chip::CharSpan span;

	chip_err = aDecoder.Decode(in_use);
	if (chip_err != CHIP_NO_ERROR) {
		chip_err = aDecoder.Decode(span);
		if (chip_err != CHIP_NO_ERROR) {
			adm_log(LOG_WARN "%s: mailbox%u Decode err %s",
			    __func__, mailbox, ErrorStr(chip_err));
			return chip_err;
		}
		if (span.size() == 4 && !memcmp(span.data(), "true", 4)) {
			in_use = true;
		} else {
			in_use = false;
		}
	}

	adm_log(LOG_DEBUG "write mailbox%u in_use %d", mailbox, in_use);

	/*
	 * Error if trying to set the mailbox to in use and it is already
	 * in use. This prevents two clients from thinking they successfully
	 * allocated the mailbox. If two clients attempt to allocate the
	 * mailbox at nearly the same time, the first one will win.
	 */
	if (in_use && gs) {
		adm_log(LOG_WARN "mailbox%u already in use", mailbox);
		return CHIP_ERROR_INCORRECT_STATE;
	}

	if (!in_use) {
		/*
		 * Close active session if there is one.
		 */
		if (gs) {
			lctrl_session_down(gs);
			sessions[mailbox] = NULL;
		}
		if (timer_active(&ka_timer[mailbox])) {
			adm_log(LOG_DEBUG "cancel mailbox%u keep alive timer",
			    mailbox);
			client_lock();
			client_timer_cancel(&ka_timer[mailbox]);
			client_unlock();
		}
		return CHIP_NO_ERROR;
	}

	gs = lctrl_session_alloc();
	if (!gs) {
		adm_log(LOG_ERR "mailbox%u alloc failed", mailbox);
		return CHIP_ERROR_WRITE_FAILED;
	}

	adm_log(LOG_DEBUG "write mailbox%u up", mailbox);

	sessions[mailbox] = gs;
	gs->ctxt = (void *)(u32)mailbox;
	gs->mtu_get = adm_local_control_mtu_get;
	gs->msg_tx = adm_mbox_svc_msg_tx;
	gs->close = adm_mbox_svc_session_close;
	gs->keep_alive = adm_mbox_keep_alive;
	timer_handler_init(&ka_timer[mailbox], adm_mbox_keep_alive_timeout);

	if (AE_OK != lc_mbox_init()) {
		adm_log(LOG_ERR "%s: lock_create failed", __func__);
		lctrl_session_down(gs);
		adm_mbox_svc_session_close(gs);
		return CHIP_ERROR_NO_MEMORY;
	}

	return CHIP_NO_ERROR;
}

CHIP_ERROR AylaLocalControlAccess::WriteInbox(uint8_t mailbox,
    AttributeValueDecoder &aDecoder)
{
	chip::CharSpan span;
	struct generic_session *gs = sessions[mailbox];
	CHIP_ERROR chip_err;
	enum ada_err a_err;

	adm_log(LOG_DEBUG2 "%s: write inbox%u", __func__, mailbox);

	chip_err = aDecoder.Decode(span);
	if (chip_err != CHIP_NO_ERROR) {
		adm_log(LOG_WARN "%s: mailbox%u Decode err %s",
		    __func__, mailbox, ErrorStr(chip_err));
		return chip_err;
	}

	log_dump(MOD_LOG_CLIENT, LOG_SEV_DEBUG, "in: ",
	    span.data(), span.size(), NULL);

	if (!gs) {
		adm_log(LOG_WARN "%s: mailbox%u not marked in use",
		    __func__, mailbox);
		return CHIP_ERROR_WRITE_FAILED;
	}

	a_err = lctrl_msg_rx(gs, (const u8 *)span.data(), (u16)span.size());
	if (a_err) {
		adm_log(LOG_ERR "%s: mailbox%u lctrl_msg_rx err %d",
		    __func__, mailbox, a_err);
		return CHIP_ERROR_WRITE_FAILED;
	}

	return CHIP_NO_ERROR;
}

CHIP_ERROR AylaLocalControlAccess::Write(const ConcreteDataAttributePath &aPath,
    AttributeValueDecoder &aDecoder)
{
	CHIP_ERROR chip_err = CHIP_NO_ERROR;

	VerifyOrDie(aPath.mClusterId == AylaLocalControl::Id);

	switch (aPath.mAttributeId) {
	case Mailbox0InUse::Id:
	        chip_err = WriteMailboxInuse(0, aDecoder);
		break;
	case Mailbox1InUse::Id:
	        chip_err = WriteMailboxInuse(1, aDecoder);
		break;
	case Mailbox2InUse::Id:
	        chip_err = WriteMailboxInuse(2, aDecoder);
		break;
	case Inbox0::Id:
		chip_err = WriteInbox(0, aDecoder);
		break;
	case Inbox1::Id:
		chip_err = WriteInbox(1, aDecoder);
		break;
	case Inbox2::Id:
		chip_err = WriteInbox(2, aDecoder);
		break;
	default:
		break;
	}

	if (chip_err != CHIP_NO_ERROR) {
		adm_log(LOG_ERR "LC %s  Attribute 0x%X failed: %s",
		    __func__, aPath.mAttributeId, ErrorStr(chip_err));
	}

	return chip_err;
}

} /* namespace */

void emberAfAylaLocalControlServerInitCallback(chip::EndpointId endpoint)
{
}

void emberAfPluginAylaLocalControlServerPostInitCallback(chip::EndpointId endpoint)
{
}

void MatterAylaLocalControlPluginServerInitCallback()
{
	registerAttributeAccessOverride(&gAylaLocalControlAttrAccess);
}

static int lc_read_outbox(int outbox, uint8_t *buffer, uint16_t maxReadLength)
{
	adm_log(LOG_DEBUG "%s: outbox%u", __func__, outbox);

	if (!buffer) {
		adm_log(LOG_ERR "%s: buffer NULL", __func__);
		return -1;
	}

	if (maxReadLength >= outboxes[outbox].len) {
		memcpy(buffer, outboxes[outbox].buf, outboxes[outbox].len);
		free(outboxes[outbox].buf);
		outboxes[outbox].buf = NULL;
		outboxes[outbox].len = 0;
		return 0;
	} else {
		adm_log(LOG_ERR "buffer not enough, maxReadLength %u,"
		    " outbox %d, len %u",
		    maxReadLength, outbox, outboxes[outbox].len);
		return -1;
	}
}

static enum ada_err lc_localcontrol_ext_attr_read_cb(
		const EmberAfAttributeMetadata * attributeMetadata,
		uint8_t * buffer, uint16_t maxReadLength)
{
	switch (attributeMetadata->attributeId) {
	case MailboxMask::Id:
		break;
	case Mailbox0InUse::Id:
		break;
	case Mailbox1InUse::Id:
		break;
	case Mailbox2InUse::Id:
		break;
	case Outbox0::Id:
		lc_read_outbox(0, buffer, maxReadLength);
		break;
	case Outbox1::Id:
		lc_read_outbox(1, buffer, maxReadLength);
		break;
	case Outbox2::Id:
		lc_read_outbox(2, buffer, maxReadLength);
		break;
	default:
		/* not readable, return nothing */
		adm_log(LOG_ERR "%s: aId 0x%04X not support",
		    __func__, attributeMetadata->attributeId);
		return AE_ERR;
		break;
	}
	return AE_OK;
}

Protocols::InteractionModel::Status emberAfExternalAttributeReadCallback(
		EndpointId endpoint, ClusterId clusterId,
		const EmberAfAttributeMetadata * attributeMetadata,
		uint8_t * buffer, uint16_t maxReadLength)
{
	enum ada_err ret;

	adm_log(LOG_DEBUG "%s: ep %u, cId 0x%04X, aId 0x%04X",
	    __func__, endpoint, clusterId, attributeMetadata->attributeId);

	if ((attributeMetadata == NULL) || (buffer == NULL)) {
		adm_log(LOG_ERR "%s: Invalid args,"
		     " attributeMetadata %p, buffer %p",
		     __func__, attributeMetadata, buffer);
		return Status::Failure;
	}

	if (endpoint == 0) {
		if (clusterId == AylaLocalControl::Id) {
			ret = lc_localcontrol_ext_attr_read_cb(
			    attributeMetadata, buffer, maxReadLength);
			if (ret == AE_OK) {
				return Status::Success;
			}
		}
	} else {
		if (adm_ext_attr_cb) {
			ret = adm_ext_attr_cb(endpoint, clusterId,
			    attributeMetadata->attributeId, ADM_ATTR_READ,
			    buffer, maxReadLength);
			if (ret == AE_OK) {
				return Status::Success;
			} else {
				adm_log(LOG_ERR "%s: read ep %u, cId %u"
				    ", aId %u ret %d",
				    __func__, endpoint, clusterId,
				    attributeMetadata->attributeId, ret);
				return Status::Failure;
			}
		}
	}

	adm_log(LOG_ERR "%s: ep %u, cId %u not support",
	    __func__, endpoint, clusterId);
	return Status::Failure;
}

static int lc_write_outbox(int outbox, uint8_t *buffer)
{
	int len;
	u8 *buf;

	adm_log(LOG_DEBUG "%s: outbox%u, len %u, buf %s",
	    __func__, outbox, buffer[0], (char *)&buffer[1]);

	if (!buffer) {
		adm_log(LOG_ERR "%s: buffer NULL", __func__);
		return -1;
	}

	len = buffer[0];
	buf = (u8 *)malloc(len + 1);
	if (!buf) {
		adm_log(LOG_ERR "%s: malloc %d failed",
		    __func__, len + 1);
		return -1;
	}

	/*
	* Drop previous message if it there is one.
	*/
	u8 *buf1 = outboxes[outbox].buf;
	if (buf1) {
		outboxes[outbox].buf = NULL;
		free(buf1);
	}

	memcpy(buf, &buffer[1], len);
	buf[len] = '\0';
	outboxes[outbox].len = len;
	outboxes[outbox].buf = buf;
	return 0;
}

static enum ada_err amd_localcontrol_ext_attr_write_cb(
		const EmberAfAttributeMetadata * attributeMetadata,
		uint8_t * buffer)
{
	switch (attributeMetadata->attributeId) {
	case MailboxMask::Id:
		break;
	case Mailbox0InUse::Id:
		break;
	case Mailbox1InUse::Id:
		break;
	case Mailbox2InUse::Id:
		break;
	case Outbox0::Id:
		lc_write_outbox(0, buffer);
		break;
	case Outbox1::Id:
		lc_write_outbox(1, buffer);
		break;
	case Outbox2::Id:
		lc_write_outbox(2, buffer);
		break;
	default:
		/* not readable, return nothing */
		adm_log(LOG_ERR "%s: aId 0x%04X not support",
		    __func__, attributeMetadata->attributeId);
		return AE_ERR;
		break;
	}
	return AE_OK;
}

Protocols::InteractionModel::Status emberAfExternalAttributeWriteCallback(
		EndpointId endpoint, ClusterId clusterId,
		const EmberAfAttributeMetadata * attributeMetadata,
		uint8_t * buffer)
{
	enum ada_err ret;
	uint8_t size = 0;

	adm_log(LOG_DEBUG "%s: ep %u, cId 0x%04X, aId 0x%04X",
	    __func__, endpoint, clusterId, attributeMetadata->attributeId);

	if ((attributeMetadata == NULL) || (buffer == NULL)) {
		adm_log(LOG_ERR "%s: Invalid args,"
		    " attributeMetadata %p, buffer %p",
		    __func__, attributeMetadata, buffer);
		return Status::Failure;
	}

	if (endpoint == 0) {
		if (clusterId == AylaLocalControl::Id) {
			ret = amd_localcontrol_ext_attr_write_cb(
			    attributeMetadata, buffer);
			if (ret == AE_OK) {
				return Status::Success;
			}
		}
	} else {
		if (adm_ext_attr_cb) {
			size = emberAfAttributeSize(attributeMetadata);
			ret = adm_ext_attr_cb(endpoint, clusterId,
			    attributeMetadata->attributeId, ADM_ATTR_WRITE,
			    buffer, size);
			if (ret == AE_OK) {
				return Status::Success;
			} else {
				adm_log(LOG_ERR "%s: read ep %u, cId %u"
				    ", aId %u ret %d",
				    __func__, endpoint, clusterId,
				    attributeMetadata->attributeId, ret);
				return Status::Failure;
			}
		}
	}

	adm_log(LOG_ERR "%s: ep %u, cId %u not support",
	    __func__, endpoint, clusterId);
	return Status::Failure;
}


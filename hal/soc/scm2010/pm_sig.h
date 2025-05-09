#ifndef _PM_SIG_H_
#define _PM_SIG_H_

#include <stdint.h>

enum pm_sig_msg_type {
	PM_SIG_MSG_PEER_INFO,
	PM_SIG_MSG_WAKEUP_TIME,
};

struct pm_sig_peer_info {
	uint32_t text_lma;
	uint32_t resume_addr;
};

struct pm_sig_msg {
	uint8_t type;

	union {
		struct pm_sig_peer_info peer_info;
		uint64_t wakeup_time;
	}item;
};

typedef void (*pm_sig_cb)(struct pm_sig_msg *msg);

int pm_sig_init(pm_sig_cb cb);

#endif //_PM_SIG_H_

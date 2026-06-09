#ifndef __SCM_MCUBOOT_AGENT_H__
#define __SCM_MCUBOOT_AGENT_H__

#include <stdint.h>

struct mcuboot_agent_params {
    void (*progress_cb)(uint8_t progress, void *arg);
    void *cb_arg;
    uint8_t auto_reboot;
};

int mcuboot_agent_run(const char *url, const struct mcuboot_agent_params *params);

#endif
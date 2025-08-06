/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <string.h>

#include <ayla/log.h>
#include <ada/libada.h>
#include <adm/adm_cli.h>

#include "app_common.h"
#include "app_int.h"

#include <wise_event_loop.h>
#include <wise_wifi_types.h>
#include <wise_wifi.h>
#include <wise_err.h>
#include <scm_wifi.h>

char oem[] = DEMO_OEM_ID;
char oem_model[] = DEMO_OEM_MODEL;

/*
 * Start ADA client.
 */
static int demo_client_start(void)
{
    struct ada_conf *cf = &ada_conf;
    static char hw_id[32];
    static u8 mac[6], *pmac;
    int rc;

    scm_wifi_get_wlan_mac(&pmac, WISE_IF_WIFI_STA);
    memcpy(mac, pmac, sizeof(mac));

    cf->mac_addr = mac;
    cf->hw_id = hw_id;
    cf->enable = 1;
    cf->get_all = 1;

    rc = ada_init();
    if (rc) {
        log_put(LOG_ERR "ADA init failed");
        return -1;
    }

    return 0;
}

void app_main()
{
    log_init();

    printf("\r\n\n%s\r\n", APP_NAME " " BUILD_STRING);

    ada_client_command_func_register(app_cmd_exec);
    AYLA_ASSERT(demo_client_start() == 0);
    demo_ota_init();
    demo_init();
    demo_idle();
}

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
#include <stdbool.h>

#include <ayla/log.h>
#include <ada/libada.h>
#include <ada/client.h>
#include <ada/sched.h>
#include <adm/adm_cli.h>

#include "app_common.h"
#include "app_int.h"

#include <wise_event_loop.h>
#include <wise_wifi_types.h>
#include <wise_wifi.h>
#include <wise_err.h>
#include <scm_wifi.h>

#ifdef CONFIG_AYLA_BULB
#include "ftm.h"
#include "power_cycle_reset.h"
#endif

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

	/*
	 * Load config for individual modules
	 */
	sched_conf_load();

    cf->enable = 1;
    cf->get_all = 1;

    rc = ada_init();
    if (rc) {
        log_put(LOG_ERR "ADA init failed");
        return -1;
    }

#ifdef AYLA_LOCAL_CONTROL_SUPPORT
	/*
	 * Enable local control access.
	 */
	rc = ada_client_lc_up();
	if (rc) {
		log_put(LOG_ERR "ADA local control up failed");
	}
#endif

	/*
	 * Start schedule activities.
	 */
	ada_sched_enable();

    return 0;
}
#include "iotalink.h"

#include "scm_flash.h"

void app_main()
{
    log_init();

    printf("\r\n\n%s\r\n", APP_NAME " " BUILD_STRING);
#ifdef CONFIG_AYLA_BULB
    check_power_cycle_count();
#endif
    ada_client_command_func_register(app_cmd_exec);
    AYLA_ASSERT(demo_client_start() == 0);
    demo_ota_init();
	demo_init();


	log_put(LOG_DEBUG "wlt app_main \n");
	
	extern light_ctrl_data_t sg_light_ctrl_data;
	scm_partition_read(FLASH_PARTITION_TMP, 0, &sg_light_ctrl_data, sizeof(sg_light_ctrl_data));

	printf("-------------------> switch_status %d\n  ",sg_light_ctrl_data.switch_status);
	if(sg_light_ctrl_data.switch_status ==0xFF)iotalink_light_ctrl_data_init();
		
	iotalink_control_timer_init();
	// wlt_init

	wlt_ws2812_test();//spi Çý¶¯
	
	iotalink_light_driver_init();

//	iotalink_button_init();

	iotalink_adc_init();
	
	iotalink_light_ctrl_process();

	iotalink_max30102_init();

	wlt_uart_init();

	
    
    demo_idle();
}

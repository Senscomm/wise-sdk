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
#include <scm_flash.h>

#define MS_TO_TICKS(ms) ((uint32_t)(((uint32_t)(ms) * osKernelGetTickFreq()) / (uint32_t)1000))

#define FLASH_SECTOR_SIZE 4096
#define POWER_CYCLE_THRESHOLD 5        /* Number of power cycles to trigger reset */
#define POWER_STABLE_TIME_MS 5000      /* Time to consider power stable */

#define FLASH_EMPTY_STATE     0xFF     /* Empty flash byte */
#define POWER_ON_STATE        0x55     /* Power on marked */
#define POWER_STABLE_STATE    0x00     /* Power stable for >5 seconds */

char oem[] = DEMO_OEM_ID;
char oem_model[] = DEMO_OEM_MODEL;

static osTimerId_t power_stable_timer = NULL;
static uint32_t power_on_flash_pos = 0;     /* Position of current power on marker */

/*
 * Timer callback when device has been powered for more than 5 seconds
 */
static void power_stable_callback(void *arg)
{
    /* Change the 0x55 to 0x00 to indicate stable power */
    uint8_t state = POWER_STABLE_STATE;

    scm_partition_write(FLASH_PARTITION_LOG, power_on_flash_pos, &state, sizeof(state));
    printf("Power stable - marking position %lu as stable\n", power_on_flash_pos);
}

/*
 * Find first empty position in flash sector
 */
static int find_first_empty_position(void)
{
    uint8_t state;
    for (uint32_t i = 0; i < FLASH_SECTOR_SIZE; i++) {
        if (scm_partition_read(FLASH_PARTITION_LOG, i, &state, sizeof(state)) == 0) {
            if (state == FLASH_EMPTY_STATE) {
                return (int)i;
            }
        }
    }
    
    /* Sector is full - need to erase */
    return -1;
}

/*
 * Count consecutive 0x55 bytes before given position
 */
static int count_consecutive_power_cycles(uint32_t pos)
{
    int count = 0;
    uint8_t state;
    /* Count backwards from pos-1 to find consecutive 0x55 values */
    for (int i = (int)pos - 1; i >= 0; i--) {
        scm_partition_read(FLASH_PARTITION_LOG, i, &state, sizeof(state));
        if (state == POWER_ON_STATE) {
            count++;
        } else {
            /* Stop counting when we hit something that's not 0x55 */
            break;
        }
    }
    
    return count;
}

/*
 * Erase the flash sector and reset
 */
static void erase_flash_sector_and_reset(void)
{
    scm_partition_erase(FLASH_PARTITION_LOG, 0, FLASH_SECTOR_SIZE);
    /* After erase, the first position will be FLASH_EMPTY_STATE (0xFF) */
    power_on_flash_pos = 0;
}

/*
 * New implementation of check_power_cycle_count using flash logging
 */
void check_power_cycle_count(void)
{
    int pos;
    int cycle_count;
    
    /* Step 1: Find first empty position in flash sector */
    pos = find_first_empty_position();
    
    /* If sector is full, erase it and start over */
    if (pos < 0) {
        erase_flash_sector_and_reset();
        pos = 0;
    }

    power_on_flash_pos = (uint32_t)pos;

    /* Count consecutive power cycles (consecutive 0x55 before current position) */
    cycle_count = count_consecutive_power_cycles((uint32_t)pos);
    printf("Found %d consecutive power cycles\n", cycle_count);

    /* Check if we've reached the threshold */
    if (cycle_count >= POWER_CYCLE_THRESHOLD) {
        printf("Power cycle threshold (%d) reached, triggering factory reset\n", 
               POWER_CYCLE_THRESHOLD);
        
        /* Clear the last 0x55 (write 0x00 to prevent false trigger on next boot) */
        if (pos > 0) {
            uint8_t state = POWER_STABLE_STATE;
            scm_partition_write(FLASH_PARTITION_LOG, pos - 1, &state, sizeof(state));
        }
        
        /* Trigger factory reset */
        ada_conf_reset(1);
        return;
    }
    
    /* Mark current power on event (write 0x55 to current position) */
    uint8_t state = POWER_ON_STATE;
    scm_partition_write(FLASH_PARTITION_LOG, pos, &state, sizeof(state));
    printf("Marked power on at position %lu\n", power_on_flash_pos);
    
    /* Start timer to mark stable power after 5 seconds */
    if (power_stable_timer == NULL) {
        power_stable_timer = osTimerNew(power_stable_callback, osTimerOnce, NULL, NULL);
    }
    if (osTimerStart(power_stable_timer, MS_TO_TICKS(POWER_STABLE_TIME_MS)) == osOK) {
        printf("Started power stability timer for %d ms\n", POWER_STABLE_TIME_MS);
    } else {
        printf("Failed to start power stability timer\n");
    }
}

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

void app_main()
{
    log_init();

    printf("\r\n\n%s\r\n", APP_NAME " " BUILD_STRING);

    check_power_cycle_count();
    ada_client_command_func_register(app_cmd_exec);
    AYLA_ASSERT(demo_client_start() == 0);
    demo_ota_init();
    demo_init();
    demo_idle();
}

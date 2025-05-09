#include <stdint.h>
#include <string.h>

#include "syscfg/syscfg.h"
#include "os/os_cputime.h"

#include "controller/ble_ll.h"
#include "controller/ble_ll_conn.h"
#include "controller/ble_ll_utils.h"

#include "hal/compiler.h"
#include "hal/rom.h"

#include <hal/console.h>

#ifdef CONFIG_LINK_TO_ROM

static const uint16_t path_ble_sca_ppm_tbl[8] = {
    500, 250, 150, 100, 75, 50, 30, 20
};

extern struct ble_ll_conn_sm g_ble_ll_conn_sm[MYNEWT_VAL(BLE_MAX_CONNECTIONS)];

uint32_t
patch_ble_ll_utils_calc_window_widening(uint32_t anchor_point,
                                        uint32_t last_anchor_point,
                                        uint8_t central_sca)
{
    struct ble_ll_conn_sm *connsm;
    uint32_t total_sca_ppm;
    uint32_t window_widening;
    int32_t time_since_last_anchor;
    uint32_t delta_msec;
	uint32_t max_ww = 0;
	int i;

	for (i = 0; i < MYNEWT_VAL(BLE_MAX_CONNECTIONS); i++) {
		connsm = &g_ble_ll_conn_sm[i];
		if (anchor_point == connsm->anchor_point &&
			last_anchor_point == connsm->last_anchor_point &&
			central_sca == connsm->central_sca) {
			max_ww = (connsm->conn_itvl * (1250/2)) - BLE_LL_IFS;
		}
	}


    window_widening = 0;

    time_since_last_anchor = (int32_t)(anchor_point - last_anchor_point);
    if (time_since_last_anchor > 0) {
        delta_msec = os_cputime_ticks_to_usecs(time_since_last_anchor) / 1000;
        total_sca_ppm = path_ble_sca_ppm_tbl[central_sca] + CONFIG_BLE_SCA;
        window_widening = (total_sca_ppm * delta_msec) / 1000;
    }

	if (max_ww && window_widening >= max_ww) {
		window_widening = max_ww - 1;
	}

    return window_widening;
}
PATCH(ble_ll_utils_calc_window_widening, &ble_ll_utils_calc_window_widening, &patch_ble_ll_utils_calc_window_widening);

#endif

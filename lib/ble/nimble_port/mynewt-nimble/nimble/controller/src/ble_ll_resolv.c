#include <stdint.h>

#include "syscfg/syscfg.h"
#include "os/os.h"

#include "controller/ble_ll.h"
#include "controller/ble_ll_resolv.h"

#include "hal/compiler.h"
#include "hal/rom.h"


struct ble_ll_resolv_data
{
    uint8_t addr_res_enabled;
    uint8_t rl_size;
    uint8_t rl_cnt_hw;
    uint8_t rl_cnt;
    ble_npl_time_t rpa_tmo;
    struct ble_npl_callout rpa_timer;
};
extern struct ble_ll_resolv_data g_ble_ll_resolv_data;

void
_ble_ll_resolv_deinit(void)
{
    ble_npl_callout_stop(&g_ble_ll_resolv_data.rpa_timer);
    ble_npl_callout_deinit(&g_ble_ll_resolv_data.rpa_timer);
}
#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_ll_resolv_deinit, &ble_ll_resolv_deinit, &_ble_ll_resolv_deinit);
#else
__func_tab__ void (*ble_ll_resolv_deinit)(void) = _ble_ll_resolv_deinit;
#endif

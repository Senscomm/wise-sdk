#include <stdint.h>

#include "syscfg/syscfg.h"
#include "os/os.h"

#include "controller/ble_ll.h"
#include "controller/ble_ll_scan.h"

#include "hal/compiler.h"
#include "hal/rom.h"


extern struct ble_ll_scan_sm g_ble_ll_scan_sm;

void
_ble_ll_scan_deinit(void)
{
    ble_npl_callout_deinit(&g_ble_ll_scan_sm.duration_timer);
    ble_npl_callout_deinit(&g_ble_ll_scan_sm.period_timer);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_ll_scan_deinit, &ble_ll_scan_deinit, &_ble_ll_scan_deinit);
#else
__func_tab__ void (*ble_ll_scan_deinit)(void) = _ble_ll_scan_deinit;
#endif

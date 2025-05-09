#include <stdint.h>

#include "syscfg/syscfg.h"
#include "os/os.h"

#include "controller/ble_ll.h"
#include "controller/ble_ll_sched.h"
#include "controller/ble_ll_tmr.h"

#include "hal/compiler.h"
#include "hal/rom.h"
#include "hal/console.h"

#ifdef CONFIG_LINK_TO_ROM
typedef int (* ble_ll_sched_preempt_cb_t)(struct ble_ll_sched_item *sch,
                                          struct ble_ll_sched_item *item);

extern int (*ble_ll_sched_insert)(struct ble_ll_sched_item *sch, uint32_t max_delay,
        ble_ll_sched_preempt_cb_t preempt_cb);
extern void (*ble_ll_sched_restart)(void);
extern int preempt_none(struct ble_ll_sched_item *sch, struct ble_ll_sched_item *item);

/* This patch disable adv random delay */
int
patch_ble_ll_sched_adv_reschedule(struct ble_ll_sched_item *sch,
                                  uint32_t max_delay_ticks)
{
    struct ble_ll_sched_item *next;
    uint32_t max_end_time;
    uint32_t rand_ticks;
    os_sr_t sr;
    int rc;

    max_end_time = sch->end_time + max_delay_ticks;

    OS_ENTER_CRITICAL(sr);

    /* Try to schedule as early as possible but no later than max allowed delay.
     * If succeeded, randomize start time to be within max allowed delay from
     * the original start time but make sure it ends before next scheduled item.
     */

    rc = ble_ll_sched_insert(sch, max_delay_ticks, preempt_none);
    if (rc == 0) {
        next = TAILQ_NEXT(sch, link);
        if (next) {
            if (LL_TMR_LT(next->start_time, max_end_time)) {
                max_end_time = next->start_time;
            }
            rand_ticks = max_end_time - sch->end_time;
        } else {
            rand_ticks = max_delay_ticks;
        }

        /* disable random delay for pm test */
        rand_ticks = 0;

        sch->start_time += rand_ticks;
        sch->end_time += rand_ticks;
    }

    OS_EXIT_CRITICAL(sr);

    ble_ll_sched_restart();

    return rc;
}
PATCH(ble_ll_sched_adv_reschedule, &ble_ll_sched_adv_reschedule, &patch_ble_ll_sched_adv_reschedule);
#endif

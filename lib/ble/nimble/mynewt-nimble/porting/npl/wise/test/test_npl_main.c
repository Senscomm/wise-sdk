
#include "test_util.h"

#include <os/os_cputime.h>

void ble_npl_eventq_init(struct ble_npl_eventq *);
void ble_npl_eventq_put(struct ble_npl_eventq *, struct ble_npl_event *);
struct ble_npl_event *ble_npl_eventq_get_no_wait(struct ble_npl_eventq *evq);
struct ble_npl_event *ble_npl_eventq_get(struct ble_npl_eventq *, ble_npl_time_t tmo);
void ble_npl_eventq_run(struct ble_npl_eventq *evq);
struct ble_npl_event *ble_npl_eventq_poll(struct ble_npl_eventq **, int, ble_npl_time_t);
void ble_npl_eventq_remove(struct ble_npl_eventq *, struct ble_npl_event *);
struct ble_npl_eventq *ble_npl_eventq_dflt_get(void);

void
ble_npl_eventq_run(struct ble_npl_eventq *evq)
{
    struct ble_npl_event *ev;

    ev = ble_npl_eventq_get(evq, BLE_NPL_TIME_FOREVER);
    ble_npl_event_run(ev);
}

static struct ble_npl_eventq dflt_evq;

struct ble_npl_eventq *
ble_npl_eventq_dflt_get(void)
{
    return &dflt_evq;
}

#define TEST_NPL_CALLOUT    1
#define TEST_NPL_SEM        2
#define TEST_NPL_EVENTQ     3
#define TEST_NPL_CRITICAL   4
#define TEST_NPL_TASK       5
#define TEST_NPL_CPUTIME    6

/* test cannot be built and run together, select only one */
#define TEST_NPL_OPTION     TEST_NPL_CRITICAL

/* simply include c source file here */
#if (TEST_NPL_OPTION == TEST_NPL_CALLOUT)
#include "test_npl_callout.c"
#endif
#if (TEST_NPL_OPTION == TEST_NPL_SEM)
#include "test_npl_sem.c"
#endif
#if (TEST_NPL_OPTION == TEST_NPL_EVENTQ)
#include "test_npl_eventq.c"
#endif
#if (TEST_NPL_OPTION == TEST_NPL_CRITICAL)
#include "test_npl_critical.c"
#endif
#if (TEST_NPL_OPTION == TEST_NPL_TASK)
#include "test_npl_task.c"
#endif
#if (TEST_NPL_OPTION == TEST_NPL_CPUTIME)
#include "test_npl_cputime.c"
#endif

static int do_npl_test(int argc, char *argv[])
{
#if (TEST_NPL_OPTION == TEST_NPL_CALLOUT)
    printf("running test_npl_callout_main\n");
    test_npl_callout_main();
#endif
#if (TEST_NPL_OPTION == TEST_NPL_SEM)
    printf("running test_npl_sem_main\n");
    test_npl_sem_main(0, NULL);
#endif
#if (TEST_NPL_OPTION == TEST_NPL_EVENTQ)
    printf("running test_npl_eventq_main\n");
    test_npl_eventq_main();
#endif
#if (TEST_NPL_OPTION == TEST_NPL_CRITICAL)
    printf("running test_npl_critical_main\n");
    test_npl_critical_main();
#endif
#if (TEST_NPL_OPTION == TEST_NPL_TASK)
    printf("running test_npl_task_main\n");
    test_npl_task_main();
#endif
#if (TEST_NPL_OPTION == TEST_NPL_CPUTIME)
    printf("running test_npl_cputime_main\n");
    test_npl_cputime_main();
#endif

    /* test npl code does not return */
    return 0;
}

CMD(npl, do_npl_test,
    "CLI comamands for NPL",
    "npl"
);


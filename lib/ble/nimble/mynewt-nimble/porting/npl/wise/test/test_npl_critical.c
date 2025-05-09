

static bool                   s_tests_running = true;
static struct ble_npl_task    s_task;
static struct ble_npl_eventq  s_eventq;
static struct ble_npl_event   s_event;

struct hal_timer              s_timer;

int test_task(void)
{
    uint32_t ctx;
    bool pass;

    ctx = ble_npl_hw_enter_critical();
    pass = ble_npl_hw_is_in_critical();
    ble_npl_hw_exit_critical(ctx);

    if (pass)
        return PASS;
    else
        return FAIL;
}

void test_timeout_cb(void *arg)
{
    uint32_t ctx;
    bool pass;

    ctx = ble_npl_hw_enter_critical();
    pass = ble_npl_hw_is_in_critical();
    ble_npl_hw_exit_critical(ctx);

    VerifyOrQuit(pass != 0, "critical: condition not true");

    ble_npl_eventq_put(&s_eventq, &s_event);
}

void test_event_cb(struct ble_npl_event *ev)
{
    s_tests_running = false;
}

int test_isr(void)
{
    uint32_t t;

    hal_timer_init(0, NULL);
    os_cputime_init(20000000);

    t = os_cputime_get32();
    os_cputime_timer_init(&s_timer, test_timeout_cb, NULL);
    os_cputime_timer_start(&s_timer, t + (20 * 1000000));

    ble_npl_event_init(&s_event, test_event_cb, NULL);

    return PASS;
}

void *test_task_run(void *args)
{
    SuccessOrQuit(test_task(),   "critical from task failed");
    SuccessOrQuit(test_isr(), "critical from isr failed");

    while (s_tests_running)
    {
        ble_npl_eventq_run(&s_eventq);
    }

    printf("All tests passed\n");
    exit(PASS);

    return NULL;
}

int test_npl_critical_main(void)
{
    ble_npl_eventq_init(&s_eventq);

    SuccessOrQuit(ble_npl_task_init(&s_task, "s_task", test_task_run,
			       NULL, 1, 0, NULL, 0),
		  "task: error initializing");

    while (1) {}
}

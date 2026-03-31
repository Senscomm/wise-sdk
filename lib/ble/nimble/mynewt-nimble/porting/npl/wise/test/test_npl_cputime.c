

static bool                   s_tests_running = true;
static struct ble_npl_task    s_task;
static struct ble_npl_eventq  s_eventq;
static struct ble_npl_event   s_event;

static struct hal_timer       s_timer0;
static struct hal_timer       s_timer1;
static struct hal_timer       s_timer2;
static struct hal_timer       s_timer3;

static bool                   s_timeout0 = false;
static bool                   s_timeout1 = false;
static bool                   s_timeout2 = false;
static bool                   s_timeout3 = false;

static uint32_t               s_arg0 = 0xF0;
static uint32_t               s_arg1 = 0xF1;
static uint32_t               s_arg2 = 0xF2;
static uint32_t               s_arg3 = 0xF3;


void test_timeout_cb(void *arg)
{
    uint32_t value;

    value = *((uint32_t *)arg);
    
    printk("T %02x\n", value);

    if (value == s_arg0) {
        s_timeout0 = true;
    } else if (value == s_arg1) {
        s_timeout1 = true;
        os_cputime_timer_stop(&s_timer2);
    } else if (value == s_arg2) {
        s_timeout2 = true;
    } else if (value == s_arg3) {
        s_timeout3 = true;
        ble_npl_eventq_put(&s_eventq, &s_event);
    }
}

void test_event_cb(struct ble_npl_event *ev)
{
    s_tests_running = false;
}

void *test_task_run(void *args)
{
    uint32_t t;

    hal_timer_init(0, NULL);
    os_cputime_init(20000000);

    os_cputime_timer_init(&s_timer0, test_timeout_cb, &s_arg0);
    os_cputime_timer_init(&s_timer1, test_timeout_cb, &s_arg1);
    os_cputime_timer_init(&s_timer2, test_timeout_cb, &s_arg2);
    os_cputime_timer_init(&s_timer3, test_timeout_cb, &s_arg3);

    t = os_cputime_get32();
    
    os_cputime_timer_start(&s_timer0, t + (20 * 1000000 * 1));
    os_cputime_timer_start(&s_timer1, t + (20 * 1000000 * 2));
    os_cputime_timer_start(&s_timer2, t + (20 * 1000000 * 3));
    os_cputime_timer_start(&s_timer3, t + (20 * 1000000 * 4));

    while (s_tests_running)
    {
        ble_npl_eventq_run(&s_eventq);
    }

    if ((s_timeout0 == true) &&
        (s_timeout1 == true) &&
        (s_timeout2 == false) &&
        (s_timeout3 == true)) {

        printf("All tests passed\n");
    } else {
        printf("error: invalid timers timedout\n");
    }

    exit(PASS);

    return NULL;
}

int test_npl_cputime_main(void)
{
    ble_npl_eventq_init(&s_eventq);
    ble_npl_event_init(&s_event, test_event_cb, NULL);

    SuccessOrQuit(ble_npl_task_init(&s_task, "s_task", test_task_run,
			       NULL, 1, 0, NULL, 0),
		  "task: error initializing");

    while (1) {}
}

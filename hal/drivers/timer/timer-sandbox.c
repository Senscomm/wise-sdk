/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <hal/kernel.h>
#include <hal/timer.h>
#include <hal/console.h>

#define __USE_NATIVE_HEADER__
#include <sys/time.h>

static struct timeval start;

static int sandbox_timer_probe(struct device *timer)
{
	printk("%s: clk=1MHz\n", dev_name(timer));

	return 0;
}

static int sandbox_timer_setup(struct device *timer,
		u32 config, u32 param, timer_isr isr)
{
	return 0;
}

static int sandbox_timer_start(struct device *timer)
{
	gettimeofday(&start, NULL);

	return 0;
}

static int sandbox_timer_stop(struct device *timer)
{
	return 0;
}

static unsigned long sandbox_timer_get_rate(struct device *timer)
{
	return 1000000;
}

static u32 sandbox_timer_get_value(struct device *timer)
{
	struct timeval now, tv;

	gettimeofday(&now, NULL);
	timersub(&now, &start, &tv);

	return tv.tv_sec * 1000000 + tv.tv_usec;

}

struct timer_ops sandbox_timer_ops = {
	.setup = sandbox_timer_setup,
	.start = sandbox_timer_start,
	.stop = sandbox_timer_stop,
	.get_rate = sandbox_timer_get_rate,
	.get_value = sandbox_timer_get_value,
};

static declare_driver(timer) = {
	.name = "timer",
	.probe = sandbox_timer_probe,
	.ops = &sandbox_timer_ops,
};

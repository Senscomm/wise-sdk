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
#include <stdio.h>
#include <string.h>
#include <soc.h>
#include <hal/bitops.h>
#include <hal/kernel.h>
#include <hal/console.h>
#include <hal/rtc.h>
#include <cmsis_os.h>

/* definition of working in SCM1010, But in scm2010, it does not work */
/* In scm1010 platform, 32khz resolution is worked normally as modification of rtc related register */
/* If our HW is modified like SCM1010, do enable and verify */
//#define CONFIG_ATCRTC_SCM1010		//[!!important]

#define ATCRTC_DBG		0

/* atcrtc rtc registers */
#define OFT_RTC_32K		0x04
#define OFT_RTC_CNT		0x10
#define OFT_RTC_ALM		0x14
#define OFT_RTC_CTR		0x18
#define OFT_RTC_STS		0x1c

#define RTC_CFG			0x218 /* RTC_CFG */

/* busy loop to ensure internal RTC registers are synchoronized */
#ifdef CONFIG_ATCRTC_SCM1010
#define SYNC(dev) 	do {					\
	if (readl(dev->base[0] + OFT_RTC_STS) & (0x1 << 14))	\
	break;						\
} while (1);
#elif CONFIG_ATCRTC_SCM2010
#define SYNC(dev) 	do {					\
	if (readl(dev->base[0] + OFT_RTC_STS) & (0x1 << 16))	\
	break;						\
} while (1);
#endif

#define rtc_read(o)		({ uint32_t __val; __val = readl(dev->base[0] + o); __val; })

#ifdef CONFIG_ATCRTC_SCM1010
#define rtc_write(v, o)    do { 		\
	writel(v, dev->base[0] + o); 	    \
	SYNC(dev);			                \
} while (0)
#elif CONFIG_ATCRTC_SCM2010
#define rtc_write(v, o)    do { 		\
	writel(v, dev->base[0] + o); 	    \
} while (0)
#endif

static int atcrtc_rtc_get(struct device *dev, struct rtc_time *time)
{
	uint32_t cnt;

	if (!time)
		return -1;

	cnt = rtc_read(OFT_RTC_CNT);
	memset(time, 0, sizeof(*time));
	time->tm_sec = cnt & 0x3F;
	time->tm_min = (cnt >> 6) & 0x3F;
	time->tm_hour = (cnt >> 12) & 0x1F;
	time->tm_mday = (cnt >> 17) & 0x1F;

	return 0;
}

static int atcrtc_rtc_set(struct device *dev, const struct rtc_time *time)
{
	uint32_t cnt = 0;

	if (!time)
		return -1;

#ifdef CONFIG_ATCRTC_SCM2010
	rtc_write(rtc_read(OFT_RTC_CTR) & ~0x1, OFT_RTC_CTR);
#endif

	cnt |= time->tm_sec & 0x3F;
	cnt |= ((time->tm_min & 0x3F) << 6);
	cnt |= ((time->tm_hour & 0x1F) << 12);
	cnt |= ((time->tm_mday & 0x1F) << 17);

#ifdef CONFIG_ATCRTC_SCM1010
	/* For 32khz resolution */
	cnt <<= 15;
	rtc_write(cnt, OFT_RTC_32K);
#elif CONFIG_ATCRTC_SCM2010
	rtc_write(cnt, OFT_RTC_CNT);
	rtc_write(0, OFT_RTC_32K);
	SYNC(dev);
#endif

#ifdef CONFIG_ATCRTC_SCM2010
	rtc_write(rtc_read(OFT_RTC_CTR) | 0x1, OFT_RTC_CTR);
#endif

	return 0;
}

static int atcrtc_rtc_reset(struct device *dev)
{
	struct rtc_time time;

	memset(&time, 0, sizeof(time));
	time.tm_mday = 1;
	atcrtc_rtc_set(dev, &time);

	return 0;
}

static int atcrtc_rtc_get_32khz_count(struct device *dev, uint32_t *count)
{
	struct rtc_time time __maybe_unused;

#ifdef CONFIG_ATCRTC_SCM1010
	uint32_t cnt = 0;

	memset(&time, 0, sizeof(time));

	/* XXX HW bug - when bit[14-0] becomes 0, carry over into bit[20-15] doesn't occur at the same time,
	 * XXX but at the next clock which is about 30 usec later */
	while (((cnt = rtc_read(OFT_RTC_32K)) & 0x7FFF) == 0);
	time.tm_sec = (cnt >> 15) & 0x3F;
	time.tm_min = (cnt >> 21) & 0x3F;
	time.tm_hour = (cnt >> 27) & 0x1F;
	cnt &= 0x7FFF;
	cnt += time.tm_sec * _32khz;
	cnt += time.tm_min * 60 * _32khz;
	cnt += time.tm_hour * 60 * 60 * _32khz;

	*count = cnt;
#elif CONFIG_ATCRTC_SCM2010
	uint16_t cnt_32k;

	/* To work-around hardware issue of "32K part"
	 * not synchronized with "second part"
	 */

	while (((cnt_32k = rtc_read(OFT_RTC_32K)) & 0x7FFF) == 0);

	atcrtc_rtc_get(dev, &time);

	*count = cnt_32k;
	*count += time.tm_sec * _32khz;
	*count += time.tm_min * 60 * _32khz;
	*count += time.tm_hour * 60 * 60 * _32khz;
#endif

	return 0;
}


static int atcrtc_rtc_set_32khz_alarm(struct device *dev, uint32_t count)
{
#ifdef CONFIG_ATCRTC_SCM1010
	struct rtc_time time;

	memset(&time, 0, sizeof(time));
	time.tm_sec = count / _32khz;
	count = count % _32khz;
	time.tm_min = time.tm_sec / 60;
	time.tm_sec = time.tm_sec % 60;
	time.tm_hour = time.tm_min / 60;
	time.tm_min = time.tm_min % 60;
	time.tm_hour = time.tm_hour % 24;

	count |= time.tm_sec << 15;
	count |= time.tm_min << 21;
	count |= time.tm_hour << 27;

	rtc_write(count, OFT_RTC_ALM);

	rtc_write(rtc_read(OFT_RTC_CTR) | 0x2, OFT_RTC_CTR);
#elif CONFIG_ATCRTC_SCM2010
	uint32_t expiry;
	uint32_t sec;
	uint32_t min;
	uint32_t hour;

	/* To work-around hardware issue of "32K part"
	 * not synchronized with "second part"
	 */

	if ((count & 0x00007fff) == 0) {
		count -= 1;
	}

	sec = count / _32khz;
	min = sec / 60;
	sec = sec % 60;
	hour = min / 60;
	min = min % 60;
	hour = hour % 24;

	expiry = count % _32khz;
	expiry |= sec << 15;
	expiry |= min << 21;
	expiry |= hour << 27;

	SYNC(dev);
	rtc_write(expiry, OFT_RTC_ALM);
	rtc_write(rtc_read(OFT_RTC_CTR) | (1 << 2), OFT_RTC_CTR);

	/* sync again here
	 * as this may be just before going into the deep sleep mode
	 */

	SYNC(dev);
#endif
	return 0;
}


__iram__ int atcrtc_rtc_clear_32khz_alarm(struct device *dev)
{
#ifdef CONFIG_ATCRTC_SCM1010
	rtc_write(0x4, OFT_RTC_STS);
	rtc_write(rtc_read(OFT_RTC_CTR) & ~(1 << 2), OFT_RTC_CTR);
#elif CONFIG_ATCRTC_SCM2010
	SYNC(dev);
	rtc_write(0x4, OFT_RTC_STS);
	rtc_write(rtc_read(OFT_RTC_CTR) & ~(1 << 2), OFT_RTC_CTR);
#endif

	return 0;
}


#if (ATCRTC_DBG)
const char *intstr[8] = {
	[0 ... 1]	= "",
	[2]		= "Alarm",
	[3]		= "Day",
	[4]		= "Hour",
	[5]		= "Min",
	[6]		= "Sec",
	[7]		= "Hsec",
};
#endif

#ifdef CONFIG_SUPPORT_RTC_ALARM_INT
static int atcrtc_irq(int irq, void *data)
{
	struct device *dev = data;
	uint32_t intst = rtc_read(OFT_RTC_STS);
	uint32_t val;

#if (ATCRTC_DBG)
	int i;

	for (i = 0; i < 8; i++)
		if (intst & BIT(i))
			printk("[%s,", intstr[i]);
	printk("]\n");
#endif

	/* clear interrupt status */

#ifdef CONFIG_ATCRTC_SCM1010
	rtc_write(intst, OFT_RTC_STS);
#elif CONFIG_ATCRTC_SCM2010
	rtc_write(intst, OFT_RTC_STS);
#endif

	/* disable alarm interrupt as setting alarm will enable it again */

	val = rtc_read(OFT_RTC_CTR) & ~(1 << 2);
	rtc_write(val, OFT_RTC_CTR);

	return 0;
}
#endif

static int atcrtc_rtc_probe(struct device *dev)
{
	struct rtc_time time;
	uint32_t v;

	writel((1 << 0), SMU(RTC_CFG));

	v = readl(SMU(RTC_CFG));
	v |= (1 << 4);
	writel(v, SMU(RTC_CFG));

	rtc_write(0xffffffff, OFT_RTC_ALM);

#if 0
#ifdef CONFIG_ATCRTC_SCM1010
	rtc_write(rtc_read(OFT_RTC_CTR) | 0x1, OFT_RTC_CTR);
#elif CONFIG_ATCRTC_SCM2010
	SYNC(dev);
	rtc_write(rtc_read(OFT_RTC_CTR) | 0x1, OFT_RTC_CTR);
#endif
#endif

	/* enable alarm wakeup signal */
	rtc_write(rtc_read(OFT_RTC_CTR) | 1 << 1, OFT_RTC_CTR);

	/* initialize the rtc time as 0 */
	memset(&time, 0, sizeof(time));
	rtc_set(dev, &time);

#ifdef CONFIG_SUPPORT_RTC_ALARM_INT
	int ret = 0;
	ret = request_irq(dev->irq[0], atcrtc_irq, dev_name(dev), dev->pri[0], dev);

	if (ret) {
		printk("%s irq req is failed(%d)", __func__ , ret);

#ifdef CONFIG_ATCRTC_SCM1010
		rtc_write(0, OFT_RTC_CTR);
#elif CONFIG_ATCRTC_SCM2010
		SYNC(dev);
		rtc_write(0, OFT_RTC_CTR);
#endif
		return -1;
	}
#endif

	return 0;
}

struct rtc_ops atcrtc_rtc_ops = {
	.get = atcrtc_rtc_get,
	.set = atcrtc_rtc_set,
	.reset = atcrtc_rtc_reset,
	.get_32khz_count = atcrtc_rtc_get_32khz_count,
	.set_32khz_alarm = atcrtc_rtc_set_32khz_alarm,
	.clear_32khz_alarm = atcrtc_rtc_clear_32khz_alarm
};

static declare_driver(rtc) = {
	.name = "atcrtc",
	.probe = atcrtc_rtc_probe,
	.ops = &atcrtc_rtc_ops,
};

#ifdef CONFIG_CMD_RTC
/**
 * RTC CLI commands
 */
#include <cli.h>

#include <stdio.h>
#include <stdlib.h>

static int do_rtc_get_time(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("atcrtc");
	struct rtc_time time;

	if (!dev) {
		return CMD_RET_FAILURE;
	}

	rtc_get(dev, &time);

	printf("day - %2d %2d:%2d:%2d\n",time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);

	return CMD_RET_SUCCESS;
}

static int do_rtc_set_time(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("atcrtc");
	struct rtc_time time;

	if (!dev) {
		return CMD_RET_FAILURE;
	}

	if (argc != 5) {
		return CMD_RET_USAGE;
	}

	time.tm_mday = strtoul(argv[1], NULL, 0);
	time.tm_hour = strtoul(argv[2], NULL, 0);
	time.tm_min = strtoul(argv[3], NULL, 0);
	time.tm_sec = strtoul(argv[4], NULL, 0);

	rtc_set(dev, &time);

	return CMD_RET_SUCCESS;
}

static int do_rtc_reset(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("atcrtc");

	if (!dev) {
		return CMD_RET_FAILURE;
	}

	rtc_reset(dev);

	return CMD_RET_SUCCESS;
}

static int do_rtc_get_32k_count(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("atcrtc");
	uint32_t count;

	if (!dev) {
		return CMD_RET_FAILURE;
	}

	rtc_get_32khz_count(dev, &count);

	printf("current RTC count : 0x%08x\n", count);

	return CMD_RET_SUCCESS;
}

static int do_rtc_set_alram_32k(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("atcrtc");
	uint32_t count;
	uint32_t alarm;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	alarm = atoi(argv[1]);


	rtc_get_32khz_count(dev, &count);
	rtc_set_32khz_alarm(dev, count + alarm);

	printf("current 0x%08x - after 0x%08x alarm\n", count, alarm);

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd rtc_cmd[] = {
	CMDENTRY(get, do_rtc_get_time, "", ""),
	CMDENTRY(set, do_rtc_set_time, "", ""),
	CMDENTRY(reset, do_rtc_reset, "", ""),
	CMDENTRY(get32k, do_rtc_get_32k_count, "", ""),
	CMDENTRY(alarm32k, do_rtc_set_alram_32k, "", ""),
};

static int do_rtc(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], rtc_cmd, ARRAY_SIZE(rtc_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(rtc, do_rtc,
		"test routines for RTC (Real Time Clock)",
		"rtc get" OR
		"rtc set <day> <hour> <min> <sec>" OR
		"rtc reset" OR
		"rtc get32k" OR
		"rtc alarm32k <count>"
   );

#endif

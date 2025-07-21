/*
 * Copyright 2018-2019 Senscomm Semiconductor Co., Ltd.	All rights reserved.
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
#include <stdlib.h>

#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/tlv.h>
#include <ayla/conf.h>
#include <ayla/log.h>
#include <ayla/clock.h>
#include <ada/ada_conf.h>
#include <ada/client.h>
#include <ada/metrics.h>
#include <adw/wifi.h>
#include <adb/adb.h>
#include <adb/al_bt.h>
#include <sys/time.h>

#include <cmsis_os.h>

#define AYLA_DEMO_STACK_SIZE		1024 * 4

void ayla_demo_task(void *arg)
{
	extern void app_main(void);

	app_main();
}

void ayla_demo_run(void)
{
	static int g_demo_start;
	osThreadAttr_t attr = {
		.name 		= "ayla-main",
		.stack_size = 1024 * 4,
		.priority 	= osPriorityLow,
	};


	if (g_demo_start) {
		printf("AYLA demo already started");
		return;
	}

	g_demo_start = 1;

	/* run the demo in a new thread to allow further CLI */
	if (osThreadNew(ayla_demo_task, NULL, &attr) == NULL) {
		printf("Ayla application start failed\n");
	}
}

#ifndef AYLA_MATTER_SUPPORT

int main(void)
{
	printf("AYLA Demo!\n");

#if 1 /* automatically run ayla demo */
	ayla_demo_run();
#endif

	return 0;
}

#else

/* ayla_demo_run() will be the main entry point from
 * the Matter application.
 */

#endif

#ifdef CONFIG_CMDLINE

#include "cli.h"

extern int run_ayla_cmd(int argc, char *argv[]);
static int do_ayla(int argc, char *argv[])
{
	argc--;
	argv++;

	return run_ayla_cmd(argc, argv);
}

CMD(ayla, do_ayla,
		"ayla cli",
		"ayla [command ...]"
);

#endif


/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <ctype.h>
#include <stdlib.h>
#include <cmsis_os.h>
#include <scm_log.h>
#include "hal/kernel.h"
#include "cli.h"
#include "scm_cli.h"
#include "scm_gpio.h"

#define DEMO_LEDCTRL_TAG 	"DEMO_LEDCTRL"

#define BLINK_COUNT 		15
#define BLINK_INTERVAL 		300

#define GPIO_23 			23
#define GPIO_24 			24
#define GPIO_15 			15
#define GPIO_RGB 			16

enum gpio_polarity {
	SCM_GPIO_HIGH_TO_LOW	= 0,
	SCM_GPIO_LOW_TO_HIGH 	= 1,
};

volatile bool blink_flag = false;

static void blink_led(int led_gpio)
{
	unsigned int cnt = 0;

	while (blink_flag && (cnt < BLINK_COUNT)) {
		scm_gpio_write(led_gpio, SCM_GPIO_LOW_TO_HIGH);
		osDelay(pdMS_TO_TICKS(BLINK_INTERVAL));
		scm_gpio_write(led_gpio, SCM_GPIO_HIGH_TO_LOW);
		osDelay(pdMS_TO_TICKS(BLINK_INTERVAL));
		SCM_INFO_LOG(DEMO_LEDCTRL_TAG, "cnt = %u\n", cnt);
		cnt++;
	}
}

static void gpio_toggle_blinking(uint8_t led_gpio, bool start)
{
	if (start) {
		blink_flag = true;
		blink_led(led_gpio);
	} else {
		blink_flag = false;
	}
}

void ledc_blink(void)
{
	/* configure GPIOs as outputs */
	scm_gpio_configure(GPIO_15, SCM_GPIO_PROP_OUTPUT);
	scm_gpio_configure(GPIO_23, SCM_GPIO_PROP_OUTPUT);
	scm_gpio_configure(GPIO_24, SCM_GPIO_PROP_OUTPUT);
	scm_gpio_configure(GPIO_RGB, SCM_GPIO_PROP_OUTPUT);

	/* start toggling */
	gpio_toggle_blinking(GPIO_RGB, true);

	/* set high */
    scm_gpio_write(GPIO_RGB, SCM_GPIO_HIGH_TO_LOW);
}

int main(void)
{
	printf("LEDC Blink demo\n");

	ledc_blink();

	return 0;
}

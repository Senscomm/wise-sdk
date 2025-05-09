#include <stdio.h>
#include <stdlib.h>
#include <scm_log.h>
#include "hal/kernel.h"
#include "hal/console.h"
#include "cli.h"
#include "scm_timer.h"
#include "scm_gpio.h"
#include "wise_err.h"

#define LED_TIMER_IDX 	SCM_TIMER_IDX_0	/* Assume using timer index 0 */
#define LED_TIMER_CH 	SCM_TIMER_CH_0	/* Assume using timer channel 0 */

/*
 * LED mapping to a GPIO pin can be specified by the board configuration
 * For example, the default on SCM2010 EVB, TIMER0-CHANNEL0 is mapped to GPIO 15
 */

void ledc_pwm(void)
{
	struct scm_timer_cfg cfg;

	/* you can further adjust PWM parameters to change LED brightness.
	 * for example, by modifying the high and low values through the corresponding function calls
	 */

	/* Configure as PWM mode */
	cfg.mode = SCM_TIMER_MODE_PWM;
	cfg.intr_en = 0;  			/* Disable interrupts */
	cfg.data.pwm.high = 1000;	/* Duration of high level (unit: microseconds) */
	cfg.data.pwm.low = 1000;	/* Duration of low level (unit: microseconds) */
	cfg.data.pwm.park = 0;		/* Park value, set to 0 */

	/* Call the TIMER driver configuration function */
	int ret = scm_timer_configure(LED_TIMER_IDX, LED_TIMER_CH, &cfg, NULL, NULL);
	if (ret) {
		printf("TIMER PWM configure error = %x\n", ret);
	} else {
		/* Start the TIMER */
		scm_timer_start(LED_TIMER_IDX, LED_TIMER_CH);
	}
}

int main(void)
{
	printf("LEDC PWM demo\n");

	ledc_pwm();

	return 0;
}

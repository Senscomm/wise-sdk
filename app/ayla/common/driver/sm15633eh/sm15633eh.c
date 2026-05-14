#include "wise_system.h"
#include <kernel.h>

#include "hal/kernel.h"
#include "hal/console.h"
#include "cli/cli.h"
#include "sm15633eh.h"

struct PWM_CHANNEL_STATUS
{
    bool rchannel;
    bool gchannel;
    bool bchannel;
    bool whitechannel;
    bool warmchannel;
};

static struct PWM_CHANNEL_STATUS pwm_channel_status_info ={0};

static led_present_info_t led_present_info={200,200,200,200,200};


int sm15633eh_pwm_init(uint8_t colour_select,uint8_t Rcolour,uint8_t Gcolour,uint8_t Bcolour,uint8_t Whitecolour,uint8_t Warmcolour)
{
    struct scm_timer_cfg pwm_cfg;
    uint16_t pwm_value_high;
    uint16_t pwm_value_low;
    uint16_t period;
    
     period = 1000000/PWM_FREQUENCY;
     
    switch(colour_select)
        {
        case LED_R_CHANNEL:
            if(Rcolour==0)
                {
                    Rcolour =1;// set 0 will cause pwm error
                }
            else if(Rcolour>254)
                {
                Rcolour = 254;// 255 will cause pwm error
                }
            led_present_info.rcolour = Rcolour;
            pwm_value_high = (period*Rcolour)/255;
            pwm_value_low = period-pwm_value_high;
            pwm_cfg.mode = SCM_TIMER_MODE_PWM;
            pwm_cfg.intr_en = 0;

            /* Setting 38Khz */
            pwm_cfg.data.pwm.high = pwm_value_high;
            pwm_cfg.data.pwm.low = pwm_value_low;
            pwm_cfg.data.pwm.park = 0;

            scm_timer_configure(PWM0_CHANNEL_ID, RED_PWM_CH, &pwm_cfg, NULL, NULL);
//            printf("%s,R pwm_highg=%d,pwm_low=%d\r\n",__FUNCTION__,pwm_value_high,pwm_value_low);
            break;

        case LED_G_CHANNEL:
            if(Gcolour==0)
                {
                    Gcolour =1;// set 0 will cause pwm error
                }
            else if(Gcolour>254)
                {
                Gcolour = 254;// 255 will cause pwm error
                }
            led_present_info.gcolour = Gcolour;
            pwm_value_high = (period*Gcolour)/255;
            pwm_value_low = period-pwm_value_high;
            pwm_cfg.mode = SCM_TIMER_MODE_PWM;
            pwm_cfg.intr_en = 0;

            /* Setting 38Khz */
            pwm_cfg.data.pwm.high = pwm_value_high;
            pwm_cfg.data.pwm.low = pwm_value_low;
            pwm_cfg.data.pwm.park = 0;

            scm_timer_configure(PWM0_CHANNEL_ID, GREEN_PWM_CH, &pwm_cfg, NULL, NULL);
//            printf("%s,G pwm_highg=%d,pwm_low=%d\r\n",__FUNCTION__,pwm_value_high,pwm_value_low);
            break;

        case LED_B_CHANNEL:
            if(Bcolour==0)
                {
                    Bcolour =1;// set 0 will cause pwm error
                }
            else if(Bcolour>254)
                {
                Bcolour = 254;// 255 will cause pwm error
                }
            led_present_info.bcolour = Bcolour;
            pwm_value_high = (period*Bcolour)/255;
            pwm_value_low = period-pwm_value_high;
            pwm_cfg.mode = SCM_TIMER_MODE_PWM;
            pwm_cfg.intr_en = 0;

            /* Setting 38Khz */
            pwm_cfg.data.pwm.high = pwm_value_high;
            pwm_cfg.data.pwm.low = pwm_value_low;
            pwm_cfg.data.pwm.park = 0;

            scm_timer_configure(PWM0_CHANNEL_ID, BLUE_PWM_CH, &pwm_cfg, NULL, NULL);
//            printf("%s,B pwm_highg=%d,pwm_low=%d\r\n",__FUNCTION__,pwm_value_high,pwm_value_low);
            break;

        case LED_WHITE_CHANNEL:
            if(Whitecolour==0)
                {
                    Whitecolour =1;// set 0 will cause pwm error
                }
            else if(Whitecolour>254)
                {
                Whitecolour = 254;// 255 will cause pwm error
                }
            led_present_info.whitecolour = Whitecolour;
            pwm_value_high = (period*Whitecolour)/255;
            pwm_value_low = period-pwm_value_high;
            pwm_cfg.mode = SCM_TIMER_MODE_PWM;
            pwm_cfg.intr_en = 0;

            /* Setting 38Khz */
            pwm_cfg.data.pwm.high = pwm_value_high;
            pwm_cfg.data.pwm.low = pwm_value_low;
            pwm_cfg.data.pwm.park = 0;

            scm_timer_configure(PWM1_CHANNEL_ID, WHITE_PWM_CH, &pwm_cfg, NULL, NULL);
//            printf("%s,white pwm_highg=%d,pwm_low=%d\r\n",__FUNCTION__,pwm_value_high,pwm_value_low);
            break;

        case LED_WARM_CHANNEL:
            if(Warmcolour==0)
                {
                    Warmcolour =1;// set 0 will cause pwm error
                }
            else if(Warmcolour>254)
                {
                Warmcolour = 254;// 255 will cause pwm error
                }
            led_present_info.warmcolour = Warmcolour;
            pwm_value_high = (period*Warmcolour)/255;
            pwm_value_low = period-pwm_value_high;
            pwm_cfg.mode = SCM_TIMER_MODE_PWM;
            pwm_cfg.intr_en = 0;

            /* Setting 38Khz */
            pwm_cfg.data.pwm.high = pwm_value_high;
            pwm_cfg.data.pwm.low = pwm_value_low;
            pwm_cfg.data.pwm.park = 0;

            scm_timer_configure(PWM0_CHANNEL_ID, WARM_PWM_CH, &pwm_cfg, NULL, NULL);
//            printf("%s,warm pwm_highg=%d,pwm_low=%d\r\n",__FUNCTION__,pwm_value_high,pwm_value_low);
            break;
            
        default:
            break;
        }
           
	return 0;
}


void sm15633eh_present_info_get(uint8_t *Rcolour,uint8_t *Gcolour,uint8_t *Bcolour,uint8_t *Whitecolour,uint8_t *Warmcolour)
{
    *Rcolour = led_present_info.rcolour;
    *Gcolour = led_present_info.gcolour;
    *Bcolour = led_present_info.bcolour;
    *Whitecolour = led_present_info.whitecolour;
    *Warmcolour = led_present_info.warmcolour;
}

int sm15633eh_pwm_start(uint8_t colour_select,bool onoff)
{
    switch(colour_select)
        {
        case LED_R_CHANNEL:
        if(onoff)
            {
            if(pwm_channel_status_info.rchannel==0)
                {
                pwm_channel_status_info.rchannel = 1;
                scm_timer_start(PWM0_CHANNEL_ID, RED_PWM_CH);
                }
            }
        else
            {
            if(pwm_channel_status_info.rchannel == 1)
                {
                pwm_channel_status_info.rchannel = 0;
                scm_timer_stop(PWM0_CHANNEL_ID, RED_PWM_CH);
                }
            }
        break;

        case LED_G_CHANNEL:
            if(onoff)
            {
            if(pwm_channel_status_info.gchannel==0)
                {
                pwm_channel_status_info.gchannel = 1;
                scm_timer_start(PWM0_CHANNEL_ID, GREEN_PWM_CH);
                }
            }
        else
            {
            if(pwm_channel_status_info.gchannel==1)
                {
                pwm_channel_status_info.gchannel = 0;
                scm_timer_stop(PWM0_CHANNEL_ID, GREEN_PWM_CH);
                }
            }
            break;

        case LED_B_CHANNEL:
            if(onoff)
            {
            if(pwm_channel_status_info.bchannel==0)
                {
                pwm_channel_status_info.bchannel = 1;
//                printf("b channel start\r\n");
                scm_timer_start(PWM0_CHANNEL_ID, BLUE_PWM_CH);
                }
            }
        else
            {
            if(pwm_channel_status_info.bchannel==1)
                {
                pwm_channel_status_info.bchannel = 0;
//                printf("b channel stop\r\n");
                scm_timer_stop(PWM0_CHANNEL_ID, BLUE_PWM_CH);
                }
            }
            break;

        case LED_WHITE_CHANNEL:
            if(onoff)
            {
            if(pwm_channel_status_info.whitechannel==0)
                {
                pwm_channel_status_info.whitechannel = 1;
                scm_timer_start(PWM1_CHANNEL_ID, WHITE_PWM_CH);
                }
            }
        else
            {
            if(pwm_channel_status_info.whitechannel==1)
                {
                pwm_channel_status_info.whitechannel= 0;
                scm_timer_stop(PWM1_CHANNEL_ID, WHITE_PWM_CH);
                }
            }
            break;

        case LED_WARM_CHANNEL:
            if(onoff)
            {
            if(pwm_channel_status_info.warmchannel==0)
                {
                pwm_channel_status_info.warmchannel = 1;
                scm_timer_start(PWM0_CHANNEL_ID, WARM_PWM_CH);
                }
            }
        else
            {
            if(pwm_channel_status_info.warmchannel==1)
                {
                pwm_channel_status_info.warmchannel = 0;
                scm_timer_stop(PWM0_CHANNEL_ID, WARM_PWM_CH);
                }
            }
            break;    

        default:
            break;
        }
    return 0;
}

static int ledpwm_cli_config(int argc, char *argv[])
{
	uint8_t colour_select;
	uint8_t colour_value;
	int ret;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	colour_select = atoi(argv[1]);
	colour_value = atoi(argv[2]);
           sm15633eh_pwm_init(colour_select,colour_value,colour_value,colour_value,colour_value,colour_value);
	return CMD_RET_SUCCESS;
}

static int ledpwm_cli_start(int argc, char *argv[])
{
        uint8_t colour_select;
        bool onoff;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	colour_select = atoi(argv[1]);
	onoff = atoi(argv[2]);
	sm15633eh_pwm_start(colour_select,onoff);
	return CMD_RET_SUCCESS;
}
#if 1
static const struct cli_cmd ledpwm_cli_cmd[] = {
	CMDENTRY(config, ledpwm_cli_config, "", ""),
	CMDENTRY(start, ledpwm_cli_start, "", ""),
};

static int do_ledpwm_cli(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], ledpwm_cli_cmd, ARRAY_SIZE(ledpwm_cli_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(ledpwm, do_ledpwm_cli,
	"CLI for ledpwm API test",
	"ledpwm config sel colour" OR
	"ledpwm start  sel onoff" OR
);
#endif

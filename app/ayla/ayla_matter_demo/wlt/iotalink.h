#ifndef __IOTALINK_H__
#define __IOTALINK_H__

//保存 wlt 头文件




//#include <sys/types.h>
#include <lwip/sys.h>
#include "wise_wifi.h"
#include "wise_wifi_types.h"
#include "wise_system.h"

#include "scm_gpio.h"
#include "scm_timer.h"
#include "scm_adc.h"
//#include "scm_uart.h"

//#include "scm_spi.h"

//#include "wlt_ws2812_spi.h"

#include "iotalink_ws2812.h"
#include "iotalink_button.h"
#include "iotalink_music.h"
#include "iotalink_control.h"


	
void demo_send_prop(const char *name);







#endif //__IOTALINK_H__

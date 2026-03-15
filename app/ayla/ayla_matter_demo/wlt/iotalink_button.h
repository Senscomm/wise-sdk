#ifndef _IOTALINK_BUTTON_H
#define _IOTALINK_BUTTON_H



#include "iotalink.h"




void iotalink_button_init(void );

void button_detection_cb(void *data );

int iotalink_button_event_handle(unsigned char btn, unsigned int press_time, unsigned char pressed);


#endif










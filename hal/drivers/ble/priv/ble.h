#ifndef _BLE_H_
#define _BLE_H_

#include <hal/device.h>

typedef void (*ble_isr)(void);

typedef void (*ble_timer_isr)(void);

int ble_init(struct device *ble, ble_isr isr, ble_timer_isr t_isr);

#endif //_BLE_H_

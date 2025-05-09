/*
 * Copyright 2009, Colin Günther. All Rights Reserved.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_BUS_H_
#define _FBSD_COMPAT_SYS_BUS_H_

/* FIXME: I am getting rid of haiku-*
#include "haiku-module.h"
*/
#include <sys/queue.h>

/* Note that we reversed the original order, so whenever actual (negative)
   numbers are used in a driver, we have to change it. */
#define BUS_PROBE_SPECIFIC		0
#define BUS_PROBE_LOW_PRIORITY	10
#define BUS_PROBE_DEFAULT		20
#
// TODO per platform, these are 32-bit

const char *device_get_name(device_t dev);
const char *device_get_nameunit(device_t dev);
int device_get_unit(device_t dev);
void *device_get_softc(device_t dev);
int device_printf(device_t dev, const char *, ...) __printflike(2, 3);
void device_set_desc(device_t dev, const char *desc);
void device_set_desc_copy(device_t dev, const char *desc);
const char *device_get_desc(device_t dev);
device_t device_get_parent(device_t dev);
u_int32_t device_get_flags(device_t dev);
int device_get_children(device_t dev, device_t **devlistp, int *devcountp);

void device_set_ivars(device_t dev, void *);
void *device_get_ivars(device_t dev);

device_t device_add_child(device_t dev, driver_t *driver, const char *name, int unit);
int device_delete_child(device_t dev, device_t child);
int device_is_attached(device_t dev);
int device_attach(device_t dev);
int device_detach(device_t dev);
int device_set_driver(device_t dev, driver_t *driver);
int device_is_alive(device_t dev);

#endif	/* _FBSD_COMPAT_SYS_BUS_H_ */

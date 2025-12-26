/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2018 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef __LIGHTING_CTRL_H__
#define __LIGHTING_CTRL_H__

#include "app_event.h"

typedef void (*lc_callback_completed)(bool timeout);

extern void lighting_ctrl_init(void);
extern void lighting_ctrl_add_event(struct app_event *evt);
/* timeout: in ms, 0 if not used
 * num: # of times to run, -1 if indefinite */
extern void lighting_ctrl_run(int num, u32 timeout, lc_callback_completed cb);
/* timeout: whether or not due to timeout
 * silent: terminate without callback
 */
extern void lighting_ctrl_terminate(bool timeout, bool silent);

#endif

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

#ifndef _SCM_WDT_H_
#define _SCM_WDT_H_

#include <stdint.h>

/**
 * @brief watchdog timer start and expire after ms
 *
 * @param[in] ms expire time of watchdog timer
 */
int scm_wdt_start(uint32_t ms);

/**
 * @brief watchdog timer stop
 */
int scm_wdt_stop(void);

/**
 * @brief watchdog timer feed
 */
int scm_wdt_feed(void);

#endif //_SCM_WDT_H_

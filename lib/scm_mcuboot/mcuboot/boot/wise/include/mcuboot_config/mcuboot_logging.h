/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _MCUBOOT_LOGGING_H_
#define _MCUBOOT_LOGGING_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MCUBOOT_LOG_MODULE_DECLARE(...)
#define MCUBOOT_LOG_MODULE_REGISTER(...)

#define MCUBOOT_LOG_ERR(format, ...) \
    printf("%s: " format "\n", __FUNCTION__, ##__VA_ARGS__)

#define MCUBOOT_LOG_WRN(format, ...) \
    printf("%s: " format "\n", __FUNCTION__, ##__VA_ARGS__)

#define MCUBOOT_LOG_INF(format, ...) \
    printf("%s: " format "\n", __FUNCTION__, ##__VA_ARGS__)

#define MCUBOOT_LOG_DBG(format, ...) \
    printf("%s: " format "\n", __FUNCTION__, ##__VA_ARGS__)

#endif /* _MCUBOOT_LOGGING_H_ */

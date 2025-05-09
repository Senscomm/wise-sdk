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


#ifndef __SCM_LOG_H__
#define __SCM_LOG_H__

#include <stdint.h>
#include <stdarg.h>
#include "wise_log.h"

#ifdef __cplusplus
extern "C" {
#endif

//#define SCM_DBG_EN

#ifdef SCM_DBG_EN
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL WISE_LOG_DEBUG

#define SCM_DBG_LOG(tag, format, ... ) WISE_LOG_LEVEL_LOCAL(WISE_LOG_DEBUG,    tag, format, ##__VA_ARGS__)
#else
#define SCM_DBG_LOG(...)
#endif
#define SCM_ERR_LOG(tag, format, ... ) WISE_LOG_LEVEL_LOCAL(WISE_LOG_ERROR,    tag, format, ##__VA_ARGS__)
#define SCM_INFO_LOG(tag, format, ... ) WISE_LOG_LEVEL_LOCAL(WISE_LOG_INFO,    tag, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __SCM_LOG_H__ */

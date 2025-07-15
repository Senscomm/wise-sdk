/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __WISE_SYS_TIME_H__
#define __WISE_SYS_TIME_H__

#include_next <sys/time.h>

/* XXX: must use what the platform provides. */
#if 1 /* ndef __USE_NATIVE_HEADER__ */

#ifdef __cplusplus
extern "C" {
#endif

int _os_gettimeofday(struct timeval *tv, struct timezone *tz);

int _gettimeofday(struct timeval *t, struct timezone *tz);
#define gettimeofday 		_gettimeofday

int _settimeofday(struct timeval *t, struct timezone *tz);
#define settimeofday		_settimeofday

#ifdef __cplusplus
}
#endif

#endif /* __USE_NATIVE_HEADER__ */

#endif

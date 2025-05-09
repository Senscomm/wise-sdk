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

#ifndef __WISE_STRING_H__
#define __WISE_STRING_H__

#include <stddef.h>
#include <stdarg.h>

#include_next <string.h>

#ifndef __USE_NATIVE_HEADER__

char *os_strndup2(const char *s, size_t n);
char *os_strcasestr2(const char *str, const char *substr);

#undef strndup
#define strndup os_strndup2

#undef strcasestr
#define strcasestr os_strcasestr2

#endif /* __USE_NATIVE_HEADER__ */

#endif

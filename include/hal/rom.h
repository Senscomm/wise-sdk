/*
 * Copyright 2022-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __ROM_H__
#define __ROM_H__

#include <u-boot/linker-lists.h>

struct fcopy {
	unsigned long dst;
	unsigned long src;
};

#define _PROVIDE(_func_) \
	ll_entry_declare(struct fcopy, _func_, provide)

#define PROVIDE(fn, d, s)   \
	_PROVIDE(fn) = {	    \
		.dst = (unsigned long)d, 		    \
		.src = (unsigned long)s		    \
	}

#define provide_start() ll_entry_start(struct fcopy, provide)
#define provide_end() ll_entry_end(struct fcopy, provide)

#define _PATCH(_func_) \
	ll_entry_declare(struct fcopy, _func_, patch)

/*
 * please set your function name with prefix patch_xxx
 * ex: patch_original_func_name
 */
#define PATCH(fn, d, s)   \
	_PATCH(fn) = {	    \
		.dst = (unsigned long)d, 		    \
		.src = (unsigned long)s		    \
	}

#define patch_start() ll_entry_start(struct fcopy, patch)
#define patch_end() ll_entry_end(struct fcopy, patch)

void patch(void);
void provide(void);

#endif // __ROM_H__

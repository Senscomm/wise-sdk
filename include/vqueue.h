/*
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __VQUEUE_H__
#define __VQUEUE_H__

#include <stdint.h>
#include <freebsd/errors.h>
#include "vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

int vqueue(int count, int size);
int vqueue_put(int fd, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout);
int vqueue_get(int fd, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout);
int vqueue_capacity(int fd);
int vqueue_msg_size(int fd);
int vqueue_count(int fd);
int vqueue_space(int fd);

#ifdef __cplusplus
}
#endif

#endif /* __VQUEUE_H__ */

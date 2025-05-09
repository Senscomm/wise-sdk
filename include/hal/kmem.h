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
#ifndef __KMEM_H__
#define __KMEM_H__

#include <hal/kernel.h>

extern void *(*kmalloc)(size_t size);
void *kzalloc(size_t size);
void _kfree(void *ptr);
extern void (*kfree)(void *ptr);

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
void *dma_kmalloc(size_t size);
void dma_kfree(void *ptr);
#endif

#endif

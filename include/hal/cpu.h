/*
 * Copyright (c) 2020-2024 Senscomm Semiconductor Co., Ltd. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __WISE_CPU_H__
#define __WISE_CPU_H__

#ifdef __cplusplus
extern "C" {
#endif

void dcache_enable(void);
void dcache_disable(void);

void dcache_flush_all(void);
void dcache_flush_range(unsigned long start, unsigned long stop);

void dcache_invalidate_range(unsigned long start, unsigned long stop);
void dcache_invalidate_all(void);

void icache_invalidate_all(void);

#ifdef __cplusplus
}
#endif

#endif /* __WISE_CPU_H__ */

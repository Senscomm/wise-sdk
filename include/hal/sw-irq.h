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

#ifndef __SW_IRQ_H__
#define __SW_IRQ_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int request_sw_irq(int irq,
		       int (*handler)(int, void *),
		       const char *name,
		       unsigned priority,
		       void *priv);

extern void free_sw_irq(int irq, const char *name);
extern int get_sw_irq_stat(char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif

/*
 * Copyright (c) 2018-2024 Senscomm Semiconductor Co., Ltd. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#ifndef _CHANGEITEM_H
#define _CHANGEITEM_H

#include <hal/types.h>
#include <u-boot/list.h>

struct changeitem {
	struct list_head list;
	struct list_head *head;

	struct changeitem *(*p_next)(struct changeitem *this);
	const char *(*m_name)(struct changeitem *this);
	int (*m_current)(struct changeitem *this);
	void (*m_init)(struct changeitem *this);
	bool (*m_change)(struct changeitem *this);
	void (*m_print)(struct changeitem *this);
	bool (*m_is_included)(struct changeitem *this, struct changeitem *other);

	char name[32];
	int start;
	int end;
	int step;
	int current;
};

struct changeitem *create_changeitem(const char *name,
		int start, int end, int step);
void destroy_changeitem(struct changeitem *this);
#endif /* _CHANGEITEM_H */

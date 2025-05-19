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
#ifndef _CHANGESET_H
#define _CHANGESET_H

#include <hal/types.h>
#include <u-boot/list.h>
#include <changeitem.h>

#ifdef __cplusplus
extern "C" {
#endif

struct changeset {
	struct list_head list;

	bool (*m_init)(struct changeset *this);
	bool (*m_iter)(struct changeset *this);
	void (*m_add)(struct changeset *this, const char *name, int start, int end, int step);
	void (*m_print)(struct changeset *this);
	struct changeitem *(*m_find)(struct changeset *this, const char *name);
	bool (*m_is_subset)(struct changeset *this, struct changeset *other);
	u32  (*m_curidx)(struct changeset *this);

	struct list_head items;
	u32 curidx;
};

#define for_each_changeitem(ci, cs)			\
	list_for_each_entry(ci, &cs->items, list)

struct changeset *create_changeset(void);
void destroy_changeset(struct changeset *this);

#ifdef __cplusplus
}
#endif

#endif /* _CHANGESET_H */

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
#include <stdio.h>
#include <string.h>
#include <hal/kernel.h>
#include <hal/kmem.h>
#include <changeset.h>

static
bool changeset_init(struct changeset *this)
{
	struct changeitem *item = NULL;

	list_for_each_entry(item, &this->items, list)
		item->m_init(item);

	this->curidx = 0;

	return item != NULL;
}

static
bool changeset_iter(struct changeset *this)
{
	if (!list_empty(&this->items)) {
		struct changeitem *first = list_first_entry(&this->items, struct changeitem, list);
		this->curidx++;
		return first->m_change(first);
	}
	return false;
}

static
void changeset_add(struct changeset *this,
		const char *name, int start, int end, int step)
{
	struct changeitem *item = create_changeitem(name, start, step == 0 ? start : end, step);
	list_add_tail(&item->list, &this->items);
	item->head = &this->items;
}

static
void changeset_print(struct changeset *this)
{
	struct changeitem *item;

	list_for_each_entry(item, &this->items, list)
		item->m_print(item);
}

static
struct changeitem *changeset_find(struct changeset *this,
		const char *name)
{
	struct changeitem *ci;

	list_for_each_entry(ci, &this->items, list) {
		if (!strcmp(ci->m_name(ci), name))
			return ci;
	}

	return NULL;
}

static
bool changeset_is_subset(struct changeset *this,
		struct changeset *other)
{
	struct changeitem *my_ci;
        struct changeitem *to_check;
	bool subset = false;

	if (list_empty(&other->items))
		return false;

	list_for_each_entry(to_check, &other->items, list) {
		if ((my_ci = this->m_find(this, to_check->m_name(to_check))) != NULL) {
			subset = my_ci->m_is_included(my_ci, to_check);
		} else
			subset = false;
		if (subset == false)
			break;
	}

	return subset;
}

static
u32 changeset_curidx(struct changeset *this)
{
	return this->curidx;
}

struct changeset *create_changeset(void)
{
	struct changeset *cs = kmalloc(sizeof(struct changeset));

	assert(cs);

	INIT_LIST_HEAD(&cs->items);

	cs->m_init = changeset_init;
	cs->m_iter = changeset_iter;
	cs->m_add = changeset_add;
	cs->m_print = changeset_print;
	cs->m_find = changeset_find;
	cs->m_is_subset = changeset_is_subset;
	cs->m_curidx = changeset_curidx;

	return cs;
}

void destroy_changeset(struct changeset *this)
{
	struct changeitem *ci, *tmp;

	if (!this)
		return;

	list_for_each_entry_safe(ci, tmp, &this->items, list)
		destroy_changeitem(ci);

	kfree(this);
}

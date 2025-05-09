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
#include <assessor.h>

static
void assessor_exclude(struct assessor *this, struct changeset *cs)
{
	list_add_tail(&cs->list, &this->excluded);
}

static
bool assessor_assess(struct assessor *this, struct changeset *cs)
{
	struct changeset *check;

	list_for_each_entry(check, &this->excluded, list) {
		if (cs->m_is_subset(cs, check))
			return false;
	}

	return true;
}

struct assessor *create_assessor(void)
{
	struct assessor *as = kmalloc(sizeof(struct assessor));
	assert(as);

	INIT_LIST_HEAD(&as->excluded);

	as->m_exclude = assessor_exclude;
	as->m_assess = assessor_assess;

	return as;
}

void destroy_assessor(struct assessor *this)
{
	struct changeset *cs;

	if (!this)
		return;

	list_for_each_entry(cs, &this->excluded, list)
		destroy_changeset(cs);

	kfree(this);
}

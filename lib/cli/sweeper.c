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
#include <sweeper.h>

static
void sweeper_set_changeset(struct sweeper *this, struct changeset *cs)
{
	this->cs = cs;
}

static
void sweeper_set_assessor(struct sweeper *this, struct assessor *as)
{
	this->as = as;
}

static
void sweeper_set_action(struct sweeper *this, struct action *ac)
{
	this->ac = ac;
}

static
void sweeper_sweep(struct sweeper *this)
{
	if (!this->cs) {
		printf("No changeset in this sweeper\n");
		return;
	}
	if (!this->ac || !this->ac->m_do) {
		printf("No action in this sweeper\n");
		return;
	}

	taskqueue_drain(this->tq, &this->task);
	if (this->cs->m_init(this->cs)) {
		this->busy = true;
		taskqueue_enqueue_fast(this->tq, &this->task);
	}
}

static
bool sweeper_is_busy(struct sweeper *this)
{
	return this->busy;
}

static void sweep(void *arg, int pending)
{
	struct sweeper *this = arg;
	bool passed;
	int ret = 0;

	printf("\nsweep start\n");

	do {
		passed = (this->as ? this->as->m_assess(this->as, this->cs) : true);
		if (passed) {
			while ((ret = this->ac->m_do(this->ac, this->cs)) == -EAGAIN)
				osDelay(1);
		}
	} while (ret == 0 && this->cs->m_iter(this->cs));

	if (ret < 0) {
		printf("sweep failed at: \n");
		this->cs->m_print(this->cs);
	}

	this->busy = false;

	printf("\nsweep done\n");
}

struct sweeper *create_sweeper(void)
{
	struct sweeper *s = kmalloc(sizeof(struct sweeper));

	assert(s);

	s->tq = taskqueue_create("sw", 0,
			taskqueue_thread_enqueue, &s->tq);
	taskqueue_start_threads(&s->tq, 1, PI_NET, "%s taskq",
	    "sweeper");
	TASK_INIT(&s->task, 0, sweep, s);

	s->m_set_changeset = sweeper_set_changeset;
	s->m_set_assessor = sweeper_set_assessor;
	s->m_set_action = sweeper_set_action;
	s->m_sweep = sweeper_sweep;
	s->m_is_busy = sweeper_is_busy;

	s->busy = false;

	return s;
}

void destroy_sweeper(struct sweeper *this)
{
	kfree(this);
}

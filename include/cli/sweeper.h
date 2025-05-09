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
#ifndef _SWEEPER_H
#define _SWEEPER_H

#include <hal/types.h>
#include <u-boot/list.h>
#include <cmsis_os.h>
#include <taskqueue.h>
#include <changeset.h>
#include <assessor.h>
#include <action.h>

struct sweeper {
	void (*m_set_changeset)(struct sweeper *this, struct changeset *cs);
	void (*m_set_assessor)(struct sweeper *this, struct assessor *as);
	void (*m_set_action)(struct sweeper *this, struct action *ac);
	void (*m_sweep)(struct sweeper *this);
	bool (*m_is_busy)(struct sweeper *this);

	struct changeset *cs;
	struct assessor *as;
	struct action *ac;

	struct taskqueue *tq;
	struct task task;

	bool busy;
};

struct sweeper *create_sweeper(void);
void destroy_sweeper(struct sweeper *this);

#endif /* _SWEEPER_H */

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
#ifndef _ASSESSOR_H
#define _ASSESSOR_H

#include <hal/types.h>
#include <u-boot/list.h>
#include <changeset.h>

#ifdef __cplusplus
extern "C" {
#endif

struct assessor {
	void (*m_exclude)(struct assessor *this, struct changeset *cs);
	bool (*m_assess)(struct assessor *this, struct changeset *cs);

	struct list_head excluded;
};

struct assessor *create_assessor(void);
void destroy_assessor(struct assessor *this);

#ifdef __cplusplus
}
#endif

#endif /* _ASSESSOR_H */

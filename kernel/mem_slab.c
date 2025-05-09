/*
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 * Copyright (c) 2016 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define __LINUX_ERRNO_EXTENSIONS__

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/param.h>

#include "hal/console.h"
#include "hal/kmem.h"
#include "hal/init.h"
#include "hal/timer.h"

#include "mem_slab.h"

/* The list of defined memory slabs */
LIST_HEAD_DEF(memslabs);


/**
 * @brief Initialize kernel memory slab subsystem.
 *
 * Perform any initialization of memory slabs that wasn't done at build time.
 * Currently this just involves creating the list of free blocks for each slab.
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p slab contains invalid configuration and/or values.
 */

static int create_free_list(struct mem_slab *slab)
{
    char *p;

    /* blocks must be word aligned */
    if (((slab->info.block_size | (uintptr_t)slab->buffer) &
                (sizeof(void *) - 1)) != 0U) {
        return -EINVAL;
    }

    slab->free_list = NULL;
    p = slab->buffer + slab->info.block_size * (slab->info.num_blocks - 1);

    while (p >= slab->buffer) {
        *(char **)p = slab->free_list;
        slab->free_list = p;
        p -= slab->info.block_size;
    }
    return 0;
}

static int create_wait_q(struct mem_slab *slab)
{
    osMessageQueueAttr_t attr = {0,};

    attr.cb_mem = &slab->cb;
    attr.cb_size = sizeof(slab->cb);
    attr.mq_mem = &slab->free_block;
    attr.mq_size = sizeof(char *);
    slab->wait_q = osMessageQueueNew(1, sizeof(char *), &attr);

    if (slab->wait_q == NULL) {
        return -EINVAL;
    }

    return 0;
}

int mem_slab_init(struct mem_slab *slab, void *buffer,
        size_t block_size, uint32_t num_blocks)
{
    int rc = 0;

    slab->info.num_blocks = num_blocks;
    slab->info.block_size = block_size;
    slab->buffer = buffer;
    slab->info.num_used = 0U;
    slab->info.max_used = 0U;

    rc = create_free_list(slab);
    if (rc < 0) {
        goto err;
    }

    rc = create_wait_q(slab);
    if (rc < 0) {
        goto err;
    }

    list_add_tail(&slab->list, &memslabs);
    return 0;

err:
    printk ("SLAB: memory slab at %x could not be initialized\n", slab);

    return rc;
}

static int delete_wait_q(struct mem_slab *slab)
{
    if (slab->wait_q == NULL) {
        return 0;
    }

    assert(osMessageQueueGetCount(slab->wait_q) == 0);

    if (osMessageQueueDelete(slab->wait_q)) {
        return -EINVAL;
    }

    slab->wait_q = NULL;

    return 0;
}

int mem_slab_deinit(struct mem_slab *slab)
{
    int rc = 0;

    assert(slab->info.num_used == 0);

    rc = delete_wait_q(slab);
    if (rc < 0) {
        goto err;
    }

    return 0;

err:
    printk ("SLAB: memory slab at %x could not be Deinitialized\n", slab);

    return rc;
}

static bool slab_ptr_is_good(struct mem_slab *slab, const void *ptr)
{
    const char *p = ptr;
    ptrdiff_t offset = p - slab->buffer;

    return (offset >= 0) &&
        (offset < (slab->info.block_size * slab->info.num_blocks)) &&
        ((offset % slab->info.block_size) == 0);
}

int mem_slab_alloc(struct mem_slab *slab, void **mem, uint32_t timeout)
{
    uint32_t flags;
    int result;
    osStatus_t stat;

    local_irq_save(flags);

    if (slab->free_list != NULL) {
        bool sane = true;
        /* take a free block */
        *mem = slab->free_list;
        slab->free_list = *(char **)(slab->free_list);
        slab->info.num_used++;
#if 0
        assert((slab->free_list == NULL &&
                    slab->info.num_used == slab->info.num_blocks) ||
                slab_ptr_is_good(slab, slab->free_list));
#else
        sane = ((slab->free_list == NULL &&
                    slab->info.num_used == slab->info.num_blocks) ||
                slab_ptr_is_good(slab, slab->free_list));
        if (!sane) {
            printk("slab: %p, free_list: %p, num_used: %d, num_blocks: %d, block_size: %d, buffer: %p\n",
                    slab, slab->free_list, slab->info.num_used, slab->info.num_blocks,
                    slab->info.block_size, slab->buffer);
            assert(false);
        }
#endif
        slab->info.max_used = MAX(slab->info.num_used,
                slab->info.max_used);
        result = 0;
    } else if (!timeout) {
        /* don't wait for a free block to become available */
        *mem = NULL;
        result = -ENOMEM;
    } else {
        u32 tick;

        local_irq_restore(flags);

        if (timeout == osWaitForever) {
            tick = timeout;
        } else {
            tick = pdMS_TO_TICKS(timeout);
        }

        /* wait for a free block or timeout (ms) */
        stat = osMessageQueueGet(slab->wait_q, mem, NULL, tick);
        switch (stat) {
            case osErrorParameter:
            case osErrorResource:
                result = -EINVAL;
                break;
            case osErrorTimeout:
                result = -ETIME;
                break;
            default:
                result = 0;
#if 0
                printk("Directly sent.\n");
#endif
                break;
        }
        return result;
    }

    local_irq_restore(flags);

    return result;
}

void mem_slab_free(struct mem_slab *slab, void *mem)
{
    uint32_t flags;
    int result;

    local_irq_save(flags);

    if (slab->free_list == NULL
            && osMessageQueueGetNumberOfTasksWaitingToReceive(slab->wait_q) != 0
            && osMessageQueueGetSpace(slab->wait_q) != 0) {
        result = osMessageQueuePut(slab->wait_q, &mem, 0, 0);
        assert(result == osOK || result == osErrorNeedSched);
        local_irq_restore(flags);
        return;
    }

    *(char **) mem = slab->free_list;
    slab->free_list = (char *) mem;
    slab->info.num_used--;

    local_irq_restore(flags);
}


/**
 * mem_slab_init() - initialize mem_slab subsystem
 *
 * The function initializes all *statically* defined
 * memory slabs BEFORE all device drivers are initialized.
 * By doing this, we could allow a device driver
 * to use a memory slab since during initialization.
 */

__iram__ static int memory_slab_init(void)
{
    struct mem_slab *s;
    int ret = 0;

    for (s = memslab_start(); s < memslab_end(); s++) {
        ret = create_free_list(s);
        if (ret < 0) {
            break;
        }
        ret = create_wait_q(s);
        if (ret < 0) {
            break;
        }
        printk ("SLAB: memory slab at %x initialized\n", s);
        list_add_tail(&s->list, &memslabs);
    }

    if (ret < 0) {
        printk ("SLAB: memory slab at %x could not be initialized\n", s);
    }

    return ret;
}

/* subsystem initialization will be done before driver initialization. */

__initcall__ (subsystem, memory_slab_init);

__iram__ static int memory_slab_fini(void)
{
    INIT_LIST_HEAD(&memslabs);

    return 0;
}

__finicall__ (subsystem, memory_slab_fini);

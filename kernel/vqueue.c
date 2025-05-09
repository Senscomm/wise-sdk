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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#include <hal/init.h>
#include <hal/kernel.h>

#include "sys/ioctl.h"
#include "vfs.h"
#include "mmap.h"
#include "cmsis_os.h"

#define IOCTL_VQUEUE_PUT            (1)
#define IOCTL_VQUEUE_GET            (2)
#define IOCTL_VQUEUE_QUERY          (3)
#define IOCTL_VQUEUE_RESET          (4)

struct vqueue_put_arg {
    const void *msg_ptr;
    uint8_t msg_prio;
    uint32_t timeout;
};

struct vqueue_get_arg {
    void *msg_ptr;
    uint8_t msg_prio;
    uint32_t timeout;
};

enum vqueue_query_item {
    VQUEUE_CAPACITY = 0,
    VQUEUE_MSGSIZE  = 1,
    VQUEUE_COUNT    = 2,
    VQUEUE_SPACE    = 3,
};

struct vqueue_query_arg {
    enum vqueue_query_item item;
    uint32_t val;
};

struct vqueue {
    struct file file;
    osMessageQueueId_t qid;
};

static struct fops vqueue_fops;

/*
 * Helper functions
 */

#define file_to_vqueue(file) container_of(file, struct vqueue, file)

static void vq_get(struct vqueue *vq)
{
    file_get(&vq->file);
}

static void vq_put(struct vqueue *vq)
{
    file_put(&vq->file);
}

/**
 * vqueue_alloc() - allocate and initialize a vqueue object
 * @count: maximum number of messages to hold
 * @size: size of one message
 */
static struct vqueue *vqueue_alloc(int count, int size)
{
    struct vqueue *vq;
    struct file *file;

    /* Allocate a vqueue object and initialize it */
    if ((vq = zalloc(sizeof(*vq))) == NULL)
        return NULL;

    vq->qid = osMessageQueueNew(count, size, NULL);

    file = &vq->file;
    vfs_init_file(file);
    file->f_flags = O_RDWR;
    file->f_type = FTYPE_VIRTUAL;
    file->f_fops = &vqueue_fops;

    return vq;
}

/**
 * vqueue_free() - free a struct vqueue object
 * @vq: vqueue to free
 */
static void vqueue_free(struct vqueue* vq)
{
    osMessageQueueDelete(vq->qid);
    vq->qid = NULL;
    vfs_destroy_file(&vq->file);
    free(vq);
}

/**
 * fd_to_vqueue() - obtain the struct vqueue object from file descriptor number
 * @fd: file descriptor of a vqueue
 */
static struct vqueue *fd_to_vqueue(int fd)
{
    struct file *file;
    struct vqueue *vq;

    if ((file = vfs_fd_to_file(fd)) == NULL)
        return NULL;

    vq = file_to_vqueue(file);
    return vq;
}

/*
 * vqueue file operations
 */

static int vqueue_release(struct file *file)
{
    struct vqueue *vq = file_to_vqueue(file);

    vqueue_free(vq);

    return 0;
}

static int vqueue_ioctl(struct file *file, unsigned int cmd, void *argp)
{
    struct vqueue *vq = file_to_vqueue(file);
    int ret = 0;

    switch (cmd) {
        case IOCTL_VQUEUE_PUT: {
                                   struct vqueue_put_arg *put_arg = argp;
                                   if (osMessageQueuePut(vq->qid, put_arg->msg_ptr, put_arg->msg_prio,
                                               put_arg->timeout)) {
                                       ret =  -EINVAL;
                                   }
                                   break;
                               }
        case IOCTL_VQUEUE_GET: {
                                   struct vqueue_get_arg *get_arg = argp;
                                   if (osMessageQueueGet(vq->qid, get_arg->msg_ptr, &get_arg->msg_prio,
                                               get_arg->timeout)) {
                                       ret = -ENOMEM;
                                   }
                                   break;
                               }
        case IOCTL_VQUEUE_QUERY: {
                                     struct vqueue_query_arg *query_arg = argp;
                                     enum vqueue_query_item item = query_arg->item;
#if 0
                                     if (item == VQUEUE_CAPACITY) {
                                         query_arg->val = osMessageQueueGetCapacity(vq->qid);
                                     } else if (item == VQUEUE_MSGSIZE) {
                                         query_arg->val = osMessageQueueGetMsgSize(vq->qid);
                                     } else
#endif
                                         if (item == VQUEUE_COUNT) {
                                             query_arg->val = osMessageQueueGetCount(vq->qid);
                                         } else if (item == VQUEUE_SPACE) {
                                             query_arg->val = osMessageQueueGetSpace(vq->qid);
                                         } else {
                                             ret = -EINVAL;
                                         }
                                     break;
                                 }
#if 0
        case IOCTL_VQUEUE_RESET: {
                                     if (osMessageQueueReset(vq->qid)) {
                                         ret = -EINVAL;
                                     }
                                     break;
                                 }
#endif
        default:
                                 break;
    }

    return ret;
}

    static
unsigned vqueue_poll(struct file *file, struct poll_table *pt, struct pollfd *pfd)
{
    struct vqueue *vq = file_to_vqueue(file);
    unsigned mask = 0;

    assert((pfd->events & POLLOUT) == 0);
    assert((pfd->events & POLLIN) != 0);

    if (pfd->events & POLLIN) {
        poll_add_wait(file, vq->qid, pt);
    }

    mask |= osMessageQueueGetCount(vq->qid) ? POLLIN : 0;

    return mask;
}

static struct fops vqueue_fops = {
    .release  = vqueue_release,
    .poll     = vqueue_poll,
    .ioctl    = vqueue_ioctl,
};

/*
 * vqueue library functions implementation
 */

/**
 * vqueue() - create a vqueue for messaging
 */

int vqueue(int count, int size)
{
    struct vqueue *vq = NULL;
    struct file *file;
    int fd = -1, ret;

    /* Allocate a free file descriptor number and a vqueue */
    if ((fd = vfs_get_free_fd()) < 0) {
        ret = fd;
        goto error;
    }

    if ((vq = vqueue_alloc(count, size)) == NULL) {
        ret = -ENOMEM;
        goto error;
    }

    file = &vq->file;
    atomic_inc(&file->f_refcnt);
    vfs_install_fd(fd, file);
    return fd;

error:
    if (vq)
        vqueue_free(vq);
    if (fd >= 0)
        vfs_free_fd(fd);
    if (ret < 0) {
        errno = -ret;
        ret = -1;
    }

    return ret;
}

/**
 * vqueue_put() - put a message into vqueue
 */

int vqueue_put(int fd, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout)
{
    struct vqueue *vq = fd_to_vqueue(fd);
    struct vqueue_put_arg arg;
    int ret;

    assert(vq);

    vq_get(vq);

    arg.msg_ptr = msg_ptr;
    arg.msg_prio = msg_prio;
    arg.timeout = timeout;

    ret = ioctl(fd, IOCTL_VQUEUE_PUT, &arg);

    vq_put(vq);

    return ret;
}

/**
 * vqueue_get() - get a message from vqueue
 */

int vqueue_get(int fd, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout)
{
    struct vqueue *vq = fd_to_vqueue(fd);
    struct vqueue_get_arg arg;
    int ret;

    assert(vq);

    vq_get(vq);

    arg.msg_ptr = msg_ptr;
    arg.timeout = timeout;

    ret = ioctl(fd, IOCTL_VQUEUE_GET, &arg);

    if (msg_prio) {
        *msg_prio = arg.msg_prio;
    }

    vq_put(vq);

    return ret;
}

/**
 * vqueue_capacity() - get capacity of vqueue
 */

int vqueue_capacity(int fd)
{
    struct vqueue *vq = fd_to_vqueue(fd);
    struct vqueue_query_arg arg;

    vq_get(vq);

    arg.item = VQUEUE_CAPACITY;

    ioctl(fd, IOCTL_VQUEUE_QUERY, &arg);

    vq_put(vq);

    return arg.val;
}

/**
 * vqueue_msg_size() - get message size of vqueue
 */

int vqueue_msg_size(int fd)
{
    struct vqueue *vq = fd_to_vqueue(fd);
    struct vqueue_query_arg arg;

    arg.item = VQUEUE_MSGSIZE;

    vq_get(vq);

    ioctl(fd, IOCTL_VQUEUE_QUERY, &arg);

    vq_put(vq);

    return arg.val;
}

/**
 * vqueue_count() - get current message count of vqueue
 */

int vqueue_count(int fd)
{
    struct vqueue *vq = fd_to_vqueue(fd);
    struct vqueue_query_arg arg;

    arg.item = VQUEUE_COUNT;

    vq_get(vq);

    ioctl(fd, IOCTL_VQUEUE_QUERY, &arg);

    vq_put(vq);

    return arg.val;
}

/**
 * vqueue_space() - get available space in vqueue
 */

int vqueue_space(int fd)
{
    struct vqueue *vq = fd_to_vqueue(fd);
    struct vqueue_query_arg arg;

    arg.item = VQUEUE_SPACE;

    vq_get(vq);

    ioctl(fd, IOCTL_VQUEUE_QUERY, &arg);

    vq_put(vq);

    return arg.val;
}

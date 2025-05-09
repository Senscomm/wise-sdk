/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifdef CONFIG_LINK_TO_ROM

#include <stdio.h>
#include <poll.h>

#include "hal/console.h"
#include "hal/kmem.h"

#include <sys/time.h>
#include <cmsis_os.h>

#include "vfs.h"

extern void (*vfs_lock)(void);
extern void (*vfs_unlock)(void);

/* The array of open files */
static struct file *vfs_fds_x[CONFIG_VFS_MAX_FDS];

/* The reserved slot of vfs_fds */
static struct file vfs_res;

/**
 * vfs_get_free_fd() - obtain an unused file descriptor number
 *
 * Return:
 * The function returns the first free file descriptor number, or -ENFILE if
 * none is available.
  */

__ilm__ int patch_vfs_get_free_fd(void)
{
    int fd = -ENFILE;

    vfs_lock();
    for (fd = 0; fd < ARRAY_SIZE(vfs_fds_x); fd++) {
        if (vfs_fds_x[fd] == NULL) {
			/* https://github.com/Senscomm/wise/issues/2036 */
			/* XXX: we need to make sure different clients won't
			 * get the same fd between get_free_fd and install_fd
			 * by marking it to be 'reserved'.
			 */
			vfs_fds_x[fd] = &vfs_res;
            goto out;
		}
    }
  out:
    vfs_unlock();
    return fd;
}

extern int (*vfs_get_free_fd)(void);

PATCH(vfs_get_free_fd, &vfs_get_free_fd, &patch_vfs_get_free_fd);


/**
 * vfs_free_fd() - free the file descriptor number
 * @fd: file descriptor number to free
 */

__ilm__ void patch_vfs_free_fd(int fd)
{
    vfs_lock();
    vfs_fds_x[fd] = NULL;
    vfs_unlock();
}

extern void (*vfs_free_fd)(int fd);

PATCH(vfs_free_fd, &vfs_free_fd, &patch_vfs_free_fd);

/**
 * vfs_install_fd() - associate the given file object the file number
 * @fd: file descriptor number
 * @file: file object
 */

__ilm__ void patch_vfs_install_fd(int fd, struct file *file)
{
    vfs_fds_x[fd] = file;
    file->f_fd = fd;
}

extern void (*vfs_install_fd)(int fd, struct file * file);

PATCH(vfs_install_fd, &vfs_install_fd, &patch_vfs_install_fd);

/**
 * vfs_fd_to_file() - return a file object of the given file descriptor number
 * @fd: file descriptor number
 */

__ilm__ struct file *patch_vfs_fd_to_file(int fd)
{
    if (fd < 0 || fd >= CONFIG_VFS_MAX_FDS * 2)
        return NULL;

    return vfs_fds_x[fd];
}

extern struct file *(*vfs_fd_to_file) (int fd);

PATCH(vfs_fd_to_file, &vfs_fd_to_file, &patch_vfs_fd_to_file);


extern struct filesystem *(*vfs_find_filesystem)(const char *pathname);

int patch_os_open(const char *pathname, int flags, ...)
{
    struct file *file = NULL;
    struct filesystem *fs;
    int fd, ret;
    mode_t mode = 0;

    if ((fd = vfs_get_free_fd()) < 0) {
        errno = -fd;
        goto error;
    }
    if ((fs = vfs_find_filesystem(pathname)) == NULL) {
        errno = ENOENT;
        goto error;
    }
    /* Let the filesystem @fs open the file */
    ret = fs->open(&file, pathname, flags, mode);
    if (ret < 0 || file == NULL) {
        errno = -ret;
        goto error;
    }
    file_get(file);
    /* If file has its own open() callback, call it */
    if (file->f_fops && file->f_fops->open) {
        if ((ret = file->f_fops->open(file)) < 0) {
            file_put(file);
            vfs_free_file(file);
            errno = -ret;
            goto error;
        }
    }
    atomic_inc(&file->f_refcnt);
    file->f_flags = flags;
    file->f_fs = fs;
	/* Device file is special such that its instance
	 * will not actually be thrown away by close().
	 * Therefore we need to maintain f_path field
	 * to avoid memory leak.
	 */
	if (file->f_type != FTYPE_DEVICE_FILE)
		file->f_path = strdup(pathname);

    vfs_install_fd(fd, file);
    file_put(file);
    return fd;
  error:
	if (fd >= 0)
		vfs_free_fd(fd);
    return -1;
}

extern int (*os_open)(const char *pathname, int flags, ...);

PATCH(os_open, &os_open, &patch_os_open);

int patch_os_close(int fd)
{
    struct file *file = vfs_fd_to_file(fd);
    int ret = 0;

    if (file == NULL) {
        errno = EBADF;
        return -1;
    }
    file_get(file);
    vfs_free_fd(fd);
    file_put(file);

    /*
     * Clean up
     * It is the driver's responsibility to free file object if
     * it supports release() method.
     */
	/* We can't throw away a device file instance because
	 * it is a singleton.
	 * And we don't want to mandate defining release operation
	 * for every device unnecessarily.
	 */
    if (file->f_fops && file->f_fops->release)
        ret = file->f_fops->release(file);
    else if (file->f_type != FTYPE_DEVICE_FILE)
        vfs_free_file(file);
    if (ret < 0) {
        errno = -ret;
        ret = -1;
    }
    return ret;
}

extern int (*os_close)(int fd);

PATCH(os_close, &os_close, &patch_os_close);

void patch_vfs_destroy_file(struct file *file)
{
    osMutexAcquire(file->f_lock, osWaitForever);

    if (!atomic_dec_and_test(&file->f_refcnt)) {
        osMutexRelease(file->f_lock);
        return;
    }

    list_del_init(&file->f_list);
    if (file->f_path)
        kfree(file->f_path);
    osMutexRelease(file->f_lock);
    osMutexDelete(file->f_lock);
}

extern void (*vfs_destroy_file)(struct file * file);
PATCH(vfs_destroy_file, &vfs_destroy_file, &patch_vfs_destroy_file);

void patch_vfs_free_file(struct file *file)
{
    osMutexAcquire(file->f_lock, osWaitForever);

    if (!atomic_dec_and_test(&file->f_refcnt)) {
        osMutexRelease(file->f_lock);
        return;
    }

    list_del_init(&file->f_list);
    if (file->f_path)
        kfree(file->f_path);
    osMutexRelease(file->f_lock);
    osMutexDelete(file->f_lock);
    kfree(file);
}

extern void (*vfs_free_file)(struct file * file);
PATCH(vfs_free_file, &vfs_free_file, &patch_vfs_free_file);

extern int
_os_select(int nfds, fd_set * readfds, fd_set * writefds,
           fd_set * exceptfds, struct timeval *timeout);
int
patch_os_select(int nfds, fd_set * readfds, fd_set * writefds,
           fd_set * exceptfds, struct timeval *timeout)
{
	/* Is it legitimate to emulate sleep when nfds is zero.
	 */
	if (nfds == 0) {
		unsigned long usec = 0;
		int msec;

		if (timeout && (timeout->tv_sec > 0 || timeout->tv_usec > 0)) {
			usec = timeout->tv_sec * 1000000 + timeout->tv_usec;
		}

#define ceil(x,y) (((x) + (y) - 1) / (y))
		msec = ceil(usec, 1000);

		osDelay(pdMS_TO_TICKS(msec));
		return 0;
	}

	return _os_select(nfds, readfds, writefds, exceptfds, timeout);
}

extern  int (*os_select)(int nfds, fd_set * readfds, fd_set * writefds,
              fd_set * exceptfds, struct timeval * timeout);
PATCH(os_select, &os_select, &patch_os_select);


extern int _os_poll(struct pollfd *fds, nfds_t nfds, int timeout);

int patch_os_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{

	/* Is it legitimate to emulate sleep when nfds is zero.
	 */
	if (nfds == 0) {
		osDelay(pdMS_TO_TICKS(timeout));
		return 0;
	}

	return _os_poll(fds, nfds, timeout);
}

extern int
 (*os_poll)(struct pollfd * fds, nfds_t nfds, int timeout);
PATCH(os_poll, &os_poll, &patch_os_poll);

/* New: unmount, format */
/* XXX: to be included in the ROM library, i.e., vfs.c */

void os_unmount(const char * pathname)
{
    struct filesystem *fs;

    fs = vfs_find_filesystem(pathname);
    if (fs && fs->unmount)
        fs->unmount();
}

int os_format(const char * pathname)
{
    struct filesystem *fs;
    int ret = -EINVAL;

    fs = vfs_find_filesystem(pathname);
    if (!fs || !fs->format) {
        errno = ENOENT;
        ret = -1;
    }

    ret = fs->format();
    if (ret < 0) {
        errno = -ret;
        ret = -1;
    }

    return ret;
}

#endif

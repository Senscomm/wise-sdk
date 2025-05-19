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

#ifndef __WISE_VFS_H__
#define __WISE_VFS_H__

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdarg.h>
#include <fcntl.h>
#include <dirent.h>

#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/queue.h>

#include <freebsd/mutex.h>
#include <freebsd/atomic.h>

#include <hal/kernel.h>
#include <hal/device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FTYPE_GENERIC_FILE	0
#define FTYPE_DEVICE_FILE	1
#define FTYPE_SOCKET		2
#define FTYPE_VIRTUAL		3

struct file;
struct poll_table;
struct pollfd;

struct devfs_data
{
  struct list_head files;
  int nr_files;
};

/**
 * struct filesystem
 *
 * @source: source directory
 * @root: directory the filesystem is mounted to
 * @name: filesystem name
 * @fs_list: list of mounted filesystem
 * @fs_files: list of files in the filesystem
 */
struct filesystem
{
  const char *source;
  const char *root;
  const char *name;
  struct list_head fs_list;
  struct mtx lock;
  void *private;

  int (*mount) (struct filesystem *, const char *, const char *,
		unsigned long, const void *);
  int (*open) (struct file **, const char *, int, mode_t);
  int (*remove) (const char *pathname);
  int (*stat) (const char *, struct stat *);
  int (*rename) (const char *, const char *);
  DIR *(*opendir) (const char *);
  int (*closedir) (DIR *);
  struct dirent *(*readdir) (DIR *);
  void (*unmount) (void);
  int (*format) (void);
};

#define FILESYSTEM(_name_) \
	ll_entry_declare(struct filesystem, _name_, fs)

#define filesystem(_name_) \
	llsym(struct filesystem, _name_, fs)

#define filesystem_start() ll_entry_start(struct filesystem, fs)
#define filesystem_end()   ll_entry_end(struct filesystem, fs)

struct fops
{
  int (*open) (struct file *);
  int (*release) (struct file *);
    off_t (*llseek) (struct file *, off_t, int);
    ssize_t (*read) (struct file *, void *, size_t, off_t *);
    ssize_t (*write) (struct file *, void *, size_t, off_t *);
  unsigned (*poll) (struct file *, struct poll_table *, struct pollfd *);
  int (*ioctl) (struct file *, unsigned int, void *);
  int (*fcntl) (struct file *, int, va_list);
    ssize_t (*readv) (struct file *, const struct iovec *, int);
    ssize_t (*writev) (struct file *, const struct iovec *, int);
  /*int      (*fstat)(struct file *, struct stat *); */
  int (*fsync) (struct file *);
};

struct file
{
  char *f_path;
  struct fops *f_fops;
  struct list_head f_list;
  osMutexId_t f_lock;
  int f_type;			/* socket, device, or regular file */
  int f_flags;			/* given in open() */
  off_t f_pos;			/* offset */
  atomic_t f_refcnt;
  struct filesystem *f_fs;
  void *f_priv;			/* filesystem-specific data structure */
  int f_fd;			/* file descriptor number if > */
};

/* VFS functions */
void _vfs_lock (void);
void _vfs_unlock (void);

void _vfs_init_file (struct file *);
int _vfs_get_free_fd (void);
void _vfs_free_fd (int fd);
void _vfs_install_fd (int fd, struct file *file);
struct file *_vfs_fd_to_file (int fd);

void _vfs_init_file (struct file *file);
void _vfs_destroy_file (struct file *file);
struct file *_vfs_alloc_file (void);
void _vfs_free_file (struct file *file);

static inline void file_get (struct file *file)
{
  atomic_inc (&file->f_refcnt);
}

static inline void file_put (struct file *file)
{
  atomic_dec (&file->f_refcnt);
}

struct file *_vfs_register_device_file (char *name, struct fops *fops,
				       void *data);

struct filesystem *_vfs_find_filesystem (const char *);

struct poll_table
{
  QueueSetHandle_t qset;
  struct list_head head;
  int error;
};

int
_poll_add_wait (struct file *, QueueSetMemberHandle_t, struct poll_table *);

int devfs_mount(struct filesystem *fs, const char *source,
		       const char *target, unsigned long flags,
		       const void *data);
int devfs_open(struct file **filep, const char *pathname,
		      int flags, mode_t mode);
int devfs_stat(const char *pathname, struct stat *sb);
DIR *devfs_opendir(const char *pathname);
int devfs_closedir(DIR *dir);
struct dirent *devfs_readdir(DIR *dir);

extern struct file *(*vfs_alloc_file) (void);
extern int(*poll_add_wait) (struct file * filp, QueueSetMemberHandle_t wait,
		    struct poll_table * table);
extern struct file *(*vfs_register_device_file)(char *pathname, struct fops *fops, void *data);
extern void (*vfs_free_file) (struct file * file);
extern void (*vfs_init_file) (struct file * file);
extern void (*vfs_destroy_file) (struct file * file);
extern int (*os_close) (int fd);
extern int (*vfs_get_free_fd) (void);
extern void (*vfs_install_fd) (int fd, struct file * file);
extern void (*vfs_install_fd) (int fd, struct file * file);
extern void (*vfs_free_fd) (int fd);
extern struct file *(*vfs_fd_to_file) (int fd);

#ifdef __cplusplus
}
#endif

#endif

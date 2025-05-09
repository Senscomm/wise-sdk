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

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define __LINUX_ERRNO_EXTENSIONS__
#include <errno.h>

#include "spiffs.h"
#include "spiffs_nucleus.h"

#include "hal/types.h"
#include "hal/spi-flash.h"
#include <freebsd/mutex.h>
#include "vfs.h"

struct spiffs_priv {
	struct spi_flash *flash;
	struct mtx lock;
} spiffs_wise_data;

spiffs fs = {
	.user_data = &spiffs_wise_data,
};


/* Helper functions */
void spiffs_lock(void *data)
{
	spiffs *fs = data;
	struct spiffs_priv *priv = fs->user_data;
	mtx_lock(&priv->lock);
}

void spiffs_unlock(void *data)
{
	spiffs *fs = data;
	struct spiffs_priv *priv = fs->user_data;
	mtx_unlock(&priv->lock);
}


#define fentry(pf) { pf, SPIFFS_## pf }
static struct {
	int posix;
	int spiffs;
} fflags[] = {
	fentry(O_APPEND),
	fentry(O_TRUNC),
	fentry(O_CREAT),
	fentry(O_EXCL),
};

static int posix2spiffs_flags(int flags)
{
	int i, sflags = 0;
	int accmod;

	for (i = 0; i < ARRAY_SIZE(fflags); i++)
		if (flags & fflags[i].posix)
			sflags |= fflags[i].spiffs;

	accmod = flags & O_ACCMODE;
	if (accmod == O_RDWR)
		sflags |= SPIFFS_O_RDWR;
	else if (accmod == O_WRONLY)
		sflags |= SPIFFS_O_WRONLY;
	else
		sflags |= SPIFFS_O_RDONLY;

	return sflags;
}

#define EINDEX(err) (SPIFFS_ERR_NOT_MOUNTED-(err))

const int spiffs_err_to_errno[] = {
	[EINDEX(SPIFFS_ERR_NOT_MOUNTED)] = ENOENT,
	[EINDEX(SPIFFS_ERR_FULL)] = ENOSPC,
	[EINDEX(SPIFFS_ERR_NOT_FOUND)] = ENOENT,
	[EINDEX(SPIFFS_ERR_END_OF_OBJECT)] = EINVAL,
	[EINDEX(SPIFFS_ERR_DELETED)] = EBADFD,
	[EINDEX(SPIFFS_ERR_NOT_FINALIZED)] = EBADFD,
	[EINDEX(SPIFFS_ERR_NOT_INDEX)] = EBADFD,
	[EINDEX(SPIFFS_ERR_OUT_OF_FILE_DESCS)] = ENFILE,
	[EINDEX(SPIFFS_ERR_FILE_CLOSED)] = EBADF,
	[EINDEX(SPIFFS_ERR_FILE_DELETED)] = ENOENT,
	[EINDEX(SPIFFS_ERR_BAD_DESCRIPTOR)] = EBADF,

	[EINDEX(SPIFFS_ERR_IS_INDEX)] = EBADFD,
	[EINDEX(SPIFFS_ERR_IS_FREE)] = EBADFD,
	[EINDEX(SPIFFS_ERR_INDEX_SPAN_MISMATCH)] = EBADFD,
	[EINDEX(SPIFFS_ERR_DATA_SPAN_MISMATCH)] = EBADFD,

	/* Broken filesystem */
	[EINDEX(SPIFFS_ERR_INDEX_REF_FREE)] = EBADFD,
	[EINDEX(SPIFFS_ERR_INDEX_REF_LU)] = EBADFD,
	[EINDEX(SPIFFS_ERR_INDEX_REF_INVALID)] = EBADFD,
	[EINDEX(SPIFFS_ERR_INDEX_FREE)] = EBADFD,
	[EINDEX(SPIFFS_ERR_INDEX_LU)] = EBADFD,
	[EINDEX(SPIFFS_ERR_INDEX_INVALID)] = EBADFD,

	[EINDEX(SPIFFS_ERR_NOT_WRITABLE)] = EACCES,
	[EINDEX(SPIFFS_ERR_NOT_READABLE)] = EACCES,
	[EINDEX(SPIFFS_ERR_CONFLICTING_NAME)] = EEXIST,

	[EINDEX(SPIFFS_ERR_NOT_CONFIGURED)] = ENOENT,
	[EINDEX(SPIFFS_ERR_NOT_A_FS)] = EBADFD,
	[EINDEX(SPIFFS_ERR_MOUNTED)] = EBUSY, /* you can't format if mounted */
	[EINDEX(SPIFFS_ERR_ERASE_FAIL)] = EIO,
	[EINDEX(SPIFFS_ERR_MAGIC_NOT_POSSIBLE)] = EINVAL,
	[EINDEX(SPIFFS_ERR_NO_DELETED_BLOCKS)] = EINVAL,

	[EINDEX(SPIFFS_ERR_FILE_EXISTS)] = EEXIST,
	[EINDEX(SPIFFS_ERR_NOT_A_FILE)] = EBADFD,
	[EINDEX(SPIFFS_ERR_RO_NOT_IMPL)] = EACCES,
	[EINDEX(SPIFFS_ERR_RO_ABORTED_OPERATION)] = EACCES,
	[EINDEX(SPIFFS_ERR_PROBE_TOO_FEW_BLOCKS)] = EINVAL,
	[EINDEX(SPIFFS_ERR_PROBE_NOT_A_FS)] = EINVAL,
	[EINDEX(SPIFFS_ERR_NAME_TOO_LONG)] = ENAMETOOLONG,

	[EINDEX(SPIFFS_ERR_IX_MAP_UNMAPPED)] = EINVAL,
	[EINDEX(SPIFFS_ERR_IX_MAP_MAPPED)] = EINVAL,
	[EINDEX(SPIFFS_ERR_IX_MAP_BAD_RANGE)] = EINVAL,

	[EINDEX(SPIFFS_ERR_SEEK_BOUNDS)] = EINVAL,
};

static int spiffs_update_errno(void)
{
	if (fs.err_code == 0)
		return 0;

	errno = spiffs_err_to_errno[EINDEX(fs.err_code)];
	if (errno == 0) {
		printk("fs.err_code=%d, errno=%d\n", fs.err_code, errno);
	}
	assert(errno != 0);
	return 1;
}

static int spiffs_errno(int err)
{
	return !err ? 0 : spiffs_err_to_errno[EINDEX(err)];
}

static int spiffs2posix_stat(struct stat *posix, spiffs_stat *spiffs)
{
	memset(posix, 0, sizeof(*posix));

	posix->st_ino = spiffs->obj_id;
	posix->st_size = spiffs->size;

	/*
	 * spiffs_stat::type
	 * 0: file, 1: dir, 2: hardlink, 3: softlink
	 */
	switch (spiffs->type) {
	case 1:
		posix->st_mode = S_IFREG;
		break;
	case 2:
		posix->st_mode = S_IFDIR;
		break;
	case 3:
		posix->st_mode = S_IFLNK;
		break;
	default:
		return -1;
	}

	return 0;
}

/* File operations */

#define CALL(f)							\
{								\
	ret = f;						\
	if (ret) {						\
		ret = -spiffs_err_to_errno[EINDEX(ret)] : 0;	\
		goto error;					\
	}							\
}

static inline spiffs_file spiffs_get_fd(struct file *file)
{
	spiffs_fd *fdesc = file->f_priv;
	return SPIFFS_FH_OFFS(fdesc->fs, fdesc->file_nbr);
}

static inline off_t spiffs_get_pos(struct file *file)
{
	spiffs_fd *fdesc = file->f_priv;
	return fdesc->offset;
}

static
ssize_t vfs_spiffs_read(struct file *file, void *buf, size_t count, off_t *pos)
{
	spiffs_file fd = spiffs_get_fd(file);
	ssize_t ret;

	ret = SPIFFS_read(&fs, fd, buf, (s32_t) count);
	if (ret < 0)
		return -spiffs_errno(ret);
	if (pos)
		*pos = spiffs_get_pos(file);
	return ret;
}

static
ssize_t vfs_spiffs_write(struct file *file, void *buf, size_t count, off_t *pos)
{
	spiffs_file fd = spiffs_get_fd(file);
	ssize_t ret;

	ret = SPIFFS_write(&fs, fd, buf, (s32_t) count);
	if (ret < 0)
		return -spiffs_errno(ret);
	if (pos)
		*pos = spiffs_get_pos(file);
	return ret;
}

static
off_t vfs_spiffs_lseek(struct file *file, off_t offset, int whence)
{
	spiffs_file fd = spiffs_get_fd(file);
	static int swhence[] = {
		[SEEK_SET] = SPIFFS_SEEK_SET,
		[SEEK_CUR] = SPIFFS_SEEK_CUR,
		[SEEK_END] = SPIFFS_SEEK_END,
	};
	int ret;

	ret = SPIFFS_lseek(&fs, fd, offset, swhence[whence]);
	if (ret < 0)
		return  -spiffs_errno(ret);
	file->f_pos = (off_t) ret;
	return (off_t) ret;
}

static
int vfs_spiffs_release(struct file *file)
{
	spiffs_file fd = spiffs_get_fd(file);
	int ret;

	ret = SPIFFS_close(&fs, fd);
	vfs_free_file(file);
	return (ret < 0) ? -spiffs_errno(ret) : 0;
}
/*
static
int vfs_spiffs_fstat(struct file *file, struct stat *statbuf)
{
	spiffs_file fd = spiffs_get_fd(file);
	spiffs_stat sstat = {0, };
	int ret;

	ret = SPIFFS_fstat(&fs, fd, &sstat);
	if (ret < 0)
		return -spiffs_errno(ret);
	spiffs2posix_stat(statbuf, &sstat);
	return 0;
}
*/

static
int vfs_spiffs_fsync(struct file *file)
{
	spiffs_file fd = spiffs_get_fd(file);
	int ret;

	ret = SPIFFS_fflush(&fs, fd);
	return (ret < 0) ? -spiffs_errno(ret) : 0;
}

struct fops spiffs_fops = {
	.release = vfs_spiffs_release,
	.llseek  = vfs_spiffs_lseek,
	.read    = vfs_spiffs_read,
	.write   = vfs_spiffs_write,
	/*.fstat   = vfs_spiffs_fstat,*/
	.fsync    = vfs_spiffs_fsync,
};

/* Filesystem methods */
int vfs_spiffs_open(struct file **filep, const char *pathname,
			   int flags, mode_t mode)
{
	int fd, retval, sflags = posix2spiffs_flags(flags);
	struct file *file;
	spiffs_fd *spiffs_fd;

	fd = SPIFFS_open(&fs, pathname, sflags, mode);
	if (fd < 0) {
		retval = -spiffs_errno(fd);
		goto out;
	}
	retval = spiffs_fd_get(&fs, SPIFFS_FH_UNOFFS(&fs, fd), &spiffs_fd);
	if (retval != SPIFFS_OK) {
		retval = -spiffs_err_to_errno[EINDEX(retval)];
		goto out;
	}
	file = vfs_alloc_file();
	if (file == NULL) {
		retval = -ENOMEM;
		goto out;
	}
	file->f_priv = spiffs_fd;
	file->f_fops = &spiffs_fops;
	file->f_pos = 0;

	*filep = file;
 out:
	return retval;
}

int vfs_spiffs_remove(const char *pathname)
{
	if (SPIFFS_remove(&fs, pathname) < 0 &&
	    spiffs_update_errno())
		return -1;
	return 0;
}

int vfs_spiffs_stat(const char *pathname, struct stat *statbuf)
{
	spiffs_stat sstat = {0, };

	if (SPIFFS_stat(&fs, pathname, &sstat) < 0 &&
	    spiffs_update_errno())
		return -1;

	spiffs2posix_stat(statbuf, &sstat);
	return 0;
}

int vfs_spiffs_rename(const char *oldpath, const char *newpath)
{
	if (SPIFFS_rename(&fs, oldpath, newpath) < 0 &&
	    spiffs_update_errno())
		return -1;
	return 0;
}

struct vfs_spiffs_DIR {
	DIR dir;
	spiffs_DIR sdir;
};

struct vfs_spiffs_dirent {
	struct dirent dirent;
	struct spiffs_dirent sdirent;
};

DIR *vfs_spiffs_opendir(const char *name)
{
	struct vfs_spiffs_DIR *d;

	d = malloc(sizeof(*d));
	if (d == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	if (SPIFFS_opendir(&fs, name, &d->sdir) == NULL &&
	    spiffs_update_errno()) {
		free(d);
		return NULL;
	}

	return &d->dir;
}

int vfs_spiffs_closedir(DIR *dirp)
{
	struct vfs_spiffs_DIR *s_DIR;
	int ret = 0;

	s_DIR = container_of(dirp, struct vfs_spiffs_DIR, dir);
	if (SPIFFS_closedir(&s_DIR->sdir) < 0 &&
	    spiffs_update_errno())
		ret = -1;

	free(dirp);
	return ret;
}

struct dirent *vfs_spiffs_readdir(DIR *dirp)
{
	struct vfs_spiffs_DIR *s_DIR;
	static struct vfs_spiffs_dirent de = {{0}};
	struct spiffs_dirent *sdirent;

	s_DIR = container_of(dirp, struct vfs_spiffs_DIR, dir);

	sdirent = SPIFFS_readdir(&s_DIR->sdir, &de.sdirent);
	if (sdirent == NULL) {
		spiffs_update_errno();
		return NULL;
	}

	de.dirent.d_ino = sdirent->obj_id;
	de.dirent.d_type = sdirent->type;
	strncpy(de.dirent.d_name, (char *)sdirent->name, sizeof(de.dirent.d_name));

	return &de.dirent;
}

void vfs_spiffs_unmount(void)
{
	SPIFFS_unmount(&fs);
}

int vfs_spiffs_format(void)
{
	if (SPIFFS_format(&fs) < 0 &&
	    spiffs_update_errno())
		return -1;
	return 0;
}

/*
 * spiffs/wise interface
 */

static int hal_flash_read(spiffs *fs, u32_t addr, u32_t size, u8_t *dst)
{
	struct spiffs_priv *priv = fs->user_data;
	struct spi_flash *flash = priv->flash;
	off_t offset;

	offset = (off_t) addr - (off_t) flash->mem_base;
	return spi_flash_read(flash, offset, size, dst);
}

static int hal_flash_write(spiffs *fs, u32_t addr, u32_t size, u8_t *src)
{
	struct spiffs_priv *priv = fs->user_data;
	struct spi_flash *flash = priv->flash;
	off_t offset;
	int ret;
	offset = (off_t) addr - (off_t) flash->mem_base;
	ret = spi_flash_write(flash, offset, size, src);
	if(ret > 0)
		return SPIFFS_OK;
	return ret;
}

static int hal_flash_erase(spiffs *fs, u32_t addr, u32_t size)
{
	struct spiffs_priv *priv = fs->user_data;
	struct spi_flash *flash = priv->flash;
	off_t offset;

	offset = (off_t) addr - (off_t) flash->mem_base;
	return spi_flash_erase(flash, offset, size, 0);
}

/**
 * spiffs_mount() - initialize spiffs filesystem
 *
 */
int __spiffs_mount(flash_part_t *part)
{
	struct spi_flash *flash;
	int err;
	uint8_t *work = NULL, *fds = NULL, *cache = NULL;
	uint32_t cache_sz = 0, fds_sz, work_sz;
	spiffs_config config = {
		.hal_erase_f = 	hal_flash_erase,
		.hal_read_f = 	hal_flash_read,
		.hal_write_f = 	hal_flash_write,
	};

	flash = spi_flash_find_by_addr(part->start);
	if (!flash)
		return -ENODEV;

	config.phys_addr = (off_t) part->start;
	config.phys_size = part->size;

	config.phys_erase_block = flash_partition_erase_size(part);
	config.log_block_size = config.phys_erase_block;
	config.log_page_size = flash->page_size;
	if (!config.log_page_size)
		config.log_page_size = 256;
#if SPIFFS_FILEHDL_OFFSET
	config.fh_ix_offset = SPIFFS_FILEHDL_OFFSET;
#endif

	work_sz = config.log_page_size * 2;
	work = malloc(work_sz);
	ASSERT(work != NULL, "testbench work buffer could not be malloced");
	memset(work, 0, work_sz);

	fds_sz = CONFIG_SPIFFS_NUM_OPEN_FILES * sizeof(spiffs_fd);
	fds = malloc(fds_sz);
	ASSERT(fds != NULL, "testbench fd buffer could not be malloced");
	memset(fds, 0, fds_sz);

#if SPIFFS_CACHE
	cache_sz = sizeof(spiffs_cache);
	cache_sz += (CONFIG_SPIFFS_NUM_CACHE_PAGES
		     * (sizeof(spiffs_cache_page) + config.log_page_size));
	cache = malloc(cache_sz);
	ASSERT(cache != NULL, "testbench cache could not be malloced");
	memset(cache, 0, cache_sz);
#endif

	spiffs_wise_data.flash = flash;
	mtx_init(&spiffs_wise_data.lock, NULL, NULL, MTX_DEF);

	err = SPIFFS_mount(&fs, &config, work, fds, fds_sz, cache, cache_sz, NULL);
	if (err == 0)
		return 0;

#if 1 /* proper when flash partitions are not used. */
	if (err == SPIFFS_ERR_NOT_A_FS) {
		printk("retry spiffs after formatting\n");
		SPIFFS_format(&fs);
		err = SPIFFS_mount(&fs, &config, work, fds, fds_sz, cache, cache_sz, NULL);
        if (err == 0)
            return 0;
	}
#endif

	if (work)
		free(work);
	if (fds)
		free(fds);
	if (cache)
		free(cache);

	return -spiffs_err_to_errno[EINDEX(err)];
}

int vfs_spiffs_mount(struct filesystem *fs, const char *src, const char *dest,
		     unsigned long mountflags, const void *private)
{
#if 1 /* flash partitions are not used. */

	flash_part_t *part = fs->private;
	int ret;

	printk("VFS: spiffs mounting: 0x%08x, 0x%08x\n", part->start, part->size);

	ret = __spiffs_mount(part);
	if (ret == 0)
		return 0;

	return -EINVAL;
#else

	struct spi_flash *flash;
	flash_part_t *part;
	int ret;

	if (src) {
		part = flash_lookup_partition((char *)src, NULL);
		if (part == NULL) {
			printk("Could not find the partition %s\n", src);
			return -ENODEV;
		}
		return __spiffs_mount(part);
	}

	/* Try all the partitions */
	for_each_spi_flash(flash) {
		for_each_spi_flash_partition(flash, part) {
			ret = __spiffs_mount(part);
			printk("VFS: spiffs trying partition='%s: %s on %s %s'\n",
			       __func__, part->name, fs->root,
			       ret == 0? "succeeded" : "failed");

			if (ret == 0)
				return 0;
		}
	}
	return -EINVAL;
#endif
}

flash_part_t sys_part = {
    .start = CONFIG_SPIFFS_SYSTEM_PART_ADDR,
    .size = CONFIG_SPIFFS_SYSTEM_PART_SIZE,
};

FILESYSTEM(spiffs) = {
    .name     = "spiffs",
    .root     = "/",
    .private  = &sys_part,
    .fs_list  = LIST_HEAD_INIT(filesystem(spiffs)->fs_list),
    .mount    = vfs_spiffs_mount,
    .open 	  = vfs_spiffs_open,
    .remove   = vfs_spiffs_remove,
    .stat     = vfs_spiffs_stat,
    .rename   = vfs_spiffs_rename,
    .opendir  = vfs_spiffs_opendir,
    .closedir = vfs_spiffs_closedir,
    .readdir  = vfs_spiffs_readdir,
    .unmount  = vfs_spiffs_unmount,
    .format   = vfs_spiffs_format,
};

#if 0
#include <cli.h>

static int do_mount(int argc, char *argv[])
{
	flash_part_t *part;
	char fs[32];
	int c, ret;

	optind = 0;
	while ((c = getopt(argc, argv, "t:")) != -1) {
		switch (c) {
		case 't':
			strcpy(fs, optarg);
			break;
		default:
			printf("unknown option '-%c'\n", c);
			break;
		}
	}
	if (optind >= argc)
		return CMD_RET_USAGE;

	part = flash_lookup_partition(argv[optind], NULL);
	if (part == NULL) {
		printf("Could not find the partition %s\n", argv[optind]);
		return CMD_RET_FAILURE;
	}

	if (!strcmp(fs, "spiffs")) {
		ret = __spiffs_mount(part);
		if (ret < 0) {
			printf("mount failed: %s\n", strerror(-ret));
			return CMD_RET_FAILURE;
		}
	} else {
		printf("Filesystem %s not supported\n", fs);
		return CMD_RET_FAILURE;
	}
	return 0;
}

CMD(mount, do_mount, "mount a spiffs filesystem",
    "mount -t <filesystem> <partition name>");
#endif

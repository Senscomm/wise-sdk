/*
 * Copyright 2025-2026 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __VFS_SPIFFS_H__
#define __VFS_SPIFFS_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int vfs_spiffs_mount(struct filesystem *fs, const char *src, const char *dest,
		     unsigned long mountflags, const void *private);
extern int vfs_spiffs_open(struct file **filep, const char *pathname,
			   int flags, mode_t mode);
extern int vfs_spiffs_remove(const char *pathname);
extern int vfs_spiffs_stat(const char *pathname, struct stat *statbuf);
extern int vfs_spiffs_rename(const char *oldpath, const char *newpath);
extern DIR *vfs_spiffs_opendir(const char *name);
extern int vfs_spiffs_closedir(DIR *dirp);
extern struct dirent *vfs_spiffs_readdir(DIR *dirp);
extern void vfs_spiffs_unmount(void);
extern int vfs_spiffs_format(void);

#ifdef __cplusplus
}
#endif

#endif

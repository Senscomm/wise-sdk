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

#ifndef __WISE_DIRENT_H__
#define __WISE_DIRENT_H__

typedef struct __dirstream {
	void *dir;
} DIR;

struct dirent {
	unsigned d_ino;
	unsigned d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[128];
};


extern DIR *(*os_opendir) (const char *name);
extern int (*os_closedir) (DIR * dirp);
extern struct dirent *(*os_readdir) (DIR * dirp);
#ifdef CONFIG_LINK_TO_ROM
extern void (os_unmount) (const char *pathname);
extern int (os_format) (const char *pathname);
#else
#if 0 /* Will be enabled at the next Romization */
extern void (*os_unmount) (const char *pathname);
extern int (*os_format) (const char *pathname);
#endif
#endif

#define opendir os_opendir
#define closedir os_closedir
#define readdir os_readdir
#define unmount os_unmount
#define fformat os_format


#endif /* __WISE_DIRENT_H__ */

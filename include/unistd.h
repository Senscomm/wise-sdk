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

#ifndef __WISE_UNISTD_H__
#define __WISE_UNISTD_H__

#if defined(__USE_NATIVE_HEADER__)
#include_next <unistd.h>

//#if !defined(__USE_NATIVE_HEADER__)
#else

#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <getopt.h>

#define alarm(s)

#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int      (*os_open)  (const char *pathname, int flags, ...);
extern int      (*os_creat) (const char *pathname, mode_t mode);
extern int      (*os_close) (int fd);
extern int      (*os_stat)  (const char *pathname, struct stat * statbuf);
extern int      (*os_fstat) (int fd, struct stat * statbuf);
extern int      (*os_fsync) (int fd);
extern ssize_t  (*os_read)  (int fd, void *buf, size_t count);
extern ssize_t  (*os_write) (int fd,const void *buf, size_t count);
extern off_t    (*os_lseek) (int fd, off_t offset, int whence);
extern int      (*os_dup)   (int fd);
extern int      (*os_select)(int nfds, fd_set * readfds, fd_set * writefds,
		                     fd_set * exceptfds, struct timeval * timeout);

#define open(pathname, flags, ...)          (*os_open)(pathname, flags, ##__VA_ARGS__)
#define creat(pathname, mode)               (*os_creat)(pathname, mode)
#define close(fd)                           (*os_close)(fd)
#define stat(pathname, statbuf)             (*os_stat)(pathname, statbuf)
#define fstat(fd, statbuf)                  (*os_fstat)(fd, statbuf)
#define fsync(fd)                           (*os_fsync)(fd)
#define read(fd, buf, count)                (*os_read)(fd, buf, count)
#define write(fd, buf, count)               (*os_write)(fd, buf, count)
#define lseek(fd, offset, whence)           (*os_lseek)(fd, offset, whence)
#define select(fd, rfds, wfds, exfds, tv)   (*os_select)(fd, rfds, wfds, exfds, tv)
#define dup(fd)                             (*os_dup)(fd)

#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#ifdef __cplusplus
}
#endif

#endif /* __USE_NATIVE_HEADER__ */

#endif

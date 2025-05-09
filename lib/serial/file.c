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

#define __USE_NATIVE_HEADER__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <hal/types.h>
#include <u-boot/xyzModem.h>
#include <cli.h>

static int fd;
char fname[128];

int xyzModem_stream_open(connection_info_t *info, int *err)
{
#undef open
	cli_readline("Enter the file name: ", fname, sizeof(fname));

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		printf("open: %s\n", strerror(errno));
		return fd;
	}
	return 0;
}

void xyzModem_stream_close(int *err)
{
#undef close
	if (fd > 0)
		close(fd);
	fd = 0;
}

int xyzModem_stream_read(char *buf, int size, int *err)
{
#undef read
	return read(fd, buf, size);
}

void xyzModem_stream_terminate(bool metohd, int (*getc)(void))
{

}

char *xyzModem_error(int err)
{
	return strerror(errno);
}

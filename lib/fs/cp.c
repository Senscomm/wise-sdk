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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include "cli.h"

static int do_cp(int argc, char *argv[])
{
	char *src, *dst, buf[128];
	int fdin, fdout, ret = 0;
	ssize_t len;

	if (argc != 3)
		return CMD_RET_USAGE;

	src = argv[1];
	dst = argv[2];

	fdin = open(src, O_RDONLY, 0);
	if (fdin < 0) {
		printf("%s: '%s': %s\n",
		       argv[0], src, strerror(errno));
		return CMD_RET_FAILURE;
	}

	fdout = open(dst, O_RDWR|O_CREAT|O_TRUNC, 0);
	if (fdout < 0) {
		printf("%s: '%s': %s\n",
		       argv[0], dst, strerror(errno));
		return CMD_RET_FAILURE;
	}

	while (1) {
		len = read(fdin, buf, sizeof(buf));
		if (len < 0) {
			printf("%s: '%s': %s\n",
			       argv[0], src, strerror(errno));
			ret = CMD_RET_FAILURE;
			break;
		} else if (len == 0)
			break;

		if (write(fdout, buf, len) < 0) {
			printf("%s: '%s': %s\n",
			       argv[0], dst, strerror(errno));
			ret = CMD_RET_FAILURE;
			break;
		}
	}
	close(fdin);
	close(fdout);

	if (ret != 0)
		remove(dst);

	return CMD_RET_FAILURE;
}
CMD(cp, do_cp,
    "copy files",
    "cp SOURCE DEST");

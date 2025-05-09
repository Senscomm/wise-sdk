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

static int do_cat(int argc, char *argv[])
{
	int fd, i, n;
	char buf[128];

	if (argc < 2)
		return CMD_RET_USAGE;

	for (i = 1; i < argc; i++) {
		fd = open(argv[i], O_RDONLY, 0);
		if (fd < 0) {
			printf("%s: '%s': %s\n",
			       argv[0], argv[i], strerror(errno));
			continue;
		}
		while ((n = read(fd, buf, sizeof(buf))) > 0) {
			char *p = buf;
			while (p - buf < n)
				putchar(*p++);
		}

		close(fd);
	}
	return 0;
}
CMD(cat, do_cat,
    "concatenate files and print on the standard output",
    "cat [OPTIONS]... [FILE]...");

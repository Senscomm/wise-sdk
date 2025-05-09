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

static int do_ls(int argc, char *argv[])
{
	DIR *dir;
	struct dirent *entry;
	const char *top = "/";

	if (argc == 2)
		top = argv[1];

	dir = opendir(top);
	if (!dir) {
		printf("ls: %s\n", strerror(errno));
		return CMD_RET_FAILURE;
	}
	while ((entry = readdir(dir))) {
		char ftype;
		struct stat sb = {0, };

		if (stat((char *) entry->d_name, &sb) < 0) {
			printf("%s: could not stat for '%s': %s\n",
			       argv[0], entry->d_name, strerror(errno));
			return CMD_RET_FAILURE;
		}

		switch (sb.st_mode & S_IFMT) {
		case S_IFREG:
			ftype = 'f';
			break;
		case S_IFDIR:
			ftype = 'd';
			break;
		case S_IFLNK:
			ftype = 's';
			break;
		case S_IFCHR:
			ftype = 'c';
			break;
		case S_IFSOCK:
			ftype = 's';
			break;
		default:
			ftype = '?';
			break;
		}

		printf("%c %6lu %s\n", ftype, sb.st_size, entry->d_name);
	}
	closedir(dir);

	return 0;
}
CMD(ls, do_ls,
    "list directory contents",
    "ls [OPTIONS]... [FILE]...");

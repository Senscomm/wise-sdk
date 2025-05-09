/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include "cli.h"
#include "u-boot/xyzModem.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define XYZM_BUFSZ	(1024)

static int getcxmodem(void) {
    int ret = getchar_timeout(0);
    if (ret >= 0)
        return (char) ret;

    return -1;
}

static int scm_fs_load(const char *filename)
{
    int fd;
    int size = 0, err, res;
    char *buf = NULL;

    connection_info_t info = {
        .mode = xyzModem_ymodem,
    };

    if (filename[0] != '/') {
        printf("filename %s must start with root\"/\"\n", filename);
        return -1;
    }

    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        printf("open %s fail\n", filename);
        return -1;
    }

    buf = malloc(XYZM_BUFSZ);
    if (buf == NULL) {
        printf("No memory available for receiving\n");
        return -1;
    }

    if ((res = xyzModem_stream_open(&info, &err)) != 0) {
        printf("%s\n", xyzModem_error(err));
        goto out;
    }

    while ((res = xyzModem_stream_read(buf, XYZM_BUFSZ, &err)) > 0) {
        int rc;

        rc = write(fd, buf, res);

        if (rc < 0) {
            printf("Flash program failed for %s\n", filename);
            err = -1;
            goto out;
        }

        size += res;
        /* Reading less than 1024 bytes is not necessarily an error. */
        err = 0;
    }

    if (err == xyzModem_timeout && !(size % XYZM_BUFSZ)) {
        /* False timeout at the end. */
        err = 0;
    }

    printf("## Total Size = 0x%08x = %d Bytes\n", size, size);
out:
    xyzModem_stream_close(&err);
    xyzModem_stream_terminate(false, getcxmodem);
    if (buf != NULL)
        free(buf);

    if (fd >= 0)
    {
        close(fd);
    }

    return (err == 0) ? 0: 1;
}

static int scm_fs_write(const char *filename, char *buf, int len)
{
    int fd;
    int ret = -1;

    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        printf("open fail\n");
        goto errout;
    }

    ret = write(fd, buf, len);

errout:

    if (fd >= 0)
    {
        close(fd);
    }

    return ret;
}

static int scm_fs_read(const char *filename, char *buf, int len)
{
    int fd;
    int ret = -1;

    fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        printf("open fail\n");
        goto errout;
    }

    ret = read(fd, buf, len);

errout:

    if (fd >= 0)
    {
        close(fd);
    }

    return ret;
}

static long int scm_fs_size(const char *filename)
{
    struct stat sb = {0, };

    stat(filename, &sb);

    return sb.st_size;
}

static int scm_fs_unmount(const char *pathname)
{
    unmount(pathname);

    return 0;
}

static int scm_fs_format(const char *pathname)
{
    int ret;

    ret = fformat(pathname);

    return ret;
}

static int scm_cli_fs_load(int argc, char *argv[])
{
    if (argc != 2) {
        return CMD_RET_USAGE;
    }

    const char *name = argv[1];

    printf("load local file to %s\n", name);

    scm_fs_load(name);

    return CMD_RET_SUCCESS;
}

static int scm_cli_fs_read(int argc, char *argv[])
{
    const char *name = argv[1];
    char *buf;
    int size;

    if (argc != 2) {
        return CMD_RET_USAGE;
    }

    printf("read %s\n", name);

    size = scm_fs_size(name);
    buf = (char*)malloc(size+1);

    if(buf == NULL) {
        printf("No memory available for read\n");
        return CMD_RET_USAGE;
    }

    buf[size] = '\0';
    scm_fs_read(name, buf, size);
    printf("size: %d\n%s\n", size, buf);

    free(buf);

    return CMD_RET_SUCCESS;
}

static int scm_cli_fs_write(int argc, char *argv[])
{
    const char *name = argv[1];

    if (argc != 3) {
        return CMD_RET_USAGE;
    }

    printf("write %s\n", name);
    printf("size: %d\n%s\n", strlen(argv[2]), argv[2]);

    scm_fs_write(name, argv[2], strlen(argv[2]));

    return CMD_RET_SUCCESS;
}

static int scm_cli_fs_rm(int argc, char *argv[])
{
    const char *name = argv[1];

    if (argc != 2) {
        return CMD_RET_USAGE;
    }

    printf("rm %s\n", name);

    remove(name);

    return CMD_RET_SUCCESS;
}

static int scm_cli_fs_size(int argc, char *argv[])
{
    const char *name = argv[1];

    if (argc != 2) {
        return CMD_RET_USAGE;
    }

    printf("file size: %lu\n", scm_fs_size(name));

    return CMD_RET_SUCCESS;
}

static int scm_cli_fs_unmount(int argc, char *argv[])
{
    const char *path = argv[1];

    if (argc != 2) {
        return CMD_RET_USAGE;
    }

    scm_fs_unmount(path);

    return CMD_RET_SUCCESS;
}

static int scm_cli_fs_format(int argc, char *argv[])
{
    const char *path = argv[1];

    if (argc != 2) {
        return CMD_RET_USAGE;
    }

    if (scm_fs_format(path)) {
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_fs_cmd[] = {
    CMDENTRY(load , scm_cli_fs_load, "", ""),
    CMDENTRY(read , scm_cli_fs_read, "", ""),
    CMDENTRY(write , scm_cli_fs_write, "", ""),
    CMDENTRY(rm , scm_cli_fs_rm, "", ""),
    CMDENTRY(size , scm_cli_fs_size, "", ""),
    CMDENTRY(umount , scm_cli_fs_unmount, "", ""),
    CMDENTRY(format , scm_cli_fs_format, "", ""),
};

static int do_fs(int argc, char *argv[])
{
    const struct cli_cmd *cmd;

    argc--;
    argv++;

    cmd = cli_find_cmd(argv[0], scm_fs_cmd, ARRAY_SIZE(scm_fs_cmd));
    if (cmd == NULL)
        return CMD_RET_USAGE;

    return cmd->handler(argc, argv);
}

CMD(fs, do_fs,
        "CLI for scm_fs operations",
        "fs load <filename>" OR
        "fs read <filename>" OR
        "fs write <filename> <content>" OR
        "fs rm <filename>" OR
        "fs size <filename>" OR
        "fs umount <pathname>" OR
        "fs format <pathname>"
   );

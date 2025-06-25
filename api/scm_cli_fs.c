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
#include "scm_fs.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

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

    scm_fs_rm(name);

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

static int scm_cli_fs_cwrite(int argc, char *argv[])
{
    if (argc != 4)
        return CMD_RET_USAGE;

    const char *ns = argv[1];
    const char *key = argv[2];
    const char *data = argv[3];

    printf("cwrite [%s_%s]: %s\n", ns, key, data);

    if (scm_fs_write_config_value(ns, key, data, strlen(data)) < 0)
        return CMD_RET_FAILURE;

    return CMD_RET_SUCCESS;
}

static int scm_cli_fs_cread(int argc, char *argv[])
{
    if (argc != 3)
        return CMD_RET_USAGE;

    const char *ns = argv[1];
    const char *key = argv[2];
    char buf[256] = {0};

    int len = scm_fs_read_config_value(ns, key, buf, sizeof(buf) - 1);
    if (len < 0)
        return CMD_RET_FAILURE;

    buf[len] = '\0';
    printf("cread [%s_%s]: %s\n", ns, key, buf);

    return CMD_RET_SUCCESS;
}

static int scm_cli_fs_crm(int argc, char *argv[])
{
    if (argc != 3)
        return CMD_RET_USAGE;

    const char *ns = argv[1];
    const char *key = argv[2];

    printf("crm [%s_%s]\n", ns, key);

    if (scm_fs_remove_config_value(ns, key) < 0)
        return CMD_RET_FAILURE;

    return CMD_RET_SUCCESS;
}

static int scm_cli_fs_cexist(int argc, char *argv[])
{
    if (argc != 3)
        return CMD_RET_USAGE;

    const char *ns = argv[1];
    const char *key = argv[2];

    if (scm_fs_exists_config_value(ns, key) == 0)
    {
        printf("[%s_%s] exists.\n", ns, key);
        return CMD_RET_SUCCESS;
    }

    printf("[%s_%s] does not exist.\n", ns, key);
    return CMD_RET_FAILURE;
}

static const struct cli_cmd scm_fs_cmd[] = {
    CMDENTRY(load , scm_cli_fs_load, "", ""),
    CMDENTRY(read , scm_cli_fs_read, "", ""),
    CMDENTRY(write , scm_cli_fs_write, "", ""),
    CMDENTRY(rm , scm_cli_fs_rm, "", ""),
    CMDENTRY(size , scm_cli_fs_size, "", ""),
    CMDENTRY(umount , scm_cli_fs_unmount, "", ""),
    CMDENTRY(format , scm_cli_fs_format, "", ""),
    CMDENTRY(cwrite , scm_cli_fs_cwrite, "", ""),
    CMDENTRY(cread , scm_cli_fs_cread, "", ""),
    CMDENTRY(crm , scm_cli_fs_crm, "", ""),
    CMDENTRY(cexist , scm_cli_fs_cexist, "", ""),
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
        "fs format <pathname>" OR
        "fs cwrite <ns> <key> <val>" OR
        "fs cread <ns> <key>" OR
        "fs crm <ns> <key>" OR
        "fs cexist <ns> <key>"
   );

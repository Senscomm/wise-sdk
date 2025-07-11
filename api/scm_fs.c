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

#include "scm_fs.h"
#include "u-boot/xyzModem.h"

#define XYZM_BUFSZ	(1024)

static int getcxmodem(void) {
    int ret = getchar_timeout(0);
    if (ret >= 0)
        return (char) ret;

    return -1;
}

int scm_fs_load(const char *filename)
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

int scm_fs_write(const char *filename, const char *buf, int len, bool log)
{
    int fd;
    int ret = -1;

    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        if (log)
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

int scm_fs_read(const char *filename, char *buf, int len, bool log)
{
    int fd;
    int ret = -1;

    fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        if (log)
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

int scm_fs_rm(const char *filename)
{
    if (filename == NULL)
        return -1;

    return remove(filename);
}

long int scm_fs_size(const char *filename)
{
    struct stat sb = {0, };

    stat(filename, &sb);

    return sb.st_size;
}

int scm_fs_unmount(const char *pathname)
{
    unmount(pathname);

    return 0;
}

int scm_fs_format(const char *pathname)
{
    int ret;

    ret = fformat(pathname);

    return ret;
}


#define CONFIG_DIR "/config"
#define CONFIG_PATH_MAX 128

int scm_fs_clear_all_config_value(void)
{
    DIR * dir;
    struct dirent * entry;
    char path[CONFIG_PATH_MAX];

    dir = opendir(CONFIG_DIR);
    if (dir == NULL)
    {
        printf("Failed to open %s\n", CONFIG_DIR);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "%s", entry->d_name);

        if (remove(path) == 0)
        {
            printf("Deleted config file: %s\n", path);
        }
        else
        {
            printf("Failed to delete: %s\n", path);
        }
    }

    closedir(dir);
    return 0;
}

int scm_fs_read_config_value(const char *ns, const char *key, char *buf, int len)
{
    char path[CONFIG_PATH_MAX] = {0};

    if (ns == NULL || key == NULL || buf == NULL || len <= 0)
        return -1;

    snprintf(path, sizeof(path), CONFIG_DIR "/%s_%s", ns, key);

    return scm_fs_read(path, buf, len, false);
}

int scm_fs_write_config_value(const char *ns, const char *key, const char *buf, int len)
{
    char path[CONFIG_PATH_MAX] = {0};

    if (ns == NULL || key == NULL || buf == NULL || len <= 0)
        return -1;

#if 0
    printf("ns: %s\n", ns);
    printf("key: %s\n", key);
    printf("buf: %s\n", buf);
    printf("len: %d\n", len);
#endif

    snprintf(path, sizeof(path), CONFIG_DIR "/%s_%s", ns, key);

    return scm_fs_write(path, buf, len, true);
}

int scm_fs_remove_config_value(const char *ns, const char *key)
{
    char path[CONFIG_PATH_MAX] = {0};

    if (ns == NULL || key == NULL)
        return -1;

    snprintf(path, sizeof(path), CONFIG_DIR "/%s_%s", ns, key);

    return scm_fs_rm(path);
}

int scm_fs_exists_config_value(const char *ns, const char *key)
{
    char path[CONFIG_PATH_MAX] = {0};

    if (ns == NULL || key == NULL)
        return -1;

    snprintf(path, sizeof(path), CONFIG_DIR "/%s_%s", ns, key);

#if defined(HAVE_ACCESS)
    return (access(path, F_OK) == 0) ? 0 : -1;
#else
    struct stat st;
    return (stat(path, &st) == 0) ? 0 : -1;
#endif
}

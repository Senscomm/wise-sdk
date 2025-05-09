#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/task.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>

#include <hal/device.h>
#include <hal/wdt.h>

#include <cmsis_os.h>
#include "hal/console.h"
#include "hal/io.h"
#include "cli.h"
#include "mmap.h"

#include "flash_map_backend/flash_map_backend.h"
#include "sysflash/sysflash.h"

#include <bootutil/bootutil_public.h>
#include <bootutil/bootutil.h>
#include <bootutil/image.h>

#define MAX_BUF_LEN         (1024 * 2)
#define MAX_WRITE_BUF_LEN   (1024 * 4)

extern void flash_crypto_enable(uint8_t enable);

static int sockfd;
static char *hbuf;
static char *fbuf;

uint32_t img_oft;
uint32_t fbuf_idx;

const struct flash_area *fap;

#define HTTP_DEBUG          1
#define HTTP_DEBUG_PACKET   1

#define SECODARY_HEADER_OFFSET	CONFIG_SCM2010_OTA_SECONDARY_SLOT_OFFSET

int check_magic(void)
{
    struct image_header *hdr = (struct image_header *)SECODARY_HEADER_OFFSET;

    if (hdr->ih_magic == IMAGE_MAGIC) {
        printf("boot magic verified\n");
        return 0;
    } else {
        printf("boot magic incorrect\n");
    }

    /* TODO: what about mcuboot formatted image, but not ours? */

    return -1;
}


int file_open(void)
{
    return flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), &fap);
}

int file_close(void)
{
    int ret = 0;

    if (fbuf_idx) {

        if (img_oft == 0) {
            if (check_magic()) {
                return -1;
            }
        }

        ret = flash_area_write(fap, img_oft, (const void *)fbuf, fbuf_idx);
        if (ret < 0) {
            printf("flash_area_write failed\n");
        }

        img_oft += fbuf_idx;
    }

    flash_area_close(fap);

    return ret;
}

int file_write(void *buf, int len)
{
    int ret = 0;

    if (fbuf_idx + len >= MAX_WRITE_BUF_LEN) {
        uint32_t remain;
        uint32_t copy_len;
        uint8_t *buf_ptr = buf;

        remain = (fbuf_idx + len) - MAX_WRITE_BUF_LEN;
        copy_len = len - remain;

        memcpy(&fbuf[fbuf_idx], buf_ptr, copy_len);

        ret = flash_area_write(fap, img_oft, (const void *)fbuf, MAX_WRITE_BUF_LEN);
        if (ret < 0) {
            printf("flash_area_write failed\n");
            return -1;
        }

        /* when writing the flash for the first time, check the validity
         * it is always the case that the size of the fbuf is
         * already greater than the image header
         */
        if (img_oft == 0) {
            if (check_magic()) {
                return -1;
            }
        }

        img_oft += MAX_WRITE_BUF_LEN;
        fbuf_idx = 0;

        if (remain) {
            memcpy(fbuf, buf_ptr + copy_len, remain);
            fbuf_idx += remain;
        }
    } else {
        memcpy(&fbuf[fbuf_idx], buf, len);
        fbuf_idx += len;
    }

    return ret;
}

/* TODO: should check for case insensitive, spaces, tabs, etc... */

static int http_process_header(char *buf, int len, int *status)
{
    int i;
    char *status_start;
    char *status_end;
    char status_str[8];

    if (strncmp(buf, "HTTP ", 4) != 0) {
        return -1;
    }

    status_start = strchr(buf, ' ');
    status_end = strchr(status_start + 1, ' ');
    memcpy(status_str, status_start, status_end  - status_start);
    status_str[status_end - status_start] = '\0';
    *status = atoi(status_str);

    for (i = 0; i < len; i++) {
        if (i >= 4) {
            if (buf[i - 0] == '\n' &&
                buf[i - 1] == '\r' &&
                buf[i - 2] == '\n' &&
                buf[i - 3] == '\r') {
                return i;
            }
        }
    }

    return -1;
}

static int http_get_tag(char *buf, int len, char *tag, int *val_len)
{
    int i = 0;

    while (i < len) {
        int start = i;
        int match = 0;
#if HTTP_DEBUG
        char tmp[256];
#endif

        if (strncmp(&buf[i], tag, strlen(tag)) == 0) {
            match = 1;
        }

        while (1) {
            i++;
            if (i >= len) {
                break;
            }
            if (buf[i - 0] == '\n' &&
                buf[i - 1] == '\r') {
                break;
            }
        }

        if ((i - start) == 2) {
            return -1;
        }

#if HTTP_DEBUG
        memcpy(tmp, &buf[start], i - start - 1);
        tmp[i - start - 1] = '\0';
        printf("[%s]\n", tmp);
#endif

        if (match) {
            /* return the tag value, and set tag value length */
            *val_len = i - start - 1 - strlen(tag);
            return start + strlen(tag);
        }
        i++;
    }

    return -1;
}

static int http_file_size(char *buf, int len)
{
    char line[16];
    int pos;
    int tag_len;

    pos = http_get_tag(buf, len, "Content-Length:", &tag_len);
    if (pos < 0) {
        return -1;
    }

    memcpy(line, &buf[pos], tag_len);
    line[tag_len] = '\0';

    return atoi(line);
}

static int http_recv_rsp(void)
{
    int len;
    int status;
    int hdr_len;
    int offset;
    int file_size;
    int ret = 0;

    /* get header fields : what if not within this buffer? */
    len = recv(sockfd, hbuf, MAX_BUF_LEN, 0);
    if (len <= 0) {
        return -1;
    }

    /* parse and get header size */
    hdr_len = http_process_header(hbuf, len, &status);
    if (hdr_len < 0) {
        printf("error: header\n");
        return -1;
    }

    /* check status */
    if (status != 200) {
        printf("error: status = %d\n", status);
        return -1;
    }

    /* get file size */
    file_size = http_file_size(hbuf, hdr_len);
    if (file_size == 0) {
        printf("error: cannot get file size\n");
        return -1;
    }

    /* check the initial buffer */
    printf("buf   : %d\n", len);
    printf("header: %d\n", hdr_len);
    printf("file  : %d\n", file_size);

    if (hdr_len > len) {
        printf("header not enough\n");
        return -1;
    }

    /* copy body part */
    file_open();
    ret = file_write(&hbuf[hdr_len + 1], len - (hdr_len + 1));
    offset = len - (hdr_len + 1);
    if (ret) {
        goto out;
    }

    /* receive the rest of the body */
    while (offset < file_size) {
        len = recv(sockfd, hbuf, MAX_BUF_LEN, 0);
        if (len <= 0) {
            break;
        }

        offset += len;
        ret = file_write(hbuf, len);
        if (ret) {
            goto out;
        }

#if HTTP_DEBUG_PACKET
        printf("Received: %-8d %d%%\n", offset, (offset * 100) / file_size);
#endif
    }

out:
    ret = file_close();

    return ret;
}

static int http_send_req(struct sockaddr_in serv, char *path)
{
    int ret;

    sprintf(hbuf,
            "GET /%s HTTP/1.1\r\n"
            "Accept: */*\r\n"
            "User-Agent: wise\r\n"
            "Host: %s:%d\r\n"
            "\r\n",
            path, inet_ntoa(serv.sin_addr),
            ntohs(serv.sin_port));

    ret = send(sockfd, hbuf, strlen(hbuf), 0);

    return ret;
}

int connect_timeout(struct sockaddr_in *serv, int timeout_ms)
{
	struct pollfd pfd;
	int ret;
	int flags;

	if (timeout_ms == -1) {
    	ret = connect(sockfd, (struct sockaddr*)serv, sizeof(*serv));
		return ret;
	}

	flags = fcntl(sockfd, F_GETFL, 0);
	ret = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	if (ret == -1) {
		return -1;
	}

    ret = connect(sockfd, (struct sockaddr*)serv, sizeof(*serv));
	if (ret < 0 && errno != EINPROGRESS) {
		printf("connect error: %d-%s\n", errno, strerror(errno));
		return -1;
	}

	pfd.fd = sockfd;
	pfd.events = POLLOUT;
    ret = poll(&pfd, 1, -1);
	if (ret < 0) {
		return -1;
	} else if (ret == 0) {
		return -1;
	} else {
        int so_error;
        socklen_t len = sizeof so_error;

        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

        if (so_error != 0) {
			printf("socket error %d-%s\n", so_error, strerror(so_error));
			return -1;
		}

		ret = fcntl(sockfd, F_SETFL, flags);
		if (ret == -1) {
			return -1;
		}
	}

    return 0;
}

static int get_firmware(char *addr, uint16_t port, char *path)
{
    struct sockaddr_in serv;
    int ret = -1;

    printf("firmware file: [%s:%d] [%s]\n", addr, port, path);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    hbuf = malloc(MAX_BUF_LEN);
    if (!hbuf) {
        printf("error: allocating http buffer\n");
        goto error;
    }

    fbuf = malloc(MAX_WRITE_BUF_LEN);
    if (!fbuf) {
        printf("error: allocating flash buffer\n");
        goto error;
    }

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(addr);
    serv.sin_port = htons(port);

    printf("connect to server, sock=%d\n", sockfd);

    ret = connect_timeout(&serv, -1);
    if (ret != 0) {
        printf("error: connecting %d\n", ret);
        goto error;
    }

    printf("send request\n");

    ret = http_send_req(serv, path);
    if (ret < 0) {
        printf("error: sending request\n");
        goto error;
    }

    printf("receive response\n");

    ret = http_recv_rsp();
    if (ret < 0) {
        printf("error: receiving response\n");
        goto error;
    }

    printf("complete\n");

error:

    close(sockfd);

    if (hbuf) {
        free(hbuf);
    }
    if (fbuf) {
        free(fbuf);
    }

    return ret;
}

static void check_slot_trailer(void)
{
    struct image_trailer trailer;
    uint8_t erase_val;
    int i;

    flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), &fap);

    flash_area_read(fap, fap->fa_size - sizeof(struct image_trailer),
            &trailer, sizeof(struct image_trailer));

    erase_val = flash_area_erased_val(fap);

    for (i = 0; i < 16; i++) {
        if (trailer.magic[i] != erase_val) {
            flash_area_erase(fap, fap->fa_size - sizeof(struct image_trailer),
                    sizeof(struct image_trailer));
            break;
        }
    }

    flash_area_close(fap);
}

static int do_mcuboot_agent(int argc, char *argv[])
{
    char *url = argv[1];
    char *addr;
    char *port;
    char *path;
    int ret;
    uint16_t port_num;
    struct device *wdt;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    if (strncmp(url, "http://", 7) != 0) {
        printf("error: invalid protocol\n");
        return CMD_RET_USAGE;
    }

    addr = url + 7;
    path = strchr(addr, '/');
    if (path) {
        *path++ = '\0';
    } else {
        printf("error: invalid path\n");
        return CMD_RET_USAGE;
    }

    port = strchr(addr, ':');
    if (port) {
        *port++ = '\0';
        port_num = atoi(port);
    } else {
        port_num = 80;
    }

    img_oft = 0;
    fbuf_idx = 0;

    check_slot_trailer();

    flash_crypto_enable(0);

    ret = get_firmware(addr, port_num, path);

    flash_crypto_enable(1);

    if (ret < 0) {
        return CMD_RET_FAILURE;
    }

    boot_set_pending_multi(0, 0);

    wdt = device_get_by_name("atcwdt");
    if (wdt) {
        wdt_expire_now(wdt);
    }

    return 0;
}

CMD(mcuboot_agent, do_mcuboot_agent,
    "MCUBoot update agent",
    "mcuboot_agent [URL]"
);

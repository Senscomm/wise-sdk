/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/log.h>
#include <al/al_persist.h>
#include <al/al_os_mem.h>

#include "scm_flash.h"

#define PERSIST_PATH_ROOT 		"/ayla"
#define PERSIST_PATH_STARTUP	"startup"
#define PERSIST_PATH_FACTORY	"factory"
#define PERSIST_PATH_SECTION(section) ( \
			section == AL_PERSIST_STARTUP ? \
			PERSIST_PATH_STARTUP : PERSIST_PATH_FACTORY \
			)
#define PERSIST_PATH_MAX_LEN	64

#define AYLA_FACTORY_INFO_PART_ADDR  CONFIG_FACTORY_PARTITION_OFFSET
/* Total size = 4K, ayla oem info = 1K, matter section = 2K, optional for future use = 1k */
#define AYLA_FACTORY_INFO_PART_SIZE CONFIG_FACTORY_PARTITION_SIZE
#define AYLA_FACTORY_INFO_PART_NON_AYLA_OFFSET      0x0400
#define AYLA_FACTORY_INFO_PART_MATTER_SECTION_SIZE  0x0800

struct al_persist_factory_info {
    uint8_t dsn[20];            /* factory/id/dev_id */
    uint8_t dsn_pubkey[400];    /* factory/id/key */
    uint8_t oem[20];            /* factory/oem/oem */
    uint8_t oem_model[24];      /* factory/oem/model */
    uint8_t oem_key[256];       /* factory/oem/key */
    uint8_t serial[32];
} __packed;

static int al_persist_read_system_partition(enum al_persist_section section,
        const char *name, struct al_persist_factory_info **info,
        uint8_t **data, int *sz)
{
    if (section != AL_PERSIST_FACTORY) {
        return -1;
    }

    if (strcmp(name, "id/dev_id")
            && strcmp(name, "id/key")
            && strcmp(name, "oem/oem")
            && strcmp(name, "oem/model")
            && strcmp(name, "oem/key")) {
        return -1;
    }

    *info = (struct al_persist_factory_info *)al_os_mem_alloc(sizeof(**info));
    if (*info == NULL) {
		log_put(LOG_ERR "al_os_mem_alloc failed\n");
        return -1;
    }

    if (scm_partition_read(FLASH_PARTITION_FACTORY, 0, *info, sizeof(**info)) < 0) {
		log_put(LOG_ERR "flash_read failed\n");
        al_os_mem_free(*info);
        return -1;
    }

    if (!strcmp(name, "id/dev_id")) {
        *data = (*info)->dsn;
        *sz = sizeof((*info)->dsn);
    } else if (!strcmp(name, "id/key")) {
        *data = (*info)->dsn_pubkey;
        *sz = sizeof((*info)->dsn_pubkey);
    } else if (!strcmp(name, "oem/oem")) {
        *data = (*info)->oem;
        *sz = sizeof((*info)->oem);
    } else if (!strcmp(name, "oem/model")) {
        *data = (*info)->oem_model;
        *sz = sizeof((*info)->oem_model);
    } else if (!strcmp(name, "oem/key")) {
        *data = (*info)->oem_key;
        *sz = sizeof((*info)->oem_key);
    }

    return 0;
}

static int al_persist_data_read_from_system_partition(enum al_persist_section section,
		const char *name, void *buf, size_t len)
{
    struct al_persist_factory_info *info;
    uint8_t *data;
    int sz;

    if (al_persist_read_system_partition(section, name, &info, &data, &sz) < 0) {
        return -1;
    }

    sz = (sz > len) ? len : sz;
    memcpy(buf, data, sz);
    al_os_mem_free(info);

    return sz;
}

static int al_persist_data_write_to_system_partition(enum al_persist_section section,
		const char *name, const void *buf, size_t len)
{
    struct al_persist_factory_info *info;
    uint8_t *data;
    int sz;
    uint8_t magic[4];
    bool has_matter = false;
    /* Now, we only care about 2K matter data, for 1K optional part, need additional logic for future use */
    uint8_t *matter_section;

    if (al_persist_read_system_partition(section, name, &info, &data, &sz) < 0) {
        return -1;
    }

    memset(data, 0, sz);
    sz = (sz > len) ? len : sz;
    memcpy(data, buf, sz);

    if (scm_partition_read(FLASH_PARTITION_FACTORY, AYLA_FACTORY_INFO_PART_NON_AYLA_OFFSET, magic, sizeof(magic)) >= 0) {
        /* Check Magic, Matter magic: "MATT" */
        if (magic[3] == 0x4d && magic[2] == 0x41 && magic[1] == 0x54 && magic[0] == 0x54) {
            has_matter = true;
        }
    } 

    if (has_matter) {
        matter_section = (uint8_t *)al_os_mem_alloc(AYLA_FACTORY_INFO_PART_MATTER_SECTION_SIZE);
        if (matter_section == NULL) {
            log_put(LOG_ERR "al_os_mem_alloc failed\n");
            al_os_mem_free(info);
            return -1;
        }

        if (scm_partition_read(FLASH_PARTITION_FACTORY, AYLA_FACTORY_INFO_PART_NON_AYLA_OFFSET, matter_section, AYLA_FACTORY_INFO_PART_MATTER_SECTION_SIZE) < 0) {
            log_put(LOG_ERR "flash_read failed\n");
            al_os_mem_free(info);
            al_os_mem_free(matter_section);
            return -1;
        }
    }

    if (scm_partition_erase(FLASH_PARTITION_FACTORY, 0, AYLA_FACTORY_INFO_PART_SIZE) < 0) {
        al_os_mem_free(info);
        if (has_matter) {
            al_os_mem_free(matter_section);
        }
        return -1;
    }

    if (scm_partition_write(FLASH_PARTITION_FACTORY, 0, info, sizeof(*info)) < 0) {
        al_os_mem_free(info);
        if (has_matter) {
            al_os_mem_free(matter_section);
        }
        return -1;
    }

    al_os_mem_free(info);

    if (has_matter) {
        if (scm_partition_write(FLASH_PARTITION_FACTORY, AYLA_FACTORY_INFO_PART_NON_AYLA_OFFSET, matter_section, AYLA_FACTORY_INFO_PART_MATTER_SECTION_SIZE) < 0) {
            al_os_mem_free(matter_section);
            return -1;
        }

        al_os_mem_free(matter_section);
    }

    return 0;
}

enum al_err al_persist_data_write(enum al_persist_section section,
		const char *name, const void *buf, size_t len)
{
	char path[PERSIST_PATH_MAX_LEN];
	int fd;
	int ret;

	log_put(LOG_DEBUG "persist write: %d/%s\n", section, name);

    if (al_persist_data_write_to_system_partition(section, name, buf, len) == 0) {
        return 0;
    }

	sprintf(path, "%s/%s/%s",
		PERSIST_PATH_ROOT,
		PERSIST_PATH_SECTION(section),
		name);

	fd = open(path, O_CREAT | O_WRONLY);
	if (fd < 0)
		return -1;

	if (len == 0) {
		/* Erase content */
		close(fd);
		remove(path);
		return 0;
	}

	ret = write(fd, buf, len);
	if (ret < 0) {
		log_put(LOG_ERR "write %s failed: %s (%d)\n", name, strerror(errno), errno);
	}

	close(fd);

	return ret > 0 ? AL_ERR_OK : AL_ERR_ERR;
}

ssize_t al_persist_data_read(enum al_persist_section section,
		const char *name, void *buf, size_t len)
{
	char path[PERSIST_PATH_MAX_LEN];
	int fd;
	int ret;

	log_put(LOG_DEBUG "persist read: %d/%s\n", section, name);

    if ((ret = al_persist_data_read_from_system_partition(section, name, buf, len)) > 0) {
        return ret;
    }

	sprintf(path, "%s/%s/%s",
		PERSIST_PATH_ROOT,
		PERSIST_PATH_SECTION(section),
		name);

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -AL_ERR_NOT_FOUND;

	ret = read(fd, buf, len);
	if (ret < 0) {
		log_put(LOG_ERR "read %s failed: %s (%d)\n", name, strerror(errno), errno);
	}

	close(fd);

	return ret;
}

enum al_err al_persist_data_erase(enum al_persist_section section)
{
	char path[PERSIST_PATH_MAX_LEN];
	DIR *dir;
	struct dirent *entry;
	const char *top = "/";

	log_put(LOG_DEBUG "persist erase: %d\n", section);

	sprintf(path, "%s/%s",
		PERSIST_PATH_ROOT,
		PERSIST_PATH_SECTION(section));

	dir = opendir(top);
	if (!dir) {
		return AL_ERR_ERR;
	}

	while ((entry = readdir(dir))) {
		struct stat sb = {0, };

		if (strncmp(entry->d_name, path, strlen(path)) != 0) {
			continue;
		}

		if (stat((char *) entry->d_name, &sb) < 0) {
			printf("could not stat for '%s': %s\n",
			       entry->d_name, strerror(errno));
			return AL_ERR_ERR;
		}

		if (remove(entry->d_name) != 0) {
			return AL_ERR_ERR;
		}
		printf("%s removed\n", entry->d_name);
	}
	closedir(dir);

	return AL_ERR_OK;
}

void al_persist_data_load_done(void)
{
}

void al_persist_data_save_done(void)
{
}

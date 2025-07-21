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

#define PERSIST_PATH_ROOT 		"/ayla"
#define PERSIST_PATH_STARTUP	"startup"
#define PERSIST_PATH_FACTORY	"factory"
#define PERSIST_PATH_SECTION(section) ( \
			section == AL_PERSIST_STARTUP ? \
			PERSIST_PATH_STARTUP : PERSIST_PATH_FACTORY \
			)
#define PERSIST_PATH_MAX_LEN	64

enum al_err al_persist_data_write(enum al_persist_section section,
		const char *name, const void *buf, size_t len)
{
	char path[PERSIST_PATH_MAX_LEN];
	int fd;
	int ret;

	log_put(LOG_DEBUG "persist write: %d/%s\n", section, name);

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


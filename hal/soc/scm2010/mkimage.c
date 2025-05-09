#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "head.h"

unsigned int crc32 (unsigned int crc, const char *p, unsigned int len);

int main(int argc, char *argv[])
{
	int c, fd, tfd, debug = 0, type = 0, head = 0;
	char *file = NULL, *endp;
	unsigned vma = -1, lma = -1, ver = -1, size = 0;
	img_hdr_t *hdr;

	while ((c = getopt(argc, argv, "f:v:l:s:r:dth")) != -1) {
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 't':
			type = 1;
			break;
		case 'h':
			head = 1;
			break;
		case 'f':
			file = optarg;
			break;
		case 'v':
			vma = strtoul(optarg, &endp, 0);
			if (*endp != '\0') {
				fprintf(stderr, "%s: invalid VMA %s\n",
					argv[0], optarg);
				goto usage;
			}
			break;
		case 'l':
			lma = strtoul(optarg, &endp, 0);
			if (*endp != '\0') {
				fprintf(stderr, "%s: invalid LMA %s\n",
					argv[0], optarg);
				goto usage;
			}
			break;
		case 's':
			size = strtoul(optarg, &endp, 0);
			if (*endp != '\0') {
				fprintf(stderr, "%s: invalid size %s\n",
					argv[0], optarg);
				goto usage;
			}
			break;
		case 'r':
			ver = strtoul(optarg, &endp, 0);
			if (*endp != '\0') {
				fprintf(stderr, "%s: invalid VER %s\n",
					argv[0], optarg);
				goto usage;
			}
			break;
		default:
			goto usage;
		}
	}

	if (file == NULL || vma == -1 || lma == -1)
		goto usage;

	if ((fd = open(file, O_RDWR)) < 0) {
		fprintf(stderr, "open: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	if (size == 0) {
		/* size is equal to length of the image body */
		struct stat st;
		stat(file, &st);
		if (head == 1) {
			/* adding new header, the size is just size of the file */
			size = st.st_size;
		} else {
			/* already has the header, the header size must be subtracted from the file size */
			size = st.st_size - sizeof(*hdr);
		}
	}

	if (head) {
		unsigned char buf[1024];
		ssize_t sz;

		if ((tfd = open("__tmp__", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
			fprintf(stderr, "open: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}

		lseek(tfd, sizeof(*hdr), SEEK_SET);
		do {
			sz = read(fd, buf, sizeof(buf));
			if (write(tfd, buf, sz) < 0) {
				fprintf(stderr, "write: %s\n", strerror(errno));
				return EXIT_FAILURE;
			}
		} while (sz > 0);
		close(fd);
		rename("__tmp__", file);
		fd = tfd;
	}

	hdr = mmap(NULL, sizeof(*hdr) + size, PROT_READ|PROT_WRITE,
		   MAP_SHARED, fd, 0);
	if (hdr == (void *)-1) {
		fprintf(stderr, "mmap: %s\n", strerror(errno));
		goto usage;
	}

	hdr->bhdr.hcrc32 = 0;
	if (head) {
		hdr->bhdr.magic = 0x31707848;
	}

	hdr->bhdr.size = uswap_32(size);
	hdr->bhdr.lma = uswap_32(lma);
	hdr->bhdr.vma = uswap_32(vma);
	hdr->bhdr.ver = uswap_32(ver);
	hdr->bhdr.type = type;
	hdr->bhdr.dcrc32 = uswap_32(crc32(0, (void *) (hdr + 1), size));
	hdr->bhdr.hcrc32 = uswap_32(crc32(0, (void *) hdr, sizeof(boot_hdr_t)));

	if (debug) {
		printf("magic=0x%x, type=%02x\n",
				hdr->bhdr.magic, hdr->bhdr.type);
		printf("hcrc=%08x\n", hdr->bhdr.hcrc32);
		printf("lma=%08x, vma=%08x, size=%08x\n",
		       hdr->bhdr.lma, hdr->bhdr.vma, hdr->bhdr.size);
		printf("dcrc=%08x\n", hdr->bhdr.dcrc32);
	}

	munmap(hdr, sizeof(*hdr) + size);
	fsync(fd);
	close(fd);
	return EXIT_SUCCESS;

 usage:
	fprintf(stderr,
		"Usage: %s -f <file> -l <lma> -v <vma> -s <size> (-t)\n",
		argv[0]);

	exit(EXIT_FAILURE);
}

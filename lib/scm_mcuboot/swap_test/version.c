
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

#include <cmsis_os.h>
#include "hal/console.h"
#include "cli.h"

#include "flash_map_backend/flash_map_backend.h"
#include "sysflash/sysflash.h"

#include <bootutil/bootutil_public.h>

#define begin_packed_struct
#define end_packed_struct   __attribute__((packed))

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BOOT_HEADER_MAGIC_V1 0x96f3b83d
#define BOOT_HEADER_SIZE_V1  32

/****************************************************************************
 * Private Types
 ****************************************************************************/

begin_packed_struct struct mcuboot_raw_version_v1_s
{
    uint8_t  major;
    uint8_t  minor;
    uint16_t revision;
    uint32_t build_num;
} end_packed_struct;

begin_packed_struct struct mcuboot_raw_header_v1_s
{
	uint32_t header_magic;
	uint32_t image_load_address;
	uint16_t header_size;
	uint16_t pad;
	uint32_t image_size;
	uint32_t image_flags;
	struct mcuboot_raw_version_v1_s version;
	uint32_t pad2;
} end_packed_struct;

begin_packed_struct struct mcuboot_raw_trailer_v1_s
{
	uint32_t swap_size;
	uint32_t reserved0[1];
	uint8_t swap_type;
	uint8_t reserve1[7];
	uint8_t copy_done;
	uint8_t reserve2[7];
	uint8_t image_ok;
	uint8_t reserve3[7];
	uint32_t magic[4];
} end_packed_struct;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int boot_read_v1_header(uint8_t area_id,
		struct mcuboot_raw_header_v1_s *raw_header)
{
	const struct flash_area *fa;
	int rc;

	rc = flash_area_open(area_id, &fa);
	if (rc) {
		return rc;
	}

	rc = flash_area_read(fa, 0, raw_header,
			sizeof(struct mcuboot_raw_header_v1_s));
	flash_area_close(fa);
	if (rc) {
		return rc;
	}

	raw_header->header_magic       = le32toh(raw_header->header_magic);
	raw_header->image_load_address = le32toh(raw_header->image_load_address);
	raw_header->header_size        = le16toh(raw_header->header_size);
	raw_header->image_size         = le32toh(raw_header->image_size);
	raw_header->image_flags        = le32toh(raw_header->image_flags);
	raw_header->version.revision   = le16toh(raw_header->version.revision);
	raw_header->version.build_num  = le32toh(raw_header->version.build_num);

	return 0;
}

static void show_header_info(struct mcuboot_raw_header_v1_s *header)
{
	printf("-header-\n");
	printf("magic    : 0x%08x\n", header->header_magic);
	printf("load     : 0x%08x\n", header->image_load_address);
	printf("size     : 0x%08x\n", header->image_size);
	printf("flag     : 0x%08x\n", header->image_flags);
	printf("version  : %d.%d.%d.%ld\n", header->version.major,
										header->version.minor,
										header->version.revision,
										header->version.build_num);
}

static int boot_read_v1_trailer(uint8_t area_id,
		struct mcuboot_raw_trailer_v1_s *raw_trailer)
{
	const struct flash_area *fa;
	int rc;

	rc = flash_area_open(area_id, &fa);
	if (rc) {
		return rc;
	}

	rc = flash_area_read(fa, fa->fa_size - sizeof(struct mcuboot_raw_trailer_v1_s), raw_trailer,
			sizeof(struct mcuboot_raw_trailer_v1_s));
	flash_area_close(fa);
	if (rc) {
		return rc;
	}

	return 0;
}

static void show_trailer_info(struct mcuboot_raw_trailer_v1_s *trailer)
{
	printf("-trailer-\n");
	printf("swap size: 0x%08x\n", trailer->swap_size);
	printf("swap type: 0x%02x\n", trailer->swap_type);
	printf("copy done: 0x%02x\n", trailer->copy_done);
	printf("image ok : 0x%02x\n", trailer->copy_done);
	printf("magic0   : 0x%08x\n", trailer->magic[0]);
	printf("magic1   : 0x%08x\n", trailer->magic[1]);
	printf("magic2   : 0x%08x\n", trailer->magic[2]);
	printf("magic3   : 0x%08x\n", trailer->magic[3]);
}

static int do_mcuboot_version(int argc, char *argv[])
{
	int rc;
	struct mcuboot_raw_header_v1_s raw_header;
	struct mcuboot_raw_trailer_v1_s raw_trailer;

	rc = boot_read_v1_header(0, &raw_header);
	if (rc) {
		return rc;
	}

	rc = boot_read_v1_trailer(0, &raw_trailer);
	if (rc) {
		return rc;
	}

	printf("\nImage 0\n");
	show_header_info(&raw_header);
	show_trailer_info(&raw_trailer);

	rc = boot_read_v1_header(1, &raw_header);
	if (rc) {
		return rc;
	}

	rc = boot_read_v1_trailer(1, &raw_trailer);
	if (rc) {
		return rc;
	}

	printf("\nImage 1\n");
	show_header_info(&raw_header);
	show_trailer_info(&raw_trailer);

	return 0;
}

CMD(mcuboot_version, do_mcuboot_version,
		"MCUBoot version",
		"mcuboot_version"
   );

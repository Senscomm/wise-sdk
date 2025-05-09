#include <hal/kernel.h>
#include <hal/device.h>
#include <cli.h>
#include <version.h>

#include <stdlib.h>
#include <stdio.h>

__dram__ const char version_string[] = WISE_VERSION_STRING;

#ifdef CONFIG_CMD_VERSION

extern struct device __device_tab_start, __device_tab_end;

static int do_version(int argc, char *argv[])
{
	struct device *dev;
	struct driver *drv;
	char ver[128] = {0, };

	printf("%s\n", version_string);
	printf(CC_VERSION_STRING "\n");
	printf(LD_VERSION_STRING "\n");
	printf(GIT_TAG_STRING "\n");
	printf(GIT_VERSION_STRING "\n");
	printf(GIT_API_VERSION_STRING "\n");
	for (dev = &__device_tab_start; dev < &__device_tab_end; dev++)	{
		drv = dev->driver;
		if (!drv || !drv->version)
			continue;
		drv->version(dev, ver, ARRAY_SIZE(ver));
		printf("[%s] %s\n", dev_name(dev), ver);
	}

	return 0;
}

CMD(version, do_version,
	"display wise, compiler and linker version",
	""
);

#endif

#if defined(CONFIG_HOSTBOOT) || defined(CONFIG_SUPPORT_SCDC)

__attribute__((weak)) void soc_get_revinfo(struct sncmf_rev_info *revinfo) {}
__attribute__((weak)) void board_get_revinfo(struct sncmf_rev_info *revinfo) {}

void sncmf_get_revinfo(struct sncmf_rev_info *revinfo)
{
	char ucoderev[10], *gitver = GIT_VERSION_STRING;

	soc_get_revinfo(revinfo);
	board_get_revinfo(revinfo);

	memset(ucoderev, 0, sizeof(ucoderev));
	ucoderev[0] = '0';
	ucoderev[1] = 'x';
	memcpy(&ucoderev[2], gitver, 8);
	revinfo->ucoderev = strtoul(ucoderev, NULL, 16);
}

#endif


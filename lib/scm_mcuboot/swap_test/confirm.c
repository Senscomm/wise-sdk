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


static int do_mcuboot_confirm(int argc, char *argv[])
{
    boot_set_confirmed_multi(0);

    return 0;
}

CMD(mcuboot_confirm, do_mcuboot_confirm,
    "MCUBoot confirm",
    "mcuboot_confirm"
);

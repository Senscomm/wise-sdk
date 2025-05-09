#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>

#include <cmsis_os.h>
#include "hal/console.h"
#include "hal/io.h"
#include "cli.h"

extern void mcuboot_main(void);

static int do_mcuboot_loader(int argc, char *argv[])
{
    mcuboot_main();

    return 0;
}

CMD(mcuboot_loader, do_mcuboot_loader,
    "MCUBoot loader",
    "mcuboot_loader"
);

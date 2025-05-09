/*
 * iperf, Copyright (c) 2014, 2015, 2017, The Regents of the University of
 * California, through Lawrence Berkeley National Laboratory (subject
 * to receipt of any required approvals from the U.S. Dept. of
 * Energy).  All rights reserved.
 *
 * If you have questions about your rights to use or distribute this
 * software, please contact Berkeley Lab's Technology Transfer
 * Department at TTD@lbl.gov.
 *
 * NOTICE.  This software is owned by the U.S. Department of Energy.
 * As such, the U.S. Government has been granted for itself and others
 * acting on its behalf a paid-up, nonexclusive, irrevocable,
 * worldwide license in the Software to reproduce, prepare derivative
 * works, and perform publicly and display publicly.  Beginning five
 * (5) years after the date permission to assert copyright is obtained
 * from the U.S. Department of Energy, and subject to any subsequent
 * five (5) year renewals, the U.S. Government is granted for itself
 * and others acting on its behalf a paid-up, nonexclusive,
 * irrevocable, worldwide license in the Software to reproduce,
 * prepare derivative works, distribute copies to the public, perform
 * publicly and display publicly, and to permit others to do so.
 *
 * This code is distributed under a BSD style license, see the LICENSE
 * file for complete information.
 */
#include "iperf_config.h"

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
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <cmsis_os.h>

#include "iperf.h"
#include "iperf_api.h"
#include "units.h"
#include "iperf_locale.h"
#include "net.h"

#include "cli.h"

static int run(struct iperf_test *test);

/**************************************************************************/
int
iperf3_main(int argc, char **argv)
{
    struct iperf_test *test;
    int ret = CMD_RET_SUCCESS;

    // XXX: Setting the process affinity requires root on most systems.
    //      Is this a feature we really need?
#ifdef TEST_PROC_AFFINITY
    /* didn't seem to work.... */
    /*
     * increasing the priority of the process to minimise packet generation
     * delay
     */
    int rc = setpriority(PRIO_PROCESS, 0, -15);

    if (rc < 0) {
        perror("setpriority:");
        fprintf(stderr, "setting priority to valid level\n");
        rc = setpriority(PRIO_PROCESS, 0, 0);
    }

    /* setting the affinity of the process  */
    cpu_set_t cpu_set;
    int affinity = -1;
    int ncores = 1;

    sched_getaffinity(0, sizeof(cpu_set_t), &cpu_set);
    if (errno)
        perror("couldn't get affinity:");

    if ((ncores = sysconf(_SC_NPROCESSORS_CONF)) <= 0)
        err("sysconf: couldn't get _SC_NPROCESSORS_CONF");

    CPU_ZERO(&cpu_set);
    CPU_SET(affinity, &cpu_set);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) != 0)
        err("couldn't change CPU affinity");
#endif

    test = iperf_new_test();
    if (!test)
        iperf_errexit(NULL, "create new test error - %s", iperf_strerror(i_errno));
    iperf_defaults(test);	/* sets defaults */

    if (iperf_parse_arguments(test, argc, argv) < 0) {
        iperf_err(test, "parameter error - %s", iperf_strerror(i_errno));
        usage_long();
    }

    if (run(test) < 0) {
        iperf_errexit(test, "error - %s", iperf_strerror(i_errno));
        ret = CMD_RET_FAILURE;
    }

    if (!test->daemon) {
        iperf_free_test(test);
    }

    return ret;
}

/* Reserve each 2 ports for STA and SAP modes */
#define MAX_USED_SERVERPORTS 4
static uint16_t used_serverports[MAX_USED_SERVERPORTS] = {0};

static int serverports_alloc(uint16_t server_port)
{
	int i;

	for (i = 0; i < MAX_USED_SERVERPORTS; i++) {
		if (used_serverports[i] == server_port) {
			return 1;
		}
	}

	for (i = 0; i < MAX_USED_SERVERPORTS; i++) {
		if (used_serverports[i] == 0) {
			used_serverports[i] = server_port;
			break;
		}
	}

	return 0;
}

static void serverports_free(uint16_t server_port)
{
	int i;

	for (i = 0; i < MAX_USED_SERVERPORTS; i++) {
		if (used_serverports[i] == server_port) {
			used_serverports[i] = 0;
			break;
		}
	}
}

static int _iperf_server_thread(struct iperf_test *test)
{
    int rc = 0;

    for (;;) {
		rc = iperf_run_server(test);
		if (rc < 0) {
		    struct iperf_stream *sp;

		    /* Close all stream sockets */
		    SLIST_FOREACH(sp, &test->streams, streams) {
		        close(sp->socket);
		    }
		    iperf_err(test, "error - %s", iperf_strerror(i_errno));
		    if (rc < -1) {
		        iperf_errexit(test, "exiting");
			break;
		    }
		}
		iperf_reset_test(test);
		if (iperf_get_test_one_off(test))
		    break;
		if (i_errno == IESERVERTERM) {
		    i_errno = IENONE;
		    break;
		}
	}

	serverports_free(test->server_port);
	return rc;
}

static void iperf_server_thread(void *arg)
{
    struct iperf_test *test = arg;
    int exitcode;

    exitcode = _iperf_server_thread(test);

    /* for the case of Daemon, we should free resource here to avoid memory leak */
    iperf_free_test(test);

    osThreadExit();
    (void)exitcode;
}

static const osThreadAttr_t attr = {
	.name = "iperf_server",
	.stack_size = 6 * 1024,
	.priority = osPriorityNormal,
};

/**************************************************************************/
static int
run(struct iperf_test *test)
{
	switch (test->role) {
	case 's':
		if (serverports_alloc(test->server_port)) {
			iperf_errexit(test, "error - %s", iperf_strerror(IELISTEN));
			/* daemon = 0: free resource to avoid memory leak */
			test->daemon = 0;
			break;
		}
		if (test->daemon)
			osThreadNew(iperf_server_thread, test, &attr);
		else
			_iperf_server_thread(test);
		break;
	case 'c':
		if (iperf_run_client(test) < 0) {
			struct iperf_stream *sp;

			/* Close all stream sockets */
			SLIST_FOREACH(sp, &test->streams, streams) {
				close(sp->socket);
			}
			iperf_errexit(test, "error - %s", iperf_strerror(i_errno));
		}
		break;
	default:
		usage();
		break;
	}

	if (!test->daemon) {
		/* Close control socket */
		if (test->ctrl_sck) {
			iperf_got_sigend(test);
			close(test->ctrl_sck);
		}
	}

	return 0;
}

CMD(iperf3, iperf3_main,
    "A TCP, UDP, and SCTP network bandwidth measurement tool",
    usage_longstr);

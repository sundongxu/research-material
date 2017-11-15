/*
 * BUILD COMMAND:
 *   gcc -Wall -O2 -o RDMA_RC_example -libverbs hello_world_rc_send.c
 *
 * Copyright (c) 2011 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id: basic_test_flow.c 3720 2011-07-23 12:35:42Z gsivan $ 
 * 
 */

/******************************************************************************
*
*           RDMA Aware Networks Programming Example
*
*  This code demonstrates how to perform the following operations using the
*  VPI Verbs API:
*
*           Send
*           Receive
*           RDMA Read
*           RDMA Write
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include "sock.h"

/* poll CQ timeout in milisec (2 seconds) */
#define MAX_POLL_CQ_TIMEOUT 2000
#define MSG "hello world"
#define MSG_SIZE (strlen(MSG) + 1)

/* structure of test parameters */
struct config_t {
	const char			*dev_name;	/* IB device name */
	char				*server_name;	/* daemon host name */
	u_int32_t			tcp_port;	/* daemon TCP port */
	int				ib_port;	/* local IB port to work with */
};

/* structure of needed test resources */
struct resources {

/********* To Do ***********/

	int				sock;		/* TCP socket file descriptor */
};

struct config_t config = {
	"mlx4_0",			/* dev_name */
	NULL,				/* server_name */
	19875,				/* tcp_port */
	1				/* ib_port */
};

/******************************************************************************
* Function: resources_init
*
* Input
*   res                     pointer to resources structure
*
* Output
*   res                     is initialized
*
* Returns
*   none
*
* Description
*   res is initialized to default values
******************************************************************************/

static void resources_init(struct resources *res)
{
	memset(res, 0, sizeof *res);
	res->sock     = -1;
}

/******************************************************************************
* Function: resources_create
*
* Input
*   res                     pointer to resources structure to be filled in
*
* Output
*   res                     filled in with resources
*
* Returns
*   0 on success, 1 on failure
*
* Description
*
*  This function creates and allocates all necessary system resources.  These
*  are stored in res.
*****************************************************************************/
/* To Do */
static int resources_create(struct resources *res)
{	

	/* if client side */
	if (config.server_name) {
		res->sock = sock_client_connect(config.server_name, config.tcp_port);
		if (res->sock < 0) {
			fprintf(stderr, "failed to establish TCP connection to server %s, port %d\n", 
				config.server_name, config.tcp_port);
			return -1;
		}
	} else {
		fprintf(stdout, "waiting on port %d for TCP connection\n", config.tcp_port);

		res->sock = sock_daemon_connect(config.tcp_port);
		if (res->sock < 0) {
			fprintf(stderr, "failed to establish TCP connection with client on port %d\n", 
				config.tcp_port);
			return -1;
		}
	}

	fprintf(stdout, "TCP connection was established\n");

	fprintf(stdout, "searching for IB devices in host\n");

	/* get device names in the system */

	/* if there isn't any IB device in host */

	/* search for the specific device we want to work with */

	/* if the device wasn't found in host */

	/* get device handle */

	/* We are now done with device list, free it */

	/* query port properties  */

	/* allocate Protection Domain */

	/* each side will send only one WR, so Completion Queue with 1 entry is enough */

	/* allocate the memory buffer that will hold the data */

	/* only in the daemon side put the message in the memory buffer */

	/* register this memory buffer */

	/* create the Queue Pair */

	return 0;
}


/******************************************************************************
* Function: resources_destroy
*
* Input
*   res                     pointer to resources structure
*
* Output
*   none
*
* Returns
*   0 on success, 1 on failure
*
* Description
*   Cleanup and deallocate all resources used
******************************************************************************/

static int resources_destroy(struct resources *res)
{
	int rc = 0;

	if (res->qp) {
		if (ibv_destroy_qp(res->qp)) {
			fprintf(stderr, "failed to destroy QP\n");
			rc = 1;
		}
	}

	if (res->mr) {
		if (ibv_dereg_mr(res->mr)) {
			fprintf(stderr, "failed to deregister MR\n");
			rc = 1;
		}
	}

	if (res->buf)
		free(res->buf);

	if (res->cq) {
		if (ibv_destroy_cq(res->cq)) {
			fprintf(stderr, "failed to destroy CQ\n");
			rc = 1;
		}
	}

	if (res->pd) {
		if (ibv_dealloc_pd(res->pd)) {
			fprintf(stderr, "failed to deallocate PD\n");
			rc = 1;
		}
	}

	if (res->ib_ctx) {
		if (ibv_close_device(res->ib_ctx)) {
			fprintf(stderr, "failed to close device context\n");
			rc = 1;
		}
	}

	if (res->sock >= 0) {
		if (close(res->sock)) {
			fprintf(stderr, "failed to close socket\n");
			rc = 1;
		}
	}

	return rc;
}

/******************************************************************************
* Function: print_config
*
* Input
*   none
*
* Output
*   none
*
* Returns
*   none 
*
* Description
*   Print out config information
******************************************************************************/

static void print_config(void)
{
	fprintf(stdout, " ------------------------------------------------\n");
	fprintf(stdout, " Device name                  : \"%s\"\n", config.dev_name);
	fprintf(stdout, " IB port                      : %u\n", config.ib_port);
	if (config.server_name)
		fprintf(stdout, " IP                           : %s\n", config.server_name);
	fprintf(stdout, " TCP port                     : %u\n", config.tcp_port);
	fprintf(stdout, " ------------------------------------------------\n\n");
}

/******************************************************************************
* Function: usage
*
* Input
*   argv0                   command line arguments
*
* Output
*   none
*
* Returns
*   none
*
* Description
*   print a description of command line syntax
******************************************************************************/

static void usage(const char *argv0)
{
	fprintf(stdout, "Usage:\n");
	fprintf(stdout, "  %s            start a server and wait for connection\n", argv0);
	fprintf(stdout, "  %s <host>     connect to server at <host>\n", argv0);
	fprintf(stdout, "\n");
	fprintf(stdout, "Options:\n");
	fprintf(stdout, "  -p, --port=<port>      listen on/connect to port <port> (default 18515)\n");
	fprintf(stdout, "  -d, --ib-dev=<dev>     use IB device <dev> (default first device found)\n");
	fprintf(stdout, "  -i, --ib-port=<port>   use port <port> of IB device (default 1)\n");
}

/******************************************************************************
* Function: main 
*
* Input
*   argc                    number of items in argv
*   argv                    command line parameters
*
* Output
*   none
*
* Returns
*   0 on success, 1 on failure
*
* Description
*   Main program code
******************************************************************************/

int main(int argc, char *argv[])
{
	struct resources 	res;
	int 			rc = 1;


	/* parse the command line parameters */
	while (1) {
		int c;

		static struct option long_options[] = {
			{ .name = "port",    .has_arg = 1, .val = 'p' },
			{ .name = "ib-dev",  .has_arg = 1, .val = 'd' },
			{ .name = "ib-port", .has_arg = 1, .val = 'i' },
			{ .name = NULL,      .has_arg = 0, .val = '\0'}
		};

		c = getopt_long(argc, argv, "p:d:i", long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'p':
			config.tcp_port = strtoul(optarg, NULL, 0);
			break;

		case 'd':
			config.dev_name = strdup(optarg);
			break;

		case 'i':
			config.ib_port = strtoul(optarg, NULL, 0);
			if (config.ib_port < 0) {
				usage(argv[0]);
				return 1;
			}
			break;

		default:
			usage(argv[0]);
			return 1;
		}
	}

	/* parse the last parameter (if exists) as the server name */
	if (optind == argc - 1)
		config.server_name = strdup(argv[optind]);
	else if (optind < argc) {
		usage(argv[0]);
		return 1;
	}

	/* print the used parameters for info*/
	print_config();

	/* init all of the resources, so cleanup will be easy */
	resources_init(&res);

	/* create resources before using them */
	if (resources_create(&res)) {
		fprintf(stderr, "failed to create resources\n");
		goto cleanup;
	}

	rc = 0;

cleanup:
	if (resources_destroy(&res)) {
		fprintf(stderr, "failed to destroy resources\n");
		rc = 1;
	}

	fprintf(stdout, "\ntest status is %d\n", rc);

	return rc;
}


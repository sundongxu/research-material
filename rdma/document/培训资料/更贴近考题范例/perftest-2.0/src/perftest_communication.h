/*
 * Copyright (c) 2011 Mellanox Technologies Ltd.  All rights reserved.
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
 * Author: Ido Shamay <idos@dev.mellanox.co.il>
 *
 * Description : ...
 *
 * Methods : ...
 */

#ifndef PERFTEST_COMMUNICATION_H
#define PERFTEST_COMMUNICATION_H

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include "perftest_resources.h"

#define KEY_MSG_SIZE 	 (59)   // Message size without gid.
#define KEY_MSG_SIZE_GID (108)   // Message size with gid (MGID as well).
#define SYNC_SPEC_ID	 (5)

// The Format of the message we pass through sockets , without passing Gid.
#define KEY_PRINT_FMT "%04x:%04x:%06x:%06x:%08x:%016Lx:%08x"

// The Format of the message we pass through sockets (With Gid).
#define KEY_PRINT_FMT_GID "%04x:%04x:%06x:%06x:%08x:%016Lx:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%08x:"

// The Basic print format for all verbs.
#define BASIC_ADDR_FMT " %s address: LID %#04x QPN %#06x PSN %#06x"

// Addition format string for READ - the outstanding reads.
#define READ_FMT       " OUT %#04x"

// The print format of the pingpong_dest element for RDMA verbs.
#define RDMA_FMT       " RKey %#08x VAddr %#016Lx"

// The print number of SRQ in case of XRC
#define XRC_FMT 	   " SRQn %#08x"

// The print format of a global address or a multicast address.
#define PERF_GID_FMT " %s: %02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d\n"

struct perftest_comm {
	struct pingpong_context    *rdma_ctx;
	struct perftest_parameters *rdma_params;
};

/* create_comm_struct
 *
 * Description : Creating the communication struct for Etherent or rdma_cm options.
 *
 * Parameters :
 *	comm - An empty Communication struct.
 *  user_param -  Perftest parameters.
 *
 * Return Value : SUCCESS,FAILURE.
 */
int create_comm_struct (struct perftest_comm *comm,
						struct perftest_parameters *user_param);


/* set_up_connection .
 *
 * Description : Fills the my_dest with all of the machine proporties.
 *
 *
 * Parameters :
 *  ctx 		- Pingoong context after the ctx_init function.
 *  user_parm   - Perftest parameters.
 *  my_dest		- An empty pingpong_dest struct.
 *
 * Return Value : SUCCESS,FAILURE.
 */
int set_up_connection(struct pingpong_context *ctx,
					  struct perftest_parameters *user_parm,
					  struct pingpong_dest *my_dest);

/* establish_connection .
 *
 * Description :
 *
 *  Connect the client the a well known server to a requested port.
 *  It assumes the Server is waiting for request on the port.
 *  It uses Ethernet sockets or rdma_cm as mentioned in use_rdma_cm.
 *
 * Parameters :
 *		comm - The communication struct with all the data.
 *
 * Return Value : SUCCESS,FAILURE.
 */
int establish_connection(struct perftest_comm *comm);

/* rdma_client_connect .
 *
 * Description : Connects the client to a QP on the other machine with rdma_cm.
 *
 * Parameters :
 *		ctx - An empty resources struct to fill the resources created for this QP.
 *		user_param - Perftest parameters.
 *
 * Return Value : SUCCESS,FAILURE.
 */
int rdma_client_connect(struct pingpong_context *ctx,
						struct perftest_parameters *user_param);

/* rdma_server_connect .
 *
 * Description : Assinging a server to listen on a rdma_cm port and connect to it.
 *
 * Parameters :
 *		ctx - An empty resources struct to fill the resources created for this QP.
 *		user_param - Perftest parameters.
 *
 * Return Value : SUCCESS,FAILURE.
 */
int rdma_server_connect(struct pingpong_context *ctx,
						struct perftest_parameters *user_param);
/* ctx_hand_shake .
 *
 * Description :
 *
 *  Exchanging the data , represented in struct pingpong_dest , between
 *  a server and client that performed the ctx_server/clinet_connect.
 *  The method fills in rem_dest the remote machine data , and passed the data
 *  in my_dest to other machine.
 *
 * Parameters :
 *
 *  params   - The parameters needed for this method. Are mentioned above ,and
 *             contains standard IB info. (exists on perftest).
 *  my_dest  - Contains the data you want to pass to the other side.
 *  rem_dest - The other side data.
 *
 * Return Value : 0 upon success. -1 if it fails.
 */
int ctx_hand_shake(struct perftest_comm *comm,
				   struct pingpong_dest *my_dest,
				   struct pingpong_dest *rem_dest);



/* ctx_print_pingpong_data.
 *
 * Description :
 *
 *  Prints the data stored in the struct pingpong_dest.
 *
 * Parameters :
 *
 *  params  - The parameters of the machine.
 *  element - The element to print.
 */
void ctx_print_pingpong_data(struct pingpong_dest *element,
							 struct perftest_comm *comm);

/* ctx_close_connection .
 *
 * Description :
 *
 *  Close the connection between the 2 machines.
 *  It performs an handshake to ensure the 2 sides are there.
 *
 * Parameters :
 *
 *  params            - The parameters of the machine
 *  my_dest ,rem_dest - The 2 sides that ends the connection.
 *
 * Return Value : 0 upon success. -1 if it fails.
 */
int ctx_close_connection(struct perftest_comm *comm,
				         struct pingpong_dest *my_dest,
				         struct pingpong_dest *rem_dest);

/* ctx_xchg_data .
 *
 * Description :
 *
 *  Exchanging data between
 *  a server and client after performing ctx_server/client_connect.
 *  The method fills in rem_data the remote machine data , and passed the data
 *  in my_dest to other machine.
 *
 * Parameters :
 *
 *  comm   - contains connections info
 *  my_data  - Contains the data you want to pass to the other side.
 *  rem_data - The other side data.
 *  size - size of my_data (after casting is made)
 *
 * Return Value : 0 upon success. -1 if it fails.
 */
int ctx_xchg_data( struct perftest_comm *comm,
				   void *my_data,
				   void *rem_data,int size);

/* ethernet_write_data .
 *
 * Description :
 *
 *  Sends data that is written in msg using ethernet
 *  This functions can send any basic type (int,float,double,char*,string, etc..).
 *	If you need to send a struct, decoder and encoder must be implemented to convert
 *  the struct to a string
 *
 * Parameters :
 *
 *  comm   - contains connections info
 *  msg    - message that will be sent
 * Return Value : 0 upon success. -1 if it fails.
 */
int ethernet_write_data(struct perftest_comm *comm, char* msg);

/* ethernet_read_data .
 *
 * Description :
 *
 *  Read data from remote machine using ethernet.
 *
 * Parameters :
 *
 *  comm   - contains connections info
 *  recv_msg    - function will return, in this argument, the message from remote machine 
 * Return Value : 0 upon success. -1 if it fails.
 */
int ethernet_read_data(struct perftest_comm *comm, char* recv_msg);

/* rdma_write_data .
 *
 * Description :
 *
 *  Sends data that to remote machine using RDMA.
 *  This functions can send any variable type
 *
 * Parameters :
 *
 *  data   - data that will be sent
 *  comm   - contains connections info
 *  size    - size of data
 * Return Value : 0 upon success. -1 if it fails.
 */
int rdma_write_data(void *data,	struct perftest_comm *comm, int size);

/* rdma_read_data .
 *
 * Description :
 *
 *  Reads  data from remote machine using RDMA.
 *  This functions can read any variable type
 *
 * Parameters :
 *
 *  data   - data that will be sent
 *  comm   - contains connections info
 *  size    - size of data
 * Return Value : 0 upon success. -1 if it fails.
 * Return Value : 0 upon success. -1 if it fails.
 */
int rdma_read_data(void *data, struct perftest_comm *comm, int size);

/* ctx_xchg_data .
 *
 * Description :
 *
 *  Implements ctx_xchg_data for ethernet
 *
 * Parameters :
 *
 *  comm   - contains connections info
 *  my_data  - Contains the data you want to pass to the other side.
 *  rem_data - The other side data.
 *  size - size of my_data (after casting is made)
 *
 * Return Value : 0 upon success. -1 if it fails.
 */
int ctx_xchg_data_ethernet( struct perftest_comm *comm, void *my_data, void *rem_data,int size);

/* ctx_xchg_data .
 *
 * Description :
 *
 *  Implements ctx_xchg_data for RDMA
 *
 * Parameters :
 *
 *  comm   - contains connections info
 *  my_data  - Contains the data you want to pass to the other side.
 *  rem_data - The other side data.
 *  size - size of my_data (after casting is made)
 *
 * Return Value : 0 upon success. -1 if it fails.
 */
int ctx_xchg_data_rdma( struct perftest_comm *comm, void *my_data, void *rem_data,int size);

/* ctx_xchg_data .
 *
 * Description :
 *
 *  Exchanging bw reports between
 *  a server and client after performing ctx_server/client_connect.
 *  The method fills in rem_data the remote machine data , and passed the data
 *  in my_dest to other machine.
 *
 * Parameters :
 *
 *  comm   - contains connections info
 *  my_bw_rep  - Contains the data you want to pass to the other side.
 *  rem_bw_rep - The other side data.
 *
 * Return Value : 0 upon success. -1 if it fails.
 */
void xchg_bw_reports (struct perftest_comm *comm, struct bw_report_data *my_bw_rep,
                                                        struct bw_report_data *rem_bw_rep);
														
														/* exchange_versions.
 *
 * Description :
 * 	Exchange versions between sides.
 *
 */
void exchange_versions (struct perftest_comm *user_comm, struct perftest_parameters *user_param);

#endif /* PERFTEST_COMMUNICATION_H */




#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <malloc.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>

#include "pingpong.h"
#include "pingpong.c"
enum {
	PINGPONG_RECV_WRID = 1,
	PINGPONG_SEND_WRID = 2,
};
int main(int argc, char *argv[]){
	rdma_context ctx;
	if(argc > 0) {
		ctx = rdma_socket(argv[0],0,0);
	}
	else {
		ctx = rdma_socket(null,0,0);
	}
	rdma_post_send(ctx);
	int r = rdma_query_cq(ctx);
	
}
struct rdma_context {
	struct ibv_context	*context;
	struct ibv_comp_channel *channel;
	struct ibv_pd		*pd;
	struct ibv_mr		*mr;
	struct ibv_cq		*cq;
	struct ibv_qp		*qp;
	void			*buf;
	int			  size;
	int			 rx_depth;
	int		  id;
	int			 pending;
	struct ibv_port_attr     portinfo;
};

struct info_con {
	uint32_t lid;
	uint32_t qpn;
	uint32_t psn;
	union ibv_gid gid;
};

static struct rdma_context *init_rdma_context(struct ibv_device *ib_dev,
					    int rx_depth, int port,
					    int use_event, int is_server){
	struct rdma_context ctx;
	
	ctx = calloc(1, sizeof *ctx);
	if(!ctx) {
		return NULL;
	}
	
	ctx->rx_depth = rx_depth;
	ctx->id = 0;
	
	int page_size = sysconf(_SC_PAGESIZE);
	ctx->size = 4 * page_size;
	ctx->buf = memalign(page_size, ctx->size);
	if (!ctx->buf) {
		fprintf(stderr, "Couldn't allocate work buf.\n");
		goto clean_ctx;
	}
	memset(ctx->buf, 0x7b + is_server, ctx->size);
	
	ctx->context = ibv_open_device(ib_dev);
	if (!ctx->context) {
		fprintf(stderr, "Couldn't get context for %s\n",
			ibv_get_device_name(ib_dev));
		goto clean_buffer;
	}
	
	if (use_event) {
		ctx->channel = ibv_create_comp_channel(ctx->context);
		if (!ctx->channel) {
			fprintf(stderr, "Couldn't create completion channel\n");
			goto clean_device;
		}
	} 
	else {
		ctx->channel = NULL;
	}
	
	ctx->pd = ibv_alloc_pd(ctx->context);
	if (!ctx->pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		goto clean_comp_channel;
	}

	ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, ctx->size, IBV_ACCESS_LOCAL_WRITE);
	if (!ctx->mr) {
		fprintf(stderr, "Couldn't register MR\n");
		goto clean_pd;
	}
	
	ctx->cq = ibv_create_cq(ctx->context, rx_depth + 1, NULL,
				ctx->channel, 0);
	if (!ctx->cq) {
		fprintf(stderr, "Couldn't create CQ\n");
		goto clean_mr;
	}
	
	{
		struct ibv_qp_init_attr attr = {
			.send_cq = ctx->cq,
			.recv_cq = ctx->cq,
			.cap     = {
				.max_send_wr  = 1,// TODO
				.max_recv_wr  = rx_depth,
				.max_send_sge = 1,
				.max_recv_sge = 1
			},
			.qp_type = IBV_QPT_RC
		};

		ctx->qp = ibv_create_qp(ctx->pd, &attr);
		if (!ctx->qp)  {
			fprintf(stderr, "Couldn't create QP\n");
			goto clean_cq;
		}
	}
	
	{
		struct ibv_qp_attr attr = {
			.qp_state        = IBV_QPS_INIT,
			.pkey_index      = 0,
			.port_num        = port,
			.qp_access_flags = 0
		};

		if (ibv_modify_qp(ctx->qp, &attr,
				  IBV_QP_STATE              |
				  IBV_QP_PKEY_INDEX         |
				  IBV_QP_PORT               |
				  IBV_QP_ACCESS_FLAGS)) {
			fprintf(stderr, "Failed to modify QP to INIT\n");
			goto clean_qp;
		}
	}
	
	return ctx;
	
clean_qp:
	ibv_destroy_qp(ctx->qp);

clean_cq:
	ibv_destroy_cq(ctx->cq);

clean_mr:
	ibv_dereg_mr(ctx->mr);

clean_pd:
	ibv_dealloc_pd(ctx->pd);

clean_comp_channel:
	if (ctx->channel)
		ibv_destroy_comp_channel(ctx->channel);

clean_device:
	ibv_close_device(ctx->context);

clean_buffer:
	free(ctx->buf);
	
clean_ctx:
	free(ctx);

	return NULL;
}

static int rdma_post_recv(struct rdma_context *ctx, int n) {
	struct ibv_sge list = {
		.addr	= (uintptr_t) ctx->buf,
		.length = ctx->size,
		.lkey	= ctx->mr->lkey
	};
	struct ibv_recv_wr wr = {
		.wr_id	    = PINGPONG_RECV_WRID,//todo
		.sg_list    = &list,
		.num_sge    = 1,
	};
	struct ibv_recv_wr *bad_wr;
	int rc = ibv_post_recv(ctx->qp, &wr, &bad_wr);
	if (rc) {
		fprintf(stderr, "failed to post RR\n");
		return 1;
	}
	
	return rc;
}
	
static struct info_con *rdma_client_exch_dest(const char *servername, int port,
						 const struct info_con *my_info)
{
	struct addrinfo *res, *t;
	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char *service;
	char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
	int n;
	int sockfd = -1;
	struct info_con *remote_info = NULL;
	char gid[33];

	if (asprintf(&service, "%d", port) < 0)
		return NULL;

	n = getaddrinfo(servername, service, &hints, &res);

	if (n < 0) {
		fprintf(stderr, "%s for %s:%d\n", gai_strerror(n), servername, port);
		free(service);
		return NULL;
	}

	for (t = res; t; t = t->ai_next) {
		sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
		if (sockfd >= 0) {
			if (!connect(sockfd, t->ai_addr, t->ai_addrlen))
				break;
			close(sockfd);
			sockfd = -1;
		}
	}

	freeaddrinfo(res);
	free(service);

	if (sockfd < 0) {
		fprintf(stderr, "Couldn't connect to %s:%d\n", servername, port);
		return NULL;
	}

	gid_to_wire_gid(&my_info->gid, gid);
	sprintf(msg, "%04x:%06x:%06x:%s", my_info->lid, my_info->qpn, my_info->psn, gid);
	if (write(sockfd, msg, sizeof msg) != sizeof msg) {
		fprintf(stderr, "Couldn't send local address\n");
		goto out;
	}

	if (read(sockfd, msg, sizeof msg) != sizeof msg) {
		perror("client read");
		fprintf(stderr, "Couldn't read remote address\n");
		goto out;
	}

	write(sockfd, "done", sizeof "done");

	remote_info = malloc(sizeof *remote_info);
	if (!remote_info)
		goto out;

	sscanf(msg, "%x:%x:%x:%s", &remote_info->lid, &remote_info->qpn, &remote_info->psn, gid);
	wire_gid_to_gid(gid, &remote_info->gid);

out:
	close(sockfd);
	return remote_info;
}
	
static struct info_con *rdma_server_exch_dest(struct rdma_context *ctx,
						 int ib_port, enum ibv_mtu mtu,
						 int port, int sl,
						 const struct info_con *my_dest,
						 int sgid_idx)
{
	struct addrinfo *res, *t;
	struct addrinfo hints = {
		.ai_flags    = AI_PASSIVE,
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char *service;
	char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
	int n;
	int sockfd = -1, connfd;
	struct info_con *remote_info = NULL;
	char gid[33];

	if (asprintf(&service, "%d", port) < 0)
		return NULL;

	n = getaddrinfo(NULL, service, &hints, &res);

	if (n < 0) {
		fprintf(stderr, "%s for port %d\n", gai_strerror(n), port);
		free(service);
		return NULL;
	}

	for (t = res; t; t = t->ai_next) {
		sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
		if (sockfd >= 0) {
			n = 1;

			setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n);

			if (!bind(sockfd, t->ai_addr, t->ai_addrlen))
				break;
			close(sockfd);
			sockfd = -1;
		}
	}

	freeaddrinfo(res);
	free(service);

	if (sockfd < 0) {
		fprintf(stderr, "Couldn't listen to port %d\n", port);
		return NULL;
	}

	listen(sockfd, 1);
	connfd = accept(sockfd, NULL, 0);
	close(sockfd);
	if (connfd < 0) {
		fprintf(stderr, "accept() failed\n");
		return NULL;
	}

	n = read(connfd, msg, sizeof msg);
	if (n != sizeof msg) {
		perror("server read");
		fprintf(stderr, "%d/%d: Couldn't read remote address\n", n, (int) sizeof msg);
		goto out;
	}

	remote_info = malloc(sizeof *remote_info);
	if (!remote_info)
		goto out;

	sscanf(msg, "%x:%x:%x:%s", &remote_info->lid, &remote_info->qpn, &remote_info->psn, gid);
	wire_gid_to_gid(gid, &remote_info->gid);

	if (rdma_connect_ctx(ctx, ib_port, my_dest->psn, mtu, sl, remote_info, sgid_idx)) {
		fprintf(stderr, "Couldn't connect to remote QP\n");
		free(remote_info);
		remote_info = NULL;
		goto out;
	}


	gid_to_wire_gid(&my_dest->gid, gid);
	sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn, my_dest->psn, gid);
	if (write(connfd, msg, sizeof msg) != sizeof msg) {
		fprintf(stderr, "Couldn't send local address\n");
		free(remote_info);
		remote_info = NULL;
		goto out;
	}

	read(connfd, msg, sizeof msg);

out:
	close(connfd);
	return remote_info;
}
	
static int rdma_connect_ctx(struct rdma_context *ctx, int port, int my_psn,
			  enum ibv_mtu mtu, int sl,
			  struct info_con *dest, int sgid_idx)
{
	struct ibv_qp_attr attr = {
		.qp_state		= IBV_QPS_RTR,
		.path_mtu		= mtu,
		.dest_qp_num		= dest->qpn,
		.rq_psn			= dest->psn,
		.max_dest_rd_atomic	= 1,
		.min_rnr_timer		= 12,
		.ah_attr		= {
			.is_global	= 0,
			.dlid		= dest->lid,
			.sl		= sl,
			.src_path_bits	= 0,
			.port_num	= port
		}
	};

	if (dest->gid.global.interface_id) {
		attr.ah_attr.is_global = 1;
		attr.ah_attr.grh.hop_limit = 1;
		attr.ah_attr.grh.dgid = dest->gid;
		attr.ah_attr.grh.sgid_index = sgid_idx;
	}
	if (ibv_modify_qp(ctx->qp, &attr,
			  IBV_QP_STATE              |
			  IBV_QP_AV                 |
			  IBV_QP_PATH_MTU           |
			  IBV_QP_DEST_QPN           |
			  IBV_QP_RQ_PSN             |
			  IBV_QP_MAX_DEST_RD_ATOMIC |
			  IBV_QP_MIN_RNR_TIMER)) {
		fprintf(stderr, "Failed to modify QP to RTR\n");
		return 1;
	}

	attr.qp_state	    = IBV_QPS_RTS;
	attr.timeout	    = 14;
	attr.retry_cnt	    = 7;
	attr.rnr_retry	    = 7;
	attr.sq_psn	    = my_psn;
	attr.max_rd_atomic  = 1;
	if (ibv_modify_qp(ctx->qp, &attr,
			  IBV_QP_STATE              |
			  IBV_QP_TIMEOUT            |
			  IBV_QP_RETRY_CNT          |
			  IBV_QP_RNR_RETRY          |
			  IBV_QP_SQ_PSN             |
			  IBV_QP_MAX_QP_RD_ATOMIC)) {
		fprintf(stderr, "Failed to modify QP to RTS\n");
		return 1;
	}

	return 0;
}
	
rdma_context rdma_socket(const char *hostname, int iport, int is_server) {

	//+++++++++++ start of declaration for resource
	struct ibv_device      **dev_list;
	struct ibv_device	*ib_dev;
	struct rdma_context *ctx;
	struct info_con     my_info;
	struct info_con    *remote_info;
	struct timeval           start, end;
	char                    *ib_devname = NULL;//todo
	char                    *servername = NULL;
	int                      port = 18515;
	int                      ib_port = 1;
	int                      size = 4096;
	enum ibv_mtu		 mtu = IBV_MTU_1024;
	int                      rx_depth = 500;
	int                      iters = 1000;
	int                      use_event = 0;
	int                      rcnt, scnt;
	int                      num_cq_events = 0;
	int                      sl = 0;
	int			 gidx = -1;
	char			 gid[33];
	//------------- end of declaration for resource
	
	//++++++++++++ start of open ib resource
	servername = hostname;
	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		perror("Failed to get IB devices list");
		return 1;
	}
	
	if (!ib_devname) {
		ib_dev = *dev_list;
		if (!ib_dev) {
			fprintf(stderr, "No IB devices found\n");
			return 1;
		}
	} else {
		int i;
		for (i = 0; dev_list[i]; ++i)
			if (!strcmp(ibv_get_device_name(dev_list[i]), ib_devname))
				break;
		ib_dev = dev_list[i];
		if (!ib_dev) {
			fprintf(stderr, "IB device %s not found\n", ib_devname);
			return 1;
		}
	}
	
	ctx = init_rdma_context(ib_dev, rx_depth, ib_port, use_event, is_server);
	if(!ctx) {
		return 1;
	}
	
	int routs = rdma_post_recv(ctx, ctx->rx_depth);
	if (routs != 0) {
		fprintf(stderr, "Couldn't post receive (%d)\n", routs);
		return 1;
	}
	
	if (use_event) {
		if (ibv_req_notify_cq(ctx->cq, 0)) {
			fprintf(stderr, "Couldn't request CQ notification\n");
			return 1;
		}
	}
	
	if (ibv_query_port(ctx->context, ib_port, &ctx->portinfo)) {
		fprintf(stderr, "Couldn't get port info\n");
		return 1;
	}
	my_info.lid = ctx->portinfo.lid;
	if (ctx->portinfo.link_layer == IBV_LINK_LAYER_INFINIBAND && !my_info.lid) {
		fprintf(stderr, "Couldn't get local LID\n");
		return 1;
	}
	
	if (gidx >= 0) {
		if (ibv_query_gid(ctx->context, ib_port, gidx, &my_info.gid)) {
			fprintf(stderr, "Could not get local gid for gid index %d\n", gidx);
			return 1;
		}
	} else {
		memset(&my_info.gid, 0, sizeof my_info.gid);
	}
	
	my_info.qpn = ctx->qp->qp_num;
	my_info.psn = lrand48() & 0xffffff;
	inet_ntop(AF_INET6, &my_info.gid, gid, sizeof gid);// FOR DEBUG
	printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
	       my_info.lid, my_info.qpn, my_info.psn, gid);
		   
	if (is_server) {
		remote_info = rdma_client_exch_dest(servername, port, &my_info);
	}
	else {
		remote_info = rdma_server_exch_dest(ctx, ib_port, mtu, port, sl, &my_info, gidx);
	}
	
	if (!remote_info)
		return 1;
		
	inet_ntop(AF_INET6, &remote_info->gid, gid, sizeof gid);
	printf("  remote address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
	       remote_info->lid, remote_info->qpn, remote_info->psn, gid);

	if (!is_server) {
		if (rdma_connect_ctx(ctx, ib_port, my_info.psn, mtu, sl, remote_info, gidx))
			return 1;
	}
	ctx->pending = PINGPONG_RECV_WRID;
	ibv_free_device_list(dev_list);
	free(remote_info);
	
	return ctx;
}

int rdma_post_send(struct rdma_context *ctx)
{
	struct ibv_sge list = {
		.addr	= (uintptr_t) ctx->buf,
		.length = ctx->size,
		.lkey	= ctx->mr->lkey
	};
	struct ibv_send_wr wr = {
		.wr_id	    = PINGPONG_SEND_WRID,
		.sg_list    = &list,
		.num_sge    = 1,
		.opcode     = IBV_WR_SEND,
		.send_flags = IBV_SEND_SIGNALED,
	};
	struct ibv_send_wr *bad_wr;
	
	if(ibv_post_send(ctx->qp, &wr, &bad_wr)) {
		fprintf(stderr, "Couldn't post send\n");
			return 1;
	}
	
	ctx->pending |= PINGPONG_SEND_WRID;
	return 0;
}

int rdma_query_cq(struct rdma_context *ctx) {
	int use_event = 0; 
	if (use_event) {
			struct ibv_cq *ev_cq;
			void          *ev_ctx;

			if (ibv_get_cq_event(ctx->channel, &ev_cq, &ev_ctx)) {
				fprintf(stderr, "Failed to get cq_event\n");
				return 1;
			}

			if (ev_cq != ctx->cq) {
				fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
				return 1;
			}

			if (ibv_req_notify_cq(ctx->cq, 0)) {
				fprintf(stderr, "Couldn't request CQ notification\n");
				return 1;
			}
		}

		{
			struct ibv_wc wc[2];
			int ne, i;

			do {
				ne = ibv_poll_cq(ctx->cq, 2, wc);
				if (ne < 0) {
					fprintf(stderr, "poll CQ failed %d\n", ne);
					return 1;
				}

			} while (!use_event && ne < 1);

			for (i = 0; i < ne; ++i) {
				if (wc[i].status != IBV_WC_SUCCESS) {
					fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(wc[i].status),
						wc[i].status, (int) wc[i].wr_id);
					return 1;
				}

				ctx->pending &= ~(int) wc[i].wr_id;
			}
			if(ne == 2)
			fprintf(stdout, "success!\n");
		}
		
		return 0;
}

int rdma_close_ctx(struct pingpong_context *ctx)
{
	if (ibv_destroy_qp(ctx->qp)) {
		fprintf(stderr, "Couldn't destroy QP\n");
		return 1;
	}

	if (ibv_destroy_cq(ctx->cq)) {
		fprintf(stderr, "Couldn't destroy CQ\n");
		return 1;
	}

	if (ibv_dereg_mr(ctx->mr)) {
		fprintf(stderr, "Couldn't deregister MR\n");
		return 1;
	}

	if (ibv_dealloc_pd(ctx->pd)) {
		fprintf(stderr, "Couldn't deallocate PD\n");
		return 1;
	}

	if (ctx->channel) {
		if (ibv_destroy_comp_channel(ctx->channel)) {
			fprintf(stderr, "Couldn't destroy completion channel\n");
			return 1;
		}
	}

	if (ibv_close_device(ctx->context)) {
		fprintf(stderr, "Couldn't release context\n");
		return 1;
	}

	free(ctx->buf);
	free(ctx);

	return 0;
}
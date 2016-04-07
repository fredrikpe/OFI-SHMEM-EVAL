
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <netdb.h>

#include <rdma/fabric.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>

#include "shmem_impl.h"
#include "shared.h"

static enum ft_rma_opcodes op_type = FT_RMA_WRITE;
struct fi_rma_iov remote;
static uint64_t cq_data = 1;


static int alloc_ep_res(struct fi_info *fi)
{
	uint64_t access_mode;
	int ret;

	ret = ft_alloc_active_res(fi);
	if (ret)
		return ret;

	switch (op_type) {
	case FT_RMA_READ:
		access_mode = FI_REMOTE_READ;
		break;
	case FT_RMA_WRITE:
	case FT_RMA_WRITEDATA:
		access_mode = FI_REMOTE_WRITE;
		break;
	default:
		assert(0);
		return -FI_EINVAL;
	}
	ret = fi_mr_reg(domain, buf, buf_size,
			access_mode, 0, FT_MR_KEY, 0, &mr, NULL);
	if (ret) {
		FT_PRINTERR("fi_mr_reg", ret);
		return ret;
	}

	return 0;
}

static int server_connect(void)
{
	struct fi_eq_cm_entry entry;
	uint32_t event;
	struct fi_info *info = NULL;
	ssize_t rd;
	int ret;

	rd = fi_eq_sread(eq, &event, &entry, sizeof entry, -1, 0);
	if (rd != sizeof entry) {
		FT_PROCESS_EQ_ERR(rd, eq, "fi_eq_sread", "listen");
		return (int) rd;
	}

	info = entry.info;
	if (event != FI_CONNREQ) {
		fprintf(stderr, "Unexpected CM event %d\n", event);
		ret = -FI_EOTHER;
		goto err;
	}

	ret = fi_domain(fabric, info, &domain, NULL);
	if (ret) {
		FT_PRINTERR("fi_domain", ret);
		goto err;
	}

	ret = alloc_ep_res(info);
	if (ret)
		 goto err;

	ret = ft_init_ep();
	if (ret)
		goto err;

	ret = fi_accept(ep, NULL, 0);
	if (ret) {
		FT_PRINTERR("fi_accept", ret);
		goto err;
	}

	rd = fi_eq_sread(eq, &event, &entry, sizeof entry, -1, 0);
	if (rd != sizeof entry) {
		FT_PROCESS_EQ_ERR(rd, eq, "fi_eq_sread", "accept");
		ret = (int) rd;
		goto err;
	}

	if (event != FI_CONNECTED || entry.fid != &ep->fid) {
		fprintf(stderr, "Unexpected CM event %d fid %p (ep %p)\n",
			event, entry.fid, ep);
 		ret = -FI_EOTHER;
 		goto err;
 	}

 	fi_freeinfo(info);
 	return 0;

err:
 	fi_reject(pep, info->handle, NULL, 0);
 	fi_freeinfo(info);
 	return ret;
}

static int client_connect(void)
{
	struct fi_eq_cm_entry entry;
	uint32_t event;
	ssize_t rd;
	int ret;

	ret = fi_getinfo(FT_FIVERSION, opts.dst_addr, opts.dst_port, 0, hints, &fi);
	if (ret) {
		FT_PRINTERR("fi_getinfo", ret);
		return ret;
	}

	ret = ft_open_fabric_res();
	if (ret)
		return ret;

	ret = alloc_ep_res(fi);
	if (ret)
		return ret;

	ret = ft_init_ep();
	if (ret)
		return ret;

	ret = fi_connect(ep, fi->dest_addr, NULL, 0);
	if (ret) {
		FT_PRINTERR("fi_connect", ret);
		return ret;
	}

 	rd = fi_eq_sread(eq, &event, &entry, sizeof entry, -1, 0);
	if (rd != sizeof entry) {
		FT_PROCESS_EQ_ERR(rd, eq, "fi_eq_sread", "connect");
		return (int) rd;
	}

 	if (event != FI_CONNECTED || entry.fid != &ep->fid) {
 		fprintf(stderr, "Unexpected CM event %d fid %p (ep %p)\n",
 			event, entry.fid, ep);
 		return -FI_EOTHER;
 	}

	return 0;
}


int free_res_lf(void)
{
	ft_free_res();
}

int shmem_finalize(void)
{
	ft_sync();
	ft_finalize();
}

int shutdown_lf(void)
{
	fi_shutdown(ep, 0);
}

int shmem_put(size_t len) //const void *buffer, size_t len)
{
    int ret = fi_write(ep, buf, len, fi_mr_desc(mr),
               0, remote.addr, remote.key, ep);
    if (ret)
        FT_PRINTERR("fi_write", ret);
    
    ret = ft_get_tx_comp(++tx_seq);
	if (ret)
		return ret;
}

int shmem_get(size_t len) //const void *buffer, size_t len)
{
    int ret = fi_read(ep, buf, len, fi_mr_desc(mr),
            0, remote.addr, remote.key, ep);

    if (ret)
		FT_PRINTERR("fi_read", ret);
    
    ret = ft_get_tx_comp(++tx_seq);
	if (ret)
		return ret;
}

int shmem_ft_sync(void)
{
    return ft_sync();
}

int shmem_init(void)
{
	char *node, *service;
	uint64_t flags;
	int i, ret;

	ret = ft_read_addr_opts(&node, &service, hints, &flags, &opts);
	if (ret)
		return ret;

	if (!opts.dst_addr) {
		ret = ft_start_server();
		if (ret)
			return ret;
	}

	ret = opts.dst_addr ? client_connect() : server_connect();
	if (ret)
		return ret;

	ret = ft_exchange_keys(&remote);
	if (ret)
		return ret;
}

int init_opts(int argc, char **argv)
{
	int op, ret;

	opts = INIT_OPTS;

	hints = fi_allocinfo();
	if (!hints)
		return EXIT_FAILURE;

	while ((op = getopt(argc, argv, "ho:" CS_OPTS INFO_OPTS)) != -1) {
		switch (op) {
		case 'o':
			if (!strcmp(optarg, "read")) {
				op_type = FT_RMA_READ;
			} else if (!strcmp(optarg, "writedata")) {
				op_type = FT_RMA_WRITEDATA;
				cq_attr.format = FI_CQ_FORMAT_DATA;
			} else if (!strcmp(optarg, "write")) {
				op_type = FT_RMA_WRITE;
			} else {
				ft_csusage(argv[0], NULL);
				fprintf(stderr, "  -o <op>\tselect operation type (read or write)\n");
				return EXIT_FAILURE;
			}
			break;
		default:
			ft_parseinfo(op, optarg, hints);
			ft_parsecsopts(op, optarg, &opts);
			break;
		case '?':
		case 'h':
			ft_csusage(argv[0], "Ping pong client and server using message RMA.");
			fprintf(stderr, "  -o <op>\trma op type: read|write|writedata (default: write)]\n");
			return EXIT_FAILURE;
		}
	}

	if (optind < argc)
		opts.dst_addr = argv[optind];

	hints->ep_attr->type = FI_EP_MSG;
	hints->caps = FI_MSG | FI_RMA;
	hints->mode = FI_LOCAL_MR | FI_RX_CQ_DATA;

	return -ret;
}



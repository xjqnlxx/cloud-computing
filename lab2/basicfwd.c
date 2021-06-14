/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_udp.h>
#include <stdint.h>
#include <inttypes.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
	},
};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0) {
		printf("Error during getting device (port %u) info: %s\n",
				port, strerror(-retval));
		return retval;
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct rte_ether_addr addr;
	retval = rte_eth_macaddr_get(port, &addr);
	if (retval != 0)
		return retval;

	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
		return retval;

	return 0;
}
static void udp_pkg(struct rte_mbuf *m, const char* data, uint32_t data_len) {
	struct rte_ether_hdr *ether_header = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    struct rte_ipv4_hdr *ipv4_header = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
	struct rte_udp_hdr *udp_header = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));

	udp_header->src_port = 80;
	udp_header->dst_port = 8080;
	udp_header->dgram_len = (data_len + sizeof(struct rte_udp_hdr)) << 8;
	udp_header->dgram_cksum = 0xffff;

	struct rte_ether_addr s_addr_tmp, d_addr_tmp;
	rte_eth_macaddr_get(0, &s_addr_tmp);
	rte_eth_macaddr_get(0, &d_addr_tmp);
	ether_header->s_addr = s_addr_tmp;
	ether_header->d_addr = d_addr_tmp;
	ether_header->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

	ipv4_header->version_ihl=RTE_IPV4_VHL_DEF;
    	ipv4_header->type_of_service=RTE_IPV4_HDR_DSCP_MASK;
	ipv4_header->src_addr = 0;
	ipv4_header->dst_addr = 0;
	ipv4_header->total_length = rte_cpu_to_be_16(data_len + sizeof(struct rte_udp_hdr)+sizeof(struct rte_ipv4_hdr));
	ipv4_header->packet_id = 0;
	ipv4_header->fragment_offset = 0;
	ipv4_header->time_to_live = 100;
	ipv4_header->next_proto_id = 17;
	ipv4_header->hdr_checksum = rte_ipv4_cksum(ipv4_header);

	void *payload = rte_pktmbuf_mtod_offset(m, void *, sizeof(struct rte_udp_hdr) + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
	memcpy(payload, data, data_len);
	
	m->data_len=sizeof(struct rte_ether_hdr)+sizeof(struct rte_ipv4_hdr)+sizeof(struct rte_udp_hdr)+data_len;
	
	m->pkt_len=sizeof(struct rte_ether_hdr)+sizeof(struct rte_ipv4_hdr)+sizeof(struct rte_udp_hdr)+data_len;
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	uint16_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;
	
	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS ,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	RTE_ETH_FOREACH_DEV(portid)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu16 "\n",
					portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the main core only. */
	/* rte_mbuf array of BURST_SIZE*/
		struct rte_mbuf *bufs[BURST_SIZE];
		int i = 0;
	while(i < BURST_SIZE){
		bufs[i] = rte_pktmbuf_alloc(mbuf_pool);
		udp_pkg(bufs[i], "DAKEWANGMESSAGE", 15);
		i++;
	}

	uint16_t nb_tx = rte_eth_tx_burst(0,0,bufs,BURST_SIZE);
	printf("main: successfully send %d UDP packets\n", nb_tx);
	
	i--;
	while(i >= 0){
		rte_pktmbuf_free(bufs[i]);
		i--;
	}

	return 0;
}


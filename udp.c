#include <inttypes.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <stdint.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
    .rxmode =
        {
            .max_rx_pkt_len = RTE_ETHER_MAX_LEN,
        },
};

static inline int port_init(uint16_t port, struct rte_mempool *mbuf_pool) {
  struct rte_eth_conf port_conf = port_conf_default;
  const uint16_t rx_rings = 1, tx_rings = 1;
  uint16_t nb_rxd = RX_RING_SIZE;
  uint16_t nb_txd = TX_RING_SIZE;
  int retval;
  uint16_t q;
  struct rte_eth_dev_info dev_info;
  struct rte_eth_txconf txconf;

  if (!rte_eth_dev_is_valid_port(port)) return -1;

  retval = rte_eth_dev_info_get(port, &dev_info);
  if (retval != 0) {
    printf("Error during getting device (port %u) info: %s\n", port,
           strerror(-retval));
    return retval;
  }

  if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
    port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;

  /* Configure the Ethernet device. */
  retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
  if (retval != 0) return retval;

  retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
  if (retval != 0) return retval;

  /* Allocate and set up 1 RX queue per Ethernet port. */
  for (q = 0; q < rx_rings; q++) {
    retval = rte_eth_rx_queue_setup(
        port, q, nb_rxd, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
    if (retval < 0) return retval;
  }

  txconf = dev_info.default_txconf;
  txconf.offloads = port_conf.txmode.offloads;
  /* Allocate and set up 1 TX queue per Ethernet port. */
  for (q = 0; q < tx_rings; q++) {
    retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                                    rte_eth_dev_socket_id(port), &txconf);
    if (retval < 0) return retval;
  }

  /* Start the Ethernet port. */
  retval = rte_eth_dev_start(port);
  if (retval < 0) return retval;

  /* Display the port MAC address. */
  struct rte_ether_addr addr;
  retval = rte_eth_macaddr_get(port, &addr);
  if (retval != 0) return retval;

  printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8
         " %02" PRIx8 " %02" PRIx8 "\n",
         port, addr.addr_bytes[0], addr.addr_bytes[1], addr.addr_bytes[2],
         addr.addr_bytes[3], addr.addr_bytes[4], addr.addr_bytes[5]);

  /* Enable RX in promiscuous mode for the Ethernet device. */
  retval = rte_eth_promiscuous_enable(port);
  if (retval != 0) return retval;

  return 0;
}

void lcore_main(struct rte_mempool *mbuf_pool) {
  struct rte_mbuf *bufs[BURST_SIZE];
  int ether_hdr_len = sizeof(struct rte_ether_hdr);
  int ipv4_hdr_len = sizeof(struct rte_ipv4_hdr);
  int udp_hdr_len = sizeof(struct rte_udp_hdr);
  for (int i = 0; i < BURST_SIZE; i++) {
    bufs[i] = rte_pktmbuf_alloc(mbuf_pool);
    struct rte_ether_hdr *ether_hdr =
        rte_pktmbuf_mtod(bufs[i], struct rte_ether_hdr *);
    struct rte_ipv4_hdr *ipv4_hdr =
        (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(bufs[i], char *) +
                                ether_hdr_len);
    struct rte_udp_hdr *udp_hdr =
        (struct rte_udp_hdr *)(rte_pktmbuf_mtod(bufs[i], char *) +
                               ether_hdr_len + ipv4_hdr_len);
    char *data = (char *)(rte_pktmbuf_mtod(bufs[i], char *) + ether_hdr_len +
                          ipv4_hdr_len + udp_hdr_len);

    struct rte_ether_addr s_addr = {{0x0c, 0x29, 0x6e, 0x15, 0xa9, 0x08}};
    struct rte_ether_addr d_addr = {{0x00, 0x50, 0xc0, 0x00, 0x02, 0x00}};
    ether_hdr->d_addr = d_addr;
    ether_hdr->s_addr = s_addr;
    ether_hdr->ether_type = 0x0008;

    ipv4_hdr->version_ihl = RTE_IPV4_VHL_DEF;
    ipv4_hdr->type_of_service = RTE_IPV4_HDR_DSCP_MASK;
    ipv4_hdr->total_length = 0x2000;
    ipv4_hdr->packet_id = 0;
    ipv4_hdr->fragment_offset = 0;
    ipv4_hdr->time_to_live = 100;
    ipv4_hdr->src_addr = 0xB837A8C0;
    ipv4_hdr->next_proto_id = 17;
    ipv4_hdr->dst_addr = 0x8A8C0;
    ipv4_hdr->hdr_checksum = rte_ipv4_cksum(ipv4_hdr);

    udp_hdr->src_port = 8080;
    udp_hdr->dst_port = 8080;
    udp_hdr->dgram_len = 0x0c00;
    udp_hdr->dgram_cksum = 1;

    *data = 'h';
    *(data + 1) = 'e';
    *(data + 2) = 'l';
    *(data + 3) = 'l';
    *(data + 4) = 'o';
    *(data + 5) = ' ';
    *(data + 6) = 'f';
    *(data + 7) = 'r';
    *(data + 8) = 'o';
    *(data + 9) = 'm';
    *(data + 10) = ' ';
    *(data + 11) = 'v';
    *(data + 12) = 'm';
    bufs[i]->pkt_len = bufs[i]->data_len =
        ether_hdr_len + ipv4_hdr_len + udp_hdr_len + 13;
  }
  uint16_t nb_tx = rte_eth_tx_burst(0, 0, bufs, BURST_SIZE);
  printf("%d packets sent successfully", BURST_SIZE);

  for (int i = 0; i < BURST_SIZE; i++) rte_pktmbuf_free(bufs[i]);
}

int main(int argc, char *argv[]) {
  struct rte_mempool *mbuf_pool;
  unsigned nb_ports;
  uint16_t portid;
  /* Initialize the Environment Abstraction Layer (EAL). */
  int ret = rte_eal_init(argc, argv);
  if (ret < 0) rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
  argc -= ret;
  argv += ret;
  /* Creates a new mempool in memory to hold the mbufs. */
  mbuf_pool =
      rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS, MBUF_CACHE_SIZE, 0,
                              RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
  /* Initialize all ports. */
  if (port_init(0, mbuf_pool) != 0)
    rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n", portid);

  lcore_main(mbuf_pool);

  return 0;
}
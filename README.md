# dpdk

### Q1: What's the purpose of using hugepage?
(1) 在使用内存范围一定的条件下减少需要的page table entry, 减少TLB miss rate;

(2) 减少页表级数，提高page table查询效率;

(3) 发生一次page fault分配更多内存, 从而减少page fault的次数;

(4) 增加程序使用的物理内存的连续性;

### Q2: Take examples/helloworld as an example, describe the execution flow of DPDK programs?
````c
int main(int argc, char **argv) {
  int ret;
  unsigned lcore_id;
  /*STEP1: 初始化环境抽象层EAL(Environment Abstraction Layer), 如果初始化失败则报错 */
  ret = rte_eal_init(argc, argv);
  if (ret < 0) rte_panic("Cannot init EAL\n");
  /* STEP2: 遍历EAL提供的LCORE, 对于每一个worker CPU核加载线程运行lcore_hello函数  */
  /* call lcore_hello() on every worker lcore */
  RTE_LCORE_FOREACH_WORKER(lcore_id) {
    rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
  }
  

  /* STEP3: 在主CPU核运行lcore_hello函数 */
  lcore_hello(NULL);
  
  /* STEP4: 等待各LCORE上的线程运行结束 */
  rte_eal_mp_wait_lcore();

  /* STEP5: 清理ELA, 程序结束 */
  rte_eal_cleanup();

  return 0;
}
````
主要执行流为: 初始化ELA环境 -> 在其他worker LCORE和main core上加载线程执行对应的函数 -> 等待各cpu core上的线程结束 -> 清理ELA环境

### Q3: Read the codes of examples/skeleton, describe DPDK APIs related to sending and receiving packets.

#### API related to receiving packets:

````static inline uint16_t rte_eth_rx_burst(uint16_t port_id, uint16_t queue_id, struct rte_mbuf **rx_pkts, const uint16_t nb_pkts)````

````
@param port_id
Port id
@param queue_id
The index of the receive queue from which to retrieve input packets.
@param rx_pkts
The address of the buffer(缓冲区地址)
@param nb_pkts
The maximum number of packets to retrieve.
@return
The number of packets actually retrieved
````

#### API related to sending packets:

````static inline uint16_t rte_eth_tx_burst(uint16_t port_id, uint16_t queue_id, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)````

````
@param port_id
The port identifier of the Ethernet device.
@param queue_id
The index of the transmit queue through which output packets must be sent.
@param tx_pkts
The address of the buffer(缓冲区地址)
@param nb_pkts
The maximum number of packets to transmit.
@return
The number of output packets actually stored in transmit descriptors of the transmit ring.
````

### Q4:  Describe the data structure of 'rte_mbuf'.

````
void rte_pktmbuf_init(struct rte_mempool *mp, __rte_unused void *opaque_arg, void *_m, __rte_unused unsigned i) {
  struct rte_mbuf *m = _m;
  uint32_t mbuf_size, buf_len, priv_size;

  priv_size = rte_pktmbuf_priv_size(mp);
  mbuf_size = sizeof(struct rte_mbuf) + priv_size;
  buf_len = rte_pktmbuf_data_room_size(mp);

  memset(m, 0, mbuf_size);
  /* start of buffer is after mbuf structure and priv data */
  m->priv_size = priv_size;
  m->buf_addr = (char *)m + mbuf_size;
  m->buf_iova = rte_mempool_virt2iova(m) + mbuf_size;
  m->buf_len = (uint16_t)buf_len;

  /* keep some headroom between start of buffer and data */
  m->data_off = RTE_MIN(RTE_PKTMBUF_HEADROOM, (uint16_t)m->buf_len);

  /* init some constant fields */
  m->pool = mp;
  m->nb_segs = 1;
  m->port = RTE_MBUF_PORT_INVALID;
  rte_mbuf_refcnt_set(m, 1);
  m->next = NULL;
}
````
 - rte_mbuf有priv_size, buf_addr, buf_iova, buf_len等
# lab2

## 518021910515

## Part 1

#### Q1: What's the purpose of using hugepage?

相比于不用巨页来说，在使用相同内存大小的情况下，使用巨页可以减少页表项的数目，增加TLB的命中率，提升性能

#### Q2: Take examples/helloworld as an example, describe the execution flow of DPDK progtams？

```flowchart
st=>start: Start
e=>end: End
op=>operation: 初始化运行环境
op2=>operation: 报错
op3=>operation: 每个slave运行lcore_hello函数
op4=>operation: main运行lcore_hello函数
op5=>operation: 等待所有核结束执行
cond=>condition: 初始化运行环境是否成功

st->op->cond
cond(yes)->op3->op4->op5->e
cond(no)->op2
```

#### Q3: Read the codes of examples/skeleton, describe DPDK APIs related to sending and receiving packets.

```cpp
static inline uint16_t rte_eth_rx_burst(uint8_t port_id, uit16_t queue_id, 
struct rte_mbuf **rx_pkts, const uint16_t nb_pkts) 
static inline uint16_t rte_eth_tx_burst(uint8_t port_id, uint16_t queue_id, 
struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
```

参数依次为端口号，队列号，package buffer ，收发包数

返回值为实际上的收发包数

#### Q4: Describe the data structure of 'rte_mbuf'.

```cpp
struct rte_mbuf {
    void *buf_addr; /**< irtual address of segment buffer. */
    uint16_t data_off;
    uint32_t pkt_len; /**< Total pkt len: sum of all segments. */
    uint16_t data_len; /**< Amount of data in segment buffer. */
    uint16_t buf_len
    ......
}
```

![Minion](/Users/mac/cloud-labs/lab2/mbuf.png)

+ headroom：  
  一般用来存放用户自己针对于mbuf的一些描素信息，一般保留给用户使用

+ data：
  
  data区域一般指的是地址区间在 `buf_addr + data_off` 到 `buf_add + data_off + data_len

+ tailroom：
  
  一般指的是，data_len还未包含的东西

## Part 2

![check](/Users/mac/cloud-labs/lab2/check.jpeg)

#### 验证

```cpp
// main
while(i < BURST_SIZE){
		bufs[i] = rte_pktmbuf_alloc(mbuf_pool);
		udp_pkg(bufs[i], "DAKEWANGMESSAGE", 15);
		i++;
	}
// udp_pkg
ipv4_header->src_addr = 0;
ipv4_header->dst_addr = 0;
udp_header->dgram_cksum = 0xffff;
```

payload checksum等相同，发包正确



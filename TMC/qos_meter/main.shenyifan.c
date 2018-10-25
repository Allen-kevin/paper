/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <linux/if_ether.h>
#include <netinet/ip.h>
#include <linux/tcp.h>
#include <assert.h>

#define TCP_SEQ_LT(a,b) 		((int32_t)((a)-(b)) < 0)
#define TCP_SEQ_LEQ(a,b)		((int32_t)((a)-(b)) <= 0)
#define TCP_SEQ_GT(a,b) 		((int32_t)((a)-(b)) > 0)
#define TCP_SEQ_GEQ(a,b)		((int32_t)((a)-(b)) >= 0)
#define TCP_SEQ_BETWEEN(a,b,c)          (TCP_SEQ_GEQ(a,b) && TCP_SEQ_LEQ(a,c))



#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define NB_MBUF   800000	//used to be 8192, bigger than BDP-- shenyifan

#define MAX_PKT_BURST 32	//used to be 32 --shenyifan
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128*8
#define RTE_TEST_TX_DESC_DEFAULT 512*2
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t l2fwd_enabled_port_mask = 0;

/* list of enabled ports */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];

static unsigned int l2fwd_rx_queue_per_lcore = 1;

struct mbuf_table {
	unsigned len;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
	struct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];

} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

struct rte_mempool * l2fwd_pktmbuf_pool = NULL;

/* Per-port statistics struct */
struct l2fwd_port_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;//right now, packets only dropped because of full dq
    int dq_count;//added by shenyifan
    uint64_t retransmition;
    int drop;//count for random drop made by shenyifan
    int outoforder;
} __rte_cache_aligned;
struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];

/* A tsc-based timer responsible for triggering statistics printout */
#define TIMER_MILLISECOND 2000000ULL /* around 1ms at 2 Ghz */
#define MAX_TIMER_PERIOD 86400 /* 1 day max */
static int64_t timer_period = 2 * TIMER_MILLISECOND * 1000; 
/* default period is 10 seconds */

// shenyifan defines
#define PKT_STATE_NORMAL        0
#define PKT_STATE_RETRANS       1
#define PKT_STATE_OUTOFORDER    2
#define PKT_STATE_DROPPED       3
//#define DIRECT_FWD

#ifndef DIRECT_FWD
	#define TX_DELAY_US 			250000
    #define DELAY_QUEUE_LEN			300000
    
//	#define RATE_CONTROL
		#ifdef RATE_CONTROL
			#define RATE_LIMIT_M 		500
			#define BUCKET_SIZE 		150000
			#define TOKEN_INC_THRESH	50
			#define BACKLOG_LIMIT
		#endif
	
	#define DROP_PACKETS
		#ifdef DROP_PACKETS
			#define DROP_RAND
				#define DROP_RATE 		100000
        
//			#define DROP_BURST
				#define DROP_CYCLE		100000
				#define DROP_NUMBER		112
        
			#define DROP_RETRANSMIT_PACKET
    	#endif
#endif

// shenyifan debug
#define DEBUG_DROP
//#define DEBUG_OUT_OF_ORDER
//	#define DEBUG_RETRANS

/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	unsigned portid;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };

		/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");

	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		/* skip disabled ports */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64
			   "\nPackets dropped: %21"PRIu64
			   "\nPackets out_of_order: %16"PRIu64
               "\nPackets retransmition: %15"PRIu64,
			   portid,
			   port_statistics[portid].tx,
			   port_statistics[portid].rx,
			   port_statistics[portid].dropped,
			   port_statistics[portid].outoforder,
               port_statistics[portid].retransmition);
		printf("\n####shenyifan: packet droped: %d", port_statistics[portid].drop);
        total_packets_dropped += port_statistics[portid].dropped;
		total_packets_tx += port_statistics[portid].tx;
		total_packets_rx += port_statistics[portid].rx;
	}
/*
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets sent: %18"PRIu64
		   "\nTotal packets received: %14"PRIu64
		   "\nTotal packets dropped: %15"PRIu64,
		   total_packets_tx,
		   total_packets_rx,
		   total_packets_dropped);
*/
	printf("\neth_port statistics ================================");
	struct rte_eth_stats eth_stats;
	unsigned i;
	for (i = 0; i < rte_eth_dev_count(); i++) {
			rte_eth_stats_get(i, &eth_stats);
			printf("\nPort %u stats:\n", i);
			printf(" - Pkts in:   %"PRIu64"\n", eth_stats.ipackets);
			printf(" - Pkts out:  %"PRIu64"\n", eth_stats.opackets);
			printf(" - In Errs:   %"PRIu64"\n", eth_stats.ierrors);
			printf(" - Out Errs:  %"PRIu64"\n", eth_stats.oerrors);
			printf(" - Mbuf Errs: %"PRIu64"\n", eth_stats.rx_nombuf);
	}
	printf("\n====================================================");
#ifndef DIRECT_FWD
    printf("\ndelay %d us, limit %d packets", TX_DELAY_US, DELAY_QUEUE_LEN);
    #ifdef DROP_RAND
        printf("\ndrop rate:%1.5lf%%", (double)100 / (double)DROP_RATE);
    #endif
    #ifdef DROP_BURST
        printf("\ndrop %d every %d packets", DROP_NUMBER, DROP_CYCLE );
    #endif
    #ifdef DROP_RETRANSMIT_PACKET
        printf("\nretransmition packets also can be dropped!");
    #endif
	#ifdef RATE_CONTROL
		printf("\ntbf rate %d Mbps, burst %d Byte",
				RATE_LIMIT_M, BUCKET_SIZE);
		#ifdef BACKLOG_LIMIT
			printf(" with backlog_limit");
		#endif
	#endif
#else
    printf("\ndirect forward");
#endif
    printf("\n====================================================\n");
}

/* Send the burst of packets on an output interface */
static int
l2fwd_send_burst(struct lcore_queue_conf *qconf, unsigned n, uint8_t port)
{
	struct rte_mbuf **m_table;
	unsigned ret;
	unsigned queueid =0;
    
    m_table = (struct rte_mbuf **)qconf->tx_mbufs[port].m_table;
    
    int i;
	ret = rte_eth_tx_burst(port, (uint16_t) queueid, m_table, (uint16_t) n);
	port_statistics[port].tx += ret;
	if (unlikely(ret < n)) {
		port_statistics[port].dropped += (n - ret);
		do {
			rte_pktmbuf_free(m_table[ret]);
		} while (++ret < n);
	}

	return 0;
}

/* Enqueue packets for TX and prepare them to be sent */
static int
l2fwd_send_packet(struct rte_mbuf *m, uint8_t port)
{
	unsigned lcore_id, len;
	struct lcore_queue_conf *qconf;

	lcore_id = rte_lcore_id();

	qconf = &lcore_queue_conf[lcore_id];
	len = qconf->tx_mbufs[port].len;
	qconf->tx_mbufs[port].m_table[len] = m;
	len++;

	/* enough pkts to be sent */
	if (unlikely(len == MAX_PKT_BURST)) {
		l2fwd_send_burst(qconf, MAX_PKT_BURST, port);
		len = 0;
	}

	qconf->tx_mbufs[port].len = len;
	return 0;
}

// shenyifan vars
typedef struct DQ_pkt {
    struct rte_mbuf *m;
    uint64_t timestamp;
    unsigned dst_port;
    char state;
    //uint32_t seq;// just for debug
} dq_pkt;


// shenyifan functions
inline struct ether_hdr *get_eth_hdr(struct rte_mbuf *m) 
{
    return rte_pktmbuf_mtod(m, struct ether_hdr *);
}
inline struct ipv4_hdr *get_ipv4_hdr(struct rte_mbuf *m)
{
    return (struct ipv4_hdr *)(get_eth_hdr(m) + 1);
}
inline struct tcp_hdr *get_tcp_hdr(struct rte_mbuf *m)
{
    struct ipv4_hdr *ip_hdr = get_ipv4_hdr(m);
    if (ip_hdr->next_proto_id != 6)
        return NULL;
    else
        return(struct tcp_hdr *)((unsigned char *) ip_hdr + sizeof(struct ipv4_hdr));
}

inline struct ethhdr *get_ethhdr(struct rte_mbuf *m)
{
    return (struct ethhdr *)rte_pktmbuf_mtod(m, uint8_t *);
}
inline struct iphdr *get_iphdr(struct rte_mbuf *m)
{
    return (struct iphdr *)(rte_pktmbuf_mtod(m, uint8_t *) + sizeof(struct ethhdr));
}
inline struct tcphdr *get_tcphdr(struct rte_mbuf *m)
{
    struct iphdr *iph = get_iphdr(m);
    if (iph->protocol == 6)
        return (struct tcphdr *) ((u_char *)iph + (iph->ihl << 2));
    else
        return NULL;
}
int payloadlen(struct rte_mbuf *m)
{
    struct iphdr *iph = get_iphdr(m);
    if (iph->protocol == 6) {
        struct tcphdr *tcph = (struct tcphdr *) ((u_char *)iph + (iph->ihl << 2));
        uint8_t *payload = (uint8_t *)tcph + (tcph->doff << 2);
        return (ntohs(iph->tot_len) - (payload - (u_char *)iph));
    }
    else
        return 0;
}

#ifdef RATE_CONTROL
int get_token(uint64_t cur_tsc, uint16_t len)
{
	static uint64_t produce_time = 0;
	static uint64_t token_bucket = BUCKET_SIZE;
	uint64_t interval = cur_tsc - produce_time;
	uint64_t new_token;

	if (unlikely(!produce_time)) {
		produce_time = cur_tsc;
		token_bucket -= len;
		return 1;
	}

	// notice that we should translate bit into Byte
	new_token = ((interval * RATE_LIMIT_M) >> 3) / (rte_get_tsc_hz() >> 20);
	if (new_token > TOKEN_INC_THRESH) {
		token_bucket += new_token;
		if (token_bucket > BUCKET_SIZE) {
			token_bucket = BUCKET_SIZE;
		}
		produce_time = cur_tsc;
	}
	
	if (token_bucket >= len) {
		token_bucket -= len;
		return 1;
	}
	else {
		return 0;
	}
}
#endif

static void deal_queue(cur_tsc)
{
#ifndef DIRECT_FWD
    const uint64_t delay_tsc = (rte_get_tsc_hz() + US_PER_S - 1) 
                                / US_PER_S * TX_DELAY_US;//added by shenyifan
    dq_pkt delay_queue[DELAY_QUEUE_LEN];
#endif
    dq_pkt tmp;
    unsigned dst_port; 
    long seqn=0;// seq in this flow
    long retransmition= 0;// num of packets retransed in this flow
    int dq_head = 0, dq_tail = 0, dq_count = 0;
    uint32_t tx_exp_send_seq = 0;
    uint32_t rx_exp_send_seq = 0;
    uint32_t last_send_seq = 0;
#ifdef BACKLOG_LIMIT
	uint64_t dq_backlog = 0;
#endif
    //struct rte_mbuf *history[5000];
    //int history_count=0;
#ifdef DROP_RAND
    srand((unsigned) time(NULL));
#endif

    /* edited by shenyifan, check dealy queue */
#ifndef DIRECT_FWD
    if (dq_count > DELAY_QUEUE_LEN) {
        printf("####shenyifan: delay queue overflow!\n");
        assert(0);
    }
    cur_tsc = rte_rdtsc();
    if(dq_count) {
//	        port_statistics[1].dq_count = dq_count;//monitor
        tmp = delay_queue[dq_head];    
        while (dq_count && (cur_tsc - tmp.timestamp > delay_tsc)) {
        // process every packet if it has been delayed for enough time
            struct tcphdr *tcph = get_tcphdr(tmp.m);
    #ifdef RATE_CONTROL
            //process token_bucket first
            if (!get_token(cur_tsc, tmp.m->pkt_len)) {
            //don't send packet if rate control token_bucket is empty
                break;
            }
        #ifdef BACKLOG_LIMIT
            dq_backlog -= tmp.m->pkt_len;
        #endif
    #endif
            dq_count--;
            if (dq_count) {
                dq_head = (dq_head + 1) % DELAY_QUEUE_LEN;
            }
            // process the packet
            if (tcph) {
                uint32_t send_seq = ntohl(tcph->seq);
                if (tcph->syn) { // init at every start of the flow
                    last_send_seq = send_seq - 1;
                    retransmition = 0;
                    seqn = 0;
                    for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++){
                    /* skip disabled ports */
                        if((l2fwd_enabled_port_mask & (1 << portid)) == 0)
                            continue;
                        port_statistics[portid].tx = 0;
                        port_statistics[portid].rx = 0;
                        port_statistics[portid].dropped = 0;
                        port_statistics[portid].outoforder = 0;
                        port_statistics[portid].retransmition = 0;
                        port_statistics[portid].drop = 0;
                        port_statistics[portid].dq_count = 0;
                    }
                    struct rte_eth_stats eth_stats;
                    unsigned i;
                    for (i = 0; i < rte_eth_dev_count(); i++) {
                        rte_eth_stats_get(i, &eth_stats);
                        eth_stats.ipackets = 0;
                        eth_stats.opackets = 0;
                        eth_stats.ierrors = 0;
                        eth_stats.oerrors = 0;
                        eth_stats.rx_nombuf = 0;
                    }
                }
                seqn++;// count the packets received during this flow
                if (TCP_SEQ_GT(send_seq, last_send_seq)) {
                // not retransmit packets
                    last_send_seq = send_seq;
    #ifdef DROP_PACKETS
        #ifdef DROP_BURST
                    if (seqn % DROP_CYCLE > (DROP_CYCLE-DROP_NUMBER)) 
        #endif
        #ifdef DROP_RAND
                    if (rand() % DROP_RATE == 0)
        #endif
                    {
                        rte_pktmbuf_free(tmp.m);
                        port_statistics[1].drop ++;
                        tmp.state = PKT_STATE_DROPPED;
        #ifdef DEBUG_DROP
                        fprintf(stderr,
                                "random drop seq:\t%lu,\tlast_snd:\t%lu\n"
                                , send_seq, last_send_seq);
        #endif
                    }
    #endif
                }
                else {// retransmit packets
                    retransmition++;
    #ifndef DEBUG_OUT_OF_ORDER
                    //don't count for two times if defined check_ooo
                    port_statistics[portid].retransmition++;
        #ifdef DEBUG_RETRANS
                    fprintf(stderr,
                            "retransmit seq:\t%lu,\tmax_snd:\t%lu\n",
                            send_seq, last_send_seq);
        #endif
    #endif
    #ifdef DROP_RETRANSMIT_PACKET
        #ifdef DROP_BURST
//						if (retransmition % DROP_CYCLE > 
//								(DROP_CYCLE - DROP_NUMBER)) 
                    if (seqn % DROP_CYCLE > (DROP_CYCLE-DROP_NUMBER)) 
        #endif
        #ifdef DROP_RAND
                    if (rand() % DROP_RATE == 0)
//						if (rand() % 100 == 0)
        #endif
                    {
                    // drop some of the retransmit packets
                        rte_pktmbuf_free(tmp.m);
                        port_statistics[1].drop ++;
                        tmp.state = PKT_STATE_DROPPED;
                    }
                    else {
                        tmp.state = PKT_STATE_RETRANS;
                    }
        #else//if drop packets except retransmition
                    tmp.state = PKT_STATE_RETRANS;
    #endif
                }
            }
            if (tmp.state != PKT_STATE_DROPPED) {
                l2fwd_send_packet(tmp.m, (uint8_t)tmp.dst_port);
            }
            // pop new packet from the head of the delay queue 
            tmp = delay_queue[dq_head];
            cur_tsc = rte_rdtsc();
        }
    }
#endif  /* end not direct_fwd */

}

static void insert_packet_into_delay_queue(unsigned dst_port, dq_pkt tmp, int dq_count, uint64_t dq_backlog, rte_mbuf *m);
{
    uint32_t snd_seq;
    uint32_t rcv_nxt_max = 0;
    struct tcphdr *tcph;
	uint32_t last_ack = 0;
    const uint64_t dq_backlog_limit = (uint64_t)BUCKET_SIZE + 
		(uint64_t)(TX_DELAY_US? TX_DELAY_US:100000) * 
		(uint64_t)(RATE_LIMIT_M << 17) / US_PER_S;
    /* edited by shenyifan, 
     * insert the packet into the delay queue 
     */
    if (dst_port == 0){
        tmp.timestamp = rte_rdtsc();
        tmp.m = m;
        tmp.dst_port = dst_port;
#ifdef DEBUG_OUT_OF_ORDER
        tcph = get_tcphdr(m);
        if (tcph) {
            snd_seq = ntohl(tcph->seq);
            if (ntohs(tcph->dest) != 5001) {
                fprintf(stderr, 
                        "####pick up the wrong packet!\n");
            }
            if (tcph->syn) {
                rcv_nxt_max = snd_seq + 1;
                tmp.state = PKT_STATE_NORMAL;
            }
            else {
                if (rcv_nxt_max == snd_seq) {// in order packet
                    int plen = payloadlen(m);
                    rcv_nxt_max = snd_seq + plen;
                    tmp.state = PKT_STATE_NORMAL;
                }
                else {
                    if (TCP_SEQ_GT(snd_seq, rcv_nxt_max)) {
                    // out of order packet
                        port_statistics[portid].outoforder++;
                        tmp.state = PKT_STATE_OUTOFORDER;
    #ifdef DEBUG_RETRANS
                        fprintf(stderr,
                                "ofo seq:\t%lu,\trcv_nxt:\t%lu\n",
                                snd_seq, rcv_nxt_max);
    #endif
                    }
                    else {// retransmition packet
                        port_statistics[portid].retransmition++;
                        tmp.state = PKT_STATE_RETRANS;
    #ifdef DEBUG_RETRANS
                        fprintf(stderr,
                            "retrans seq:\t%lu,\trcv_nxt:\t%lu\n",
                            snd_seq, rcv_nxt_max);
    #endif
                    }
                }
            }
        }
    #else
        tmp.state = PKT_STATE_NORMAL;
#endif
        //tmp.seq = tcph->seq;//debug by shenyifan

        if (dq_count) {
            if (dq_count == DELAY_QUEUE_LEN - 1) {
#ifdef DEBUG_DROP
                fprintf(stderr,
                    "####drop because of full dq packet%lu\n",
                    snd_seq);
#endif
                rte_pktmbuf_free(tmp.m);
                port_statistics[portid].dropped++;
            }
            else {
#ifdef BACKLOG_LIMIT
                dq_backlog += tmp.m->pkt_len;
                if (dq_backlog > dq_backlog_limit) {
#ifdef DEBUG_DROP
                    fprintf(stderr,
                            "####drop because of full dq backlog"
                            "\t%lu, backlog %llu\n", 
                            snd_seq, dq_backlog);
#endif
                    rte_pktmbuf_free(tmp.m);
                    port_statistics[portid].dropped++;
                    dq_backlog -= tmp.m->pkt_len;
                }
                else
#endif	
                {
                    dq_tail = (dq_tail + 1) % DELAY_QUEUE_LEN;
                    delay_queue[dq_tail] = tmp;
                    dq_count++;
                }
            }
        } 
        else {
            delay_queue[dq_tail] = tmp;
            dq_count++;
#ifdef BACKLOG_LIMIT
            dq_backlog += tmp.m->pkt_len;
#endif
        }
        if (dq_count >= DELAY_QUEUE_LEN) {
            printf("####delay queue packet overflow!\n");
            assert(0);
        }
#ifdef BACKLOG_LIMIT
        if (dq_backlog > dq_backlog_limit) {
            printf("####delay queue backlog overflow!\n");
            printf("backlog: %llu, limit:%llu\n", dq_backlog, 
                    dq_backlog_limit);
            assert(0);
        }
#endif
    }
    else {
#ifdef DEBUG_OUT_OF_ORDER
        tcph = get_tcphdr(m);
        if (tcph) {
            uint32_t ack_seq = ntohl(tcph->ack_seq);
            if (ntohs(tcph->source) != 5001) {
                fprintf(stderr, 
                        "####pick up the wrong packet!"
                        "from %u to %u\n",
                        ntohs(tcph->source), ntohs(tcph->dest));
            }
            if (tcph->syn) {
                last_ack = ack_seq;
            }
            else {
                if (ack_seq == last_ack){ //dup ack
                }
                else {
                    if (TCP_SEQ_GT(ack_seq, last_ack)) {
                    // ack new data
                        last_ack = ack_seq;
                    }
                    else {// very old ack
                        fprintf(stderr,
                            "####old ack: %lu, last_ack: %lu\n",
                            ack_seq, last_ack);
                        //assert(0);
                    }
                }
            }
        }
#endif
        l2fwd_send_packet(m, (uint8_t)dst_port);
    }

}


/* main processing loop */
static void
l2fwd_main_loop(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	unsigned i, j, portid, nb_rx;
	struct lcore_queue_conf *qconf;
    const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) 
                                / US_PER_S * BURST_TX_DRAIN_US;

	prev_tsc = 0;
	timer_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_port; i++) {

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
	}

	while (1) {
		cur_tsc = rte_rdtsc();//in fact it's not necessary
        deal_queue(cur_tsc);.//wanwenkai
		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {
			for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
				if (qconf->tx_mbufs[portid].len == 0)
					continue;
                l2fwd_send_burst(&lcore_queue_conf[lcore_id],
						 qconf->tx_mbufs[portid].len,
						 (uint8_t) portid);
				qconf->tx_mbufs[portid].len = 0;
			}

			/* if timer is enabled */
			if (timer_period > 0) {
				/* advance the timer */
				timer_tsc += diff_tsc;
				/* if timer has reached its timeout */
				if (unlikely(timer_tsc >= (uint64_t) timer_period)) {
					/* do this only on master core */
					if (lcore_id == rte_get_master_lcore()) {
						print_stats();
						/* reset the timer */
						timer_tsc = 0;
					}
				}
			}

			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_port; i++) {

			portid = qconf->rx_port_list[i];
	        dst_port = l2fwd_dst_ports[portid];
			nb_rx = rte_eth_rx_burst((uint8_t) portid, 0,
						 pkts_burst, MAX_PKT_BURST);

			port_statistics[portid].rx += nb_rx;

			for (j = 0; j < nb_rx; j++) {
                struct tcphdr *tcph;
                m = pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
#ifndef DIRECT_FWD		
                insert_packet_into_delay_queue(dst_port, tm, dq_count, dq_backlog, m);//wanwenkai	
#endif
                l2fwd_send_packet(m, (uint8_t)dst_port);
			}
		}
		fflush(stderr);
	}
}

static int
l2fwd_launch_one_lcore(__attribute__((unused)) void *dummy)
{
	l2fwd_main_loop();
	return 0;
}

/* display usage */
static void
l2fwd_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
	       "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	       "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
		   "  -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default, 86400 maximum)\n",
	       prgname);
}

static int
l2fwd_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (pm == 0)
		return -1;

	return pm;
}

static unsigned int
l2fwd_parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
	if (n == 0)
		return 0;
	if (n >= MAX_RX_QUEUE_PER_LCORE)
		return 0;

	return n;
}

static int
l2fwd_parse_timer_period(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n >= MAX_TIMER_PERIOD)
		return -1;

	return n;
}

/* Parse the argument given in the command line of the application */
static int
l2fwd_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:q:T:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			l2fwd_enabled_port_mask = l2fwd_parse_portmask(optarg);
			if (l2fwd_enabled_port_mask == 0) {
				printf("invalid portmask\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* nqueue */
		case 'q':
			l2fwd_rx_queue_per_lcore = l2fwd_parse_nqueue(optarg);
			if (l2fwd_rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* timer period */
		case 'T':
			timer_period = l2fwd_parse_timer_period(optarg) * 1000 * TIMER_MILLISECOND;
			if (timer_period < 0) {
				printf("invalid timer period\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* long options */
		case 0:
			l2fwd_usage(prgname);
			return -1;

		default:
			l2fwd_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 0; /* reset getopt lib */
	return ret;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == 0) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	struct rte_eth_dev_info dev_info;
	int ret;
	uint8_t nb_ports;
	uint8_t nb_ports_available;
	uint8_t portid, last_port;
	unsigned lcore_id, rx_lcore_id;
	unsigned nb_ports_in_mask = 0;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");

	/* create the mbuf pool */
	l2fwd_pktmbuf_pool =
		rte_mempool_create("mbuf_pool", NB_MBUF,
				   MBUF_SIZE, 32,
				   sizeof(struct rte_pktmbuf_pool_private),
				   rte_pktmbuf_pool_init, NULL,
				   rte_pktmbuf_init, NULL,
				   rte_socket_id(), 0);
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

	nb_ports = rte_eth_dev_count();
    if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

	/* reset l2fwd_dst_ports */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
		l2fwd_dst_ports[portid] = 0;
	last_port = 0;

	/*
	 * Each logical core is assigned a dedicated TX queue on each port.
	 */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		if (nb_ports_in_mask % 2) {
			l2fwd_dst_ports[portid] = last_port;
			l2fwd_dst_ports[last_port] = portid;
		}
		else
			last_port = portid;

		nb_ports_in_mask++;

		rte_eth_dev_info_get(portid, &dev_info);
	}
	if (nb_ports_in_mask % 2) {
		printf("Notice: odd number of ports in portmask.\n");
		l2fwd_dst_ports[last_port] = last_port;
	}

	rx_lcore_id = 0;
	qconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       lcore_queue_conf[rx_lcore_id].n_rx_port ==
		       l2fwd_rx_queue_per_lcore) {
			rx_lcore_id++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
		}

		if (qconf != &lcore_queue_conf[rx_lcore_id])
			/* Assigned a new logical core in the loop above. */
			qconf = &lcore_queue_conf[rx_lcore_id];

		qconf->rx_port_list[qconf->n_rx_port] = portid;
		qconf->n_rx_port++;
		printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned) portid);
	}

	nb_ports_available = nb_ports;

	/* Initialise each port */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %u\n", (unsigned) portid);
			nb_ports_available--;
			continue;
		}
		/* init port */
		printf("Initializing port %u... ", (unsigned) portid);
		fflush(stdout);
		ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, (unsigned) portid);

		rte_eth_macaddr_get(portid,&l2fwd_ports_eth_addr[portid]);

		/* init one RX queue */
		fflush(stdout);
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     NULL,
					     l2fwd_pktmbuf_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		/* init one TX queue on each port */
		fflush(stdout);
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				NULL);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
				ret, (unsigned) portid);

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		printf("done: \n");

		rte_eth_promiscuous_enable(portid);

		printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
				(unsigned) portid,
				l2fwd_ports_eth_addr[portid].addr_bytes[0],
				l2fwd_ports_eth_addr[portid].addr_bytes[1],
				l2fwd_ports_eth_addr[portid].addr_bytes[2],
				l2fwd_ports_eth_addr[portid].addr_bytes[3],
				l2fwd_ports_eth_addr[portid].addr_bytes[4],
				l2fwd_ports_eth_addr[portid].addr_bytes[5]);

		/* initialize port stats */
		memset(&port_statistics, 0, sizeof(port_statistics));
	}

	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled. Please set portmask.\n");
	}

	check_all_ports_link_status(nb_ports, l2fwd_enabled_port_mask);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}


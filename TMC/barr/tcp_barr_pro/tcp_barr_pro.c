/*
 * TCP Bandwidth Adaptive for Rate Control retransmission(BARR) algorithm
 */

#include <linux/module.h>
#include <net/tcp.h>
#include <linux/ktime.h>
#include "tcp_barr_pro.h"

/*
 * Get sum packets in circular queue.
 */
u32 SumPktsInQueue(struct sock *sk){

        struct tcp_sock *tp = tcp_sk(sk);
        int i = 0;
        u32 sum_pkts = 0;

        i = tp->ackqueue.front;
        while(i != tp->ackqueue.tail){

                sum_pkts += tp->ackqueue.acknode[i].packet_size;
                i = (i + 1)%QUEUEMAX;
        }

        return sum_pkts;
}

/*
 *Get interval time from first packet to last packet in circular queue.
 */
u32 SumTimeInQueue(struct sock *sk){

        struct tcp_sock *tp = tcp_sk(sk);
        
        u32 sum_time = 0;
		sum_time       = tp->ackqueue.acknode[(tp->ackqueue.tail - 1 + QUEUEMAX)%QUEUEMAX].arvl_time - 
						 tp->ackqueue.acknode[tp->ackqueue.front].arvl_time;

        return sum_time;
}


void tcp_barr_pkts_acked(struct sock *sk, u32 cnt, s32 rtt_us)
{
    struct barr_info *barr = inet_csk_retrans(sk);
    struct tcp_sock  *tp   = tcp_sk(sk);
   
    u32 interval = 0;
	u32 remainder = 0;
    if (rtt_us < 0) {
        return;
    }

    /* Never allow zero rtt or baseRTT */
    barr->cpy_RTT = usecs_to_jiffies(rtt_us + 1);

 /* Caculate bandwith */
    barr->sum_packet = SumPktsInQueue(sk);
    interval = SumTimeInQueue(sk);

	tp->sum_packet = barr->sum_packet;//test
	tp->barr_interval = interval;//test

    if (interval != 0) {
		remainder = do_div(barr->sum_packet,interval);
		barr->predict_bw = barr->sum_packet;
		if (remainder >= (interval >> 1)) {
			barr->predict_bw++;
		}
	}
}

/* Called by every ACK */
static void tcp_barr_cwnd_reduction(struct sock *sk, int newly_acked_sacked,
                                  int fast_rexmit)
{
    struct tcp_sock *tp = tcp_sk(sk);
    struct barr_info *barr = inet_csk_retrans(sk);
	
    /* Adjust CWND based on the bandwidth we predicted */
	u32 rtt = 0,remainder = 0;
	u32 temp = 0;

	rtt   = barr->cpy_RTT;
	if (barr->predict_bw != 0) {
		temp = (barr->predict_bw)*rtt;
		remainder    = do_div(temp,tp->mss_cache);
		/* test */
		tp->test_mss = tp->mss_cache;
		tp->bdp_temp = temp;
		tp->barr_rtt = rtt;
		/* end */
		if (tp->snd_cwnd < temp)
			tp->snd_cwnd = (remainder > (tp->mss_cache >> 1)?(temp + 1):(temp));
    }
}

void tcp_barr_init_cwnd_reduction(struct sock *sk, const bool set_ssthresh)
{
    struct tcp_sock *tp = tcp_sk(sk);
    //struct barr_info *barr = inet_csk_retrans(sk);

    tp->high_seq = tp->snd_nxt;
    tp->tlp_high_seq = 0;
    tp->snd_cwnd_cnt = 0;
    tp->prior_cwnd = tp->snd_cwnd;
    if (tp->ecn_flags & TCP_ECN_OK) {
        tp->ecn_flags |= TCP_ECN_QUEUE_CWR;
    }
}

void tcp_barr_end_cwnd_reduction(struct sock *sk)
{
    struct tcp_sock *tp = tcp_sk(sk);
    //struct barr_info *barr = inet_csk_retrans(sk);

    tp->snd_ssthresh = tp->snd_cwnd;
    tcp_ca_event(sk, CA_EVENT_COMPLETE_CWR);

}


/*
* Extend cwnd by ((highest_sack_seq -reordering) - snd_una).
* We use (highest_sack_seq-reordering) for the following reasons:
*     a. To keep consistency, we choose to send new packets instead
*        of forward retransmition if allowed by awnd+awnd_extension.
*     b. the number of *loss pacekts* between retransmit_high and
*        highest_sack_seq is tp->reordering. So the packets from
*        snd_una to (highest_sack_seq) must been retransmited or sacked.
*/

u32 tcp_barr_pro_get_awnd_extension(const struct tcp_sock *tp)
{
	u32 awnd_extension = 0;
//	u32 temp_bdp = 2000*tp->mss_cache;
	if (after(tcp_highest_sack_seq(tp), tp->reordering*tp->mss_cache + tp->snd_una) && tcp_is_sack(tp)) {
		awnd_extension = tcp_highest_sack_seq(tp) - (u32)tp->reordering*tp->mss_cache - tp->snd_una;
	//	if (awnd_extension < temp_bdp)
			awnd_extension = awnd_extension*sysctl_tcp_temp_threshold/sysctl_tcp_queue_threshold;
	}
	return awnd_extension;
}

static struct tcp_retrans_ops tcp_bandwidth_adaptive_rate_control = {
    .name                = "BARR-PRO",
    .owner               = THIS_MODULE,
    .init_cwnd_reduction = tcp_barr_init_cwnd_reduction,
    .cwnd_reduction      = tcp_barr_cwnd_reduction,
    .end_cwnd_reduction  = tcp_barr_end_cwnd_reduction,
    .pkts_acked          = tcp_barr_pkts_acked,
	.get_awnd_extension  = tcp_barr_pro_get_awnd_extension,

};

static int __init bandwidth_adaptive_rate_control_register(void)
{
    return tcp_register_retransmission_algorithm(&tcp_bandwidth_adaptive_rate_control);
}

static void __exit bandwidth_adaptive_rate_control_unregister(void)
{
    tcp_unregister_retransmission_algorithm(&tcp_bandwidth_adaptive_rate_control);
}

module_init(bandwidth_adaptive_rate_control_register);
module_exit(bandwidth_adaptive_rate_control_unregister);

MODULE_AUTHOR("Wen Kai Wan");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCP PQRC-PRO");

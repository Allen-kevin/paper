/*
 * TCP Self-Adaptive Rate Reduction(SARR) retransmission algorithm
 */

#include <linux/module.h>
#include <net/tcp.h>

#include "tcp_sarr.h"

u32 sarr_compute_diff(struct sock *sk)
{
    struct sarr_info *sarr = inet_csk_retrans(sk);
    struct tcp_sock *tp = tcp_sk(sk);
    u32 rtt, diff;
    
    /* 
     * This is the min RTT seen during the last RTT.
     */
    rtt = sarr->minRTT;

    // because cwnd vary greatly, so we choose to use packet_inflight to compute diff
    if (sarr->baseRTT == 0) diff = 0;
    else diff = tcp_packets_in_flight(tp) * (rtt - sarr->baseRTT) / sarr->baseRTT;

    return diff;
}

void tcp_sarr_pkts_acked(struct sock *sk, u32 cnt, s32 rtt_us)
{
    struct sarr_info *sarr = inet_csk_retrans(sk);
    u32 vrtt;

    if (rtt_us < 0) {
        return;
    }

    /* Check baseRTT */
    if (sarr->baseRTT == 0){
        sarr->baseRTT = 0x7fffffff;
    }

    /* Never allow zero rtt or baseRTT */
    vrtt = rtt_us + 1;

    /* Filter to find propagation delay: */
    if (vrtt < sarr->baseRTT) {
        sarr->baseRTT = vrtt;
    }

    /* Update minRTT with every valid RTT estimation when retransmission lost packets */
    if (sarr->minRTT != vrtt) {
        sarr->cntRTT++;
    }
    sarr->minRTT = vrtt;
}

/* Called by every ACK */
static void tcp_sarr_cwnd_reduction(struct sock *sk, int newly_acked_sacked,
                                  int fast_rexmit)
{
    struct tcp_sock *tp = tcp_sk(sk);
    struct sarr_info *sarr = inet_csk_retrans(sk);
    u32 diff, new_cwnd;

    if (after(tcp_highest_sack_seq(tp), sarr->beg_snd_nxt)){
        /* 
         * Set the begining of the next send window.
         * This make sure we only adjust cwnd per RTT 
         */
        sarr->beg_snd_nxt = tp->snd_nxt;


        /* cwnd can add beta at most when diff is zero */
        new_cwnd = tp->snd_cwnd + sysctl_tcp_congestion_threshold;

        diff = sarr_compute_diff(sk);
        if (diff > new_cwnd) {
        /* the variation of RTT is too violent, halving cwnd directly */
            tp->snd_cwnd = tp->snd_cwnd >> 1;
        } else {
            tp->snd_cwnd = max(tp->snd_cwnd >> 1, min(tp->snd_cwnd, new_cwnd -diff));
        }        
    }
}

void tcp_sarr_init_cwnd_reduction(struct sock *sk, const bool set_ssthresh)
{
    struct tcp_sock *tp = tcp_sk(sk);
    struct sarr_info *sarr = inet_csk_retrans(sk);

    tp->high_seq = tp->snd_nxt;
    tp->tlp_high_seq = 0;
    tp->snd_cwnd_cnt = 0;
    tp->prior_cwnd = tp->snd_cwnd;
    /* the next packet be sent is the Loss Packet */
    sarr->beg_snd_nxt = tp->snd_una;

    //printk(KERN_ALERT "[init_cwnd_reduction] cwnd = %u.\n", tp->snd_cwnd);
    if (tp->ecn_flags & TCP_ECN_OK) {
        tp->ecn_flags |= TCP_ECN_QUEUE_CWR;
    }
}

void tcp_sarr_end_cwnd_reduction(struct sock *sk)
{
    struct tcp_sock *tp = tcp_sk(sk);
    struct sarr_info *sarr = inet_csk_retrans(sk);

    //printk(KERN_ALERT "[end_cwnd_reduction] cwnd = %u.\n", tp->snd_cwnd);
    tp->snd_ssthresh = tp->snd_cwnd - 1;
    sarr->baseRTT = sarr->minRTT = 0x7fffffff;
    sarr->cntRTT = 0;

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
u32 tcp_sarr_pro_get_awnd_extension(const struct tcp_sock *tp)
{
    u32 awnd_extension = 0;

    if (after(tcp_highest_sack_seq(tp), tp->reordering + tp->snd_una + 3)
        && tcp_is_sack(tp)) {
        awnd_extension = tcp_highest_sack_seq(tp) - (u32)tp->reordering - tp->snd_una;
    }
    return awnd_extension;
}

static struct tcp_retrans_ops tcp_self_adaptive_rate_reduction = {
    .name                = "SARR-PRO",
    .owner               = THIS_MODULE,
    .init_cwnd_reduction = tcp_sarr_init_cwnd_reduction,
    .cwnd_reduction      = tcp_sarr_cwnd_reduction,
    .end_cwnd_reduction  = tcp_sarr_end_cwnd_reduction,
    .pkts_acked          = tcp_sarr_pkts_acked,
    .get_awnd_extension  = tcp_sarr_pro_get_awnd_extension,
};

static int __init self_adaptive_rate_reduction_register(void)
{
    return tcp_register_retransmission_algorithm(&tcp_self_adaptive_rate_reduction);
}

static void __exit self_adaptive_rate_reduction_unregister(void)
{
    tcp_unregister_retransmission_algorithm(&tcp_self_adaptive_rate_reduction);
}

module_init(self_adaptive_rate_reduction_register);
module_exit(self_adaptive_rate_reduction_unregister);

MODULE_AUTHOR("Perth Charles");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Self-Adaptive Rate Reduction");

/*
 * TCP Proportion Rate Reduction with awnd as counter(PRR-PRO) retransmission algorithm
 */

#include <linux/module.h>
#include <net/tcp.h>

static void tcp_prr_pro_cwnd_reduction(struct sock *sk, int newly_acked_sacked,
                                  int fast_rexmit)
{
    struct tcp_sock *tp = tcp_sk(sk);
    int sndcnt = 0;
    int delta = tp->snd_ssthresh - tcp_packets_in_flight(tp);

    tp->prr_delivered += newly_acked_sacked;
    if (tcp_packets_in_flight(tp) > tp->snd_ssthresh) {
        u64 dividend = (u64)tp->snd_ssthresh * tp->prr_delivered +
                       tp->prior_cwnd - 1;
        sndcnt = div_u64(dividend, tp->prior_cwnd) - tp->prr_out;
    } else {
        sndcnt = min_t(int, delta,
                       max_t(int, tp->prr_delivered - tp->prr_out,
                             newly_acked_sacked) + 1);
    }

    sndcnt = max(sndcnt, (fast_rexmit ? 1 : 0));
    tp->snd_cwnd = tcp_packets_in_flight(tp) + sndcnt;
}

void tcp_prr_pro_init_cwnd_reduction(struct sock *sk, const bool set_ssthresh)
{
    struct tcp_sock *tp = tcp_sk(sk);

    tp->high_seq = tp->snd_nxt;
    tp->tlp_high_seq = 0;
    tp->snd_cwnd_cnt = 0;
    tp->prior_cwnd = tp->snd_cwnd;
    tp->prr_delivered = 0;
    tp->prr_out = 0;
    
    //printk(KERN_ALERT " [init_reduction]: snd_una = %u, rcv_nxt = %u, retrans_out = %u, prr_delivered = %u, prr_out = %u, retrasmit_high = %u, high_seq = %u\n", 
    //       tp->snd_una, tp->rcv_nxt, tp->retrans_out, tp->prr_delivered, tp->prr_out, tp->retransmit_high, tp->high_seq);
    
    if (set_ssthresh) {
        tp->snd_ssthresh = inet_csk(sk)->icsk_ca_ops->ssthresh(sk);
    }
    if(tp->ecn_flags & TCP_ECN_OK){
        tp->ecn_flags |= TCP_ECN_QUEUE_CWR;
    }
}

void tcp_prr_pro_end_cwnd_reduction(struct sock *sk)
{
    struct tcp_sock *tp = tcp_sk(sk);

    //printk(KERN_ALERT " [end_reduction]: cwnd = %u, ssthresh = %u, snd_una = %u, rcv_nxt = %u, retrans_out = %u, prr_delivered = %u, prr_out = %u, retrasmit_high = %u, high_seq = %u\n", 
           //tp->snd_cwnd, tp->snd_ssthresh, tp->snd_una, tp->rcv_nxt, tp->retrans_out, tp->prr_delivered, tp->prr_out, tp->retransmit_high, tp->high_seq);

    /* Reset cwnd to ssthresh in CWR or Recovery (unless it's undone) */
    if (inet_csk(sk)->icsk_ca_state == TCP_CA_CWR ||
        (tp->undo_marker && tp->snd_ssthresh < TCP_INFINITE_SSTHRESH)) {
        tp->snd_cwnd = tp->snd_ssthresh;
        tp->snd_cwnd_stamp = tcp_time_stamp;
    }
    tcp_ca_event(sk, CA_EVENT_COMPLETE_CWR);
}

/* 
 * Extend cwnd by ((highest_sack_seq -reordering) - snd_una).
 * We use (highest_sack_seq-reordering) for the following reasons:
 *     a. To keep consistency, we choose to send new packets instead
 *        of forward retransmition if allowed by awnd+awnd_extensio.
 *     b. the number of *loss pacekts* between retransmit_high and
 *        highest_sack_seq is tp->reordering. So the packets from
 *        snd_una to (highest_sack_seq) must be retransmited or sacked.
 */
u32 tcp_prr_pro_get_awnd_extension(const struct tcp_sock *tp)
{
    u32 awnd_extension = 0;

    if (after(tcp_highest_sack_seq(tp), tp->reordering + tp->snd_una + 3)
        && tcp_is_sack(tp)) {
        awnd_extension = tcp_highest_sack_seq(tp) - (u32)tp->reordering - tp->snd_una;
    }
    return awnd_extension;
}
/*
bool tcp_prr_pro_is_awnd_permitted(const struct tcp_sock *tp,
                                   const struct sk_buff *skb,
                                   unsigned int cur_mss)
{
    u32 end_seq = TCP_SKB_CB(skb)->end_seq;
    u32 awnd_extension;

    if (skb->len > cur_mss) {
        end_seq = TCP_SKB_CB(skb)->seq + cur_mss;
    }

    awnd_extension = 0;
    if (after(tcp_highest_sack_seq(tp), tp->reordering + tp->snd_una)
        && tcp_is_sack(tp)) {
        awnd_extension = tcp_highest_sack_seq(tp) - (u32)tp->reordering - tp->snd_una;
        //printk(KERN_ALERT "RETRANS_MODULE: awnd_extension = %u highest_sack = %u reordering = %u snd_una = %u\n",
        //       awnd_extension, tcp_highest_sack_seq(tp), tp->reordering, tp->snd_una);
    }

    return !after(end_seq, tcp_wnd_end(tp) + awnd_extension); 
}
*/

static struct tcp_retrans_ops tcp_prr_pro = {
    .name                = "PRR-PRO",
    .owner               = THIS_MODULE,
    .init_cwnd_reduction = tcp_prr_pro_init_cwnd_reduction,
    .cwnd_reduction      = tcp_prr_pro_cwnd_reduction,
    .end_cwnd_reduction  = tcp_prr_pro_end_cwnd_reduction,
    .get_awnd_extension  = tcp_prr_pro_get_awnd_extension,
 //   .is_awnd_permitted     = tcp_prr_pro_is_awnd_permitted,
};

static int __init prr_pro_register(void)
{
    return tcp_register_retransmission_algorithm(&tcp_prr_pro);
}

static void __exit prr_pro_unregister(void)
{
    tcp_unregister_retransmission_algorithm(&tcp_prr_pro);
}

module_init(prr_pro_register);
module_exit(prr_pro_unregister);

MODULE_AUTHOR("Perth Charles");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Proportion Rate Reduction with awnd as counter");

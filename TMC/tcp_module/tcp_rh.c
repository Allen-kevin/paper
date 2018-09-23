/*
 * TCP Rate halving retransmission algorithm
 * 
 * This is based on the retransmission algorithm described in
 *     "TCP Rate-Halving with Bounding Parameters"
 *         See http://www.psc.edu/media/networking/tcprate-halvingwithboundingparameters.pdf
 */

#include <linux/module.h>
#include <net/tcp.h>

#define FLAG_DATA		0x01 /* Incoming frame contained data.		*/
#define FLAG_WIN_UPDATE		0x02 /* Incoming ACK was a window update.	*/
#define FLAG_DATA_ACKED		0x04 /* This ACK acknowledged new data.		*/
#define FLAG_RETRANS_DATA_ACKED	0x08 /* "" "" some of which was retransmitted.	*/
#define FLAG_SYN_ACKED		0x10 /* This ACK acknowledged SYN.		*/
#define FLAG_DATA_SACKED	0x20 /* New SACK.				*/
#define FLAG_ECE		0x40 /* ECE in this ACK				*/
#define FLAG_SLOWPATH		0x100 /* Do not skip RFC checks for window update.*/
#define FLAG_ORIG_SACK_ACKED	0x200 /* Never retransmitted data are (s)acked	*/
#define FLAG_SND_UNA_ADVANCED	0x400 /* Snd_una was changed (!= FLAG_DATA_ACKED) */
#define FLAG_DSACKING_ACK	0x800 /* SACK blocks contained D-SACK info */
#define FLAG_SACK_RENEGING	0x2000 /* snd_una advanced to a sacked seq */
#define FLAG_UPDATE_TS_RECENT	0x4000 /* tcp_replace_ts_recent() */

#define FLAG_ACKED		(FLAG_DATA_ACKED|FLAG_SYN_ACKED)
#define FLAG_NOT_DUP		(FLAG_DATA|FLAG_WIN_UPDATE|FLAG_ACKED)
#define FLAG_CA_ALERT		(FLAG_DATA_SACKED|FLAG_ECE)
#define FLAG_FORWARD_PROGRESS	(FLAG_ACKED|FLAG_DATA_SACKED)


/* Lower bound on congestion window is slow start threshold
 * unless congestion avoidance choice decides to overide it.
 */
static inline u32 tcp_cwnd_min(const struct sock *sk)
{
	const struct tcp_congestion_ops *ca_ops = inet_csk(sk)->icsk_ca_ops;

	return ca_ops->min_cwnd ? ca_ops->min_cwnd(sk) : tcp_sk(sk)->snd_ssthresh;
}


static void tcp_rh_cwnd_reduction(struct sock *sk, int newly_acked_sacked,
                                  int fast_rexmit)
{
    struct tcp_sock *tp = tcp_sk(sk);
    int decr = tp->snd_cwnd_cnt + 1;

    tp->snd_cwnd_cnt = decr & 1;
    decr >>= 1;

    if (decr && tp->snd_cwnd > tcp_cwnd_min(sk)) {
        tp->snd_cwnd -= decr;
    }

    tp->snd_cwnd = min(tp->snd_cwnd, tcp_packets_in_flight(tp) + 1);
    tp->snd_cwnd_stamp = tcp_time_stamp;
}

void tcp_rh_init_cwnd_reduction(struct sock *sk, const bool set_ssthresh)
{
    struct tcp_sock *tp = tcp_sk(sk);

    //printk(KERN_ALERT " [init_reduction]: snd_una = %u, retrans_out = %u, prr_delivered = %u, prr_out = %u, retrasmit_high = %u, high_seq = %u\n", 
           //tp->snd_una, tp->retrans_out, tp->prr_delivered, tp->prr_out, tp->retransmit_high, tp->high_seq);

    tp->tlp_high_seq = 0;
    tp->prior_cwnd = tp->snd_cwnd;
    tp->prr_delivered = 0;
    tp->prr_out = 0;

    tp->snd_ssthresh = inet_csk(sk)->icsk_ca_ops->ssthresh(sk);
    tp->snd_cwnd = min(tp->snd_cwnd,
                       tcp_packets_in_flight(tp) + 1U);
    tp->snd_cwnd_cnt = 0;
    tp->high_seq = tp->snd_nxt;
    tp->snd_cwnd_stamp = tcp_time_stamp;

    if (tp->ecn_flags & TCP_ECN_OK) {
        tp->ecn_flags |= TCP_ECN_QUEUE_CWR;
    }
}

void tcp_rh_end_cwnd_reduction(struct sock *sk)
{
    struct tcp_sock *tp = tcp_sk(sk);

    //printk(KERN_ALERT " [end_reduction]: snd_una = %u, retrans_out = %u, prr_delivered = %u, prr_out = %u, retrasmit_high = %u, high_seq = %u\n", 
           //tp->snd_una, tp->retrans_out, tp->prr_delivered, tp->prr_out, tp->retransmit_high, tp->high_seq);

    tp->snd_cwnd = min(tp->snd_cwnd, tp->snd_ssthresh);
    tp->snd_cwnd_stamp = tcp_time_stamp;
    tcp_ca_event(sk, CA_EVENT_COMPLETE_CWR);
}

static struct tcp_retrans_ops tcp_rate_havling = {
    .name                = "RH",
    .owner               = THIS_MODULE,
    .init_cwnd_reduction = tcp_rh_init_cwnd_reduction,
    .cwnd_reduction      = tcp_rh_cwnd_reduction,
    .end_cwnd_reduction  = tcp_rh_end_cwnd_reduction,
};

static int __init rate_halving_register(void)
{
    return tcp_register_retransmission_algorithm(&tcp_rate_havling);
}

static void __exit rate_halving_unregister(void)
{
    tcp_unregister_retransmission_algorithm(&tcp_rate_havling);
}

module_init(rate_halving_register);
module_exit(rate_halving_unregister);

MODULE_AUTHOR("Perth Charles");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Rate Halving");

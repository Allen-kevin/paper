/*
 * TCP SARR retransmission algorithm control interface
 */
#ifndef __TCP_SARR_H
#define __TCP_SARR_H 1

/* SARR variables */
struct sarr_info {
    u32 beg_snd_nxt;    /* begining of the next send packet sequence */
    u16 cntRTT;		/* # of RTTs measured within last RTT */
    u32 minRTT;		/* min of RTTs measured within last RTT (in usec) */
    u32 baseRTT;	/* the min of all Vegas RTT measurements seen (in usec) */
};
#endif

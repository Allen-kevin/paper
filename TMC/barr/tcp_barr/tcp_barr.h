/*
 * TCP BARR retransmission algorithm control interface
 */
#ifndef __TCP_BARR_H
#define __TCP_BARR_H 1

/* SARR variables */
struct barr_info {
    u16 cntRTT;		/* # of RTTs measured within last RTT */
//    u32 minRTT;		/* min of RTTs measured within last RTT (in usec) */

    u32 sum_packet;
    u32 predict_bw;
    u32 cpy_RTT;
};
#endif

/*
 *  tvheadend, RTP interface
 *  Copyright (C) 2007 Andreas Öman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "tvhead.h"
#include "channels.h"
#include "rtp.h"
#include "dispatch.h"

#include <ffmpeg/avformat.h>
#include <ffmpeg/random.h>

#define TSBLKS_PER_PKT 7


typedef struct th_rtp_pkt {
  TAILQ_ENTRY(th_rtp_pkt) trp_link;
  uint32_t trp_ts;  /* 90kHz clock */
  uint8_t trp_ts_valid;
  uint8_t trp_blocks;  /* no of 188 byte blocks stored so far */
  int64_t trp_time;
  th_rtp_streamer_t *trp_trs;

  void *trp_timer;

  unsigned char trp_pkt[12 + 188 * TSBLKS_PER_PKT];
} th_rtp_pkt_t;





static void
rtp_send(void *aux)
{
  th_rtp_pkt_t *pkt = aux;
  th_rtp_streamer_t *trs = pkt->trp_trs;

  TAILQ_REMOVE(&trs->trs_sendq, pkt, trp_link);

  sendto(trs->trs_fd, pkt->trp_pkt, pkt->trp_blocks * 188 + 12, 0, 
	 (struct sockaddr *)&trs->trs_dest, sizeof(struct sockaddr_in));

  free(pkt);

  pkt = TAILQ_FIRST(&trs->trs_sendq);
  if(pkt == NULL)
    return;

  pkt->trp_timer = stimer_add_hires(rtp_send, pkt, pkt->trp_time);
}




static void
rtp_schedule(th_rtp_streamer_t *trs, th_rtp_pkt_t *last, int64_t next_ts)
{
  int64_t now, sched;
  th_rtp_pkt_t *first, *pkt;
  uint32_t tsdelta, ts;
  int ipd, ipdu;
  int i = 0;
  int sendq_empty = !TAILQ_FIRST(&trs->trs_sendq);

  first = TAILQ_FIRST(&trs->trs_pktq);

  assert(first->trp_ts_valid);

  tsdelta = next_ts - first->trp_ts;
  ipd  = tsdelta / (trs->trs_qlen + 1);
  ipdu = (tsdelta * 1000000) / 90000 / (trs->trs_qlen + 1);

  now = getclock_hires();

  do {
    trs->trs_seq++;

    pkt = TAILQ_FIRST(&trs->trs_pktq);
    TAILQ_REMOVE(&trs->trs_pktq, pkt, trp_link);
    trs->trs_qlen--;
  
    pkt->trp_pkt[0] = 0x80;
    pkt->trp_pkt[1] = 33;
    pkt->trp_pkt[2] = trs->trs_seq >> 8;
    pkt->trp_pkt[3] = trs->trs_seq;

    ts = first->trp_ts + i * ipd;

    pkt->trp_pkt[4] = ts >> 24;
    pkt->trp_pkt[5] = ts >> 16;
    pkt->trp_pkt[6] = ts >>  8;
    pkt->trp_pkt[7] = ts;

    pkt->trp_pkt[8]  = 0;
    pkt->trp_pkt[9]  = 0;
    pkt->trp_pkt[10] = 0;
    pkt->trp_pkt[11] = 0;

    sched = now + i * ipdu;

    pkt->trp_time = sched;

    TAILQ_INSERT_TAIL(&trs->trs_sendq, pkt, trp_link);
    i++;
    pkt->trp_trs = trs;
    
  } while(pkt != last);

  printf("Sending %d packets\n", i);

  if(sendq_empty)
    rtp_send(TAILQ_FIRST(&trs->trs_sendq));
}



void 
rtp_streamer(struct th_subscription *s, uint8_t *buf, th_pid_t *pi,
	     int64_t pcr)
{
  th_rtp_streamer_t *trs = s->ths_opaque;
  th_rtp_pkt_t *pkt;

  if(buf == NULL)
    return;

  pkt = TAILQ_LAST(&trs->trs_pktq, th_rtp_pkt_queue);

  if(trs->trs_qlen > 1 && pcr != AV_NOPTS_VALUE) {
    rtp_schedule(trs, pkt, pcr);
    pkt = TAILQ_LAST(&trs->trs_pktq, th_rtp_pkt_queue);
  }

  if(pkt == NULL && pcr == AV_NOPTS_VALUE)
    return; /* make sure first packet in queue always has pcr */

  if(pkt == NULL || pkt->trp_blocks == TSBLKS_PER_PKT) {
    pkt = malloc(sizeof(th_rtp_pkt_t));
    pkt->trp_blocks = 0;
    pkt->trp_ts_valid = 0;
    pkt->trp_trs = trs;
    pkt->trp_timer = NULL;

    TAILQ_INSERT_TAIL(&trs->trs_pktq, pkt, trp_link);
    trs->trs_qlen++;
  }

  if(pkt->trp_ts_valid == 0 && pcr != AV_NOPTS_VALUE) {
    pkt->trp_ts = pcr;
    pkt->trp_ts_valid = 1;
  }
  
  memcpy(&pkt->trp_pkt[12 + pkt->trp_blocks * 188], buf, 188);
  pkt->trp_blocks++;
}



void
rtp_streamer_init(th_rtp_streamer_t *trs, int fd, struct sockaddr_in *dst)
{
  printf("RTP: Streaming to %s:%d (fd = %d)\n",
	 inet_ntoa(dst->sin_addr), ntohs(dst->sin_port), fd);

  trs->trs_fd = fd;
  trs->trs_dest = *dst;

  TAILQ_INIT(&trs->trs_pktq);
  TAILQ_INIT(&trs->trs_sendq);
  trs->trs_qlen = 0;
  trs->trs_seq = 0;
}




void
rtp_streamer_deinit(th_rtp_streamer_t *trs)
{
  th_rtp_pkt_t *pkt;

  while((pkt = TAILQ_FIRST(&trs->trs_pktq)) != NULL) {
    TAILQ_REMOVE(&trs->trs_pktq, pkt, trp_link);
    free(pkt);
  }

  while((pkt = TAILQ_FIRST(&trs->trs_sendq)) != NULL) {
    TAILQ_REMOVE(&trs->trs_sendq, pkt, trp_link);
    if(pkt->trp_timer != NULL)
      stimer_del(pkt->trp_timer);
    free(pkt);
  }
}

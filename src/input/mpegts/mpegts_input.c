/*
 *  Tvheadend - MPEGTS input source
 *  Copyright (C) 2013 Adam Sutton
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

#include "input/mpegts.h"
#include "tsdemux.h"
#include "packet.h"
#include "streaming.h"
#include "atomic.h"

#include <pthread.h>

const idclass_t mpegts_input_class =
{
  .ic_class      = "mpegts_input",
  .ic_caption    = "MPEGTS Input",
  .ic_properties = (const property_t[]){
  }
};

size_t
mpegts_input_recv_packets
  ( mpegts_input_t *mi, uint8_t *tsb, size_t l,
    int64_t *pcr, uint16_t *pcr_pid )
{
  int len = l; // TODO: fix ts_resync() to remove this
  int i = 0, table_wakeup = 0;
  tvhtrace("mpegts", "recv_packets tsb=%p, len=%d, pcr=%p, pcr_pid=%p",
           tsb, (int)len, pcr, pcr_pid);
  
  /* Not enough data */
  if (len < 188) return 0;
  
  /* Streaming - lock mutex */
  pthread_mutex_lock(&mi->mi_delivery_mutex);

  /* Raw stream */
  if (LIST_FIRST(&mi->mi_streaming_pad.sp_targets) != NULL) {
    streaming_message_t sm;
    pktbuf_t *pb = pktbuf_alloc(tsb, len);
    memset(&sm, 0, sizeof(sm));
    sm.sm_type = SMT_MPEGTS;
    sm.sm_data = pb;
    streaming_pad_deliver(&mi->mi_streaming_pad, &sm);
    pktbuf_ref_dec(pb);
  }
  
  /* Process */
  while ( len >= 188 ) {
    //printf("tsb[%d] = %02X\n", i, tsb[i]);

    /* Sync */
    if ( tsb[i] == 0x47 ) {
      int pid = ((tsb[i+1] & 0x1f) << 8) | tsb[i+2];

      /* SI data */
      if (mi->mi_table_filter[pid]) {
        printf("pid = %04X\n", pid);
        if (!(tsb[i+1] & 0x80)) {
          mpegts_table_feed_t *mtf = malloc(sizeof(mpegts_table_feed_t));
          memcpy(mtf->mtf_tsb, tsb+i, 188);
          TAILQ_INSERT_TAIL(&mi->mi_table_feed, mtf, mtf_link);
          table_wakeup = 1;
        }
      
      /* Other */
      } else {
        service_t *s;
        int64_t tpcr = PTS_UNSET;
        LIST_FOREACH(s, &mi->mi_transports, s_active_link) {
          ts_recv_packet1((mpegts_service_t*)s, tsb+i, NULL);
        }
        if (tpcr != PTS_UNSET) {
 //         printf("pcr = %p, %"PRId64" pcr_pid == %p, %d, pid = %d\n", pcr, tpcr, pcr_pid, *pcr_pid, pid);
          if (pcr && pcr_pid){// && (*pcr_pid == 0 || *pcr_pid == pid)) {
            printf("SET\n");
            *pcr_pid = pid;
            *pcr     = tpcr;
            printf("pcr set to %"PRId64"\n", tpcr);
          }
        }
      }

      i   += 188;
      len -= 188;
    
    /* Re-sync */
    } else {
      // TODO: set flag (to avoid spam)
      tvhlog(LOG_DEBUG, "mpegts", "%s ts sync lost", "TODO");
      if (ts_resync(tsb, &len, &i)) break;
      tvhlog(LOG_DEBUG, "mpegts", "%s ts sync found", "TODO");
    }

  }

  /* Wake table */
  if (table_wakeup)
    pthread_cond_signal(&mi->mi_table_feed_cond);

  pthread_mutex_unlock(&mi->mi_delivery_mutex);

  /* Bandwidth monitoring */
  atomic_add(&mi->mi_bytes, i);
  
  /* Reset buffer */
  if (len) memmove(tsb, tsb+i, len);

  return len;
}

mpegts_input_t*
mpegts_input_create0 ( const char *uuid )
{
  mpegts_input_t *mi = idnode_create(mpegts_input, uuid);

  /* Init mutex */
  pthread_mutex_init(&mi->mi_delivery_mutex, NULL);
  
  /* Table input */
  TAILQ_INIT(&mi->mi_table_feed);
  pthread_cond_init(&mi->mi_table_feed_cond, NULL);

  /* Init input thread control */
  mi->mi_thread_pipe.rd = mi->mi_thread_pipe.wr = -1;

  return mi;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/

/*
 * AVC helper functions for muxers
 * Copyright (c) 2006 Baptiste Coudurier <baptiste.coudurier@smartjog.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avc.h"

static const uint8_t *
avc_find_startcode(const uint8_t *p, const uint8_t *end)
{
  int i;
	
  uint32_t sc=0xFFFFFFFF;
  size_t len = (end -p)+1;
  for (i=0;i<len;i++)
    {
      sc = (sc <<8) | p[i];
      if((sc & 0xffffff00) == 0x00000100) {
	return p+i-3;
      }
		
    }
  return end;
}
static int
avc_parse_nal_units(sbuf_t *sb, const uint8_t *buf_in, int size)
{
  const uint8_t *p = buf_in;
  const uint8_t *end = p + size;
  const uint8_t *nal_start, *nal_end; 

  //printf("CONVERT SIZE %d\n", size);

  size = 0;
  nal_start = avc_find_startcode(p, end);
  while (nal_start < end) {
    while(!*(nal_start++));
    nal_end = avc_find_startcode(nal_start, end);
    /*printf("%4d bytes  %5d : %d\n", nal_end - nal_start,
      nal_start - buf_in,
      nal_end   - buf_in);*/

    int l = nal_end - nal_start;

    if (l) {
      sbuf_put_be32(sb, l);
      sbuf_append(sb, nal_start, l);
      size += 4 + l;
    }
    nal_start = nal_end;
  }
  return size;
}


static int
avc_parse_nal_units_buf(const uint8_t *buf_in, uint8_t **buf, int *size)
{
  sbuf_t sb;
  sbuf_init(&sb);
  avc_parse_nal_units(&sb, buf_in, *size);

  free(*buf);
  *buf = sb.sb_data;
  *size = sb.sb_ptr;
  return 0;
}


static uint32_t 
RB32(const uint8_t *d)
{
  return (d[0] << 24) | (d[1] << 16) | (d[2] << 8) | d[3];
}

static uint32_t 
RB24(const uint8_t *d)
{
  return (d[0] << 16) | (d[1] << 8) | d[2];
}


static int
isom_write_avcc(sbuf_t *sb, const uint8_t *data, int len)
{
  if (len > 6) {
    /* check for h264 start code */
    if (RB32(data) == 0x00000001 ||
	RB24(data) == 0x000001) {
      uint8_t *buf=NULL, *end, *start;
      uint32_t *sps_size_array=0, *pps_size_array=0;
      uint32_t pps_count=0,sps_count=0;
      uint8_t **sps_array=0, **pps_array=0;
      int i;

      int ret = avc_parse_nal_units_buf(data, &buf, &len);
      if (ret < 0)
	return ret;
      start = buf;
      end = buf + len;

      /* look for sps and pps */
      while (buf < end) {
	unsigned int size;
	uint8_t nal_type;
	size = RB32(buf);
	nal_type = buf[4] & 0x1f;
	if (nal_type == 7) { /* SPS */
	  sps_array = realloc(sps_array,sizeof(uint8_t*)*(sps_count+1));
	  sps_size_array = realloc(sps_size_array,sizeof(uint32_t)*(sps_count+1));
	  sps_array[sps_count] = buf + 4;
	  sps_size_array[sps_count] = size;
	  sps_count++;
	} else if (nal_type == 8) { /* PPS */
	  pps_size_array = realloc(pps_size_array,sizeof(uint32_t)*(pps_count+1));
	  pps_array = realloc(pps_array,sizeof (uint8_t*)*(pps_count+1));
	  pps_array[pps_count] = buf + 4;
	  pps_size_array[pps_count] = size;
	  pps_count++;
	}
	buf += size + 4;
      }
      if(!sps_count || !pps_count) {
	free(start);
	if (sps_count)
	  free(sps_array);
	if (pps_count)
	  free(pps_array);
	return -1;
      }

      sbuf_put_byte(sb, 1); /* version */
      sbuf_put_byte(sb, sps_array[0][1]); /* profile */
      sbuf_put_byte(sb, sps_array[0][2]); /* profile compat */
      sbuf_put_byte(sb, sps_array[0][3]); /* level */
      sbuf_put_byte(sb, 0xff); /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
      sbuf_put_byte(sb, 0xe0+sps_count); /* 3 bits reserved (111) + 5 bits number of sps (00001) */
      for (i=0;i<sps_count;i++) {
	sbuf_put_be16(sb, sps_size_array[i]);
	sbuf_append(sb, sps_array[i], sps_size_array[i]);
      }

      sbuf_put_byte(sb, pps_count); /* number of pps */
      for (i=0;i<pps_count;i++) {
	sbuf_put_be16(sb, pps_size_array[i]);
	sbuf_append(sb, pps_array[i], pps_size_array[i]);
      }
      free(start);

      if (sps_count)
	free(sps_array);
      if (pps_count)
	free(pps_array);
    } else {
      sbuf_append(sb, data, len);
    }
  }
  return 0;
}




th_pkt_t *
avc_convert_pkt(th_pkt_t *src)
{
  th_pkt_t *pkt = malloc(sizeof(th_pkt_t));
  *pkt = *src;
  pkt->pkt_refcount = 1;
  pkt->pkt_header = NULL;
  pkt->pkt_payload = NULL;

  pkt->pkt_payload = malloc(sizeof(pktbuf_t));
  pkt->pkt_payload->pb_refcount=1;
  if (src->pkt_header) {
    sbuf_t headers;
    sbuf_init(&headers);
    
    isom_write_avcc(&headers, pktbuf_ptr(src->pkt_header),
		    pktbuf_len(src->pkt_header));
    pkt->pkt_header = pktbuf_make(headers.sb_data, headers.sb_ptr);
  }

  sbuf_t payload;
  sbuf_init(&payload);

  if(src->pkt_header)
    avc_parse_nal_units(&payload, pktbuf_ptr(src->pkt_header),
			pktbuf_len(src->pkt_header));

  avc_parse_nal_units(&payload, pktbuf_ptr(src->pkt_payload),
		      pktbuf_len(src->pkt_payload));
  
  pkt->pkt_payload = pktbuf_make(payload.sb_data, payload.sb_ptr);
  pkt_ref_dec(src);
  return pkt;
}


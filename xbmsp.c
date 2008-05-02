/*
 *  tvheadend, XBMSP interface
 *  Copyright (C) 2008 Andreas Öman
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

#include <pthread.h>
#include <assert.h>
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
#include "subscriptions.h"
#include "dispatch.h"
#include "xbmsp.h"
#include "tcp.h"
#include "access.h"

#define XBMSP_FILEFORMAT "ts"

static LIST_HEAD(, xbmsp) xbmsp_sessions;

extern AVOutputFormat mpegts_muxer;


/**
 * Function for delivery of data.
 * We try to respond to any pending read
 */
static void
xbmsp_output_file(void *opaque)
{
  xbmsp_subscrption_t *xs = opaque;
  tffm_fifo_t *tf = xs->xs_fifo;
  xbmsp_t *xbmsp = xs->xs_xbmsp;
  uint32_t msgid = xs->xs_pending_read_msgid;
  tcp_queue_t tq;
  int rem, tlen, len = xs->xs_pending_read_size;
  uint8_t buf[13];
  tffm_fifo_pkt_t *pkt, *n;
  
  if(len == 0 || tf->tf_pktq_len < len)
    return;

  tlen = len + 4 + 4 + 1;

  buf[0] = tlen >> 24;
  buf[1] = tlen >> 16;
  buf[2] = tlen >> 8;
  buf[3] = tlen;
  buf[4] = XBMSP_PACKET_FILE_CONTENTS;
  buf[5] = msgid >> 24;
  buf[6] = msgid >> 16;
  buf[7] = msgid >> 8;
  buf[8] = msgid;
  buf[9] = len >> 24;
  buf[10] = len >> 16;
  buf[11] = len >> 8;
  buf[12] = len;

  tcp_init_queue(&tq, -1);
  tcp_qput(&tq, buf, 13);

   while(len > 0) {
    pkt = TAILQ_FIRST(&tf->tf_pktq);
    assert(pkt != NULL);
    
    if(len >= pkt->tfp_pktsize) {
      /* Consume entire packet */
      tcp_qput(&tq, pkt->tfp_buf, pkt->tfp_pktsize);
    } else {
      /* Partial, create new packet at front with remaining data */
      tcp_qput(&tq, pkt->tfp_buf, len);
      rem = pkt->tfp_pktsize - len;
      n = malloc(sizeof(tffm_fifo_pkt_t) + rem);
      n->tfp_pktsize = rem;
      memcpy(n->tfp_buf, pkt->tfp_buf + len, rem);
      TAILQ_INSERT_HEAD(&tf->tf_pktq, n, tfp_link);

      tf->tf_pktq_len += rem;
    }
    len -= pkt->tfp_pktsize;
    tf->tf_pktq_len -= pkt->tfp_pktsize;
    TAILQ_REMOVE(&tf->tf_pktq, pkt, tfp_link);
    free(pkt);
  }

  xs->xs_pending_read_size = 0;
  xs->xs_pending_read_msgid = 0;
  tcp_output_queue(&xbmsp->xbmsp_tcp_session, NULL, &tq);  
}




/**
 * Called when a subscription gets/loses access to a transport
 */
static void
xbmsp_subscription_callback(struct th_subscription *s,
			    subscription_event_t event, void *opaque)
{
  th_transport_t *t = s->ths_transport;
  xbmsp_subscrption_t *xs = opaque;
  xbmsp_t *xbmsp = xs->xs_xbmsp;
  th_ffmuxer_t *tffm = &xs->xs_tffm;
  th_muxer_t *tm = &tffm->tffm_muxer;
  th_muxstream_t *tms;
  AVFormatContext *fctx;
  int err;

  switch(event) {
  case TRANSPORT_AVAILABLE:
    tm->tm_opaque = tffm;
    tm->tm_new_pkt = tffm_packet_input;

    xs->xs_fifo = tffm_fifo_create(xbmsp_output_file, xs);

    fctx = av_alloc_format_context();
    fctx->oformat = &mpegts_muxer;
    
    err = url_fopen(&fctx->pb, tffm_filename(xs->xs_fifo), URL_WRONLY);
    if(err < 0)
      abort(); /* Should not happen, we've just created the fifo */

    tffm_open(tffm, t, fctx, xbmsp->xbmsp_logname);
    LIST_INSERT_HEAD(&t->tht_muxers, tm, tm_transport_link);
    tffm_set_state(tffm, TFFM_WAIT_AUDIO_LOCK);
    break;

  case TRANSPORT_UNAVAILABLE:
    LIST_REMOVE(tm, tm_transport_link);
    tffm_close(tffm);

    /* Destroy muxstreams, XXX: Should be in tffm_close() */

    while((tms = LIST_FIRST(&tm->tm_streams)) != NULL) {
      LIST_REMOVE(tms, tms_muxer_link0);
      free(tms);
    }
    tffm_fifo_destroy(xs->xs_fifo);
    break;
  }
}

/**
 * Close subscription given by 'handle', free all data
 * If handle == 0, close all.
 * return -1 if we fail to locate it. 
 */
static int
xbmsp_close_subscription(xbmsp_t *xbmsp, uint32_t handle)
{
  xbmsp_subscrption_t *xs, *next;

  for(xs = LIST_FIRST(&xbmsp->xbmsp_subscriptions); xs != NULL; xs = next) {
    next = LIST_NEXT(xs, xs_link);

    if(xs->xs_handle == handle || handle == 0) {
      subscription_unsubscribe(xs->xs_subscription);
      LIST_REMOVE(xs, xs_link);
      free(xs);

      if(handle != 0)
	return 0;
    }
  }
  
  return handle == 0 ? 0 : -1;
}



/**
 * Add an entry to a dirhandle
 */
static void
xbmsp_dir_add_entry(xbmsp_dirhandle_t *xdh, const char *name, 
		    const char *displayname, const char *type)
{
  xbmsp_direntry_t *xde;
  char xmlbuf[1000];

  xde = malloc(sizeof(xbmsp_direntry_t));
  xde->xde_filename = strdup(name);

  snprintf(xmlbuf, sizeof(xmlbuf),
	   "<DIRECTORYITEM>"
	   "<NAME>%s</NAME>"
	   "<ATTRIB>%s</ATTRIB>"
	   "</DIRECTORYITEM>", displayname, type);
  
  xde->xde_xmlmeta = strdup(xmlbuf);
  TAILQ_INSERT_TAIL(&xdh->xdh_entries, xde, xde_link);
}

/**
 *
 */
static th_channel_group_t *
xbmsp_cur_channel_group(xbmsp_t *xbmsp)
{
  th_channel_group_t *tcg;
  
  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
    if(tcg->tcg_hidden)
      continue;
    if(!strcmp(tcg->tcg_name, xbmsp->xbmsp_wd))
      return tcg;
  }
  return NULL;
}


/**
 * Populate a dirhandle with direntries based on the current
 * working directory
 */
static int
xbmsp_dir_populate(xbmsp_t *xbmsp, xbmsp_dirhandle_t *xdh, const char *filter)
{
  th_channel_group_t *tcg;
  th_channel_t *ch;
  char name[100];

  if(xbmsp->xbmsp_wd[0] == 0) {
    /* root dir */

    TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
      if(tcg->tcg_hidden)
	continue;

      if(filter != NULL && strcmp(tcg->tcg_name, filter))
	continue;

      xbmsp_dir_add_entry(xdh, tcg->tcg_name, tcg->tcg_name, "directory");
    }
  } else {
    
    if((tcg = xbmsp_cur_channel_group(xbmsp)) == NULL)
      return -1;

    TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
      if(LIST_FIRST(&ch->ch_transports) == NULL)
	continue;

      if(filter != NULL && strcmp(ch->ch_name, filter))
	continue;

      snprintf(name, sizeof(name), "%s." XBMSP_FILEFORMAT, ch->ch_name);

      xbmsp_dir_add_entry(xdh, name, ch->ch_name, "stream");
    }
  }
  return 0;
}

/**
 * Close dirhandle given by 'handle', free all data
 * If handle == 0, close all.
 * Return -1 if we fail to locate it.
 */
static int
xbmsp_close_dirhandle(xbmsp_t *xbmsp, uint32_t handle)
{
  xbmsp_dirhandle_t *xdh, *next;
  xbmsp_direntry_t *xde;

  for(xdh = LIST_FIRST(&xbmsp->xbmsp_dirhandles); xdh != NULL; xdh = next) {
    next = LIST_NEXT(xdh, xdh_link);
    if(xdh->xdh_handle == handle || handle == 0) {

      while((xde = TAILQ_FIRST(&xdh->xdh_entries)) != NULL) {
	TAILQ_REMOVE(&xdh->xdh_entries, xde, xde_link);
	free((void *)xde->xde_filename);
	free((void *)xde->xde_xmlmeta);
	free(xde);
      }
      LIST_REMOVE(xdh, xdh_link);
      free(xdh);

      if(handle != 0)
	return 0;
    }
  }
  
  return handle == 0 ? 0 : -1;
}




/**
 * xbmsp_cdup() - Change to one directory up (cd ..)
 */
static const char *
xbmsp_cdup(xbmsp_t *xbmsp)
{
  char *wd = xbmsp->xbmsp_wd;
  char *r;

  r = strrchr(wd, '/');
  if(r == NULL) {
    *wd = 0;
  } else {
    *r = 0;
  }
  return NULL;
}

/**
 * xbmsp_cdroot() - Change to root (cd /)
 */
static const char *
xbmsp_cdroot(xbmsp_t *xbmsp)
{
  free(xbmsp->xbmsp_wd);
  xbmsp->xbmsp_wd = strdup("");
  return NULL;
}

/**
 * xbmsp_cddown() - Change to root (cd dir)
 */
static const char *
xbmsp_cddown(xbmsp_t *xbmsp, const char *dir)
{
  th_channel_group_t *tcg;
 
  if(xbmsp->xbmsp_wd[0] == 0) {
    /* root dir */

    TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
      if(tcg->tcg_hidden)
	continue;

      if(!strcmp(dir, tcg->tcg_name))
	break;
    }

    if(tcg == NULL)
      return "%s -- No such file or directory";

    free(xbmsp->xbmsp_wd);
    xbmsp->xbmsp_wd = strdup(tcg->tcg_name);
    return NULL;
  }
  return "%s -- No such file or directory";
}



/**
 * Send a message back
 */
static void
xbmsp_send_msg(xbmsp_t *xbmsp, uint8_t type, uint32_t msgid, 
	       uint8_t *payload, int payloadlen)
{
  uint8_t  buf[9];
  tcp_queue_t tq;

  int tlen = payloadlen + 5;

  buf[0] = tlen >> 24;
  buf[1] = tlen >> 16;
  buf[2] = tlen >> 8;
  buf[3] = tlen;
  buf[4] = type;
  buf[5] = msgid >> 24;
  buf[6] = msgid >> 16;
  buf[7] = msgid >> 8;
  buf[8] = msgid;

  tcp_init_queue(&tq, -1);
  tcp_qput(&tq, buf, 9);
  if(payloadlen > 0) {
    if(payload == NULL) {
      payload = alloca(payloadlen);
      memset(payload, 0, payloadlen);
    }
    tcp_qput(&tq, payload, payloadlen);
  }
  tcp_output_queue(&xbmsp->xbmsp_tcp_session, NULL, &tq);
}


/**
 * Send an error code back
 */
static void
xbmsp_send_err(xbmsp_t *xbmsp, uint32_t msgid, uint8_t errcode,
	       const char *errfmt, ...)
{
  int slen;
  uint8_t *buf;
  char errbuf[200];
  va_list ap;

  va_start(ap, errfmt);
  vsnprintf(errbuf, sizeof(errbuf), errfmt, ap);
  va_end(ap);

  syslog(LOG_INFO, "%s: %s", xbmsp->xbmsp_logname, errbuf);

  slen = strlen(errbuf);

  buf = alloca(slen + 5);
  buf[0] = errcode;
  buf[1] = slen >> 24;
  buf[2] = slen >> 16;
  buf[3] = slen >> 8;
  buf[4] = slen;
  memcpy(buf + 5, errbuf, slen);

  xbmsp_send_msg(xbmsp, XBMSP_PACKET_ERROR, msgid, buf, slen + 5);
}

/**
 * Send a handle id back
 */
static void
xbmsp_send_handle(xbmsp_t *xbmsp, uint32_t msgid, uint32_t handle)
{
  uint8_t buf[4];

  buf[0] = handle >> 24;
  buf[1] = handle >> 16;
  buf[2] = handle >> 8;
  buf[3] = handle;

  xbmsp_send_msg(xbmsp, XBMSP_PACKET_HANDLE, msgid, buf, 4);
}


/**
 * Extract a string from the current buffer adjusting the pointers
 * sent in
 */
static char *
xbmsp_extract_string(xbmsp_t *xbmsp, uint8_t **bufp, int *lenp)
{
  uint8_t *buf = *bufp;
  uint32_t slen;
  char *str;
  if(*lenp < 4)
    return NULL;

  slen = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

  if(slen == 0)
    return strdup(""); /* empty string */

  *lenp -= 4;
  buf += 4;
  
  if(slen > *lenp)
    return NULL; /* String exceeds past end of buffer */

  str = malloc(slen + 1);
  memcpy(str, buf, slen);
  str[slen] = 0;
  *bufp = buf + slen;
  return str;
}

/**
 * Extract an u32 from the current buffer and adjust the pointers
 * sent in
 */
static int
xbmsp_extract_u32(xbmsp_t *xbmsp, uint8_t **bufp, int *lenp, uint32_t *res)
{
  uint8_t *buf = *bufp;
  if(*lenp < 4)
    return -1;

  *res = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  *lenp -= 4;
  *bufp += 4;
  return 0;
}

/**
 * Handle XBMSP_PACKET_AUTHENTICATION_INIT
 */
static int
xbmsp_input_authentication_init(xbmsp_t *xbmsp, uint32_t msgid, 
				uint8_t *buf, int len)
{
  char *authtype;

  authtype = xbmsp_extract_string(xbmsp, &buf, &len);
  if(authtype == NULL)
    return EBADMSG;

  if(strcmp(authtype, "password")) {
    xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_AUTHENTICATION_FAILED,
		   "Authentication \"%s\" type not supported", authtype);
    free(authtype);
    return 0;
  }
  free(authtype);

  /* Generate handle and send a positive response back. */
  xbmsp->xbmsp_handle_tally++;
  xbmsp_send_handle(xbmsp, msgid, xbmsp->xbmsp_handle_tally);
  return 0;
}

/**
 * Handle XBMSP_PACKET_AUTHENTICATE
 */
static int
xbmsp_input_authenticate(xbmsp_t *xbmsp, uint32_t msgid, 
			 uint8_t *buf, int len)
{
  char *username, *password;
  uint32_t handle;

  if(xbmsp_extract_u32(xbmsp, &buf, &len, &handle))
    return EBADMSG;
  if((username = xbmsp_extract_string(xbmsp, &buf, &len)) == NULL)
    return EBADMSG;
  if((password = xbmsp_extract_string(xbmsp, &buf, &len)) == NULL) {
    free(username);
    return EBADMSG;
  }

  snprintf(xbmsp->xbmsp_logname, sizeof(xbmsp->xbmsp_logname),
	   "xbmsp: %s @ %s", username, tcp_logname(&xbmsp->xbmsp_tcp_session));

  if(access_verify(username, password,
		   (struct sockaddr *)&xbmsp->xbmsp_tcp_session.tcp_peer_addr,
		   ACCESS_STREAMING) != 0) {
    xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_AUTHENTICATION_FAILED,
		   "Access denied");
    return 0;
  }
  xbmsp->xbmsp_authenticated = 1;
  /* Auth ok */
  xbmsp_send_msg(xbmsp, XBMSP_PACKET_OK, msgid, NULL, 0);

  free(username);
  free(password);
  return 0;
}

/**
 * Handle XBMSP_PACKET_FILELIST_OPEN
 */
static int
xbmsp_input_filelist_open(xbmsp_t *xbmsp, uint32_t msgid, 
			  uint8_t *buf, int len)
{
  xbmsp_dirhandle_t *xdh;

  xbmsp->xbmsp_handle_tally++;
  xdh = calloc(1, sizeof(xbmsp_dirhandle_t));
  TAILQ_INIT(&xdh->xdh_entries);
  xdh->xdh_handle = xbmsp->xbmsp_handle_tally;
  LIST_INSERT_HEAD(&xbmsp->xbmsp_dirhandles, xdh, xdh_link);

  if(xbmsp_dir_populate(xbmsp, xdh, NULL)) {
    xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_NO_SUCH_FILE, 
		   "CWD \"%s\" invalid", xbmsp->xbmsp_wd);
  } else {
    xbmsp_send_handle(xbmsp, msgid, xdh->xdh_handle);
  }
  return 0;
}

/**
 * Send a XBMSP_PACKET_FILE_DATA reply
 */
static int
xbmsp_reply_file_data(xbmsp_t *xbmsp, uint32_t msgid, xbmsp_dirhandle_t *xdh,
		      const char *single_file)
{
  xbmsp_direntry_t *xde;
  int len1, len2;
  uint8_t *out;

  xde = TAILQ_FIRST(&xdh->xdh_entries);
  if(xde == NULL) {
    if(single_file != NULL) {
      xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_NO_SUCH_FILE, 
		     "File \"%s\" not found", single_file);
    } else {
      xbmsp_send_msg(xbmsp, XBMSP_PACKET_FILE_DATA, msgid, NULL, 8);
    }
    return 1;
  }

  len1 = strlen(xde->xde_filename);
  len2 = strlen(xde->xde_xmlmeta);
  
  out = alloca(8 + len1 + len2);

  out[0] = len1 >> 24;
  out[1] = len1 >> 16;
  out[2] = len1 >> 8;
  out[3] = len1;

  memcpy(out + 4, xde->xde_filename, len1);

  out[len1 + 4 + 0] = len2 >> 24;
  out[len1 + 4 + 1] = len2 >> 16;
  out[len1 + 4 + 2] = len2 >> 8;
  out[len1 + 4 + 3] = len2;
  
  memcpy(out + 8 + len1, xde->xde_xmlmeta, len2);

  xbmsp_send_msg(xbmsp, XBMSP_PACKET_FILE_DATA, msgid, out, 8 + len1 + len2);

  TAILQ_REMOVE(&xdh->xdh_entries, xde, xde_link);
  free((void *)xde->xde_filename);
  free((void *)xde->xde_xmlmeta);
  free(xde);
  return 0;
}



/**
 * Handle XBMSP_PACKET_FILELIST_READ
 */
static int
xbmsp_input_filelist_read(xbmsp_t *xbmsp, uint32_t msgid, 
			  uint8_t *buf, int len)
{
  xbmsp_dirhandle_t *xdh;
  uint32_t handle;

  if(xbmsp_extract_u32(xbmsp, &buf, &len, &handle))
    return EBADMSG;

  LIST_FOREACH(xdh, &xbmsp->xbmsp_dirhandles, xdh_link)
    if(xdh->xdh_handle == handle)
      break;

  if(xdh == NULL) {
    xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_INVALID_HANDLE,
		   "Invalid file handle (0x%x)", handle);
    return 0;
  }

  if(xbmsp_reply_file_data(xbmsp, msgid, xdh, NULL)) {
    LIST_REMOVE(xdh, xdh_link);
    free(xdh);
  }
  return 0;
}

/**
 * Handle XBMSP_PACKET_SETCWD
 */
static int
xbmsp_input_setcwd(xbmsp_t *xbmsp, uint32_t msgid, uint8_t *buf, int len)
{
  char *newdir;
  const char *errtxt;

  if((newdir = xbmsp_extract_string(xbmsp, &buf, &len)) == NULL)
    return EBADMSG;

  if(newdir[0] == 0 || !strcmp(newdir, ".")) {
    /* change to current dir */
    errtxt = NULL;
  } else if(!strcmp(newdir, "..")) {
    /* change to parent dir */
    errtxt = xbmsp_cdup(xbmsp);
  } else if(!strcmp(newdir, "/")) {
    /* change to root */
    errtxt = xbmsp_cdroot(xbmsp);
  } else {
    errtxt = xbmsp_cddown(xbmsp, newdir);
  }

  if(errtxt == NULL) {
    xbmsp_send_msg(xbmsp, XBMSP_PACKET_OK, msgid, NULL, 0);
  } else {
    xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_NO_SUCH_FILE, errtxt, newdir);
  }

  free(newdir);
  return 0;
}

/**
 * Handle XBMSP_PACKET_UPCWD
 */
static int
xbmsp_input_upcwd(xbmsp_t *xbmsp, uint32_t msgid, uint8_t *buf, int len)
{
  const char *errtxt;
  uint32_t levels;

  if(xbmsp_extract_u32(xbmsp, &buf, &len, &levels))
    return EBADMSG;

  if(levels == 0xffffffff) {
    errtxt = xbmsp_cdroot(xbmsp);
  } else {
    errtxt = NULL;
    while(levels > 0 && errtxt == NULL) {
      levels--;
      errtxt = xbmsp_cdup(xbmsp);
    }
  }

  if(errtxt == NULL) {
    xbmsp_send_msg(xbmsp, XBMSP_PACKET_OK, msgid, NULL, 0);
  } else {
    xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_NO_SUCH_FILE, errtxt);
  }
  return 0;
}


/**
 * Handle XBMSP_PACKET_FILE_INFO
 */
static int
xbmsp_input_file_info(xbmsp_t *xbmsp, uint32_t msgid, uint8_t *buf, int len)
{
  char *fname, *tr;
  xbmsp_dirhandle_t xdh;

  if((fname = xbmsp_extract_string(xbmsp, &buf, &len)) == NULL)
    return EBADMSG;

  tr = strstr(fname, "." XBMSP_FILEFORMAT);
  if(tr != NULL)
    *tr = 0;

  TAILQ_INIT(&xdh.xdh_entries);
  xbmsp_dir_populate(xbmsp, &xdh, fname);
  free(fname);

  xbmsp_reply_file_data(xbmsp, msgid, &xdh, fname);
  return 0;
}



/**
 * Handle XBMSP_PACKET_FILE_OPEN
 */
static int
xbmsp_input_file_open(xbmsp_t *xbmsp, uint32_t msgid, uint8_t *buf, int len)
{
  char *fname = NULL, *tr;
  th_channel_group_t *tcg;
  th_channel_t *ch;
  xbmsp_subscrption_t *xs;

  if((fname = xbmsp_extract_string(xbmsp, &buf, &len)) == NULL) {
    return EBADMSG;
  }

  tr = strstr(fname, "." XBMSP_FILEFORMAT);
  if(tr != NULL)
    *tr = 0;

  if((tcg = xbmsp_cur_channel_group(xbmsp)) == NULL) {
    xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_NO_SUCH_FILE, 
		   "Invalid directory \"%s\"", fname);
    free(fname);
    return 0;
  }


  TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
    if(LIST_FIRST(&ch->ch_transports) == NULL)
      continue;
    if(!strcmp(ch->ch_name, fname))
      break;
  }

  if(ch == NULL) {
    xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_NO_SUCH_FILE, 
		   "File \"%s\" not found", fname);
    free(fname);
    return 0;
  }
  free(fname);

  xs = calloc(1, sizeof(xbmsp_subscrption_t));

  xbmsp->xbmsp_handle_tally++;
  xs->xs_handle = xbmsp->xbmsp_handle_tally;

  xs->xs_xbmsp = xbmsp;
  xs->xs_subscription = subscription_create(ch, 100, xbmsp->xbmsp_logname,
					    xbmsp_subscription_callback, xs);

  LIST_INSERT_HEAD(&xbmsp->xbmsp_subscriptions, xs, xs_link);
  xbmsp_send_handle(xbmsp, msgid, xs->xs_handle);
  return 0;
}


/**
 * Handle XBMSP_PACKET_CLOSE
 */
static int
xbmsp_input_close(xbmsp_t *xbmsp, uint32_t msgid, uint8_t *buf, int len)
{
  uint32_t handle;

  if(xbmsp_extract_u32(xbmsp, &buf, &len, &handle))
    return EBADMSG;
  
  if(xbmsp_close_dirhandle(xbmsp, handle)) {
    if(xbmsp_close_subscription(xbmsp, handle)) {
      xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_INVALID_HANDLE,
		     "Invalid handle (0x%x)", handle);
      return 0;
    }
  }
  xbmsp_send_msg(xbmsp, XBMSP_PACKET_OK, msgid, NULL, 0);
  return 0;
}

/**
 * Handle XBMSP_PACKET_CLOSE_ALL
 */
static int
xbmsp_input_close_all(xbmsp_t *xbmsp, uint32_t msgid, uint8_t *buf, int len)
{
  xbmsp_close_dirhandle(xbmsp, 0);
  xbmsp_close_subscription(xbmsp, 0);

  xbmsp_send_msg(xbmsp, XBMSP_PACKET_OK, msgid, NULL, 0);
  return 0;
}


/**
 * Handle XBMSP_PACKET_FILE_READ
 */
static int
xbmsp_input_file_read(xbmsp_t *xbmsp, uint32_t msgid, uint8_t *buf, int len)
{
  uint32_t handle, wantlen;
  xbmsp_subscrption_t *xs;

  if(xbmsp_extract_u32(xbmsp, &buf, &len, &handle))
    return EBADMSG;

  if(xbmsp_extract_u32(xbmsp, &buf, &len, &wantlen))
    return EBADMSG;

  LIST_FOREACH(xs, &xbmsp->xbmsp_subscriptions, xs_link)
    if(xs->xs_handle == handle)
      break;

  if(xs == NULL) {
    xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_INVALID_HANDLE,
		   "Invalid handle (0x%x)", handle);
    return 0;
  }

  if(xs->xs_pending_read_size != 0) {
    xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_UNSUPPORTED,
		   "Read already pending");
    return 0;
  }

  xs->xs_pending_read_size = wantlen;
  xs->xs_pending_read_msgid = msgid;
  
  xbmsp_output_file(xs);
  return 0;
}


/**
 * Function for parsing XBMSP 1.0 messages
 */
static void
xbmsp_input(xbmsp_t *xbmsp, uint8_t *buf, int len)
{
  uint8_t msgtype;
  uint32_t msgid;
  int r;

  if(len < 5) {
    tcp_disconnect(&xbmsp->xbmsp_tcp_session, EBADMSG);
    return;
  }

  msgtype = buf[0];
  msgid = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];

  /* Shift to payload */
  buf += 5;
  len -= 5;

  if(msgtype != XBMSP_PACKET_AUTHENTICATION_INIT && 
     msgtype != XBMSP_PACKET_AUTHENTICATE && 
     xbmsp->xbmsp_authenticated == 0) {
    xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_AUTHENTICATION_NEEDED,
		   "Authentication needed");
    return;
  }

  switch(msgtype) {
  case XBMSP_PACKET_NULL:
    xbmsp_send_msg(xbmsp, XBMSP_PACKET_OK, msgid, NULL, 0);
    r = 0;
    break;

  case XBMSP_PACKET_SETCWD:
    r = xbmsp_input_setcwd(xbmsp, msgid, buf, len);
    break;
    
  case XBMSP_PACKET_UPCWD:
    r = xbmsp_input_upcwd(xbmsp, msgid, buf, len);
    break;

  case XBMSP_PACKET_FILELIST_OPEN:
    r = xbmsp_input_filelist_open(xbmsp, msgid, buf, len);
    break;

  case XBMSP_PACKET_FILELIST_READ:
    r = xbmsp_input_filelist_read(xbmsp, msgid, buf, len);
    break;

  case XBMSP_PACKET_FILE_INFO:
    r = xbmsp_input_file_info(xbmsp, msgid, buf, len);
    break;

  case XBMSP_PACKET_FILE_OPEN:
    r = xbmsp_input_file_open(xbmsp, msgid, buf, len);
    break;

  case XBMSP_PACKET_FILE_READ:
    r = xbmsp_input_file_read(xbmsp, msgid, buf, len);
    break;

  case XBMSP_PACKET_CLOSE:
    r = xbmsp_input_close(xbmsp, msgid, buf, len);
    break;

  case XBMSP_PACKET_CLOSE_ALL:
    r = xbmsp_input_close_all(xbmsp, msgid, buf, len);
    break;

  case XBMSP_PACKET_AUTHENTICATION_INIT:
    r = xbmsp_input_authentication_init(xbmsp, msgid, buf, len);
    break;

  case XBMSP_PACKET_AUTHENTICATE:
    r = xbmsp_input_authenticate(xbmsp, msgid, buf, len);
    break;

  default:
    xbmsp_send_err(xbmsp, msgid, XBMSP_ERROR_UNSUPPORTED,
		   "Unsupported command (%d)", msgtype);
    r = 0;
    break;
  }

  if(r)
    tcp_disconnect(&xbmsp->xbmsp_tcp_session, r);
}

 

/*
 * 
 */
static void
xbmsp_data_input(xbmsp_t *xbmsp)
{
  tcp_session_t *tcp = &xbmsp->xbmsp_tcp_session;
  int r, l;

  switch(xbmsp->xbmsp_state) {
  case XBMSP_STATE_CLIENT_IDENTIFY:
    if(xbmsp->xbmsp_bufptr > 500) {
      tcp_disconnect(tcp, EBADMSG);
      return;
    }
    r = read(tcp->tcp_fd, xbmsp->xbmsp_buf + xbmsp->xbmsp_bufptr, 1);
    if(r < 1) {
      tcp_disconnect(tcp, r == 0 ? ECONNRESET : errno);
      return;
    }

    if(xbmsp->xbmsp_buf[xbmsp->xbmsp_bufptr] == 0xa) {
      xbmsp->xbmsp_buf[xbmsp->xbmsp_bufptr] = 0;
      xbmsp->xbmsp_state = XBMSP_STATE_1_0;
      xbmsp->xbmsp_bufptr = 0;
      return;
    }
    xbmsp->xbmsp_bufptr++;
    break;

  case XBMSP_STATE_1_0:

    if(xbmsp->xbmsp_bufptr < 4) {
      r = read(tcp->tcp_fd, xbmsp->xbmsp_buf + xbmsp->xbmsp_bufptr,
	       4 - xbmsp->xbmsp_bufptr);
      if(r < 1) {
	tcp_disconnect(tcp, r == 0 ? ECONNRESET : errno);
	return;
      }

      xbmsp->xbmsp_bufptr += r;
      if(xbmsp->xbmsp_bufptr < 4)
	return;

      xbmsp->xbmsp_msglen = (xbmsp->xbmsp_buf[0] << 24) +
	(xbmsp->xbmsp_buf[1] << 16) + (xbmsp->xbmsp_buf[2] << 8) +
	xbmsp->xbmsp_buf[3] + 4;

      if(xbmsp->xbmsp_msglen < 9 || xbmsp->xbmsp_msglen > 16 * 1024 * 1024) {
	tcp_disconnect(tcp, EBADMSG);
	return;
      }

      if(xbmsp->xbmsp_bufsize < xbmsp->xbmsp_msglen) {
	xbmsp->xbmsp_bufsize = xbmsp->xbmsp_msglen;
	free(xbmsp->xbmsp_buf);
	xbmsp->xbmsp_buf = malloc(xbmsp->xbmsp_bufsize);
      }
    }

    l = xbmsp->xbmsp_msglen - xbmsp->xbmsp_bufptr;
  
    r = read(tcp->tcp_fd, xbmsp->xbmsp_buf + xbmsp->xbmsp_bufptr, l);
    if(r < 1) {
      tcp_disconnect(tcp, r == 0 ? ECONNRESET : errno);
      return;
    }
  
    xbmsp->xbmsp_bufptr += r;
  
    if(xbmsp->xbmsp_bufptr == xbmsp->xbmsp_msglen) {
      xbmsp_input(xbmsp, xbmsp->xbmsp_buf + 4, xbmsp->xbmsp_msglen - 4);
      xbmsp->xbmsp_bufptr = 0;
      xbmsp->xbmsp_msglen = 0;
    }
    break;
  }
}



/*
 * 
 */
static void
xbmsp_disconnect(xbmsp_t *xbmsp)
{
  xbmsp_close_dirhandle(xbmsp, 0);
  xbmsp_close_subscription(xbmsp, 0);
  free(xbmsp->xbmsp_wd);
  free(xbmsp->xbmsp_buf);
  LIST_REMOVE(xbmsp, xbmsp_global_link);
}


/*
 * 
 */
static void
xbmsp_connect(xbmsp_t *xbmsp)
{
  LIST_INSERT_HEAD(&xbmsp_sessions, xbmsp, xbmsp_global_link);

  xbmsp->xbmsp_wd = strdup(""); /* start in root */

  xbmsp->xbmsp_bufsize = 1000;
  xbmsp->xbmsp_buf = malloc(xbmsp->xbmsp_bufsize);

  snprintf(xbmsp->xbmsp_logname, sizeof(xbmsp->xbmsp_logname),
	   "xbmsp: <noauth> @ %s", tcp_logname(&xbmsp->xbmsp_tcp_session));

  tcp_printf(&xbmsp->xbmsp_tcp_session,
	     "XBMSP-1.0 1.0 HTS/Tvheadend\n");
}

/*
 *
 */
static void
xbmsp_tcp_callback(tcpevent_t event, void *tcpsession)
{
  xbmsp_t *xbmsp = tcpsession;

  switch(event) {
  case TCP_CONNECT:
    xbmsp_connect(xbmsp);
    break;

  case TCP_DISCONNECT:
    xbmsp_disconnect(xbmsp);
    break;

  case TCP_INPUT:
    xbmsp_data_input(xbmsp);
    break;
  }
}


/**
 *  Fire up XBMSP server
 */
void
xbmsp_start(int port)
{
  tcp_create_server(port, sizeof(xbmsp_t), "xbmsp", xbmsp_tcp_callback);
}

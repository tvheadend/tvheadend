/*
 *  IPTV - libav handler
 *
 *  Copyright (C) 2017 Jaroslav Kysela
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

#include "tvheadend.h"
#include "iptv_private.h"
#include "libav.h"

#include <fcntl.h>
#include <signal.h>

#define WRITE_BUFFER_SIZE (188*500)

typedef struct {
  const char *url;
  iptv_mux_t *mux;
  int running;
  int pause;
  pthread_t thread;
  pthread_mutex_t lock;
  th_pipe_t pipe;
  AVFormatContext *ictx;
  AVFormatContext *octx;
  sbuf_t sbuf;
} iptv_libav_priv_t;

/*
 *
 */
static int
iptv_libav_write_packet(void *opaque, uint8_t *buf, int buf_size)
{
  iptv_libav_priv_t *la = opaque;

  if (buf_size > 0) {
    pthread_mutex_lock(&la->lock);
    if (la->sbuf.sb_ptr < 5*1024*1024) {
      while (atomic_get(&la->pause)) {
        if (!atomic_get(&la->running))
          goto fin;
        pthread_mutex_unlock(&la->lock);
        tvh_usleep(500000);
        pthread_mutex_lock(&la->lock);
      }
      sbuf_append(&la->sbuf, buf, buf_size);
      /* notify iptv layer that we have new data to read */
      if (write(la->pipe.wr, "", 1)) {};
    }
fin:
    pthread_mutex_unlock(&la->lock);
  }
  return 0;
}

/*
 *
 */
static int
iptv_libav_interrupt_callback(void *opaque)
{
  iptv_libav_priv_t *la = opaque;

  return atomic_get(&la->running) == 0;
}

/*
 *
 */
static void *
iptv_libav_thread(void *aux)
{
  iptv_libav_priv_t *la = aux;
  AVStream *in_stream, *out_stream;
  AVPacket pkt;
  uint8_t *buf = NULL;
  int ret, i;

  buf = malloc(WRITE_BUFFER_SIZE);
  if (buf == NULL)
    goto fail;

  if ((ret = avformat_open_input(&la->ictx, la->url, 0, 0)) < 0) {
    tvherror(LS_IPTV, "libav: Could not open input '%s': %s", la->url, av_err2str(ret));
    goto fail;
  }
  la->ictx->interrupt_callback.callback = iptv_libav_interrupt_callback;
  la->ictx->interrupt_callback.opaque = la;
  if ((ret = avformat_find_stream_info(la->ictx, 0)) < 0) {
    tvherror(LS_IPTV, "libav: Unable to find stream info for input '%s': %s", la->url, av_err2str(ret));
    goto fail;
  }

  avformat_alloc_output_context2(&la->octx, NULL, "mpegts", NULL);
  if (la->octx == NULL)
    goto fail;
  la->octx->pb = avio_alloc_context(buf, WRITE_BUFFER_SIZE, AVIO_FLAG_WRITE,
                                    la, NULL, iptv_libav_write_packet, NULL);
  la->octx->interrupt_callback.callback = iptv_libav_interrupt_callback;
  la->octx->interrupt_callback.opaque = la;

  for (i = 0; i < la->ictx->nb_streams; i++) {
    in_stream = la->ictx->streams[i];
    out_stream = avformat_new_stream(la->octx, in_stream->codec->codec);
    if (out_stream == NULL) {
      tvherror(LS_IPTV, "libav: Failed allocating output stream");
      goto fail;
    }
    ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
    if (ret < 0) {
      tvherror(LS_IPTV, "libav: Failed to copy context from input to output stream codec context: %s", av_err2str(ret));
      goto fail;
    }
    out_stream->codec->codec_tag = 0;
    if (la->octx->oformat->flags & AVFMT_GLOBALHEADER)
      out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  ret = avformat_write_header(la->octx, NULL);
  if (ret < 0) {
    tvherror(LS_IPTV, "libav: Unable to write header");
    goto fail;
  }

  while (atomic_get(&la->running)) {
    ret = av_read_frame(la->ictx, &pkt);
    if (ret < 0) {
      if (ret != AVERROR_EOF)
        tvherror(LS_IPTV, "libav: unable to read frame: %s", av_err2str(ret));
      break;
    }
    if (atomic_get(&la->running) == 0)
      goto unref;
    if ((pkt.dts != AV_NOPTS_VALUE && pkt.dts < 0) ||
        (pkt.pts != AV_NOPTS_VALUE && pkt.pts < 0))
      goto unref;
    in_stream  = la->ictx->streams[pkt.stream_index];
    out_stream = la->octx->streams[pkt.stream_index];
    /* copy packet */
    pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
    pkt.pos = -1;
    ret = av_interleaved_write_frame(la->octx, &pkt);
    if (ret < 0) {
      tvherror(LS_IPTV, "libav: Error muxing packet: %s", av_err2str(ret));
      break;
    }
unref:
    av_packet_unref(&pkt);
  }
  av_write_trailer(la->octx);

fail:
  free(buf);
  return NULL;
}

/*
 * Start new thread
 */
static int
iptv_libav_start
  ( iptv_input_t *mi, iptv_mux_t *im, const char *raw, const url_t *url )
{
  iptv_libav_priv_t *la = calloc(1, sizeof(*la));

  assert(raw);
  pthread_mutex_init(&la->lock, NULL);
  im->im_opaque = la;
  if (strncmp(raw, "libav:", 6) == 0)
    raw += 6;
  la->url = raw;
  la->mux = im;
  tvh_pipe(O_NONBLOCK, &la->pipe);
  im->mm_iptv_fd = la->pipe.rd;
  iptv_input_fd_started(mi, im);
  atomic_set(&la->running, 1);
  atomic_set(&la->pause, 0);
  sbuf_init(&la->sbuf);
  tvhthread_create(&la->thread, NULL, iptv_libav_thread, la, "libavinput");
  if (raw[0])
    iptv_input_mux_started(mi, im);
  return 0;
}

static void
iptv_libav_stop
  ( iptv_input_t *mi, iptv_mux_t *im )
{
  iptv_libav_priv_t *la = im->im_opaque;

  atomic_set(&la->running, 0);
  im->im_opaque = NULL;
  pthread_kill(la->thread, SIGUSR1);
  pthread_join(la->thread, NULL);
  tvh_pipe_close(&la->pipe);
  avformat_close_input(&la->ictx);
  avformat_free_context(la->octx);
  sbuf_free(&la->sbuf);
  free(la);
}

static ssize_t
iptv_libav_read ( iptv_input_t *mi, iptv_mux_t *im )
{
  iptv_libav_priv_t *la = im->im_opaque;
  char buf[8192];
  ssize_t ret;

  if (la == NULL)
    return 0;
  pthread_mutex_lock(&la->lock);
  ret = la->sbuf.sb_ptr;
  sbuf_append_from_sbuf(&im->mm_iptv_buffer, &la->sbuf);
  sbuf_reset(&la->sbuf, WRITE_BUFFER_SIZE * 2);
  if (read(la->pipe.rd, buf, sizeof(buf))) {};
  pthread_mutex_unlock(&la->lock);
  return ret;
}

static void
iptv_libav_pause ( iptv_input_t *mi, iptv_mux_t *im, int pause )
{
  iptv_libav_priv_t *la = im->im_opaque;

  if (la)
    atomic_set(&la->pause, pause);
}

/*
 * Initialise libav handler
 */
void
iptv_libav_init ( void )
{
  static iptv_handler_t ih[] = {
    {
      .scheme = "libav",
      .buffer_limit = 5000,
      .start  = iptv_libav_start,
      .stop   = iptv_libav_stop,
      .read   = iptv_libav_read,
      .pause  = iptv_libav_pause,
    },
  };
  iptv_handler_register(ih, ARRAY_SIZE(ih));
}

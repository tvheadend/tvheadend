// SPDX-License-Identifier: GPL-3.0-or-later
/*
 *  Tvheadend - recorded media input
 *  Copyright (C) 2026 Tvheadend contributors
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
#include "dvr_playback.h"
#include "lang_codes.h"
#include "libav.h"
#include "parsers/parsers.h"

#include <libavcodec/avcodec.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

struct dvr_playback {
  int fd;
  int (*wait)(void *);
  void *wait_opaque;
  int seeking;
  int seek_eof;
  off_t probed_size;
  off_t scanned;
  int64_t last_stamp;
  int (*interrupt)(void *);
  void *opaque;
  AVFormatContext *format;
  AVIOContext *io;
  AVPacket *packet;
  int pending;
  int primary;
  int64_t origin;
  int64_t duration;
  streaming_start_t *start;
  int *components;
  unsigned nb_streams;
};

#define DVR_PLAYBACK_IO_SIZE 32768
#define DVR_PLAYBACK_TS_SIZE 188

enum { DVR_PLAYBACK_CLOCK = 90000 };
static const AVRational playback_timebase = { 1, DVR_PLAYBACK_CLOCK };

static int
dvr_playback_read_bytes(void *opaque, uint8_t *data, int size)
{
  dvr_playback_t *playback = opaque;
  ssize_t len;

  if (playback->interrupt && playback->interrupt(playback->opaque))
    return AVERROR_EXIT;
  do {
    len = read(playback->fd, data, size);
  } while (len < 0 && errno == EINTR);
  while (len == 0 && playback->wait && !playback->seeking) {
    int result = playback->wait(playback->wait_opaque);
    if (result < 0)
      return AVERROR_EXIT;
    if (!result)
      playback->wait = NULL;
    /* Completion can race with the writer's final append. Read once more. */
    do {
      len = read(playback->fd, data, size);
    } while (len < 0 && errno == EINTR);
  }
  if (len == 0 && playback->seeking)
    playback->seek_eof = 1;
  return len > 0 ? len : len == 0 ? AVERROR_EOF : AVERROR(errno);
}

static int64_t
dvr_playback_seek_bytes(void *opaque, int64_t offset, int whence)
{
  dvr_playback_t *playback = opaque;
  struct stat st;
  off_t pos;

  if (whence == AVSEEK_SIZE)
    return fstat(playback->fd, &st) ? AVERROR(errno) : st.st_size;
  pos = lseek(playback->fd, offset, whence & ~AVSEEK_FORCE);
  return pos < 0 ? AVERROR(errno) : pos;
}

static int64_t
dvr_playback_timestamp(int64_t value, AVRational timebase)
{
  return value == AV_NOPTS_VALUE ? PTS_UNSET :
    av_rescale_q(value, timebase, playback_timebase);
}

static streaming_component_type_t
dvr_playback_type(enum AVCodecID codec)
{
  /* Like dvb_psi_pmt.c, distinguish ADTS AAC from LATM (SCT_AAC). */
  return codec == AV_CODEC_ID_AAC ? SCT_MP4A : codec_id2streaming_component_type(codec);
}

static int
dvr_playback_streams(dvr_playback_t *playback)
{
  AVFormatContext *format = playback->format;
  AVStream *stream;
  AVCodecParameters *codec;
  AVDictionaryEntry *language;
  streaming_start_component_t *component;
  streaming_component_type_t type;
  AVRational aspect, rate;
  unsigned i, nb_streams = format->nb_streams;
  int count = 0, samples, latm = 0, audio = 0, num, den;

  playback->nb_streams = nb_streams;
  playback->components = malloc(nb_streams * sizeof(int));
  if (!playback->components)
    return AVERROR(ENOMEM);
  for (i = 0; i < nb_streams; i++) {
    type = dvr_playback_type(format->streams[i]->codecpar->codec_id);
    if (format->streams[i]->codecpar->codec_id == AV_CODEC_ID_AAC_LATM) {
      tvhwarn(LS_DVR, "Skipping unsupported AAC-LATM recording track");
      latm = 1;
    } else if (SCT_ISAUDIO(type)) {
      audio = 1;
    }
    playback->components[i] = SCT_ISAV(type) || type == SCT_DVBSUB ? count++ : -1;
  }
  if (latm && !audio)
    return AVERROR(ENOSYS);
  /* Packet component indices use eight bits in th_pkt_t. */
  if (count == 0 || count > UINT8_MAX)
    return AVERROR_INVALIDDATA;

  playback->start = calloc(1, sizeof(streaming_start_t) +
                             count * sizeof(streaming_start_component_t));
  if (!playback->start)
    return AVERROR(ENOMEM);
  playback->start->ss_refcount = 1;
  playback->start->ss_num_components = count;
  playback->primary = -1;

  for (i = 0; i < nb_streams; i++) {
    if (playback->components[i] < 0)
      continue;
    stream = format->streams[i];
    codec = stream->codecpar;
    component = &playback->start->ss_components[playback->components[i]];
    component->es_index = playback->components[i];
    component->es_pid = stream->id;
    component->es_type = dvr_playback_type(codec->codec_id);
    language = av_dict_get(stream->metadata, "language", NULL, 0);
    if (language)
      snprintf(component->es_lang, sizeof(component->es_lang), "%s",
               lang_code_get(language->value));
    if (codec->extradata_size > 0) {
      component->ssc_gh = pktbuf_alloc(codec->extradata, codec->extradata_size);
      if (!component->ssc_gh)
        return AVERROR(ENOMEM);
    }
    if (SCT_ISVIDEO(component->es_type)) {
      if (playback->primary < 0 ||
          format->streams[playback->primary]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
        playback->primary = i;
      if (codec->width <= 0 || codec->height <= 0)
        return AVERROR_INVALIDDATA;
      component->es_width = codec->width;
      component->es_height = codec->height;
      rate = av_guess_frame_rate(format, stream, NULL);
      if (rate.num > 0 && rate.den > 0)
        component->es_frame_duration = av_rescale_q(1, av_inv_q(rate), playback_timebase);
      aspect = av_guess_sample_aspect_ratio(format, stream, NULL);
      if (aspect.num <= 0 || aspect.den <= 0)
        aspect = (AVRational){ 1, 1 };
      av_reduce(&num, &den, (int64_t)aspect.num * codec->width,
                           (int64_t)aspect.den * codec->height, UINT16_MAX);
      component->es_aspect_num = num;
      component->es_aspect_den = den;
    } else if (SCT_ISAUDIO(component->es_type)) {
      if (playback->primary < 0)
        playback->primary = i;
#if LIBAVCODEC_VERSION_MAJOR >= 59
      component->es_channels = codec->ch_layout.nb_channels;
#else
      component->es_channels = codec->channels;
#endif
      if (!component->es_channels || codec->sample_rate <= 0)
        return AVERROR_INVALIDDATA;
      component->es_sri = rate_to_sri(codec->sample_rate);
      samples = av_get_audio_frame_duration2(codec, 0);
      if (samples > 0)
        component->es_frame_duration = av_rescale_q(samples,
                                      (AVRational){ 1, codec->sample_rate },
                                      playback_timebase);
    } else if (component->es_type == SCT_DVBSUB && codec->extradata_size >= 4) {
      component->es_composition_id = (codec->extradata[0] << 8) | codec->extradata[1];
      component->es_ancillary_id = (codec->extradata[2] << 8) | codec->extradata[3];
    }
  }
  return playback->primary < 0 ? AVERROR_INVALIDDATA : 0;
}

int
dvr_playback_open(dvr_playback_t **result, const char *filename,
                  int (*interrupt)(void *), void *opaque)
{
  dvr_playback_t *playback;
  streaming_start_component_t *primary;
  struct stat st;
  uint8_t *buffer;
  int error;

  *result = NULL;
  playback = calloc(1, sizeof(*playback));
  if (!playback)
    return AVERROR(ENOMEM);
  playback->interrupt = interrupt;
  playback->opaque = opaque;
  /* Avoid blocking on a non-regular path before fstat rejects it. */
  playback->fd = tvh_open(filename, O_RDONLY | O_NONBLOCK, 0);
  if (playback->fd < 0) {
    error = errno > 0 ? AVERROR(errno) : AVERROR(EIO);
    goto fail;
  }
  if (fstat(playback->fd, &st) || !S_ISREG(st.st_mode)) {
    error = AVERROR(EINVAL);
    goto fail;
  }
  playback->probed_size = st.st_size;
  buffer = av_malloc(DVR_PLAYBACK_IO_SIZE);
  if (!buffer) {
    error = AVERROR(ENOMEM);
    goto fail;
  }
  playback->io = avio_alloc_context(buffer, DVR_PLAYBACK_IO_SIZE, 0, playback,
                                   dvr_playback_read_bytes, NULL,
                                   dvr_playback_seek_bytes);
  if (!playback->io) {
    av_free(buffer);
    error = AVERROR(ENOMEM);
    goto fail;
  }
  playback->format = avformat_alloc_context();
  playback->packet = av_packet_alloc();
  if (!playback->format || !playback->packet) {
    error = AVERROR(ENOMEM);
    goto fail;
  }
  playback->format->interrupt_callback.callback = interrupt;
  playback->format->interrupt_callback.opaque = opaque;
  playback->format->pb = playback->io;
  playback->format->flags |= AVFMT_FLAG_CUSTOM_IO;
  error = avformat_open_input(&playback->format, NULL, av_find_input_format("mpegts"), NULL);
  if (error < 0)
    goto fail;
  error = avformat_find_stream_info(playback->format, NULL);
  if (error < 0)
    goto fail;
  if (playback->format->start_time == AV_NOPTS_VALUE ||
      playback->format->duration == AV_NOPTS_VALUE) {
    error = AVERROR_INVALIDDATA;
    goto fail;
  }
  playback->origin = av_rescale_q(playback->format->start_time,
                                 AV_TIME_BASE_Q, playback_timebase);
  playback->duration = av_rescale_q(playback->format->duration,
                                   AV_TIME_BASE_Q, playback_timebase);
  error = dvr_playback_streams(playback);
  if (error < 0)
    goto fail;
  /* The duration refresh continues from the end measured here. */
  primary = &playback->start->ss_components[playback->components[playback->primary]];
  playback->last_stamp = playback->origin + playback->duration - primary->es_frame_duration;
  playback->scanned = st.st_size - st.st_size % DVR_PLAYBACK_TS_SIZE;
  *result = playback;
  return 0;

fail:
  dvr_playback_close(playback);
  return error;
}

void
dvr_playback_follow(dvr_playback_t *playback, int (*wait)(void *), void *opaque)
{
  playback->wait = wait;
  playback->wait_opaque = opaque;
}

/* PTS of a PES packet of this PID that starts in the TS packet, or PTS_UNSET. */
static int64_t
dvr_playback_ts_pts(const uint8_t *ts, int pid)
{
  const uint8_t *pes;
  int offset = 4;

  /* Sync byte, no transport error, PES start, our PID, not scrambled, payload. */
  if (ts[0] != 0x47 || (ts[1] & 0x80) || !(ts[1] & 0x40) ||
      (((ts[1] & 0x1f) << 8) | ts[2]) != pid || (ts[3] & 0xc0) || !(ts[3] & 0x10))
    return PTS_UNSET;
  if (ts[3] & 0x20)
    offset += 1 + ts[4];
  /* 9 bytes of PES header and 5 bytes of PTS must be in this packet. */
  if (offset + 14 > DVR_PLAYBACK_TS_SIZE)
    return PTS_UNSET;
  pes = ts + offset;
  if (pes[0] || pes[1] || pes[2] != 1 || (pes[6] & 0xc0) != 0x80 ||
      !(pes[7] & 0x80) || pes[8] < 5)
    return PTS_UNSET;
  return getpts(pes + 9);
}

/* Scan only the TS packets appended since the last call. pread() keeps the
 * file position of the reader. libav is not used here: its demuxer would
 * report the PES packet cut by the file end as corrupt on every refresh. */
int
dvr_playback_refresh(dvr_playback_t *playback, const char *filename)
{
  streaming_start_component_t *primary;
  struct stat current, path;
  uint8_t buffer[DVR_PLAYBACK_TS_SIZE * 64];
  ssize_t len;
  int64_t pts, wrap;
  int i;

  if (fstat(playback->fd, &current) || stat(filename, &path))
    return AVERROR(errno);
  if (current.st_dev != path.st_dev || current.st_ino != path.st_ino ||
      current.st_size < playback->probed_size)
    return AVERROR_INVALIDDATA;
  if (current.st_size == playback->probed_size)
    return 0;
  primary = &playback->start->ss_components[playback->components[playback->primary]];
  /* Like libav, map 33-bit PTS values to the 2^33 ticks that begin 60 s
   * before the start, so the duration uses the timeline of the reader. */
  wrap = playback->origin - 60 * DVR_PLAYBACK_CLOCK;
  while (playback->scanned + DVR_PLAYBACK_TS_SIZE <= current.st_size) {
    len = pread(playback->fd, buffer,
                MIN((off_t)sizeof(buffer), current.st_size - playback->scanned),
                playback->scanned);
    if (len < 0 && errno == EINTR)
      continue;
    if (len < 0)
      return AVERROR(errno);
    for (i = 0; i + DVR_PLAYBACK_TS_SIZE <= len; i += DVR_PLAYBACK_TS_SIZE) {
      /* Find the sync byte again after a damaged packet. */
      while (i + DVR_PLAYBACK_TS_SIZE <= len && buffer[i] != 0x47)
        i++;
      if (i + DVR_PLAYBACK_TS_SIZE > len)
        break;
      pts = dvr_playback_ts_pts(buffer + i, primary->es_pid);
      if (pts == PTS_UNSET)
        continue;
      /* Keep the highest value; older ones, for example from B-frames,
       * are ignored. */
      playback->last_stamp = MAX(playback->last_stamp,
                                 wrap + ((pts - wrap) & PTS_MASK));
    }
    if (i == 0)
      break;
    /* A packet cut by the file end is read again next time. */
    playback->scanned += i;
  }
  playback->duration = MAX(playback->duration,
                           playback->last_stamp - playback->origin + primary->es_frame_duration);
  playback->probed_size = current.st_size;
  return 0;
}

void
dvr_playback_close(dvr_playback_t *playback)
{
  if (!playback)
    return;
  av_packet_free(&playback->packet);
  avformat_close_input(&playback->format);
  if (playback->io) {
    av_freep(&playback->io->buffer);
    avio_context_free(&playback->io);
  }
  if (playback->fd >= 0)
    close(playback->fd);
  if (playback->start)
    streaming_start_unref(playback->start);
  free(playback->components);
  free(playback);
}

streaming_start_t *
dvr_playback_start(dvr_playback_t *playback)
{
  return streaming_start_copy(playback->start);
}

int64_t
dvr_playback_duration(dvr_playback_t *playback)
{
  return playback->duration;
}

int64_t
dvr_playback_origin(dvr_playback_t *playback)
{
  return playback->origin;
}

int
dvr_playback_primary(dvr_playback_t *playback, const th_pkt_t *packet)
{
  return packet->pkt_componentindex == playback->components[playback->primary];
}

int
dvr_playback_read(dvr_playback_t *playback, th_pkt_t **result)
{
  AVPacket *packet = playback->packet;
  AVStream *stream;
  AVCodecParserContext *parser;
  streaming_start_component_t *component;
  th_pkt_t *pkt;
  int error, index, samples;

  *result = NULL;
  for (;;) {
    if (playback->pending) {
      playback->pending = 0;
    } else {
      error = av_read_frame(playback->format, packet);
      if (error < 0)
        return error == AVERROR_EOF ? 0 : error;
    }
    /* A new PMT can add streams after probing. Reconfiguration requires a
     * new START; do not index the original component map with a new stream. */
    if ((unsigned)packet->stream_index >= playback->nb_streams) {
      av_packet_unref(packet);
      return AVERROR_INVALIDDATA;
    }
    index = playback->components[packet->stream_index];
    if (index >= 0)
      break;
    av_packet_unref(packet);
  }
  stream = playback->format->streams[packet->stream_index];
  component = &playback->start->ss_components[index];
  /* Added streams require a new session. Same-stream format changes are not
   * reliably exposed in codecpar after probing and remain unsupported. */
  if (dvr_playback_type(stream->codecpar->codec_id) != component->es_type) {
    av_packet_unref(packet);
    return AVERROR_INVALIDDATA;
  }
  pkt = pkt_alloc(component->es_type, packet->data, packet->size,
                  dvr_playback_timestamp(packet->pts, stream->time_base),
                  dvr_playback_timestamp(packet->dts, stream->time_base),
                  dvr_playback_timestamp(packet->dts != AV_NOPTS_VALUE ?
                                         packet->dts : packet->pts, stream->time_base));
  if (!pkt) {
    av_packet_unref(packet);
    return AVERROR(ENOMEM);
  }
  pkt->pkt_componentindex = index;
  pkt->pkt_duration = packet->duration > 0 ?
    av_rescale_q(packet->duration, stream->time_base, playback_timebase) :
    component->es_frame_duration;
  if (!pkt->pkt_duration && SCT_ISAUDIO(component->es_type) &&
      stream->codecpar->sample_rate > 0) {
    samples = av_get_audio_frame_duration2(stream->codecpar, packet->size);
    if (samples > 0)
      pkt->pkt_duration = av_rescale_q(samples,
        (AVRational){ 1, stream->codecpar->sample_rate }, playback_timebase);
  }
  pkt->pkt_err = !!(packet->flags & AV_PKT_FLAG_CORRUPT);
  if (component->ssc_gh)
    pkt->pkt_meta = pktbuf_ref_inc(component->ssc_gh);
  if (SCT_ISVIDEO(component->es_type)) {
    parser = av_stream_get_parser(stream);
    pkt->v.pkt_frametype = packet->flags & AV_PKT_FLAG_KEY ? PKT_I_FRAME :
      parser && parser->pict_type == AV_PICTURE_TYPE_B ? PKT_B_FRAME : PKT_P_FRAME;
    pkt->v.pkt_aspect_num = component->es_aspect_num;
    pkt->v.pkt_aspect_den = component->es_aspect_den;
  } else if (SCT_ISAUDIO(component->es_type)) {
    pkt->a.pkt_keyframe = 1;
    pkt->a.pkt_channels = component->es_channels;
    pkt->a.pkt_sri = component->es_sri;
  }
  av_packet_unref(packet);
  *result = pkt;
  return 1;
}

static int
dvr_playback_seek0(dvr_playback_t *playback, int64_t time, int64_t *landed)
{
  AVPacket *packet = playback->packet;
  AVStream *stream = playback->format->streams[playback->primary];
  streaming_start_component_t *primary =
    &playback->start->ss_components[playback->components[playback->primary]];
  int64_t target, before, position, lookback = 0;
  int error;

  time = MAX(0, MIN(time, playback->duration -
         MAX(primary->es_frame_duration, 1)));
  av_packet_unref(packet);
  playback->pending = 0;
  for (;;) {
    playback->io->eof_reached = 0;
    playback->io->error = 0;
    before = MAX(0, time - lookback);
    if (before == 0) {
      error = avio_seek(playback->io, 0, SEEK_SET) < 0 ? AVERROR(EIO) : 0;
      avformat_flush(playback->format);
    } else {
      target = av_rescale_q(playback->origin + before, playback_timebase, stream->time_base);
      error = av_seek_frame(playback->format, playback->primary, target, AVSEEK_FLAG_BACKWARD);
    }
    if (error < 0)
      return error;
    playback->seek_eof = 0;
    while ((error = av_read_frame(playback->format, packet)) >= 0) {
      if (packet->stream_index == playback->primary &&
          (packet->flags & AV_PKT_FLAG_KEY) &&
          !(packet->flags & AV_PKT_FLAG_CORRUPT) && packet->pts != AV_NOPTS_VALUE)
        break;
      av_packet_unref(packet);
    }
    if (error < 0 && (error != AVERROR_EOF || before == 0))
      return error;
    if (error >= 0) {
      position = dvr_playback_timestamp(packet->pts, stream->time_base) - playback->origin;
      if ((!playback->wait || !playback->seek_eof) && (position <= time || before == 0))
        break;
      if (before == 0 && playback->seek_eof) {
        av_packet_unref(packet);
        return AVERROR_INVALIDDATA;
      }
      av_packet_unref(packet);
    }
    /* MPEG-TS seeks can land after the preceding video keyframe. */
    lookback = lookback ? MIN(time, lookback * 2) : MIN(time, DVR_PLAYBACK_CLOCK);
  }
  *landed = position;
  playback->pending = 1;
  return 0;
}

int
dvr_playback_seek(dvr_playback_t *playback, int64_t time, int64_t *landed)
{
  int result;
  playback->seeking = 1;
  playback->io->eof_reached = 0;
  playback->io->error = 0;
  result = dvr_playback_seek0(playback, time, landed);
  playback->seeking = 0;
  playback->io->eof_reached = 0;
  playback->io->error = 0;
  return result;
}

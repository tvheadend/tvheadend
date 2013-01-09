/*
 *  TV headend - Timeshift
 *  Copyright (C) 2012 Adam Sutton
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

#ifndef __TVH_TIMESHIFT_PRIVATE_H__
#define __TVH_TIMESHIFT_PRIVATE_H__

#define TS_PLAY_BUF 100000 // us to buffer in TX

/**
 * Indexes of import data in the stream
 */
typedef struct timeshift_index_iframe
{
  off_t                               pos;    ///< Position in the file
  int64_t                             time;   ///< Packet time
  TAILQ_ENTRY(timeshift_index_iframe) link;   ///< List entry
} timeshift_index_iframe_t;

typedef TAILQ_HEAD(timeshift_index_iframe_list,timeshift_index_iframe) timeshift_index_iframe_list_t;

/**
 * Indexes of import data in the stream
 */
typedef struct timeshift_index_data
{
  off_t                             pos;    ///< Position in the file
  streaming_message_t              *data;   ///< Associated data
  TAILQ_ENTRY(timeshift_index_data) link;   ///< List entry
} timeshift_index_data_t;

typedef TAILQ_HEAD(timeshift_index_data_list,timeshift_index_data) timeshift_index_data_list_t;

/**
 * Timeshift file
 */
typedef struct timeshift_file
{
  int                           fd;       ///< Write descriptor
  char                          *path;    ///< Full path to file

  time_t                        time;     ///< Files coarse timestamp
  size_t                        size;     ///< Current file size;
  int64_t                       last;     ///< Latest timestamp

  uint8_t                       bad;      ///< File is broken

  int                           refcount; ///< Reader ref count

  timeshift_index_iframe_list_t iframes;  ///< I-frame indexing
  timeshift_index_data_list_t   sstart;   ///< Stream start messages

  TAILQ_ENTRY(timeshift_file) link;     ///< List entry
} timeshift_file_t;

typedef TAILQ_HEAD(timeshift_file_list,timeshift_file) timeshift_file_list_t;

/**
 *
 */
typedef struct timeshift {
  // Note: input MUST BE FIRST in struct
  streaming_target_t          input;      ///< Input source
  streaming_target_t          *output;    ///< Output dest

  int                         id;         ///< Reference number
  char                        *path;      ///< Directory containing buffer
  time_t                      max_time;   ///< Maximum period to shift
  int                         ondemand;   ///< Whether this is an on-demand timeshift

  enum {
    TS_INIT,
    TS_EXIT,
    TS_LIVE,
    TS_PAUSE,
    TS_PLAY,
  }                           state;       ///< Play state
  pthread_mutex_t             state_mutex; ///< Protect state changes
  uint8_t                     full;        ///< Buffer is full

  streaming_queue_t           wr_queue;   ///< Writer queue
  pthread_t                   wr_thread;  ///< Writer thread

  pthread_t                   rd_thread;  ///< Reader thread
  th_pipe_t                   rd_pipe;    ///< Message passing to reader

  pthread_mutex_t             rdwr_mutex; ///< Buffer protection
  timeshift_file_list_t       files;      ///< List of files

  int                         vididx;     ///< Index of (current) video stream

} timeshift_t;

/*
 * Write functions
 */
ssize_t timeshift_write_start   ( int fd, int64_t time, streaming_start_t *ss );
ssize_t timeshift_write_sigstat ( int fd, int64_t time, signal_status_t *ss );
ssize_t timeshift_write_packet  ( int fd, int64_t time, th_pkt_t *pkt );
ssize_t timeshift_write_mpegts  ( int fd, int64_t time, void *data );
ssize_t timeshift_write_skip    ( int fd, streaming_skip_t *skip );
ssize_t timeshift_write_speed   ( int fd, int speed );
ssize_t timeshift_write_stop    ( int fd, int code );
ssize_t timeshift_write_exit    ( int fd );
ssize_t timeshift_write_eof     ( int fd );

void timeshift_writer_flush ( timeshift_t *ts );

/*
 * Threads
 */
void *timeshift_reader ( void *p );
void *timeshift_writer ( void *p );

/*
 * File management
 */
void timeshift_filemgr_init     ( void );
void timeshift_filemgr_term     ( void );
int  timeshift_filemgr_makedirs ( int ts_index, char *buf, size_t len );

timeshift_file_t *timeshift_filemgr_last
  ( timeshift_t *ts );
timeshift_file_t *timeshift_filemgr_get
  ( timeshift_t *ts, int create );
timeshift_file_t *timeshift_filemgr_prev
  ( timeshift_file_t *ts, int *end, int keep );
timeshift_file_t *timeshift_filemgr_next
  ( timeshift_file_t *ts, int *end, int keep );
void timeshift_filemgr_remove
  ( timeshift_t *ts, timeshift_file_t *tsf, int force );
void timeshift_filemgr_flush ( timeshift_t *ts, timeshift_file_t *end );
void timeshift_filemgr_close ( timeshift_file_t *tsf );

#endif /* __TVH_TIMESHIFT_PRIVATE_H__ */

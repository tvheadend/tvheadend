[← Back to Table of Contents](00-TOC.md)

## 9. Muxer System and File Formats

The Muxer System is responsible for packaging streaming data into container formats suitable for storage or transmission. It provides a flexible framework for converting the internal streaming representation into various file formats (Matroska, MPEG-TS, MP4, etc.) while handling metadata embedding, multi-track management, and efficient file I/O. This section documents the muxer architecture, supported container formats, metadata handling, and file writing strategies.

### 9.1 Muxer Architecture

The muxer system uses an object-oriented design that allows different muxer implementations to handle specific container formats while sharing common configuration and management infrastructure.

#### 9.1.1 The muxer_t Structure

**Location**: `src/muxer.h`, `src/muxer.c`

The `muxer_t` structure is the base class for all muxer implementations. It provides virtual methods for format-specific behavior and manages the lifecycle of file writing operations.

**Structure Definition**:
```c
typedef struct muxer {
  // Virtual methods (function pointers)
  int         (*m_open_stream)(struct muxer *, int fd);
  int         (*m_open_file)  (struct muxer *, const char *filename);
  const char* (*m_mime)       (struct muxer *, const struct streaming_start *);
  int         (*m_init)       (struct muxer *, struct streaming_start *, const char *);
  int         (*m_reconfigure)(struct muxer *, const struct streaming_start *);
  int         (*m_close)      (struct muxer *);
  void        (*m_destroy)    (struct muxer *);
  int         (*m_write_meta) (struct muxer *, struct epg_broadcast *, const char *comment);
  int         (*m_write_pkt)  (struct muxer *, streaming_message_type_t, void *);
  int         (*m_add_marker) (struct muxer *);

  // State and configuration
  int                    m_eos;        // End of stream flag
  int                    m_errors;     // Error counter
  int                    m_caps;       // Capabilities flags
  muxer_config_t         m_config;     // Configuration
  muxer_hints_t         *m_hints;      // Client hints
} muxer_t;
```

**Key Virtual Methods**:

1. **`m_open_stream(muxer, fd)`** - Open for Socket Streaming
   - **Purpose**: Initialize muxer for streaming to a network socket
   - **Parameters**:
     - `fd`: File descriptor of the socket
   - **Returns**: 0 on success, negative errno on error
   - **Use Case**: HTTP streaming, HTSP streaming
   - **Notes**: Muxer writes directly to socket, no file buffering

2. **`m_open_file(muxer, filename)`** - Open for File Storage
   - **Purpose**: Initialize muxer for writing to a file
   - **Parameters**:
     - `filename`: Path to output file
   - **Returns**: 0 on success, negative errno on error
   - **Use Case**: DVR recordings
   - **Notes**: Creates file with configured permissions, handles atomic writes

3. **`m_mime(muxer, streaming_start)`** - Determine MIME Type
   - **Purpose**: Returns the MIME type for the container format
   - **Parameters**:
     - `streaming_start`: Stream metadata
   - **Returns**: MIME type string (e.g., "video/x-matroska", "video/mp2t")
   - **Use Case**: HTTP Content-Type header, file type detection
   - **Notes**: May return different MIME types for audio-only vs video streams

4. **`m_init(muxer, streaming_start, name)`** - Initialize Muxer
   - **Purpose**: Initialize muxer with stream information
   - **Parameters**:
     - `streaming_start`: Stream metadata (codecs, PIDs, languages)
     - `name`: Service or channel name
   - **Returns**: 0 on success, negative errno on error
   - **Use Case**: Called after open, before first packet
   - **Notes**: Writes container headers, sets up tracks

5. **`m_reconfigure(muxer, streaming_start)`** - Reconfigure Streams
   - **Purpose**: Handle stream changes (PMT updates)
   - **Parameters**:
     - `streaming_start`: Updated stream metadata
   - **Returns**: 0 on success, negative errno on error
   - **Use Case**: PMT changes, codec changes, track additions
   - **Notes**: Not all formats support reconfiguration

6. **`m_close(muxer)`** - Close Muxer
   - **Purpose**: Finalize container, write trailers, close file
   - **Returns**: 0 on success, negative errno on error
   - **Use Case**: End of recording, stream termination
   - **Notes**: Writes index, updates headers, flushes buffers

7. **`m_destroy(muxer)`** - Free Resources
   - **Purpose**: Free all memory and resources
   - **Returns**: void
   - **Use Case**: After close, during cleanup
   - **Notes**: Must be called even if close fails

8. **`m_write_meta(muxer, epg_broadcast, comment)`** - Write Metadata
   - **Purpose**: Embed EPG metadata into container
   - **Parameters**:
     - `epg_broadcast`: EPG program information
     - `comment`: Additional comment text
   - **Returns**: 0 on success, negative errno on error
   - **Use Case**: Recording metadata, program information
   - **Notes**: Format-specific, not all containers support metadata

9. **`m_write_pkt(muxer, message_type, data)`** - Write Packet
   - **Purpose**: Write a media packet to the container
   - **Parameters**:
     - `message_type`: Type of streaming message (SMT_PACKET, SMT_MPEGTS, etc.)
     - `data`: Packet data (th_pkt_t or pktbuf_t)
   - **Returns**: 0 on success, negative errno on error
   - **Use Case**: Every media packet during streaming
   - **Notes**: Most frequently called method, performance critical

10. **`m_add_marker(muxer)`** - Add Chapter Marker
    - **Purpose**: Add a chapter marker or bookmark
    - **Returns**: 0 on success, negative errno on error
    - **Use Case**: Commercial breaks, scene changes
    - **Notes**: Optional, not all formats support markers

**State Fields**:

1. **`m_eos`** - End of Stream
   - Set to 1 when stream has ended
   - Prevents further writes
   - Used for cleanup logic

2. **`m_errors`** - Error Counter
   - Incremented on write errors
   - Used for error reporting
   - May trigger automatic retry or failover

3. **`m_caps`** - Capabilities
   - Bitmask of muxer capabilities
   - `MC_CAP_ANOTHER_SERVICE`: Can stream multiple services with same SID
   - Used for feature detection

4. **`m_config`** - Configuration
   - Copy of muxer_config_t passed at creation
   - Contains format-specific settings
   - Immutable after creation

5. **`m_hints`** - Client Hints
   - Optional hints about client capabilities
   - Currently contains User-Agent string
   - Used for format selection and optimization

#### 9.1.2 The muxer_config_t Structure

**Location**: `src/muxer.h`

The `muxer_config_t` structure contains all configuration parameters for creating a muxer instance.

**Structure Definition**:
```c
typedef struct muxer_config {
  int                  m_type;                    // Container type (MC_*)
  int                  m_cache;                   // Cache strategy
  int                  m_file_permissions;        // File permissions (octal)
  int                  m_directory_permissions;   // Directory permissions (octal)
  int                  m_output_chunk;            // Chunk size for output (bytes)

  // Format-specific configuration
  union {
    struct {
      int              m_rewrite_sid;             // Rewrite service ID
      int              m_rewrite_pat;             // Rewrite PAT table
      int              m_rewrite_pmt;             // Rewrite PMT table
      int              m_rewrite_sdt;             // Rewrite SDT table
      int              m_rewrite_nit;             // Rewrite NIT table
      int              m_rewrite_eit;             // Rewrite EIT table
      char            *m_cmdline;                 // External command
      char            *m_mime;                    // MIME type override
      int              m_killsig;                 // Signal to send on stop
      int              m_killtimeout;             // Timeout before SIGKILL
    } pass;
#if ENABLE_LIBAV
    struct {
      uint16_t         m_rewrite_sid;             // Rewrite service ID
      int              m_rewrite_pmt;             // Rewrite PMT table
      int              m_rewrite_nit;             // Rewrite NIT table
    } transcode;
#endif
    struct {
      int              m_dvbsub_reorder;          // Reorder DVB subtitles
    } mkv;
    struct {
      int              m_force_type;              // Force audio type
      int              m_index;                   // Stream index
    } audioes;
  } u;
} muxer_config_t;
```

**Common Configuration Fields**:

1. **`m_type`** - Container Type
   - Enum value specifying the container format
   - Values: `MC_MATROSKA`, `MC_MPEGTS`, `MC_PASS`, `MC_AVMP4`, etc.
   - Determines which muxer implementation to use

2. **`m_cache`** - Cache Strategy
   - Controls file system caching behavior
   - Values:
     - `MC_CACHE_UNKNOWN` (0): Use system default
     - `MC_CACHE_SYSTEM` (1): Normal file system caching
     - `MC_CACHE_DONTKEEP` (2): Advise kernel not to cache
     - `MC_CACHE_SYNC` (3): Sync after each write
     - `MC_CACHE_SYNCDONTKEEP` (4): Sync and don't cache
   - Trade-off between performance and data safety

3. **`m_file_permissions`** - File Permissions
   - Octal permissions for created files (e.g., 0644)
   - Applied via chmod() after file creation
   - Default: 0644 (rw-r--r--)

4. **`m_directory_permissions`** - Directory Permissions
   - Octal permissions for created directories (e.g., 0755)
   - Used when creating parent directories
   - Default: 0755 (rwxr-xr-x)

5. **`m_output_chunk`** - Output Chunk Size
   - If > 0, output is written in chunks of this size
   - Used for network streaming to control packet sizes
   - 0 = no chunking (write as data arrives)

**Format-Specific Configuration**:

The union `u` contains format-specific settings. Only the relevant struct is used based on `m_type`.

**Pass-Through Configuration** (`u.pass`):
- Used for `MC_PASS` and `MC_RAW` types
- `m_rewrite_*`: Flags to rewrite PSI/SI tables
- `m_cmdline`: External command to pipe output through
- `m_mime`: Override MIME type
- `m_killsig`: Signal to send to external process on stop
- `m_killtimeout`: Seconds to wait before SIGKILL

**Transcoding Configuration** (`u.transcode`):
- Used when transcoding is enabled
- `m_rewrite_sid`: Rewrite service ID in output
- `m_rewrite_pmt`: Rewrite PMT table
- `m_rewrite_nit`: Rewrite NIT table

**Matroska Configuration** (`u.mkv`):
- Used for `MC_MATROSKA` and `MC_WEBM` types
- `m_dvbsub_reorder`: Reorder DVB subtitle packets for better compatibility

**Audio Elementary Stream Configuration** (`u.audioes`):
- Used for audio-only formats (MP3, AAC, AC3, etc.)
- `m_force_type`: Force specific audio type
- `m_index`: Stream index to extract

#### 9.1.3 Muxer Lifecycle

The muxer lifecycle follows a well-defined sequence of operations:

**1. Creation**:
```c
muxer_config_t cfg = {
  .m_type = MC_MATROSKA,
  .m_cache = MC_CACHE_SYSTEM,
  .m_file_permissions = 0644,
  .m_directory_permissions = 0755,
  .u.mkv.m_dvbsub_reorder = 1
};

muxer_hints_t *hints = muxer_hints_create("Tvheadend");
muxer_t *muxer = muxer_create(&cfg, hints);
```

**2. Opening**:
```c
// For file recording
int ret = muxer_open_file(muxer, "/path/to/recording.mkv");

// OR for network streaming
int ret = muxer_open_stream(muxer, socket_fd);
```

**3. Initialization**:
```c
streaming_start_t *ss = /* stream metadata */;
int ret = muxer_init(muxer, ss, "Channel Name");
```

**4. Writing Packets**:
```c
// Write media packets
th_pkt_t *pkt = /* packet */;
muxer_write_pkt(muxer, SMT_PACKET, pkt);

// Write metadata (optional)
epg_broadcast_t *epg = /* EPG data */;
muxer_write_meta(muxer, epg, "Recorded by Tvheadend");

// Add chapter markers (optional)
muxer_add_marker(muxer);
```

**5. Reconfiguration** (if stream changes):
```c
streaming_start_t *new_ss = /* updated metadata */;
int ret = muxer_reconfigure(muxer, new_ss);
```

**6. Closing**:
```c
int ret = muxer_close(muxer);
```

**7. Destruction**:
```c
muxer_destroy(muxer);
```

**Error Handling**:
- All operations return 0 on success, negative errno on error
- Errors increment `m_errors` counter
- Some errors are recoverable (e.g., temporary disk full)
- Fatal errors set `m_eos` flag to prevent further writes

#### 9.1.4 Muxer Selection Logic

The muxer factory function `muxer_create()` selects the appropriate muxer implementation based on the container type:

**Selection Process**:
```c
muxer_t* muxer_create(muxer_config_t *m_cfg, muxer_hints_t *hints)
{
  muxer_t *m;

  // Try pass-through muxer (handles MC_PASS, MC_RAW, MC_MPEGTS)
  m = pass_muxer_create(m_cfg, hints);
  if (m) return m;

  // Try Matroska muxer (handles MC_MATROSKA, MC_WEBM)
  m = mkv_muxer_create(m_cfg, hints);
  if (m) return m;

  // Try audio elementary stream muxer (handles MC_MPEG2AUDIO, MC_AAC, etc.)
  m = audioes_muxer_create(m_cfg, hints);
  if (m) return m;

#if CONFIG_LIBAV
  // Try libav muxer (handles MC_AVMATROSKA, MC_AVWEBM, MC_AVMP4)
  m = lav_muxer_create(m_cfg, hints);
  if (m) return m;
#endif

  // No suitable muxer found
  tvherror(LS_MUXER, "Can't find a muxer that supports '%s' container",
           muxer_container_type2txt(m_cfg->m_type));
  return NULL;
}
```

**Muxer Implementations**:

1. **Pass-Through Muxer** (`pass_muxer_create`)
   - Handles: `MC_PASS`, `MC_RAW`, `MC_MPEGTS`
   - Location: `src/muxer/muxer_pass.c`
   - Minimal processing, passes data through

2. **Matroska Muxer** (`mkv_muxer_create`)
   - Handles: `MC_MATROSKA`, `MC_WEBM`
   - Location: `src/muxer/muxer_mkv.c`
   - Native Matroska/WebM implementation

3. **Audio Elementary Stream Muxer** (`audioes_muxer_create`)
   - Handles: `MC_MPEG2AUDIO`, `MC_AAC`, `MC_AC3`, `MC_MP4A`, `MC_VORBIS`, `MC_AC4`
   - Location: `src/muxer/muxer_audioes.c`
   - Extracts and writes audio-only streams

4. **libav Muxer** (`lav_muxer_create`) - Optional
   - Handles: `MC_AVMATROSKA`, `MC_AVWEBM`, `MC_AVMP4`
   - Location: `src/muxer/muxer_libav.c`
   - Uses FFmpeg's libav for container handling
   - Only available if compiled with `CONFIG_LIBAV`

**Selection Criteria**:
- Each muxer checks if it can handle the requested container type
- Returns NULL if not supported
- First matching muxer is used
- Order matters: more specific muxers checked first

**Container Type Mapping**:

| Container Type | Muxer Implementation | Description |
|----------------|---------------------|-------------|
| `MC_PASS` | Pass-Through | Raw pass-through |
| `MC_RAW` | Pass-Through | Raw MPEG-TS |
| `MC_MPEGTS` | Pass-Through | MPEG-TS with PSI/SI rewriting |
| `MC_MATROSKA` | Matroska | Native MKV implementation |
| `MC_WEBM` | Matroska | WebM (subset of Matroska) |
| `MC_AVMATROSKA` | libav | MKV via FFmpeg |
| `MC_AVWEBM` | libav | WebM via FFmpeg |
| `MC_AVMP4` | libav | MP4 via FFmpeg |
| `MC_MPEG2AUDIO` | Audio ES | MPEG-2 Audio Layer II |
| `MC_AAC` | Audio ES | AAC audio |
| `MC_AC3` | Audio ES | Dolby Digital |
| `MC_MP4A` | Audio ES | MP4 audio |
| `MC_VORBIS` | Audio ES | Vorbis audio |
| `MC_AC4` | Audio ES | AC-4 audio |


### 9.2 Container Formats

Tvheadend supports multiple container formats, each with different characteristics, capabilities, and use cases. This section documents the major container formats and their implementations.

#### 9.2.1 Matroska (MKV) Format

**Overview**:
- Open, royalty-free container format
- Based on EBML (Extensible Binary Meta Language)
- Supports virtually all codecs
- Excellent metadata support
- Good seeking capabilities

**Implementation**: Native implementation in `src/muxer/muxer_mkv.c`

**Key Features**:
- **Multi-track support**: Multiple audio, video, and subtitle tracks
- **Metadata embedding**: Title, description, artwork, tags
- **Chapter markers**: Named chapters with timestamps
- **Attachments**: Embedded fonts, images, documents
- **Cue points**: Index for fast seeking
- **Segment linking**: Link multiple files together

**Codec Support**:
- **Video**: H.264, HEVC, MPEG-2, VP8, VP9, AV1, and more
- **Audio**: AAC, MP3, AC3, EAC3, DTS, Opus, Vorbis, FLAC, and more
- **Subtitles**: SRT, ASS/SSA, VobSub, PGS, DVB subtitles

**File Structure**:
```
EBML Header
  - DocType: "matroska"
  - DocTypeVersion: 4
  - DocTypeReadVersion: 2

Segment
  - SeekHead (index of top-level elements)
  - SegmentInfo (duration, title, muxing app, etc.)
  - Tracks (track definitions)
    - Track 1: Video (codec, resolution, frame rate)
    - Track 2: Audio (codec, sample rate, channels)
    - Track 3: Subtitles (codec, language)
  - Chapters (optional)
  - Tags (metadata)
  - Attachments (optional)
  - Cluster (media data)
    - SimpleBlock (video/audio packets)
    - BlockGroup (packets with additional info)
  - Cues (seek index)
```

**Advantages**:
- Universal codec support
- Rich metadata capabilities
- Open standard, no licensing fees
- Excellent for archival
- Good player support (VLC, MPC-HC, Kodi, etc.)

**Limitations**:
- Larger overhead than MPEG-TS
- Not streamable until finalized (needs Cues at end)
- Limited hardware player support compared to MP4

**Configuration Options**:
```c
muxer_config_t cfg = {
  .m_type = MC_MATROSKA,
  .u.mkv.m_dvbsub_reorder = 1  // Reorder DVB subtitles for compatibility
};
```

**Use Cases**:
- DVR recordings for archival
- Multi-track recordings (multiple audio languages)
- Recordings with subtitles
- When metadata embedding is important

#### 9.2.2 MPEG-TS (Transport Stream) Format

**Overview**:
- Standard broadcast container format
- Designed for lossy transmission
- Fixed 188-byte packets
- Robust error recovery
- Low overhead

**Implementation**: Pass-through muxer in `src/muxer/muxer_pass.c`

**Key Features**:
- **Fixed packet size**: 188 bytes per packet
- **Sync byte**: 0x47 at start of each packet
- **PID-based multiplexing**: Each stream has a unique PID
- **PSI/SI tables**: PAT, PMT, SDT, NIT, EIT for metadata
- **PCR**: Program Clock Reference for synchronization
- **Error detection**: Continuity counters, transport error indicator

**Packet Structure**:
```
Sync Byte (0x47) - 1 byte
Header - 3 bytes
  - Transport Error Indicator
  - Payload Unit Start Indicator
  - Transport Priority
  - PID (13 bits)
  - Scrambling Control
  - Adaptation Field Control
  - Continuity Counter (4 bits)
Adaptation Field (optional) - variable
Payload - remaining bytes (up to 184 bytes)
```

**PSI/SI Tables**:
- **PAT** (Program Association Table): Maps program numbers to PMT PIDs
- **PMT** (Program Map Table): Lists elementary streams for a program
- **SDT** (Service Description Table): Service names and metadata
- **NIT** (Network Information Table): Network information
- **EIT** (Event Information Table): EPG data

**Advantages**:
- Universal broadcast standard
- Streamable (no finalization needed)
- Robust to packet loss
- Hardware decoder support
- Low latency

**Limitations**:
- No native metadata support (uses PSI/SI tables)
- No chapter markers
- No attachments
- Higher overhead than PS or raw streams

**Configuration Options**:
```c
muxer_config_t cfg = {
  .m_type = MC_MPEGTS,
  .u.pass.m_rewrite_sid = 1,    // Rewrite service ID
  .u.pass.m_rewrite_pat = 1,    // Rewrite PAT
  .u.pass.m_rewrite_pmt = 1,    // Rewrite PMT
  .u.pass.m_rewrite_sdt = 1,    // Rewrite SDT
  .u.pass.m_rewrite_nit = 0,    // Don't rewrite NIT
  .u.pass.m_rewrite_eit = 0     // Don't rewrite EIT
};
```

**Use Cases**:
- Live streaming over HTTP
- IPTV streaming
- SAT>IP streaming
- When hardware compatibility is critical
- When low latency is required

#### 9.2.3 MP4 Format (via libav)

**Overview**:
- ISO Base Media File Format (ISO/IEC 14496-12)
- Widely supported container
- Optimized for progressive download
- Good metadata support

**Implementation**: libav muxer in `src/muxer/muxer_libav.c` (requires `CONFIG_LIBAV`)

**Key Features**:
- **Atom-based structure**: Hierarchical boxes (atoms)
- **Fast start**: moov atom at beginning for streaming
- **Fragmented MP4**: For live streaming (not currently used)
- **Metadata**: iTunes-compatible tags
- **Chapters**: Chapter track support

**File Structure**:
```
ftyp (File Type Box)
  - Major brand: "isom", "mp42", etc.
  - Compatible brands

moov (Movie Box) - metadata
  - mvhd (Movie Header)
  - trak (Track) - one per stream
    - tkhd (Track Header)
    - mdia (Media)
      - mdhd (Media Header)
      - hdlr (Handler)
      - minf (Media Information)
        - stbl (Sample Table)
          - stsd (Sample Description)
          - stts (Time-to-Sample)
          - stsc (Sample-to-Chunk)
          - stsz (Sample Size)
          - stco (Chunk Offset)

mdat (Media Data) - actual packets
```

**Codec Support**:
- **Video**: H.264, HEVC, MPEG-4 Part 2
- **Audio**: AAC, MP3, AC3, EAC3
- **Subtitles**: Limited (tx3g, WebVTT)

**Advantages**:
- Universal player support
- Good for mobile devices
- Efficient seeking
- Progressive download support
- iTunes/QuickTime compatible

**Limitations**:
- Requires finalization (moov atom at end, or fast-start)
- Limited codec support compared to MKV
- Subtitle support is limited
- Requires libav/FFmpeg

**Configuration Options**:
```c
muxer_config_t cfg = {
  .m_type = MC_AVMP4,
#if ENABLE_LIBAV
  .u.transcode.m_rewrite_sid = 1,
  .u.transcode.m_rewrite_pmt = 1
#endif
};
```

**Use Cases**:
- Recordings for mobile devices
- When maximum compatibility is needed
- Progressive download scenarios
- iTunes/QuickTime integration

#### 9.2.4 Pass-Through Format

**Overview**:
- Minimal processing container
- Passes input stream through with optional modifications
- Can pipe to external command
- Lowest CPU overhead

**Implementation**: Pass-through muxer in `src/muxer/muxer_pass.c`

**Key Features**:
- **Minimal overhead**: No remuxing, just pass-through
- **PSI/SI rewriting**: Optional table rewriting
- **External command**: Can pipe to ffmpeg or other tools
- **Custom MIME type**: Override MIME type for HTTP streaming

**Processing Options**:
1. **Raw pass-through** (`MC_RAW`):
   - No modifications
   - Passes data exactly as received
   - Fastest option

2. **MPEG-TS with rewriting** (`MC_MPEGTS`):
   - Rewrites PSI/SI tables
   - Adjusts service IDs
   - Fixes continuity counters

3. **External command** (`MC_PASS`):
   - Pipes data to external program
   - Program's stdout becomes output
   - Useful for custom processing

**Configuration Options**:
```c
muxer_config_t cfg = {
  .m_type = MC_PASS,
  .u.pass.m_rewrite_sid = 1,
  .u.pass.m_rewrite_pat = 1,
  .u.pass.m_rewrite_pmt = 1,
  .u.pass.m_cmdline = "ffmpeg -i pipe:0 -c copy -f matroska pipe:1",
  .u.pass.m_mime = "video/x-matroska",
  .u.pass.m_killsig = SIGTERM,
  .u.pass.m_killtimeout = 5
};
```

**External Command Example**:
```bash
# Transcode to H.264 + AAC in MP4
ffmpeg -i pipe:0 -c:v libx264 -c:a aac -f mp4 -movflags frag_keyframe+empty_moov pipe:1

# Add watermark
ffmpeg -i pipe:0 -i logo.png -filter_complex overlay=10:10 -c:a copy -f matroska pipe:1

# Convert to HLS
ffmpeg -i pipe:0 -c copy -f hls -hls_time 10 -hls_list_size 0 output.m3u8
```

**Advantages**:
- Minimal CPU usage
- Maximum flexibility (external commands)
- Preserves original quality
- Fast processing

**Limitations**:
- No metadata embedding (unless via external command)
- No chapter markers
- External command adds complexity
- Process management overhead

**Use Cases**:
- Live streaming with minimal latency
- When original format is desired
- Custom processing via external tools
- Testing and debugging

#### 9.2.5 Audio Elementary Stream Formats

**Overview**:
- Audio-only container formats
- Extracts single audio track
- Minimal overhead
- Format-specific headers

**Implementation**: Audio ES muxer in `src/muxer/muxer_audioes.c`

**Supported Formats**:

1. **MPEG-2 Audio Layer II** (`MC_MPEG2AUDIO`)
   - Extension: `.mp2`
   - MIME: `audio/mpeg`
   - Common in DVB broadcasts

2. **AAC** (`MC_AAC`)
   - Extension: `.aac`
   - MIME: `audio/aac`
   - ADTS framing

3. **AC-3 (Dolby Digital)** (`MC_AC3`)
   - Extension: `.ac3`
   - MIME: `audio/ac3`
   - Surround sound support

4. **MP4 Audio** (`MC_MP4A`)
   - Extension: `.mp4a`
   - MIME: `audio/aac`
   - MP4 container with audio only

5. **Vorbis** (`MC_VORBIS`)
   - Extension: `.oga`
   - MIME: `audio/ogg`
   - Ogg container

6. **AC-4** (`MC_AC4`)
   - Extension: `.ac4`
   - MIME: `audio/ac4`
   - Next-gen audio codec

**Configuration Options**:
```c
muxer_config_t cfg = {
  .m_type = MC_AAC,
  .u.audioes.m_force_type = 0,  // Auto-detect
  .u.audioes.m_index = 0        // First audio track
};
```

**Use Cases**:
- Radio streaming
- Audio-only recordings
- Extracting audio from video
- Minimal file size

#### 9.2.6 Format Comparison

| Feature | Matroska | MPEG-TS | MP4 | Pass | Audio ES |
|---------|----------|---------|-----|------|----------|
| **Codec Support** | Excellent | Good | Good | Any | Audio only |
| **Metadata** | Excellent | Limited | Good | None | None |
| **Chapters** | Yes | No | Yes | No | No |
| **Subtitles** | Excellent | Good | Limited | Pass-through | No |
| **Multi-track** | Yes | Yes | Yes | Yes | No |
| **Streaming** | Limited | Excellent | Good | Excellent | Excellent |
| **Seeking** | Excellent | Good | Excellent | Depends | Limited |
| **Overhead** | Medium | Low | Medium | Minimal | Minimal |
| **Hardware Support** | Limited | Excellent | Excellent | Depends | Good |
| **File Size** | Medium | Large | Medium | Depends | Small |
| **Complexity** | High | Medium | High | Low | Low |

**Selection Guidelines**:

- **For archival recordings**: Matroska (MKV)
- **For live streaming**: MPEG-TS or Pass-through
- **For mobile devices**: MP4
- **For maximum compatibility**: MPEG-TS or MP4
- **For minimal processing**: Pass-through
- **For radio/audio**: Audio ES formats
- **For custom processing**: Pass-through with external command


### 9.3 Metadata Embedding

Metadata embedding allows muxers to include program information, artwork, and other descriptive data within the container file. This section documents how different formats handle metadata and how Tvheadend embeds EPG data into recordings.

#### 9.3.1 Metadata Structure

**EPG Broadcast Structure** (`epg_broadcast_t`):

The primary source of metadata for recordings is the EPG (Electronic Program Guide) broadcast structure, which contains:

```c
struct epg_broadcast {
  char *title;              // Program title
  char *subtitle;           // Episode title or subtitle
  char *summary;            // Short description
  char *description;        // Full description
  
  uint16_t episode_num;     // Episode number
  uint16_t season_num;      // Season number
  uint16_t part_num;        // Part number (for multi-part episodes)
  uint16_t part_cnt;        // Total parts
  
  time_t start;             // Start time (Unix timestamp)
  time_t stop;              // End time (Unix timestamp)
  
  char *genre[16];          // Genre classifications
  uint8_t genre_count;      // Number of genres
  
  uint8_t age_rating;       // Age rating (0-18)
  uint8_t star_rating;      // Star rating (0-10)
  
  char *image;              // URL to program artwork
  char *copyright_year;     // Copyright year
  
  // ... additional fields
};
```

**Metadata Embedding Function**:
```c
int muxer_write_meta(muxer_t *m, epg_broadcast_t *eb, const char *comment);
```

**Parameters**:
- `m`: Muxer instance
- `eb`: EPG broadcast data (can be NULL)
- `comment`: Additional comment text (can be NULL)

**Returns**: 0 on success, negative errno on error

#### 9.3.2 Matroska Metadata Embedding

Matroska provides the most comprehensive metadata support through its Tags element.

**Tag Structure**:
```xml
<Tags>
  <Tag>
    <Targets>
      <TargetTypeValue>50</TargetTypeValue>  <!-- Movie/Episode level -->
    </Targets>
    <Simple>
      <Name>TITLE</Name>
      <String>Program Title</String>
    </Simple>
    <Simple>
      <Name>SUBTITLE</Name>
      <String>Episode Title</String>
    </Simple>
    <Simple>
      <Name>DESCRIPTION</Name>
      <String>Full program description</String>
    </Simple>
    <Simple>
      <Name>DATE_RECORDED</Name>
      <String>2025-11-15T14:30:00Z</String>
    </Simple>
    <Simple>
      <Name>GENRE</Name>
      <String>Drama</String>
    </Simple>
    <Simple>
      <Name>PART_NUMBER</Name>
      <String>1</String>
    </Simple>
    <Simple>
      <Name>TOTAL_PARTS</Name>
      <String>2</String>
    </Simple>
  </Tag>
</Tags>
```

**Supported Metadata Fields**:

| EPG Field | Matroska Tag | Description |
|-----------|--------------|-------------|
| `title` | `TITLE` | Program title |
| `subtitle` | `SUBTITLE` | Episode title |
| `summary` | `SUMMARY` | Short description |
| `description` | `DESCRIPTION` | Full description |
| `start` | `DATE_RECORDED` | Recording start time |
| `episode_num` | `PART_NUMBER` | Episode number |
| `season_num` | `SEASON` | Season number |
| `part_num` | `PART_NUMBER` | Part number |
| `part_cnt` | `TOTAL_PARTS` | Total parts |
| `genre` | `GENRE` | Genre classification |
| `age_rating` | `LAW_RATING` | Age rating |
| `star_rating` | `RATING` | Star rating |
| `copyright_year` | `COPYRIGHT` | Copyright year |
| `comment` | `COMMENT` | User comment |

**Implementation**:
```c
// In mkv_muxer_write_meta()
mk_writeStr(m->mkv_writer, MKV_TAG_SIMPLE_NAME, "TITLE");
mk_writeStr(m->mkv_writer, MKV_TAG_SIMPLE_STRING, eb->title);

mk_writeStr(m->mkv_writer, MKV_TAG_SIMPLE_NAME, "DESCRIPTION");
mk_writeStr(m->mkv_writer, MKV_TAG_SIMPLE_STRING, eb->description);

// ... write other tags
```

**Advantages**:
- Rich metadata support
- Standardized tag names
- Multiple target levels (file, track, chapter)
- Extensible (custom tags supported)

#### 9.3.3 MP4 Metadata Embedding

MP4 uses iTunes-compatible metadata atoms in the `udta` (User Data) box.

**Metadata Atoms**:
```
moov
  └─ udta (User Data)
      └─ meta (Metadata)
          ├─ hdlr (Handler: 'mdir')
          └─ ilst (Item List)
              ├─ ©nam (Title)
              ├─ ©des (Description)
              ├─ ©day (Year)
              ├─ ©gen (Genre)
              ├─ tvsn (TV Season)
              ├─ tves (TV Episode)
              ├─ tvsh (TV Show)
              └─ covr (Cover Art)
```

**Supported Metadata Fields**:

| EPG Field | MP4 Atom | Description |
|-----------|----------|-------------|
| `title` | `©nam` | Title |
| `subtitle` | `©des` | Description |
| `description` | `ldes` | Long description |
| `start` | `©day` | Year (extracted from date) |
| `episode_num` | `tves` | TV episode number |
| `season_num` | `tvsn` | TV season number |
| `genre` | `©gen` | Genre |
| `copyright_year` | `©day` | Copyright year |

**Implementation** (via libav):
```c
// In lav_muxer_write_meta()
AVDictionary *metadata = NULL;

av_dict_set(&metadata, "title", eb->title, 0);
av_dict_set(&metadata, "description", eb->description, 0);
av_dict_set(&metadata, "genre", eb->genre[0], 0);

char episode[32];
snprintf(episode, sizeof(episode), "%d", eb->episode_num);
av_dict_set(&metadata, "episode_id", episode, 0);

// Apply metadata to format context
av_dict_copy(&m->lav_oc->metadata, metadata, 0);
av_dict_free(&metadata);
```

**Limitations**:
- Less flexible than Matroska
- Limited field support
- iTunes-centric naming
- No multi-part episode support

#### 9.3.4 MPEG-TS Metadata Embedding

MPEG-TS does not have native metadata embedding. Instead, it uses PSI/SI tables for program information.

**Service Description Table (SDT)**:
```
SDT (Service Description Table)
  - Service ID
  - Service Name
  - Service Provider Name
  - Service Type (TV, Radio, etc.)
```

**Event Information Table (EIT)**:
```
EIT (Event Information Table)
  - Event ID
  - Start Time
  - Duration
  - Running Status
  - Free CA Mode
  - Descriptors:
    - Short Event Descriptor (title, description)
    - Extended Event Descriptor (detailed description)
    - Content Descriptor (genre)
    - Parental Rating Descriptor
```

**Implementation**:
```c
// In pass_muxer_write_meta()
// MPEG-TS muxer updates EIT table with EPG data
if (m->pm_rewrite_eit) {
  eit_section_t *eit = eit_section_create();
  eit_add_event(eit, eb->event_id, eb->start, eb->stop - eb->start);
  eit_add_short_event_descriptor(eit, eb->title, eb->summary);
  eit_add_extended_event_descriptor(eit, eb->description);
  eit_add_content_descriptor(eit, eb->genre);
  
  // Write EIT section to stream
  pass_muxer_write_section(m, eit);
}
```

**Limitations**:
- Metadata is in-band (part of stream)
- Limited field support
- Requires PSI/SI parsing to extract
- Not all players support EIT

#### 9.3.5 Artwork Embedding

Some formats support embedding artwork (cover images, posters) directly in the container.

**Matroska Attachments**:
```c
// Add artwork as attachment
mk_createAttachment(m->mkv_writer,
                    "cover.jpg",           // Filename
                    "Cover Art",           // Description
                    "image/jpeg",          // MIME type
                    artwork_data,          // Image data
                    artwork_size);         // Image size
```

**MP4 Cover Art**:
```c
// Add artwork via libav
AVPacket pkt;
av_init_packet(&pkt);
pkt.data = artwork_data;
pkt.size = artwork_size;
pkt.stream_index = video_stream_index;

AVDictionaryEntry *tag = av_dict_get(m->lav_oc->metadata, "cover", NULL, 0);
if (!tag) {
  av_dict_set(&m->lav_oc->metadata, "cover", "1", 0);
  // Attach artwork to stream
}
```

**Artwork Sources**:
1. **EPG Image URL**: Downloaded from `epg_broadcast->image`
2. **Channel Icon**: From channel configuration
3. **User-provided**: Manually specified artwork file

**Artwork Processing**:
- Downloaded asynchronously during recording
- Cached locally to avoid repeated downloads
- Resized if necessary (e.g., for thumbnails)
- Embedded at recording finalization

#### 9.3.6 Chapter Markers

Chapter markers allow users to navigate to specific points in a recording.

**Matroska Chapters**:
```xml
<Chapters>
  <EditionEntry>
    <EditionFlagDefault>1</EditionFlagDefault>
    <ChapterAtom>
      <ChapterTimeStart>00:00:00.000</ChapterTimeStart>
      <ChapterDisplay>
        <ChapterString>Program Start</ChapterString>
        <ChapterLanguage>eng</ChapterLanguage>
      </ChapterDisplay>
    </ChapterAtom>
    <ChapterAtom>
      <ChapterTimeStart>00:15:30.000</ChapterTimeStart>
      <ChapterDisplay>
        <ChapterString>Commercial Break</ChapterString>
        <ChapterLanguage>eng</ChapterLanguage>
      </ChapterDisplay>
    </ChapterAtom>
    <ChapterAtom>
      <ChapterTimeStart>00:18:00.000</ChapterTimeStart>
      <ChapterDisplay>
        <ChapterString>Program Resume</ChapterString>
        <ChapterLanguage>eng</ChapterLanguage>
      </ChapterDisplay>
    </ChapterAtom>
  </EditionEntry>
</Chapters>
```

**Adding Chapters**:
```c
// Add chapter marker
int muxer_add_marker(muxer_t *m);

// Implementation in MKV muxer
static int mkv_muxer_add_marker(muxer_t *m)
{
  mkv_muxer_t *mkv = (mkv_muxer_t *)m;
  int64_t timestamp = mkv->current_timestamp;
  
  mk_createChapterAtom(mkv->mkv_writer, timestamp);
  mk_writeStr(mkv->mkv_writer, MKV_CHAPTER_STRING, "Chapter");
  mk_writeStr(mkv->mkv_writer, MKV_CHAPTER_LANGUAGE, "eng");
  
  return 0;
}
```

**Chapter Sources**:
1. **Manual markers**: User-triggered during recording
2. **Commercial detection**: Automatic detection of commercial breaks
3. **Scene changes**: Detected via video analysis
4. **EPG boundaries**: Program start/end times

**MP4 Chapters**:
```c
// MP4 uses a chapter track (tref)
AVChapter *chapter = avformat_new_chapter(m->lav_oc,
                                          chapter_id,
                                          (AVRational){1, 1000},
                                          start_time,
                                          end_time,
                                          "Chapter Title");
```

**MPEG-TS Chapters**:
- Not supported natively
- Can use custom descriptors or external files

#### 9.3.7 Metadata Update Timing

Metadata can be written at different points during recording:

**1. At Recording Start** (`muxer_init`):
- Basic metadata (title, description)
- Known at start time
- Written to container headers

**2. During Recording** (`muxer_write_meta`):
- Updated EPG information
- Additional metadata discovered during recording
- Chapter markers

**3. At Recording End** (`muxer_close`):
- Final metadata updates
- Artwork embedding
- Index/cue generation
- Header finalization

**Example Flow**:
```c
// 1. Start recording
muxer_t *m = muxer_create(&cfg, hints);
muxer_open_file(m, "/recordings/program.mkv");
muxer_init(m, ss, "Program Title");

// Initial metadata
epg_broadcast_t *epg = epg_get_current();
muxer_write_meta(m, epg, "Recorded by Tvheadend");

// 2. During recording
while (recording) {
  th_pkt_t *pkt = get_packet();
  muxer_write_pkt(m, SMT_PACKET, pkt);
  
  // Add chapter at commercial break
  if (commercial_detected) {
    muxer_add_marker(m);
  }
  
  // Update metadata if EPG changes
  if (epg_updated) {
    epg = epg_get_current();
    muxer_write_meta(m, epg, NULL);
  }
}

// 3. End recording
muxer_close(m);  // Finalizes metadata, writes index
muxer_destroy(m);
```


### 9.4 Subtitle and Audio Track Handling

Modern container formats support multiple audio and subtitle tracks, allowing users to select their preferred language or audio format. This section documents how Tvheadend handles multi-track streams in different container formats.

#### 9.4.1 Multi-Track Support Overview

**Track Types**:
1. **Video tracks**: Primary video stream (usually only one)
2. **Audio tracks**: Multiple audio languages or formats
3. **Subtitle tracks**: Multiple subtitle languages and formats

**Track Identification**:
- Each track has a unique index/ID within the container
- Tracks have associated metadata (language, codec, format)
- Default track flags indicate preferred tracks

**Track Selection**:
- Muxer includes all tracks from source stream
- Client/player selects which tracks to decode
- Some formats support track disabling

#### 9.4.2 Audio Track Handling

**Audio Track Structure**:
```c
typedef struct elementary_stream {
  int                 es_index;           // Track index
  streaming_component_type_t es_type;     // Type (AUDIO, VIDEO, SUBTITLE)
  uint16_t            es_pid;             // PID (for MPEG-TS)
  char                es_lang[4];         // ISO 639-2 language code
  uint8_t             es_audio_type;      // Audio type (clean, visual impaired, etc.)
  uint8_t             es_audio_version;   // Audio version
  
  // Codec information
  int                 es_codec;           // Codec ID
  char               *es_codec_profile;   // Codec profile
  int                 es_channels;        // Channel count
  int                 es_sample_rate;     // Sample rate
  
  // Metadata
  char               *es_title;           // Track title
  int                 es_default;         // Default track flag
  int                 es_forced;          // Forced track flag
} elementary_stream_t;
```

**Audio Types**:
```c
#define AUDIO_TYPE_UNDEFINED        0x00
#define AUDIO_TYPE_CLEAN_EFFECTS    0x01  // Clean audio (no effects)
#define AUDIO_TYPE_HEARING_IMPAIRED 0x02  // For hearing impaired
#define AUDIO_TYPE_VISUAL_IMPAIRED  0x03  // Audio description
```

**Matroska Audio Track**:
```c
// Create audio track in MKV
mk_createTrack(m->mkv_writer, MK_TRACK_AUDIO);
mk_writeStr(m->mkv_writer, MKV_TRACK_CODEC_ID, "A_AAC");
mk_writeStr(m->mkv_writer, MKV_TRACK_LANGUAGE, "eng");
mk_writeStr(m->mkv_writer, MKV_TRACK_NAME, "English Stereo");
mk_writeUInt(m->mkv_writer, MKV_TRACK_DEFAULT, 1);
mk_writeUInt(m->mkv_writer, MKV_TRACK_FORCED, 0);

// Audio settings
mk_writeUInt(m->mkv_writer, MKV_AUDIO_CHANNELS, 2);
mk_writeFloat(m->mkv_writer, MKV_AUDIO_SAMPLERATE, 48000.0);
mk_writeUInt(m->mkv_writer, MKV_AUDIO_BITDEPTH, 16);
```

**MPEG-TS Audio Track**:
```c
// Audio track in PMT
pmt_add_stream(pmt, STREAM_TYPE_AUDIO_AAC, audio_pid);
pmt_add_descriptor(pmt, DESCRIPTOR_ISO_639_LANGUAGE,
                   "eng", AUDIO_TYPE_CLEAN_EFFECTS);
```

**MP4 Audio Track**:
```c
// Audio track via libav
AVStream *audio_stream = avformat_new_stream(m->lav_oc, NULL);
audio_stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
audio_stream->codecpar->codec_id = AV_CODEC_ID_AAC;
audio_stream->codecpar->channels = 2;
audio_stream->codecpar->sample_rate = 48000;

av_dict_set(&audio_stream->metadata, "language", "eng", 0);
av_dict_set(&audio_stream->metadata, "title", "English Stereo", 0);
```

**Audio Track Mapping**:
```
Source Stream → Muxer → Container
  Audio PID 101 (eng, stereo)  → Track 1 (default)
  Audio PID 102 (spa, stereo)  → Track 2
  Audio PID 103 (eng, 5.1)     → Track 3
  Audio PID 104 (fra, stereo)  → Track 4
```

**Audio Format Conversion**:
- Muxer typically passes audio through without transcoding
- Format conversion handled by profile/transcoding layer
- Some formats require specific audio codecs (e.g., MP4 prefers AAC)

#### 9.4.3 Subtitle Track Handling

**Subtitle Types**:

1. **DVB Subtitles** (Bitmap-based)
   - Transmitted as PES packets
   - Contain bitmap images
   - Common in European broadcasts
   - Codec ID: `S_DVBSUB` (Matroska), `SUBTITLE_DVB` (MPEG-TS)

2. **Teletext Subtitles** (Text-based)
   - Transmitted in VBI (Vertical Blanking Interval)
   - Text with simple formatting
   - Common in European broadcasts
   - Codec ID: `S_TEXT/UTF8` (Matroska), `PRIVATE_DATA` (MPEG-TS)

3. **SRT Subtitles** (Text-based)
   - SubRip format
   - Plain text with timestamps
   - Widely supported
   - Codec ID: `S_TEXT/UTF8` (Matroska)

4. **ASS/SSA Subtitles** (Text-based)
   - Advanced SubStation Alpha
   - Rich formatting, positioning, effects
   - Codec ID: `S_TEXT/ASS` (Matroska)

5. **VobSub** (Bitmap-based)
   - DVD subtitle format
   - Bitmap images with palette
   - Codec ID: `S_VOBSUB` (Matroska)

6. **PGS** (Bitmap-based)
   - Blu-ray subtitle format
   - High-resolution bitmaps
   - Codec ID: `S_HDMV/PGS` (Matroska)

**Subtitle Track Structure**:
```c
typedef struct elementary_stream {
  // ... (same as audio)
  
  // Subtitle-specific fields
  uint8_t             es_composition_id;   // DVB subtitle composition ID
  uint8_t             es_ancillary_id;     // DVB subtitle ancillary ID
  uint16_t            es_teletext_page;    // Teletext page number
  uint8_t             es_teletext_type;    // Teletext type
} elementary_stream_t;
```

**Matroska Subtitle Track**:
```c
// Create subtitle track in MKV
mk_createTrack(m->mkv_writer, MK_TRACK_SUBTITLE);
mk_writeStr(m->mkv_writer, MKV_TRACK_CODEC_ID, "S_DVBSUB");
mk_writeStr(m->mkv_writer, MKV_TRACK_LANGUAGE, "eng");
mk_writeStr(m->mkv_writer, MKV_TRACK_NAME, "English Subtitles");
mk_writeUInt(m->mkv_writer, MKV_TRACK_DEFAULT, 0);
mk_writeUInt(m->mkv_writer, MKV_TRACK_FORCED, 0);

// DVB subtitle private data
uint8_t private_data[4] = {
  es->es_composition_id >> 8,
  es->es_composition_id & 0xff,
  es->es_ancillary_id >> 8,
  es->es_ancillary_id & 0xff
};
mk_writeBin(m->mkv_writer, MKV_TRACK_CODEC_PRIVATE, private_data, 4);
```

**MPEG-TS Subtitle Track**:
```c
// Subtitle track in PMT
pmt_add_stream(pmt, STREAM_TYPE_PRIVATE_DATA, subtitle_pid);
pmt_add_descriptor(pmt, DESCRIPTOR_SUBTITLING,
                   "eng", SUBTITLING_TYPE_NORMAL,
                   composition_id, ancillary_id);
```

**Subtitle Reordering** (Matroska-specific):

DVB subtitles may arrive out of order. Matroska muxer can reorder them for better compatibility:

```c
// Configuration
muxer_config_t cfg = {
  .m_type = MC_MATROSKA,
  .u.mkv.m_dvbsub_reorder = 1  // Enable reordering
};

// Implementation
if (m->mkv_dvbsub_reorder && es->es_type == SCT_DVBSUB) {
  // Buffer subtitle packets
  dvbsub_buffer_add(m->dvbsub_buffer, pkt);
  
  // Flush when display set is complete
  if (dvbsub_is_complete(pkt)) {
    dvbsub_buffer_flush(m->dvbsub_buffer, m->mkv_writer);
  }
}
```

**Subtitle Format Conversion**:

Some formats require subtitle conversion:

1. **DVB → SRT**: Extract text from DVB subtitles (OCR required)
2. **Teletext → SRT**: Parse teletext pages to plain text
3. **SRT → ASS**: Add formatting to plain text subtitles

Tvheadend typically passes subtitles through without conversion. External tools (e.g., CCExtractor, ffmpeg) can be used for conversion.

#### 9.4.4 Language Metadata

**ISO 639-2 Language Codes**:
- 3-letter codes (e.g., "eng", "spa", "fra", "deu")
- Used in all container formats
- Stored in track metadata

**Language Code Mapping**:
```c
// Map DVB language descriptor to ISO 639-2
const char *lang = dvb_lang_to_iso639(dvb_lang_code);

// Set track language
mk_writeStr(m->mkv_writer, MKV_TRACK_LANGUAGE, lang);
```

**Language Priority**:
- User can configure preferred languages
- Muxer marks preferred language as default track
- Players typically auto-select default track

**Example Language Configuration**:
```c
// User preferences
const char *preferred_audio_langs[] = {"eng", "spa", NULL};
const char *preferred_subtitle_langs[] = {"eng", NULL};

// Mark default tracks
for (int i = 0; i < audio_track_count; i++) {
  if (lang_in_list(audio_tracks[i].lang, preferred_audio_langs)) {
    audio_tracks[i].default_flag = 1;
    break;  // Only one default
  }
}
```

#### 9.4.5 Track Disabling and Filtering

**Track Filtering**:

Users can configure which tracks to include in recordings:

```c
// DVR configuration
typedef struct dvr_config {
  // ...
  int dvr_audio_all;           // Include all audio tracks
  char *dvr_audio_langs;       // Comma-separated language list
  int dvr_subtitle_all;        // Include all subtitle tracks
  char *dvr_subtitle_langs;    // Comma-separated language list
} dvr_config_t;
```

**Track Selection Logic**:
```c
// Check if audio track should be included
int include_audio_track(elementary_stream_t *es, dvr_config_t *cfg)
{
  // Include all if configured
  if (cfg->dvr_audio_all)
    return 1;
  
  // Check language list
  if (cfg->dvr_audio_langs) {
    char *langs = strdup(cfg->dvr_audio_langs);
    char *lang = strtok(langs, ",");
    while (lang) {
      if (strcmp(lang, es->es_lang) == 0) {
        free(langs);
        return 1;
      }
      lang = strtok(NULL, ",");
    }
    free(langs);
  }
  
  return 0;  // Exclude track
}
```

**Matroska Track Disabling**:
```c
// Disable track (still in container, but marked as disabled)
mk_writeUInt(m->mkv_writer, MKV_TRACK_ENABLED, 0);
```

**MPEG-TS Track Filtering**:
```c
// Simply don't include PID in PMT
// Packets for that PID are not written to output
```

#### 9.4.6 Multi-Track Recording Example

**Scenario**: Recording a program with multiple audio and subtitle tracks

**Source Stream**:
- Video: H.264 1080p (PID 100)
- Audio 1: AAC Stereo English (PID 101)
- Audio 2: AAC Stereo Spanish (PID 102)
- Audio 3: AC3 5.1 English (PID 103)
- Subtitle 1: DVB English (PID 201)
- Subtitle 2: DVB Spanish (PID 202)

**Matroska Output**:
```
Tracks:
  Track 1: Video (H.264, 1920x1080, 25fps) [default]
  Track 2: Audio (AAC, Stereo, 48kHz, English) [default]
  Track 3: Audio (AAC, Stereo, 48kHz, Spanish)
  Track 4: Audio (AC3, 5.1, 48kHz, English)
  Track 5: Subtitle (DVB, English)
  Track 6: Subtitle (DVB, Spanish)
```

**MPEG-TS Output**:
```
PMT:
  Video: PID 100, Type 0x1B (H.264)
  Audio: PID 101, Type 0x0F (AAC), Lang "eng"
  Audio: PID 102, Type 0x0F (AAC), Lang "spa"
  Audio: PID 103, Type 0x81 (AC3), Lang "eng"
  Subtitle: PID 201, Type 0x06 (Private), Lang "eng"
  Subtitle: PID 202, Type 0x06 (Private), Lang "spa"
```

**Player Behavior**:
- Plays default video track (Track 1)
- Plays default audio track (Track 2 - English Stereo)
- User can switch to other audio tracks (Spanish, 5.1)
- User can enable subtitles (English or Spanish)


### 9.5 File Writing and Buffering

Efficient file I/O is critical for recording performance and data integrity. This section documents the file writing strategies, buffering mechanisms, and error handling used by Tvheadend's muxer system.

#### 9.5.1 File I/O Strategy

**File Opening**:
```c
int muxer_open_file(muxer_t *m, const char *filename)
{
  int fd;
  
  // Create parent directories if needed
  if (makedirs(filename, m->m_config.m_directory_permissions) < 0)
    return -errno;
  
  // Open file with appropriate flags
  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0)
    return -errno;
  
  // Set final permissions
  if (fchmod(fd, m->m_config.m_file_permissions) < 0) {
    close(fd);
    return -errno;
  }
  
  // Store file descriptor
  m->m_fd = fd;
  
  return 0;
}
```

**File Flags**:
- `O_WRONLY`: Write-only access
- `O_CREAT`: Create file if it doesn't exist
- `O_TRUNC`: Truncate existing file to zero length
- `O_SYNC`: Synchronous writes (optional, based on cache config)
- `O_DIRECT`: Direct I/O, bypass page cache (optional)

**Permissions**:
- Initial: 0600 (rw------) during writing
- Final: Configured permissions (e.g., 0644) after close
- Prevents access to incomplete files

#### 9.5.2 Buffering Mechanisms

**Write Buffering**:

Muxers use internal buffers to batch writes and reduce system call overhead:

```c
typedef struct muxer_buffer {
  uint8_t *mb_data;        // Buffer data
  size_t   mb_size;        // Buffer size
  size_t   mb_used;        // Bytes used
  size_t   mb_capacity;    // Total capacity
} muxer_buffer_t;
```

**Buffer Initialization**:
```c
void muxer_buffer_init(muxer_buffer_t *mb, size_t capacity)
{
  mb->mb_data = malloc(capacity);
  mb->mb_size = 0;
  mb->mb_used = 0;
  mb->mb_capacity = capacity;
}
```

**Buffered Write**:
```c
int muxer_buffer_write(muxer_buffer_t *mb, int fd, const void *data, size_t size)
{
  // If data fits in buffer, just copy
  if (mb->mb_used + size <= mb->mb_capacity) {
    memcpy(mb->mb_data + mb->mb_used, data, size);
    mb->mb_used += size;
    return 0;
  }
  
  // Flush existing buffer
  if (mb->mb_used > 0) {
    if (write(fd, mb->mb_data, mb->mb_used) != mb->mb_used)
      return -errno;
    mb->mb_used = 0;
  }
  
  // If data is larger than buffer, write directly
  if (size > mb->mb_capacity) {
    if (write(fd, data, size) != size)
      return -errno;
    return 0;
  }
  
  // Copy to buffer
  memcpy(mb->mb_data, data, size);
  mb->mb_used = size;
  
  return 0;
}
```

**Buffer Flush**:
```c
int muxer_buffer_flush(muxer_buffer_t *mb, int fd)
{
  if (mb->mb_used == 0)
    return 0;
  
  if (write(fd, mb->mb_data, mb->mb_used) != mb->mb_used)
    return -errno;
  
  mb->mb_used = 0;
  return 0;
}
```

**Buffer Sizes**:
- Default: 64 KB - 256 KB
- Larger buffers reduce system call overhead
- Smaller buffers reduce memory usage
- Trade-off between performance and latency

#### 9.5.3 Cache Strategies

Tvheadend supports multiple cache strategies to balance performance and data safety:

**Cache Types**:

1. **MC_CACHE_SYSTEM** (Default)
   - Use system default caching
   - Kernel manages page cache
   - Best performance
   - Data may be lost on crash

2. **MC_CACHE_DONTKEEP**
   - Advise kernel not to cache
   - Frees memory for other uses
   - Slightly slower writes
   - Good for large recordings

3. **MC_CACHE_SYNC**
   - Synchronous writes
   - Data flushed to disk immediately
   - Slowest performance
   - Maximum data safety

4. **MC_CACHE_SYNCDONTKEEP**
   - Sync + don't keep
   - Combines sync and don't keep
   - Slowest, but safest
   - Minimal memory usage

**Implementation**:
```c
void muxer_cache_update(muxer_t *m, int fd, off_t pos, size_t size)
{
  switch (m->m_config.m_cache) {
  case MC_CACHE_UNKNOWN:
  case MC_CACHE_SYSTEM:
    // Do nothing, use system default
    break;
    
  case MC_CACHE_SYNC:
    // Sync data to disk
    fdatasync(fd);
    break;
    
  case MC_CACHE_SYNCDONTKEEP:
    // Sync data to disk
    fdatasync(fd);
    // Fall through to don't keep
    
  case MC_CACHE_DONTKEEP:
    // Advise kernel not to cache
#if defined(PLATFORM_DARWIN)
    fcntl(fd, F_NOCACHE, 1);
#elif !ENABLE_ANDROID
    posix_fadvise(fd, pos, size, POSIX_FADV_DONTNEED);
#endif
    break;
  }
}
```

**fdatasync vs fsync**:
- `fdatasync()`: Syncs data only (not metadata like timestamps)
- `fsync()`: Syncs data and metadata
- `fdatasync()` is faster and sufficient for most cases

**Platform-Specific Implementations**:
- **Linux**: `fdatasync()`, `posix_fadvise()`
- **macOS**: `fcntl(F_FULLFSYNC)`, `fcntl(F_NOCACHE)`
- **FreeBSD**: `fsync()`, `posix_fadvise()`
- **Android**: Limited support, uses defaults

**Cache Strategy Selection**:
```c
// For live streaming (low latency)
cfg.m_cache = MC_CACHE_SYSTEM;

// For large recordings (save memory)
cfg.m_cache = MC_CACHE_DONTKEEP;

// For critical recordings (maximum safety)
cfg.m_cache = MC_CACHE_SYNC;

// For critical + large recordings
cfg.m_cache = MC_CACHE_SYNCDONTKEEP;
```

#### 9.5.4 Atomic Write Operations

To prevent corruption from crashes or power failures, Tvheadend uses atomic write operations:

**Temporary File Pattern**:
```c
// Write to temporary file
char temp_filename[PATH_MAX];
snprintf(temp_filename, sizeof(temp_filename), "%s.tmp", final_filename);

// Open and write to temp file
int fd = open(temp_filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
// ... write data ...
close(fd);

// Atomically rename to final filename
if (rename(temp_filename, final_filename) < 0) {
  unlink(temp_filename);
  return -errno;
}
```

**Benefits**:
- Incomplete files never have final name
- Rename is atomic on most filesystems
- Prevents partial file corruption
- Easy to detect and clean up temp files

**Limitations**:
- Requires 2x disk space during write
- Rename may not be atomic across filesystems
- Not used for streaming (only for file recordings)

**Finalization Process**:
```c
int muxer_close(muxer_t *m)
{
  // Flush any buffered data
  muxer_buffer_flush(&m->m_buffer, m->m_fd);
  
  // Write container trailer/index
  m->m_write_trailer(m);
  
  // Sync to disk
  if (m->m_config.m_cache >= MC_CACHE_SYNC)
    fdatasync(m->m_fd);
  
  // Set final permissions
  fchmod(m->m_fd, m->m_config.m_file_permissions);
  
  // Close file
  close(m->m_fd);
  m->m_fd = -1;
  
  // Rename from temp to final (if using temp file)
  if (m->m_temp_filename) {
    if (rename(m->m_temp_filename, m->m_final_filename) < 0) {
      unlink(m->m_temp_filename);
      return -errno;
    }
  }
  
  return 0;
}
```

#### 9.5.5 Error Handling

**Write Error Detection**:
```c
ssize_t bytes_written = write(fd, data, size);
if (bytes_written < 0) {
  // Write failed
  int err = errno;
  tvherror(LS_MUXER, "Write failed: %s", strerror(err));
  m->m_errors++;
  return -err;
}

if (bytes_written != size) {
  // Partial write (disk full, quota exceeded, etc.)
  tvherror(LS_MUXER, "Partial write: %zd of %zu bytes", bytes_written, size);
  m->m_errors++;
  return -ENOSPC;
}
```

**Common Error Codes**:
- `ENOSPC`: No space left on device
- `EDQUOT`: Disk quota exceeded
- `EIO`: I/O error (hardware failure)
- `EPIPE`: Broken pipe (for streaming)
- `ECONNRESET`: Connection reset (for streaming)

**Error Recovery**:

1. **Retry on Temporary Errors**:
```c
int retries = 3;
while (retries > 0) {
  ssize_t ret = write(fd, data, size);
  if (ret == size)
    return 0;  // Success
  
  if (errno == EINTR || errno == EAGAIN) {
    // Temporary error, retry
    retries--;
    usleep(100000);  // Wait 100ms
    continue;
  }
  
  // Permanent error
  return -errno;
}
return -ETIMEDOUT;
```

2. **Disk Space Monitoring**:
```c
// Check available space before writing
struct statvfs vfs;
if (fstatvfs(fd, &vfs) == 0) {
  uint64_t available = vfs.f_bavail * vfs.f_frsize;
  uint64_t required = estimated_recording_size;
  
  if (available < required + MIN_FREE_SPACE) {
    tvhwarn(LS_MUXER, "Low disk space: %"PRIu64" MB available", 
            available / 1024 / 1024);
    // Optionally stop recording
  }
}
```

3. **Graceful Degradation**:
```c
// On write error, try to finalize file gracefully
if (write_error) {
  tvherror(LS_MUXER, "Write error, attempting graceful close");
  
  // Mark end of stream
  m->m_eos = 1;
  
  // Write trailer if possible
  if (m->m_write_trailer)
    m->m_write_trailer(m);
  
  // Close file
  close(m->m_fd);
  
  // Notify user
  notify_recording_error(m, "Disk write error");
}
```

#### 9.5.6 Performance Optimization

**Write Coalescing**:

Combine small writes into larger ones:

```c
// Bad: Many small writes
for (int i = 0; i < 1000; i++) {
  write(fd, &packet[i], sizeof(packet[i]));  // 1000 system calls
}

// Good: Batch writes
uint8_t buffer[1000 * sizeof(packet)];
for (int i = 0; i < 1000; i++) {
  memcpy(buffer + i * sizeof(packet), &packet[i], sizeof(packet[i]));
}
write(fd, buffer, sizeof(buffer));  // 1 system call
```

**Alignment**:

Align writes to block boundaries for better performance:

```c
#define BLOCK_SIZE 4096

// Align buffer to block size
void *buffer = aligned_alloc(BLOCK_SIZE, buffer_size);

// Pad writes to block size
size_t padded_size = (size + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
```

**Asynchronous I/O** (Optional):

Use `aio_write()` for non-blocking writes:

```c
struct aiocb aio;
memset(&aio, 0, sizeof(aio));
aio.aio_fildes = fd;
aio.aio_buf = data;
aio.aio_nbytes = size;
aio.aio_offset = offset;

// Start async write
if (aio_write(&aio) < 0)
  return -errno;

// Continue processing...

// Wait for completion later
while (aio_error(&aio) == EINPROGRESS)
  usleep(1000);

ssize_t ret = aio_return(&aio);
```

**Direct I/O** (Advanced):

Bypass page cache for very large files:

```c
// Open with O_DIRECT
int fd = open(filename, O_WRONLY | O_CREAT | O_DIRECT, 0600);

// Requires aligned buffers and sizes
void *buffer = aligned_alloc(512, size);
write(fd, buffer, size);
```

**Benefits**:
- Reduces memory pressure
- Predictable performance
- Good for very large recordings

**Drawbacks**:
- More complex (alignment requirements)
- May be slower for small writes
- Not supported on all filesystems

#### 9.5.7 Chunked Output

For network streaming, output can be written in fixed-size chunks:

**Configuration**:
```c
muxer_config_t cfg = {
  .m_type = MC_MPEGTS,
  .m_output_chunk = 188 * 7  // 7 TS packets (1316 bytes)
};
```

**Implementation**:
```c
int muxer_write_chunked(muxer_t *m, const void *data, size_t size)
{
  size_t chunk_size = m->m_config.m_output_chunk;
  size_t offset = 0;
  
  while (offset < size) {
    size_t to_write = MIN(chunk_size, size - offset);
    
    ssize_t ret = write(m->m_fd, (uint8_t *)data + offset, to_write);
    if (ret != to_write)
      return -errno;
    
    offset += to_write;
    
    // Optional: Add delay between chunks for rate limiting
    if (m->m_rate_limit)
      usleep(m->m_rate_limit);
  }
  
  return 0;
}
```

**Use Cases**:
- HTTP streaming (control packet sizes)
- Rate-limited streaming
- Network congestion control
- UDP packet size limits


### 9.6 Format Selection Logic

Choosing the appropriate container format is critical for compatibility, quality, and performance. This section documents how Tvheadend selects formats and provides guidelines for different use cases.

#### 9.6.1 Automatic Format Selection

**Selection Factors**:

1. **Profile Configuration**: Profile specifies preferred container type
2. **Codec Compatibility**: Container must support source codecs
3. **Client Capabilities**: User-Agent hints indicate client preferences
4. **Use Case**: Recording vs streaming, live vs on-demand
5. **Feature Requirements**: Metadata, chapters, multi-track support

**Selection Algorithm**:
```c
muxer_container_type_t select_container_type(profile_t *profile,
                                             streaming_start_t *ss,
                                             muxer_hints_t *hints)
{
  // 1. Use profile's configured type if specified
  if (profile->pro_container_type != MC_UNKNOWN)
    return profile->pro_container_type;
  
  // 2. Check client hints
  if (hints && hints->mh_agent) {
    if (strstr(hints->mh_agent, "VLC"))
      return MC_MATROSKA;  // VLC prefers MKV
    if (strstr(hints->mh_agent, "iPhone") || strstr(hints->mh_agent, "iPad"))
      return MC_AVMP4;     // iOS prefers MP4
    if (strstr(hints->mh_agent, "Android"))
      return MC_AVMP4;     // Android prefers MP4
  }
  
  // 3. Check if video is present
  int has_video = 0;
  for (int i = 0; i < ss->ss_num_components; i++) {
    if (ss->ss_components[i].ssc_type == SCT_VIDEO) {
      has_video = 1;
      break;
    }
  }
  
  // 4. Audio-only: use audio elementary stream format
  if (!has_video) {
    // Detect audio codec
    for (int i = 0; i < ss->ss_num_components; i++) {
      if (ss->ss_components[i].ssc_type == SCT_AUDIO) {
        switch (ss->ss_components[i].ssc_codec) {
          case CODEC_AAC:
            return MC_AAC;
          case CODEC_MP3:
            return MC_MPEG2AUDIO;
          case CODEC_AC3:
            return MC_AC3;
          case CODEC_VORBIS:
            return MC_VORBIS;
          default:
            break;
        }
      }
    }
  }
  
  // 5. Default: Matroska for recordings, MPEG-TS for streaming
  if (profile->pro_recording)
    return MC_MATROSKA;
  else
    return MC_MPEGTS;
}
```

#### 9.6.2 User Override Options

Users can override automatic selection through configuration:

**Profile Configuration**:
```c
typedef struct profile {
  // ...
  muxer_container_type_t pro_container_type;  // Preferred container
  int                    pro_container_auto;  // Auto-select if 1
} profile_t;
```

**DVR Configuration**:
```c
typedef struct dvr_config {
  // ...
  muxer_container_type_t dvr_container;       // Recording container
  char                  *dvr_container_ext;   // File extension override
} dvr_config_t;
```

**Web UI Options**:
- Profile editor: Container format dropdown
- DVR configuration: Recording format selection
- Per-recording override: Format selection in recording dialog

**Command-Line Override**:
```bash
# Force Matroska for all recordings
tvheadend --dvr-container matroska

# Force MPEG-TS for streaming
tvheadend --stream-container mpegts
```

#### 9.6.3 Codec Compatibility Matrix

Not all codecs are supported by all containers. The muxer system checks compatibility:

**Compatibility Check**:
```c
int muxer_check_codec_compatibility(muxer_container_type_t container,
                                   int codec_id)
{
  switch (container) {
    case MC_MATROSKA:
    case MC_WEBM:
      // Matroska supports virtually all codecs
      return 1;
      
    case MC_MPEGTS:
      // MPEG-TS supports most broadcast codecs
      switch (codec_id) {
        case CODEC_H264:
        case CODEC_HEVC:
        case CODEC_MPEG2VIDEO:
        case CODEC_AAC:
        case CODEC_MP3:
        case CODEC_AC3:
        case CODEC_EAC3:
          return 1;
        default:
          return 0;
      }
      
    case MC_AVMP4:
      // MP4 supports limited codecs
      switch (codec_id) {
        case CODEC_H264:
        case CODEC_HEVC:
        case CODEC_AAC:
        case CODEC_MP3:
        case CODEC_AC3:
          return 1;
        default:
          return 0;
      }
      
    default:
      return 0;
  }
}
```

**Codec Support Matrix**:

| Codec | Matroska | MPEG-TS | MP4 | Pass | Audio ES |
|-------|----------|---------|-----|------|----------|
| **Video** |
| H.264 | ✓ | ✓ | ✓ | ✓ | - |
| HEVC | ✓ | ✓ | ✓ | ✓ | - |
| MPEG-2 | ✓ | ✓ | - | ✓ | - |
| VP8 | ✓ | - | - | ✓ | - |
| VP9 | ✓ | - | - | ✓ | - |
| AV1 | ✓ | - | - | ✓ | - |
| **Audio** |
| AAC | ✓ | ✓ | ✓ | ✓ | ✓ |
| MP3 | ✓ | ✓ | ✓ | ✓ | ✓ |
| AC3 | ✓ | ✓ | ✓ | ✓ | ✓ |
| EAC3 | ✓ | ✓ | ✓ | ✓ | - |
| Opus | ✓ | - | - | ✓ | - |
| Vorbis | ✓ | - | - | ✓ | ✓ |
| FLAC | ✓ | - | - | ✓ | - |
| **Subtitles** |
| DVB | ✓ | ✓ | - | ✓ | - |
| Teletext | ✓ | ✓ | - | ✓ | - |
| SRT | ✓ | - | ✓ | - | - |
| ASS/SSA | ✓ | - | - | - | - |

**Fallback Strategy**:
```c
// If preferred container doesn't support codec, fall back
muxer_container_type_t container = preferred_container;

if (!muxer_check_codec_compatibility(container, video_codec)) {
  tvhwarn(LS_MUXER, "Container %s doesn't support codec %s, falling back to Matroska",
          muxer_container_type2txt(container),
          codec_id2str(video_codec));
  container = MC_MATROSKA;
}
```

#### 9.6.4 Use Case Guidelines

**For Archival Recordings**:
```c
muxer_config_t cfg = {
  .m_type = MC_MATROSKA,
  .m_cache = MC_CACHE_SYNC,
  .u.mkv.m_dvbsub_reorder = 1
};
```
- **Format**: Matroska (MKV)
- **Reason**: Best metadata support, all codecs, chapter markers
- **Cache**: Sync for data safety
- **Features**: Subtitle reordering, metadata embedding

**For Live Streaming**:
```c
muxer_config_t cfg = {
  .m_type = MC_MPEGTS,
  .m_cache = MC_CACHE_SYSTEM,
  .m_output_chunk = 188 * 7,
  .u.pass.m_rewrite_pat = 1,
  .u.pass.m_rewrite_pmt = 1
};
```
- **Format**: MPEG-TS
- **Reason**: Streamable, low latency, hardware support
- **Cache**: System default for performance
- **Features**: PSI/SI rewriting, chunked output

**For Mobile Devices**:
```c
muxer_config_t cfg = {
  .m_type = MC_AVMP4,
  .m_cache = MC_CACHE_SYSTEM
};
```
- **Format**: MP4
- **Reason**: Universal mobile support, progressive download
- **Cache**: System default
- **Features**: iTunes-compatible metadata

**For Radio/Audio-Only**:
```c
muxer_config_t cfg = {
  .m_type = MC_AAC,  // or MC_MP3, MC_VORBIS
  .m_cache = MC_CACHE_SYSTEM,
  .u.audioes.m_index = 0
};
```
- **Format**: Audio elementary stream
- **Reason**: Minimal overhead, small file size
- **Cache**: System default
- **Features**: Single audio track extraction

**For Maximum Compatibility**:
```c
muxer_config_t cfg = {
  .m_type = MC_AVMP4,
  .m_cache = MC_CACHE_SYSTEM
};
// Ensure H.264 + AAC codecs via transcoding profile
```
- **Format**: MP4 with H.264 + AAC
- **Reason**: Plays on virtually all devices
- **Cache**: System default
- **Features**: Standard codecs, good seeking

**For Custom Processing**:
```c
muxer_config_t cfg = {
  .m_type = MC_PASS,
  .m_cache = MC_CACHE_SYSTEM,
  .u.pass.m_cmdline = "ffmpeg -i pipe:0 -c copy -f matroska pipe:1",
  .u.pass.m_mime = "video/x-matroska"
};
```
- **Format**: Pass-through with external command
- **Reason**: Maximum flexibility
- **Cache**: System default
- **Features**: Custom processing via ffmpeg

#### 9.6.5 Format Migration

When changing container formats, consider:

**Remuxing Existing Recordings**:
```bash
# Convert MKV to MP4
ffmpeg -i recording.mkv -c copy recording.mp4

# Convert TS to MKV
ffmpeg -i recording.ts -c copy recording.mkv

# Extract audio only
ffmpeg -i recording.mkv -vn -c:a copy recording.aac
```

**Batch Conversion**:
```bash
# Convert all MKV files to MP4
for f in *.mkv; do
  ffmpeg -i "$f" -c copy "${f%.mkv}.mp4"
done
```

**Metadata Preservation**:
```bash
# Preserve metadata during conversion
ffmpeg -i input.mkv -c copy -map_metadata 0 output.mp4
```

**Considerations**:
- Remuxing is fast (no re-encoding)
- Some metadata may be lost (format-specific)
- Subtitles may need conversion
- Test with sample files first

#### 9.6.6 Format Selection Decision Tree

```
Start
  |
  ├─ Is it audio-only?
  |    ├─ Yes → Use audio ES format (AAC, MP3, etc.)
  |    └─ No → Continue
  |
  ├─ Is it for recording?
  |    ├─ Yes → Is metadata important?
  |    |         ├─ Yes → Use Matroska (MKV)
  |    |         └─ No → Is mobile compatibility needed?
  |    |                  ├─ Yes → Use MP4
  |    |                  └─ No → Use Matroska (MKV)
  |    |
  |    └─ No (streaming) → Is it live streaming?
  |                        ├─ Yes → Use MPEG-TS
  |                        └─ No → Is client mobile?
  |                                 ├─ Yes → Use MP4
  |                                 └─ No → Use MPEG-TS or MKV
  |
  └─ Are all codecs supported by chosen format?
       ├─ Yes → Use chosen format
       └─ No → Fall back to Matroska (MKV)
```

**Summary**:
- **Default for recordings**: Matroska (MKV)
- **Default for streaming**: MPEG-TS
- **Mobile devices**: MP4
- **Audio-only**: Audio ES formats
- **Maximum compatibility**: MP4 with H.264 + AAC
- **Maximum flexibility**: Pass-through with external command
- **Fallback**: Matroska (supports all codecs)

---

[← Previous](08-Profile-System.md) | [Table of Contents](00-TOC.md) | [Next →](10-Subscription-System.md)

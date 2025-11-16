[← Back to Table of Contents](00-TOC.md)

## 8. Profile System and Stream Processing

The Profile System is responsible for configuring how streams are processed and delivered to clients. It provides a flexible framework for defining stream transformations, including pass-through streaming, transcoding, format conversion, and stream filtering. Profiles control quality settings, codec selection, container formats, and resource allocation, enabling Tvheadend to adapt streams for different client capabilities and network conditions. This section documents the profile architecture, profile chains, transcoding pipeline, stream filtering, and performance considerations.

### 8.1 Profile Architecture

The profile system uses an object-oriented design that allows different profile types to implement specific streaming behaviors while sharing common configuration and management infrastructure.

#### 8.1.1 The profile_t Structure

**Location**: `src/profile.h`, `src/profile.c`

The `profile_t` structure is the base class for all streaming profiles. It inherits from `idnode_t` for configuration management and provides virtual methods for profile-specific behavior.

**Structure Definition**:
```c
typedef struct profile {
  idnode_t pro_id;                      // Configuration node (UUID, properties)
  TAILQ_ENTRY(profile) pro_link;        // Link in global profile list

  int pro_refcount;                     // Reference count

  LIST_HEAD(,dvr_config) pro_dvr_configs;  // DVR configs using this profile
  idnode_list_head_t pro_accesses;      // Access entries using this profile

  int pro_sflags;                       // Service flags
  int pro_enabled;                      // Profile enabled flag
  int pro_shield;                       // Protected from deletion
  char *pro_name;                       // Profile name
  char *pro_comment;                    // User comment
  int pro_prio;                         // Default priority
  int pro_fprio;                        // Force priority
  int pro_timeout;                      // Data timeout (seconds)
  int pro_timeout_start;                // Start timeout (seconds)
  int pro_restart;                      // Restart on error flag
  int pro_contaccess;                   // Continue if descrambling fails
  int pro_ca_timeout;                   // Descrambling timeout (ms)
  int pro_swservice;                    // Switch to another service on error
  int pro_svfilter;                     // Preferred service video type

  // Virtual methods
  void (*pro_free)(struct profile *pro);
  void (*pro_conf_changed)(struct profile *pro);

  int (*pro_work)(profile_chain_t *prch, struct streaming_target *dst,
                  uint32_t timeshift_period, profile_work_flags_t flags);
  int (*pro_reopen)(profile_chain_t *prch, muxer_config_t *m_cfg,
                    muxer_hints_t *hints, int flags);
  int (*pro_open)(profile_chain_t *prch, muxer_config_t *m_cfg,
                  muxer_hints_t *hints, int flags, size_t qsize);
} profile_t;
```

**Key Fields**:

1. **Configuration and Identity**:
   - `pro_id`: Inherits from `idnode_t`, providing UUID, property system, and persistence
   - `pro_name`: User-visible profile name
   - `pro_comment`: Free-form description
   - `pro_enabled`: Whether profile is available for use
   - `pro_shield`: Protected profiles cannot be deleted (e.g., built-in profiles)

2. **Reference Counting**:
   - `pro_refcount`: Tracks active uses of the profile
   - Incremented via `profile_grab()`
   - Decremented via `profile_release()`
   - Profile freed when refcount reaches zero

3. **Relationships**:
   - `pro_dvr_configs`: List of DVR configurations using this profile
   - `pro_accesses`: List of access control entries referencing this profile

4. **Priority and Scheduling**:
   - `pro_prio`: Default priority for subscriptions using this profile
   - `pro_fprio`: If non-zero, forces this priority regardless of subscription request
   - Priority values range from `PROFILE_SPRIO_UNIMPORTANT` to `PROFILE_SPRIO_IMPORTANT`

5. **Timeout Configuration**:
   - `pro_timeout`: Maximum seconds to wait for data (0 = infinite)
   - `pro_timeout_start`: Timeout specifically for stream startup
   - `pro_ca_timeout`: Milliseconds to wait for descrambler response

6. **Error Handling**:
   - `pro_restart`: If true, restart streaming on errors (useful for DVR)
   - `pro_contaccess`: Continue streaming even if descrambling fails
   - `pro_swservice`: Try alternate services on failure

7. **Service Selection**:
   - `pro_svfilter`: Preferred video type (SD, HD, FHD, UHD)
   - Used when multiple services are available for a channel

8. **Virtual Methods**:
   - `pro_free()`: Profile-specific cleanup
   - `pro_conf_changed()`: Called when configuration changes
   - `pro_work()`: Setup streaming chain for this profile
   - `pro_reopen()`: Reopen streaming chain (e.g., after PMT change)
   - `pro_open()`: Open streaming chain with muxer

**Priority Levels**:
```c
typedef enum {
  PROFILE_SPRIO_NOTSET = 0,           // Use default
  PROFILE_SPRIO_IMPORTANT,            // Highest priority
  PROFILE_SPRIO_HIGH,
  PROFILE_SPRIO_NORMAL,               // Default
  PROFILE_SPRIO_LOW,
  PROFILE_SPRIO_UNIMPORTANT,          // Lowest priority
  PROFILE_SPRIO_DVR_IMPORTANT,        // DVR-specific priorities
  PROFILE_SPRIO_DVR_HIGH,
  PROFILE_SPRIO_DVR_NORMAL,
  PROFILE_SPRIO_DVR_LOW,
  PROFILE_SPRIO_DVR_UNIMPORTANT
} profile_sprio_t;
```

**Service Video Filter**:
```c
typedef enum {
  PROFILE_SVF_NONE = 0,               // No preference
  PROFILE_SVF_SD,                     // Prefer SD (≤576p)
  PROFILE_SVF_HD,                     // Prefer HD (720p/1080i)
  PROFILE_SVF_FHD,                    // Prefer Full HD (1080p)
  PROFILE_SVF_UHD                     // Prefer Ultra HD (4K+)
} profile_svfilter_t;
```

#### 8.1.2 Profile Types

Tvheadend supports several profile types, each implementing different streaming behaviors:

**1. Pass-Through Profile** (`profile_mpegts_pass_class`)

**Purpose**: Streams content without modification, passing through the original MPEG-TS stream.

**Characteristics**:
- Minimal CPU usage (no transcoding)
- Lowest latency
- Preserves original quality
- Supports all codecs present in source
- Can rewrite PSI/SI tables (PAT, PMT, SDT, NIT, EIT)
- Optional external command execution (e.g., pipe to ffmpeg)

**Use Cases**:
- Streaming to clients that support native formats
- Recording original broadcast quality
- Minimal server load scenarios
- Network bandwidth is not a constraint

**Configuration Options**:
- Rewrite service ID
- Rewrite PSI/SI tables
- External command for post-processing
- MIME type override

**2. Matroska Profile** (`profile_matroska_class`)

**Purpose**: Remuxes stream into Matroska (MKV) container format.

**Characteristics**:
- Moderate CPU usage (remuxing only, no transcoding)
- Supports multiple audio/subtitle tracks
- Embeds metadata (EPG information, artwork)
- Better seeking support than MPEG-TS
- Widely supported by media players

**Use Cases**:
- Recording for archival
- Streaming to clients preferring MKV
- When metadata embedding is desired
- Multi-track audio/subtitle support needed

**Configuration Options**:
- DVB subtitle reordering
- Metadata embedding options
- Track selection

**3. Transcoding Profile** (when `ENABLE_LIBAV` is defined)

**Purpose**: Transcodes streams to different codecs and bitrates.

**Characteristics**:
- High CPU usage (encoding/decoding)
- Reduces bandwidth requirements
- Adapts quality for client capabilities
- Can resize video resolution
- Supports hardware acceleration (VAAPI, NVENC, QSV)

**Use Cases**:
- Streaming over limited bandwidth
- Adapting for mobile devices
- Converting incompatible codecs
- Reducing storage requirements for recordings

**Configuration Options**:
- Video codec (H.264, HEVC, VP8, VP9)
- Audio codec (AAC, MP3, Opus, Vorbis)
- Video resolution and bitrate
- Audio bitrate and sample rate
- Hardware acceleration method
- Quality presets (fast, medium, slow)

**4. Audio-Only Profiles**

**Purpose**: Extracts and streams only audio tracks.

**Characteristics**:
- Very low CPU usage
- Minimal bandwidth
- Supports various audio formats (MP3, AAC, AC3, etc.)

**Use Cases**:
- Radio streaming
- Audio-only clients
- Bandwidth-constrained scenarios

#### 8.1.3 Profile Selection Logic

Profiles are selected based on several factors:

**Selection Process**:

1. **User/Access Control Specification**:
   - User account may specify a default profile
   - Access control entry may restrict available profiles
   - Subscription request may explicitly specify a profile

2. **DVR Configuration**:
   - DVR entries use profile specified in DVR configuration
   - Allows different recording quality than live streaming

3. **Service Flags Matching**:
   - Profile's `pro_sflags` must match subscription requirements
   - Ensures profile is suitable for the requested operation

4. **Enabled Check**:
   - Only enabled profiles (`pro_enabled == 1`) are considered
   - Allows temporarily disabling profiles without deletion

5. **Default Fallback**:
   - If no profile specified or found, use system default profile
   - Default profile is marked with special flag

**Profile Lookup Functions**:

```c
// Find profile by UUID
profile_t *profile_find_by_uuid(const char *uuid);

// Find profile by name
profile_t *profile_find_by_name(const char *name, const char *alt);

// Find profile from list of UUIDs or names
profile_t *profile_find_by_list(htsmsg_t *uuids, const char *name,
                                const char *alt, int sflags);

// Verify profile is suitable for service flags
int profile_verify(profile_t *pro, int sflags);
```

**Example Selection**:
```c
// Subscription specifies profile by name
profile_t *pro = profile_find_by_name("HD Streaming", NULL);

// DVR uses profile from configuration
profile_t *pro = dvr_config->dvr_profile;

// Access control restricts to specific profiles
profile_t *pro = profile_find_by_list(access->aa_profiles, 
                                      "Mobile", "SD Streaming", 
                                      sflags);
```

#### 8.1.4 Profile Configuration

Profiles are configured through the web UI or API and stored as idnode objects.

**Common Configuration Properties**:

| Property | Type | Description | Default |
|----------|------|-------------|---------|
| `name` | String | Profile name | (required) |
| `enabled` | Boolean | Enable/disable profile | true |
| `comment` | String | User description | "" |
| `timeout` | Integer | Data timeout (seconds) | 5 |
| `priority` | Enum | Default priority | Normal |
| `restart` | Boolean | Restart on error | false |
| `contaccess` | Boolean | Continue if descrambling fails | true |
| `catimeout` | Integer | Descrambling timeout (ms) | 2000 |
| `swservice` | Boolean | Switch service on error | true |
| `svfilter` | Enum | Preferred video type | None |

**Pass-Through Specific**:

| Property | Type | Description |
|----------|------|-------------|
| `rewrite_sid` | Boolean | Rewrite service ID |
| `rewrite_pat` | Boolean | Rewrite PAT table |
| `rewrite_pmt` | Boolean | Rewrite PMT table |
| `rewrite_sdt` | Boolean | Rewrite SDT table |
| `rewrite_nit` | Boolean | Rewrite NIT table |
| `rewrite_eit` | Boolean | Rewrite EIT table |

**Matroska Specific**:

| Property | Type | Description |
|----------|------|-------------|
| `dvbsub_reorder` | Boolean | Reorder DVB subtitles |

**Transcoding Specific** (when available):

| Property | Type | Description |
|----------|------|-------------|
| `vcodec` | Enum | Video codec |
| `acodec` | Enum | Audio codec |
| `resolution` | Enum | Video resolution |
| `vbitrate` | Integer | Video bitrate (kbps) |
| `abitrate` | Integer | Audio bitrate (kbps) |
| `hwaccel` | Enum | Hardware acceleration |

**Configuration Storage**:
- Profiles stored in `~/.hts/tvheadend/profile/<uuid>`
- JSON format with all properties
- Automatically loaded at startup
- Changes saved immediately via idnode system

**Built-in Profiles**:
- Tvheadend includes several built-in profiles
- Built-in profiles are "shielded" (cannot be deleted)
- Can be disabled but not removed
- Serve as templates for custom profiles

### 8.2 Profile Chains

A profile chain (`profile_chain_t`) represents an active streaming pipeline that processes data from a service through various transformation stages before delivering it to a client or recording. Profile chains are the runtime instantiation of profiles, managing the actual data flow and resource allocation.

#### 8.2.1 The profile_chain_t Structure

**Location**: `src/profile.h`

**Structure Definition**:
```c
typedef struct profile_chain {
  LIST_ENTRY(profile_chain) prch_link;        // Link in global chain list
  int                       prch_linked;      // Linked flag

  struct profile_sharer    *prch_sharer;      // Shared transcoder (if any)
  LIST_ENTRY(profile_chain) prch_sharer_link; // Link in sharer's chain list

  struct profile           *prch_pro;         // Profile being used
  void                     *prch_id;          // Chain identifier (subscription, DVR entry)

  int64_t                   prch_ts_delta;    // Timestamp delta for synchronization

  int                       prch_flags;       // Chain flags
  int                       prch_stop;        // Stop flag
  int                       prch_start_pending; // Start pending flag
  int                       prch_sq_used;     // Streaming queue in use
  struct streaming_queue    prch_sq;          // Streaming queue
  struct streaming_target  *prch_post_share;  // Post-share target
  struct streaming_target  *prch_st;          // Final streaming target
  struct muxer             *prch_muxer;       // Muxer (if recording)
  struct streaming_target  *prch_gh;          // Global headers target
  struct streaming_target  *prch_tsfix;       // TS fix target
#if ENABLE_TIMESHIFT
  struct streaming_target  *prch_timeshift;   // Timeshift target
  struct streaming_target  *prch_rtsp;        // RTSP target
#endif
  struct streaming_target   prch_input;       // Input target (receives from service)
  struct streaming_target  *prch_share;       // Share target (for transcoding)

  int (*prch_can_share)(struct profile_chain *prch,
                        struct profile_chain *joiner);  // Can share check
} profile_chain_t;
```

**Key Components**:

1. **Chain Identity**:
   - `prch_pro`: The profile this chain is using
   - `prch_id`: Identifier (typically subscription or DVR entry pointer)
   - `prch_link`: Links chain into global list for management

2. **Streaming Targets** (Pipeline Stages):
   - `prch_input`: Entry point, receives data from service
   - `prch_tsfix`: Fixes MPEG-TS issues (PCR, continuity counters)
   - `prch_gh`: Adds global headers (codec extradata)
   - `prch_timeshift`: Timeshift buffer (if enabled)
   - `prch_share`: Transcoder sharing (if applicable)
   - `prch_post_share`: Post-transcoding processing
   - `prch_st`: Final output target (to client or muxer)

3. **Muxer Integration**:
   - `prch_muxer`: Muxer instance for file writing (DVR)
   - Converts streaming messages to file format
   - Handles metadata embedding

4. **Streaming Queue**:
   - `prch_sq`: Queue for buffering messages
   - `prch_sq_used`: Whether queue is active
   - Provides flow control and buffering

5. **Transcoder Sharing**:
   - `prch_sharer`: Shared transcoder instance
   - `prch_sharer_link`: Links into sharer's chain list
   - `prch_can_share()`: Function to check if chains can share transcoder
   - Allows multiple clients to use same transcoded stream

6. **State Management**:
   - `prch_flags`: Various state flags
   - `prch_stop`: Chain is stopping
   - `prch_start_pending`: Start operation pending
   - `prch_ts_delta`: Timestamp adjustment for synchronization

#### 8.2.2 Chain Building Process

Profile chains are built dynamically when a subscription or recording starts. The building process creates a pipeline of streaming targets that transform the data according to the profile configuration.

**Chain Building Steps**:

1. **Initialization**:
   ```c
   void profile_chain_init(profile_chain_t *prch, profile_t *pro, 
                          void *id, int queue)
   {
     memset(prch, 0, sizeof(*prch));
     prch->prch_pro = pro;
     prch->prch_id = id;
     profile_grab(pro);  // Increment profile reference count
     
     if (queue) {
       streaming_queue_init(&prch->prch_sq, 0, 0);
       prch->prch_sq_used = 1;
     }
     
     LIST_INSERT_HEAD(&profile_chains, prch, prch_link);
     prch->prch_linked = 1;
   }
   ```

2. **Open Chain** (via `pro_open()` virtual method):
   - Calls profile-specific open function
   - Creates necessary streaming targets
   - Connects targets in pipeline order
   - Initializes muxer if recording

3. **Pipeline Construction**:
   ```
   Service → prch_input → [tsfix] → [gh] → [timeshift] → [transcoder] → 
             [post_share] → prch_st → Client/Muxer
   ```

4. **Component Creation**:
   - **TS Fix**: Created if source is MPEG-TS and needs correction
   - **Global Headers**: Created if codec requires extradata
   - **Timeshift**: Created if timeshift is enabled for subscription
   - **Transcoder**: Created if profile requires transcoding
   - **Muxer**: Created if recording to file

**Example Chain Building** (Pass-Through Profile):
```c
int profile_mpegts_pass_open(profile_chain_t *prch, muxer_config_t *m_cfg,
                             muxer_hints_t *hints, int flags, size_t qsize)
{
  streaming_target_t *dst = NULL;
  
  // Create muxer if recording
  if (m_cfg) {
    prch->prch_muxer = muxer_create(m_cfg, hints);
    dst = muxer_get_streaming_target(prch->prch_muxer);
  } else {
    dst = prch->prch_st;  // Direct to client
  }
  
  // Create TS fix if needed
  if (needs_tsfix) {
    prch->prch_tsfix = tsfix_create(dst);
    dst = prch->prch_tsfix;
  }
  
  // Create global headers if needed
  if (needs_gh) {
    prch->prch_gh = globalheaders_create(dst);
    dst = prch->prch_gh;
  }
  
  // Connect input to first stage
  streaming_target_init(&prch->prch_input, input_callback, prch, 0);
  
  return 0;
}
```

#### 8.2.3 Chain Execution

Once built, the profile chain processes streaming messages from the service through each stage of the pipeline.

**Data Flow**:

1. **Service Delivers Message**:
   - Service calls `streaming_pad_deliver()`
   - Message delivered to all connected targets
   - Profile chain's `prch_input` receives message

2. **Input Target Callback**:
   ```c
   static void profile_chain_input(void *opaque, streaming_message_t *sm)
   {
     profile_chain_t *prch = opaque;
     
     // Check for stop condition
     if (prch->prch_stop) {
       streaming_msg_free(sm);
       return;
     }
     
     // Queue message if queue is enabled
     if (prch->prch_sq_used) {
       streaming_queue_put(&prch->prch_sq, sm);
       return;
     }
     
     // Pass to next stage
     if (prch->prch_tsfix)
       streaming_target_deliver(prch->prch_tsfix, sm);
     else if (prch->prch_gh)
       streaming_target_deliver(prch->prch_gh, sm);
     else
       streaming_target_deliver(prch->prch_st, sm);
   }
   ```

3. **Pipeline Processing**:
   - Each target processes the message
   - May transform, filter, or pass through unchanged
   - Delivers to next target in chain

4. **Final Delivery**:
   - Last target delivers to client or muxer
   - Client receives via HTSP/HTTP
   - Muxer writes to file

**Message Types Handled**:
- `SMT_START`: Initialize pipeline, configure codecs
- `SMT_PACKET`: Process media packets
- `SMT_STOP`: Cleanup pipeline, close resources
- `SMT_GRACE`: Handle grace period
- `SMT_SERVICE_STATUS`: Update status
- `SMT_SIGNAL_STATUS`: Update signal quality
- `SMT_SPEED`: Handle speed changes (timeshift)
- `SMT_SKIP`: Handle seek operations (timeshift)

#### 8.2.4 Chain Sharing (Transcoder Optimization)

To reduce CPU usage, multiple profile chains can share a single transcoder instance when they use the same transcoding settings.

**Profile Sharer Structure**:
```c
typedef struct profile_sharer {
  uint32_t                  prsh_do_queue: 1;      // Queue enabled
  uint32_t                  prsh_queue_run: 1;     // Queue running
  pthread_t                 prsh_queue_thread;     // Queue thread
  tvh_mutex_t               prsh_queue_mutex;      // Queue mutex
  tvh_cond_t                prsh_queue_cond;       // Queue condition
  TAILQ_HEAD(,profile_sharer_message) prsh_queue; // Message queue
  streaming_target_t        prsh_input;            // Input from service
  LIST_HEAD(,profile_chain) prsh_chains;           // Chains sharing this transcoder
  struct profile_chain     *prsh_master;           // Master chain
  struct streaming_start   *prsh_start_msg;        // Stream start message
  struct streaming_target  *prsh_tsfix;            // TS fix
#if ENABLE_LIBAV
  struct streaming_target  *prsh_transcoder;       // Transcoder instance
#endif
} profile_sharer_t;
```

**Sharing Process**:

1. **First Chain** (Master):
   - Creates transcoder instance
   - Creates profile_sharer structure
   - Becomes master chain

2. **Additional Chains** (Joiners):
   - Check if can share via `prch_can_share()`
   - Connect to existing sharer
   - Receive transcoded output

3. **Sharing Criteria**:
   - Same transcoding profile
   - Same source service
   - Compatible settings (resolution, bitrate, codec)

4. **Benefits**:
   - Reduced CPU usage (one transcode for multiple clients)
   - Lower memory usage
   - Improved scalability

5. **Limitations**:
   - All clients receive same transcoded stream
   - Cannot have per-client customization
   - Master chain controls transcoder lifecycle

**Example Sharing Check**:
```c
int profile_chain_can_share(profile_chain_t *prch, profile_chain_t *joiner)
{
  // Must use same profile
  if (prch->prch_pro != joiner->prch_pro)
    return 0;
  
  // Must have same source
  if (get_service(prch) != get_service(joiner))
    return 0;
  
  // Profile-specific checks
  if (prch->prch_pro->pro_can_share)
    return prch->prch_pro->pro_can_share(prch, joiner);
  
  return 1;
}
```

#### 8.2.5 Chain Lifecycle

**Creation**:
```c
profile_chain_t *prch = malloc(sizeof(profile_chain_t));
profile_chain_init(prch, profile, subscription, 1);  // 1 = use queue
profile_chain_open(prch, muxer_cfg, hints, flags, queue_size);
```

**Operation**:
- Chain receives messages from service
- Processes through pipeline
- Delivers to client or file

**Reconfiguration**:
```c
// Called when PMT changes or stream needs restart
profile_chain_reopen(prch, new_muxer_cfg, hints, flags);
```

**Destruction**:
```c
void profile_chain_close(profile_chain_t *prch)
{
  prch->prch_stop = 1;
  
  // Close muxer
  if (prch->prch_muxer) {
    muxer_close(prch->prch_muxer);
    muxer_destroy(prch->prch_muxer);
  }
  
  // Destroy streaming targets
  if (prch->prch_tsfix)
    tsfix_destroy(prch->prch_tsfix);
  if (prch->prch_gh)
    globalheaders_destroy(prch->prch_gh);
  if (prch->prch_timeshift)
    timeshift_destroy(prch->prch_timeshift);
  
  // Cleanup queue
  if (prch->prch_sq_used)
    streaming_queue_clear(&prch->prch_sq);
  
  // Unlink from global list
  if (prch->prch_linked) {
    LIST_REMOVE(prch, prch_link);
    prch->prch_linked = 0;
  }
  
  // Release profile
  profile_release(prch->prch_pro);
  
  free(prch);
}
```

**Chain Weight Calculation**:
```c
// Calculate resource weight for scheduling
int profile_chain_weight(profile_chain_t *prch, int custom)
{
  int weight = 0;
  
  // Base weight from profile priority
  weight += profile_priority_to_weight(prch->prch_pro->pro_prio);
  
  // Add custom weight (from subscription)
  weight += custom;
  
  // Transcoding adds significant weight
  if (prch->prch_sharer && prch->prch_sharer->prsh_transcoder)
    weight += 1000;
  
  return weight;
}
```

The weight is used for:
- Input resource allocation (tuner selection)
- Priority resolution when resources are limited
- Scheduling decisions


### 8.3 Transcoding Pipeline

The transcoding pipeline (available when compiled with `ENABLE_LIBAV`) provides on-the-fly codec conversion, resolution scaling, and bitrate adaptation using FFmpeg/libav libraries. This section documents the transcoding architecture, codec support, and hardware acceleration.

#### 8.3.1 Transcoding Architecture

**Location**: `src/transcoding/`

The transcoding subsystem is implemented as a streaming target that sits in the profile chain pipeline, receiving packets from the source and delivering transcoded packets to the output.

**High-Level Architecture**:
```
Input Stream → Decoder → Scaler/Filter → Encoder → Output Stream
```

**Transcoder Creation**:
```c
streaming_target_t *transcoder_create(streaming_target_t *output,
                                     const char **profiles,
                                     const char **src_codecs);
```

**Parameters**:
- `output`: Next target in chain (receives transcoded packets)
- `profiles`: Array of profile names for codec selection
- `src_codecs`: Array of source codec names

**Transcoder Structure** (internal):
```c
typedef struct transcoder {
  streaming_target_t tc_input;        // Input target (receives from source)
  streaming_target_t *tc_output;      // Output target (delivers transcoded)
  
  // Decoder contexts (one per input stream)
  AVCodecContext *tc_vdec_ctx;        // Video decoder
  AVCodecContext *tc_adec_ctx;        // Audio decoder
  
  // Encoder contexts (one per output stream)
  AVCodecContext *tc_venc_ctx;        // Video encoder
  AVCodecContext *tc_aenc_ctx;        // Audio encoder
  
  // Filtering/scaling
  AVFilterGraph *tc_vfilter;          // Video filter graph
  AVFilterGraph *tc_afilter;          // Audio filter graph
  
  // Frame buffers
  AVFrame *tc_vframe;                 // Video frame buffer
  AVFrame *tc_aframe;                 // Audio frame buffer
  
  // Packet buffers
  AVPacket *tc_vpkt;                  // Video packet buffer
  AVPacket *tc_apkt;                  // Audio packet buffer
  
  // Hardware acceleration
  AVBufferRef *tc_hw_device_ctx;      // Hardware device context
  AVBufferRef *tc_hw_frames_ctx;      // Hardware frames context
  enum AVHWDeviceType tc_hw_type;     // Hardware type (VAAPI, NVENC, QSV)
  
  // Threading
  pthread_t tc_thread;                // Transcoding thread
  tvh_mutex_t tc_mutex;               // Mutex for state
  tvh_cond_t tc_cond;                 // Condition for queue
  TAILQ_HEAD(,th_pkt) tc_queue;       // Input packet queue
  int tc_running;                     // Running flag
  
  // Statistics
  int64_t tc_frames_in;               // Input frames
  int64_t tc_frames_out;              // Output frames
  int64_t tc_bytes_in;                // Input bytes
  int64_t tc_bytes_out;               // Output bytes
} transcoder_t;
```

#### 8.3.2 libav/FFmpeg Integration

Tvheadend uses FFmpeg's libav libraries for transcoding:

**Required Libraries**:
- `libavcodec`: Codec encoding/decoding
- `libavformat`: Container format handling
- `libavutil`: Utility functions
- `libavfilter`: Audio/video filtering
- `libswscale`: Video scaling
- `libswresample`: Audio resampling

**Initialization**:
```c
void libav_init(void)
{
  // Register all codecs and formats
  av_register_all();
  avcodec_register_all();
  avfilter_register_all();
  
  // Set log level
  av_log_set_level(AV_LOG_WARNING);
  
  // Set log callback
  av_log_set_callback(tvh_libav_log_callback);
}
```

**Decoder Setup**:
```c
// Find decoder for input codec
AVCodec *decoder = avcodec_find_decoder(codec_id);

// Allocate decoder context
AVCodecContext *dec_ctx = avcodec_alloc_context3(decoder);

// Set decoder parameters from input stream
dec_ctx->width = input_width;
dec_ctx->height = input_height;
dec_ctx->pix_fmt = input_pix_fmt;
dec_ctx->time_base = input_time_base;

// Open decoder
avcodec_open2(dec_ctx, decoder, NULL);
```

**Encoder Setup**:
```c
// Find encoder for output codec
AVCodec *encoder = avcodec_find_encoder_by_name(codec_name);

// Allocate encoder context
AVCodecContext *enc_ctx = avcodec_alloc_context3(encoder);

// Set encoder parameters
enc_ctx->width = output_width;
enc_ctx->height = output_height;
enc_ctx->pix_fmt = encoder->pix_fmts[0];  // Use encoder's preferred format
enc_ctx->time_base = (AVRational){1, 90000};
enc_ctx->bit_rate = target_bitrate;
enc_ctx->gop_size = 50;  // Keyframe interval
enc_ctx->max_b_frames = 2;

// Set quality preset
av_opt_set(enc_ctx->priv_data, "preset", "medium", 0);

// Open encoder
avcodec_open2(enc_ctx, encoder, NULL);
```

#### 8.3.3 Codec Support

**Video Codecs**:

| Codec | Decoder | Encoder | Hardware Accel | Notes |
|-------|---------|---------|----------------|-------|
| H.264 (AVC) | ✓ | ✓ | VAAPI, NVENC, QSV | Most common, widely supported |
| HEVC (H.265) | ✓ | ✓ | VAAPI, NVENC, QSV | Better compression, higher CPU |
| MPEG-2 | ✓ | ✓ | VAAPI | Legacy, broadcast standard |
| VP8 | ✓ | ✓ | - | WebM container |
| VP9 | ✓ | ✓ | VAAPI | Better than VP8, high CPU |
| AV1 | ✓ | ✓ | - | Next-gen, very high CPU |
| MPEG-4 Part 2 | ✓ | ✓ | - | Legacy |

**Audio Codecs**:

| Codec | Decoder | Encoder | Notes |
|-------|---------|---------|-------|
| AAC | ✓ | ✓ | Most common, good quality |
| MP3 | ✓ | ✓ | Universal compatibility |
| AC3 (Dolby Digital) | ✓ | ✓ | Surround sound |
| EAC3 (DD+) | ✓ | ✓ | Enhanced AC3 |
| Opus | ✓ | ✓ | Low latency, excellent quality |
| Vorbis | ✓ | ✓ | Open source, good quality |
| FLAC | ✓ | ✓ | Lossless |

**Codec Selection**:
- Profile specifies preferred output codecs
- Transcoder selects based on:
  - Client compatibility
  - Encoding performance
  - Quality requirements
  - Hardware acceleration availability

#### 8.3.4 Hardware Acceleration

Hardware acceleration significantly reduces CPU usage and improves transcoding performance by offloading encoding/decoding to dedicated hardware.

**Supported Hardware Acceleration Methods**:

**1. VAAPI (Video Acceleration API)**

**Platform**: Linux with Intel/AMD GPUs

**Setup**:
```c
// Create VAAPI device context
AVBufferRef *hw_device_ctx = NULL;
av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI,
                       "/dev/dri/renderD128", NULL, 0);

// Set decoder to use hardware
dec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

// Create hardware frames context for encoder
AVBufferRef *hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ctx);
AVHWFramesContext *frames_ctx = (AVHWFramesContext*)hw_frames_ctx->data;
frames_ctx->format = AV_PIX_FMT_VAAPI;
frames_ctx->sw_format = AV_PIX_FMT_NV12;
frames_ctx->width = width;
frames_ctx->height = height;
av_hwframe_ctx_init(hw_frames_ctx);

enc_ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);
```

**Supported Codecs**:
- H.264 decode/encode
- HEVC decode/encode
- MPEG-2 decode/encode
- VP9 decode

**Performance**: 5-10x faster than software encoding

**2. NVENC (NVIDIA Encoder)**

**Platform**: Linux/Windows with NVIDIA GPUs (GTX 600+, Quadro, Tesla)

**Setup**:
```c
// Create CUDA device context
AVBufferRef *hw_device_ctx = NULL;
av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA,
                       "0", NULL, 0);  // GPU index

// Use h264_nvenc or hevc_nvenc encoder
AVCodec *encoder = avcodec_find_encoder_by_name("h264_nvenc");

// Set encoder options
av_opt_set(enc_ctx->priv_data, "preset", "p4", 0);  // Quality preset
av_opt_set(enc_ctx->priv_data, "rc", "vbr", 0);     // Rate control
```

**Supported Codecs**:
- H.264 encode
- HEVC encode

**Performance**: 10-20x faster than software encoding

**Quality Presets**:
- `p1` (fastest): Lowest quality, highest speed
- `p4` (medium): Balanced quality/speed
- `p7` (slowest): Highest quality, lower speed

**3. QSV (Intel Quick Sync Video)**

**Platform**: Linux/Windows with Intel CPUs (Sandy Bridge+)

**Setup**:
```c
// Create QSV device context
AVBufferRef *hw_device_ctx = NULL;
av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_QSV,
                       "auto", NULL, 0);

// Use h264_qsv or hevc_qsv encoder
AVCodec *encoder = avcodec_find_encoder_by_name("h264_qsv");
```

**Supported Codecs**:
- H.264 decode/encode
- HEVC decode/encode
- MPEG-2 decode/encode
- VP9 decode

**Performance**: 8-15x faster than software encoding

**Hardware Acceleration Selection**:
```c
// Detect available hardware acceleration
enum AVHWDeviceType detect_hw_accel(void)
{
  enum AVHWDeviceType type;
  
  // Try VAAPI first (Linux)
  type = av_hwdevice_find_type_by_name("vaapi");
  if (type != AV_HWDEVICE_TYPE_NONE && test_hw_device(type))
    return type;
  
  // Try NVENC (NVIDIA)
  type = av_hwdevice_find_type_by_name("cuda");
  if (type != AV_HWDEVICE_TYPE_NONE && test_hw_device(type))
    return type;
  
  // Try QSV (Intel)
  type = av_hwdevice_find_type_by_name("qsv");
  if (type != AV_HWDEVICE_TYPE_NONE && test_hw_device(type))
    return type;
  
  // Fall back to software
  return AV_HWDEVICE_TYPE_NONE;
}
```

#### 8.3.5 Transcoding Process

**Message Flow**:

1. **SMT_START Message**:
   - Analyze input streams (codecs, resolution, bitrate)
   - Select output codecs based on profile
   - Initialize decoders and encoders
   - Setup filter graphs for scaling/resampling
   - Send modified SMT_START to output with new stream info

2. **SMT_PACKET Messages**:
   - Receive input packet
   - Decode packet to raw frame
   - Apply filters (scale, deinterlace, etc.)
   - Encode frame to output codec
   - Send transcoded packet to output

3. **SMT_STOP Message**:
   - Flush encoder buffers
   - Close codecs
   - Free resources
   - Forward SMT_STOP to output

**Packet Processing**:
```c
void transcode_packet(transcoder_t *tc, th_pkt_t *pkt)
{
  AVPacket avpkt;
  AVFrame *frame;
  int ret;
  
  // Convert th_pkt to AVPacket
  av_init_packet(&avpkt);
  avpkt.data = pktbuf_ptr(pkt->pkt_payload);
  avpkt.size = pktbuf_len(pkt->pkt_payload);
  avpkt.pts = pkt->pkt_pts;
  avpkt.dts = pkt->pkt_dts;
  
  // Decode packet
  ret = avcodec_send_packet(tc->tc_vdec_ctx, &avpkt);
  if (ret < 0) {
    tvherror(LS_TRANSCODE, "decode error");
    return;
  }
  
  // Receive decoded frame
  frame = av_frame_alloc();
  ret = avcodec_receive_frame(tc->tc_vdec_ctx, frame);
  if (ret < 0) {
    av_frame_free(&frame);
    return;
  }
  
  // Apply filters (scaling, etc.)
  if (tc->tc_vfilter) {
    av_buffersrc_add_frame(tc->tc_vfilter_src, frame);
    av_buffersink_get_frame(tc->tc_vfilter_sink, frame);
  }
  
  // Encode frame
  ret = avcodec_send_frame(tc->tc_venc_ctx, frame);
  av_frame_free(&frame);
  
  // Receive encoded packet
  AVPacket enc_pkt;
  av_init_packet(&enc_pkt);
  ret = avcodec_receive_packet(tc->tc_venc_ctx, &enc_pkt);
  if (ret == 0) {
    // Convert AVPacket to th_pkt and send to output
    th_pkt_t *out_pkt = create_pkt_from_avpacket(&enc_pkt);
    streaming_target_deliver(tc->tc_output, 
                            streaming_msg_create_pkt(out_pkt));
    av_packet_unref(&enc_pkt);
  }
}
```

**Filter Graph Setup** (for scaling):
```c
void setup_video_filter(transcoder_t *tc, int in_w, int in_h, 
                       int out_w, int out_h)
{
  char args[512];
  AVFilterInOut *inputs, *outputs;
  
  // Create filter graph
  tc->tc_vfilter = avfilter_graph_alloc();
  
  // Source filter (input)
  snprintf(args, sizeof(args),
           "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d",
           in_w, in_h, tc->tc_vdec_ctx->pix_fmt,
           tc->tc_vdec_ctx->time_base.num,
           tc->tc_vdec_ctx->time_base.den);
  avfilter_graph_create_filter(&tc->tc_vfilter_src,
                               avfilter_get_by_name("buffer"),
                               "in", args, NULL, tc->tc_vfilter);
  
  // Sink filter (output)
  avfilter_graph_create_filter(&tc->tc_vfilter_sink,
                               avfilter_get_by_name("buffersink"),
                               "out", NULL, NULL, tc->tc_vfilter);
  
  // Scale filter
  snprintf(args, sizeof(args), "w=%d:h=%d", out_w, out_h);
  AVFilterContext *scale_ctx;
  avfilter_graph_create_filter(&scale_ctx,
                               avfilter_get_by_name("scale"),
                               "scale", args, NULL, tc->tc_vfilter);
  
  // Connect filters: source → scale → sink
  avfilter_link(tc->tc_vfilter_src, 0, scale_ctx, 0);
  avfilter_link(scale_ctx, 0, tc->tc_vfilter_sink, 0);
  
  // Configure graph
  avfilter_graph_config(tc->tc_vfilter, NULL);
}
```

#### 8.3.6 Transcoding Threads

Transcoding runs in a dedicated thread to avoid blocking the main streaming pipeline:

**Thread Function**:
```c
void *transcoder_thread(void *arg)
{
  transcoder_t *tc = arg;
  th_pkt_t *pkt;
  
  while (tc->tc_running) {
    // Wait for packet in queue
    tvh_mutex_lock(&tc->tc_mutex);
    while (TAILQ_EMPTY(&tc->tc_queue) && tc->tc_running)
      tvh_cond_wait(&tc->tc_cond, &tc->tc_mutex);
    
    pkt = TAILQ_FIRST(&tc->tc_queue);
    if (pkt)
      TAILQ_REMOVE(&tc->tc_queue, pkt, pkt_link);
    tvh_mutex_unlock(&tc->tc_mutex);
    
    if (!pkt)
      continue;
    
    // Transcode packet
    transcode_packet(tc, pkt);
    
    // Free input packet
    pkt_ref_dec(pkt);
  }
  
  return NULL;
}
```

**Benefits of Threading**:
- Non-blocking: Main thread continues processing
- Parallel processing: Transcoding happens concurrently
- Queue buffering: Smooths out processing spikes
- Resource isolation: Transcoding CPU usage doesn't block I/O

**Thread Priority**:
- Transcoding threads run at normal or slightly elevated priority
- Ensures responsive transcoding without starving other threads
- Can be configured per profile


### 8.4 Stream Filtering

Stream filtering allows selective inclusion or exclusion of elementary streams (video, audio, subtitles, teletext) based on various criteria. This enables bandwidth optimization, language selection, and content customization.

#### 8.4.1 Elementary Stream Filters

**Location**: `src/esfilter.h`, `src/esfilter.c`

Elementary stream filters define rules for which streams should be included in the output. Filters are evaluated during stream start and can be configured globally or per-profile.

**Filter Structure**:
```c
typedef struct esfilter {
  idnode_t esf_id;                    // Configuration node
  TAILQ_ENTRY(esfilter) esf_link;     // Link in filter list

  int esf_class;                      // Stream class (video, audio, etc.)
  int esf_save;                       // Save to configuration
  int esf_index;                      // Filter index (priority)
  int esf_enabled;                    // Filter enabled
  uint32_t esf_type;                  // Stream type mask
  char esf_language[4];               // Language code (ISO 639-2)
  char esf_service[UUID_HEX_SIZE];    // Service UUID (or empty for all)
  int esf_sindex;                     // Stream index
  int esf_pid;                        // PID (MPEG-TS specific)
  uint16_t esf_caid;                  // CA system ID
  uint32_t esf_caprovider;            // CA provider ID
  int esf_action;                     // Filter action
  int esf_log;                        // Log matches
  char *esf_comment;                  // User comment
} esfilter_t;
```

**Stream Classes**:
```c
typedef enum {
  ESF_CLASS_NONE = 0,
  ESF_CLASS_VIDEO,      // Video streams
  ESF_CLASS_AUDIO,      // Audio streams
  ESF_CLASS_TELETEXT,   // Teletext streams
  ESF_CLASS_SUBTIT,     // Subtitle streams
  ESF_CLASS_CA,         // CA (descrambling) streams
  ESF_CLASS_OTHER       // Other streams (data, etc.)
} esfilter_class_t;
```

**Filter Actions**:
```c
typedef enum {
  ESFA_NONE = 0,        // No action
  ESFA_USE,             // Use this stream
  ESFA_ONE_TIME,        // Use once per language
  ESFA_EXCLUSIVE,       // Use exclusively (ignore others)
  ESFA_EMPTY,           // Use if no other streams added
  ESFA_IGNORE           // Ignore this stream
} esfilter_action_t;
```

#### 8.4.2 Audio and Subtitle Track Selection

**Audio Track Selection**:

Filters can select audio tracks based on:
- **Language**: Prefer specific languages (e.g., "eng", "spa", "fra")
- **Codec**: Prefer specific codecs (AAC, AC3, MP3)
- **Channel count**: Prefer stereo vs. surround
- **Bitrate**: Prefer higher or lower bitrate

**Example Audio Filter Rules**:
```
1. Use English audio (ESFA_USE, language="eng")
2. Use Spanish audio if no English (ESFA_EMPTY, language="spa")
3. Ignore commentary tracks (ESFA_IGNORE, type=audio, description contains "commentary")
```

**Subtitle Track Selection**:

Filters can select subtitle tracks based on:
- **Language**: Match user's preferred languages
- **Type**: DVB subtitles vs. teletext
- **Hearing impaired**: Include or exclude HI subtitles
- **Forced**: Include forced subtitles for foreign language parts

**Example Subtitle Filter Rules**:
```
1. Use English subtitles (ESFA_USE, language="eng")
2. Use forced subtitles always (ESFA_USE, forced=true)
3. Ignore hearing impaired subtitles (ESFA_IGNORE, hearing_impaired=true)
```

#### 8.4.3 Stream Processing

Stream filtering is applied during the `SMT_START` message processing:

**Filter Application Process**:

1. **Receive SMT_START**:
   - Contains list of all available elementary streams
   - Each stream has: type, PID, language, codec, etc.

2. **Evaluate Filters**:
   - Iterate through all enabled filters in priority order
   - For each stream, check if filter matches
   - Apply filter action (USE, IGNORE, etc.)

3. **Build Output Stream List**:
   - Include streams marked as USE or EXCLUSIVE
   - Exclude streams marked as IGNORE
   - Apply ONE_TIME logic (one stream per language)
   - Apply EMPTY logic (fallback streams)

4. **Create Modified SMT_START**:
   - Contains only selected streams
   - Renumber streams if needed
   - Forward to next pipeline stage

**Filter Matching**:
```c
int esfilter_match(esfilter_t *esf, elementary_stream_t *st)
{
  // Check class
  if (esf->esf_class != ESF_CLASS_NONE &&
      esf->esf_class != stream_class(st->es_type))
    return 0;
  
  // Check type
  if (esf->esf_type && !(esf->esf_type & SCT_MASK(st->es_type)))
    return 0;
  
  // Check language
  if (esf->esf_language[0] &&
      strcmp(esf->esf_language, st->es_lang) != 0)
    return 0;
  
  // Check service
  if (esf->esf_service[0] &&
      strcmp(esf->esf_service, service_uuid(st->es_service)) != 0)
    return 0;
  
  // Check PID
  if (esf->esf_pid && esf->esf_pid != st->es_pid)
    return 0;
  
  // Check CA system
  if (esf->esf_caid && esf->esf_caid != st->es_caid)
    return 0;
  
  // All criteria match
  return 1;
}
```

**Filter Application**:
```c
void apply_esfilters(streaming_start_t *ss)
{
  esfilter_t *esf;
  elementary_stream_t *st;
  int i, action;
  
  // Mark all streams as not selected initially
  for (i = 0; i < ss->ss_num_components; i++)
    ss->ss_components[i].ssc_disabled = 1;
  
  // Apply filters in priority order
  TAILQ_FOREACH(esf, &esfilters[esf->esf_class], esf_link) {
    if (!esf->esf_enabled)
      continue;
    
    for (i = 0; i < ss->ss_num_components; i++) {
      st = &ss->ss_components[i];
      
      if (!esfilter_match(esf, st))
        continue;
      
      // Apply action
      switch (esf->esf_action) {
        case ESFA_USE:
          st->ssc_disabled = 0;
          break;
        
        case ESFA_EXCLUSIVE:
          // Disable all other streams of same class
          disable_other_streams(ss, st, esf->esf_class);
          st->ssc_disabled = 0;
          break;
        
        case ESFA_ONE_TIME:
          // Enable if no stream of this language enabled yet
          if (!has_language_enabled(ss, st->es_lang, esf->esf_class))
            st->ssc_disabled = 0;
          break;
        
        case ESFA_EMPTY:
          // Enable if no streams of this class enabled
          if (!has_class_enabled(ss, esf->esf_class))
            st->ssc_disabled = 0;
          break;
        
        case ESFA_IGNORE:
          st->ssc_disabled = 1;
          break;
      }
      
      if (esf->esf_log)
        tvhlog(LOG_INFO, "esfilter", "matched stream %d: %s",
               i, esfilter_action2txt(esf->esf_action));
    }
  }
}
```

#### 8.4.4 Default Filter Behavior

When no filters are configured, Tvheadend uses default behavior:

**Video Streams**:
- Include first video stream
- Ignore additional video streams (multi-angle, etc.)

**Audio Streams**:
- Include all audio streams
- Allows client to select preferred language

**Subtitle Streams**:
- Include all subtitle streams
- Allows client to enable/disable as needed

**Teletext Streams**:
- Include teletext if present
- Useful for subtitles and additional information

**CA Streams**:
- Include CA streams if service is encrypted
- Required for descrambling

**Other Streams**:
- Include data streams (HbbTV, etc.)
- May be filtered by client

#### 8.4.5 Filter Configuration Examples

**Example 1: English-Only Audio**
```
Class: Audio
Action: Use
Language: eng
Comment: Include English audio
```

**Example 2: Exclude Commentary Tracks**
```
Class: Audio
Action: Ignore
Type: AC3
Index: 2
Comment: Ignore commentary track (usually 3rd audio)
```

**Example 3: Prefer HD Video**
```
Class: Video
Action: Exclusive
Type: H.264
Service: (specific service UUID)
Comment: Use only H.264 video for this service
```

**Example 4: Multi-Language Support**
```
Filter 1:
  Class: Audio
  Action: Use
  Language: eng
  Comment: Primary language

Filter 2:
  Class: Audio
  Action: Use
  Language: spa
  Comment: Secondary language

Filter 3:
  Class: Subtitle
  Action: Use
  Language: eng
  Comment: English subtitles

Filter 4:
  Class: Subtitle
  Action: Use
  Language: spa
  Comment: Spanish subtitles
```

**Example 5: Bandwidth Optimization**
```
Filter 1:
  Class: Video
  Action: Use
  Comment: Include video

Filter 2:
  Class: Audio
  Action: One Time
  Language: eng
  Comment: One English audio track

Filter 3:
  Class: Subtitle
  Action: Ignore
  Comment: Exclude all subtitles to save bandwidth

Filter 4:
  Class: Teletext
  Action: Ignore
  Comment: Exclude teletext to save bandwidth
```

#### 8.4.6 Filter Priority and Ordering

Filters are evaluated in order of their `esf_index` value (lower values first). This allows fine-grained control over filter precedence:

**Priority Guidelines**:
- **0-99**: High priority (exclusive filters, must-have streams)
- **100-199**: Normal priority (preferred streams)
- **200-299**: Low priority (fallback streams)
- **300+**: Very low priority (ignore filters)

**Example Priority Usage**:
```
Index 10: Exclusive English audio (highest priority)
Index 100: Use Spanish audio (normal priority)
Index 200: Use French audio if no English/Spanish (fallback)
Index 300: Ignore commentary tracks (cleanup)
```

**Filter Evaluation**:
- Filters evaluated in index order
- First matching filter determines action
- Later filters can override if action is not EXCLUSIVE
- EXCLUSIVE action prevents further filter evaluation for that stream


### 8.5 Performance Considerations

The profile system's performance characteristics vary significantly based on the chosen profile type, configuration, and hardware capabilities. Understanding these considerations is essential for capacity planning and optimization.

#### 8.5.1 CPU Usage Patterns

**Pass-Through Profiles**:
- **CPU Usage**: Minimal (< 5% per stream on modern hardware)
- **Characteristics**:
  - No decoding or encoding
  - Simple packet copying and PSI/SI table rewriting
  - Scales well to many simultaneous streams
- **Bottleneck**: Network I/O and memory bandwidth
- **Typical Capacity**: 20-50+ simultaneous streams per server

**Remuxing Profiles** (Matroska, MP4):
- **CPU Usage**: Low to moderate (5-15% per stream)
- **Characteristics**:
  - Packet parsing and container format conversion
  - No codec transcoding
  - Metadata processing and embedding
- **Bottleneck**: Disk I/O for recording, CPU for parsing
- **Typical Capacity**: 10-30 simultaneous streams per server

**Transcoding Profiles** (Software):
- **CPU Usage**: High (50-100% per stream on single core)
- **Characteristics**:
  - Full decode and encode pipeline
  - CPU-intensive codec operations
  - Scales with video resolution and bitrate
- **Bottleneck**: CPU encoding performance
- **Typical Capacity**: 2-8 simultaneous streams per server (depends on resolution)

**Transcoding Profiles** (Hardware Accelerated):
- **CPU Usage**: Low (10-20% per stream)
- **Characteristics**:
  - Offloads encoding to GPU/dedicated hardware
  - CPU handles packet management and filtering
  - Limited by hardware encoder capacity
- **Bottleneck**: Hardware encoder sessions (typically 2-8 concurrent)
- **Typical Capacity**: 4-16 simultaneous streams per server (hardware dependent)

**CPU Usage by Resolution** (Software Transcoding):

| Resolution | Codec | CPU Usage (per stream) | Real-time Factor |
|------------|-------|------------------------|------------------|
| 480p (SD) | H.264 | 30-40% (1 core) | 2-3x |
| 720p (HD) | H.264 | 60-80% (1 core) | 1-1.5x |
| 1080p (FHD) | H.264 | 100%+ (1 core) | 0.5-1x |
| 1080p (FHD) | HEVC | 150%+ (1.5 cores) | 0.3-0.7x |
| 2160p (4K) | H.264 | 300%+ (3 cores) | 0.2-0.5x |
| 2160p (4K) | HEVC | 400%+ (4 cores) | 0.1-0.3x |

**Real-time Factor**: Ratio of encoding speed to playback speed. 1x means encoding keeps up with real-time playback.

#### 8.5.2 Memory Requirements

**Per-Stream Memory Usage**:

| Profile Type | Memory per Stream | Notes |
|--------------|-------------------|-------|
| Pass-through | 2-5 MB | Packet buffers only |
| Remuxing | 5-10 MB | Container format buffers |
| Transcoding (SD) | 50-100 MB | Frame buffers, codec contexts |
| Transcoding (HD) | 100-200 MB | Larger frame buffers |
| Transcoding (4K) | 200-400 MB | Very large frame buffers |

**Memory Allocation Patterns**:
- **Packet Buffers**: Allocated per stream, typically 1-2 MB
- **Frame Buffers**: Allocated for transcoding, size depends on resolution
- **Codec Contexts**: FFmpeg allocates internal buffers, 10-50 MB per codec
- **Filter Graphs**: Additional memory for scaling/filtering, 5-20 MB

**Memory Optimization**:
```c
// Limit queue sizes to prevent memory bloat
#define MAX_QUEUE_SIZE (10 * 1024 * 1024)  // 10 MB

// Use reference counting to avoid copying
pkt_ref_inc(pkt);  // Increment reference instead of copying

// Free resources promptly
streaming_msg_free(sm);  // Free message when done
```

**Memory Pressure Handling**:
- Monitor system memory usage
- Limit concurrent transcoding streams
- Use hardware acceleration to reduce memory footprint
- Implement queue size limits to prevent unbounded growth

#### 8.5.3 Quality vs Performance Tradeoffs

**Encoding Presets** (H.264 example):

| Preset | Encoding Speed | Quality | CPU Usage | Use Case |
|--------|----------------|---------|-----------|----------|
| ultrafast | 10x real-time | Poor | 20% | Low-latency, low-quality |
| superfast | 5x real-time | Fair | 30% | Fast encoding, acceptable quality |
| veryfast | 3x real-time | Good | 40% | Balanced for live streaming |
| faster | 2x real-time | Good | 50% | Default for live |
| fast | 1.5x real-time | Very Good | 60% | Higher quality live |
| medium | 1x real-time | Very Good | 80% | Best for live at 1x |
| slow | 0.5x real-time | Excellent | 100%+ | Not suitable for live |
| slower | 0.3x real-time | Excellent | 150%+ | Offline encoding only |
| veryslow | 0.1x real-time | Best | 200%+ | Offline encoding only |

**Recommended Presets for Live Streaming**:
- **Low latency**: `ultrafast` or `superfast`
- **Balanced**: `veryfast` or `faster`
- **High quality**: `fast` or `medium`
- **Never use**: `slow`, `slower`, `veryslow` (cannot keep up with real-time)

**Bitrate vs Quality**:

| Resolution | Minimum Bitrate | Recommended Bitrate | High Quality Bitrate |
|------------|----------------|---------------------|---------------------|
| 480p (SD) | 500 kbps | 1-2 Mbps | 3-4 Mbps |
| 720p (HD) | 1 Mbps | 2-4 Mbps | 5-8 Mbps |
| 1080p (FHD) | 2 Mbps | 4-8 Mbps | 10-15 Mbps |
| 2160p (4K) | 8 Mbps | 15-25 Mbps | 35-50 Mbps |

**Quality Optimization Strategies**:

1. **Two-Pass Encoding** (not suitable for live):
   - First pass analyzes video
   - Second pass optimizes bitrate allocation
   - Better quality for same bitrate
   - Requires 2x encoding time

2. **Constant Quality (CRF)**:
   - Maintains consistent quality
   - Variable bitrate
   - Good for recording
   - CRF 18-23 for high quality, 23-28 for normal

3. **Constant Bitrate (CBR)**:
   - Fixed bitrate
   - Variable quality
   - Good for streaming
   - Predictable bandwidth usage

4. **Variable Bitrate (VBR)**:
   - Adapts bitrate to content complexity
   - Better quality for same average bitrate
   - Less predictable bandwidth

#### 8.5.4 Optimization Guidelines

**Server Configuration**:

1. **CPU Selection**:
   - **Pass-through**: Any modern CPU sufficient
   - **Transcoding**: High core count (8+ cores) for multiple streams
   - **Hardware acceleration**: Intel Quick Sync, NVIDIA NVENC, or AMD VCE

2. **Memory**:
   - **Minimum**: 2 GB + (200 MB × concurrent transcoding streams)
   - **Recommended**: 4 GB + (400 MB × concurrent transcoding streams)
   - **Example**: 8 GB for 10 concurrent HD transcoding streams

3. **Storage**:
   - **SSD recommended** for DVR recording (reduces I/O bottleneck)
   - **RAID** for redundancy and performance
   - **Network storage** acceptable for pass-through, not ideal for transcoding

4. **Network**:
   - **Gigabit Ethernet** minimum for multiple streams
   - **10 Gigabit** for high-density deployments
   - **Low latency** important for live streaming

**Profile Configuration Best Practices**:

1. **Use Pass-Through When Possible**:
   - Lowest CPU usage
   - Best quality (no generation loss)
   - Lowest latency
   - Only transcode when necessary

2. **Enable Hardware Acceleration**:
   - 5-10x performance improvement
   - Reduces CPU usage by 80-90%
   - Allows more concurrent streams
   - Check hardware compatibility first

3. **Optimize Transcoding Settings**:
   - Use `veryfast` or `faster` preset for live
   - Set appropriate bitrate for target resolution
   - Enable multi-threading (`-threads auto`)
   - Use hardware decoders when available

4. **Implement Transcoder Sharing**:
   - Multiple clients share same transcoded stream
   - Reduces CPU usage significantly
   - Requires clients to accept same quality
   - Automatic in Tvheadend when possible

5. **Limit Concurrent Transcoding**:
   - Set maximum concurrent transcoding streams
   - Queue additional requests
   - Prevents server overload
   - Maintains quality for active streams

**Monitoring and Tuning**:

1. **Monitor CPU Usage**:
   ```bash
   # Check per-process CPU usage
   top -p $(pgrep tvheadend)
   
   # Check per-thread CPU usage
   top -H -p $(pgrep tvheadend)
   ```

2. **Monitor Memory Usage**:
   ```bash
   # Check memory usage
   ps aux | grep tvheadend
   
   # Check detailed memory breakdown
   pmap $(pgrep tvheadend)
   ```

3. **Monitor I/O**:
   ```bash
   # Check I/O statistics
   iostat -x 1
   
   # Check per-process I/O
   iotop -p $(pgrep tvheadend)
   ```

4. **Profile Performance**:
   - Enable Tvheadend's built-in profiling (`--tprofile`)
   - Identify bottlenecks in pipeline
   - Optimize slow components
   - Test different configurations

**Capacity Planning**:

**Example Server Configurations**:

**Configuration 1: Pass-Through Server**
- CPU: Intel Core i3 or equivalent
- RAM: 4 GB
- Storage: HDD
- Capacity: 30+ simultaneous streams
- Use Case: Simple streaming, no transcoding

**Configuration 2: Light Transcoding Server**
- CPU: Intel Core i5 with Quick Sync
- RAM: 8 GB
- Storage: SSD
- Capacity: 8-12 simultaneous HD transcoding streams
- Use Case: Mixed pass-through and transcoding

**Configuration 3: Heavy Transcoding Server**
- CPU: Intel Core i7/i9 or AMD Ryzen 7/9 (8+ cores)
- RAM: 16 GB
- GPU: NVIDIA GTX 1660 or better (for NVENC)
- Storage: SSD RAID
- Capacity: 15-20 simultaneous HD transcoding streams
- Use Case: High-density transcoding

**Configuration 4: Enterprise Server**
- CPU: Intel Xeon or AMD EPYC (16+ cores)
- RAM: 32 GB+
- GPU: NVIDIA Quadro or Tesla (for NVENC)
- Storage: NVMe SSD RAID
- Network: 10 Gigabit
- Capacity: 50+ simultaneous streams (mixed pass-through and transcoding)
- Use Case: Large-scale deployment

**Performance Testing**:

1. **Baseline Test**:
   - Start with single stream
   - Measure CPU, memory, I/O
   - Establish baseline performance

2. **Scaling Test**:
   - Gradually add streams
   - Monitor resource usage
   - Identify breaking point
   - Determine maximum capacity

3. **Stress Test**:
   - Run at maximum capacity
   - Monitor for errors or quality degradation
   - Test for extended period (hours)
   - Verify stability

4. **Real-World Test**:
   - Simulate actual usage patterns
   - Mix of pass-through and transcoding
   - Varying resolutions and bitrates
   - Peak usage scenarios

---

[← Previous](07-Streaming-Architecture.md) | [Table of Contents](00-TOC.md) | [Next →](09-Muxer-System.md)

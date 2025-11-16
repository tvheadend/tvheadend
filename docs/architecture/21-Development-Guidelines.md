[← Back to Table of Contents](00-TOC.md)

## 21. Development Guidelines

This section provides practical guidance for developers working on Tvheadend. It covers common development tasks, debugging techniques, and coding conventions to help maintain consistency and quality across the codebase.

### 21.1 Adding New Features

This section provides step-by-step guidance for implementing common types of new features in Tvheadend.

#### 21.1.1 Adding a New Input Type

Adding support for a new input source type (e.g., a new network tuner protocol or hardware interface) requires implementing the input subsystem interfaces and integrating with the existing infrastructure.

**Step 1: Understand the Input Hierarchy**

Review the input subsystem architecture (Section 5) to understand the class hierarchy:
- `tvh_hardware` - Base class for hardware devices
- `tvh_input` - Represents a tuner/input
- `tvh_input_instance` - Active tuning instance
- `mpegts_mux` - Multiplexed stream
- `mpegts_service` - Individual service/channel

**Step 2: Create Directory Structure**

Create a new directory under `src/input/mpegts/` for your input type:

```bash
mkdir -p src/input/mpegts/myinput
cd src/input/mpegts/myinput
```

**Step 3: Define Data Structures**

Create header file `myinput.h`:

```c
#ifndef __TVH_MYINPUT_H__
#define __TVH_MYINPUT_H__

#include "input.h"
#include "mpegts_input.h"

// Hardware device structure
typedef struct myinput_hardware {
  tvh_hardware_t;  // Inherit from tvh_hardware_t
  
  // Device-specific fields
  char *mh_device_path;
  int mh_device_fd;
  // ... other fields
} myinput_hardware_t;

// Input/tuner structure
typedef struct myinput_input {
  mpegts_input_t;  // Inherit from mpegts_input_t
  
  // Input-specific fields
  int mi_tuner_number;
  // ... other fields
} myinput_input_t;

// Active instance structure
typedef struct myinput_instance {
  mpegts_input_instance_t;  // Inherit from mpegts_input_instance_t
  
  // Instance-specific fields
  pthread_t mii_thread;
  int mii_running;
  // ... other fields
} myinput_instance_t;

// Initialization functions
void myinput_init(void);
void myinput_done(void);

#endif /* __TVH_MYINPUT_H__ */
```

**Step 4: Implement Core Functions**

Create implementation file `myinput.c`:

```c
#include "tvheadend.h"
#include "myinput.h"
#include "notify.h"
#include "atomic.h"
#include "tvhpoll.h"

// Class definitions
static const idclass_t myinput_hardware_class;
static const idclass_t myinput_input_class;

// Hardware discovery
static void myinput_hardware_discover(void)
{
  myinput_hardware_t *mh;
  
  tvh_mutex_lock(&global_lock);
  
  // Scan for devices (example: enumerate /dev/myinput*)
  // For each device found:
  mh = calloc(1, sizeof(myinput_hardware_t));
  mh->mh_device_path = strdup("/dev/myinput0");
  
  // Initialize base class
  tvh_hardware_create(&mh->th_id, &myinput_hardware_class,
                      "myinput-device-0", NULL);
  
  // Create inputs (tuners) for this hardware
  myinput_input_create(mh, 0);  // Tuner 0
  myinput_input_create(mh, 1);  // Tuner 1
  
  tvh_mutex_unlock(&global_lock);
}

// Input creation
static myinput_input_t *
myinput_input_create(myinput_hardware_t *mh, int tuner_num)
{
  myinput_input_t *mi;
  char buf[256];
  
  mi = calloc(1, sizeof(myinput_input_t));
  mi->mi_tuner_number = tuner_num;
  
  // Initialize base class
  snprintf(buf, sizeof(buf), "MyInput Tuner #%d", tuner_num);
  mpegts_input_create(&mi->mi_id, &myinput_input_class,
                      buf, (tvh_hardware_t *)mh);
  
  return mi;
}

// Start streaming (called when subscription requests service)
static int
myinput_input_start_mux(mpegts_input_t *mi, mpegts_mux_instance_t *mmi,
                        int weight)
{
  myinput_input_t *mii = (myinput_input_t *)mi;
  myinput_instance_t *inst;
  
  // Create instance
  inst = calloc(1, sizeof(myinput_instance_t));
  mpegts_input_instance_create(&inst->mii_id, mi, mmi);
  
  // Open device
  inst->mii_device_fd = open(mii->mh_device_path, O_RDONLY);
  if (inst->mii_device_fd < 0) {
    free(inst);
    return -1;
  }
  
  // Start receiver thread
  inst->mii_running = 1;
  tvh_thread_create(&inst->mii_thread, NULL, 
                    myinput_input_thread, inst, "myinput");
  
  return 0;
}

// Stop streaming
static void
myinput_input_stop_mux(mpegts_input_t *mi, mpegts_mux_instance_t *mmi)
{
  myinput_instance_t *inst = (myinput_instance_t *)mmi->mmi_input_instance;
  
  if (!inst) return;
  
  // Stop thread
  inst->mii_running = 0;
  pthread_join(inst->mii_thread, NULL);
  
  // Close device
  close(inst->mii_device_fd);
  
  // Cleanup
  mpegts_input_instance_destroy(&inst->mii_id);
  free(inst);
}

// Receiver thread
static void *
myinput_input_thread(void *aux)
{
  myinput_instance_t *inst = aux;
  mpegts_mux_instance_t *mmi = inst->mii_id.mii_mux_instance;
  uint8_t buf[188 * 7];  // TS packet buffer
  ssize_t n;
  
  while (inst->mii_running) {
    // Read TS packets from device
    n = read(inst->mii_device_fd, buf, sizeof(buf));
    if (n < 0) {
      if (errno == EINTR) continue;
      tvherror(LS_MYINPUT, "read error: %s", strerror(errno));
      break;
    }
    
    if (n == 0) break;  // EOF
    
    // Deliver packets to mux
    mpegts_input_recv_packets(mmi, buf, n, NULL, NULL, NULL);
  }
  
  return NULL;
}

// Class definitions
static const idclass_t myinput_hardware_class = {
  .ic_class      = "myinput_hardware",
  .ic_caption    = N_("MyInput Hardware"),
  .ic_properties = (const property_t[]){
    {
      .type   = PT_STR,
      .id     = "device_path",
      .name   = N_("Device Path"),
      .off    = offsetof(myinput_hardware_t, mh_device_path),
    },
    {}
  }
};

static const idclass_t myinput_input_class = {
  .ic_class      = "myinput_input",
  .ic_caption    = N_("MyInput Input"),
  .ic_properties = (const property_t[]){
    {
      .type   = PT_INT,
      .id     = "tuner_number",
      .name   = N_("Tuner Number"),
      .off    = offsetof(myinput_input_t, mi_tuner_number),
    },
    {}
  }
};

// Module initialization
void myinput_init(void)
{
  // Register classes
  idclass_register(&myinput_hardware_class);
  idclass_register(&myinput_input_class);
  
  // Discover hardware
  myinput_hardware_discover();
}

void myinput_done(void)
{
  // Cleanup (called during shutdown)
}
```

**Step 5: Integrate with Build System**

Add to `Makefile`:

```makefile
# In the SRCS section
SRCS += src/input/mpegts/myinput/myinput.c
```

**Step 6: Register in Main Initialization**

Edit `src/main.c` to call your initialization function:

```c
#include "input/mpegts/myinput/myinput.h"

// In main() function, after other input initializations:
myinput_init();
```

**Step 7: Add Configuration UI**

Create web UI configuration in `src/webui/static/app/`:
- Add input type to configuration dialogs
- Implement network/mux scanning if applicable
- Add device-specific settings

**Step 8: Testing**

1. Build and run Tvheadend
2. Check that devices are discovered: Configuration → DVB Inputs
3. Add networks and muxes
4. Scan for services
5. Create subscriptions and verify streaming works
6. Test error conditions (device disconnect, tuning failures)

**Key Considerations:**

- **Thread Safety**: All access to shared data must be protected by `global_lock`
- **Reference Counting**: Use proper reference counting for services and packets
- **Error Handling**: Handle device errors gracefully, report status via streaming flags
- **Statistics**: Update input statistics (signal strength, errors, etc.)
- **Configuration**: Implement save/load for device-specific settings

#### 21.1.2 Adding a New API Endpoint

Adding a new REST API endpoint allows external applications and the web UI to interact with Tvheadend functionality.

**Step 1: Understand the API System**

Review the HTTP/API server architecture (Section 15) to understand:
- API registration system
- Request/response format (htsmsg)
- Authentication and access control
- idnode automatic API generation

**Step 2: Define API Handler Function**

Create your API handler function in the appropriate source file (e.g., `src/api/api_myfeature.c`):

```c
#include "tvheadend.h"
#include "access.h"
#include "api.h"
#include "htsmsg_json.h"

/**
 * API endpoint: /api/myfeature/list
 * Lists all items of myfeature
 */
static int
api_myfeature_list(access_t *perm, void *opaque, const char *op,
                   htsmsg_t *args, htsmsg_t **resp)
{
  htsmsg_t *list, *item;
  myfeature_t *mf;
  
  // Check permissions
  if (!access_verify2(perm, ACCESS_ADMIN)) {
    return EPERM;
  }
  
  // Create response message
  list = htsmsg_create_list();
  
  tvh_mutex_lock(&global_lock);
  
  // Iterate through items
  LIST_FOREACH(mf, &myfeature_list, mf_link) {
    item = htsmsg_create_map();
    
    // Add item fields
    htsmsg_add_str(item, "uuid", idnode_uuid_as_str(&mf->mf_id));
    htsmsg_add_str(item, "name", mf->mf_name ?: "");
    htsmsg_add_u32(item, "enabled", mf->mf_enabled);
    htsmsg_add_s64(item, "created", mf->mf_created);
    
    htsmsg_add_msg(list, NULL, item);
  }
  
  tvh_mutex_unlock(&global_lock);
  
  // Set response
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", list);
  
  return 0;
}

/**
 * API endpoint: /api/myfeature/create
 * Creates a new myfeature item
 */
static int
api_myfeature_create(access_t *perm, void *opaque, const char *op,
                     htsmsg_t *args, htsmsg_t **resp)
{
  myfeature_t *mf;
  const char *name;
  htsmsg_t *conf;
  
  // Check permissions
  if (!access_verify2(perm, ACCESS_ADMIN)) {
    return EPERM;
  }
  
  // Get parameters
  name = htsmsg_get_str(args, "name");
  if (!name || !*name) {
    return EINVAL;
  }
  
  tvh_mutex_lock(&global_lock);
  
  // Check for duplicate
  LIST_FOREACH(mf, &myfeature_list, mf_link) {
    if (strcmp(mf->mf_name, name) == 0) {
      tvh_mutex_unlock(&global_lock);
      return EEXIST;
    }
  }
  
  // Create new item
  mf = myfeature_create(name);
  if (!mf) {
    tvh_mutex_unlock(&global_lock);
    return ENOMEM;
  }
  
  // Apply configuration from args
  conf = htsmsg_get_map(args, "conf");
  if (conf) {
    idnode_load(&mf->mf_id, conf);
  }
  
  // Save configuration
  idnode_changed(&mf->mf_id);
  
  tvh_mutex_unlock(&global_lock);
  
  // Return UUID of created item
  *resp = htsmsg_create_map();
  htsmsg_add_str(*resp, "uuid", idnode_uuid_as_str(&mf->mf_id));
  
  return 0;
}

/**
 * API endpoint: /api/myfeature/delete
 * Deletes a myfeature item
 */
static int
api_myfeature_delete(access_t *perm, void *opaque, const char *op,
                     htsmsg_t *args, htsmsg_t **resp)
{
  myfeature_t *mf;
  const char *uuid;
  
  // Check permissions
  if (!access_verify2(perm, ACCESS_ADMIN)) {
    return EPERM;
  }
  
  // Get UUID parameter
  uuid = htsmsg_get_str(args, "uuid");
  if (!uuid) {
    return EINVAL;
  }
  
  tvh_mutex_lock(&global_lock);
  
  // Find item
  mf = myfeature_find_by_uuid(uuid);
  if (!mf) {
    tvh_mutex_unlock(&global_lock);
    return ENOENT;
  }
  
  // Delete item
  myfeature_delete(mf);
  
  tvh_mutex_unlock(&global_lock);
  
  return 0;
}

/**
 * Initialize API endpoints
 */
void api_myfeature_init(void)
{
  static api_hook_t ah[] = {
    { "myfeature/list",   ACCESS_ADMIN, api_myfeature_list,   NULL },
    { "myfeature/create", ACCESS_ADMIN, api_myfeature_create, NULL },
    { "myfeature/delete", ACCESS_ADMIN, api_myfeature_delete, NULL },
    { NULL },
  };
  
  api_register_all(ah);
}
```

**Step 3: Register API Endpoints**

Add initialization call in `src/api.c`:

```c
// In api_init() function:
api_myfeature_init();
```

**Step 4: Add to Build System**

Add to `Makefile`:

```makefile
SRCS += src/api/api_myfeature.c
```

**Step 5: Test API Endpoints**

Test using curl or the web UI:

```bash
# List items
curl -u admin:admin http://localhost:9981/api/myfeature/list

# Create item
curl -u admin:admin -X POST http://localhost:9981/api/myfeature/create \
  -H "Content-Type: application/json" \
  -d '{"name": "test-item", "conf": {"enabled": true}}'

# Delete item
curl -u admin:admin -X POST http://localhost:9981/api/myfeature/delete \
  -H "Content-Type: application/json" \
  -d '{"uuid": "12345678-1234-1234-1234-123456789abc"}'
```

**Using idnode for Automatic API Generation**

If your feature uses idnode (configuration objects), you get automatic API endpoints:

```c
// Define idnode class
static const idclass_t myfeature_class = {
  .ic_class      = "myfeature",
  .ic_caption    = N_("My Feature"),
  .ic_event      = "myfeature",
  .ic_save       = myfeature_class_save,
  .ic_delete     = myfeature_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type   = PT_STR,
      .id     = "name",
      .name   = N_("Name"),
      .off    = offsetof(myfeature_t, mf_name),
    },
    {
      .type   = PT_BOOL,
      .id     = "enabled",
      .name   = N_("Enabled"),
      .off    = offsetof(myfeature_t, mf_enabled),
    },
    {}
  }
};

// Register class
idclass_register(&myfeature_class);
```

This automatically provides:
- `/api/idnode/load` - Load object configuration
- `/api/idnode/save` - Save object configuration
- `/api/idnode/delete` - Delete object
- `/api/idnode/tree` - List objects in tree format

**Key Considerations:**

- **Authentication**: Always check permissions using `access_verify2()`
- **Input Validation**: Validate all input parameters
- **Error Codes**: Return appropriate errno values (EPERM, EINVAL, ENOENT, etc.)
- **Thread Safety**: Hold `global_lock` when accessing shared data
- **Response Format**: Use htsmsg for structured responses
- **Documentation**: Document API endpoints in user documentation

#### 21.1.3 Adding Streaming Features

Adding new streaming features involves working with the streaming pad/target architecture and understanding the data flow through the system.

**Step 1: Understand Streaming Architecture**

Review the streaming architecture (Section 7) to understand:
- Streaming pad/target pattern
- Streaming message types
- Packet delivery mechanism
- Service streaming state

**Step 2: Create Streaming Target**

If your feature consumes streaming data, implement a streaming target:

```c
#include "tvheadend.h"
#include "streaming.h"
#include "service.h"

typedef struct myfeature_target {
  streaming_target_t mt_target;  // Base streaming target
  
  // Feature-specific fields
  service_t *mt_service;
  int mt_packet_count;
  int64_t mt_byte_count;
  
  // Processing state
  elementary_stream_t *mt_video_stream;
  elementary_stream_t *mt_audio_stream;
} myfeature_target_t;

/**
 * Streaming callback - receives all streaming messages
 */
static void
myfeature_target_input(void *opaque, streaming_message_t *sm)
{
  myfeature_target_t *mt = opaque;
  th_pkt_t *pkt;
  elementary_stream_t *st;
  
  switch (sm->sm_type) {
    
  case SMT_START:
    // Stream starting - analyze component list
    tvhtrace(LS_MYFEATURE, "stream starting");
    
    // Find video and audio streams
    TAILQ_FOREACH(st, &sm->sm_data, es_link) {
      if (SCT_ISVIDEO(st->es_type) && !mt->mt_video_stream) {
        mt->mt_video_stream = st;
        tvhinfo(LS_MYFEATURE, "found video stream: %s", 
                streaming_component_type2txt(st->es_type));
      }
      else if (SCT_ISAUDIO(st->es_type) && !mt->mt_audio_stream) {
        mt->mt_audio_stream = st;
        tvhinfo(LS_MYFEATURE, "found audio stream: %s",
                streaming_component_type2txt(st->es_type));
      }
    }
    break;
    
  case SMT_STOP:
    // Stream stopping
    tvhtrace(LS_MYFEATURE, "stream stopping: code=%d", sm->sm_code);
    mt->mt_video_stream = NULL;
    mt->mt_audio_stream = NULL;
    break;
    
  case SMT_PACKET:
    // Data packet received
    pkt = sm->sm_data;
    
    // Filter by stream
    if (pkt->pkt_componentindex == mt->mt_video_stream->es_index) {
      // Process video packet
      myfeature_process_video_packet(mt, pkt);
    }
    else if (pkt->pkt_componentindex == mt->mt_audio_stream->es_index) {
      // Process audio packet
      myfeature_process_audio_packet(mt, pkt);
    }
    
    // Update statistics
    mt->mt_packet_count++;
    mt->mt_byte_count += pkt->pkt_payload ? pkt->pkt_payload->pb_size : 0;
    break;
    
  case SMT_GRACE:
    // Grace period - service quality testing
    tvhtrace(LS_MYFEATURE, "grace period: %d seconds", sm->sm_code);
    break;
    
  case SMT_SERVICE_STATUS:
    // Service status update
    tvhtrace(LS_MYFEATURE, "service status: 0x%x", sm->sm_code);
    break;
    
  case SMT_SIGNAL_STATUS:
    // Signal quality update
    signal_status_t *sig = sm->sm_data;
    tvhtrace(LS_MYFEATURE, "signal: snr=%d strength=%d", 
             sig->snr, sig->signal_strength);
    break;
    
  case SMT_NOSTART:
    // Failed to start
    tvhwarn(LS_MYFEATURE, "failed to start stream: code=%d", sm->sm_code);
    break;
    
  case SMT_MPEGTS:
    // Raw MPEG-TS data (if SUBSCRIPTION_MPEGTS flag set)
    mpegts_psi_table_t *mt = sm->sm_data;
    // Process raw TS data
    break;
    
  case SMT_SKIP:
    // Seek operation
    streaming_skip_t *skip = sm->sm_data;
    tvhtrace(LS_MYFEATURE, "skip: type=%d time=%"PRId64, 
             skip->type, skip->time);
    break;
    
  case SMT_SPEED:
    // Speed change
    tvhtrace(LS_MYFEATURE, "speed: %d", sm->sm_code);
    break;
    
  case SMT_TIMESHIFT_STATUS:
    // Timeshift status
    timeshift_status_t *ts = sm->sm_data;
    tvhtrace(LS_MYFEATURE, "timeshift: shift=%"PRId64, ts->shift);
    break;
    
  case SMT_EXIT:
    // Exit signal
    tvhtrace(LS_MYFEATURE, "exit signal received");
    break;
  }
  
  // Always unref the message when done
  streaming_msg_free(sm);
}

/**
 * Create and attach streaming target to service
 */
myfeature_target_t *
myfeature_target_create(service_t *s)
{
  myfeature_target_t *mt;
  
  mt = calloc(1, sizeof(myfeature_target_t));
  mt->mt_service = s;
  
  // Initialize streaming target
  streaming_target_init(&mt->mt_target, myfeature_target_input, mt, 0);
  
  // Attach to service streaming pad
  tvh_mutex_lock(&s->s_stream_mutex);
  streaming_target_connect(&s->s_streaming_pad, &mt->mt_target);
  tvh_mutex_unlock(&s->s_stream_mutex);
  
  return mt;
}

/**
 * Detach and destroy streaming target
 */
void
myfeature_target_destroy(myfeature_target_t *mt)
{
  service_t *s = mt->mt_service;
  
  // Detach from service
  tvh_mutex_lock(&s->s_stream_mutex);
  streaming_target_disconnect(&s->s_streaming_pad, &mt->mt_target);
  tvh_mutex_unlock(&s->s_stream_mutex);
  
  // Cleanup
  free(mt);
}
```

**Step 3: Create Streaming Pad (If Generating Data)**

If your feature generates streaming data, implement a streaming pad:

```c
typedef struct myfeature_source {
  streaming_pad_t ms_pad;  // Base streaming pad
  
  // Source-specific fields
  pthread_t ms_thread;
  int ms_running;
  
  // Stream configuration
  elementary_stream_t ms_video_stream;
  elementary_stream_t ms_audio_stream;
} myfeature_source_t;

/**
 * Send stream start message
 */
static void
myfeature_source_send_start(myfeature_source_t *ms)
{
  streaming_start_t *ss;
  
  // Create start message with component list
  ss = streaming_start_create();
  
  // Add video stream
  streaming_start_component_add(ss, &ms->ms_video_stream);
  
  // Add audio stream
  streaming_start_component_add(ss, &ms->ms_audio_stream);
  
  // Deliver to all targets
  streaming_pad_deliver(&ms->ms_pad, streaming_msg_create(SMT_START, ss));
}

/**
 * Send data packet
 */
static void
myfeature_source_send_packet(myfeature_source_t *ms, 
                             uint8_t *data, size_t len,
                             int stream_index, int64_t pts, int64_t dts)
{
  th_pkt_t *pkt;
  
  // Create packet
  pkt = pkt_alloc(&ms->ms_video_stream, data, len, pts, dts, 0);
  pkt->pkt_componentindex = stream_index;
  
  // Deliver to all targets
  streaming_pad_deliver(&ms->ms_pad, streaming_msg_create_pkt(pkt));
  
  // Release our reference
  pkt_ref_dec(pkt);
}

/**
 * Send stream stop message
 */
static void
myfeature_source_send_stop(myfeature_source_t *ms, int reason)
{
  streaming_pad_deliver(&ms->ms_pad, 
                       streaming_msg_create_code(SMT_STOP, reason));
}

/**
 * Create streaming source
 */
myfeature_source_t *
myfeature_source_create(void)
{
  myfeature_source_t *ms;
  
  ms = calloc(1, sizeof(myfeature_source_t));
  
  // Initialize streaming pad
  streaming_pad_init(&ms->ms_pad);
  
  // Configure video stream
  memset(&ms->ms_video_stream, 0, sizeof(elementary_stream_t));
  ms->ms_video_stream.es_index = 0;
  ms->ms_video_stream.es_type = SCT_H264;
  ms->ms_video_stream.es_width = 1920;
  ms->ms_video_stream.es_height = 1080;
  
  // Configure audio stream
  memset(&ms->ms_audio_stream, 0, sizeof(elementary_stream_t));
  ms->ms_audio_stream.es_index = 1;
  ms->ms_audio_stream.es_type = SCT_AAC;
  ms->ms_audio_stream.es_channels = 2;
  ms->ms_audio_stream.es_sri = 48000;
  
  return ms;
}
```

**Step 4: Integration Example**

Example of using streaming features in a subscription:

```c
void
myfeature_start_streaming(const char *channel_uuid)
{
  channel_t *ch;
  th_subscription_t *sub;
  myfeature_target_t *mt;
  
  tvh_mutex_lock(&global_lock);
  
  // Find channel
  ch = channel_find_by_uuid(channel_uuid);
  if (!ch) {
    tvh_mutex_unlock(&global_lock);
    return;
  }
  
  // Create subscription
  sub = subscription_create_from_channel(ch, 100, "myfeature",
                                         SUBSCRIPTION_PACKET,
                                         NULL, NULL, NULL, NULL);
  if (!sub) {
    tvh_mutex_unlock(&global_lock);
    return;
  }
  
  tvh_mutex_unlock(&global_lock);
  
  // Create streaming target and attach to subscription
  mt = myfeature_target_create(sub->ths_service);
  
  // Store for later cleanup
  // ...
}
```

**Key Considerations:**

- **Message Ownership**: Always call `streaming_msg_free()` when done with messages
- **Packet References**: Use `pkt_ref_inc()` and `pkt_ref_dec()` correctly
- **Thread Safety**: Hold `s_stream_mutex` when connecting/disconnecting targets
- **Component Analysis**: Parse SMT_START to identify available streams
- **Error Handling**: Handle SMT_STOP and SMT_NOSTART appropriately
- **Performance**: Keep streaming callbacks fast, defer heavy work to tasklets


### 21.2 Debugging Techniques

Effective debugging is essential for maintaining and improving Tvheadend. This section covers the debugging tools and techniques available for diagnosing issues.

#### 21.2.1 Debug Logging

Tvheadend provides comprehensive debug logging that can be enabled at runtime or compile time.

**Runtime Debug Logging**

Enable debug logging for specific subsystems using command-line options:

```bash
# Enable debug logging for specific subsystems
tvheadend --debug service,subscription,streaming

# Enable debug for multiple subsystems
tvheadend --debug linuxdvb,iptv,satip

# Enable all debug logging (very verbose!)
tvheadend --debug all

# Combine with other options
tvheadend -C -c /tmp/tvh-test --debug dvr,epg
```

**Available Debug Subsystems:**

Common subsystems for debugging:
- `service` - Service lifecycle and operations
- `subscription` - Subscription management
- `streaming` - Streaming data flow
- `mpegts` - MPEG-TS parsing
- `linuxdvb` - DVB hardware input
- `iptv` - IPTV input
- `satip` - SAT>IP client/server
- `dvr` - DVR recording
- `epg` - EPG grabbing and matching
- `htsp` - HTSP protocol
- `http` - HTTP server
- `descrambler` - Descrambling operations
- `caclient` - CA client communication
- `timeshift` - Timeshift operations
- `transcode` - Transcoding
- `api` - API calls
- `config` - Configuration loading/saving
- `access` - Access control
- `channel` - Channel management
- `bouquet` - Bouquet management
- `idnode` - Configuration object system
- `lock` - Lock acquisition/release (requires ENABLE_TRACE)
- `thread` - Thread operations (requires ENABLE_TRACE)

**Debug Logging in Code:**

Add debug logging to your code:

```c
#include "tvhlog.h"

// Debug logging (only output if subsystem debug enabled)
tvhdebug(LS_MYSUBSYS, "processing item: %s", item_name);

// Trace logging (very verbose, requires --trace)
tvhtrace(LS_MYSUBSYS, "detailed state: x=%d y=%d", x, y);

// Info logging (always output)
tvhinfo(LS_MYSUBSYS, "operation completed successfully");

// Warning logging
tvhwarn(LS_MYSUBSYS, "unusual condition: %s", condition);

// Error logging
tvherror(LS_MYSUBSYS, "operation failed: %s", strerror(errno));

// Logging with hexdump
tvhlog_hexdump(LS_MYSUBSYS, data, len);
```

**Define Your Subsystem:**

Add your subsystem to `src/tvhlog.h`:

```c
// In tvhlog.h
#define LS_MYSUBSYS  "mysubsys"

// Register in tvhlog.c
static const char *subsys_filters[] = {
  // ... existing subsystems ...
  "mysubsys",
  NULL
};
```

**Logging Levels:**

```c
// Set log level
tvhlog_set_debug("mysubsys");   // Enable debug
tvhlog_set_trace("mysubsys");   // Enable trace (very verbose)

// Check if debug/trace enabled
if (tvhlog_debug_enabled(LS_MYSUBSYS)) {
  // Expensive debug operation
  char *debug_str = generate_debug_info();
  tvhdebug(LS_MYSUBSYS, "debug info: %s", debug_str);
  free(debug_str);
}
```

**Log Output Destinations:**

```bash
# Log to stderr (default)
tvheadend --debug service

# Log to syslog
tvheadend --debug service --syslog

# Log to file
tvheadend --debug service --logfile /var/log/tvheadend.log

# Combine destinations
tvheadend --debug service --syslog --logfile /var/log/tvheadend.log
```

#### 21.2.2 Trace Logging

Trace logging provides extremely detailed output for deep debugging. It requires building with trace support.

**Build with Trace Support:**

```bash
# Configure with trace enabled
./configure --enable-trace

# Build
make clean
make
```

**Enable Trace Logging:**

```bash
# Enable trace for specific subsystems
tvheadend --trace service,streaming

# Trace shows very detailed information:
# - Function entry/exit
# - Lock acquisition/release
# - Detailed state changes
# - Data structure dumps
```

**Trace Logging in Code:**

```c
// Trace logging (only with --trace and ENABLE_TRACE)
tvhtrace(LS_MYSUBSYS, "entering function: param=%d", param);

// Conditional trace
if (tvhtrace_enabled()) {
  // Expensive trace operation
  dump_data_structure(obj);
}

// Trace with source location
tvhtrace(LS_MYSUBSYS, "%s:%d: detailed state", __FILE__, __LINE__);
```

**Lock Tracing:**

With trace enabled, lock operations are logged:

```bash
tvheadend --trace lock

# Output shows:
# [TRACE] lock: acquiring global_lock at service.c:123
# [TRACE] lock: acquired global_lock (held 0.5ms)
# [TRACE] lock: releasing global_lock at service.c:145
```

#### 21.2.3 Thread State Analysis

Understanding thread state is crucial for debugging concurrency issues.

**Thread Debugging Options:**

```bash
# Enable thread debugging
tvheadend --thrdebug 1

# Set deadlock detection timeout (seconds)
export TVHEADEND_THREAD_WATCH_LIMIT=15

# Enable thread and lock tracing
tvheadend --trace thread,lock
```

**Thread Watchdog (Debug Builds):**

When built with `ENABLE_TRACE`, the thread watchdog monitors lock hold times:

```bash
# Build with trace
./configure --enable-trace
make

# Run with thread debugging
tvheadend --thrdebug 1

# Watchdog reports:
# - Locks held longer than threshold (default 15 seconds)
# - Potential deadlocks
# - Lock contention
```

**Deadlock Detection:**

When a deadlock is detected, Tvheadend writes `mutex-deadlock.txt`:

```
REASON: deadlock (src/service.c:456)
mutex 0x7f8a4c001234 locked in: src/service.c:123 (thread 12345)
mutex 0x7f8a4c001234   waiting in: src/streaming.c:789 (thread 12346)
mutex 0x7f8a4c005678 other in: src/streaming.c:750 (thread 12346)
mutex 0x7f8a4c005678   waiting in: src/service.c:130 (thread 12345)

Thread 12345 backtrace:
  #0  pthread_mutex_lock
  #1  tvh_mutex_lock at tvh_thread.c:234
  #2  service_start at service.c:130
  ...

Thread 12346 backtrace:
  #0  pthread_mutex_lock
  #1  tvh_mutex_lock at tvh_thread.c:234
  #2  streaming_target_deliver at streaming.c:789
  ...
```

**Analyzing Deadlock Reports:**

1. Identify the circular wait: Thread A waits for lock held by Thread B, Thread B waits for lock held by Thread A
2. Check lock ordering: Verify locks are acquired in correct order (global_lock before s_stream_mutex)
3. Review code paths: Examine the source locations in the backtrace
4. Fix lock ordering or use tasklets to break the cycle

**Using GDB for Thread Analysis:**

```bash
# Attach GDB to running process
gdb -p $(pidof tvheadend)

# List all threads
(gdb) info threads

# Switch to specific thread
(gdb) thread 5

# Show backtrace
(gdb) bt

# Show backtrace for all threads
(gdb) thread apply all bt

# Show local variables
(gdb) info locals

# Continue execution
(gdb) continue

# Detach without killing process
(gdb) detach
```

**Analyzing Thread Dumps:**

```bash
# Generate thread dump (send SIGQUIT)
kill -QUIT $(pidof tvheadend)

# Thread dump written to stderr/log shows:
# - All threads and their states
# - Current function for each thread
# - Lock ownership
```

**Lock Assertions:**

Add lock assertions to verify locking requirements:

```c
#include "lock.h"

void function_requiring_global_lock(void)
{
  // Verify global_lock is held
  lock_assert(&global_lock);
  
  // ... operations requiring global_lock ...
}

void function_requiring_stream_mutex(service_t *s)
{
  // Verify s_stream_mutex is held
  lock_assert(&s->s_stream_mutex);
  
  // ... streaming operations ...
}
```

Lock assertions help catch locking bugs early during development.

#### 21.2.4 Streaming Analysis

Debugging streaming issues requires understanding the data flow and monitoring streaming state.

**Enable Streaming Debug:**

```bash
# Enable streaming subsystem debug
tvheadend --debug streaming,service,subscription

# For detailed packet flow
tvheadend --trace streaming
```

**Monitor Streaming Status:**

Check service streaming status flags:

```c
// In code - check streaming status
if (s->s_streaming_status & TSS_PACKETS) {
  tvhdebug(LS_SERVICE, "service delivering packets");
}

if (s->s_streaming_status & TSS_GRACEPERIOD) {
  tvhdebug(LS_SERVICE, "service in grace period");
}

if (s->s_streaming_status & TSS_ERRORS) {
  tvherror(LS_SERVICE, "service has errors: 0x%x", 
           s->s_streaming_status);
}
```

**Streaming Status Flags:**

Progress flags (good):
- `TSS_INPUT_HARDWARE` - Hardware input active
- `TSS_INPUT_SERVICE` - Service input active
- `TSS_MUX_PACKETS` - Receiving mux packets
- `TSS_PACKETS` - Receiving service packets

Error flags (problems):
- `TSS_GRACEPERIOD` - Testing service quality
- `TSS_NO_DESCRAMBLER` - No descrambler available
- `TSS_NO_ACCESS` - Access denied
- `TSS_TIMEOUT` - Data timeout
- `TSS_TUNING` - Tuning failure

**Debug Streaming Targets:**

Add debug logging to streaming target callbacks:

```c
static void
my_streaming_target_input(void *opaque, streaming_message_t *sm)
{
  my_target_t *mt = opaque;
  
  tvhtrace(LS_STREAMING, "received message type %d", sm->sm_type);
  
  switch (sm->sm_type) {
  case SMT_START:
    tvhdebug(LS_STREAMING, "stream starting");
    // Log component list
    elementary_stream_t *st;
    TAILQ_FOREACH(st, &sm->sm_data, es_link) {
      tvhdebug(LS_STREAMING, "  stream %d: %s", st->es_index,
               streaming_component_type2txt(st->es_type));
    }
    break;
    
  case SMT_PACKET:
    th_pkt_t *pkt = sm->sm_data;
    tvhtrace(LS_STREAMING, "packet: stream=%d pts=%"PRId64" size=%zu",
             pkt->pkt_componentindex, pkt->pkt_pts,
             pkt->pkt_payload ? pkt->pkt_payload->pb_size : 0);
    break;
    
  case SMT_STOP:
    tvhdebug(LS_STREAMING, "stream stopping: code=%d (%s)",
             sm->sm_code, streaming_code2txt(sm->sm_code));
    break;
  }
  
  streaming_msg_free(sm);
}
```

**Monitor Subscription State:**

```c
// Check subscription state
const char *state_str[] = {
  [SUBSCRIPTION_IDLE] = "IDLE",
  [SUBSCRIPTION_TESTING_SERVICE] = "TESTING",
  [SUBSCRIPTION_GOT_SERVICE] = "RUNNING",
  [SUBSCRIPTION_BAD_SERVICE] = "BAD",
  [SUBSCRIPTION_ZOMBIE] = "ZOMBIE",
};

tvhdebug(LS_SUBSCRIPTION, "subscription %d state: %s",
         sub->ths_id, state_str[sub->ths_state]);
```

**Packet Flow Analysis:**

Track packet flow through the system:

```c
// At input
tvhtrace(LS_INPUT, "received %zu bytes from input", len);

// At service
tvhtrace(LS_SERVICE, "service %s: delivering packet pts=%"PRId64,
         s->s_nicename, pkt->pkt_pts);

// At descrambler
tvhtrace(LS_DESCRAMBLER, "descrambling packet: scrambled=%d",
         pkt->pkt_err & PKT_ERR_SCRAMBLED);

// At output
tvhtrace(LS_HTSP, "sending packet to client: size=%zu",
         pkt->pkt_payload->pb_size);
```

**Web UI Debugging:**

Use the web UI to monitor streaming:
1. Configuration → Debugging → Enable debug logging
2. Status → Subscriptions - View active subscriptions
3. Status → Stream - View streaming statistics
4. Status → Connections - View client connections

**Streaming Statistics:**

Monitor streaming statistics in code:

```c
// Service statistics
tvhdebug(LS_SERVICE, "service %s stats: packets=%"PRId64" bytes=%"PRId64,
         s->s_nicename, s->s_stat_packets, s->s_stat_bytes);

// Subscription statistics
tvhdebug(LS_SUBSCRIPTION, "subscription %d: in=%"PRId64" out=%"PRId64,
         sub->ths_id, sub->ths_bytes_in, sub->ths_bytes_out);

// Input statistics
tvhdebug(LS_INPUT, "input stats: signal=%d snr=%d ber=%d unc=%d",
         stats->signal, stats->snr, stats->ber, stats->unc);
```

#### 21.2.5 Using External Tools

**Valgrind for Memory Debugging:**

```bash
# Run with Valgrind
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --log-file=valgrind.log \
         --suppressions=support/valgrind.supp \
         ./build.linux/tvheadend -C -c /tmp/tvh-test

# Check for errors
grep "ERROR SUMMARY" valgrind.log
grep "definitely lost" valgrind.log
grep "Invalid" valgrind.log
```

**AddressSanitizer (ASan):**

```bash
# Build with ASan
./configure --cc=clang \
            CFLAGS="-fsanitize=address -fno-omit-frame-pointer -g"
make

# Run (ASan errors printed to stderr)
./build.linux/tvheadend -C -c /tmp/tvh-test

# ASan detects:
# - Heap/stack buffer overflows
# - Use-after-free
# - Double-free
# - Memory leaks
```

**ThreadSanitizer (TSan):**

```bash
# Build with TSan
./configure --cc=clang \
            CFLAGS="-fsanitize=thread -g"
make

# Run (TSan reports data races)
./build.linux/tvheadend -C -c /tmp/tvh-test
```

**strace for System Call Tracing:**

```bash
# Trace system calls
strace -f -o strace.log ./build.linux/tvheadend -C -c /tmp/tvh-test

# Trace specific calls
strace -e open,read,write -f ./build.linux/tvheadend

# Trace file operations
strace -e trace=file -f ./build.linux/tvheadend

# Attach to running process
strace -p $(pidof tvheadend)
```

**ltrace for Library Call Tracing:**

```bash
# Trace library calls
ltrace -f -o ltrace.log ./build.linux/tvheadend -C -c /tmp/tvh-test

# Trace specific libraries
ltrace -l libavcodec.so -f ./build.linux/tvheadend
```

**perf for Performance Analysis:**

```bash
# Record performance data
perf record -g ./build.linux/tvheadend -C -c /tmp/tvh-test

# Analyze performance
perf report

# Show hotspots
perf top -p $(pidof tvheadend)

# Trace specific events
perf stat -e cache-misses,cache-references ./build.linux/tvheadend
```

**Core Dump Analysis:**

```bash
# Enable core dumps
ulimit -c unlimited

# After crash, analyze core dump
gdb ./build.linux/tvheadend core

# In GDB:
(gdb) bt              # Backtrace
(gdb) info threads    # All threads
(gdb) thread apply all bt  # All backtraces
(gdb) frame 3         # Switch to frame
(gdb) print variable  # Examine variable
(gdb) info locals     # Local variables
```


### 21.3 Code Style and Conventions

Maintaining consistent code style and following established conventions makes the codebase easier to read, understand, and maintain. This section documents the coding standards used in Tvheadend.

#### 21.3.1 Naming Conventions

Tvheadend follows consistent naming patterns that make code more readable and help identify the purpose and scope of identifiers.

**Function Names:**

Functions use lowercase with underscores, prefixed by subsystem name:

```c
// Pattern: subsystem_action()
service_start()
service_stop()
subscription_create()
channel_find_by_uuid()
dvr_entry_create()
epg_broadcast_find_by_time()

// Static (file-local) functions
static void service_internal_helper(service_t *s);
static int subscription_validate_params(htsmsg_t *args);
```

**Structure Names:**

Structures use lowercase with underscores, suffixed with `_t`:

```c
// Pattern: subsystem_t or subsystem_object_t
typedef struct service service_t;
typedef struct th_subscription th_subscription_t;
typedef struct channel channel_t;
typedef struct dvr_entry dvr_entry_t;
typedef struct streaming_message streaming_message_t;
```

**Structure Member Names:**

Members are prefixed with abbreviated structure name:

```c
typedef struct service {
  idnode_t s_id;              // s_ prefix for service
  char *s_nicename;
  int s_status;
  int s_refcount;
  streaming_pad_t s_streaming_pad;
  // ...
} service_t;

typedef struct channel {
  idnode_t ch_id;             // ch_ prefix for channel
  char *ch_name;
  uint32_t ch_number;
  // ...
} channel_t;

typedef struct th_subscription {
  int ths_id;                 // ths_ prefix for th_subscription
  int ths_state;
  int ths_weight;
  // ...
} th_subscription_t;
```

**Global Variables:**

Global variables use lowercase with underscores:

```c
// Global mutexes
tvh_mutex_t global_lock;
tvh_mutex_t mtimer_lock;

// Global lists
service_list_t service_all;
channel_list_t channel_list;

// Global state
int tvheadend_running;
```

**Constants and Macros:**

Constants and macros use uppercase with underscores:

```c
// Constants
#define MAX_CHANNELS 1000
#define DEFAULT_TIMEOUT 30

// Macros
#define SERVICE_IS_RUNNING(s) ((s)->s_status == SERVICE_RUNNING)
#define CHANNEL_ENABLED(ch) ((ch)->ch_enabled)

// Enum values
typedef enum {
  SERVICE_IDLE,
  SERVICE_RUNNING,
  SERVICE_ZOMBIE,
} service_status_t;
```

**Type Names:**

Type names use lowercase with underscores, suffixed with `_t`:

```c
typedef int (*callback_t)(void *opaque);
typedef enum service_status service_status_t;
typedef struct streaming_message streaming_message_t;
```

#### 21.3.2 Locking Patterns

Proper locking is critical for thread safety. Follow these established patterns.

**Basic Lock Pattern:**

```c
// ✅ CORRECT: Basic lock/unlock pattern
void update_service_name(service_t *s, const char *name)
{
  tvh_mutex_lock(&global_lock);
  
  // Critical section
  free(s->s_nicename);
  s->s_nicename = strdup(name);
  
  tvh_mutex_unlock(&global_lock);
}
```

**Lock with Early Return:**

```c
// ✅ CORRECT: Unlock before all returns
int process_service(const char *uuid)
{
  service_t *s;
  int result;
  
  tvh_mutex_lock(&global_lock);
  
  s = service_find_by_uuid(uuid);
  if (!s) {
    tvh_mutex_unlock(&global_lock);
    return -ENOENT;
  }
  
  if (s->s_status == SERVICE_ZOMBIE) {
    tvh_mutex_unlock(&global_lock);
    return -EINVAL;
  }
  
  result = service_do_something(s);
  
  tvh_mutex_unlock(&global_lock);
  return result;
}
```

**Lock with Cleanup (goto pattern):**

```c
// ✅ CORRECT: Using goto for cleanup
int complex_operation(const char *uuid)
{
  service_t *s = NULL;
  char *buffer = NULL;
  int fd = -1;
  int ret = -1;
  
  tvh_mutex_lock(&global_lock);
  
  s = service_find_by_uuid(uuid);
  if (!s) {
    ret = -ENOENT;
    goto unlock;
  }
  
  service_ref(s);
  tvh_mutex_unlock(&global_lock);
  
  // Operations without lock
  buffer = malloc(1024);
  if (!buffer) {
    ret = -ENOMEM;
    goto cleanup;
  }
  
  fd = open("/some/file", O_RDONLY);
  if (fd < 0) {
    ret = -errno;
    goto cleanup;
  }
  
  // Success
  ret = 0;
  
cleanup:
  if (fd >= 0) close(fd);
  free(buffer);
  
  if (s) {
    tvh_mutex_lock(&global_lock);
    service_unref(s);
unlock:
    tvh_mutex_unlock(&global_lock);
  }
  
  return ret;
}
```

**Hierarchical Locking:**

```c
// ✅ CORRECT: Acquire global_lock before s_stream_mutex
void deliver_packet_to_service(service_t *s, th_pkt_t *pkt)
{
  // Get reference with global_lock
  tvh_mutex_lock(&global_lock);
  if (s->s_status != SERVICE_RUNNING) {
    tvh_mutex_unlock(&global_lock);
    pkt_ref_dec(pkt);
    return;
  }
  service_ref(s);
  tvh_mutex_unlock(&global_lock);
  
  // Now acquire s_stream_mutex
  tvh_mutex_lock(&s->s_stream_mutex);
  streaming_pad_deliver(&s->s_streaming_pad, 
                       streaming_msg_create_pkt(pkt));
  tvh_mutex_unlock(&s->s_stream_mutex);
  
  // Release reference
  tvh_mutex_lock(&global_lock);
  service_unref(s);
  tvh_mutex_unlock(&global_lock);
  
  pkt_ref_dec(pkt);
}
```

**Lock Assertions:**

```c
// ✅ CORRECT: Document locking requirements
void service_internal_function(service_t *s)
{
  // Assert that caller holds required lock
  lock_assert(&global_lock);
  
  // ... operations requiring global_lock ...
}

void streaming_internal_function(service_t *s)
{
  // Assert that caller holds stream mutex
  lock_assert(&s->s_stream_mutex);
  
  // ... streaming operations ...
}
```

#### 21.3.3 Error Handling Patterns

Consistent error handling makes code more reliable and easier to debug.

**Return Value Convention:**

```c
// Return 0 on success, negative errno on error
int create_something(const char *name)
{
  if (!name || !*name)
    return -EINVAL;  // Invalid argument
  
  if (already_exists(name))
    return -EEXIST;  // Already exists
  
  void *obj = malloc(sizeof(something_t));
  if (!obj)
    return -ENOMEM;  // Out of memory
  
  if (initialize(obj) < 0) {
    free(obj);
    return -EIO;  // I/O error
  }
  
  return 0;  // Success
}
```

**Common Error Codes:**

```c
// Standard errno values
-EINVAL   // Invalid argument
-ENOENT   // No such entry/file
-EEXIST   // Already exists
-ENOMEM   // Out of memory
-EPERM    // Permission denied
-EACCES   // Access denied
-EIO      // I/O error
-ETIMEDOUT // Timeout
-EAGAIN   // Try again
-EBUSY    // Resource busy
```

**Error Logging:**

```c
// ✅ CORRECT: Log errors with context
int open_device(const char *path)
{
  int fd = open(path, O_RDWR);
  if (fd < 0) {
    tvherror(LS_MYSUBSYS, "failed to open device %s: %s",
             path, strerror(errno));
    return -errno;
  }
  
  return fd;
}

// ✅ CORRECT: Log warnings for recoverable errors
int retry_operation(void)
{
  int ret = try_operation();
  if (ret < 0) {
    tvhwarn(LS_MYSUBSYS, "operation failed, will retry: %s",
            strerror(-ret));
    return ret;
  }
  
  return 0;
}
```

**Cleanup on Error:**

```c
// ✅ CORRECT: Clean up all resources on error
int create_complex_object(const char *name)
{
  my_object_t *obj = NULL;
  char *buffer = NULL;
  int fd = -1;
  int ret = -1;
  
  obj = calloc(1, sizeof(my_object_t));
  if (!obj) {
    ret = -ENOMEM;
    goto error;
  }
  
  obj->name = strdup(name);
  if (!obj->name) {
    ret = -ENOMEM;
    goto error;
  }
  
  buffer = malloc(1024);
  if (!buffer) {
    ret = -ENOMEM;
    goto error;
  }
  
  fd = open("/some/file", O_RDONLY);
  if (fd < 0) {
    ret = -errno;
    tvherror(LS_MYSUBSYS, "failed to open file: %s", strerror(errno));
    goto error;
  }
  
  // Success
  ret = 0;
  goto done;
  
error:
  // Cleanup on error
  if (obj) {
    free(obj->name);
    free(obj);
    obj = NULL;
  }
  
done:
  // Cleanup always
  free(buffer);
  if (fd >= 0) close(fd);
  
  return ret;
}
```

#### 21.3.4 Memory Allocation Patterns

Proper memory management prevents leaks and use-after-free bugs.

**Allocation and Initialization:**

```c
// ✅ CORRECT: Use calloc for zero-initialization
my_object_t *obj = calloc(1, sizeof(my_object_t));
if (!obj) {
  tvherror(LS_MYSUBSYS, "out of memory");
  return -ENOMEM;
}

// Initialize fields
obj->name = strdup(name);
obj->enabled = 1;
obj->created = time(NULL);

// Alternative: malloc + memset
my_object_t *obj = malloc(sizeof(my_object_t));
if (!obj) return -ENOMEM;
memset(obj, 0, sizeof(my_object_t));
```

**String Management:**

```c
// ✅ CORRECT: Use tvh_str_set for string assignment
typedef struct {
  char *name;
  char *description;
} my_object_t;

void update_name(my_object_t *obj, const char *new_name)
{
  // Safe: Frees old string, duplicates new string
  tvh_str_set(&obj->name, new_name);
}

void update_description(my_object_t *obj, const char *new_desc)
{
  // Only updates if different, returns 1 if changed
  if (tvh_str_update(&obj->description, new_desc)) {
    tvhdebug(LS_MYSUBSYS, "description changed");
  }
}

void free_object(my_object_t *obj)
{
  free(obj->name);
  free(obj->description);
  free(obj);
}
```

**Reference Counting:**

```c
// ✅ CORRECT: Reference counting pattern
typedef struct {
  int refcount;
  char *data;
} refcounted_object_t;

refcounted_object_t *object_create(void)
{
  refcounted_object_t *obj = calloc(1, sizeof(refcounted_object_t));
  obj->refcount = 1;  // Initial reference
  return obj;
}

void object_ref(refcounted_object_t *obj)
{
  lock_assert(&global_lock);  // Must hold lock
  obj->refcount++;
}

void object_unref(refcounted_object_t *obj)
{
  lock_assert(&global_lock);  // Must hold lock
  
  if (--obj->refcount == 0) {
    // Last reference released, free object
    free(obj->data);
    free(obj);
  }
}

// Usage
void store_reference(refcounted_object_t *obj)
{
  tvh_mutex_lock(&global_lock);
  object_ref(obj);  // Increment before storing
  my_stored_object = obj;
  tvh_mutex_unlock(&global_lock);
}

void release_reference(void)
{
  tvh_mutex_lock(&global_lock);
  if (my_stored_object) {
    object_unref(my_stored_object);  // Decrement, may free
    my_stored_object = NULL;
  }
  tvh_mutex_unlock(&global_lock);
}
```

**Packet Handling:**

```c
// ✅ CORRECT: Packet reference counting
void process_and_forward_packet(th_pkt_t *pkt, target_list_t *targets)
{
  target_t *target;
  
  // Process packet
  analyze_packet(pkt);
  
  // Forward to each target (increment reference for each)
  LIST_FOREACH(target, targets, link) {
    pkt_ref_inc(pkt);
    target_deliver(target, pkt);
  }
  
  // Release our reference
  pkt_ref_dec(pkt);  // May free packet
  
  // Don't access pkt after this point!
}
```

**List Management:**

```c
// ✅ CORRECT: List insertion and removal
typedef struct my_item {
  LIST_ENTRY(my_item) link;
  char *name;
} my_item_t;

LIST_HEAD(, my_item) item_list;

void add_item(const char *name)
{
  my_item_t *item = calloc(1, sizeof(my_item_t));
  item->name = strdup(name);
  
  tvh_mutex_lock(&global_lock);
  LIST_INSERT_HEAD(&item_list, item, link);
  tvh_mutex_unlock(&global_lock);
}

void remove_item(my_item_t *item)
{
  tvh_mutex_lock(&global_lock);
  LIST_REMOVE(item, link);
  tvh_mutex_unlock(&global_lock);
  
  // Free after removing from list
  free(item->name);
  free(item);
}

void remove_all_items(void)
{
  my_item_t *item, *next;
  
  tvh_mutex_lock(&global_lock);
  
  // Safe iteration with removal
  for (item = LIST_FIRST(&item_list); item; item = next) {
    next = LIST_NEXT(item, link);
    LIST_REMOVE(item, link);
    free(item->name);
    free(item);
  }
  
  tvh_mutex_unlock(&global_lock);
}
```

#### 21.3.5 Code Formatting

Consistent formatting improves readability.

**Indentation:**

```c
// Use 2 spaces for indentation (not tabs)
void function(void)
{
  if (condition) {
    do_something();
    if (nested_condition) {
      do_nested_thing();
    }
  }
}
```

**Braces:**

```c
// ✅ CORRECT: Opening brace on same line for control structures
if (condition) {
  statement;
}

while (condition) {
  statement;
}

for (i = 0; i < n; i++) {
  statement;
}

// ✅ CORRECT: Opening brace on new line for functions
void function(void)
{
  statement;
}

// ✅ CORRECT: Always use braces, even for single statements
if (condition) {
  single_statement();
}

// ❌ WRONG: No braces for single statement
if (condition)
  single_statement();  // Avoid this
```

**Line Length:**

```c
// Keep lines under 80 characters when practical
// Break long lines at logical points

// ✅ CORRECT: Break long function calls
result = very_long_function_name(first_parameter,
                                 second_parameter,
                                 third_parameter,
                                 fourth_parameter);

// ✅ CORRECT: Break long conditions
if (very_long_condition_one &&
    very_long_condition_two &&
    very_long_condition_three) {
  statement;
}
```

**Comments:**

```c
// ✅ CORRECT: Use // for single-line comments
// This is a single-line comment

// ✅ CORRECT: Use /* */ for multi-line comments
/*
 * This is a multi-line comment
 * explaining something complex
 */

/**
 * Function documentation comment
 * 
 * @param s    Service to process
 * @param name New name for service
 * @return 0 on success, negative errno on error
 */
int service_set_name(service_t *s, const char *name)
{
  // Implementation
}
```

**Whitespace:**

```c
// ✅ CORRECT: Space after keywords, around operators
if (condition) {
  x = a + b;
  y = c * d;
}

// ✅ CORRECT: No space after function names
function(arg1, arg2);

// ✅ CORRECT: Space after commas
function(arg1, arg2, arg3);

// ✅ CORRECT: No space inside parentheses
if (condition) {
  function(arg);
}
```

#### 21.3.6 Documentation

Document your code to help other developers understand it.

**Function Documentation:**

```c
/**
 * Create a new service instance
 *
 * Creates and initializes a new service with the given parameters.
 * The service is added to the global service list and assigned a UUID.
 *
 * @param name        Service name (will be duplicated)
 * @param type        Service type (STYPE_STD, STYPE_RAW)
 * @param source_type Source type (MPEG_TS, MPEG_PS, OTHER)
 * @return Pointer to created service, or NULL on error
 *
 * @note Caller must hold global_lock
 * @note Service is created with refcount=1
 */
service_t *
service_create(const char *name, int type, int source_type)
{
  // Implementation
}
```

**Complex Algorithm Documentation:**

```c
/**
 * Match EPG broadcast to existing episode
 *
 * Uses fuzzy matching algorithm to find existing episode that matches
 * the broadcast. Matching criteria (in order of priority):
 * 1. Exact episode number match (season + episode)
 * 2. Original air date match
 * 3. Title and description similarity (> 80%)
 * 4. Duration match (within 5 minutes)
 *
 * @param broadcast  Broadcast to match
 * @param brand      Brand/series to search within
 * @return Matching episode, or NULL if no match found
 */
static epg_episode_t *
epg_match_episode(epg_broadcast_t *broadcast, epg_brand_t *brand)
{
  // Complex matching logic
}
```

**TODO and FIXME Comments:**

```c
// TODO: Implement retry logic for network errors
// FIXME: Race condition when service is destroyed during streaming
// XXX: Temporary workaround for hardware bug, remove when fixed
// HACK: This is ugly but works, needs refactoring
```

**Header File Documentation:**

```c
/**
 * @file service.h
 * @brief Service management subsystem
 *
 * This file defines the service structure and related functions.
 * Services represent TV channels or radio stations available from
 * input sources.
 */

#ifndef __TVH_SERVICE_H__
#define __TVH_SERVICE_H__

// Declarations

#endif /* __TVH_SERVICE_H__ */
```

---

[← Previous](20-Known-Issues-Pitfalls.md) | [Table of Contents](00-TOC.md) | [Next →](22-Version-History.md)

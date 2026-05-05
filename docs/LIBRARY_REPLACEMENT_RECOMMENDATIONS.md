# Library Replacement Recommendations for Tvheadend

This document provides a comprehensive analysis of custom implementations in the Tvheadend codebase that could potentially be replaced by well-established external libraries. Each recommendation includes the current implementation, suggested library alternatives, and considerations for migration.

---

## Table of Contents

1. [JSON Parsing and Serialization](#1-json-parsing-and-serialization)
2. [XML Parsing](#2-xml-parsing)
3. [URL Parsing](#3-url-parsing)
4. [UUID Generation and Handling](#4-uuid-generation-and-handling)
5. [HTTP Client/Server](#5-http-clientserver)
6. [Buffer Management](#6-buffer-management)
7. [Message/Data Serialization](#7-messagedata-serialization)
8. [Cron Expression Parsing](#8-cron-expression-parsing)
9. [String Manipulation](#9-string-manipulation)
10. [Character Encoding Conversion](#10-character-encoding-conversion)
11. [Compression (zlib wrapper)](#11-compression-zlib-wrapper)
12. [Configuration Management](#12-configuration-management)
13. [Logging System](#13-logging-system)
14. [Thread Pool and Async Operations](#14-thread-pool-and-async-operations)
15. [File System Monitoring](#15-file-system-monitoring)
16. [Regular Expression Handling](#16-regular-expression-handling)
17. [M3U Parsing](#17-m3u-parsing)
18. [Double/Float String Conversion](#18-doublefloat-string-conversion)

---

## 1. JSON Parsing and Serialization

### Current Implementation
- **Files**: `src/misc/json.c`, `src/misc/json.h`, `src/htsmsg_json.c`, `src/htsmsg_json.h`
- **Description**: Custom JSON parser/serializer integrated with the htsmsg message format

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **cJSON** | MIT | ~30KB | Lightweight, widely used, easy to integrate |
| **json-c** | MIT | ~250KB | Full-featured, stable, good performance |
| **Jansson** | MIT | ~200KB | Well-documented, easy API, actively maintained |
| **yyjson** | MIT | ~50KB | Fastest JSON parser for C, read-only support |
| **parson** | MIT | ~30KB | Single-file, simple API |

### Recommendation
**cJSON** or **Jansson** would be the best choices:
- cJSON is extremely lightweight and easy to integrate
- Jansson provides better error handling and documentation
- Both are MIT licensed and actively maintained

### Migration Considerations
- The current implementation is tightly integrated with `htsmsg_t` structure
- Would need adapter functions to convert between library format and htsmsg
- Consider keeping htsmsg as the internal format and using JSON library only for I/O

---

## 2. XML Parsing

### Current Implementation
- **Files**: `src/htsmsg_xml.c`, `src/htsmsg_xml.h`
- **Description**: Custom XML parser supporting UTF-8, ISO-8859-1, CDATA, comments, namespaces, and basic XPath

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **libxml2** | MIT | ~2MB | Full-featured, industry standard |
| **expat** | MIT | ~300KB | SAX parser, event-based, fast |
| **mxml** | Apache 2.0 | ~150KB | Lightweight, easy API |
| **pugixml** | MIT | ~150KB | C++, fast DOM parser |
| **yxml** | MIT | ~10KB | Ultra-lightweight, streaming parser |

### Recommendation
**expat** is the best choice:
- Lightweight and fast
- Event-based parsing fits TV stream processing
- Widely used, stable, well-tested
- MIT licensed

### Migration Considerations
- Current implementation has custom XPath support
- Would need to implement XPath separately or use libxml2 for full XPath
- Consider expat for basic parsing, libxml2 for complex XPath needs

---

## 3. URL Parsing

### Current Implementation
- **Files**: `src/url.c`, `src/url.h`
- **Description**: URL parsing using regex or liburiparser (already conditional)
- **Note**: Already has optional liburiparser support via `ENABLE_URIPARSER`

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **liburiparser** | BSD-3 | ~100KB | Already optionally used, full RFC 3986 support |
| **curl** (URL API) | MIT | N/A | If already using libcurl |
| **yuarel** | MIT | ~5KB | Single-file, minimal |

### Recommendation
**Make liburiparser mandatory** instead of optional:
- Already integrated and tested
- Full RFC 3986 compliance
- Removes maintenance burden of regex fallback

### Migration Considerations
- Simply make `ENABLE_URIPARSER` always enabled
- Remove the regex-based fallback code
- Minimal effort required

---

## 4. UUID Generation and Handling

### Current Implementation
- **Files**: `src/uuid.c`, `src/uuid.h`
- **Description**: Custom UUID generation using `/dev/urandom`, hex conversion utilities

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **libuuid** | BSD-3 | ~50KB | Standard on Linux, part of util-linux |
| **uuid4** | MIT | ~5KB | Single-header, UUID v4 only |
| **ossp-uuid** | MIT | ~200KB | Full UUID versions support |

### Recommendation
**libuuid** (from util-linux):
- Standard on all Linux systems
- Supports UUID v1, v3, v4, v5
- Thread-safe
- Already a dependency on most systems

### Migration Considerations
- Current implementation is simple and functional
- libuuid would provide proper UUID versioning
- Low priority change - current implementation is adequate

---

## 5. HTTP Client/Server

### Current Implementation
- **Files**: `src/http.c`, `src/httpc.c`, `src/tcp.c`
- **Description**: Custom HTTP/1.1 client and server implementation

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **libcurl** | MIT | ~1MB | Industry standard, full HTTP/2/3 support |
| **libmicrohttpd** | LGPL | ~300KB | Embedded HTTP server |
| **libevent** | BSD-3 | ~500KB | Event-driven networking |
| **mongoose** | GPL/Commercial | ~300KB | Embedded web server |

### Recommendation
**Not recommended to replace entirely**:
- Current implementation is well-tuned for TV streaming needs
- However, consider using **libcurl** for outbound HTTP requests (EPG fetching, etc.)
- Keep custom server for HTSP protocol needs

### Migration Considerations
- HTTP client portion could use libcurl
- Server portion is specialized for streaming
- Partial migration is possible

---

## 6. Buffer Management

### Current Implementation
- **Files**: `src/htsbuf.c`, `src/htsbuf.h`, `src/sbuf.h`
- **Description**: Custom queue-based buffer implementation for streaming data

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **klib** (kvec) | MIT | ~20KB | Header-only dynamic arrays |
| **uthash** (utarray) | BSD | ~20KB | Header-only arrays |
| **GLib** (GByteArray) | LGPL | Large | Full featured |

### Recommendation
**Keep current implementation**:
- Custom buffer is optimized for streaming use case
- Minimal overhead, well-tested
- No significant benefit from migration

---

## 7. Message/Data Serialization

### Current Implementation
- **Files**: `src/htsmsg.c`, `src/htsmsg_binary.c`, `src/htsmsg_binary2.c`
- **Description**: Custom hierarchical message format (htsmsg) with binary serialization

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **Protocol Buffers** | BSD-3 | ~2MB | Google's serialization, requires schema |
| **MessagePack** | BSL-1.0 | ~100KB | Binary JSON, schemaless |
| **FlatBuffers** | Apache 2.0 | ~1MB | Zero-copy access |
| **CBOR (tinycbor)** | MIT | ~50KB | Binary JSON, RFC 8949 |

### Recommendation
**Keep current implementation for internal use**:
- htsmsg is deeply integrated throughout codebase
- Optimized for TV metadata structures
- Consider **MessagePack** or **CBOR** for external interoperability if needed

### Migration Considerations
- Very high effort to replace
- Current implementation is well-suited to the application
- Not recommended for replacement

---

## 8. Cron Expression Parsing

### Current Implementation
- **Files**: `src/cron.c`, `src/cron.h`
- **Description**: Custom cron expression parser for scheduling

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **ccronexpr** | Apache 2.0 | ~30KB | Full cron expression support |
| **libcron** | MIT | ~50KB | C++ cron library |
| **cron-parser** | MIT | ~20KB | Simple cron parser |

### Recommendation
**ccronexpr**:
- Full standard cron expression support
- Well-tested
- Small footprint

### Migration Considerations
- Current implementation is functional but limited
- Migration would add support for more cron features
- Medium priority

---

## 9. String Manipulation

### Current Implementation
- **Files**: `src/htsstr.c`, `src/tvh_string.h`
- **Description**: String escaping, argument parsing, substitution

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **bstrlib** | BSD | ~100KB | Better string library for C |
| **sds** | BSD-2 | ~30KB | Simple dynamic strings (Redis) |
| **GLib** (GString) | LGPL | Large | Full string utilities |

### Recommendation
**Keep current implementation**:
- Functions are simple and well-tested
- No significant benefit from migration
- Would require extensive code changes

---

## 10. Character Encoding Conversion

### Current Implementation
- **Files**: `src/intlconv.c`, `src/intlconv.h`
- **Description**: Character encoding conversion wrapper around iconv

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **glibc iconv** | LGPL | N/A | Standard on Linux (system-provided) |
| **libiconv** | LGPL | ~2MB | GNU iconv for non-glibc systems |
| **ICU** | Unicode License | ~25MB | Full Unicode support |
| **utf8proc** | MIT | ~200KB | UTF-8 specific |

### Recommendation
**Keep current implementation**:
- Already using system iconv (glibc iconv or libiconv) efficiently
- Custom wrapper in `intlconv.c` provides caching for performance
- No benefit from changing to a different encoding library

---

## 11. Compression (zlib wrapper)

### Current Implementation
- **Files**: `src/zlib.c`
- **Description**: Wrapper around zlib for gzip compression/decompression

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **zlib** | zlib License | ~200KB | Already used |
| **zlib-ng** | zlib License | ~200KB | Performance optimized fork |
| **lz4** | BSD-2 | ~100KB | Faster compression |
| **zstd** | BSD-3 | ~500KB | Better compression ratio |

### Recommendation
**Consider zlib-ng**:
- Drop-in replacement for zlib
- Better performance on modern CPUs
- Same API, minimal changes needed

### Migration Considerations
- zlib-ng is API compatible
- Could be a configure option
- Low risk change

---

## 12. Configuration Management

### Current Implementation
- **Files**: `src/settings.c`, `src/settings.h`, `src/config.c`
- **Description**: JSON-based configuration storage using htsmsg

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **libconfig** | LGPL | ~200KB | Human-readable config files |
| **inih** | BSD-3 | ~10KB | INI file parser |
| **tomlc99** | MIT | ~50KB | TOML parser |

### Recommendation
**Keep current implementation**:
- JSON-based config is flexible
- Works well with htsmsg system
- Migration would break existing configs

---

## 13. Logging System

### Current Implementation
- **Files**: `src/tvhlog.c`, `src/tvhlog.h`
- **Description**: Custom logging with subsystems, levels, file rotation

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **syslog** | N/A | N/A | Standard Unix logging |
| **log4c** | LGPL | ~200KB | Flexible logging framework |
| **zlog** | LGPL | ~100KB | High-performance logging |

### Recommendation
**Keep current implementation**:
- Well-suited to application needs
- Subsystem-based logging is important for debugging
- Would lose TV-specific features

---

## 14. Thread Pool and Async Operations

### Current Implementation
- **Files**: `src/tvh_thread.c`, `src/tvhpoll.c`
- **Description**: Custom thread management and epoll/kqueue wrapper

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **libevent** | BSD-3 | ~500KB | Event-driven I/O |
| **libuv** | MIT | ~500KB | Cross-platform async I/O |
| **libev** | BSD-2 | ~100KB | Lightweight event loop |

### Recommendation
**Consider libevent or libuv for new code**:
- Current implementation works but limits portability
- New features could use libuv for better cross-platform support
- Not recommended for full replacement due to deep integration

---

## 15. File System Monitoring

### Current Implementation
- **Files**: `src/fsmonitor.c`, `src/fsmonitor.h`
- **Description**: inotify-based file system monitoring

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **libfswatch** | GPL | ~500KB | Cross-platform file watching |
| **libuv** | MIT | ~500KB | Includes fs events |
| **inotify-tools** | GPL | ~100KB | Linux specific |

### Recommendation
**Keep current implementation for Linux**:
- inotify is efficient and well-suited
- Consider libuv if cross-platform support needed

---

## 16. Regular Expression Handling

### Current Implementation
- **Files**: `src/tvhregex.h` (wrapper header)
- **Description**: Wrapper around PCRE or POSIX regex

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **PCRE2** | BSD | ~1MB | Already optionally supported |
| **RE2** | BSD-3 | ~1MB | Google's safe regex |
| **TRE** | BSD-2 | ~200KB | POSIX-like, approximate matching |

### Recommendation
**Make PCRE2 mandatory**:
- Already conditionally supported
- Better performance than POSIX regex
- More features for EPG scraping

---

## 17. M3U Parsing

### Current Implementation
- **Files**: `src/misc/m3u.c`, `src/misc/m3u.h`
- **Description**: Custom M3U/M3U8 playlist parser

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **libm3u** | MIT | ~20KB | Dedicated M3U parser |
| **hls-parser** | Various | ~50KB | HLS specific parsing |

### Recommendation
**Keep current implementation**:
- M3U format is simple
- Current parser handles TV-specific extensions
- No significant benefit from migration

---

## 18. Double/Float String Conversion

### Current Implementation
- **Files**: `src/misc/dbl.c`, `src/misc/dbl.h`
- **Description**: Custom locale-independent float/string conversion

### Recommended Libraries

| Library | License | Size | Notes |
|---------|---------|------|-------|
| **double-conversion** | BSD-3 | ~100KB | Google's library |
| **ryu** | Apache/BSL | ~50KB | Fast float printing |
| **dragonbox** | Apache/BSL | ~30KB | Fastest float-to-string |

### Recommendation
**Consider ryu or dragonbox**:
- Generally faster than custom implementations (based on published benchmarks)
- Guaranteed correct rounding per IEEE 754
- Well-tested in production (used by V8, Swift, etc.)

### Migration Considerations
- Low risk, isolated functionality
- Potential performance improvement for JSON serialization

---

## Summary Priority Matrix

| Category | Current Quality | Migration Effort | Benefit | Priority |
|----------|----------------|------------------|---------|----------|
| JSON (cJSON/Jansson) | Good | Medium | Medium | **Medium** |
| XML (expat) | Good | High | Medium | **Low** |
| URL (liburiparser) | Excellent | Low | Low | **High** (make mandatory) |
| UUID (libuuid) | Good | Low | Low | **Low** |
| HTTP Client (libcurl) | Good | Medium | High | **Medium** |
| Cron (ccronexpr) | Fair | Low | Medium | **Medium** |
| Regex (PCRE2) | Good | Low | Medium | **High** (make mandatory) |
| Compression (zlib-ng) | Good | Low | Medium | **Medium** |
| Float conversion (ryu) | Good | Low | Medium | **Low** |

---

## Recommended Action Plan

### Phase 1 (Low Effort, High Value)
1. Make liburiparser mandatory (remove regex fallback)
2. Make PCRE2 mandatory for regex operations
3. Consider zlib-ng as optional high-performance alternative

### Phase 2 (Medium Effort)
1. Evaluate libcurl for HTTP client operations
2. Replace cron parser with ccronexpr
3. Evaluate cJSON for JSON parsing (with htsmsg adapter)

### Phase 3 (Future Consideration)
1. Consider libuv for new async features
2. Evaluate MessagePack/CBOR for external APIs
3. Consider expat for XML parsing improvements

---

## Notes

- Many custom implementations are well-suited to Tvheadend's specific needs
- The htsmsg system is central to the architecture and should be preserved
- Library replacements should be evaluated for both functionality and performance
- Consider backwards compatibility with existing configurations
- Some libraries (liburiparser, PCRE2, zlib) are already optionally supported

---

*Document generated: 2026-01-02*
*Based on analysis of Tvheadend source code*

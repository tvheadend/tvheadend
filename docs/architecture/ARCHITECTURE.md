# Tvheadend Architecture Documentation

**Version:** 1.0.0  
**Last Updated:** November 15, 2025  
**Status:** Initial Release

---

## About This Documentation

This architecture documentation has been organized into multiple subdocuments for easier navigation and maintenance. Please refer to the [Table of Contents](00-TOC.md) for the complete structure.

## Quick Links

- [Table of Contents](00-TOC.md) - Complete documentation structure
- [Introduction](01-Introduction.md) - About Tvheadend and this documentation
- [High-Level Architecture](02-High-Level-Architecture.md) - System overview and design principles
- [System Initialization](03-System-Initialization.md) - Startup sequence and main loop
- [Threading Model](04-Threading-Model.md) - Threads, locks, timers, and tasklets
- [Input Subsystem](05-Input-Subsystem.md) - Input sources and hardware management
- [Service Management](06-Service-Management.md) - Service lifecycle and operations
- [Streaming Architecture](07-Streaming-Architecture.md) - Data flow and streaming patterns
- [Profile System](08-Profile-System.md) - Stream processing and transcoding
- [Muxer System](09-Muxer-System.md) - Container formats and file writing
- [Subscription System](10-Subscription-System.md) - Client subscriptions and service selection
- [Timeshift Support](11-Timeshift-Support.md) - Buffer management and seek operations
- [DVR Subsystem](12-DVR-Subsystem.md) - Recording functionality
- [EPG Subsystem](13-EPG-Subsystem.md) - Electronic Program Guide
- [Descrambler Subsystem](14-Descrambler-Subsystem.md) - Conditional Access systems
- [HTTP/API Server](15-HTTP-API-Server.md) - Web interface and REST API
- [HTSP Server](16-HTSP-Server.md) - Native streaming protocol
- [SAT>IP Server](17-SATIP-Server.md) - SAT>IP protocol server
- [Access Control System](18-Access-Control-System.md) - Authentication and permissions
- [Configuration and Persistence](19-Configuration-Persistence.md) - Settings and storage
- [Known Issues and Pitfalls](20-Known-Issues-Pitfalls.md) - Common problems and solutions
- [Development Guidelines](21-Development-Guidelines.md) - Best practices for contributors
- [Version History](22-Version-History.md) - Documentation changelog
- [Documentation Maintenance](23-Documentation-Maintenance.md) - How to keep docs updated

---

## Document Structure

The architecture documentation is organized into 23 main sections:

1. **Introduction** - Project overview, audience, and how to use this documentation
2. **High-Level Architecture** - System overview, major subsystems, and design principles
3. **System Initialization** - Startup sequence, command-line processing, and shutdown
4. **Threading Model** - Threads, synchronization, timers, and tasklets
5. **Input Subsystem** - Hardware and network input sources
6. **Service Management** - TV service lifecycle and operations
7. **Streaming Architecture** - Data flow and streaming patterns
8. **Profile System** - Stream processing, transcoding, and filtering
9. **Muxer System** - Container formats and file writing
10. **Subscription System** - Client request handling and service selection
11. **Timeshift Support** - Buffer management and seek operations
12. **DVR Subsystem** - Recording, scheduling, and file management
13. **EPG Subsystem** - Electronic Program Guide data and matching
14. **Descrambler Subsystem** - Conditional Access and decryption
15. **HTTP/API Server** - Web interface and REST API
16. **HTSP Server** - Native streaming protocol server
17. **SAT>IP Server** - SAT>IP protocol server implementation
18. **Access Control System** - Authentication and permission management
19. **Configuration and Persistence** - Settings storage and management
20. **Known Issues and Pitfalls** - Common problems and how to avoid them
21. **Development Guidelines** - Best practices for contributors
22. **Version History** - Documentation changelog
23. **Documentation Maintenance** - Guidelines for keeping documentation current

---

## Current Status

**Completed Sections:**
- ✅ All 23 sections completed
- ✅ Cross-references and links added
- ✅ Source file references included
- ✅ Version history documented
- ✅ Maintenance guidelines provided

---

## For Developers

If you're new to the Tvheadend codebase, we recommend reading the sections in order:

1. Start with [Introduction](01-Introduction.md) to understand the project scope
2. Read [High-Level Architecture](02-High-Level-Architecture.md) for the big picture
3. Study [System Initialization](03-System-Initialization.md) to understand startup
4. **Carefully review** [Threading Model](04-Threading-Model.md) - this is critical for writing correct code
5. Then explore specific subsystems based on your area of work

---

**Note:** This is a living document. As the Tvheadend codebase evolves, this documentation should be updated to reflect architectural changes, new subsystems, and lessons learned. See [Documentation Maintenance](23-Documentation-Maintenance.md) for guidelines on keeping this documentation current.

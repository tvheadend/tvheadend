# Tvheadend Architecture Documentation

**Version:** 1.0.0  
**Last Updated:** November 15, 2025  
**Status:** Initial Release

---

## Table of Contents

1. [Introduction](01-Introduction.md)
   - 1.1 About Tvheadend
   - 1.2 Purpose of This Document
   - 1.3 Intended Audience
   - 1.4 How to Use This Document
   - 1.5 Related Documentation

2. [High-Level Architecture](02-High-Level-Architecture.md)
   - 2.1 System Overview
   - 2.2 Architecture Diagram
   - 2.3 Core Design Principles
   - 2.4 Major Subsystems

3. [System Initialization and Main Loop](03-System-Initialization.md)
   - 3.1 Entry Point and Command-Line Processing
   - 3.2 Initialization Sequence
   - 3.3 Main Event Loop
   - 3.4 Shutdown Sequence

4. [Threading Model](04-Threading-Model.md)
   - 4.1 Thread Inventory
   - 4.2 Threading Architecture
   - 4.3 Synchronization Primitives
   - 4.4 Locking Hierarchy and Rules
   - 4.5 Timer Systems
   - 4.6 Tasklet System

5. [Input Subsystem](05-Input-Subsystem.md)
   - 5.1 Input Class Hierarchy
   - 5.2 Supported Input Types
   - 5.3 Input Discovery and Initialization
   - 5.4 Mux and Service Relationship
   - 5.5 Input Statistics

6. [Service Management](06-Service-Management.md)
   - 6.1 Service Structure and States
   - 6.2 Service Lifecycle
   - 6.3 Service Operations
   - 6.4 Service Reference Counting

7. [Streaming Architecture](07-Streaming-Architecture.md)
   - 7.1 Streaming Pad/Target Pattern
   - 7.2 Streaming Architecture Diagram
   - 7.3 Streaming Message Types
   - 7.4 Data Flow
   - 7.5 Streaming Queues

8. [Profile System and Stream Processing](08-Profile-System.md)
   - 8.1 Profile Architecture
   - 8.2 Profile Chains
   - 8.3 Transcoding Pipeline
   - 8.4 Stream Filtering
   - 8.5 Performance Considerations

9. [Muxer System and File Formats](09-Muxer-System.md)
   - 9.1 Muxer Architecture
   - 9.2 Container Formats
   - 9.3 Metadata Embedding
   - 9.4 Subtitle and Audio Track Handling
   - 9.5 File Writing and Buffering
   - 9.6 Format Selection Logic

10. [Subscription System](10-Subscription-System.md)
    - 10.1 Subscription Structure and States
    - 10.2 Subscription Lifecycle
    - 10.3 Subscription Creation and Management
    - 10.4 Service Instance Selection
    - 10.5 Subscription-to-Service Connection

11. [Timeshift Support](11-Timeshift-Support.md)
    - 11.1 Timeshift Architecture
    - 11.2 Buffer Management
    - 11.3 Seek and Skip Operations
    - 11.4 Disk Space Management
    - 11.5 Integration with Subscriptions

12. [DVR Subsystem](12-DVR-Subsystem.md)
    - 12.1 DVR Components
    - 12.2 Recording Flow
    - 12.3 Recording Lifecycle
    - 12.4 DVR Configuration System
    - 12.5 File Management
    - 12.6 DVR-Subscription Relationship

13. [EPG Subsystem](13-EPG-Subsystem.md)
    - 13.1 EPG Data Model
    - 13.2 EPG Grabber Types
    - 13.3 EPG Storage and Persistence
    - 13.4 EPG Update and Matching
    - 13.5 EPG-Channel Relationship

14. [Descrambler Subsystem](14-Descrambler-Subsystem.md)
    - 14.1 Descrambler Architecture
    - 14.2 Supported CA Client Types
    - 14.3 ECM/EMM Handling
    - 14.4 Key Management and Decryption
    - 14.5 Descrambler-Service Relationship

15. [HTTP/API Server](15-HTTP-API-Server.md)
    - 15.1 HTTP Server Architecture
    - 15.2 API Registration System
    - 15.3 idnode System
    - 15.4 Authentication and Access Control
    - 15.5 WebSocket/Comet Implementation

16. [HTSP Server](16-HTSP-Server.md)
    - 16.1 HTSP Protocol Overview
    - 16.2 HTSP Connection Handling
    - 16.3 HTSP Message Types

17. [SAT>IP Server](17-SATIP-Server.md)
    - 17.1 SAT>IP Server Architecture
    - 17.2 RTSP Server Implementation
    - 17.3 RTP Streaming
    - 17.4 Client Session Management
    - 17.5 SAT>IP Discovery

18. [Access Control System](18-Access-Control-System.md)
    - 18.1 Access Control Architecture
    - 18.2 Permission Model
    - 18.3 User Authentication
    - 18.4 IP-Based Access Control
    - 18.5 Resource Restrictions
    - 18.6 Access Verification Flow

19. [Configuration and Persistence](19-Configuration-Persistence.md)
    - 19.1 Configuration Storage
    - 19.2 Settings System
    - 19.3 idnode Persistence

20. [Known Issues and Pitfalls](20-Known-Issues-Pitfalls.md)
    - 20.1 Threading Concerns
    - 20.2 Memory Management Pitfalls
    - 20.3 Legacy Code Areas

21. [Development Guidelines](21-Development-Guidelines.md)
    - 21.1 Adding New Features
    - 21.2 Debugging Techniques
    - 21.3 Code Style and Conventions

22. [Version History](22-Version-History.md)

23. [Documentation Maintenance](23-Documentation-Maintenance.md)

---

**Note:** This is a living document. As the Tvheadend codebase evolves, this documentation should be updated to reflect architectural changes, new subsystems, and lessons learned.
